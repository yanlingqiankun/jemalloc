//
// Created by shado on 2021/12/2.
//
#include "jemalloc/internal/jemalloc_internal.h"
#include <dirent.h>

int numa_avail() {
	// return 0 if numa is available
    if (get_mempolicy(NULL, NULL, 0, 0, 0) < 0 && errno == ENOSYS)
        return -1;
    return 0;
}

// copy from libnuma
#define bitsperlong (8 * sizeof(unsigned long))
#define longsperbits(n) howmany(n, bitsperlong)
#define bitsperint (8 * sizeof(unsigned int))

#define BITS_PER_LONG (sizeof(long)*8)

#define round_up(x,y) (((x) + (y) - 1) & ~((y)-1))

#define CPU_BYTES(x) (round_up(x, BITS_PER_LONG)/8)
#define CPU_LONGS(x) (CPU_BYTES(x) / sizeof(long))

static const char *mask_size_file = "/proc/self/status";
static const char *nodemask_prefix = "Mems_allowed:\t";
static int nodemask_sz = 0;
static int numprocnode = -1;
struct bitmask *numa_all_nodes_ptr = NULL;
struct bitmask *numa_nodes_ptr = NULL;
static struct bitmask *numa_memnode_ptr = NULL;
nodemask_t numa_all_nodes;
static int maxconfigurednode = -1;

static struct bitmask **node_cpu_mask;

bool numa_initialized = false;
cpu_topology_t cpu_topology;


static unsigned int
_getbit(const struct bitmask *bmp, unsigned int n)
{
	if (n < bmp->size)
		return (bmp->maskp[n/bitsperlong] >> (n % bitsperlong)) & 1;
	else
		return 0;
}
/* Hamming Weight: number of set bits */
unsigned int numa_bitmask_weight(const struct bitmask *bmp)
{
	unsigned int i;
	unsigned int w = 0;
	for (i = 0; i < bmp->size; i++)
		if (_getbit(bmp, i))
			w++;
	return w;
}

static int
read_mask(char *s, struct bitmask *bmp)
{
	char *end = s;
	int tmplen = (bmp->size + bitsperint - 1) / bitsperint;
	unsigned int tmp[tmplen];
	unsigned int *start = tmp;
	unsigned int i, n = 0, m = 0;

	if (!s)
		return 0;	/* shouldn't happen */

	i = strtoul(s, &end, 16);

	/* Skip leading zeros */
	while (!i && *end++ == ',') {
		i = strtoul(end, &end, 16);
	}

	if (!i)
		/* End of string. No mask */
		return -1;

	start[n++] = i;
	/* Read sequence of ints */
	while (*end++ == ',') {
		i = strtoul(end, &end, 16);
		start[n++] = i;

		/* buffer overflow */
		if (n > tmplen)
			return -1;
	}

	/*
	 * Invert sequence of ints if necessary since the first int
	 * is the highest and we put it first because we read it first.
	 */
	while (n) {
		int w;
		unsigned long x = 0;
		/* read into long values in an endian-safe way */
		for (w = 0; n && w < bitsperlong; w += bitsperint)
			x |= ((unsigned long)start[n-- - 1] << w);

		bmp->maskp[m++] = x;
	}
	/*
	 * Return the number of bits set
	 */
	return numa_bitmask_weight(bmp);
}

struct bitmask *
numa_bitmask_alloc(unsigned int n)
{
	struct bitmask *bmp;

	if (n < 1) {
		errno = EINVAL;
		malloc_printf("request to allocate mask for invalid number and size is %d\n", n);
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

struct bitmask *
numa_allocate_nodemask(void)
{
	struct bitmask *bmp;
	int nnodes = nodemask_sz;

	bmp = numa_bitmask_alloc(nnodes);
	return bmp;
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

struct bitmask *
numa_bitmask_setbit(struct bitmask *bmp, unsigned int i)
{
	_setbit(bmp, i, 1);
	return bmp;
}

struct bitmask *
numa_bitmask_clearbit(struct bitmask *bmp, unsigned int i)
{
	_setbit(bmp, i, 0);
	return bmp;
}

char *
strcasestr(const char *s, const char *find)
{
	char c, sc;
	size_t len;
	if ((c = *find++) != 0) {
		c = (char)tolower((unsigned char)c);
		len = strlen(find);
		do {
			do {
				if ((sc = *s++) == 0)
					return (NULL);
			} while ((char)tolower((unsigned char)sc) != c);
		} while (strncasecmp(s, find, len) != 0);
		s--;
	}
	return ((char *)s);
}

int
numa_bitmask_isbitset(const struct bitmask *bmp, unsigned int i)
{
	return _getbit(bmp, i);
}

/* (cache the result?) */
long long numa_node_size64(int node, long long *freep)
{
	size_t len;
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
		malloc_printf("Cannot parse sysfs meminfo (%d)\n", ok);
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
			numa_all_nodes_ptr = numa_allocate_nodemask();
			char *mask = strchr(buf, '\t') + 1;
			numprocnode = read_mask(mask, numa_all_nodes_ptr);
			break;
		}
	}
	free(buf);
	fclose(fp);
	int i;
	if (numprocnode <= 0) {
		for (i = 0; i <= maxconfigurednode; i++)
			numa_bitmask_setbit(numa_all_nodes_ptr, i);
		numprocnode = maxconfigurednode + 1;
	}
done:
	if (nodemask_sz == 0) {/* fall back on error */
		int pol;
		unsigned long *mask = NULL;
		nodemask_sz = 16;
		do {
			nodemask_sz <<= 1;
			mask = realloc(mask, nodemask_sz / 8);
			if (!mask)
				return;
		} while (get_mempolicy(&pol, mask, nodemask_sz + 1, 0, 0) < 0 && errno == EINVAL &&
				nodemask_sz < 4096*8);
		free(mask);
	}
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

int numa_parse_bitmap(char *line, struct bitmask *mask)
{
	int i, ncpus;
	char *p = strchr(line, '\n');
	if (!p)
		return -1;
	ncpus = mask->size;

	for (i = 0; p > line;i++) {
		char *oldp, *endp;
		oldp = p;
		if (*p == ',')
			--p;
		while (p > line && *p != ',')
			--p;
		/* Eat two 32bit fields at a time to get longs */
		if (p > line && sizeof(unsigned long) == 8) {
			oldp--;
			memmove(p, p+1, oldp-p+1);
			while (p > line && *p != ',')
				--p;
		}
		if (*p == ',')
			p++;
		if (i >= CPU_LONGS(ncpus))
			return -1;
		mask->maskp[i] = strtoul(p, &endp, 16);
		if (endp != oldp)
			return -1;
		p--;
	}
	return 0;
}

bool init_node_mask() {
	int node;
	size_t len = 0;
	FILE *f; 
	char fn[64], *line;
	for (node = 0; node < numprocnode; ++node) {
		node_cpu_mask[node] = numa_bitmask_alloc(cpu_topology.core_per_node * cpu_topology.numa_nodes_num);
		sprintf(fn, "/sys/devices/system/node/node%d/cpumap", node);
		f = fopen(fn, "r");
		if (!f || getdelim(&line, &len, '\n', f) < 1) {
			malloc_printf("failed to open cpumap\n");
			return true;
		}
		if (line && numa_parse_bitmap(line, node_cpu_mask[node])) {
			malloc_printf("failed to parse bitmap of cpumap\n");
			return true;
		}
	}
	return false;
}

/*
 * copy a bitmask map body to another bitmask body
 * fill a larger destination with zeroes
 */
void
copy_bitmask_to_bitmask(struct bitmask *bmpfrom, struct bitmask *bmpto)
{
	int bytes;

	if (bmpfrom->size >= bmpto->size) {
		memcpy(bmpto->maskp, bmpfrom->maskp, CPU_BYTES(bmpto->size));
	} else if (bmpfrom->size < bmpto->size) {
		bytes = CPU_BYTES(bmpfrom->size);
		memcpy(bmpto->maskp, bmpfrom->maskp, bytes);
		memset(((char *)bmpto->maskp)+bytes, 0,
					CPU_BYTES(bmpto->size)-bytes);
	}
}

struct bitmask *get_cpu_mask(int node_id) {
	if (node_id >= maxconfigurednode && !node_cpu_mask) {
		return NULL;
	}
	struct bitmask *ret = numa_bitmask_alloc(cpu_topology.core_per_node * cpu_topology.numa_nodes_num);
	copy_bitmask_to_bitmask(node_cpu_mask[node_id], ret);
	return ret;
}

static __always_inline int ffs_long (unsigned long word)
{
    
	int num = 0;
	if ((word & 0xffffffff) == 0) {
    
		num += 32;
		word >>= 32;
	}
	if ((word & 0xffff) == 0) {
    
		num += 16;
		word >>= 16;
	}
	if ((word & 0xff) == 0) {
    
		num += 8;
		word >>= 8;
	}
	if ((word & 0xf) == 0) {
    
		num += 4;
		word >>= 4;
	}
	if ((word & 0x3) == 0) {
    
		num += 2;
		word >>= 2;
	}
	if ((word & 0x1) == 0)
		num += 1;
	return num;
}

int find_first_cpu_of_node(int node) {
	struct bitmask * node_ptr = node_cpu_mask[node];
	int ret = -1;
	int i = 0; 
	do{
		ret = ffs_long(node_ptr->maskp[i]);
		++i;
	} while(ret < 0 && i < node_ptr->size);
	return ret;
}

void
numa_bitmask_free(struct bitmask *bmp)
{
	if (bmp == 0)
		return;
	free(bmp->maskp);
	bmp->maskp = (unsigned long *)0xdeadcdef;  /* double free tripwire */
	free(bmp);
	return;
}

bool cpu_topology_boot() {
    if (numa_avail()) {
        return true;
    }
    int i;
    set_nodemask_size();
    set_configured_nodes();
    
    int max = numa_num_configured_nodes();
    for (i = 0; i < max; i++)
        nodemask_set_compat((nodemask_t *)&numa_all_nodes, i);

    cpu_topology.numa_nodes_num = numprocnode;
	cpu_topology.node_mask = *numa_all_nodes_ptr;
	cpu_topology.core_num = sysconf(_SC_NPROCESSORS_CONF);
	if (cpu_topology.core_num == 0) {
		return true;
	}
	cpu_topology.core_per_node = cpu_topology.core_num / numprocnode;
	node_cpu_mask = (struct bitmask **) malloc (sizeof(struct bitmask *)*numprocnode); 
	if (init_node_mask()) {
		return true;
	}
    return false;
}
