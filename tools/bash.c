/*
 * bash tool — run a shell command in a forked child and return its output
 * as a ToolResult.
 *
 * The child side (pipe setup, fork, dup2, execl) is given below — you have
 * already written this pattern in Project 1. What the LLM actually needs is
 * the *other half*: after fork, the parent holds pipefd[0] and pid, and
 * must turn that into a ToolResult the agent can send back.
 */

 //本文件实现了一个名为bash_tool_exec的函数，该函数接受一个cJSON对象作为参数，执行其中指定的shell命令，并返回一个ToolResult结构体，包含命令执行的结果和输出。函数通过创建管道、fork子进程、重定向输出、执行命令、读取输出和等待子进程结束等步骤来实现这个功能。
 //tool_result_free函数用于释放ToolResult结构体中的资源，特别是output字符串所占用的内存。BASH_TOOL_NAME、BASH_TOOL_DESC和BASH_TOOL_SCHEMA是定义工具名称、描述和参数模式的常量字符串，用于与LLM交互时提供工具信息。
#include "tools.h"
#include "util.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

const char *BASH_TOOL_NAME = "bash";
const char *BASH_TOOL_DESC =
    "Run a shell command and return its combined stdout/stderr.";
const char *BASH_TOOL_SCHEMA =
    "{\"type\":\"object\","
    "\"properties\":{\"command\":{\"type\":\"string\","
    "\"description\":\"The shell command to execute\"}},"
    "\"required\":[\"command\"]}";

void tool_result_free(ToolResult *r) {
  if (!r)
    return;
  free(r->output);
  r->output = NULL;
} //ToolResult 是一个结构体，包含一个布尔值ok和一个字符串指针output。
// 这个函数的作用是释放ToolResult结构体中的output字符串所占用的内存，并将output指针设置为NULL，以避免悬空指针的使用。

//cJSON是C语言JSON解析库，cJSON_GetObjectItem函数用于从JSON对象中获取指定键的值——返回一个cJSON指针，cJSON_GetStringValue函数用于获取JSON字符串的值，返回一个指向字符串的指针。
ToolResult bash_tool_exec(cJSON *args) {
  const char *cmd = cJSON_GetStringValue(cJSON_GetObjectItem(args, "command"));
  if (!cmd)
    return (ToolResult){.ok = false,
                        .output = xstrdup("missing 'command' argument")};
//xstrdup在util.c中出现，把const str转化到malloc分配的内存中，方便后续修改和释放。
  int pipefd[2];//file descriptors for the pipe, pipefd[0] for reading and pipefd[1] for writing
  if (pipe(pipefd) != 0)//创建一个管道，如果成功，pipefd[0]将用于读取数据，pipefd[1]将用于写入数据。如果失败，返回-1，并设置errno以指示错误原因。
    return (ToolResult){
        .ok = false,
        .output = xasprintf("pipe failed: %s", strerror(errno)),
    };

  pid_t pid = fork();
  if (pid < 0) {
    int e = errno;
    close(pipefd[0]);
    close(pipefd[1]);
    return (ToolResult){.ok = false,
                        .output = xasprintf("fork: %s", strerror(e))};
  }//xasprintf是util.c中出现的一个函数，类似于sprintf，但它会自动分配足够的内存来存储格式化后的字符串，并返回一个指向该字符串的指针。使用xasprintf可以避免缓冲区溢出的问题，因为它会根据需要动态分配内存。

  if (pid == 0) {
    close(pipefd[0]);
    /* dprintf + _exit avoid stdio-buffer double-flush after fork. */
    if (dup2(pipefd[1], STDOUT_FILENO) < 0 ||
        dup2(pipefd[1], STDERR_FILENO) < 0)//把子进程的标准输出和标准错误重定向到管道的写端。
      _exit(127);
    close(pipefd[1]);//关闭管道的写端，因为子进程已经将其重定向到标准输出和标准错误了。
    execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);//bin是Unix系统常见目录，包含了许多可执行文件，sh是一个常见的Unix shell，-c选项告诉sh执行后面的命令字符串cmd。
    //execl函数的参数：/bin/sh是shell的路径，"sh"是进程名，"-c"是命令行选项，cmd是要执行的命令，(char *)NULL是参数列表的结束符。
    dprintf(STDERR_FILENO, "exec failed: %s\n", strerror(errno));
    _exit(127);
  }

  close(pipefd[1]);

  /*
   * TODO(student, Part 1A):
   *
   * You are in the parent. The child is running `cmd` with its stdout and
   * stderr both wired to pipefd[1]; you hold pipefd[0] and pid.
   *
   * Produce a ToolResult describing what the child did. The LLM will read
   * whatever you put in .output verbatim on the next turn, so think about
   * what it would actually want to see — *including* when the command
   * failed. A non-zero exit or a fatal signal is information the LLM needs
   * to react to; see waitpid(2) and the WIFEXITED / WEXITSTATUS /
   * WIFSIGNALED / WTERMSIG macros in <sys/wait.h>.
   *
   * Remember to close pipefd[0] and reap the child before returning.
   */
    // 读取所有输出
    char *output = NULL;
    size_t output_size = 0;
    char buffer[4096];
    ssize_t bytes_read;

    while ((bytes_read = read(pipefd[0], buffer, sizeof(buffer))) > 0) {//while循环是因为read函数可能会多次调用才能读取到所有的输出，特别是当输出较大时。每次调用read函数都会将从管道中读取的数据存储在buffer中，并返回实际读取的字节数bytes_read。
      char *new_output = xrealloc(output, output_size + bytes_read + 1);
      if (!new_output) {
        free(output);
        close(pipefd[0]);
        waitpid(pid, NULL, 0);
        return (ToolResult){.ok = false,
                            .output = xstrdup("memory allocation failed")};
      }
      output = new_output;
      memcpy(output + output_size, buffer, bytes_read);
      output_size += bytes_read;
      output[output_size] = '\0';
  }
    close(pipefd[0]);
    //完成读取，并关闭管道的读端。

    //等待子进程结束，并获取其退出状态。
    int status;
    if (waitpid(pid, &status, 0) < 0) {
      free(output);
      return (ToolResult){.ok = false,
                          .output = xasprintf("waitpid failed: %s", strerror(errno))};
    }

    // 检查子进程的退出状态，判断命令是否成功执行。
    bool success = false;
    char* final_output = NULL;

    if (WIFEXITED(status)) {
      int exit_code = WEXITSTATUS(status);
      if (exit_code == 0) {
        success = true;
        final_output = output ? xstrdup(output) : xstrdup("no output");
      } else {
        success = false;
        final_output = xasprintf("%s\n[exit code: %d]", output ? output : "", exit_code);
      }
    } else if (WIFSIGNALED(status)) {
      int signal_num = WTERMSIG(status);
      success = false;
      final_output = xasprintf("%s\n[terminated by signal: %d]", output ? output : "", signal_num);
    } else {
      success = false;
      final_output = xasprintf("%s\n[terminated abnormally]", output ? output : "");
    }
    free(output);
    return (ToolResult){.ok = success, .output = final_output};
}