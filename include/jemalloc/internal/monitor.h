#ifdef JEMALLOC_H_TYPES
#ifdef __x86_64__
#define PERF_INDEX_NUM 8   // cycle stalled-cycle + read write local_r local_w + upi_rx upi_tx
typedef enum
{
    NEHALEM_EP = 26,
    NEHALEM = 30,
    ATOM = 28,
    ATOM_2 = 53,
    CENTERTON = 54,
    BAYTRAIL = 55,
    AVOTON = 77,
    CHERRYTRAIL = 76,
    APOLLO_LAKE = 92,
    DENVERTON = 95,
    SNOWRIDGE = 134,
    CLARKDALE = 37,
    WESTMERE_EP = 44,
    NEHALEM_EX = 46,
    WESTMERE_EX = 47,
    SANDY_BRIDGE = 42,
    JAKETOWN = 45,
    IVY_BRIDGE = 58,
    HASWELL = 60,
    HASWELL_ULT = 69,
    HASWELL_2 = 70,
    IVYTOWN = 62,
    HASWELLX = 63,
    BROADWELL = 61,
    BROADWELL_XEON_E3 = 71,
    BDX_DE = 86,
    SKL_UY = 78,
    KBL = 158,
    KBL_1 = 142,
    CML = 166,
    CML_1 = 165,
    ICL = 126,
    ICL_1 = 125,
    RKL = 167,
    TGL = 140,
    TGL_1 = 141,
    ADL = 151,
    BDX = 79,
    KNL = 87,
    SKL = 94,
    SKX = 85,
    ICX_D = 108,
    ICX = 106,
    END_OF_MODEL_LIST = 0x0ffff    
}cpu_model_t;

typedef enum {
    INTEL,
    AMD,
    OTHERS
} cpu_brand_t;

#elif (defined (__aarch64__))
typedef enum{
    KUNPENG920 = 0xd01
}cpu_model_t;

typedef enum {
    ARM = 0x41,
    Broadcom = 0x42,
    Cavium = 0x43,
    DigitalEquipment = 0x44,
    HiSilicon = 0x48,
    Infineon = 0x49,
    Freescale = 0x4D,
    NVIDIA = 0x4E,
    APM = 0x50,
    Qualcomm = 0x51,
    Marvell = 0x56,
    Intel = 0x69
} cpu_brand_t;
#endif 

#define SOCKET_EVENT_MASK 0xff00
#define MONITOR_INTERVAL 1000000
#define MONITOR_TIMES 1e6/MONITOR_INTERVAL

typedef struct {
    cpu_brand_t brand;
    cpu_model_t model;
    int family;
    int stepping;
} cpu_info_t;

typedef enum {
    //CPU performance
    STALL_CYCLE_BACK_END,
    CYCLE,
    CACHE_MISS,
    CACHE_LOCAL_MISS,
    INSTRUCTIONS,


    // node performance record in core
    MEMORY_READ,
    MEMORY_WRITE,
    LOCAL_READ,
    LOCAL_WRITE,
    
    // uncore performance
    UPI_RECEIVE,
    UPI_TRANSMIT,
    IMC_READ,
    IMC_WRITE,

    // hisi performance
    HISI_FLUX_RD,
    HISI_FLUX_WR,
    HISI_RX_OUTER
} event_config;

typedef enum {
    CORE,
    BUS,
    SOCKET,
    CPU
} perf_range_t;

typedef struct {
    uint64_t nr;
    uint64_t values[];
} read_format;

typedef struct {
    perf_range_t range;
    event_config config;
    struct perf_event_attr attr;
    read_format *read_fmt;
    int u_id;    // uncore_id
    int s_id; // socket_id
    int fd;
} event_s;

typedef enum {
    UPDATING,
    INUSE,
    SUCCESS,
} status;

typedef struct {
    // unsigned int core_num;
    unsigned int socket_num;
    unsigned int bus_num;
    unsigned int perf_num;
    unsigned int imc_num;

    // cpu
    uint64_t cpu_cycle;
    uint64_t *instructions;

    // bus
    int *link0;
    int *link1;
    uint64_t *bandwidth;

    // socket
    // uint64_t *remote_access;
    uint64_t *memory_read;
    uint64_t *memory_write;

    // index and perf_data_list
    int *evesel_index;
    event_s *evesel;

    float *node_weights;
    int *nodes;
    bool runing;

    uint64_t all_memory_write;
    uint64_t all_memory_read;
}performance_t;

#endif /* JEMALLOC_H_TYPES */
/******************************************************************************/
#ifdef JEMALLOC_H_EXTERNS

bool monitor_boot();
bool monitor_destroy();

extern cpu_info_t cpu_info;
extern performance_t performance;

extern pthread_t monitor_thread_handle;

#endif /* JEMALLOC_H_EXTERNS */
/******************************************************************************/
#ifdef JEMALLOC_H_INLINES


#endif /* JEMALLOC_H_INLINES */