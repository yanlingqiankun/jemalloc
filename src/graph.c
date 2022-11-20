#include "jemalloc/internal/jemalloc_internal.h"

#define MAX(x, y) x > y ? x : y
#define MIN(x, y) x < y ? x : y 
#define INFINITY 1 << 63

typedef struct Node{
    int num_children;
    struct Node *children;
    int *bus_type;
    uint64_t bottleneck;
    int node_id;
    int merge_addr;
}node;
int root_num;
node *root;

#define BUS_0(x) (-3.59563965e-10)*x*x-(3.69836472e-01)*x+2.78174121e+09
#define BUS_2(x) (-3.59563965e-10)*x*x-(3.69836472e-01)*x+2.78174121e+09
#define BUS_1(x) (-1.52224248e-16)*x*x*x+(3.37122384e-08)*x*x-2.30076880*x+4.36546674e+09
#define BUS_4(x) 1 << 63
#define BUS(x, num) BUS##_num(x)

bool graph_init() {
    root_num = 2;
    node temp_node_0[2]; 
    {
        temp_node_0[0].num_children = 2;
        int temp_bus_type[2] = {0, 1};        
        node temp_node_1[2];
        {
            temp_node_1[0].num_children = 0;
            temp_node_1[0].node_id = 0;
            temp_node_1[0].merge_addr = 0;
        }
        {
            temp_node_1[1].num_children = 1;
            int temp_bus_type[1] = {1};
            node temp_node_2[1];
            {
                temp_node_2[0].num_children = 0;
                temp_node_2[0].node_id = 1;
            }
            temp_node_1[1].bus_type = temp_bus_type;
            temp_node_1[1].children = temp_node_2;
            temp_node_1[1].merge_addr = 1;
        }
        temp_node_0[0].bus_type = temp_bus_type;
        temp_node_0[0].children = temp_node_1;
        temp_node_0[0].merge_addr = 0;
    }
    {
        temp_node_0[1].num_children = 2;
        int temp_bus_type[2] = {2, 1};
        node temp_node_1[2];
        {
            temp_node_1[0].num_children = 0;
            temp_node_1[0].node_id = 1;
            temp_node_1[0].merge_addr = 3;
        }
        {            
            temp_node_1[1].num_children = 1;
            int temp_bus_type[1] = {0};
            node temp_node_2[1];
            {
                temp_node_2[0].num_children = 0;
                temp_node_2[0].node_id = 0;
                temp_node_2[0].merge_addr = 2;
            }
            temp_node_1[1].bus_type = temp_bus_type;
            temp_node_1[1].children = temp_node_2;
            temp_node_1[1].merge_addr = 2;
        }
        temp_node_0[1].bus_type = temp_bus_type;
        temp_node_0[1].children = temp_node_1;
        temp_node_0[1].merge_addr = 2;
    }
    root = temp_node_0;
    // init extern values
    traffic[0] = traffic[1] = traffic[2] = 0;
    weight[0] = weight[1] = weight[2] = weight[3] = 0.5;

}

void update_weight_inside(node *n, int bus_type, uint64_t bl, float *w, int *d, uint64_t *b){
    uint64_t bw = BUS(bus_type, traffic[bus_type]);
    n->bottleneck = MIN(bw, bl);
    int d_num = 0;
    uint64_t b_sum = 0;
    if (n->num_children > 0) {
        for(int i = 0; i < n->num_children; ++i) {
            int *d_child;
            uint64_t *b_child;
            update_weight_inside(&n->children[i], n->bus_type[i], n->bottleneck, w, d_child, b_child);
            d_num += *d_child;
            b_sum += *b_child;
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

void update_weight(){
    int i;
    for(i  = 0; i < performance.socket_num; ++i) {
        int temp_d, temp_b;
        update_weight_inside(&root[i], 4, INFINITY, &weight[i*performance.socket_num], temp_d, temp_b);
    }
}
