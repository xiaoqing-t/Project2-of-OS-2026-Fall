#include "tools.h"
#include "sandbox.h"
#include "util.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TOOL_OUTPUT 50000

// 函数声明
ToolResult tool_read_file(cJSON *args);

// 定义 ToolDef 结构体实例
ToolDef read_file_def = {
    .name = "read_file",
    .desc = "Reads the contents of a file and returns it as a string.",
    .param_schema = "{\"type\":\"object\","
                    "\"properties\":{"
                    "\"path\":{\"type\":\"string\",\"description\":\"Relative path inside the workspace\"},"
                    "\"limit\":{\"type\":\"integer\",\"description\":\"Optional maximum number of lines to return\"}"
                    "},"
                    "\"required\":[\"path\"]}",
    .exec = tool_read_file,
    .read_only = true,
};

ToolResult tool_read_file(cJSON *args) {
    // 提取参数
    const cJSON *path_json = cJSON_GetObjectItem(args, "path");
    if (!path_json || !cJSON_IsString(path_json)) {
        return (ToolResult){.ok = false, .output = xstrdup("Invalid or missing 'path' parameter")};
    }
    const char *rel_path = cJSON_GetStringValue(path_json);

    // 解析limit
    int line_limit = 0;
    const cJSON *limit_json = cJSON_GetObjectItem(args, "limit");
    if (limit_json && cJSON_IsNumber(limit_json)) {
        line_limit = (int)cJSON_GetNumberValue(limit_json);
        if(line_limit < 0) line_limit = 0; 
    }

    // 沙箱验证路径
    char *safe_path = resolve_workspace_path(rel_path);
    if (!safe_path) {
        return (ToolResult){.ok = false, .output = xstrdup("sandbox: path rejected or outside workspace")};
    }

    // 打开文件
    FILE *f = fopen(safe_path, "r");
    free(safe_path);
    if (!f) {
        return (ToolResult){.ok = false, .output = xasprintf("Failed to open file: %s", strerror(errno))};
    }

    // 读取文件内容
    char *content = NULL;
    size_t content_size = 0;
    char line[4096];
    int lines_read = 0;

    while (fgets(line, sizeof(line), f)) {
        lines_read++;
        size_t line_len = strlen(line);

        if (line_limit > 0 && lines_read > line_limit) {
            break;
        }

        // 重新分配内存以容纳新内容
        char *new_content = xrealloc(content, content_size + line_len + 1);
        if (!new_content) {
            free(content);
            fclose(f);
            return (ToolResult){.ok = false, .output = xstrdup("Memory allocation failed")};
        }
        content = new_content;
        memcpy(content + content_size, line, line_len);
        content_size += line_len;
        content[content_size] = '\0'; // 确保字符串以 null 结尾

        // 限制输出大小
        if (content_size >= MAX_TOOL_OUTPUT) {
            // 超过限制，添加截断提示
            char *trunc_msg = "\n... (output truncated due to size limit)";
            size_t msg_len = strlen(trunc_msg);
            char *new_content2 = xrealloc(content, content_size + msg_len + 1);
            if (new_content2) {
                content = new_content2;
                memcpy(content + content_size, trunc_msg, msg_len);
                content_size += msg_len;
                content[content_size] = '\0';
            }
            break;
        }
    }

    fclose(f);

    // 返回结果
    if (!content || content_size == 0) {
        return (ToolResult){.ok = false, .output = xstrdup("File is empty or could not be read")};
    }

    return (ToolResult){.ok = true, .output = content};
}