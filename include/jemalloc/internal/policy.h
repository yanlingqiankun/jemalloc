//
// Created by shado on 2021/12/3.
//
#ifdef JEMALLOC_H_TYPES

typedef enum {
    mbind_default       = MPOL_DEFAULT,
    mbind_perferred     = MPOL_PREFERRED,
    mbind_bind          = MPOL_BIND,
    mbind_interleave    = MPOL_INTERLEAVE
} mbind_t;
#define MBIND_DEFAULT   MPOL_INTERLEAVE

#endif /* JEMALLOC_H_TYPES */
/******************************************************************************/
#ifdef JEMALLOC_H_STRUCTS


#endif /* JEMALLOC_H_STRUCTS */
/******************************************************************************/
#ifdef JEMALLOC_H_EXTERNS

mbind_t mbind_policy;

bool policy_boot();
void * place_pages(void *addr, size_t size);

#endif /* JEMALLOC_H_EXTERNS */
/******************************************************************************/
#ifdef JEMALLOC_H_INLINES


#endif /* JEMALLOC_H_INLINES */
/******************************************************************************/

