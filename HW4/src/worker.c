#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "worker.h"

void *worker_function(void *arg) {
    Buffer *buffer = (Buffer *)arg;

    while (1) {
        BufferElement item;

        // Pop item from buffer
        while (!buffer_pop(buffer, &item)) {
            if (buffer_is_done(buffer)) return NULL;
            // Buffer is empty, wait for data
            usleep(1000);
        }

        // Copy file content
        char read_buffer[1024];
        ssize_t bytes_read;
        while ((bytes_read = read(item.src_fd, read_buffer, sizeof(read_buffer))) > 0) write(item.dest_fd, read_buffer, bytes_read);

        // Close files
        close(item.src_fd);
        close(item.dest_fd);

        // Output completion status
        //printf("Copied %s\n", item.filename);
    }

    return NULL;
}
