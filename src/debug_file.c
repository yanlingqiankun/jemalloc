#include "jemalloc/internal/jemalloc_internal.h"

#ifdef JEMALLOC_DEBUG
bool is_file_open = false;

bool debug_file_boot() {
    is_file_open = true;
    return false;
}

void write_to_malloc(void *ptr, uint64_t size) {
    if(__glibc_likely(is_file_open)) {
        char write_buffer[64];
        struct timeval start_time;
        gettimeofday(&start_time, NULL);
        malloc_printf("[L_S] %p %ld %ld %ld\n", ptr, size, start_time.tv_sec, start_time.tv_usec);  // lifetime-start
    }
}

void write_to_free(void *ptr) {
    if (__glibc_likely(is_file_open)) {
        char write_buffer[64];
        struct timeval end_time;
        gettimeofday(&end_time, NULL);
        malloc_printf("[L_E] %p %ld %ld\n", ptr, end_time.tv_sec, end_time.tv_usec); // lifetime-end
    }
}
void write_performance_info() {
#ifdef __x86_64__
    malloc_printf("[P_I] %ld %ld %ld %ld %ld %ld %ld %ld\n", 
    performance.memory_read[0],
    performance.memory_read[1], 
    performance.memory_write[0],
    performance.memory_write[1],
    performance.bandwidth[0],
    performance.bandwidth[1],
    performance.bandwidth[2],
    performance.bandwidth[3]); // performance-information
#elif (defined (__aarch64__))
    malloc_printf("[P_I] %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld\n", 
        performance.memory_read[0],
        performance.memory_read[1], 
        performance.memory_read[2],
        performance.memory_read[3], 
        performance.memory_write[0],
        performance.memory_write[1],
        performance.memory_write[2],
        performance.memory_write[3],
        performance.bandwidth[0],
        performance.bandwidth[1],
        performance.bandwidth[2],
        performance.bandwidth[3]
    );
#endif
}

#endif