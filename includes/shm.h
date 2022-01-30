#pragma once
#include <stddef.h>

static void random_name(char *buf); 
static int create_shm_file(void);
int allocate_shm(size_t size);
