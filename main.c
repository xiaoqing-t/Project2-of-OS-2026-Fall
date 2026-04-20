#include "agent/agent.h"
#include "config.h"
#include "ui/ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INPUT_BUF 4096

int main(void) {
  config_init();

  //初始化UI
  ui_init();
  ui_banner();

  Agent *a = agent_create();
  if (!a) {
    fprintf(stderr, "agent_create failed\n");
    return 1;
  }

  // 启动UI渲染线程
  ui_start();

  char input[INPUT_BUF];
  int rc = 0;

  // REPL 循环
  while (1) {
    ui_prompt();
    fflush(stdout);

    if (!fgets(input, sizeof(input), stdin)) {
      printf("\n");
      break;
    }

    size_t len = strlen(input);
    if (len > 0 && input[len - 1] == '\n')
      input[len - 1] = '\0';

    if (strcmp(input, "exit") == 0 ||
        strcmp(input, "quit") == 0 ||
        strcmp(input, "q") == 0) {
      break;
    }
    
    const char *reply = agent_chat(a, input);
    rc = reply ? 0 : 1;
    if (reply)
      printf("%s\n", reply);
    else {
      ui_error("agent_chat failed");
    }
  }

  agent_free(a);
  ui_stop();
  return rc;
}