//
// Created by shado on 2021/12/3.
//
#ifdef JEMALLOC_H_TYPES

#define MPOL_DEFAULT     0
#define MPOL_PREFERRED   1
#define MPOL_BIND        2
#define MPOL_INTERLEAVE  3
#define MPOL_LOCAL       4

/* Flags for set_mempolicy, specified in mode */
#define MPOL_F_NUMA_BALANCING	(1 << 13) /* Optimize with NUMA balancing if possible */

/* Flags for get_mem_policy */
#define MPOL_F_NODE    (1<<0)   /* return next il node or node of address */
				/* Warning: MPOL_F_NODE is unsupported and
				   subject to change. Don't use. */
#define MPOL_F_ADDR     (1<<1)  /* look up vma using address */
#define MPOL_F_MEMS_ALLOWED (1<<2) /* query nodes allowed in cpuset */

/* Flags for mbind */
#define MPOL_MF_STRICT  (1<<0)  /* Verify existing pages in the mapping */
#define MPOL_MF_MOVE	(1<<1)  /* Move pages owned by this process to conform to mapping */
#define MPOL_MF_MOVE_ALL (1<<2) /* Move every page to conform to mapping */

typedef enum {
    mbind_default       = MPOL_DEFAULT,
    mbind_perferred     = MPOL_PREFERRED,
    mbind_bind          = MPOL_BIND,
    mbind_interleave    = MPOL_INTERLEAVE,
    mbind_local         = MPOL_LOCAL
} mbind_t;
#define MBIND_DEFAULT   MPOL_INTERLEAVE

#endif /* JEMALLOC_H_TYPES */
/******************************************************************************/
#ifdef JEMALLOC_H_STRUCTS


#endif /* JEMALLOC_H_STRUCTS */
/******************************************************************************/
#ifdef JEMALLOC_H_EXTERNS

mbind_t mbind_policy;
extern int threads_num_of_node[M];

bool policy_boot();
void *place_pages(void *addr, size_t size);
int get_node_of_thread();
void *mbind_chunk(void *addr, size_t size, arena_t *arena);
void schedule_myself_to_arena(arena_t *arena);

#endif /* JEMALLOC_H_EXTERNS */
/******************************************************************************/
#ifdef JEMALLOC_H_INLINES


#endif /* JEMALLOC_H_INLINES */
/******************************************************************************/

