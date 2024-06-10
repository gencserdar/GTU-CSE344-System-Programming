#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <semaphore.h>
#include <pthread.h>
#include "semaphore.h"

typedef struct {
    char name[255]; 
    int reader_count;
    sem_t reader_mutex;
    sem_t writer_mutex;
} File;

typedef struct shared_memory_files {
    int capacity;
    int size;
    File files[64];
} Shared;

void reader_enter(File *sync_system) {
    sem_wait(&(sync_system->reader_mutex)); // Acquire reader mutex
    sync_system->reader_count++;
    if (sync_system->reader_count == 1) {
        // First reader, block writers
        sem_wait(&(sync_system->writer_mutex));
    }
    sem_post(&(sync_system->reader_mutex)); // Release reader mutex
}

void reader_exit(File *sync_system) {
    sem_wait(&(sync_system->reader_mutex)); // Acquire reader mutex
    sync_system->reader_count--;
    if (sync_system->reader_count == 0) {
        // Last reader, allow writers
        sem_post(&(sync_system->writer_mutex));
    }
    sem_post(&(sync_system->reader_mutex)); // Release reader mutex
}

void writer_enter(File *sync_system) {
    sem_wait(&(sync_system->writer_mutex)); // Acquire writer mutex
    // Ensure no readers while writing
    sem_wait(&(sync_system->reader_mutex));
}

void writer_exit(File *sync_system) {
    sem_post(&(sync_system->reader_mutex)); // Release reader mutex
    sem_post(&(sync_system->writer_mutex)); // Release writer mutex
}

File* init_file(char* filename, Shared* sdir) 
{
    // Shared memory file capacity reached, cant add more
    if (sdir->size == sdir->capacity) {
        return NULL; 
    }
    File* file = &sdir->files[sdir->size];

    strcpy(file->name, filename);

    // Initialize semaphores
    if (sem_init(&file->reader_mutex, 1, 1) == -1) {
        return NULL;
    }
    if (sem_init(&file->writer_mutex, 1, 1) == -1) {
        return NULL;
    }
    file->reader_count = 0;
    sdir->size += 1;
    return file;
}

// Initialize existing files
int initialize(Shared *sdir) {
    sdir->capacity = 64; 
    sdir->size = 0;

    DIR *dir;
    struct dirent *entry;
    // Open the client directory
    dir = opendir(".");
    if (dir == NULL) {
        perror("opendir");
        return -1;
    }

    // Iterate over each file in the directory
    while ((entry = readdir(dir)) != NULL) {
        // Skip "." and ".." entries
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            if (init_file(entry->d_name, sdir) == NULL) {
                fprintf(stderr, "Failed to initialize file: %s\n", entry->d_name);
                continue;
            }
        }
    }
    closedir(dir);
    return 0;
}
// If file is already in shared memory files list then return it
File *get_file(Shared *sdir, const char *file)
{
    for (int i = 0; i < sdir->size; ++i) if (strcmp(sdir->files[i].name, file) == 0) return &sdir->files[i];
    return NULL;
}