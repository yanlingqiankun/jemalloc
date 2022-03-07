#include "jemalloc/internal/jemalloc_internal.h"

#define cpuid(func,eax,ebx,ecx,edx)\
    __asm__ __volatile__ ("cpuid":\
    "=a" (eax), "=b" (ebx),"=c" (ecx), "=d" (edx):\
    "a" (func));

cpu_info_t cpu_info;

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

void *monitor_task() {
    return ;
}

bool monitor_boot(){
    if (cpu_model_boot()) {
        return (true);
    }
    return false;
}
