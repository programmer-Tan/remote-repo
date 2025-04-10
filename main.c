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

// Global variable for heap management
void *g_heap;

#define SYSCALL_setpriority 11
#define SYSCALL_getpriority 12
#define PRI_USER_MIN    0
#define PRI_USER_MAX  127
#define MAX_TASKS 128 

// 共享数据结构
typedef struct {
    int array[200];
    int frames[200 * 200];
    volatile int current_frame;
    volatile int done;
    int tid;
    int color;
    int x_offset;
} SortThread;

SortThread thread_A, thread_B;
volatile int running = 1;
int emp2 = 10;
int i,j,k;

// 优先级条位置参数
#define PRIORITY_BAR_Y (g_graphic_dev.YResolution - 100)  // 将优先级条下移
#define PRIORITY_BAR_HEIGHT 30

// 优先级管理
int thread_priorities[MAX_TASKS] = {0};

int setpriority(int tid, int prio) {
    if (tid < 0 || tid >= MAX_TASKS) return -1;
    if (prio < PRI_USER_MIN || prio > PRI_USER_MAX) return -1;
    thread_priorities[tid] = prio;
    return 0;
}

int getpriority(int tid) {
    if (tid < 0 || tid >= MAX_TASKS) return -1;
    return thread_priorities[tid];
}

void msleep(int milliseconds) {
    struct timespec ts = {
        .tv_sec = milliseconds / 1000,
        .tv_nsec = (milliseconds % 1000) * 1000000
    };
    nanosleep(&ts, NULL);
}

// 冒泡排序线程
void tsk_bubble_sort(void *pv) {
    SortThread *thread = (SortThread *)pv;
    thread->tid = task_getid();
    setpriority(thread->tid, 5); // 默认优先级
    
    for (i = 0; i < 200 && running; i++) {
        for (j = 0; j < 200 - i - 1; j++) {
            if (thread->array[j] > thread->array[j + 1]) {
                int temp = thread->array[j];
                thread->array[j] = thread->array[j + 1];
                thread->array[j + 1] = temp;
            }
        }
        
        // 保存当前帧
        for (k = 0; k < 200; k++) {
            thread->frames[i * 200 + k] = thread->array[k];
        }
        thread->current_frame = i;
        
        // 动态速度控制
        msleep(100 - getpriority(thread->tid));
    }
    thread->done = 1;
    task_exit(0);
}

// 绘制优先级条
void draw_priority_bars(int prio_A, int prio_B) {
    // 用黑色矩形覆盖整个区域来"清除"
    int y;
    for(y = PRIORITY_BAR_Y - PRIORITY_BAR_HEIGHT/2; 
    y <= PRIORITY_BAR_Y + PRIORITY_BAR_HEIGHT/2; 
    y++) {
    line(g_graphic_dev.XResolution/2 - PRI_USER_MAX*emp2 - 2, y,
        g_graphic_dev.XResolution/2 + PRI_USER_MAX*emp2 + 2, y,
        RGB(0,0,0));
    }
    
    // 绘制新的优先级条（修改颜色）
    for(i = 0; i <= prio_A; i++) {
        line(g_graphic_dev.XResolution/2 - i*emp2 - 1, 
             PRIORITY_BAR_Y - PRIORITY_BAR_HEIGHT/2,
             g_graphic_dev.XResolution/2 - i*emp2 - 1,
             PRIORITY_BAR_Y + PRIORITY_BAR_HEIGHT/2, 
             RGB(255,165,0)); // 橙色表示线程A
    }
    for(i = 0; i <= prio_B; i++) {
        line(g_graphic_dev.XResolution/2 + i*emp2 + 1,
             PRIORITY_BAR_Y - PRIORITY_BAR_HEIGHT/2,
             g_graphic_dev.XResolution/2 + i*emp2 + 1,
             PRIORITY_BAR_Y + PRIORITY_BAR_HEIGHT/2,
             RGB(0,255,0)); // 绿色表示线程B
    }
    
}

// 控制线程
void tsk_control(void *pv) {
    int prio_A = 5, prio_B = 5;
    draw_priority_bars(prio_A, prio_B);
    
    while(running) {
        int key = getchar();
        
        if(key == 0x4800 && prio_A < PRI_USER_MAX) { // 上箭头
            prio_A++;
            setpriority(thread_A.tid, prio_A);
        }
        else if(key == 0x5000 && prio_A > PRI_USER_MIN) { // 下箭头
            prio_A--;
            setpriority(thread_A.tid, prio_A);
        }
        else if(key == 0x4d00 && prio_B < PRI_USER_MAX) { // 右箭头
            prio_B++;
            setpriority(thread_B.tid, prio_B);
        }
        else if(key == 0x4b00 && prio_B > PRI_USER_MIN) { // 左箭头
            prio_B--;
            setpriority(thread_B.tid, prio_B);
        }
        else if(key == 'q') {
            running = 0;
        }
        
        draw_priority_bars(prio_A, prio_B);
    }
    task_exit(0);
}

// 主绘制函数
void draw_both_sorts() {
    int last_frame_A = -1, last_frame_B = -1;
    
    while ((!thread_A.done || !thread_B.done) && running) {
        // 绘制线程A
        if (thread_A.current_frame != last_frame_A) {
            // 清除A的上一帧
            if (last_frame_A >= 0) {
                for (j = 0; j < 200; j++) {
                    line(thread_A.x_offset, 50 + j*2,
                         thread_A.x_offset + thread_A.frames[last_frame_A*200 + j],
                         50 + j*2, RGB(0,0,0));
                }
            }
            // 绘制A的当前帧
            for (j = 0; j < 200; j++) {
                line(thread_A.x_offset, 50 + j*2,
                     thread_A.x_offset + thread_A.frames[thread_A.current_frame*200 + j],
                     50 + j*2, thread_A.color);
            }
            last_frame_A = thread_A.current_frame;
        }
        
        // 绘制线程B
        if (thread_B.current_frame != last_frame_B) {
            // 清除B的上一帧
            if (last_frame_B >= 0) {
                for (j = 0; j < 200; j++) {
                    line(thread_B.x_offset, 50 + j*2,
                         thread_B.x_offset + thread_B.frames[last_frame_B*200 + j],
                         50 + j*2, RGB(0,0,0));
                }
            }
            // 绘制B的当前帧
            for (j = 0; j < 200; j++) {
                line(thread_B.x_offset, 50 + j*2,
                     thread_B.x_offset + thread_B.frames[thread_B.current_frame*200 + j],
                     50 + j*2, thread_B.color);
            }
            last_frame_B = thread_B.current_frame;
        }
        
        msleep(16); // ~60fps
    }
}

void __main() {
    size_t heap_size = 32*1024*1024;
    void *heap_base = mmap(NULL, heap_size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON, -1, 0);
    g_heap = tlsf_create_with_pool(heap_base, heap_size);
}

void main(void *pv) {
    // 初始化数据结构
    thread_A.color = RGB(255,0,0);  // 排序过程仍用红色
    thread_A.x_offset = 100;
    thread_B.color = RGB(0,0,255);  // 排序过程仍用蓝色
    thread_B.x_offset = 400;
    
    // 初始化随机数组
    srand(time(NULL));
    for (i = 0; i < 200; i++) {
        thread_A.array[i] = rand() % 200;
        thread_B.array[i] = rand() % 200;
    }
    
    // 初始化图形
    init_graphic(0x143);
    
    // 分配栈空间
    unsigned char *stack_A = malloc(1024*1024);
    unsigned char *stack_B = malloc(1024*1024);
    unsigned char *stack_ctrl = malloc(1024*1024);
    
    // 创建线程
    task_create(stack_A + 1024*1024, &tsk_bubble_sort, &thread_A);
    task_create(stack_B + 1024*1024, &tsk_bubble_sort, &thread_B);
    task_create(stack_ctrl + 1024*1024, &tsk_control, NULL);
    
    // 主绘制循环
    draw_both_sorts();
    
    // 清理
    free(stack_A);
    free(stack_B);
    free(stack_ctrl);
    
    task_exit(0);
}