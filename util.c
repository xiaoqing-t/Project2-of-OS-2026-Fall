#include "util.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void die_oom(void) {
  fprintf(stderr, "Fatal: out of memory\n");
  exit(1);
}

void *xmalloc(size_t size) {
  void *p = malloc(size);
  if (!p)
    die_oom();//die_oom()函数被调用时，会打印一条错误消息 "Fatal: out of memory" 到标准错误流，并终止程序的执行，返回一个非零的退出状态码。
  return p;
}

void *xrealloc(void *ptr, size_t size) {
  void *p = realloc(ptr, size);//realloc函数用于重新分配内存块的大小。它接受一个指向之前分配的内存块的指针ptr和新的大小size作为参数。如果realloc成功，它会返回一个指向新内存块的指针；如果失败，它会返回NULL，并且原来的内存块保持不变。
  if (!p)
    die_oom();
  return p;
}

char *xstrdup(const char *s) {
  char *p = strdup(s);//strdup函数用于复制一个字符串，并返回一个指向新字符串的指针。它会在内部调用malloc来分配足够的内存来存储复制的字符串。如果strdup函数返回NULL，表示内存分配失败，此时调用die_oom()函数来处理内存不足的情况。
  if (!p)
    die_oom();
  return p;
}//把const char *s复制到一个新的内存位置，并返回指向新字符串的指针。

char *xasprintf(const char *fmt, ...) {
  va_list ap, ap2;
  va_start(ap, fmt);//va_start宏用于初始化一个va_list类型的变量ap，使其指向可变参数列表中的第一个参数。fmt参数是一个格式字符串，后面跟着可变数量的参数。
  va_copy(ap2, ap);//va_copy宏用于将一个va_list类型的变量ap复制到另一个va_list类型的变量ap2。这是因为在调用vsnprintf函数时，va_list变量会被修改，所以需要使用va_copy来保留原始的参数列表，以便后续再次使用。

  int len = vsnprintf(NULL, 0, fmt, ap);//vsnprintf函数用于将格式化的数据写入一个字符串中。第一个参数为NULL，表示不需要实际的输出缓冲区，第二个参数为0，表示不限制输出的长度。fmt参数是格式字符串，ap是可变参数列表。vsnprintf函数会返回格式化后的字符串的长度，不包括终止符'\0'。
  va_end(ap);//va_end宏用于结束对可变参数列表的访问，清理相关资源。调用va_end后，ap变量不再有效。
  if (len < 0) {
    va_end(ap2);
    fprintf(stderr, "Fatal: vsnprintf failed\n");
    exit(1);
  }

  char *buf = xmalloc((size_t)len + 1);
  vsnprintf(buf, (size_t)len + 1, fmt, ap2);//vsnprintf函数再次被调用，这次提供了一个实际的输出缓冲区buf和足够的长度(len + 1)来存储格式化后的字符串。ap2是之前复制的参数列表，确保了参数的正确使用。
  va_end(ap2);
  return buf;
}
