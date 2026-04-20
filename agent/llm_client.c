/*
 * llm_client.c — HTTP+JSON glue between the Agent and the LLM service.
 *
 * Your job: implement llm_chat. Everything else in this file is yours to
 * design. You will certainly want helpers (request construction, response
 * parsing, …); whether and how you decompose them is a decision for you.
 */
#include "llm_client.h"
#include "util.h"
#include "config.h"
#include "http.h"
#include "tools/tools.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define LLM_TIMEOUT_SEC 120

int llm_chat(const MessageList *messages, const char *system_prompt,
             const char *model, LLMResponse *out, char *err, size_t err_cap) {
  (void)messages;
  (void)system_prompt;
  (void)model;
  (void)out;
  
  // 检查参数
  if (!messages || !out || !err || err_cap == 0) {
    return -1;
  }

  // 初始化out结构体
  out->content = NULL;
  out->raw_message = NULL;
  out->n_tool_calls = 0;
  out->tool_calls = NULL;

  // 创建JSON对象
  cJSON *root = cJSON_CreateObject();
  if (!root) {
    snprintf(err, err_cap, "Failed to create JSON object");
    return -1;
  }

  // 构建请求体
  // 添加model字段
  if (!cJSON_AddStringToObject(root, "model", model)) {
    snprintf(err, err_cap, "Failed to add model to JSON");
    cJSON_Delete(root);
    return -1;
  }

  // 添加max_tokens字段
  if (!cJSON_AddNumberToObject(root, "max_tokens", g_config.max_tokens)) {
    snprintf(err, err_cap, "Failed to add max_tokens to JSON");
    cJSON_Delete(root);
    return -1;
  }

  // 添加messages字段
  cJSON *messages_array = cJSON_CreateArray();
  if (!messages_array) {
    snprintf(err, err_cap, "Failed to create messages array");
    cJSON_Delete(root);
    return -1;
  }
  cJSON_AddItemToObject(root, "messages", messages_array);// messages_array 被添加到root对象，并且root对象现在拥有一个名为"messages"的字段，指向这个数组。不需要单独释放。

  // 添加system_prompt（如果有）
  if (system_prompt && system_prompt[0]) {
    cJSON *system_message = cJSON_CreateObject();
    if (!system_message) {
      snprintf(err, err_cap, "Failed to create system message object");
      cJSON_Delete(root);
      return -1;
    }
    cJSON_AddStringToObject(system_message, "role", "system");
    cJSON_AddStringToObject(system_message, "content", system_prompt);
    cJSON_AddItemToArray(messages_array, system_message);// system_message 被添加到messages_array数组中，并且messages_array现在拥有一个新的元素，指向这个对象。不需要单独释放。
    }
  
    // 添加历史消息（messages参数每条消息）
    for (int i = 0; i < messages->len; i++) {
      // messages->items[i] 是一个JSON 字符串(字符串)
      cJSON *msg = cJSON_Parse(messages->items[i]);
      if (!msg) {
        snprintf(err, err_cap, "Failed to parse message JSON at index %d", i);
        cJSON_Delete(root);
        return -1;
      }
      cJSON_AddItemToArray(messages_array, msg);//msg所有权也给了messages_array，不需要单独释放。
    }
    
    // 添加工具描述
    cJSON *tools_array = cJSON_CreateArray();
    if (!tools_array) {
      snprintf(err, err_cap, "Failed to create tools array");
      cJSON_Delete(root);
      return -1;
    }
    cJSON_AddItemToObject(root, "tools", tools_array);// tools_array 被添加

    // 创建一个工具对象，描述bash工具
    cJSON *tool_obj = cJSON_CreateObject();
    if (!tool_obj) {
      snprintf(err, err_cap, "Failed to create tool object");
      cJSON_Delete(root);
      return -1;
    }
    cJSON_AddItemToArray(tools_array, tool_obj);// tool_obj 被添加到tools_array数组中，并且tools_array现在拥有一个新的元素，指向这个对象。不需要单独释放。

    // 添加type字段
    cJSON_AddStringToObject(tool_obj, "type", "function");// type字段被添加到tool_obj对象中，并且值为"function"。不需要单独释放。

    // 添加function字段
    cJSON *function_obj = cJSON_CreateObject();
    if (!function_obj) {
      snprintf(err, err_cap, "Failed to create function object");
      cJSON_Delete(root);
      return -1;
    }
    cJSON_AddItemToObject(tool_obj, "function", function_obj);
    
    // 添加function.name字段
    cJSON_AddStringToObject(function_obj, "name", BASH_TOOL_NAME);
    // 添加function.description字段
    cJSON_AddStringToObject(function_obj, "description", BASH_TOOL_DESC);
    // 添加function.parameters字段
    // JSON scheme是raw string，先添加为string，再解析回object
    cJSON *params = cJSON_Parse(BASH_TOOL_SCHEMA);
    if (!params) {
      snprintf(err, err_cap, "Failed to parse BASH_TOOL_SCHEMA");
      cJSON_Delete(root);
      return -1;
    }
    cJSON_AddItemToObject(function_obj, "parameters", params);

    // 将JSON对象转换为字符串
    char *body = cJSON_PrintUnformatted(root);
    if (!body) {
      snprintf(err, err_cap, "Failed to serialize JSON");
      cJSON_Delete(root);
      return -1;
    }

    //-------------------------------------------------------------------

    //构建HTTP请求
    // 格式：
    // POST /api/v1/chat/completions HTTP/1.1
    // Host: host:port
    // Authorization: Bearer api_key
    // Content-Type: application/json
    // Content-Length: body长度

    size_t body_len = strlen(body);

    // 计算请求头长度
    char header[4096];
    int header_len = snprintf(header, sizeof(header),
      "POST /api/v1/chat/completions HTTP/1.1\r\n"
      "Host: %s:%d\r\n"
      "Authorization: Bearer %s\r\n"
      "Content-Type: application/json\r\n"
      "Content-Length: %zu\r\n"
      "Connection: close\r\n"
      "\r\n",
      g_config.llm_host, g_config.llm_port, g_config.api_key, body_len
    );// %zu 用于无符号整数，表示body的长度。header_len 是实际写入header缓冲区的字节数，不包括终止的NUL字符。

    if (header_len < 0 || (size_t)header_len >= sizeof(header)) {
      snprintf(err, err_cap, "Failed to construct HTTP header");
      free(body);
      cJSON_Delete(root);
      return -1;
    }

    // 建立TCP连接
    int fd = tcp_connect(g_config.llm_host, g_config.llm_port, err, err_cap);
    if (fd < 0) {
      free(body);
      cJSON_Delete(root);
      return -1;
    }

    // 发送HTTP请求头
    if (send_all(fd, header, (size_t)header_len) < 0) {
      snprintf(err, err_cap, "Failed to send HTTP header: %s", strerror(errno));
      close(fd);
      free(body);
      cJSON_Delete(root);
      return -1;
    }
    
    // ----------------------debug-----------------------
    printf("=== HTTP HEADER ===\n%s", header);
    printf("=== HTTP BODY ===\n%s\n", body);
    // ----------------------debug-----------------------

    // 发送HTTP请求体
    if (send_all(fd, body, body_len) < 0) {
      snprintf(err, err_cap, "Failed to send HTTP body: %s", strerror(errno));
      close(fd);
      free(body);
      cJSON_Delete(root);
      return -1;
    }

    // 接收HTTP响应
    char *response = NULL;//堆分配的缓冲区，需要free
    size_t response_len = 0;

    // 使用recv_all读取完整的HTTP响应，设置超时时间为LLM_TIMEOUT_SEC秒
    if (recv_all(fd, LLM_TIMEOUT_SEC, &response, &response_len, err, err_cap) < 0) {
      // snprintf(err, err_cap, "Failed to receive HTTP response: %s", strerror(errno));
      close(fd);
      free(body);
      cJSON_Delete(root);
      return -1;
    }
    
    // 关闭TCP连接
    close(fd);

    // 解析HTTP响应
    int http_status = 0;
    const char *response_body = NULL;
    if (http_parse_response(response, &http_status, &response_body) < 0) {
      snprintf(err, err_cap, "Failed to parse HTTP response");
      free(response);
      free(body);
      cJSON_Delete(root);
      return -1;
    }

    // 检查HTTP状态码是否为200
    if (http_status != 200) {
      snprintf(err, err_cap, "HTTP request failed with status %d", http_status);
      free(response);
      free(body);
      cJSON_Delete(root);
      return -1;
    }

    // 解析HTTP响应体中的JSON为cJSON对象
    cJSON *json_root = cJSON_Parse(response_body);
    free(response);
    if (!json_root) {
      snprintf(err, err_cap, "Failed to parse JSON response");
      free(body);
      cJSON_Delete(root);
      return -1;
    }

    // 获取choices[0].message对象
    cJSON *choices = cJSON_GetObjectItem(json_root, "choices");
    if (!choices || !cJSON_IsArray(choices) || cJSON_GetArraySize(choices) == 0) {
      snprintf(err, err_cap, "Invalid JSON response: missing choices");
      cJSON_Delete(json_root);
      free(body);
      cJSON_Delete(root);
      return -1;
    }
    //得到的choices是array，获取第一个元素
    cJSON *first_choice = cJSON_GetArrayItem(choices, 0);
    cJSON *message = cJSON_GetObjectItem(first_choice, "message");
    if (!message || !cJSON_IsObject(message)) {
      snprintf(err, err_cap, "Invalid JSON response: missing message");
      cJSON_Delete(json_root);
      free(body);
      cJSON_Delete(root);
      return -1;
    }

    // 提取content字段（可能NULL或空）
    cJSON *content = cJSON_GetObjectItem(message, "content");
    if (content && cJSON_IsString(content)) {
      const char *content_str = cJSON_GetStringValue(content);
      if (content_str && content_str[0] != '\0') {
        out->content = xstrdup(content_str);
        if (!out->content) {
          snprintf(err, err_cap, "Failed to allocate content string");
          cJSON_Delete(json_root);
          free(body);
          cJSON_Delete(root);
          return -1;
        }
      }
    }// 如果content字段不存在或不是字符串，out->content保持为NULL。

    // 保存raw_message（整个message对象的未格式化JSON字符串）
    char *raw_message = cJSON_PrintUnformatted(message);
    if (!raw_message) {
      snprintf(err, err_cap, "Failed to print raw message");
      cJSON_Delete(json_root);
      free(body);
      cJSON_Delete(root);
      return -1;
    }
    out->raw_message = xstrdup(raw_message);
    free(raw_message);
    if (!out->raw_message) {
      snprintf(err, err_cap, "Failed to allocate raw message string");
      cJSON_Delete(json_root);
      free(body);
      cJSON_Delete(root);
      return -1;
    }

    // 提取tool_calls数组
    cJSON *tool_calls = cJSON_GetObjectItem(message, "tool_calls");
    if (tool_calls && cJSON_IsArray(tool_calls)) {
      int n_tool_calls = cJSON_GetArraySize(tool_calls);
      out->n_tool_calls = n_tool_calls;
      if (n_tool_calls > 0) {
        // 分配工具调用数组
        out->tool_calls = xmalloc((size_t)n_tool_calls * sizeof(LLMToolCall));
        if (!out->tool_calls) {
          snprintf(err, err_cap, "Failed to allocate tool calls array");
          cJSON_Delete(json_root);
          free(body);
          cJSON_Delete(root);
          return -1;
        }
        // 解析每个工具调用
        for (int i = 0; i < n_tool_calls; i++) {
          cJSON *tool_call = cJSON_GetArrayItem(tool_calls, i);
          if (!tool_call || !cJSON_IsObject(tool_call)) {
            continue; // 跳过无效的工具调用
          }
          // 提取id字段
          cJSON *id = cJSON_GetObjectItem(tool_call, "id");
          if (id && cJSON_IsString(id)) {
            out->tool_calls[i].id = xstrdup(cJSON_GetStringValue(id));
            if (!out->tool_calls[i].id) {
              snprintf(err, err_cap, "Failed to allocate tool call id string");
              // 这里不用清理，在后面统一处理
              goto cleanup;
            }
          }

          // 提取function对象
          cJSON *function = cJSON_GetObjectItem(tool_call, "function");
          if (function && cJSON_IsObject(function)) {
            // 提取function.name字段
            cJSON *name = cJSON_GetObjectItem(function, "name");
            if (name && cJSON_IsString(name)) {
              out->tool_calls[i].name = xstrdup(cJSON_GetStringValue(name));
              if (!out->tool_calls[i].name) {
                snprintf(err, err_cap, "Failed to allocate tool call name string");
                goto cleanup;
              }
            }
            // 提取function.arguments字段
            cJSON *args_json = cJSON_GetObjectItem(function, "arguments");
            if (args_json && cJSON_IsString(args_json)) {
              const char *args_str = cJSON_GetStringValue(args_json);
              if (args_str && args_str[0] != '\0') {
                cJSON *args_obj = cJSON_Parse(args_str);
                if (args_obj) {
                  out->tool_calls[i].args = args_obj; // 直接使用解析后的对象
                } else {
                  // 如果解析失败，创建一个空对象
                  out->tool_calls[i].args = cJSON_CreateObject();
                }
              } else {
                // 如果arguments字段是空字符串，创建一个空对象
                out->tool_calls[i].args = cJSON_CreateObject();
              }
            }
          }
        }
      }
    }


  /*
   * TODO(student, Part 1A):
   *
   * 1. Build the request body. It is a JSON object with fields
   *    `model`, `messages` (system prompt prepended to the given list),
   *    `tools` (one entry describing the bash tool — see BASH_TOOL_NAME /
   *    BASH_TOOL_DESC / BASH_TOOL_SCHEMA in tools/tools.h), and
   *    `max_tokens` (g_config.max_tokens). Use cJSON (see libs/cJSON.h);
   *    hand-splicing strings will not scale.
   *
   * 2. Open a TCP connection (see http.h) and send a POST to
   *    /api/v1/chat/completions with Authorization: Bearer <api_key>.
   *
   * 3. Read the full response with recv_all, parse it with
   *    http_parse_response, and reject any non-200 status.
   *
   * 4. Parse the JSON body. The assistant message lives at
   *    choices[0].message. Extract `content` (may be missing or empty)
   *    and every entry of `tool_calls` (each has `id`, `function.name`,
   *    `function.arguments`). `arguments` arrives as a JSON *string* on
   *    the wire — cJSON_Parse it back into an object. If the string is
   *    empty, treat it as an empty object.
   *
   * 5. Also keep a serialized copy of the whole assistant message
   *    (cJSON_PrintUnformatted is convenient) in out->raw_message — the
   *    agent will push it into history verbatim so the LLM sees its own
   *    previous reply on the next call.
   *
   * Everything you malloc / cJSON_Parse here has to be freed somewhere.
   * Decide where. `make asan` will tell you if you got it wrong.
   */
  
   cJSON_Delete(json_root);
   free(body);
   cJSON_Delete(root);
   return 0;

cleanup:
    if (out->content) {
      free(out->content);
      out->content = NULL;
    }
    if (out->raw_message) {
      free(out->raw_message);
      out->raw_message = NULL;
    }
    if (out->tool_calls) {
      for (int j = 0; j < out->n_tool_calls; j++) {
        if (out->tool_calls[j].id) {
          free(out->tool_calls[j].id); 
        }
        if (out->tool_calls[j].name) {
          free(out->tool_calls[j].name);
        }
        if (out->tool_calls[j].args) {
          cJSON_Delete(out->tool_calls[j].args);
        }
      }
      free(out->tool_calls);
      out->tool_calls = NULL;
    }
    cJSON_Delete(json_root);
    free(body);
    cJSON_Delete(root);
    return -1;  
}
