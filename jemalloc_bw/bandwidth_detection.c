#include "../include/jemalloc/internal/jemalloc_internal.h"

void je_malloc_printf(const char *__restrict __format, ...){
    va_list valist;
    printf(__format, valist);
}

void *je_base_alloc(size_t size) {
    return malloc(size);
}

int main() {
    if (cpu_topology_boot()){
        return 1;
    }
    if (monitor_boot()) {
        return 1;
    }
}
