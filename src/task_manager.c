#include "openmpl-runtime.h"


static unsigned int runtime_thread_num;

void set_thread_num(int t_num)
{
    runtime_thread_num = t_num;
}

void exec_thread_omp(task_info_t* tsk, void (*t_driver_routine)(int, task_info_t*))
{
#pragma omp parallel num_threads(runtime_thread_num)
    {
        // run do function
#pragma omp for schedule(static) nowait
        for (int core = 0; core < runtime_thread_num; core++) {
            t_driver_routine(core, &tsk[core]);
        }
    }
}

void exec_thread_pthread(task_info_t* tsk, void (*t_driver_routine)(int, task_info_t*))
{
    pthread_t* threads = (pthread_t*)malloc(sizeof(pthread_t) * runtime_thread_num);
    pthread_arg_t* thr_arg = (pthread_arg_t*)malloc(sizeof(pthread_arg_t) * runtime_thread_num);
    for (int core = 0; core < runtime_thread_num; core++) {
        thr_arg[core].core_id = core;
        thr_arg[core].task = &tsk[core];
        thr_arg[core].t_driver_routine = t_driver_routine;
        OpenVML_FUNCNAME(pthread_driver(&thr_arg[core]));
    }
}