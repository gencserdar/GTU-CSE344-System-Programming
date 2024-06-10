#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <ctype.h>
#include "structs.c"

#define SERVER_DIR "server_directory"
int server_pid;
void kill_child() {
    char server_fifo_path[256];
    snprintf(server_fifo_path, sizeof(server_fifo_path), SERVER_DIR"/server_%d.fifo", server_pid);
    char client_fifo[256];
    snprintf(client_fifo, sizeof(client_fifo), "client_%d.fifo", getpid());

    printf("Kill signal received.\nTerminating..\n");
    struct stat stat_buffer;
    if (stat(server_fifo_path, &stat_buffer) != -1) {
        int client_fd = open(client_fifo, O_WRONLY); // Open client fifo to send command
        if (client_fd == -1) {
            fprintf(stderr, "Error opening client FIFO: %s\n", strerror(errno));
            unlink(client_fifo); // Remove client FIFO before exit
            exit(EXIT_FAILURE);
        }
        char *command = "quit";
        // Send command with help of client fifo
        if (write(client_fd, command, strlen(command)) == -1) {
            fprintf(stderr, "Error writing command: %s\n", strerror(errno));
            close(client_fd); // Close client FIFO before exit
            unlink(client_fifo); // Remove client FIFO before exit
            exit(EXIT_FAILURE);
        }
        close(client_fd);
    }
    unlink(client_fifo);

}

int main(int argc, char *argv[]) {
    struct sigaction sa;
    // Set up handler for SIGCHLD
    sa.sa_handler = kill_child;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("Error setting up SIGINT handler");
        exit(EXIT_FAILURE);
    }
    sa.sa_handler = kill_child;
    if (sigaction(SIGTSTP, &sa, NULL) == -1) {
        perror("Error setting up SIGINT handler");
        exit(EXIT_FAILURE);
    }
    if (argc != 3 || (strcmp(argv[1], "Connect") != 0 && strcmp(argv[1], "tryConnect") != 0)) {
        fprintf(stderr, "Usage: %s <Connect/tryConnect> <ServerPID>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    const char *connection_type = argv[1];
    server_pid = atoi(argv[2]);
    char client_fifo[256];
    char server_fifo_path[256];
    struct client new_client;

    // Create client FIFO
    snprintf(client_fifo, sizeof(client_fifo), "client_%d.fifo", getpid());
    if (mkfifo(client_fifo, 0666) == -1) {
        fprintf(stderr, "Error creating client FIFO: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    snprintf(server_fifo_path, sizeof(server_fifo_path), SERVER_DIR"/server_%d.fifo", server_pid);
    // Open server FIFO for writing
    int server_fd = open(server_fifo_path, O_WRONLY);
    if (server_fd == -1) {
        fprintf(stderr, "Error opening server FIFO: %s\n", strerror(errno));
        unlink(client_fifo);
        exit(EXIT_FAILURE);
    }

    new_client.pid= getpid();
    if(strcmp(connection_type, "Connect")==0) new_client.tryConnect=0;
    else if (strcmp(connection_type, "tryConnect")==0) new_client.tryConnect=1;

    if (write(server_fd, &new_client, sizeof(struct client)) == -1) {
        fprintf(stderr, "Error writing to server FIFO: %s\n", strerror(errno));
        close(server_fd); // Close server FIFO before exit
        unlink(client_fifo);
        exit(EXIT_FAILURE);
    }
    printf("Connected to server (ServerPID: %d) as client_%d\n", server_pid, new_client.pid);

    // Close server FIFO
    close(server_fd);
    if(new_client.tryConnect==1){
        int full;
        int client_fd = open(client_fifo, O_RDONLY);
        if (client_fd == -1) {
            fprintf(stderr, "Error opening client FIFO: %s\n", strerror(errno));
            unlink(client_fifo); // Remove client FIFO before exit
            exit(EXIT_FAILURE);
        }
        if(read(client_fd,&full,sizeof(int))== -1){
            fprintf(stderr, "Error reading from client FIFO: %s\n", strerror(errno));
            close(client_fd); // Close client FIFO before exit
            unlink(client_fifo); // Remove client FIFO before exit
            exit(EXIT_FAILURE);
        }
        close(client_fd); // Close client FIFO after reading
        if(full==1){
            printf("Server is currently full. Not waiting in queue. Terminating...\n");
            unlink(client_fifo); // Remove client FIFO before exit
            exit(EXIT_FAILURE);
        }
    }

    int client_fd;
    while(1){
        // Check if server FIFO exists
        struct stat stat_buffer;
        // If client didn't termminate when server is killed, terminate it (this happens when client is in queue)
        if (stat(server_fifo_path, &stat_buffer) == -1) {
            // Server FIFO does not exist, assume server is killed
            printf("Server is dead. Exiting.\n");
            printf("bye ;)");
            unlink(client_fifo);
            break;
        }
        
        // Enter command
        char command[256];
        if (fgets(command, sizeof(command), stdin) == NULL) {
            exit(EXIT_FAILURE);
        }

        // Double check if client didn't termminate when server is killed, terminate it
        if (stat(server_fifo_path, &stat_buffer) == -1) { 
            // Server FIFO does not exist, assume server is killed
            printf("Server is dead. Exiting.\n");
            printf("bye ;)\n");
            unlink(client_fifo);
            break;
        }
        // Remove trailing newline character, if present
        size_t len = strlen(command);
        if (len > 0 && command[len - 1] == '\n') {
            command[len - 1] = '\0';
        }

        client_fd = open(client_fifo, O_WRONLY); // Open client fifo to send command
        if (client_fd == -1) {
            fprintf(stderr, "Error opening client FIFO: %s\n", strerror(errno));
            unlink(client_fifo); // Remove client FIFO before exit
            exit(EXIT_FAILURE);
        }
        size_t command_len = strlen(command) + 1; // Include size of null terminator too
        // Send command with help of client fifo
        if (write(client_fd, command, command_len) == -1) {
            fprintf(stderr, "Error writing command: %s\n", strerror(errno));
            close(client_fd); // Close client FIFO before exit
            unlink(client_fifo); // Remove client FIFO before exit
            exit(EXIT_FAILURE);
        }
        close(client_fd); // Close client FIFO after writing
        if (strcmp(command,"quit")==0){
            printf("bye ;)");
            break;
        }

        client_fd = open(client_fifo, O_RDONLY); // Open fifo to read server response
        if (client_fd == -1) {
            fprintf(stderr, "Error opening client FIFO: %s\n", strerror(errno));
            unlink(client_fifo); // Remove client FIFO before exit
            exit(EXIT_FAILURE);
        }

        char response[4096];
        int bytes_read;
        while ((bytes_read = read(client_fd, response, sizeof(response))) > 0) {
            // Ensure response is null-terminated
            response[bytes_read] = '\0';
            printf("%s", response);

            // Reset response for the next iteration
            memset(response, 0, sizeof(response));
        }
        if (bytes_read < 0) {
            // Handle read error
            fprintf(stderr, "Error reading response from client FIFO: %s\n", strerror(errno));
        }
        close(client_fd); // Close client FIFO after reading
    }

    // Remove client FIFO
    unlink(client_fifo);

    return 0;
}