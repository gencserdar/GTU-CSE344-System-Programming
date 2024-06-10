#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "manager.h"
#include <sys/stat.h> 
#include <limits.h> 

// Function to recursively copy directories and files
void copy_directory(const char *src_dir, const char *dest_dir, Buffer *buffer) {
    DIR *dir = opendir(src_dir);
    if (dir == NULL) {
        perror("Error opening source directory");
        destroy_buffer(buffer);
        exit(EXIT_FAILURE);
    }

    // Create destination directory if it doesn't exist
    if (mkdir(dest_dir, 0755) == -1 && errno != EEXIST) {
        printf("Error creating destination directory %s", dest_dir);
        destroy_buffer(buffer);
        exit(EXIT_FAILURE);
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            // Construct source and destination paths
            char src_path[PATH_MAX];
            char dest_path[PATH_MAX];
            snprintf(src_path, sizeof(src_path), "%s/%s", src_dir, entry->d_name);
            snprintf(dest_path, sizeof(dest_path), "%s/%s", dest_dir, entry->d_name);

            // Check if entry is a directory
            if (entry->d_type == DT_DIR) copy_directory(src_path, dest_path, buffer); // Recursively copy subdirectory
            else {
                // Open source file
                int src_fd = open(src_path, O_RDONLY);
                if (src_fd == -1) {
                    perror("Error opening source file");
                    destroy_buffer(buffer);
                    exit(EXIT_FAILURE);
                }

                // Open or create destination file
                int dest_fd = open(dest_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (dest_fd == -1) {
                    perror("Error opening destination file");
                    close(src_fd);
                    destroy_buffer(buffer);
                    exit(EXIT_FAILURE);
                }

                // Add file descriptors and names to buffer
                BufferElement item;
                item.src_fd = src_fd;
                item.dest_fd = dest_fd;
                strncpy(item.filename, entry->d_name, sizeof(item.filename) - 1);
                item.filename[sizeof(item.filename) - 1] = '\0';

                while (!buffer_push(buffer, item)) usleep(1000); // Buffer is full, wait
            }
        }
    }
    closedir(dir);
}

// Manager function
void *manager_function(void *args) {
    ManagerArgs *manager_args = (ManagerArgs *)args;
    Buffer *buffer = manager_args->buffer;
    char *src_dir = manager_args->src_dir;
    char *dest_dir = manager_args->dest_dir;

    // Copy source directory recursively
    copy_directory(src_dir, dest_dir, buffer);

    // Signal completion
    buffer_set_done(buffer);

    return NULL;
}