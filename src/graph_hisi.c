#include "jemalloc/internal/jemalloc_internal.h"

#define MAX(x, y) x > y ? x : y
#define MIN(x, y) x < y ? x : y 
#define INFINITY 1 << 31
#define DOUBLE_INFINITY 1e31

#define INIT_NODES_3(root, in_merge_addr, in_id, in_bus_type1, in_bus_type2, in_bus_type3) { \
root.children = (node *)malloc(3*sizeof(node)); \
root.bus_type = (int *)malloc(3*sizeof(int));   \
root.node_id = in_id;                           \
root.num_children = 3;                       \
root.merge_addr = in_merge_addr;                \
root.bottleneck = INFINITY;                   \
root.bus_type[0] = in_bus_type1;                \
root.bus_type[1] = in_bus_type2;\
root.bus_type[2] = in_bus_type3;\
}

#define INIT_NODES_2(root, in_merge_addr, in_id, in_bus_type1, in_bus_type2) { \
root.children = (node *)malloc(2*sizeof(node)); \
root.bus_type = (int *)malloc(2*sizeof(int));   \
root.node_id = in_id;                           \
root.num_children = 2;                       \
root.merge_addr = in_merge_addr;                \
root.bottleneck = INFINITY;                   \
root.bus_type[0] = in_bus_type1;                \
root.bus_type[1] = in_bus_type2;\
}

#define INIT_NODES_1(root, in_merge_addr, in_id, in_bus_type1) { \
root.children = (node *)malloc(sizeof(node)); \
root.bus_type = (int *)malloc(sizeof(int));   \
root.node_id = in_id;                           \
root.num_children = 1;                       \
root.merge_addr = in_merge_addr;                \
root.bottleneck = INFINITY;                   \
root.bus_type[0] = in_bus_type1;                \
}

#define INIT_NODES_0(root, in_merge_addr, in_id) { \
root.num_children = NULL;                    \
root.bus_type = NULL;                        \
root.node_id = in_id;                           \
root.num_children = 0;                       \
root.merge_addr = in_merge_addr;                \
root.bottleneck = INFINITY;\
}

unsigned long traffic[BUS_NUM];
double weight[C*M];
int memory_traffic_index[M];

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

uint64_t get_bandwidth(int type, uint64_t x) {
    switch (type){
        case 0:
        case 1:
        case 2:
        case 3:
            return (2.79567663e-26)*x*x*x*x+(-6.49128816e-17)*x*x*x+(3.25052311e-08)*x*x+(-7.96465043e+00)*x+(1.72546156e+10);
        case 4:
            return (-6.20968696e-17)*x*x*x+(1.26085859e-08)*x*x+(-4.91191362e-01)*x+(9.26791201e+09);
        default:
            return DOUBLE_INFINITY;
    }
}

bool graph_boot() {
    root_num = 2;
    INIT_NODES_3(root[0], 0, 0, 0, 1, 4)
    INIT_NODES_3(root[1], 0, 1, 2, 3, 4)

    INIT_NODES_0(root[0].children[0], 0, 0)
    INIT_NODES_0(root[0].children[1], 1, 1)
    INIT_NODES_2(root[0].children[2], 2, 1, 2, 3)
    INIT_NODES_0(root[0].children[2].children[0], 2, 2)
    INIT_NODES_0(root[0].children[2].children[1], 3, 3)

    INIT_NODES_0(root[1].children[0], 2, 2)
    INIT_NODES_0(root[1].children[1], 3, 3)
    INIT_NODES_2(root[1].children[2], 0, 0, 0, 1)
    INIT_NODES_0(root[1].children[2].children[0], 0, 0)
    INIT_NODES_0(root[1].children[2].children[1], 1, 1)

    traffic[0] = traffic[1] = traffic[2] = traffic[3] = traffic[4] = 0;
    weight[0] = weight[3] = weight[1] = weight[2] = weight[4] = weight[5] = weight[6] = weight[7] = 0;
    memory_traffic_index[0] = 0;
    memory_traffic_index[1] = 1;
    memory_traffic_index[2] = 2;
    memory_traffic_index[3] = 3;
    return false;
}

void update_weight_inside(node *n, int bus_type, uint64_t bl, double *w, int *d, uint64_t *b){
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
                w[i] = w[i] * (n->bottleneck/(b_sum+0.0));
            }
        }
    } else {
        w[n->node_id] = n->bottleneck;
    }
    *d = 1 > d_num ? 1 : d_num;
    *b = b_sum > n->bottleneck ? b_sum : n->bottleneck;
    return;
}

void compute_percent(double *array, int len) {
    int i;
    double sum = 0;
    for(i = 0; i < len; ++i)
        sum += array[i];
    for(i = 0; i < len; ++i)
        array[i] /= sum;
    return;
}

void update_weight(){
    int i;
    for(i  = 0; i < C; ++i) {
        int temp_d;
        uint64_t temp_b;
        update_weight_inside(&root[i], 5, INFINITY, &weight[i*M], &temp_d, &temp_b);
        compute_percent(&weight[i*M], M);
    }
}
