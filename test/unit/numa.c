//
// Created by shado on 2021/12/2.
//

#include "test/jemalloc_test.h"

static void 
numa_avail_verify() {
    int expected = 0;
    int ret = numa_avail();
    assert_x_eq(expected, ret,
    "NUMA available mismatch for numa_avail_test(): expected %d but got %d", expected, ret);
}

TEST_BEGIN(numa_avail_test)
{
    numa_avail_verify();
}
TEST_END

int
main(void) {

    return (test(
        numa_avail_test
    ));
}