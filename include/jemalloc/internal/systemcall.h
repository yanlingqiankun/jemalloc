//
// Created by shado on 2021/12/22.
//
#ifdef JEMALLOC_H_EXTERNS

long JEMALLOC_ATTR(weak) get_mempolicy(int *policy, unsigned long *nmask,
                        unsigned long maxnode, void *addr,
                        unsigned flags);

long JEMALLOC_ATTR(weak) mbind(void *start, unsigned long len, int mode,
	const unsigned long *nmask, unsigned long maxnode, unsigned flags);

int JEMALLOC_ATTR(weak) perf_event_open(struct perf_event_attr *attr, 
    pid_t pid, int cpu, int group_fd, unsigned long flags);

#endif /* JEMALLOC_H_EXTERNS */
/******************************************************************************/
