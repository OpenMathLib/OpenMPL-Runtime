#include "openmpl-runtime.h"

static int thread_initialized = 0;
static openvml_numa_info_t numa_info;
static int range[MAX_CORE + 1] = { 0 };

/* internal interface */
// if is numa, return the num of nodes(>1), else return 1.
int numa_check()
{
    struct dirent* entry;
    DIR* dp = opendir(NUMA_DIR);
    if (dp == NULL) {
        return 1;
    }
    int numa_node_count = 0;
    while (entry = readdir(dp)) {
        if ((strncmp(entry->d_name, "node", 4) == 0) && isdigit(entry->d_name[4])) {
            numa_node_count++;
        }
    }
    closedir(dp);
    return numa_node_count;
}

// get the mapping between core and node
void numa_mapping()
{
    if (numa_info.num_nodes == 1)
        return; // not numa;
    char cpulist_path[256];
    char buffer[256];
    int max_num_n2c = 0;
    int total_core_num = 0;
    for (int node = 0; node < numa_info.num_nodes; node++) {
        snprintf(cpulist_path, sizeof(cpulist_path), NUMA_NODE_CPULISTS, node);
        FILE* cpu_file = fopen(cpulist_path, "r");
        if (cpu_file == NULL) {
            continue;
        }
        if (fgets(buffer, sizeof(buffer), cpu_file) == NULL) {
            fclose(cpu_file);
            continue;
        }
        fclose(cpu_file);
        // parse cpulist file contents
        char* token = strtok(buffer, ",");
        int cpu_num = 0;
        while (token != NULL) {
            int start, end;
            if (sscanf(token, "%d-%d", &start, &end) == 2) {
                for (int i = 0; i < end - start + 1; i++) {
                    numa_info.node_cpu_mapping[node][cpu_num] = start + i;
                    cpu_num++;
                    total_core_num++;
                    numa_info.cpu_node_mapping[start + i] = node;
                }
            } else if (sscanf(token, "%d", &start) == 1) {
                numa_info.node_cpu_mapping[node][cpu_num] = start;
                cpu_num++;
                total_core_num++;
                numa_info.cpu_node_mapping[start] = node;
            }
            token = strtok(NULL, ",");
        }
        if (max_num_n2c < cpu_num)
            max_num_n2c = cpu_num;
    }

    // get num_node_core
    numa_info.num_node_core = max_num_n2c;
    numa_info.num_core = total_core_num;
}

/* external interface */
void OpenVML_FUNCNAME(multi_thread_record(int thread_num, struct timeval* start, struct timeval* end, const char* kernel_name, int task_size))
{
    FILE* file = fopen("multi_time_profile.csv", "a");
    if (file == NULL) {
        return;
    }
    unsigned long long start_us = start->tv_sec * 1000000ULL + start->tv_usec;
    unsigned long long end_us = end->tv_sec * 1000000ULL + end->tv_usec;
    fprintf(file, "%s,%d,%d,%llu,%llu,%llu\n", kernel_name, thread_num, task_size, start_us, end_us, end_us - start_us);
    return;
}

int OpenVML_FUNCNAME(is_thread_init())
{
    return thread_initialized;
}

void OpenVML_FUNCNAME(openvml_thread_init())
{

    numa_info.num_nodes = numa_check();
    // numa_info.num_nodes = 1;
    numa_mapping();
    if (numa_info.num_core == 0) {
        numa_info.num_core = sysconf(_SC_NPROCESSORS_ONLN);
    }

    thread_initialized = 1;
}

int OpenVML_FUNCNAME(get_cpu_core())
{

    if (numa_info.num_core == 0) {
        numa_info.num_core = sysconf(_SC_NPROCESSORS_ONLN);
    }
    return numa_info.num_core;
}

int OpenVML_FUNCNAME(get_num_core_each_node()){
    return numa_info.num_node_core;
}

int OpenVML_FUNCNAME(get_thread_num())
{
    const char* thread_value = getenv("OPENVML_THREAD_NUM");
    if (thread_value) {
        return atoi(thread_value);
    } else { // not set openvml thread num
        return OpenVML_FUNCNAME(get_cpu_core());
    }
}

unsigned int OpenVML_FUNCNAME(get_node_from_cpu(int cpu))
{
    return numa_info.cpu_node_mapping[cpu];
}

int* OpenVML_FUNCNAME(task_divide_linear(int n, int thread_num))
{   
    int width = (n + thread_num - 1) / thread_num;
    for (int i = 1; i <= thread_num; i++) {
        if (range[i - 1] + width > n)
            range[i] = n;
        else
            range[i] = range[i - 1] + width;
    }

    return range;
}


void OpenVML_FUNCNAME(pthread_driver(void* arg)){
#ifdef USE_TASK
    pthread_arg_t* pth_arg = (pthread_arg_t*)arg;
    void (*t_driver_routine)(int, task_info_t*) = pth_arg->t_driver_routine;
    t_driver_routine(pth_arg->core_id, pth_arg->task);
#endif
}

