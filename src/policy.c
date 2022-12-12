//
// Created by shado on 2021/12/3.
//
#include "jemalloc/internal/jemalloc_internal.h"

int threads_num_of_node[M];

bool policy_boot() {
    mbind_policy = MBIND_DEFAULT;
    memset(threads_num_of_node, 0, sizeof(int)*M);
    return (false);
}

JEMALLOC_INLINE int get_max(double *array, int len) {
    int i, ret = 0;
    for(i = 0; i < len; i++) {
        if (array[i] > array[ret]){
            ret = i;
        }
    }
    return ret;
}

long mbind_pages_with_weight_ordered(double *weights, void *addr, unsigned long size){
    int i, j; size_t s;
    int max_index = 0, sec_max_index, first_loop = 1;
    max_index = get_max(weights, performance.socket_num);
    sec_max_index = (max_index + 1) % performance.socket_num;
    void * temp_addr = addr;
    struct bitmask *mbind_mask = numa_bitmask_alloc(cpu_topology.node_mask.size);
    for(i = 0; i < performance.socket_num - 1; ++i) {        
        for(j = 0; j < performance.socket_num; ++j) {
            if(weights[max_index] - weights[j] > 1e-5 && weights[j] - weights[sec_max_index] > 1e-5) {
                sec_max_index = j;
            }
        }
        numa_bitmask_setbit(mbind_mask, max_index);
        s = size * (weights[max_index] - weights[sec_max_index]) * (i+1);
        // page align
        s = s >> STATIC_PAGE_SHIFT << STATIC_PAGE_SHIFT;
        long ret = mbind(temp_addr, s, mbind_interleave, mbind_mask->maskp, cpu_topology.node_mask.size, MPOL_MF_STRICT);
        if (ret) {
            malloc_printf("failed to mind %p with errno = %d\n", temp_addr, errno);
            return ret;
        }
        temp_addr += s;
        max_index = sec_max_index;
    }
    numa_bitmask_setbit(mbind_mask, max_index);
    return mbind(temp_addr, size - (temp_addr - addr), mbind_interleave, mbind_mask->maskp, 
                        cpu_topology.node_mask.size, MPOL_MF_STRICT);
}

long mbind_pages_with_weight(double *weights, void *addr, unsigned long size) {
    int i; size_t s;
    void *temp_addr = addr;
    struct bitmask *mbind_mask = numa_bitmask_alloc(cpu_topology.node_mask.size);
    for(i = 0; i < performance.socket_num - 1; ++i) {
        numa_bitmask_setbit(mbind_mask, i);
        s = size * weights[i];
        s = s >> STATIC_PAGE_SHIFT << STATIC_PAGE_SHIFT;
        long ret = mbind(temp_addr, s, mbind_bind, mbind_mask->maskp, cpu_topology.node_mask.size, MPOL_MF_STRICT);
        if (ret) {
            malloc_printf("failed to mind %p with errno = %d\n", temp_addr, errno);
            return ret;
        }
        numa_bitmask_clearbit(mbind_mask, i);
        temp_addr += s;
    }
    numa_bitmask_setbit(mbind_mask, i);
    return mbind(temp_addr, size-(temp_addr - addr), mbind_bind, mbind_mask->maskp,
                        cpu_topology.node_mask.size, MPOL_MF_STRICT);
}

// place the pages directed by mbind_policy
void *place_pages(void *addr, size_t size) {
    if (numa_initialized == false){
        return addr;
    } 

    if (addr == NULL) {
        return NULL;
    }

    if (size & PAGE_MASK) {
        return addr;
    }
    int cpu, node;
    int err = getcpu(&cpu, &node);
    if (err == -1) {
        malloc_printf("failed to get cpu id with errno = %d\n", errno);
        return NULL;
    }
    // long ret = mbind(addr, size, mbind_interleave, cpu_topology.node_mask.maskp, 
    //                     cpu_topology.node_mask.size+1, MPOL_MF_STRICT);
    long ret = mbind_pages_with_weight_ordered(&weight[performance.socket_num*node], addr, size);
    if (ret) {
        chunk_unmap(addr, size);
        return NULL;
    }
    return addr;
}

int get_node_of_thread(){
    int i, min_idx = 0;
    for(i = 0; i < M; ++i) {
        min_idx = threads_num_of_node[min_idx] < threads_num_of_node[i] ? min_idx : i ;
        if(get_bandwidth(memory_traffic_index[i], traffic[memory_traffic_index[i]]) 
                < 0.9 * get_bandwidth(memory_traffic_index[i], 0))
            if(threads_num_of_node[i] < PHYSICAL_CORE_NUM) {
            continue;
            return i;
        }
    }
    return min_idx;
}

void *mbind_chunk(void *addr, size_t size, arena_t *arena){
    if (numa_initialized == false){
        return addr;
    }
    if (!arena->node_mask)
        return addr;
    int node_id = arena->node_id;
    struct bitmask *mbind_mask = numa_bitmask_alloc(cpu_topology.node_mask.size);
    numa_bitmask_setbit(mbind_mask, node_id);
    long ret = mbind(addr, size, mbind_bind, mbind_mask->maskp, cpu_topology.node_mask.size, MPOL_MF_STRICT);
    if(ret) {
        malloc_printf("failed to mind chunk %p with errno = %d\n", addr, errno);
        return NULL;
    }
    return;
}

void schedule_myself_to_arena(arena_t *arena){
    if (!arena->node_mask) {
        return;
    }
    cpu_set_t mask;
    pthread_t myself = pthread_self();
    CPU_ZERO(&mask);
    memcpy(mask.__bits, arena->node_mask->maskp, arena->node_mask->size/sizeof(sizeof(unsigned long)));
    if (pthread_setaffinity_np(myself, sizeof(cpu_set_t), &mask)) {
        malloc_printf("error to schedule thread %ld to node %d\n", myself, arena->node_id);
    }
    return;
}
