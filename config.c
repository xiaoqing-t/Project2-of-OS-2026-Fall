#include "config.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

AgentConfig g_config;

static void copy_env_string(char *dst, size_t cap, const char *name,
                            const char *fallback) {
  const char *value = getenv(name);
  snprintf(dst, cap, "%s", (value && value[0]) ? value : fallback);
}// copy_env_string 函数用于从环境变量中获取一个字符串值，
// 并将其复制到提供的目标缓冲区中。如果环境变量未设置或为空，
// 则使用备用值。函数使用 snprintf 来确保不会发生缓冲区溢出，
// 并且目标字符串始终以 NUL 结尾。

static int parse_env_int(const char *name, int fallback, int min_value,
                         int max_value) {
  const char *value = getenv(name);
  if (!value || !value[0])
    return fallback;
  errno = 0;
  char *end = NULL;
  long parsed = strtol(value, &end, 10);
  if (errno != 0 || end == value || *end != '\0' || parsed < min_value ||
      parsed > max_value) {
    fprintf(stderr, "[config] warning: invalid %s=%s, using %d\n", name, value,
            fallback);
    return fallback;
  }
  return (int)parsed;
}//parse_env_int 函数用于从环境变量中解析一个整数值。它接受环境变量的名称、
// 一个备用值（当环境变量未设置或无效时使用）、以及允许的最小和最大值。
// 函数首先尝试获取环境变量的值，如果未设置或为空，则返回备用值。
// 然后，它使用 strtol 函数将字符串转换为长整数，
// 并检查转换过程中是否发生错误（如非数字字符、溢出等）。
// 如果转换成功且结果在指定范围内，则返回该整数；
// 否则，打印警告并返回备用值。

void config_init(void) {
  copy_env_string(g_config.model, sizeof(g_config.model), "MODEL_ID",
                  "qwen3coder");
  copy_env_string(g_config.llm_host, sizeof(g_config.llm_host), "LLM_HOST",
                  "127.0.0.1");
  copy_env_string(g_config.api_key, sizeof(g_config.api_key), "API_KEY",
                  "none");

  g_config.llm_port = parse_env_int("LLM_PORT", 18080, 1, 65535);
  g_config.max_tokens = parse_env_int("MAX_TOKENS", 8000, 1, INT_MAX);

  /* Canonicalize so tools and logs see the same path shape. */
  if (!realpath(".", g_config.workdir)) {
    if (!getcwd(g_config.workdir, sizeof(g_config.workdir))) {
      perror("getcwd");
      exit(1);
    }
  }
}// config_init 函数用于初始化全局配置结构体 g_config。
// 它从环境变量中读取配置值，
// 并使用 copy_env_string 和 parse_env_int 函数来处理字符串和整数
