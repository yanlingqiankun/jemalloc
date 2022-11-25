#ifdef JEMALLOC_H_EXTERNS
#define C 2
#define M 2
#define BUS_NUM 3
#define GENERATE_SUCCESS

extern unsigned long traffic[BUS_NUM];
extern double weight[C*M];

bool graph_boot();
void update_weight();
#endif
