#ifndef CONFIG_H
#define CONFIG_H

#include <linux/limits.h>

/*
 * Process-wide runtime configuration, derived once from environment variables.
 */
typedef struct {
  char model[128];
  char llm_host[256];
  char api_key[256];
  int llm_port;
  char workdir[PATH_MAX-100];
  int max_tokens;
  int context_window;
  int offload_threshold;
  int summary_threshold;
} AgentConfig;
// extern 是一个关键字，用于声明一个变量在其他文件中定义。它告诉编译器这个变量的内存空间是在其他地方分配的，而不是在当前文件中定义的。在这个例子中，g_config 是一个全局变量，它在 config.c 文件中定义，并且在其他文件中通过 extern 声明来使用。
extern AgentConfig g_config;

void config_init(void);

#endif
