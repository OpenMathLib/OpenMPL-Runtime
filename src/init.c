#include "openmpl-runtime.h"
static int initialized = 0;

void CONSTRUCTOR OpenVML_FUNCNAME(runtime_init())
{
    OpenVML_FUNCNAME(openvml_thread_init());
    if (!OpenVML_FUNCNAME(is_thread_init())){
        printf("Runtime error: thread init failed.\n");
        initialized = -1;
    }
    OpenVML_FUNCNAME(openvml_memory_init());
    if (!OpenVML_FUNCNAME(is_memory_init())){
        printf("Runtime error: memory init failed.\n");
        initialized = -2;
    }
    OpenVML_FUNCNAME(openvml_auto_dispatch_init());
    if (!OpenVML_FUNCNAME(is_auto_dispatch_init())){
        printf("Runtime error: auto dispatch init failed.\n");
        initialized = -3;
    }
    if(initialized == 0) initialized = 1;
}
int OpenVML_FUNCNAME(is_runtime_init()){
    return initialized;
}