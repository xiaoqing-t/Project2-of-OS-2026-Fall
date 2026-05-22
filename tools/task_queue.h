/**
 * task_queue.h - Fixed queue layer for Lab 3
 *
 * This header exists for exactly two reasons:
 * 1. give thread_pool.c a shared task/queue scaffold,
 * 2. support Phase A white-box tests of FIFO invariants.
 *
 * It does not define struct thread_pool.
 * Students should treat this file as read-only.
 */
// task_queue.h - Lab 3 任务队列的固定队列层
// 这个头文件存在的原因有两个：
// 1. 为 thread_pool.c 提供一个共享的任务/队列脚手架，
// 2. 支持 Phase A 的 FIFO 不变式的白盒测试。
// 它不定义 struct thread_pool。学生应该将这个文件视为只读。
#ifndef TASK_QUEUE_H
#define TASK_QUEUE_H

#include "thread_pool.h"
//在 task_queue.h 中包含 thread_pool.h，以便使用 thread_pool_task_fn 类型和相关定义。
typedef struct task {
    struct task *next;
    thread_pool_task_fn task_fn;
    void *task_arg;
} task;
//定义了一个 task 结构体，包含一个指向下一个任务的指针 next，一个函数指针 task_fn，指向要执行的任务函数，以及一个 void* 类型的 task_arg，作为传递给任务函数的参数。
typedef struct task_queue {
    task *head;
    task *tail;
    int count;
} task_queue;
//定义了一个 task_queue 结构体，包含一个指向队列头部的指针 head，一个指向队列尾部的指针 tail，以及一个整数 count，表示队列中的任务数量。
void task_queue_init(task_queue *queue);
//初始化任务队列，将 head 和 tail 设置为 NULL，count 设置为 0。
void task_queue_push(task_queue *queue, task *new_task);
//将新任务task放入队列的尾部，并更新 tail 和 count。
task *task_queue_pop(task_queue *queue);
//从队列的头部弹出任务，并更新 head 和 count。如果队列为空，返回 NULL。
void task_queue_clear(task_queue *queue);
//清空队列，释放所有任务，并将 head 和 tail 设置为 NULL，count 设置为 0。

#endif
