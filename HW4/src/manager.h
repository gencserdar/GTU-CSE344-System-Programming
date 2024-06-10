#ifndef MANAGER_H
#define MANAGER_H

#include "buffer.h"

typedef struct {
    Buffer *buffer;
    char *src_dir;
    char *dest_dir;
} ManagerArgs;

void *manager_function(void *args);

#endif
