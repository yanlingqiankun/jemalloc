#ifdef JEMALLOC_H_TYPES
#ifdef __x86_64__
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

#elif (defined (__aarch64__))
typedef enum{
    null
}cpu_model_t;
#endif 

typedef enum {
    INTEL,
    AMD,
    ARM
} cpu_brand_t;

typedef struct {
    cpu_brand_t brand;
    cpu_model_t model;
    int family;
    int stepping;
} cpu_info_t;

#endif /* JEMALLOC_H_TYPES */
/******************************************************************************/
#ifdef JEMALLOC_H_EXTERNS

bool monitor_boot();

#endif /* JEMALLOC_H_EXTERNS */
/******************************************************************************/
#ifdef JEMALLOC_H_INLINES


#endif /* JEMALLOC_H_INLINES */