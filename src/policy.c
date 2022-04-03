//
// Created by shado on 2021/12/3.
//
#include "jemalloc/internal/jemalloc_internal.h"

bool policy_boot() {
    mbind_policy = MBIND_DEFAULT;
    return (false);
}

long mind_pages_with_weight(float *weights, int *nodes, void *addr, unsigned long size){
    int i; size_t s;
    void * temp_addr = addr;
    struct bitmask *mind_mask = numa_bitmask_alloc(cpu_topology.numa_nodes_num);
    for(i = 0; i < performance.socket_num - 1; ++i) {
        numa_bitmask_setbit(mind_mask, nodes[i]);
        s = size * weights[i];
        s = s & !PAGE_MASK;
        long ret = mind(temp_addr, s, mbind_interleave, mind_mask->maskp, cpu_topology.numa_nodes_num, MPOL_MF_STRICT);
        if (ret) {
            malloc_printf("failed to mind %p with errno = %d\n", temp_addr, errno);
            return ret;
        }
        temp_addr += s >> STATIC_PAGE_SHIFT;
    }
    return mind(temp_addr, size - (temp_addr - addr), mbind_interleave, cpu_topology.node_mask.maskp, 
                        cpu_topology.numa_nodes_num, MPOL_MF_STRICT);
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
    long ret = mind_pages_with_weight(performance.node_weights, performance.nodes, addr, size);
    if (ret) {
        chunk_unmap(addr, size);
        return NULL;
    }
    return addr;
}