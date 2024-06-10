#ifndef BUFFER_H
#define BUFFER_H

#include <pthread.h>

typedef struct {
    int src_fd;
    int dest_fd;
    char filename[256];
} BufferElement;

typedef struct {
    BufferElement *items;
    int capacity;
    int in;
    int out;
    int count;
    int done;
    pthread_mutex_t mutex;
    pthread_cond_t empty;
    pthread_cond_t full;
} Buffer;

void init_buffer(Buffer *buffer, int size);
void destroy_buffer(Buffer *buffer);
int buffer_push(Buffer *buffer, BufferElement item);
int buffer_pop(Buffer *buffer, BufferElement *item);
void buffer_set_done(Buffer *buffer);
int buffer_is_done(Buffer *buffer);

#endif
