// context/policy_summary.c
#include "context/internal.h"
#include "config.h"

static bool summary_should_apply(Context *ctx) {
    (void)ctx;  // 暂时未使用，避免警告
    // TODO: 实现
    return false;
}

static int summary_apply(Context *ctx, char *err, size_t err_cap) {
    (void)ctx;
    (void)err;
    (void)err_cap;
    // TODO: 实现
    return 0;
}

ContextPolicy summary_policy = {
    .name = "summary",
    .should_apply = summary_should_apply,
    .apply = summary_apply,
};