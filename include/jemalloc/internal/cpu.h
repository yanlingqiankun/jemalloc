//
// Created by shado on 2021/12/2.
//
#ifdef JEMALLOC_H_TYPES

typedef struct cpu_topology_s cpu_topology_t;

#endif /* JEMALLOC_H_TYPES */
/******************************************************************************/
#ifdef JEMALLOC_H_STRUCTS

#if defined(__x86_64__) || defined(__i386__)
#define NUMA_NUM_NODES  128
#else
#define NUMA_NUM_NODES  2048
#endif

typedef struct {
        unsigned long n[NUMA_NUM_NODES/(sizeof(unsigned long)*8)];
} nodemask_t;

struct bitmask {
	unsigned long size; /* number of bits in the map */
	unsigned long *maskp;
};

struct cpu_topology_s {
    unsigned numa_nodes_num;
    unsigned core_per_node;
    unsigned core_num;
    struct bitmask node_mask;

    // nodes * nodes array
    unsigned long bandwidth[1];
    float latency[1];
};

#endif /* JEMALLOC_H_STRUCTS */
/******************************************************************************/
#ifdef JEMALLOC_H_EXTERNS
extern bool numa_initialized;
extern cpu_topology_t cpu_topology;

bool cpu_topology_boot();
int numa_avail();
int find_first_cpu_of_node(int);

#endif /* JEMALLOC_H_EXTERNS */
/******************************************************************************/
#ifdef JEMALLOC_H_INLINES

#endif /* JEMALLOC_H_INLINES */
/******************************************************************************/
