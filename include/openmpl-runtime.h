#ifndef _OPENVML_RUNTIME_H_
#define _OPENVML_RUNTIME_H_

#define _GNU_SOURCE

#include <ctype.h> // isdigit
#include <dirent.h> // open dir
#include <errno.h> // bad address
#include <numa.h>
#include <numaif.h> // mbind
#include <omp.h> // openmp
#include <pthread.h>
#include <sched.h> // Linux
#include <semaphore.h>
#include <stdio.h> // fopen
#include <string.h> // strncmp
#include <sys/mman.h> // mmap
#include <time.h>
#include <unistd.h> // get cpu core

#define CONSTRUCTOR __attribute__((constructor))
#define NUMA_DIR "/sys/devices/system/node/"
#define NUMA_NODE_DIR "/sys/devices/system/node/node%d/"
#define NUMA_NODE_CPULISTS "/sys/devices/system/node/node%d/cpulist"
#define MAX_NUMA_NODES 32
#define MAX_CORE 512
#define MAX_NODE_CORE 32
#define BUFFER_OFFSET 27 // 4MB
#define BUFFER_SIZE 1 << BUFFER_OFFSET
#define ENTRY_BUFFER 100
#define MIN_COPY_PAGE_NUM 10
#define MAX_COPY_PAGE_NUM 12
#define MIN_SIZE_CORE_PROCESS 10000
#define MAX_ASYNC_CPY 4
#define MAX_PIPLINE_CORE 32
#define TASK_ARG_NUM 20
/*  #define USE_TASK  if you want to use task interface. */ 

typedef struct task_info {
    void* arg_addr[TASK_ARG_NUM];
    void* routine; // the routine the function pointer
    int arg_num;
    int pipeline_flag; // if set pipeline_flag = 1, please ensure that this will not make mistakes.
} task_info_t;

typedef struct openvml_arg {
    VML_INT n;
    VML_FLOAT *a, *b, *y, *z;
    void* other_params;
} openvml_arg_t;

typedef struct async_cpy_arg {
    void* ori_addr;
    void* dst_addr;
    size_t cpy_size;
} async_cpy_arg_t;

typedef struct openvml_queue {
    void* routine;
    openvml_arg_t arg;
    unsigned int cpu;
    int range[2]; // [start, end)
    void *sa, *sb, *sy, *sz;
    int mode;
    const char* kernel_name;
    int size; // type size double is 8, float is 4, complex double is 16, complex float is 8
    async_cpy_arg_t cpy_arg[MAX_ASYNC_CPY];
    int pipeline_flag;
} openvml_queue_t;

typedef struct openvml_numa_info {
    int num_nodes; // total num of nodes
    int num_core; // total num of cores
    int num_node_core; // the num of cores of a node
    short node_cpu_mapping[MAX_NUMA_NODES][MAX_NODE_CORE]; //
    short cpu_node_mapping[MAX_CORE]; // give a core id return node id
} openvml_numa_info_t;

typedef struct openvml_buffer {
    void* addr;
    unsigned int core;
    unsigned int node;
    int is_used;
    unsigned long offset; // unit:byte
} openvml_buffer_t;

/* auto dispatch module */
typedef struct openvml_addr_table {
    unsigned int table_id;
    unsigned int mem_map_id;
    void* alloc_addr;
    void* ori_addr;
    size_t size;
    struct openvml_addr_table* next_entry;
} openvml_addr_table_t;

typedef struct openvml_memory_block {
    unsigned int mem_id; // not use
    void* start_addr;
    size_t size;
    char kernel_name[16];
    struct openvml_memory_block* next_block;
} openvml_memory_block_t;

typedef struct openvml_memory_manager {
    openvml_memory_block_t alloced_mem_list[MAX_CORE];
    openvml_memory_block_t free_mem_list[MAX_CORE];
    openvml_addr_table_t* alloced_addr_table[MAX_CORE];
    size_t buffer_size[MAX_CORE];
} openvml_memory_manager_t;

typedef struct pthread_arg {
    task_info_t* task;
    void (*t_driver_routine)(int, task_info_t*);
    openvml_queue_t* queue;
    unsigned int core_id;
} pthread_arg_t;

/* common var */
static openvml_buffer_t buffer[MAX_CORE]; // buffer
static pthread_t async_cpy[MAX_CORE][MAX_ASYNC_CPY]; // pipeline copy

/* thread server */
int OpenVML_FUNCNAME(is_thread_init());
void OpenVML_FUNCNAME(openvml_thread_init());
int OpenVML_FUNCNAME(get_cpu_core());
int* OpenVML_FUNCNAME(task_divide_linear(int n, int thread_num));
unsigned int OpenVML_FUNCNAME(get_node_from_cpu(int cpu));
int OpenVML_FUNCNAME(get_thread_num());
int OpenVML_FUNCNAME(get_num_core_each_node());
void OpenVML_FUNCNAME(multi_thread_record(int thread_num, struct timeval* start, struct timeval* end, const char* kernel_name, int task_size));
void OpenVML_FUNCNAME(pthread_driver(void* arg));

/* memory server */
int OpenVML_FUNCNAME(is_memory_init());
void OpenVML_FUNCNAME(openvml_memory_init());
int OpenVML_FUNCNAME(get_node_from_addr(void* addr));
void OpenVML_FUNCNAME(openvml_memory_copy(void* ori_addr, void* buf_free_addr, size_t size));
void* OpenVML_FUNCNAME(openvml_buffer_alloc(int core, size_t size));

/* auto dispatch server */
void OpenVML_FUNCNAME(openvml_auto_dispatch_init());
int OpenVML_FUNCNAME(is_auto_dispatch_init());
void* OpenVML_FUNCNAME(ad_memory_alloc(int core, size_t size, void* ori_addr));
int OpenVML_FUNCNAME(ad_get_thread_num(int n));
size_t OpenVML_FUNCNAME(pipline_copy_block(pthread_t* async_t, async_cpy_arg_t* cpy_arg, void* ori_addr, void* dst_addr, size_t block_size, size_t* pos, size_t total_size));
void OpenVML_FUNCNAME(pipline_wait_copy_block(pthread_t async_cpy));
#ifdef BUFFER_NEED_FREE
void OpenVML_FUNCNAME(ad_memory_free(int core, void* ori_addr));
#endif

/* task managr */
void set_thread_num(int t_num);
void exec_thread_omp(task_info_t* tsk, void (*t_driver_routine)(int, task_info_t*));
void exec_thread_pthread(task_info_t* tsk, void (*t_driver_routine)(int, task_info_t*));
#endif