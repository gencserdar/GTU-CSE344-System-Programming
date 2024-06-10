#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>
#include <signal.h>
#include "buffer.h"
#include "manager.h"
#include "worker.h"

Buffer buffer;
pthread_barrier_t barrier;

// Signal handler function
void signal_handler(int sig) {
    printf("\nReceived signal: %d. Cleaning up and exiting.\n", sig);
    destroy_buffer(&buffer);
    pthread_barrier_destroy(&barrier);
    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {
    // Register signal handlers
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTSTP, &sa, NULL);

    // Validate command-line arguments
    if (argc != 5) {
        printf("Usage: %s <buffer_size> <worker_amount> <source_directory> <destination_directory>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Parse arguments
    int buffer_size = atoi(argv[1]);
    int worker_amount = atoi(argv[2]);
    char *source_directory = argv[3];
    char *destination_directory = argv[4];

    // Initialize the barrier for worker_amount + 1 (manager thread)
    pthread_barrier_init(&barrier, NULL, worker_amount + 1);

    // Initialize buffer
    init_buffer(&buffer, buffer_size, &barrier);

    // Create manager and worker threads
    pthread_t manager_thread;
    pthread_t worker_threads[worker_amount];

    // Record start time
    struct timeval start_time, end_time;
    gettimeofday(&start_time, NULL);

    // Start manager thread
    ManagerArgs manager_args = {&buffer, source_directory, destination_directory, &barrier};
    pthread_create(&manager_thread, NULL, manager_function, &manager_args);

    // Start worker threads
    for (int i = 0; i < worker_amount; i++) {
        pthread_create(&worker_threads[i], NULL, worker_function, &buffer);
    }

    // Wait for manager to finish
    pthread_join(manager_thread, NULL);

    // Wait for all workers to finish
    for (int i = 0; i < worker_amount; i++) {
        pthread_join(worker_threads[i], NULL);
    }

    // Record end time
    gettimeofday(&end_time, NULL);

    // Cleanup buffer and barrier
    destroy_buffer(&buffer);
    pthread_barrier_destroy(&barrier);

    // Calculate total time
    long seconds = end_time.tv_sec - start_time.tv_sec;
    long microseconds = end_time.tv_usec - start_time.tv_usec;
    if (microseconds < 0) {
        seconds -= 1;
        microseconds += 1000000;
    }
    long milliseconds = microseconds / 1000;
    long minutes = seconds / 60;
    seconds = seconds % 60;

    // Output statistics
    printf("Copying completed.\n");
    printf("\n---------------STATISTICS--------------------\n");
    printf("Consumers: %d - Buffer Size: %d\n", worker_amount, buffer_size);
    printf("TOTAL TIME: %02ld:%02ld.%03ld (min:sec.mili)\n", minutes, seconds, milliseconds);

    return 0;
}
