/******************************************************************************/
#ifdef JEMALLOC_H_EXTERNS

bool debug_file_boot()

bool write_to_malloc(void *ptr, uint64_t size);
bool write_to_free(void *ptr);

#endif /* JEMALLOC_H_EXTERNS */
/******************************************************************************/