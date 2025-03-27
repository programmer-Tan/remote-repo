/*
 * vim: filetype=c:fenc=utf-8:ts=4:et:sw=4:sts=4
 */
#include <inttypes.h>
#include <stddef.h>
#include <math.h>
#include <stdio.h>
#include <sys/mman.h>
#include <syscall.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <time.h>
#include "graphics.h"

extern void *tlsf_create_with_pool(void* mem, size_t bytes);
extern void *g_heap;

// 全局变量
int a[200] = {0}; // 冒泡排序的数组
int b[200] = {0}; // 插入排序的数组
int c[200] = {0}; // 快速排序的数组
int k,i,j;
int bubble_frames[200 * 200] = {0}; // 冒泡排序的每一帧
int insert_frames[200 * 200] = {0}; // 插入排序的每一帧
int quick_frames[200 * 200] = {0}; // 快速排序的每一帧
volatile int bubble_done = 0; // 冒泡排序完成标志
volatile int insert_done = 0; // 插入排序完成标志
volatile int quick_done = 0; // 快速排序完成标志

/**
 * GCC insists on __main
 *    http://gcc.gnu.org/onlinedocs/gccint/Collect2.html
 */
void __main()
{
    size_t heap_size = 32 * 1024 * 1024; // 32MB 堆大小
    void *heap_base = mmap(NULL, heap_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (heap_base == MAP_FAILED) {
        printf("Failed to allocate heap memory!\n");
        return;
    }

    // 初始化 TLSF 内存池
    g_heap = tlsf_create_with_pool(heap_base, heap_size);
    if (g_heap == NULL) {
        printf("Failed to create TLSF memory pool!\n");
        return;
    }

    printf("TLSF memory pool initialized successfully!\n");
}

// 冒泡排序
void tsk_bubble_sort(void *pv)
{
    printf("This is bubble sort task with tid=%d\r\n", task_getid());

    // 冒泡排序
    for (i = 0; i < 200; i++) {
        for (j = 0; j < 200 - i - 1; j++) {
            if (a[j] > a[j + 1]) {
                // 交换 a[j] 和 a[j + 1]
                int temp = a[j];
                a[j] = a[j + 1];
                a[j + 1] = temp;
            }
        }

        // 每一轮排序后，将当前 a 数组的状态复制到 bubble_frames 数组中
        for (k = 0; k < 200; k++) {
            bubble_frames[i * 200 + k] = a[k];
        }
    }

    bubble_done = 1; // 任务完成，设置标志
    task_exit(0); // 不能直接 return，必须调用 task_exit
}

// 插入排序
void tsk_insert_sort(void *pv)
{
    printf("This is insert sort task with tid=%d\r\n", task_getid());
    int key;
    // 插入排序
    for (i = 1; i < 200; i++) {
        key = b[i];
        j = i - 1;

        while (j >= 0 && b[j] > key) {
            b[j + 1] = b[j];
            j = j - 1;
        }
        b[j + 1] = key;

        // 每一轮排序后，将当前 b 数组的状态复制到 insert_frames 数组中
        for (k = 0; k < 200; k++) {
            insert_frames[(i - 1) * 200 + k] = b[k];
        }
    }
    // 保存最后一帧
    for (k = 0; k < 200; k++) {
        insert_frames[(200 - 1) * 200 + k] = b[k];
    }
    insert_done = 1; // 任务完成，设置标志
    task_exit(0); // 不能直接 return，必须调用 task_exit
}

// 快速排序
void tsk_quick_sort(void *pv)
{
    printf("This is quick sort task with tid=%d\r\n", task_getid());

    // 快速排序递归函数
    void quick_sort(int arr[], int low, int high, int *frame_index) {
        if (low < high) {
            int pivot = arr[high]; // 选择最后一个元素作为基准
            int i = low - 1;

            for (j = low; j < high; j++) {
                if (arr[j] < pivot) {
                    i++;
                    int temp = arr[i];
                    arr[i] = arr[j];
                    arr[j] = temp;
                }
            }

            int temp = arr[i + 1];
            arr[i + 1] = arr[high];
            arr[high] = temp;

            // 每一轮排序后，将当前 c 数组的状态复制到 quick_frames 数组中
            for (k = 0; k < 200; k++) {
                quick_frames[(*frame_index) * 200 + k] = c[k];
            }
            (*frame_index)++;

            quick_sort(arr, low, i, frame_index); // 递归排序左半部分
            quick_sort(arr, i + 2, high, frame_index); // 递归排序右半部分
        }
    }

    int frame_index = 0;
    quick_sort(c, 0, 199, &frame_index); // 调用快速排序

    // 保存最后一帧
    for (k = 0; k < 200; k++) {
        quick_frames[(frame_index - 1) * 200 + k] = c[k];
    }

    quick_done = 1; // 任务完成，设置标志
    task_exit(0); // 不能直接 return，必须调用 task_exit
}

/**
 * 绘制帧函数
 * 参数说明：
 *   data: 存储每一帧数据的数组
 *   frame_count: 总帧数
 *   lines_per_frame: 每帧绘制的行数
 *   line_spacing: 行间距
 *   start_x: 绘制的起始 X 坐标
 *   start_y: 绘制的起始 Y 坐标
 *   delay_ms: 每帧之间的延迟时间（毫秒）
 *   color: 线条颜色
 */
void draw_frames(int *data, int frame_count, int lines_per_frame, 
    int line_spacing, int start_x, int start_y, int delay_ms, int color)
{
    struct timespec sleep_time = {
       .tv_sec = 0,
       .tv_nsec = delay_ms * 1000000 // 使用 delay_ms 参数
    };

    int j, i;
    for (j = 0; j < frame_count; j++) {
        for (i = 0; i < lines_per_frame; i++) {
            // 缩短线条长度，避免重叠
            line(start_x, start_y + line_spacing * i, start_x +
                 data[i + j * lines_per_frame] * (line_spacing / 2), start_y + line_spacing * i, color);
        }

        nanosleep(&sleep_time, NULL);

        // 如果不是最后一帧，清除当前帧
        if (j < frame_count - 1) {
            for (i = 0; i < lines_per_frame; i++) {
                line(start_x, start_y + line_spacing * i, start_x + 
                    data[i + j * lines_per_frame] * (line_spacing / 2), start_y + line_spacing * i, RGB(0, 0, 0));
            }
        }
    }
}

/**
 * 第一个运行在用户模式的线程所执行的函数
 */
void main(void *pv)
{
    printf("task #%d: I'm the first user task(pv=0x%08x)!\r\n",
           task_getid(), pv);

    // 初始化随机数种子
    srand(time(NULL));

    // 生成随机数组
    for (i = 0; i < 200; i++) {
        a[i] = (rand() % 1000) / 5; // 生成 0 到 200 之间的随机数
        b[i] = a[i]; // 复制到插入排序的数组
        c[i] = a[i]; // 复制到快速排序的数组
    }

    // 申请用户栈
    unsigned char *stack_bubble = (unsigned char *)malloc(1024 * 1024);
    unsigned char *stack_insert = (unsigned char *)malloc(1024 * 1024);
    unsigned char *stack_quick = (unsigned char *)malloc(1024 * 1024);

    if (stack_bubble == NULL || stack_insert == NULL || stack_quick == NULL) {
        printf("Failed to allocate stack memory!\n");
        return;
    }

    // 创建排序任务
    int tid_bubble, tid_insert, tid_quick;
    tid_bubble = task_create(stack_bubble + 1024 * 1024, &tsk_bubble_sort, (void *)0);
    tid_insert = task_create(stack_insert + 1024 * 1024, &tsk_insert_sort, (void *)0);
    tid_quick = task_create(stack_quick + 1024 * 1024, &tsk_quick_sort, (void *)0);

    printf("Bubble sort task created with tid=%d\n", tid_bubble);
    printf("Insert sort task created with tid=%d\n", tid_insert);
    printf("Quick sort task created with tid=%d\n", tid_quick);

    // 忙等待，直到所有任务完成
    while (!bubble_done || !insert_done || !quick_done);

    // 初始化图形模式
    init_graphic(0x143);

    // 绘制排序过程
    draw_frames(bubble_frames, 200, 200, 3, 0, 0, 50, RGB(255, 0, 0)); // 冒泡排序，红色
    draw_frames(insert_frames, 200, 200, 3, 250, 0, 50, RGB(0, 255, 0)); // 插入排序，绿色
    draw_frames(quick_frames, 200, 200, 3, 500, 0, 50, RGB(0, 0, 255)); // 快速排序，蓝色

    // 释放用户栈
    free(stack_bubble);
    free(stack_insert);
    free(stack_quick);

    // 主线程进入无限循环
    while (1);
    task_exit(0);
}