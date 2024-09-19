#include "runtime.h"
// if you need to use process a lot of batch, you need to define this macro. but it is very expensive !!!
// #define BUFFER_NEED_FREE 

#ifdef BUFFER_NEED_FREE
static openvml_memory_manager_t mem_manager;
#endif

static int auto_dispatch_initialized = 0;

static openvml_addr_table_t addr_table[MAX_CORE];
static openvml_addr_table_t* addr_table_entry[MAX_CORE]; // used for the entry of openvml_addr_table_t. it can expand if not enough
static int addr_table_entry_pos[MAX_CORE];

/* internal interface */
// check orignal addr is memory affinity
int check_addr_is_affinity(void* ori_addr)
{
    //
    int cpu = sched_getcpu();
    int mem_node = get_node_from_addr(ori_addr);
    int cpu_node = get_node_from_cpu(cpu);
    return cpu_node == mem_node;
}
// traverse addr table
int check_addr_alloced(int core, void* ori_addr, size_t size)
{
    openvml_addr_table_t* p = &addr_table[core];
    if (p->alloc_addr == NULL)
        return 0;
    if (p->ori_addr == ori_addr && p->size >= size) {
        return 1;
    }
    while (p->next_entry != NULL) {
        p = p->next_entry;
        if (p->alloc_addr == ori_addr && p->size >= size)
            return 1;
    }
    return 0;
}
//
void* find_free_block(int core, size_t size)
{
#ifdef BUFFER_NEED_FREE
    openvml_memory_block_t* free_block_list = &mem_manager.free_mem_list[core];
    if (free_block_list->start_addr != NULL) { //  find the suitable size
        if (free_block_list->size > size)
            return free_block_list->start_addr;
        openvml_memory_block_t* ptr_free_block = free_block_list;
        while (ptr_free_block->next_block != NULL) {
            ptr_free_block = ptr_free_block->next_block;
            if (ptr_free_block->size > size)
                return ptr_free_block->start_addr;
        }
    }
#endif
    void* addr = OpenVML_FUNCNAME(openvml_buffer_alloc(core, size));
    return addr;
}

// give the ori_addr find the
void* find_alloced_addr(int core, void* ori_addr)
{
    openvml_addr_table_t* p = &addr_table[core];
    if (p->ori_addr == ori_addr)
        return p->alloc_addr;
    while (p->next_entry != NULL) {
        p = p->next_entry;
        if (p->ori_addr == ori_addr)
            return p->alloc_addr;
    }
    return NULL;
}

void* thread_async_copy(void* arg_)
{
    async_cpy_arg_t* arg = (async_cpy_arg_t*)arg_;
    OpenVML_FUNCNAME(openvml_memory_copy(arg->ori_addr, arg->dst_addr, arg->cpy_size));
    return NULL;
}

/* external interface */
int OpenVML_FUNCNAME(is_auto_dispatch_init())
{
    return auto_dispatch_initialized;
}

void OpenVML_FUNCNAME(openvml_auto_dispatch_init())
{

    // 设置mem_manager
    int core = get_cpu_core();

    for (int core = 0; core < MAX_CORE; core++) {
        addr_table[core].alloc_addr = NULL;
        addr_table[core].next_entry = NULL;
        addr_table_entry[core] = (openvml_addr_table_t*)malloc(sizeof(openvml_addr_table_t) * ENTRY_BUFFER);
    }

    auto_dispatch_initialized = 1;
}
void* OpenVML_FUNCNAME(ad_memory_alloc(int core, size_t size, void* ori_addr))
{

    // check whether ori_addr is alloced
    int is_alloced = check_addr_alloced(core, ori_addr, size);
    // if not alloced
    if (is_alloced) {
        return find_alloced_addr(core, ori_addr);
    }

    // find the free block
    void* addr = find_free_block(core, size);

    // set memory block
    if (addr_table[core].alloc_addr == NULL) // first alloc
    {
        addr_table[core].alloc_addr = addr;
        addr_table[core].ori_addr = ori_addr;
    } else {
        if (addr_table_entry_pos[core] < ENTRY_BUFFER) {
            openvml_addr_table_t* alloced_entry = &addr_table_entry[core][addr_table_entry_pos[core]];
            alloced_entry->alloc_addr = addr;
            alloced_entry->ori_addr = ori_addr;
            openvml_addr_table_t* p = addr_table[core].next_entry;
            addr_table[core].next_entry = alloced_entry;
            alloced_entry->next_entry = p;
            addr_table_entry_pos[core]++;
        } else {
            openvml_addr_table_t* alloced_entry = (openvml_addr_table_t*)malloc(sizeof(openvml_addr_table_t));
            alloced_entry->alloc_addr = addr;
            alloced_entry->ori_addr = ori_addr;
            openvml_addr_table_t* p = addr_table[core].next_entry;
            addr_table[core].next_entry = alloced_entry;
            alloced_entry->next_entry = p;
        }
    }
    return addr;
}

#ifdef BUFFER_NEED_FREE
void OpenVML_FUNCNAME(ad_memory_free(int core, void* ori_addr))
{
    openvml_addr_table_t* addr_entry = &addr_table[core];
    openvml_memory_block_t* free_ptr = &mem_manager.free_mem_list[core];
    if (addr_entry->alloc_addr == NULL)
        return;
    if (addr_entry->ori_addr == ori_addr) { // first match
        if (free_ptr->start_addr == NULL) {
            free_ptr->start_addr = ori_addr;
            free_ptr->size = addr_entry->size;
        }
        openvml_memory_block_t* q = free_ptr;
        while (q->next_block != NULL) { // find the free block
            q = q->next_block;
            if (q->start_addr == NULL) {
                q->start_addr = ori_addr;
                q->size = addr_entry->size;
                return;
            }
        }
    }
    openvml_addr_table_t* p = addr_entry;
    while (p->next_entry != NULL) { 
        p = p->next_entry;
        if (p->ori_addr == ori_addr) {
            if (free_ptr->start_addr == NULL) {
                free_ptr->start_addr = ori_addr;
                free_ptr->size = addr_entry->size;
            }
            openvml_memory_block_t* q = free_ptr;
            while (q->next_block != NULL) { // find the free block
                q = q->next_block;
                if (q->start_addr == NULL) {
                    q->start_addr = ori_addr;
                    q->size = addr_entry->size;
                    return;
                }
            }
        }
    }
}
#endif

int OpenVML_FUNCNAME(ad_get_thread_num(int n))
{
    // get numa_info
    if (n < MIN_SIZE_CORE_PROCESS)
        return 1;
    int core_each_node = OpenVML_FUNCNAME(get_num_core_each_node());
    int max_core = OpenVML_FUNCNAME(get_cpu_core());
    // get the number core of a numa node have
    int min_node_process = core_each_node * MIN_SIZE_CORE_PROCESS;

    int core_num = (n + MIN_SIZE_CORE_PROCESS - 1) / MIN_SIZE_CORE_PROCESS;
    if (core_num > max_core)
        return max_core;
    int node_num = (core_num + core_each_node - 1) / core_each_node;
    if (node_num < 2)
        return core_num;
    if (node_num % 2) { // it must even
        node_num++;
    }
    return node_num * core_each_node;
}

size_t OpenVML_FUNCNAME(pipline_copy_block(pthread_t* async_cpy, async_cpy_arg_t* cpy_arg, void* ori_addr, void* dst_addr, size_t block_size, size_t* pos, size_t total_size))
{

    size_t cpy_size = (*pos + block_size > total_size) ? (total_size - *pos) : block_size;
    // 配置一个参数组
    cpy_arg->ori_addr = ori_addr;
    cpy_arg->dst_addr = dst_addr;
    cpy_arg->cpy_size = cpy_size;
    // 创建一个线程, 调用async_copy
    if (pthread_create(async_cpy, NULL, thread_async_copy, cpy_arg) != 0) {
        perror("pthread_create");
        return cpy_size;
    }
    *pos += cpy_size;
    // 放在join函数更新pos,
    return cpy_size;
}

void OpenVML_FUNCNAME(pipline_wait_copy_block(pthread_t async_cpy))
{
}