//
// Created by shado on 2021/12/3.
//
#include "jemalloc/internal/jemalloc_internal.h"

bool policy_boot() {
    mbind_policy = MBIND_DEFAULT;
    return (false);
}

// place the pages directed by mbind_policy
void *place_pages(void *addr, size_t size) {
    if (numa_initialized == false){
        malloc_printf("failed to init numa\n");
        return addr;
    } 
        
    if (addr == NULL) {
        return NULL;
    }

    if (size & PAGE_MASK) {
        return addr;
    }
    long ret = mbind(addr, size, mbind_interleave, cpu_topology.node_mask.maskp, 
                        cpu_topology.node_mask.size+1, MPOL_MF_STRICT);
    
    if (ret) {
        chunk_unmap(addr, size);
        return NULL;
    }
    return addr;
}