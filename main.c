#include "agent/agent.h"
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INPUT_BUF 4096

int main(void) {
  config_init();

  Agent *a = agent_create();
  if (!a) {
    fprintf(stderr, "agent_create failed\n");
    return 1;
  }

  char input[INPUT_BUF];
  if (!fgets(input, sizeof(input), stdin)) {
    agent_free(a);
    return 1;
  }
  size_t len = strlen(input);
  if (len > 0 && input[len - 1] == '\n')
    input[len - 1] = '\0';

  const char *reply = agent_chat(a, input);
  int rc = reply ? 0 : 1;
  if (reply)
    printf("%s\n", reply);

  agent_free(a);
  return rc;
}
