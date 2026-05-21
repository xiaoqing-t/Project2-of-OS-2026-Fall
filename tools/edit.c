#include "tools.h"
#include "tools/sandbox.h"
#include "util.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 函数声明
ToolResult tool_edit_file(cJSON *args);

// ToolDef 定义
ToolDef edit_file_def = {
    .name = "edit_file",
    .desc = "Replace the first occurrence of old_text with new_text in a file.",
    .param_schema = "{\"type\":\"object\","
                    "\"properties\":{"
                    "\"path\":{\"type\":\"string\",\"description\":\"Relative path inside the workspace\"},"
                    "\"old_text\":{\"type\":\"string\",\"description\":\"Text to find\"},"
                    "\"new_text\":{\"type\":\"string\",\"description\":\"Replacement text\"}"
                    "},"
                    "\"required\":[\"path\",\"old_text\",\"new_text\"]}",
    .exec = tool_edit_file,
    .read_only = false, 
};

ToolResult tool_edit_file(cJSON *args) {
    // 提取path参数
    cJSON *path_json = cJSON_GetObjectItem(args, "path");
    if (!path_json || !cJSON_IsString(path_json)) {
        return (ToolResult){.ok = false, .output = xstrdup("missing or invalid 'path' argument")};
    }
    const char *rel_path = cJSON_GetStringValue(path_json);

    // 提取old_text参数
    cJSON *old_json = cJSON_GetObjectItem(args, "old_text");
    if (!old_json || !cJSON_IsString(old_json)) {
        return (ToolResult){.ok = false, .output = xstrdup("missing or invalid 'old_text' argument")};
    }
    const char *old_text = cJSON_GetStringValue(old_json);

    if (strlen(old_text) == 0) {
        return (ToolResult){.ok = false, .output = xstrdup("'old_text' cannot be empty")};
    }

    // 提取new_text参数
    cJSON *new_json = cJSON_GetObjectItem(args, "new_text");
    if (!new_json || !cJSON_IsString(new_json)) {
        return (ToolResult){.ok = false, .output = xstrdup("missing or invalid 'new_text' argument")};
    }
    const char *new_text = cJSON_GetStringValue(new_json);

    // 沙箱验证路径
    char *safe_path = resolve_workspace_path(rel_path);
    if (!safe_path) {
        return (ToolResult){.ok = false, .output = xstrdup("sandbox: path rejected or outside workspace")};
    }

    // 读取文件内容
    FILE *f = fopen(safe_path, "r");
    if (!f) {
        free(safe_path);
        return (ToolResult){.ok = false, .output = xasprintf("failed to open file for reading: %s", strerror(errno))};
    }

    // 获取文件大小
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    // 分配缓冲区
    char *content = malloc((size_t)file_size + 1);
    size_t read_len = fread(content, 1, (size_t)file_size, f);
    content[read_len] = '\0'; // 确保字符串以null结尾
    fclose(f);

    // 查找old_text
    char *pos = strstr(content, old_text);
    if (!pos) {
        free(safe_path);
        free(content);
        return (ToolResult){.ok = false, .output = xstrdup("text to replace not found in file")};
    }

    // 构建新的文件内容
    // 计算各部分长度
    size_t prefix_len = (size_t)(pos - content);
    size_t old_len = strlen(old_text);
    size_t new_len = strlen(new_text);
    size_t suffix_len = read_len - prefix_len - old_len;

    // 分配新缓冲区
    size_t new_size = prefix_len + new_len + suffix_len;
    char *new_content = xmalloc(new_size + 1);

    // 组合新内容
    memcpy(new_content, content, prefix_len);
    memcpy(new_content + prefix_len, new_text, new_len);
    memcpy(new_content + prefix_len + new_len, pos + old_len, suffix_len);
    new_content[new_size] = '\0';

    // 写回文件
    f = fopen(safe_path, "w");
    if (!f) {
        free(content);
        free(new_content);
        free(safe_path);
        return (ToolResult){.ok = false, .output = xasprintf("failed to open file for writing: %s", strerror(errno))};
    }

    size_t written = fwrite(new_content, 1, new_size, f);
    fclose(f);
    free(content);
    free(new_content);
    free(safe_path);

    if (written != new_size) {
        return (ToolResult){.ok = false, .output = xasprintf("incomplete write: wrote %zu of %zu bytes", written, new_size)};
    }

    // 成功
    return (ToolResult){.ok = true, .output = xasprintf("replaced first occurrence of '%s' with '%s' in %s", old_text, new_text, rel_path)};
}