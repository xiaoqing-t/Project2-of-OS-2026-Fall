#include "tools.h"
#include "sandbox.h"
#include "util.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 函数声明
ToolResult tool_write_file(cJSON *args);

// ToolDef 结构体定义
ToolDef write_file_def = {
    .name = "write_file",
    .desc = "Write content to a file in the workspace (overwrites if exists).",
    .param_schema = "{\"type\":\"object\","
                    "\"properties\":{"
                    "\"path\":{\"type\":\"string\",\"description\":\"Relative path inside the workspace\"},"
                    "\"content\":{\"type\":\"string\",\"description\":\"Content to write to the file\"}"
                    "},"
                    "\"required\":[\"path\",\"content\"]}",
    .exec = tool_write_file,
    .read_only = false
};

ToolResult tool_write_file(cJSON *args) {
    // 提取path参数
    cJSON *path_json = cJSON_GetObjectItem(args, "path");
    if (!path_json || !cJSON_IsString(path_json)) {
        return (ToolResult){.ok = false, .output = xstrdup("Invalid or missing 'path' parameter")};
    }
    const char *rel_path = cJSON_GetStringValue(path_json);

    // 提取content参数
    cJSON *content_json = cJSON_GetObjectItem(args, "content");
    if (!content_json || !cJSON_IsString(content_json)) {
        return (ToolResult){.ok = false, .output = xstrdup("Invalid or missing 'content' parameter")};
    }
    const char *content = cJSON_GetStringValue(content_json);

    // 沙箱验证路径
    char *safe_path = resolve_workspace_path(rel_path);
    if (!safe_path) {
        return (ToolResult){.ok = false, .output = xasprintf("Invalid file path (outside workspace):%s", strerror(errno))};
    }

    // 打开文件
    FILE *f = fopen(safe_path, "w");
    if (!f) {
        free(safe_path);
        return (ToolResult){.ok = false, .output = xasprintf("Failed to open file for writing: %s", strerror(errno))};
    }

    // 写入内容
    size_t content_len = strlen(content);
    size_t written = fwrite(content, 1, content_len, f);
    fclose(f);
    free(safe_path);

    // 检查写入结果
    if (written != content_len) {
        return (ToolResult){.ok = false, .output = xasprintf("Failed to write complete content to file: %s", strerror(errno))};
    }

    // 成功
    return (ToolResult){.ok = true, .output = xasprintf("successfully wrote %zu bytes to %s", content_len, rel_path)};
}