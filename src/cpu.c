//
// Created by shado on 2021/12/2.
//
#include "jemalloc/internal/jemalloc_internal.h"

int numa_avail() {
    return numa_available();
}

bool cpu_topology_boot() {
    if (!numa_avail()) {
        return true;
    }
    cpu_topology.numa_nodes_num = numa_num_possible_nodes();
    cpu_topology.node_mask = numa_get_mems_allowed();
    return false;
}
