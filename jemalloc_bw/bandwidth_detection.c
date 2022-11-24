#define _GNU_SOURCE
#include "../include/jemalloc/internal/jemalloc_internal.h"

#define BACKGROUND_MEM_SIZE 1 << 24
#define TASK_MEM_SIZE 1 << 30
#define TASK_LOOP 5
#define INIT_INTERVAL 1000000

volatile bool finish;
unsigned long traffic[3];

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
int get_task(task_t **t) {
    switch (cpu_info.brand)
    {
        case INTEL:
            switch (cpu_info.model) 
            {
                case SKX:
                    if (cpu_topology.numa_nodes_num == 2) {
                        *t = (task_t *)malloc(sizeof(task_t) * 2);
                        if (!(*t)) {
                            return -1;
                        }
                        (*t)[0].tt = NtoM;
                        (*t)[0].node1 = 0;
                        (*t)[0].counter1 = &traffic[0];
                        (*t)[1].tt = NtoN;
                        (*t)[1].node1 = 0;
                        (*t)[1].node2 = 1;
                        (*t)[1].counter1 = &traffic[2];
                        (*t)[1].counter2 = &traffic[1];
                        return 2;
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
    if (!memory) return NULL;
    if (usec == 0) {
        while (!finish) {
            memset(memory, 0, BACKGROUND_MEM_SIZE);
        }
    } else {
         while (!finish) {
            usleep(usec);
            memset(memory, 0, BACKGROUND_MEM_SIZE);
        }
    }
    numa_free(memory, BACKGROUND_MEM_SIZE);
    return memory;
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
    numa_free(memory, TASK_MEM_SIZE);
    return memory;
}

void execute_task (task_t t, FILE *bandwidth_file) {
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
    for (; thread_num < cpu_topology.core_per_node; ++thread_num) {
        for(usec = 0; usec <= 1e6; usec *= 10) {
            bp = buffer;
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
            usleep(INIT_INTERVAL);
            uint64_t status = 0;
            if (t.tt == NtoM) {
                for(i = 0; i < 10; ++i) {
                    status += *t.counter1;
                    usleep(2*MONITOR_INTERVAL);
                }
            } else {
                for(i = 0; i < 10; ++i) {
                    status += *t.counter2;
                    usleep(2*MONITOR_INTERVAL);
                }
            }
            status = status / 10;
            status = status * (1e6 / MONITOR_INTERVAL);
            pthread_t write_task_p;

            gettimeofday(&start, NULL);
            pthread_create(&write_task_p, NULL, write_task, buffer);
            int aff_ret = pthread_setaffinity_np(write_task_p, sizeof(cpu_set_t), &mask);
            if (aff_ret) {
                printf("failed to set affinity\n");
            }
            pthread_join(write_task_p, NULL);
            gettimeofday(&end, NULL);
            finish = true;
            for(i = 0; i < thread_num; ++i) {
                pthread_join(p[i], NULL);
            }
            long temp = TASK_MEM_SIZE;
            double diff_time = (end.tv_sec - start.tv_sec + (end.tv_usec - start.tv_usec) / 1000000.0);
            uint64_t bandwidth = temp * TASK_LOOP / diff_time;
            fprintf(bandwidth_file, "%lu\t%lu\n", status, bandwidth);
            printf("%uus-%dthreads : \t%lu\t%lu\n", usec, i, status, bandwidth);
            if(usec == 0) usec = 1;
        }
        fflush(bandwidth_file);
    }
    fclose(bandwidth_file);
}

int main() {
    task_t *t;
    int i;
    if (cpu_topology_boot()){
        printf("failed to boot cpu_topology\n");
        return 1;
    }
    if (monitor_boot()) {
        printf("failed to boot monitor\n");
        return 1;
    }

    int ret = get_task(&t);
    if (ret <= 0) {
        return ret;
    }
    for(i = 0; i < ret; ++i) {
        char filename[128]; 
        FILE *bandwidth_file;
        sprintf(filename, "./bandwidth_%d.txt", i);
        bandwidth_file = fopen(filename, "w");
        if (!bandwidth_file) {
            printf("failed to open file\n");
            return -1;
        }
        execute_task(t[i], bandwidth_file);
    }
    return 0;
}
