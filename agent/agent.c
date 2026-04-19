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

#define MAX_TURNS 20 //防止无限循环

static const char AGENT_SYSTEM_TEMPLATE[] =
    "You are a coding agent running in the CLI at %s.\n"
    "Use the provided tools when you need to run shell commands.\n"
    "Return a short, final text reply when the task is done.";

struct Agent {
  char *system_prompt;//系统提示词
  char *last_reply;//缓存上一次LLM回复的文本内容（如果有的话），以便在下一次agent_chat调用时返回给用户。这个字段的生命周期与Agent实例相同，每次agent_chat调用都会更新它的内容。
};

static void llm_response_cleanup(LLMResponse *resp) {
    if (!resp) return;
    if (resp->content) free(resp->content);
    if (resp->raw_message) free(resp->raw_message);
    if (resp->tool_calls) {
        for (int i = 0; i < resp->n_tool_calls; i++) {
            if (resp->tool_calls[i].id) free(resp->tool_calls[i].id);
            if (resp->tool_calls[i].name) free(resp->tool_calls[i].name);
            if (resp->tool_calls[i].args) cJSON_Delete(resp->tool_calls[i].args);
        }
        free(resp->tool_calls);
    }
    resp->content = NULL;
    resp->raw_message = NULL;
    resp->tool_calls = NULL;
    resp->n_tool_calls = 0;
}

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
  int turns = 0;

  while (turns < MAX_TURNS) {
    // 1.调用 llm_chat
    int ret = llm_chat(&msgs, a->system_prompt, g_config.model,
                       &response, err_buf, sizeof(err_buf));
    if (ret < 0) {
    fprintf(stderr, "LLM chat failed: %s\n", err_buf);
    msg_list_free(&msgs);
    return NULL;
    }

    // 2.如果没有tool_calls，说明llm给出最终答案
    if (response.n_tool_calls == 0) {
      // 保存回复并返回
      if (a->last_reply) free(a->last_reply);
      a->last_reply = response.content;
      response.content = NULL;
      llm_response_cleanup(&response);
      msg_list_free(&msgs);
      return a->last_reply;
    }

    // 3.如果有 tool_calls，执行所有工具，并将结果加入历史
    // 添加assistant消息
    char *assistant_msg = xstrdup(response.raw_message);
    msg_list_push(&msgs, assistant_msg);

    // 遍历执行每个tool_call
    for (int i=0; i < response.n_tool_calls; i++) {
      LLMToolCall *tool_call = &response.tool_calls[i];
      ToolResult tool_result;

      if (strcmp(tool_call->name, "bash") == 0) {
        tool_result = bash_tool_exec(tool_call->args);
      } else {
        tool_result.ok = false;
        tool_result.output = xasprintf("Unknown tool: %s. Available :bash", tool_call->name);
      }

      // 将工具记录加入历史
      char *tool_msg = msg_tool_json(tool_call->id, tool_result.output);
      if (!tool_msg) {
        fprintf(stderr, "Failed to create tool message\n");
        free(tool_result.output);
        //清理资源之后返回NULL
        msg_list_free(&msgs);
        return NULL;
      }
      msg_list_push(&msgs, tool_msg);
      free(tool_result.output);
    }

    // turns++，继续循环
    llm_response_cleanup(&response);
    turns++;
  }

  // 4.如果循环结束且超过MAX_TURNS，报错
  fprintf(stderr, "Agent: exceed max turns (%d)\n", MAX_TURNS);
  msg_list_free(&msgs);
  return NULL;
}