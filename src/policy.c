//
// Created by shado on 2021/12/3.
//
#include "jemalloc/internal/jemalloc_internal.h"

bool policy_boot() {
    mbind_policy = MBIND_DEFAULT;
    return (false);
}

long mbind_pages_with_weight(float *weights, int *nodes, void *addr, unsigned long size){
    int i; size_t s;
    void * temp_addr = addr;
    struct bitmask *mbind_mask = numa_bitmask_alloc(cpu_topology.node_mask.size);
    for(i = 0; i < performance.socket_num - 1; ++i) {
        numa_bitmask_setbit(mbind_mask, nodes[i]);
        s = size * weights[i];
        // page align
        s = s >> STATIC_PAGE_SHIFT << STATIC_PAGE_SHIFT;
        long ret = mbind(temp_addr, s, mbind_interleave, mbind_mask->maskp, cpu_topology.node_mask.size, MPOL_MF_STRICT);
        if (ret) {
            malloc_printf("failed to mind %p with errno = %d\n", temp_addr, errno);
            return ret;
        }
        temp_addr += s;
    }
    return mbind(temp_addr, size - (temp_addr - addr), mbind_interleave, cpu_topology.node_mask.maskp, 
                        cpu_topology.node_mask.size, MPOL_MF_STRICT);
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
    // long ret = mbind(addr, size, mbind_interleave, cpu_topology.node_mask.maskp, 
    //                     cpu_topology.node_mask.size+1, MPOL_MF_STRICT);
    long ret = mbind_pages_with_weight(performance.node_weights, performance.nodes, addr, size);
    if (ret) {
        chunk_unmap(addr, size);
        return NULL;
    }
    return addr;
}