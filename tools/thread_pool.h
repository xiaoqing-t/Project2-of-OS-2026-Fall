/**
 * thread_pool.h - Thread Pool public API
 *
 * Students implement thread_pool.c against these public contracts.
 * Queue scaffolding for Phase A lives in task_queue.h.
 * The pool's private layout belongs in thread_pool.c.
 */

#ifndef THREAD_POOL_H
#define THREAD_POOL_H

typedef struct thread_pool thread_pool; //struct thread_pool 是一个结构体类型（完整名称），而thread_pool是这个结构体的别名
typedef void (*thread_pool_task_fn)(void *task_arg);//thread_pool_task_fn 是一个函数指针类型，指向一个接受 void* 参数并返回 void 的函数

/**
 * Create a thread pool with worker_count worker threads.
 *
 * Return a pool handle on success, or NULL on failure.
 * Also return NULL when worker_count <= 0.
 */
thread_pool *thread_pool_create(int worker_count);//创建一个线程池，包含 worker_count 个工作线程。成功时返回一个线程池句柄，失败时返回 NULL。当 worker_count <= 0 时也返回 NULL。

/**
 * Submit one task to the pool.
 *
 * Return values:
 *   0   submission succeeded
 *  -1   invalid input, stopped pool, or task allocation failure
 *
 * Thread-safe: multiple callers may submit concurrently, including workers.
 */
int thread_pool_submit(thread_pool *pool, thread_pool_task_fn task_fn, void *task_arg);
//提交一个任务到线程池。成功时返回 0，失败时返回 -1（无效输入、已停止的线程池或任务分配失败）。
//task_fn 是一个函数指针，指向要执行的任务函数；task_arg 是传递给任务函数的参数。多个调用者可以同时提交任务，包括工作线程。
/**
 * Block until the queue is empty and no worker is still running a task.
 *
 * Thread-safe: may be called repeatedly.
 */
void thread_pool_wait(thread_pool *pool);
//阻塞直到队列为空且没有工作线程正在运行任务。线程安全：可以重复调用。
/**
 * Destroy the pool.
 *
 * Required semantics:
 * - let already-running tasks finish
 * - discard queued-but-not-started tasks
 * - accept NULL safely
 *
 * This function should not run concurrently with other thread_pool_* calls.
 */
void thread_pool_destroy(thread_pool *pool);
//销毁线程池，需要把已经在运行的任务完成，丢弃已排队但未开始的任务，并且安全地接受 NULL。这个函数不应该与其他 thread_pool_* 调用并发运行。
#endif
