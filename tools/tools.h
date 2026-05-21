#ifndef TOOLS_H
#define TOOLS_H

#include "cJSON.h"

#include <stdbool.h>

/*
 * Part 1: one concrete tool (bash). We expose its execution as a plain
 * function rather than dressing it up behind a registry — the crudeness is
 * intentional, and it is the seed that motivates the registry refactor in
 * Part 2.
 */
typedef struct {
    bool ok;
    char *output;        /* heap-allocated; freed by tool_result_free */
} ToolResult;

void tool_result_free(ToolResult *r);

#define MAX_TOOL_OUTPUT 50000

typedef ToolResult (*ToolFn)(cJSON *args);

typedef struct {
    const char *name;
    const char *desc;
    const char *param_schema;
    ToolFn exec;
    bool read_only;
} ToolDef;

#define MAX_REGISTERED_TOOLS 16

void tools_init(void);
void tool_register(ToolDef *def);
ToolDef *tool_find(const char *name);
ToolDef *const *tool_list(int *out_count);

#endif
