#ifdef JEMALLOC_H_EXTERNS
#define C 2
#define M 2
#define BUS_NUM 3
#define GENERATE_SUCCESS

extern unsigned long traffic[BUS_NUM];
extern float weight[C*M];

void update_weight();
#endif


