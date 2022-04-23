/******************************************************************************/
#ifdef JEMALLOC_H_EXTERNS

bool debug_file_boot()

void write_to_malloc(void *ptr, uint64_t size);
void write_to_free(void *ptr);
void write_performance_info();

#endif /* JEMALLOC_H_EXTERNS */
/******************************************************************************/
