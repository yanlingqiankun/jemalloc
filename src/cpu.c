//
// Created by shado on 2021/12/2.
//
#include "jemalloc/internal/jemalloc_internal.h"
#include <dirent.h>

#define howmany(x,y) (((x)+((y)-1))/(y))
#define bitsperlong (8 * sizeof(unsigned long))
#define longsperbits(n) howmany(n, bitsperlong)

#if defined(__x86_64__) || defined(__i386__)
#define NUMA_NUM_NODES  128
#else
#define NUMA_NUM_NODES  2048
#endif

static const char *mask_size_file = "/proc/self/status";
static const char *nodemask_prefix = "Mems_allowed:\t";
static int nodemask_sz = 0;

struct bitmask *numa_nodes_ptr = NULL;
static struct bitmask *numa_memnode_ptr = NULL;
nodemask_t numa_all_nodes;

static int maxconfigurednode = -1;

int numa_avail() {
    if (get_mempolicy(NULL, NULL, 0, 0, 0) < 0 && errno == ENOSYS)
        return -1;
    return 0;
}

struct bitmask *
numa_bitmask_alloc(unsigned int n)
{
	struct bitmask *bmp;

	if (n < 1) {
		errno = EINVAL;
		malloc_printf("request to allocate mask for invalid number\n");
		exit(1);
	}
	bmp = base_alloc(sizeof(*bmp));
	if (bmp == 0)
		goto oom;
	bmp->size = n;
	bmp->maskp = base_alloc(longsperbits(n)*sizeof(unsigned long));
    memset(bmp->maskp, 0, longsperbits(n)*sizeof(unsigned long));
	if (bmp->maskp == 0) {
		free(bmp);
		goto oom;
	}
	return bmp;

oom:
	malloc_printf("Out of memory allocating bitmask\n");
	exit(1);
}

static int s2nbits(const char *s)
{
	return strlen(s) * 32 / 9;
}

/* Is string 'pre' a prefix of string 's'? */
static int strprefix(const char *s, const char *pre)
{
	return strncmp(s, pre, strlen(pre)) == 0;
}

static void
_setbit(struct bitmask *bmp, unsigned int n, unsigned int v)
{
	if (n < bmp->size) {
		if (v)
			bmp->maskp[n/bitsperlong] |= 1UL << (n % bitsperlong);
		else
			bmp->maskp[n/bitsperlong] &= ~(1UL << (n % bitsperlong));
	}
}

static unsigned int
_getbit(const struct bitmask *bmp, unsigned int n)
{
	if (n < bmp->size)
		return (bmp->maskp[n/bitsperlong] >> (n % bitsperlong)) & 1;
	else
		return 0;
}

struct bitmask *
numa_bitmask_setbit(struct bitmask *bmp, unsigned int i)
{
	_setbit(bmp, i, 1);
	return bmp;
}

int
numa_bitmask_isbitset(const struct bitmask *bmp, unsigned int i)
{
	return _getbit(bmp, i);
}

/* (cache the result?) */
long long numa_node_size64(int node, long long *freep)
{
	size_t len = 0;
	char *line = NULL;
	long long size = -1;
	FILE *f;
	char fn[64];
	int ok = 0;
	int required = freep ? 2 : 1;

	if (freep)
		*freep = -1;
	sprintf(fn,"/sys/devices/system/node/node%d/meminfo", node);
	f = fopen(fn, "r");
	if (!f)
		return -1;
	while (getdelim(&line, &len, '\n', f) > 0) {
		char *end;
		char *s = strcasestr(line, "kB");
		if (!s)
			continue;
		--s;
		while (s > line && isspace(*s))
			--s;
		while (s > line && isdigit(*s))
			--s;
		if (strstr(line, "MemTotal")) {
			size = strtoull(s,&end,0) << 10;
			if (end == s)
				size = -1;
			else
				ok++;
		}
		if (freep && strstr(line, "MemFree")) {
			*freep = strtoull(s,&end,0) << 10;
			if (end == s)
				*freep = -1;
			else
				ok++;
		}
	}
	fclose(f);
	free(line);
	if (ok != required)
		malloc_prinf("Cannot parse sysfs meminfo (%d)\n", ok);
	return size;
}

static void
set_nodemask_size(void)
{
	FILE *fp;
	char *buf = NULL;
	size_t bufsize = 0;

	if ((fp = fopen(mask_size_file, "r")) == NULL)
		goto done;

	while (getline(&buf, &bufsize, fp) > 0) {
		if (strprefix(buf, nodemask_prefix)) {
			nodemask_sz = s2nbits(buf + strlen(nodemask_prefix));
			break;
		}
	}
	free(buf);
	fclose(fp);
done:
	nodemask_sz = 16;
}

/*
 * Find nodes (numa_nodes_ptr), nodes with memory (numa_memnode_ptr)
 * and the highest numbered existing node (maxconfigurednode).
 */
static void
set_configured_nodes(void)
{
	DIR *d;
	struct dirent *de;
	long long freep;

	numa_memnode_ptr = numa_allocate_nodemask();
	numa_nodes_ptr = numa_allocate_nodemask();

	d = opendir("/sys/devices/system/node");
	if (!d) {
		maxconfigurednode = 0;
	} else {
		while ((de = readdir(d)) != NULL) {
			int nd;
			if (strncmp(de->d_name, "node", 4))
				continue;
			nd = strtoul(de->d_name+4, NULL, 0);
			numa_bitmask_setbit(numa_nodes_ptr, nd);
			if (numa_node_size64(nd, &freep) > 0)
				numa_bitmask_setbit(numa_memnode_ptr, nd);
			if (maxconfigurednode < nd)
				maxconfigurednode = nd;
		}
		closedir(d);
	}
}

int
numa_num_configured_nodes(void)
{
	/*
	* NOTE: this function's behavior matches the documentation (ie: it
	* returns a count of nodes with memory) despite the poor function
	* naming.  We also cannot use the similarly poorly named
	* numa_all_nodes_ptr as it only tracks nodes with memory from which
	* the calling process can allocate.  Think sparse nodes, memory-less
	* nodes, cpusets...
	*/
	int memnodecount=0, i;

	for (i=0; i <= maxconfigurednode; i++) {
		if (numa_bitmask_isbitset(numa_memnode_ptr, i))
			memnodecount++;
	}
	return memnodecount;
}

static inline void nodemask_set_compat(nodemask_t *mask, int node)
{
	mask->n[node / (8*sizeof(unsigned long))] |=
		(1UL<<(node%(8*sizeof(unsigned long))));
}

bool cpu_topology_boot() {
    if (!numa_avail()) {
        return true;
    }
    int i;
    set_nodemask_size();
    set_configured_nodes();
    
    int max = numa_num_configured_nodes();
    for (i = 0; i < max; i++)
        nodemask_set_compat((nodemask_t *)&numa_all_nodes, i);

    cpu_topology.numa_nodes_num = maxconfigurednode;
    struct bitmask *bmp = &(cpu_topology.node_mask);    
    if(get_mempolicy(NULL, bmp->maskp, bmp->size+1, 0, MPOL_F_MEMS_ALLOWED) < 0) {
        malloc_printf("error to get_mempolicy");
    }
    return false;
}
