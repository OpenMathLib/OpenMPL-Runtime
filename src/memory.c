#define _GNU_SOURCE // only for gnu
#include "runtime.h"

static int mem_initialized = 0;
int OpenVML_FUNCNAME(is_memory_init())
{
    return mem_initialized;
}

void CONSTRUCTOR OpenVML_FUNCNAME(openvml_memory_init())
{   
    if(!OpenVML_FUNCNAME(is_thread_init())) OpenVML_FUNCNAME(openvml_thread_init());
    // get the num of core
    int num_core = OpenVML_FUNCNAME(get_cpu_core());
    // map addr for buffer
    for (int i = 0; i < num_core; i++)
    {
        size_t size = BUFFER_SIZE;
        buffer[i].addr = mmap(NULL,
                              size,
                              PROT_READ | PROT_WRITE,
                              MAP_PRIVATE | MAP_ANONYMOUS,
                              -1, 0);
        if (buffer[i].addr == MAP_FAILED)
        {
            perror("mmap");
            exit(EXIT_FAILURE);
        }
        buffer[i].core = i;
        buffer[i].node = OpenVML_FUNCNAME(get_node_from_cpu(i));
        buffer[i].is_used = 0;
        buffer[i].offset = 0;

        unsigned long nodemask = 1 << buffer[i].node;
        // bind node
        if (mbind(buffer[i].addr, size, MPOL_BIND, &nodemask, sizeof(nodemask) * 8, 0) != 0)
        {
            perror("mbind");
            munmap(buffer[i].addr, size);
            exit(EXIT_FAILURE);
        }
    }
    mem_initialized = 1;
}

int OpenVML_FUNCNAME(get_node_from_addr(void *addr))
{
    int status;
    int page_info[BUFFER_SIZE / getpagesize()]; // store the mapping between node and pages
    int *a = addr;                              // write some data;
    status = move_pages(0, BUFFER_SIZE / getpagesize(), &addr, NULL, page_info, 0);
    if (status == -1)
    {
        perror("move_pages");
        munmap(addr, BUFFER_SIZE);
        exit(EXIT_FAILURE);
    }

    // check the first page addr
    if (page_info[0] == -EFAULT)
    {
        perror("The page is not mapped.\n");
        return -1;
    }

    return page_info[0];
}

// alloc page aligned
void *OpenVML_FUNCNAME(openvml_buffer_alloc(int core, size_t size)){
    if(buffer[core].offset + size - 1 > BUFFER_SIZE) return NULL; // the buffer size is not enough
    void *addr = buffer[core].addr + buffer[core].offset;
    size_t page_size = sysconf(_SC_PAGE_SIZE);
    size_t page_num = (size + page_size - 1) / page_size;
    buffer[core].offset += page_num * page_size; // 0 1 2 3 4
    //double *a = addr; // write a data
    //a[0] = 0.0;
    return addr;
}

void *OpenVML_FUNCNAME(openvml_get_free_buffer_addr(int core))
{
    if(buffer[core].offset > BUFFER_SIZE) return NULL; //the capacity of buffer is not enough
    void* buf_addr;
    if(buffer[core].is_used){
        buf_addr = buffer[core].addr + buffer[core].offset;
    }
    else{
        buf_addr =  buffer[core].addr;
    }
    return buf_addr;
}


void OpenVML_FUNCNAME(openvml_memory_copy(void *ori_addr, void *dst_addr, size_t size))
{   
    if(ori_addr == NULL) return;
    if(size == 0) return;
#ifdef OPENVML_TIMER
    struct timeval st, ed;
    OpenVML_FUNCNAME(tick)(&st);
#endif
    memcpy(dst_addr, ori_addr,size);
#ifdef OPENVML_TIMER
    unsigned long long duration_us = OpenVML_FUNCNAME(tock)(&st, &ed);
    int dst_node = get_node_from_addr(dst_addr);
    int ori_node = get_node_from_addr(ori_addr);
    OpenVML_FUNCNAME(multi_thread_record)(ori_node, &st, &ed, "ad", dst_node);
#endif
}
