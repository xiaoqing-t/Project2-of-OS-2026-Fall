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
  char *system_prompt;
  char *last_reply;
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
  fprintf(stderr, "agent_chat: not implemented\n");
  return NULL;
}
