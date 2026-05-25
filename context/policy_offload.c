#include "context/internal.h"
#include "util.h"
#include "config.h"

#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

// 判断消息是否应该被offload
static bool should_offload_message(const char *msg_json, int index, int total_count) {
    // 只处理最近 KEEP_RECENT_MSGS 以外的消息
    if (index >= total_count - KEEP_RECENT_MSGS) {
        return false;
    }

    // 解析 JSON
    cJSON *obj = cJSON_Parse(msg_json);
    if (!obj) return false;

    // 检查 role 是否为 tool
    const char *role = json_str(obj, "role");
    bool is_tool = (role && strcmp(role, "tool") == 0);

    // 检查 content 是否足够长
    const char *content = json_str(obj, "content");
    bool long_enough = (content && strlen(content) > 200); // 200 字符阈值

    cJSON_Delete(obj);
    return is_tool && long_enough;
}

static char *save_to_offload(Context *ctx, const char *content, int *out_id) {
    char base_path[PATH_MAX-90];
    snprintf(base_path, sizeof(base_path), "%s/.agent", g_config.workdir);
    mkdir(base_path,0755);

    char offload_path[PATH_MAX-50];
    snprintf(offload_path, sizeof(offload_path), "%s/offload", base_path);
    mkdir(offload_path, 0755);

    int id = ctx->next_offload_id++;
    *out_id = id;

    char full_path[PATH_MAX];
    snprintf(full_path, sizeof(full_path), "%s/%d.txt", offload_path, id);

    FILE *f = fopen(full_path, "w");
    if (!f) return NULL;
    fwrite(content, 1, strlen(content), f);
    fclose(f);

    return xasprintf(".agent/offload/%d.txt", id);
}

static bool offload_should_apply(Context *ctx) {
    float usage = ctx_budget_usage(ctx);
    return usage > g_config.offload_threshold;
}

static int offload_apply(Context *ctx, char *err, size_t err_cap) {
    const MessageList *history = ctx_history(ctx);
    int total = history->len;

    printf("[DEBUG] offload_apply: total=%d, workdir=%s\n", total, g_config.workdir);

    for (int i = 0; i < total; i++) {
        printf("[DEBUG] msg %d: %.80s...\n", i, history->items[i]);

        if(!should_offload_message(history->items[i], i, total)) {
            continue;
        }

        printf("[DEBUG] msg %d: offloading...\n", i);

        cJSON *obj = cJSON_Parse(history->items[i]);
        if (!obj) continue;

        const char *content = json_str(obj, "content");
        if(!content) {
            cJSON_Delete(obj);
            continue;
        }

        int offload_id;
        char *saved_path = save_to_offload(ctx, content, &offload_id);
        if(!saved_path) {
            cJSON_Delete(obj);
            snprintf(err, err_cap, "offload: failed to save content to disk");
            return -1;
        }

        char placeholder[512];
        // 获取原始内容preview
        char preview[128];
        strncpy(preview, content, 100);
        preview[100] = '\0';
        snprintf(placeholder, sizeof(placeholder), "%s...[content offloaded to %s. Use read_file tool to retrieve]", preview, saved_path);

        cJSON *new_obj = cJSON_Duplicate(obj, 1);
        cJSON_ReplaceItemInObject(new_obj, "content", cJSON_CreateString(placeholder));
        char *new_json = cJSON_PrintUnformatted(new_obj);
        cJSON_Delete(new_obj);
        cJSON_Delete(obj);

        if (!new_json) {
            free(saved_path);
            snprintf(err, err_cap, "offload: failed to create placeholder JSON");
            return -1;
        }

        ctx_replace_msg(ctx, i, new_json);
        free(saved_path);
    }

    return 0;
}

ContextPolicy offload_policy = {
    .name = "offload",
    .should_apply = offload_should_apply,
    .apply = offload_apply,
};