#define _GNU_SOURCE
#include "../include/jemalloc/internal/jemalloc_internal.h"
#include <sched.h>

#define BACKGROUND_MEM_SIZE 1 << 22
#define TASK_MEM_SIZE 1 << 20
#define TASK_LOOP 20
#define MONITOR_INTERNAL 10

volatile bool finish;
FILE *bandwidth_file;

void je_malloc_printf(const char *__restrict __format, ...){
    va_list valist;
    printf(__format, valist);
}

void *je_base_alloc(size_t size) {
    return malloc(size);
}

typedef enum{
    NtoM,
    NtoN,
} task_type;

typedef struct
{
    task_type tt;
    int node1;
    int node2;
    uint64_t *counter1;
    uint64_t *counter2;
} task_t;

#ifdef __x86_64__
int get_task(task_t *t) {
    switch (cpu_info.brand)
    {
        case INTEL:
            switch (cpu_info.family) 
            {
                case 85:
                    if (cpu_topology.numa_nodes_num == 2) {
                        t = (task_t *)malloc(sizeof(task_t) * 2);
                        if (!t) {
                            return -1;
                        }
                        t[0].tt = NtoM;
                        t[0].node1 = 0;
                        t[0].counter1 = &performance.memory_write[0];
                        t[1].tt = NtoN;
                        t[1].node1 = 0;
                        t[1].node2 = 1;
                        t[1].counter1 = &performance.memory_write[0];
                        t[1].counter2 = &performance.bandwidth[2];
                        return 1;
                    }
            }
            break;
        default:
            break;
    }
    return 0;
}
#elif (defined (__arch64__))
int get_task(task_t *t) {
    return -3;
}
#endif

static void dombind(void *mem, size_t size, int pol, struct bitmask *bmp)
{
	if (mbind(mem, size, pol, bmp ? bmp->maskp : NULL, bmp ? bmp->size + 1 : 0,
		  0) < 0)
        printf("mbind error\n");
		return;
}

void *numa_alloc_onnode(size_t size, int node) {
    char *mem;
	struct bitmask *bmp;

	bmp = numa_bitmask_alloc(cpu_topology.numa_nodes_num);
	numa_bitmask_setbit(bmp, node);
	mem = mmap(0, size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS,
		   0, 0);
	if (mem == (char *)-1)
		mem = NULL;
	else
		dombind(mem, size, MPOL_BIND, bmp);
	numa_bitmask_free(bmp);
	return mem;
}

void numa_free(void *mem, size_t size)
{
	munmap(mem, size);
}

void *write_background(void *buffer) {
    char *bp = buffer;
    task_t t;
    unsigned int usec;
    void *memory;
    memcpy(&t, bp, sizeof(task_t)); bp += sizeof(task_t);
    memcpy(&usec, bp, sizeof(unsigned int)); bp += sizeof(unsigned int);
    if (t.tt == NtoM) {
        memory = numa_alloc_onnode(BACKGROUND_MEM_SIZE, t.node1);
    } else if (t.tt == NtoN) {
        memory = numa_alloc_onnode(BACKGROUND_MEM_SIZE, t.node2);
    }
    if (!memory) return;
    while (!finish) {
        usleep(usec);
        memset(memory, 0, BACKGROUND_MEM_SIZE);
    }
    numa_free(memory, BACKGROUND_MEM_SIZE);
    return;
}

void *write_task(void *buffer){
    int i;
    char *bp = buffer;
    task_t t;
    unsigned int usec;
    void *memory;
    memcpy(&t, bp, sizeof(task_t)); bp += sizeof(task_t);
    memcpy(&usec, bp, sizeof(unsigned int)); bp += sizeof(unsigned int);
    if (t.tt == NtoM) {
        memory = numa_alloc_onnode(TASK_MEM_SIZE, t.node1);
    } else if (t.tt == NtoN) {
        memory = numa_alloc_onnode(TASK_MEM_SIZE, t.node2);
    }
    for(i = 0; i < TASK_LOOP; ++i) {
        memset(memory, 0, TASK_MEM_SIZE);
    }
}

void execute_task (task_t t) {
    struct timeval start, end;
    int i;
    unsigned int usec = 0;
    int thread_num =0;
    // package parameters
    char buffer[1024];
    char *bp = buffer;
    cpu_set_t mask;
    CPU_ZERO(&mask);
    struct bitmask *cpumask = get_cpu_mask(t.node1);
    memcpy(mask.__bits, cpumask->maskp, cpumask->size/sizeof(unsigned long));
    for (; thread_num < cpu_topology.core_per_node/2; ++thread_num) {
        for(; usec < 1; usec *= 10) {
            finish = false;
            memcpy(bp, &t, sizeof(task_t)); bp += sizeof(task_t);
            memcpy(bp, &usec, sizeof(unsigned int)); bp += sizeof(unsigned int);
            pthread_t *p = (pthread_t *)malloc(sizeof(pthread_t)*thread_num);
            if (!p) {
                printf("failed to malloc pthread_t\n");
                exit(1);
            }
            for(i = 0; i < thread_num; ++i) {
                pthread_create(&p[i], NULL, write_background, buffer);
                int aff_ret = pthread_setaffinity_np(p[i], sizeof(cpu_set_t), &mask);
                if (aff_ret) {
                    printf("failed to set affinity\n");
                }
            }
            uint64_t status = 0;
            if (t.tt == NtoM) {
                for(i = 0; i < 10; ++i) {
                    status += *t.counter1;
                    usleep(10e5);
                }
            }
            status = status / 10;
            status = status * (10e6 / MONITOR_INTERNAL);
            pthread_t write_task_p;

            gettimeofday(&start, NULL);
            pthread_create(&write_task_p, NULL, write_task, buffer);
            int aff_ret = pthread_setaffinity_np(p[i], sizeof(cpu_set_t), &mask);
            if (aff_ret) {
                printf("failed to set affinity\n");
            }
            pthread_join(write_task_p, NULL);
            gettimeofday(&end, NULL);
            finish = true;
            for(i = 0; i < thread_num; ++i) {
                pthread_join(p[i], NULL);
            }
            uint64_t bandwidth = TASK_MEM_SIZE * TASK_LOOP / (end.tv_sec - start.tv_sec);
            fprintf(bandwidth_file, "%lu\t%lu\n", status, bandwidth);
        }
        fflush(bandwidth_file);
    }
}

int main() {
    task_t *t;
    int i;
    if (cpu_topology_boot()){
        return 1;
    }
    if (monitor_boot(MONITOR_INTERNAL)) {
        return 1;
    }

    int ret = get_task(t);
    if (ret <= 0) {
        return ret;
    }
    bandwidth_file = fopen("./bandwidth.txt", "w");
    if (!bandwidth_file) {
        printf("failed to open file\n");
        return 1;
    }
    for(i = 0; i < ret; ++i) {
        execute_task(t[i]);
    }
    fclose(bandwidth_file);
}
