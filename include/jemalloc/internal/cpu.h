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
extern struct bitmask *numa_all_nodes_ptr;


bool cpu_topology_boot();
int numa_avail();
int find_first_cpu_of_node(int);
void copy_bitmask_to_bitmask(struct bitmask *bmpfrom, struct bitmask *bmpto);
struct bitmask *get_cpu_mask(int node_id);
struct bitmask *numa_bitmask_alloc(unsigned int n);
struct bitmask *numa_bitmask_clearbit(struct bitmask *bmp, unsigned int i);
struct bitmask * numa_bitmask_setbit(struct bitmask *bmp, unsigned int i);
int numa_bitmask_isbitset(const struct bitmask *bmp, unsigned int i);
void numa_bitmask_free(struct bitmask *bmp);

#endif /* JEMALLOC_H_EXTERNS */
/******************************************************************************/
#ifdef JEMALLOC_H_INLINES

#endif /* JEMALLOC_H_INLINES */
/******************************************************************************/
