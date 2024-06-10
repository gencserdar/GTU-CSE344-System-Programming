#ifndef MANAGER_H
#define MANAGER_H

#include "buffer.h"

typedef struct {
    Buffer *buffer;
    char *src_dir;
    char *dest_dir;
    pthread_barrier_t *barrier;
} ManagerArgs;


void *manager_function(void *args);

#endif