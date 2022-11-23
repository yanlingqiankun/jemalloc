#include "jemalloc/internal/jemalloc_internal.h"

#define MAX(x, y) x > y ? x : y
#define MIN(x, y) x < y ? x : y 
#define INFINITY 1 << 63

#define INIT_NODES_2(root, in_merge_addr, in_id, in_bus_type1, in_bus_type2) { \
root.children = (node *)malloc(2*sizeof(node)); \
root.bus_type = (int *)malloc(2*sizeof(int));   \
root.node_id = in_id;                           \
root.num_children = 0;                       \
root.merge_addr = in_merge_addr;                \
root.bottleneck = 1 << 62;                   \
root.bus_type[0] = in_bus_type1;                \
root.bus_type[1] = in_bus_type2;\
}

#define INIT_NODES_1(root, in_merge_addr, in_id, in_bus_type1) { \
root.children = (node *)malloc(sizeof(node)); \
root.bus_type = (int *)malloc(sizeof(int));   \
root.node_id = in_id;                           \
root.num_children = 0;                       \
root.merge_addr = in_merge_addr;                \
root.bottleneck = 1 << 62;                   \
root.bus_type[0] = in_bus_type1;                \
}

#define INIT_NODES_0(root, in_merge_addr, in_id) { \
root.num_children = NULL;                    \
root.bus_type = NULL;                        \
root.node_id = in_id;                           \
root.num_children = 0;                       \
root.merge_addr = in_merge_addr;                \
root.bottleneck = 1 << 62;\
}

unsigned long traffic[BUS_NUM];
float weight[C*M];

typedef struct Node{
    int num_children;
    struct Node *children;
    int *bus_type;
    uint64_t bottleneck;
    int node_id;
    int merge_addr;
}node;
int root_num;
node root[2];

#define BUS_0(x) (-3.59563965e-10)*x*x-(3.69836472e-01)*x+2.78174121e+09
#define BUS_2(x) (-3.59563965e-10)*x*x-(3.69836472e-01)*x+2.78174121e+09
#define BUS_1(x) (-1.52224248e-16)*x*x*x+(3.37122384e-08)*x*x-2.30076880*x+4.36546674e+09
#define BUS_4(x) 1 << 63
// #define BUS(x, num) BUS##_num(x)

inline uint64_t get_bandwidth(int type, uint64_t x) {
    switch (type){
        case 0:
            return (-3.59563965e-10)*x*x-(3.69836472e-01)*x+2.78174121e+09;
        case 1:
            return (-1.52224248e-16)*x*x*x+(3.37122384e-08)*x*x-2.30076880*x+4.36546674e+09;
        case 2:
            return (-3.59563965e-10)*x*x-(3.69836472e-01)*x+2.78174121e+09;
        default:
            return 1 << 63;
    }
}

bool graph_boot() {
    root_num = 2;
    INIT_NODES_2(root[0], 0, 0, 0, 1)
    INIT_NODES_2(root[1], 2, 1, 2, 1)
    INIT_NODES_0(root[0].children[0], 0, 0)
    INIT_NODES_1(root[0].children[1], 1, 1, 2)
    INIT_NODES_0(root[1].children[0], 3, 1)
    INIT_NODES_1(root[1].children[1], 2, 0, 0)
    INIT_NODES_0(root[0].children[1].children[0], 1, 1)
    INIT_NODES_0(root[1].children[1].children[0], 2, 0)
    // init extern values
    traffic[0] = traffic[1] = traffic[2] = 0;
    weight[0] = weight[1] = weight[2] = weight[3] = 0.5;
    return false;
}

void update_weight_inside(node *n, int bus_type, uint64_t bl, float *w, int *d, uint64_t *b){
    uint64_t bw = get_bandwidth(bus_type, traffic[bus_type]);
    n->bottleneck = MIN(bw, bl);
    int d_num = 0;
    uint64_t b_sum = 0;
    if (n->num_children > 0) {
        for(int i = 0; i < n->num_children; ++i) {
            int d_child;
            uint64_t b_child;
            update_weight_inside(&n->children[i], n->bus_type[i], n->bottleneck, w, &d_child, &b_child);
            d_num += d_child;
            b_sum += b_child;
        }
        if (b_sum > n->bottleneck){
            for(int i = n->merge_addr; i < d_num+n->merge_addr; ++i){
                w[i] = w[i] * (n->bottleneck/b_sum);
            }
        }
    } else {
        w[n->merge_addr] = n->bottleneck;
    }
    *d = d_num;
    *b = b_sum;
    return;
}

void compute_percent(float *array, int len) {
    int i;
    float sum = 0;
    for(i = 0; i < len; ++i)
        sum += array[i];
    for(i = 0; i < len; ++i)
        array[i] /= sum;
    return;
}

void update_weight(){
    int i;
    for(i  = 0; i < performance.socket_num; ++i) {
        int temp_d, temp_b;
        update_weight_inside(&root[i], 4, INFINITY, &weight[i*performance.socket_num], &temp_d, &temp_b);
        compute_percent(&weight[i*performance.socket_num], performance.socket_num);
    }
}
