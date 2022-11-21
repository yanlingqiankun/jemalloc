//
// Created by shado on 2021/12/22.
//

#include "jemalloc/internal/jemalloc_internal.h"

long JEMALLOC_ATTR(weak) get_mempolicy(int *policy, unsigned long *nmask,
                        unsigned long maxnode, void *addr,
                        unsigned flags)
{
    return syscall(__NR_get_mempolicy, policy, nmask,
                   maxnode, addr, flags);
}

long JEMALLOC_ATTR(weak) mbind(void *start, unsigned long len, int mode,
	const unsigned long *nmask, unsigned long maxnode, unsigned flags)
{
    return syscall(__NR_mbind, (long)start, len, mode, (long)nmask,
				maxnode, flags);
}

int JEMALLOC_ATTR(weak) perf_event_open(struct perf_event_attr *attr, 
    pid_t pid, int cpu, int group_fd, unsigned long flags)
{
    return syscall(__NR_perf_event_open, attr, pid, cpu, group_fd, flags);
}

int JEMALLOC_ATTR(weak) getcpu(unsigned int *core, unsigned int *socket) {
    return syscall(__NR_getcpu, core, socket, NULL);
}