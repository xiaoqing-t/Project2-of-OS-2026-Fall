/*
 * agent.c — orchestration between user input, LLM turns, and tool execution.
 *
 * The skeleton below is sized for Phase A: one request in, one request out,
 * no persistent state to speak of. Phase B and Phase C will both require
 * you to revisit `struct Agent`, agent_create, and agent_free — treat what
 * is here as a starting point, not a contract.
 */
#include "agent.h"

#include "config.h"
#include "llm_client.h"
#include "message.h"
#include "tools/tools.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char AGENT_SYSTEM_TEMPLATE[] =
    "You are a coding agent running in the CLI at %s.\n"
    "Use the provided tools when you need to run shell commands.\n"
    "Return a short, final text reply when the task is done.";

struct Agent {
  char *system_prompt;//系统提示词
  char *last_reply;//缓存上一次LLM回复的文本内容（如果有的话），以便在下一次agent_chat调用时返回给用户。这个字段的生命周期与Agent实例相同，每次agent_chat调用都会更新它的内容。
};

Agent *agent_create(void) {
  Agent *a = calloc(1, sizeof(*a));
  if (!a)
    return NULL;
  a->system_prompt = xasprintf(AGENT_SYSTEM_TEMPLATE, g_config.workdir);
  return a;
}

void agent_free(Agent *a) {
  if (!a)
    return;
  free(a->system_prompt);
  free(a->last_reply);
  free(a);
}

const char *agent_chat(Agent *a, const char *user_input) {
  (void)a;
  (void)user_input;
  
  // 创建消息列表
  MessageList msgs;
  msg_list_init(&msgs);

  // 将用户输入添加到消息列表（JSON字符串形式）
  char *user_msg = msg_user_json(user_input);
  if (!user_msg) {
    fprintf(stderr, "Failed to create user message JSON\n");
    msg_list_free(&msgs);
    return NULL;
  }
  msg_list_push(&msgs, user_msg);// msg_list_push 会接管 user_msg 的所有权，因此不需要在这里 free(user_msg)

  // 调用 llm_chat 获取 LLM 回复
  LLMResponse response;
  char err_buf[512];

  // 调用 llm_chat 函数，传入消息列表、系统提示词、模型 ID，以及用于存储回复的结构体和错误信息的缓冲区
  int ret = llm_chat(&msgs, a->system_prompt, g_config.model, &response, err_buf, sizeof(err_buf));
  if (ret < 0) {
    fprintf(stderr, "LLM chat failed: %s\n", err_buf);
    msg_list_free(&msgs);
    return NULL;
  }
  // response 是llm_client.c中的out，LLMResponse
  //typedef struct {
//   char *content;     /* assistant text, if any (may be NULL or empty) */
//   char *raw_message; /* serialized assistant message — push verbatim into
//                         history */
//   int n_tool_calls;
//   LLMToolCall *tool_calls;
// } LLMResponse;


  // 判断 LLM 回复中是否包含工具调用
  if (response.n_tool_calls == 0) {
    // 没有工具调用，直接缓存回复内容并返回
    // 缓存 content 字段的内容到 Agent 的 last_reply 中，以便下一次调用时返回给用户
    if (a->last_reply) {
      free(a->last_reply);
    }
    a->last_reply = response.content ? xstrdup(response.content) : NULL;
    // 释放其他资源
    if (response.raw_message) {
      free(response.raw_message);
    }
    if (response.tool_calls) {
      free(response.tool_calls);
    }
    msg_list_free(&msgs);
    return a->last_reply;
  }

  // 有工具调用，执行工具并获取结果(phase A只需要支持一个工具调用)
  if (response.n_tool_calls > 0) {
    // 将assistant的回复(raw_message)添加到消息列表中，以便工具调用时上下文完整
    char *assistant_msg = xstrdup(response.raw_message);
    msg_list_push(&msgs, assistant_msg); 

    // 处理第一个工具调用
    LLMToolCall *tool_call = &response.tool_calls[0];

    ToolResult tool_result;

    if (strcmp(tool_call->name, "bash") == 0) {
      // 执行bash工具
      tool_result = bash_tool_exec(tool_call->args);
    } else {
      // 不支持的工具
      tool_result.ok = false;
      tool_result.output = xasprintf("Unsupported tool: %s. Available tools: bash", tool_call->name);
    }

    // 将工具调用结果添加到消息列表中，作为工具消息
    // tool_call->id 用于将结果与工具调用配对
    char *tool_msg = msg_tool_json(tool_call->id, tool_result.output);
    if (!tool_msg) {
      fprintf(stderr, "agent_chat: Failed to create tool message JSON\n");
      free(tool_result.output);
      msg_list_free(&msgs);
      if (response.content) free(response.content);
      if (response.raw_message) free(response.raw_message);
      if (response.tool_calls) {
        for (int j = 0; j < response.n_tool_calls; j++) {
          if (response.tool_calls[j].id) free(response.tool_calls[j].id);
          if (response.tool_calls[j].name) free(response.tool_calls[j].name);
          if (response.tool_calls[j].args) cJSON_Delete(response.tool_calls[j].args);
        }
        free(response.tool_calls);
      }
      return NULL;
    }
    msg_list_push(&msgs, tool_msg); 

    free(tool_result.output);
  

    // 第二次调用llm_chat，获取LLM对工具调用结果的回复
    LLMResponse final_response;
    char err_buf2[512];

    int ret2 = llm_chat(&msgs, a->system_prompt, g_config.model, &final_response, err_buf2, sizeof(err_buf2));

    if (ret2 < 0) {
      fprintf(stderr, "agent_chat: LLM chat after tool execution failed: %s\n", err_buf2);
      // 清理资源
      if (response.content) free(response.content);
      if (response.raw_message) free(response.raw_message);
      if (response.tool_calls) {
          for (int i = 0; i < response.n_tool_calls; i++) {
              if (response.tool_calls[i].id) free(response.tool_calls[i].id);
              if (response.tool_calls[i].name) free(response.tool_calls[i].name);
              if (response.tool_calls[i].args) cJSON_Delete(response.tool_calls[i].args);
          }
          free(response.tool_calls);
      }
      msg_list_free(&msgs);
      return NULL;
    }
    
    // 保存最终回复到 agent
    if (a->last_reply) {
      free(a->last_reply);
    }
    a->last_reply = final_response.content ? final_response.content : NULL;

    // 释放其他资源
    if (final_response.raw_message) free(final_response.raw_message);
    if (final_response.tool_calls) {
      for (int i = 0; i < final_response.n_tool_calls; i++) {
        if (final_response.tool_calls[i].id) free(final_response.tool_calls[i].id);
        if (final_response.tool_calls[i].name) free(final_response.tool_calls[i].name);
        if (final_response.tool_calls[i].args) cJSON_Delete(final_response.tool_calls[i].args);
      }
      free(final_response.tool_calls);
    }

    // 清理第一次调用的资源
    if (response.content) free(response.content);
    if (response.raw_message) free(response.raw_message);
    if (response.tool_calls) {
      for (int i = 0; i < response.n_tool_calls; i++) {
        if (response.tool_calls[i].id) free(response.tool_calls[i].id);
        if (response.tool_calls[i].name) free(response.tool_calls[i].name);
        if (response.tool_calls[i].args) cJSON_Delete(response.tool_calls[i].args);
      }
      free(response.tool_calls);
    }
    msg_list_free(&msgs);
    return a->last_reply;
  }
  /*
   * TODO(student, Part 1A):
   *
   * Drive one user turn:
   *
   *   1. Build a MessageList and push the user message.
   *   2. Call llm_chat. On failure, print the error to stderr and
   *      return NULL.
   *   3. If the response carries no tool calls, cache its content on
   *      `a` (so the returned pointer stays valid until the next call),
   *      release everything else, and return it.
   *   4. Otherwise, push the assistant message into history, execute
   *      the tool the LLM asked for, push the result back as a tool
   *      message (msg_tool_json in message.h), and call llm_chat again.
   */
   return NULL;
}