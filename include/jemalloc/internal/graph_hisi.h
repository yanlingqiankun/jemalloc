#ifdef JEMALLOC_H_EXTERNS
#define C 2
#define M 4
#define BUS_NUM 5
#define PHYSICAL_CORE_NUM 32
#define GENERATE_SUCCESS

extern unsigned long traffic[BUS_NUM];
extern double weight[C*M];
extern int memory_traffic_index[M];

bool graph_boot();
void update_weight();
inline uint64_t get_bandwidth(int type, uint64_t x);
#endif
