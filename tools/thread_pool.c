/**
 * thread_pool.c - Student implementation scaffold
 *
 * The public API and queue helpers are fixed. You still need to:
 * - define struct thread_pool
 * - FIFO task-queue operations
 * - choose the shared state and wake-up protocol your pool needs
 * - pool initialization and rollback on failure
 * - worker synchronization and lifecycle
 * - wait / destroy semantics
 *
 * You may add private static helpers in this file.
 */
// 需要完成的部分：
// - 定义 struct thread_pool
// - FIFO 任务队列操作
// - 选择线程池需要的共享状态和唤醒协议
// - 线程池初始化和失败时的回滚
// - 工作线程的同步和生命周期
// - wait / destroy 语义
// 可以在这个文件中添加私有的静态帮助函数。
#include <pthread.h>
#include <stdlib.h>

#include "task_queue.h"
//因为thread_pool.h已经包含在task_queue.h中，所以在thread_pool.c中包含task_queue.h就可以使用thread_pool.h中的定义了。
struct thread_pool {
    pthread_t *threads;//工作线程数组——共享
    int worker_count;//工作线程数量——共享
    task_queue tasks;//任务队列——共享

    pthread_mutex_t mutex;//互斥锁——共享
    pthread_cond_t has_task;//条件变量：当有任务提交时，唤醒等待的工作线程
    pthread_cond_t idle;//条件变量：当线程池空闲时，唤醒等待的线程池用户

    int shutdown;//标志：线程池是否正在关闭——共享
    int active_tasks;//正在执行的任务数量——共享
    /*
     * TODO: add the shared state your design needs.
     *
     * A workable design must let submit(), worker threads, wait(), and destroy()
     * agree on:
     * - whether queued work exists,
     * - whether work is currently executing,
     * - whether shutdown has started,
     * - how sleepers are woken when those facts change.
     */
};

void task_queue_init(task_queue *queue) {
    queue->head = NULL;
    queue->tail = NULL;
    queue->count = 0;
}

/* TODO A: append one task at the tail while preserving head/tail/count. */
void task_queue_push(task_queue *queue, task *new_task) {
    // (void)queue;
    // (void)new_task;
    queue->count++;
    if (queue->head == NULL && queue->tail == NULL) {
        queue->head = new_task;
        queue->tail = new_task;
        return;
    }
    queue->tail->next = new_task;
    queue->tail = new_task;
    queue->tail->next = NULL;
}
// 队列push：需要着重考虑刚开始为空的情况，以及添加任务后更新 tail 和 count 的正确性。
/* TODO A: pop one task from the head and repair tail when the queue becomes empty. */
task *task_queue_pop(task_queue *queue) {
    // (void)queue;
    if (queue->count == 0) {
        return NULL;
    }
    queue->count--;
    task *cur_task = queue->head;
    queue->head = queue->head->next;
    if (queue->head == NULL) {
        queue->tail = NULL;
    }
    return cur_task;
}
// 队列pop：需要检查队列是否是空，并且在pop之后是不是空，正确返回任务并更新 head 和 tail。
void task_queue_clear(task_queue *queue) {
    task *next_task;

    while ((next_task = task_queue_pop(queue)) != NULL) {
        free(next_task);//根据这个操作可以知道 task_queue_pop 函数返回的任务是动态分配的，需要在清空队列时释放它们的内存。
    }

    queue->head = NULL;
    queue->tail = NULL;
    queue->count = 0;
}

/*
 * TODO B: implement the worker loop.
 *
 * Required behavior:
 * - wait while no task is available
 * - exit cleanly after shutdown
 * - do not hold the synchronization lock that protects shared state
 *   while running the task
 * - after shutdown starts, do not begin any new queued task
 * - maintain shared state so wait/destroy semantics work
 */
void *thread_pool_worker_main(void *pool_arg) {
    // (void)pool_arg;
    task *cur_task = NULL;
    thread_pool *pool = (thread_pool *)pool_arg;
    while (1) {
        // 加锁保护共享部分
        pthread_mutex_lock(&pool->mutex);
        // 三种情况：1. 没有任务，等待；2. 有任务，执行；3. 线程池正在关闭，退出。
        while (pool->tasks.count == 0 && !pool->shutdown) {
            pthread_cond_wait(&pool->has_task, &pool->mutex);
            //pthread_cond_wait 第一个参数是条件变量，这里我们传 pool->has_task；第二个参数是互斥锁，这里我们传 pool->mutex。这个函数会自动释放 mutex 并阻塞当前线程，直到被 pthread_cond_signal 或 pthread_cond_broadcast 唤醒后重新获得 mutex。
            //当 has_task 条件变量被唤醒时，线程会重新获得 mutex 锁，并继续执行下面的代码。
        }
        // 说明：当线程被唤醒时，可能是因为有新任务提交了，也可能是因为线程池正在关闭了，所以需要检查这两种情况。
        // 1.线程在关闭？
        if (pool->shutdown) {
            if (pool->active_tasks == 0 && pool->tasks.count == 0) {
                pthread_cond_broadcast(&pool->idle);//唤醒等待线程池空闲的线程
            }

            pthread_mutex_unlock(&pool->mutex);
            break; //退出while(1)循环——相当于结束线程
        }
        // 2.有任务可执行？
        cur_task = task_queue_pop(&pool->tasks);
        // 取头部任务
        // 判断是否取到
        // if (cur_task == NULL) {
        //     pthread_mutex_unlock(&pool->mutex);
        //     continue; //继续等待
        // }
        // 取到任务了，增加正在执行的任务数量
        pool->active_tasks++;
        // 解锁，允许其他线程提交任务或等待线程池空闲
        pthread_mutex_unlock(&pool->mutex);
        // 执行任务
        cur_task->task_fn(cur_task->task_arg);
        // 任务执行完了，减少正在执行的任务数量
        pthread_mutex_lock(&pool->mutex);
        pool->active_tasks--;
        // 检查线程池是否空闲：没有正在执行的任务，并且队列也空了
        if (pool->active_tasks == 0 && pool->tasks.count == 0) {
            pthread_cond_broadcast(&pool->idle);//唤醒等待线程池空闲的线程
        }
        pthread_mutex_unlock(&pool->mutex);

        // 释放任务内存
        free(cur_task);
        cur_task = NULL;
    }
    return NULL;
}

/*
 * TODO B: create the thread pool.
 *
 * You need:
 * - argument validation
 * - pool / thread-array allocation
 * - queue initialization
 * - initialization of the synchronization state your design needs
 * - worker creation
 * - rollback if any step fails after earlier steps succeeded
 */
thread_pool *thread_pool_create(int worker_count) {
    // (void)worker_count;
    // 需要进行参数验证，分配线程池和线程数组，初始化任务队列和同步状态，创建工作线程，并在任何步骤失败时进行回滚。
    // 参数验证：检查 worker_count 是否为正数。
    if (worker_count <= 0) {
        return NULL;
    }
    
    // 分配 pool 结构体
    thread_pool *pool = calloc(1, sizeof(thread_pool));//calloc 第一个参数表示要分配的元素数量，第二个参数表示每个元素的大小。这里我们需要分配一个 thread_pool 结构体，所以第一个参数是 1，第二个参数是 sizeof(thread_pool)。
    if (pool == NULL) {
        return NULL;
    }

    // 分配线程数组
    pool->threads = malloc(worker_count * sizeof(pthread_t));//malloc 第一个参数表示要分配的内存大小，这里我们需要分配 worker_count 个 pthread_t 类型的元素，所以参数是 worker_count * sizeof(pthread_t)。
    if (pool->threads == NULL) {
        free(pool);// 回滚：释放之前分配的 pool 结构体内存。
        return NULL;
    }

    // 初始化任务队列
    task_queue_init(&pool->tasks);

    // 初始化同步状态
    if(pthread_mutex_init(&pool->mutex, NULL) != 0) {//NULL 表示使用默认的互斥锁属性。
        free(pool->threads);
        free(pool);
        return NULL;
    }

    if(pthread_cond_init(&pool->has_task, NULL) != 0) {
        pthread_mutex_destroy(&pool->mutex);
        free(pool->threads);
        free(pool);
        return NULL;
    }

    if(pthread_cond_init(&pool->idle, NULL) != 0) {
        pthread_cond_destroy(&pool->has_task);
        pthread_mutex_destroy(&pool->mutex);
        free(pool->threads);
        free(pool);
        return NULL;
    }

    // 初始化其他共享状态
    pool->worker_count = 0;
    pool->shutdown = 0;
    pool->active_tasks = 0;

    // 创建工作线程
    for (int i = 0; i < worker_count; i++) {
        if (pthread_create(&pool->threads[i], NULL, thread_pool_worker_main, pool) != 0) {
            // pthread_create 第一个参数是一个指向 pthread_t 类型的指针，用于存储新线程的 ID；第二个参数是线程属性，这里我们传 NULL 表示使用默认属性；第三个参数是线程函数，这里我们传 thread_pool_worker_main；第四个参数是传递给线程函数的参数，这里我们传 pool。
            // 回滚：销毁已经创建的线程，销毁同步状态，释放内存。
            // 设置 shutdown 标志，通知已经创建的线程退出。
            pthread_mutex_lock(&pool->mutex);
            pool->shutdown = 1;
            pthread_cond_broadcast(&pool->has_task);//唤醒所有等待的工作线程
            pthread_mutex_unlock(&pool->mutex);
            // 等待已经创建的线程退出
            for (int j = 0; j < i; j++) {
                pthread_join(pool->threads[j], NULL);
                // pthread_join 第一个参数是要等待的线程 ID，这里我们传 pool->threads[j]；第二个参数是一个指向 void* 类型的指针，用于存储线程函数的返回值，这里我们传 NULL 表示不需要返回值。
        }
            pthread_cond_destroy(&pool->idle);
            pthread_cond_destroy(&pool->has_task);
            pthread_mutex_destroy(&pool->mutex);
            free(pool->threads);
            free(pool);
            return NULL;
        }
        pool->worker_count++;
    }
    return pool;
}

/*
 * TODO B: submit a task safely.
 *
 * Required behavior:
 * - reject invalid input
 * - reject a pool that is already shutting down
 * - wake a waiting worker after a successful push
 * - submission may happen from inside a running worker task
 */
int thread_pool_submit(thread_pool *pool, thread_pool_task_fn task_fn, void *task_arg) {
    // (void)pool;
    // (void)task_fn;
    // (void)task_arg;
    // 拒绝无效输入、拒绝已经关闭的线程池、成功提交后唤醒等待的工作线程、允许在正在运行的工作任务内部提交任务。
    // 参数有：pool——线程池指针；task_fn——任务函数指针；task_arg——任务函数需要的参数
    // pool里面有tasks队列，submit其实是向这个队列里添加任务，所以需要对这个队列进行操作。
    // 参数验证：检查 pool 是否为 NULL，task_fn 是否为 NULL。
    if (pool == NULL || task_fn == NULL) {
        return -1;
    }

    // 创建新任务
    task *new_task = malloc(sizeof(task));
    if (new_task == NULL) {
        return -1;
    }

    // 对新任务进行初始化，task 结构体里面有三个成员：next、task_fn、task_arg。next 是指向下一个任务的指针，初始值应该是 NULL；task_fn 是任务函数指针，应该设置为传入的 task_fn 参数；task_arg 是任务函数需要的参数，应该设置为传入的 task_arg 参数。
    new_task->next = NULL;
    new_task->task_fn = task_fn;
    new_task->task_arg = task_arg;
    
    // 准备加入队列，需要查看池子状态，所以需要加锁
    pthread_mutex_lock(&pool->mutex);

    // 检查池子是否被关闭了，如果已经关闭了，就不能再提交任务了，应该释放新任务的内存并返回错误。
    if (pool->shutdown) {
        pthread_mutex_unlock(&pool->mutex);
        free(new_task);
        return -1;
    }
    
    // 将新任务加入队列
    task_queue_push(&pool->tasks, new_task);//需要两个参数：任务队列和新任务

    // 唤醒一个等待的工作线程
    pthread_cond_signal(&pool->has_task);

    // 解锁
    pthread_mutex_unlock(&pool->mutex);

    return 0;
}

/*
 * TODO C: block until the pool is idle.
 *
 * Idle means:
 * - the queue is empty
 * - no worker is still executing a task
 */
void thread_pool_wait(thread_pool *pool) { 
    // 阻塞直到线程池空闲：也就是任务队列空，active_tasks 为 0。
    // 第一步永远都是检查参数是否合法，pool 不能为 NULL。
    if (pool == NULL) {
        return;
    }

    // 第二步要访问共享状态，所以需要加锁。
    pthread_mutex_lock(&pool->mutex);

    // 检查是否空闲
    while (pool->tasks.count > 0 || pool->active_tasks > 0) {
        // 进入睡眠，等待被唤醒
        pthread_cond_wait(&pool->idle, &pool->mutex);
        //这个函数做了三件事：1. 释放 mutex 锁；2. 阻塞当前线程，直到被 pthread_cond_signal 或 pthread_cond_broadcast 唤醒；3.被唤醒之后重新获得 mutex 锁。
        // 当 idle 条件变量被唤醒时，线程会重新获得 mutex 锁，并继续执行下面的代码，重新检查线程池是否空闲。
    }
    // 线程池空闲了，解锁并返回
    pthread_mutex_unlock(&pool->mutex);
}

/*
 * TODO D: destroy the pool.
 *
 * Required behavior:
 * - let already-started tasks finish
 * - drop not-yet-started tasks
 * - free resources after all workers exit
 */
void thread_pool_destroy(thread_pool *pool) {
    if (pool == NULL) {
        return;
    }
    // 设置 shutdown 标志，通知工作线程退出
    pthread_mutex_lock(&pool->mutex);
    pool->shutdown = 1;
    pthread_cond_broadcast(&pool->has_task);//唤醒所有等待的工作线程
    pthread_mutex_unlock(&pool->mutex);

    // 等待所有工作线程退出
    for (int i = 0; i < pool->worker_count; i++) {
        pthread_join(pool->threads[i], NULL);
    }

    // 清空任务队列，释放未开始的任务
    pthread_mutex_lock(&pool->mutex);
    task_queue_clear(&pool->tasks);
    pthread_mutex_unlock(&pool->mutex);

    // 销毁同步状态
    pthread_cond_destroy(&pool->idle);
    pthread_cond_destroy(&pool->has_task);
    pthread_mutex_destroy(&pool->mutex);

    // 释放动态内存：包括线程数组和线程池结构体
    free(pool->threads);
    free(pool);
    /* Keep an explicit reference so the starter still builds with -Werror. */
}
