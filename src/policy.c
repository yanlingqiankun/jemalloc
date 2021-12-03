//
// Created by shado on 2021/12/3.
//
#include "jemalloc/internal/jemalloc_internal.h"

bool 
policy_boot() {
    mbind_policy = MBIND_DEFAULT;
    return (false);
}

// place the pages directed by mbind_policy
void *
place_pages(void *addr, size_t size) {
    if (addr == NULL) {
        return NULL;
    }

    if (size & PAGE_MASK) {
        return addr;
    }

    long ret = mbind(addr, size, mbind_bind, cpu_topology.node_mask, 
                        cpu_topology.numa_nodes_num, MPOL_MF_STRICT);
    if (ret) {
        chunk_unmap(addr, size);
        return NULL;
    }
    return addr;
}