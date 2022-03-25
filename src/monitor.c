#include "jemalloc/internal/jemalloc_internal.h"

#define MC_CH_PCI_PMON_CTL_EN (1 << 22)
#define cpuid(func,eax,ebx,ecx,edx)\
    __asm__ __volatile__ ("cpuid":\
    "=a" (eax), "=b" (ebx),"=c" (ecx), "=d" (edx):\
    "a" (func));

#define NULL_CHECK(CALL) \
{\
    long temp = CALL;\
    if(temp==NULL)\
    {\
        return true;\
    } \
}

#define INIT_LINK(i,src,dst)\
{   \
    performance.link0[i] = src; performance.link1[i] = dst;\
}

cpu_info_t cpu_info;
performance_t performance;

pthread_t monitor_thread_handle;

bool cpu_model_boot(){
    #ifdef __x86_64__
        unsigned int eax, ebx, ecx, edx;
        union {
            char cbuf[16];
            unsigned int  ibuf[16 / sizeof(unsigned int)];
        } buf;
        cpuid(0x0, eax, ebx, ecx, edx);
        buf.ibuf[0] = ebx;
        buf.ibuf[1] = edx;
        buf.ibuf[2] = ecx;
        if (strncmp(buf.cbuf, "GenuineIntel", 4*3) == 0) {
            // intel cpu;
            unsigned long result;
            cpuid(0x1, eax, ebx, ecx, edx);
            result = eax;
            cpu_info.brand = INTEL;
            cpu_info.family = (result & 0x0F00) >> 8 | (result & 0xF000) >> 16;
            cpu_info.model = (result & 0xF0) >> 4 | (result & 0xF0000) >> 12;
            cpu_info.stepping = result & 0xF;
        } else {
            // TODO: AMD cpu
        }        

    #elif (defined (__arch64__))
    // TODO: ARM64 cpu

    #endif
    return false;
}

int read_event_type(char *filename){
    int type = -1;
    FILE *type_file = fopen(filename, "r");
    if (type_file == NULL) {
        malloc_printf("failed to open file : %s\n", filename);
    }
    fscanf(type_file, "%d", &type);
    fclose(type_file);
    return type;
}

void event_attr_init(struct perf_event_attr *attr, bool is_group, event_config config, int uncore_id) {
    memset(attr, 0, sizeof(struct perf_event_attr));
    attr->size = sizeof(struct perf_event_attr);
    attr->sample_period = 0;
    attr->sample_type = 0;
    attr->read_format = is_group ? PERF_FORMAT_GROUP : 0; /* PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING |
                          PERF_FORMAT_ID | PERF_FORMAT_GROUP ; */
    attr->disabled = 1;
    attr->inherit = 0;
    attr->pinned = 0;
    attr->exclusive = 0;
    attr->exclude_user = 0;
    attr->exclude_kernel = 0;
    attr->exclude_hv = 0;
    attr->exclude_idle = 0;
    attr->mmap = 0;
    attr->comm = 0;
    attr->freq = 0;
    attr->inherit_stat = 0;
    attr->enable_on_exec = 0;
    attr->task = 0;
    attr->watermark = 0;
    attr->wakeup_events = 0;
    switch (config) {
        case CYCLE:
            attr->config = PERF_COUNT_HW_CPU_CYCLES;
            goto perf_type_hardware;
        case STALL_CYCLE_BACK_END:
            attr->config = PERF_COUNT_HW_STALLED_CYCLES_BACKEND;
            goto perf_type_hardware;
        case INSTRUCTIONS:
            attr->config = PERF_COUNT_HW_INSTRUCTIONS;
            goto perf_type_hardware;

        case UPI_RECEIVE:
            attr->config = 0x03 | 0xF << 8 | MC_CH_PCI_PMON_CTL_EN;
            goto perf_type_uncore_upi;
        
        case UPI_TRANSMIT:
            attr->config = 0x02 | 0xF << 8 | MC_CH_PCI_PMON_CTL_EN;
            goto perf_type_uncore_upi;
        
        case IMC_READ:
            attr->config = 0x04 | 0x3 << 8 | MC_CH_PCI_PMON_CTL_EN;
            goto perf_type_uncore_imc;

        case IMC_WRITE:
            attr->config = 0x04 | 0xc << 8 | MC_CH_PCI_PMON_CTL_EN;
            goto perf_type_uncore_imc;

        case MEMORY_READ:
            attr->config =  (PERF_COUNT_HW_CACHE_LL) |
                            (PERF_COUNT_HW_CACHE_OP_READ << 8) |
                            (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16);
            goto perf_type_cache;
        case MEMORY_WRITE:
            attr->config =  (PERF_COUNT_HW_CACHE_LL) | 
                            (PERF_COUNT_HW_CACHE_OP_WRITE << 8) |
                            (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16);\
            goto perf_type_cache;
        case LOCAL_READ:
            attr->config =  (PERF_COUNT_HW_CACHE_NODE) | 
                            (PERF_COUNT_HW_CACHE_OP_READ << 8) |
                            (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16);
            goto perf_type_cache;
        case LOCAL_WRITE:
            attr->config =  (PERF_COUNT_HW_CACHE_NODE) |
                            (PERF_COUNT_HW_CACHE_OP_WRITE << 8) |
                            (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16);\
            goto perf_type_cache;

        
    }
    perf_type_hardware:
        attr->type = PERF_TYPE_HARDWARE;
        return;

    perf_type_cache:
        attr->type = PERF_TYPE_HW_CACHE;
        return;
    
    perf_type_uncore_upi:;
    {
        char filename_buf[64]; 
        sprintf(filename_buf, "/sys/bus/event_source/devices/uncore_upi_%d/type", uncore_id);
        attr->type = read_event_type(filename_buf);
        return;
    }
    perf_type_uncore_imc:;
    {
        char filename_buf[64];
        sprintf(filename_buf, "/sys/bus/event_source/devices/uncore_imc_%d/type", uncore_id);
        attr->type = read_event_type(filename_buf);
        return;
    }
}

void init_evesel(event_s *evesel, perf_range_t range, event_config config, int uncore_id) {
    int i = 0, dst = 0;
    switch (range) {
        case SOCKET:
            dst = performance.socket_num;
            for(; i < dst; ++i) {
                NULL_CHECK(evesel[i].read_fmt = ((read_format *) malloc (1024)));
                evesel[i].range = range;
                evesel[i].config = config;
                evesel[i].fd = -1;
                evesel[i].id = i;
                int core_num;
                for(core_num = 0; core_num < cpu_topology.core_per_node; ++core_num) {
                    int temp_fd = -1;
                    event_attr_init(&(evesel[i].attr), true, config, -1);
                    if (evesel[i].fd == -1) {
                        temp_fd = evesel[i].fd = perf_event_open(&evesel[i].attr, -1, i*cpu_topology.core_per_node+core_num, evesel[i].id, 0);
                    } else {
                        temp_fd = perf_event_open(&evesel[i].attr, -1, i*cpu_topology.core_per_node+core_num, evesel[i].fd, 0);
                    }
                    if (temp_fd < 0) {
                        malloc_printf("failed to open event\n");
                        return 1;
                    } 
                }
            }
        break;
        case BUS:
            evesel->config = config;
            evesel->id = uncore_id;
            evesel->range = range;
            event_attr_init(&evesel->attr, false, config, uncore_id);
            evesel->fd = perf_event_open(&evesel->attr, -1, 
                (performance.link0[uncore_id]+1)*cpu_topology.core_per_node - 1 /* the last cpu core of the socket*/, -1, 0);
            if (evesel->fd < 0) {
                malloc_printf("failed to open event\n");
                return 1;
            }
            break;
        case CPU: 
            evesel->config = config;
            evesel->id = 1;
            evesel->range = range;
            event_attr_init(&evesel->attr, false, config, uncore_id);
            evesel->fd = perf_event_open(&evesel->attr, 0, -1, -1, 0);
            if (evesel->fd < 0) {
                malloc_printf("failed to open event\n");
                return 1;
            }
        break;
        default:
            malloc_printf("no support for the simple range\n");
    }
}

void freeze() {
    int i;
    for(i = 0; i < performance.perf_num; ++i) {
        switch (performance.evesel[i].range)
        {
        case CPU:
        case BUS:
        case CORE:
            ioctl(performance.evesel[i].fd, PERF_EVENT_IOC_ENABLE, 0);
            break;        
        case SOCKET:
            ioctl(performance.evesel[i].fd, PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP);
        default:
            break;
        }
    }
}

void reset() {
    int i;
    for(i = 0; i < performance.perf_num; ++i) {
        switch (performance.evesel[i].range)
        {
        case CPU:
        case BUS:
        case CORE:
            ioctl(performance.evesel[i].fd, PERF_EVENT_IOC_RESET, 0);
            break;        
        case SOCKET:
            ioctl(performance.evesel[i].fd, PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP);
        default:
            break;
        }
    }
}

void unfreeze() {
    int i;
    for(i = 0; i < performance.perf_num; ++i) {
        switch (performance.evesel[i].range)
        {
        case CPU:
        case BUS:
        case CORE:
            ioctl(performance.evesel[i].fd, PERF_EVENT_IOC_DISABLE, 0);
            break;        
        case SOCKET:
            ioctl(performance.evesel[i].fd, PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP);
        default:
            break;
        }
    }
}

bool collect_performance(){
    freeze();
    memset(performance.memory_read, 0, sizeof(uint64_t) * performance.socket_num);
    memset(performance.memory_write, 0, sizeof(uint64_t) * performance.socket_num);
    int i;
    uint64_t temp_sum = 0;
    uint64_t bus_data;
    for(i = 0; i < performance.perf_num; ++i) {
        switch (performance.evesel[i].range)
        {
        case CPU:
            if (performance.evesel[i].config == CYCLE) {
                read(performance.evesel[i].fd, &performance.cpu_cycle, sizeof(performance.cpu_cycle));
            } else if (performance.evesel[i].config == INSTRUCTIONS) {
                read(performance.evesel[i].fd, &performance.instructions, sizeof(performance.instructions));
            }
            break; 
        case SOCKET:
            break;
        case BUS:
            read(performance.evesel[i].fd, &bus_data, sizeof(bus_data));
            switch (performance.evesel[i].config)
            {
            case UPI_RECEIVE:
                performance.bandwidth[performance.evesel[i].id] = (uint64_t) ( (double)bus_data / (64./(172./8.)) );
                break;  
            case UPI_TRANSMIT:
                performance.bandwidth[performance.evesel[2*i].id] = (uint64_t) ( (double)bus_data / (64./(172./8.)) );
                break;
            case IMC_READ:
                performance.memory_read[performance.evesel[i].id/performance.socket_num]
                    += bus_data * 64;
                break;
            case IMC_WRITE:
                performance.memory_write[performance.evesel[i].id / performance.socket_num] 
                    += bus_data * 64;
            default:
                break;
            }
        default:
            break;
        }
    }
    reset();
    unfreeze();
}

int get_imc_num() {
    DIR *uncore_dir = opendir("/sys/bus/event_source/devices/");
    if (!uncore_dir) {
        malloc_printf("failed to open uncore dir\n");
        return -1;
    }
    int i = 0;
    do {
        char str[64] ;
        sprintf(str, "/sys/bus/event_source/devices/uncore_imc_%d", i);
        DIR *d = readdir(str);
        if (!d) {
            return i - 1;
        }
        ++i;
    } while (1);
    return -1;
}

bool performance_boot() {
    int uncore_num = 0;
    performance.socket_num = cpu_topology.numa_nodes_num;
    switch (cpu_info.brand) {
        case INTEL:
            switch (cpu_info.model) {
                case SKX:
                    if (cpu_topology.numa_nodes_num == 2){
                        uncore_num = 2;
                        performance.bus_num = 2;
                        NULL_CHECK(performance.link0 = (int *) malloc (sizeof(int) * performance.bus_num));
                        NULL_CHECK(performance.link1 = (int *) malloc (sizeof(int) * performance.bus_num));
                        INIT_LINK(0, 0, 1);
                        INIT_LINK(1, 0, 1);
                    }
                    break;
            }  
            performance.imc_num = get_imc_num();
            NULL_CHECK(performance.bandwidth = (uint64_t *) malloc (sizeof(uint64_t) * performance.bus_num * 2));
            performance.perf_num = performance.bus_num * 2 /*bus: UPI links : receive and transmit*/ + performance.imc_num * 2 /*imc : read and write*/ + 2 /*stall_cycle + cycle*/;
            NULL_CHECK(performance.evesel = (event_s *) malloc (sizeof(event_s) * performance.perf_num));
            NULL_CHECK(performance.evesel_index = (int *) malloc (sizeof(int) * (2 + 2 + 2 + 1)));
            // NULL_CHECK(performance.remote_access = (int *) malloc (sizeof(uint64_t) * performance.socket_num * performance.socket_num));
            NULL_CHECK(performance.memory_read = (int *) malloc (sizeof(uint64_t) * performance.socket_num));
            NULL_CHECK(performance.memory_write = (int *) malloc (sizeof(uint64_t) * performance.socket_num));
            int i = 0, j = 0, z = 0;
            performance.evesel_index[j] = i;
            init_evesel(&performance.evesel[i], CPU, CYCLE, -1);  i += 1; ++j; performance.evesel_index[j] = i;
            init_evesel(&performance.evesel[i], CPU, INSTRUCTIONS, -1); i += 1; ++j; performance.evesel_index[j] = i;
            for(z = 0; z < performance.bus_num; ++z) {
                init_evesel(&performance.evesel[i], BUS, UPI_RECEIVE, z);
            }
            i += performance.bus_num; ++j; performance.evesel_index[j] = i;
            for(z = 0; z < performance.bus_num; ++z) {
                init_evesel(&performance.evesel[i], BUS, UPI_TRANSMIT, z);
            }
            i += performance.bus_num; ++j; performance.evesel_index[j] = i;
            for(z = 0; z < performance.imc_num; ++z) {
                init_evesel(&performance.evesel[i], BUS, IMC_READ, z);
            }
            i += performance.imc_num; ++j; performance.evesel_index[j] = i;
             for(z = 0; z < performance.imc_num; ++z) {
                init_evesel(&performance.evesel[i], BUS, IMC_WRITE, z);
            }
            i += performance.imc_num; ++j; performance.evesel_index[j] = i;
            break;
        case AMD:
            break;
        case ARM:
            break;
    }
    return false;
}

void *monitor_task() {
    while (performance.runing){
        collect_performance();
        sleep(1);
    }
}

bool monitor_boot(){
    if (cpu_model_boot()) {
        return (true);
    }
    if (performance_boot()) {
        return (true);
    }
    performance.runing = true;
    pthread_create(&monitor_thread_handle, NULL, monitor_task, NULL);
    return false;
}

bool monitor_destroy(){
    performance.runing = false;
    return false;
}
