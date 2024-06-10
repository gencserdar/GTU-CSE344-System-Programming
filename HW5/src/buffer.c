#include "buffer.h"
#include <stdlib.h>
#include <stdio.h>

void init_buffer(Buffer *buffer, int size, pthread_barrier_t *barrier) {
    buffer->capacity = size;
    buffer->items = (BufferElement *)malloc(sizeof(BufferElement) * size);
    if (buffer->items == NULL) {
        perror("Error initializing buffer");
        destroy_buffer(buffer);
        exit(EXIT_FAILURE);
    }
    buffer->in = 0;
    buffer->out = 0;
    buffer->count = 0;
    buffer->done = 0;
    buffer->barrier = barrier;  // Set the barrier
    pthread_mutex_init(&buffer->mutex, NULL);
    pthread_cond_init(&buffer->empty, NULL);
    pthread_cond_init(&buffer->full, NULL);
}

void destroy_buffer(Buffer *buffer) {
    free(buffer->items);
    pthread_mutex_destroy(&buffer->mutex);
    pthread_cond_destroy(&buffer->empty);
    pthread_cond_destroy(&buffer->full);
}

int buffer_push(Buffer *buffer, BufferElement item) {
    pthread_mutex_lock(&buffer->mutex);
    while (buffer->count >= buffer->capacity)
        pthread_cond_wait(&buffer->full, &buffer->mutex);
    buffer->items[buffer->in] = item;
    buffer->in = (buffer->in + 1) % buffer->capacity;
    buffer->count++;
    pthread_cond_signal(&buffer->empty);
    pthread_mutex_unlock(&buffer->mutex);
    return 1;
}

int buffer_pop(Buffer *buffer, BufferElement *item) {
    pthread_mutex_lock(&buffer->mutex);
    while (buffer->count <= 0 && !buffer->done)
        pthread_cond_wait(&buffer->empty, &buffer->mutex);
    if (buffer->count <= 0 && buffer->done) {
        pthread_mutex_unlock(&buffer->mutex);
        return 0;
    }
    *item = buffer->items[buffer->out];
    buffer->out = (buffer->out + 1) % buffer->capacity;
    buffer->count--;
    pthread_cond_signal(&buffer->full);
    pthread_mutex_unlock(&buffer->mutex);
    return 1;
}

void buffer_set_done(Buffer *buffer) {
    pthread_mutex_lock(&buffer->mutex);
    buffer->done = 1;
    pthread_cond_broadcast(&buffer->empty);
    pthread_mutex_unlock(&buffer->mutex);
}

int buffer_is_done(Buffer *buffer) {
    return buffer->done;
}
