//
// Created by shado on 2021/12/2.
//
#include "jemalloc/internal/cpu.h"

int numa_avail() {
    return numa_available();
}
