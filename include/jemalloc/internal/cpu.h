//
// Created by shado on 2021/12/2.
//

#define JEMALLOC_DEV_CPU_H
#include <numa.h>

#ifdef JEMALLOC_H_TYPES

typedef struct cpu_topology_s cpu_topology_t;

#endif /* JEMALLOC_H_TYPES */
/******************************************************************************/
#ifdef JEMALLOC_H_STRUCTS

struct cpu_topology_s {
    unsigned numa_nodes_num;
    unsigned core_per_node;
    struct bitmask *node_mask;

    // nodes * nodes array
    unsigned long bandwidth[1];
    float latency[1];
};

#endif /* JEMALLOC_H_STRUCTS */
/******************************************************************************/
#ifdef JEMALLOC_H_EXTERNS

cpu_topology_t cpu_topology;

bool cpu_topology_boot();
int numa_avail();

#endif /* JEMALLOC_H_EXTERNS */
/******************************************************************************/
#ifdef JEMALLOC_H_INLINES

#endif /* JEMALLOC_H_INLINES */
/******************************************************************************/
