#include "worker.h"
#include "buffer.h"
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

void *worker_function(void *arg) {
    Buffer *buffer = (Buffer *)arg;

    while (1) {
        BufferElement item;

        // Pop item from buffer
        while (!buffer_pop(buffer, &item)) {
            if (buffer_is_done(buffer)) {
                pthread_barrier_wait(buffer->barrier); // Wait at the barrier before exiting
                return NULL;
            }
            // Buffer is empty, wait for data
            usleep(1000);
        }

        // Copy file content
        char read_buffer[1024];
        ssize_t bytes_read;
        while ((bytes_read = read(item.src_fd, read_buffer, sizeof(read_buffer))) > 0) {
            write(item.dest_fd, read_buffer, bytes_read);
        }

        // Close files
        close(item.src_fd);
        close(item.dest_fd);

        // Output completion status
        // printf("Copied %s\n", item.filename);
    }

    // Wait at the barrier to synchronize with manager and other workers
    pthread_barrier_wait(buffer->barrier);

    return NULL;
}
