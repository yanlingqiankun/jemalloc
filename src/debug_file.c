#include "jemalloc/internal/jemalloc_internal.h"

#ifdef JEMALLOC_DEBUG
bool is_file_open = false;

bool debug_file_boot() {
    is_file_open = true;
    return false;
}

bool write_to_malloc(void *ptr, uint64_t size) {
    if(__glibc_likely(is_file_open)) {
        char write_buffer[64];
        struct timeval start_time;
        gettimeofday(&start_time, NULL);
        malloc_printf("[LIFETIME-START] %p %ld %ld %ld\n", ptr, size, start_time.tv_sec, start_time.tv_usec);
        return false;
    }
    return true;
}

bool write_to_free(void *ptr) {
    if (__glibc_likely(is_file_open)) {
        char write_buffer[64];
        struct timeval end_time;
        gettimeofday(&end_time, NULL);
        malloc_printf("[LIFETIME-END] %p %ld %ld\n", ptr, end_time.tv_sec, end_time.tv_usec);
        return false;
    }
    return true;
}

#endif