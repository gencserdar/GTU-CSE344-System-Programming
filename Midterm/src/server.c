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
#include "sync.c"

#define SHARED "/shared"
char* server_dir;
char server_fifo_path[256];
int log_fd, num_clients=0;
struct queue clients;
struct queue wait_que;
Shared* sd;
void client_termination_handler(int signum) {
    // Decrement num_clients when a client process terminates
    num_clients--;
}
void write_log(char* message);

void exitParent() {
    // Terminate all child processes
    while (clients.size>0) {
        struct client *client = dequeue(&clients);
        kill(client->pid, SIGINT);
        char child_fifo[256];
        snprintf(child_fifo, sizeof(child_fifo), "child_%d.fifo", client->pid);
        unlink(child_fifo);
        free(client); // Free memory allocated for client
    }
    // Cleanup remaining resources
    queue_destructor(&wait_que);
    queue_destructor(&clients);
    close(log_fd);
    unlink(server_fifo_path);
    exit(EXIT_SUCCESS);
}
void kill_server() {
    exitParent(); // Clean up and terminate parent process
}
void exitFailure(){
    queue_destructor(&clients);
    queue_destructor(&wait_que);
    close(log_fd);
    exit(EXIT_FAILURE);
}
void exitSuccess(){
    queue_destructor(&clients);
    queue_destructor(&wait_que);
    close(log_fd);
    exit(EXIT_SUCCESS);
}
void setup_log_file() {
    char log_file[FILENAME_MAX];
    // Generate log file name
    snprintf(log_file, FILENAME_MAX, "logfile_%d.log", getpid());
    // Open log file for writing, create if it doesn't exist, truncate if it does
    log_fd = open(log_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (log_fd == -1) {
        printf("Error opening log file: %s\n", strerror(errno));
        exitFailure();
    }
    printf("Log file created: %s\n", log_file);
}

void write_log(char* message) {
    ssize_t bytes_written = write(log_fd, message, strlen(message));
    if (bytes_written == -1) {
        fprintf(stderr, "Error writing to log file: %s\n", strerror(errno));
        exitFailure(); 
    }
}

void setup_server_directory() {
    struct stat st;
    // Check if the server directory exists
    if (stat(server_dir, &st) == -1) {
        // Directory doesn't exist, create it
        if (mkdir(server_dir, 0777) == -1) {
            printf("Error creating server directory: %s\n", strerror(errno));
            exitFailure();
        }
        printf("Server directory created: %s\n", server_dir);
    } else if (!S_ISDIR(st.st_mode)) {
        printf("%s is not a directory.\n", server_dir);
        exitFailure();
    }
}


void setup_server_fifo() {
    // Create server FIFO if it doesn't exist
    char server_fifo[254];
    snprintf(server_fifo, sizeof(server_fifo), "server_%d.fifo", getpid());
    snprintf(server_fifo_path, sizeof(server_fifo_path), "%s/%s", server_dir, server_fifo);
    if (mkfifo(server_fifo_path, S_IRUSR | S_IWUSR | S_IWGRP) == -1 && errno != EEXIST){
        printf("Error creating server FIFO: %s\n", strerror(errno));
        exitFailure();
    }
    char log_msg[512];
    sprintf(log_msg, "Server FIFO created: %s\n", server_fifo);
    write_log(log_msg);
}

void request(struct client new_client, char client_fifo[]){
    while(1){
        char command[256];
        // Open client FIFO for reading
        int client_fd = open(client_fifo, O_RDONLY);
        if (client_fd == -1) {
            exitFailure();
        }
        // Read command from client FIFO
        if (read(client_fd, command, sizeof(command)) == -1) {
            printf("Error reading command from client FIFO: %s\n", strerror(errno));
            close(client_fd);
            int client_fd = open(client_fifo, O_WRONLY);
            if (client_fd == -1) {
                fprintf(stderr, "Error opening client FIFO: %s\n", strerror(errno));
                exitFailure();
            }
            char* response= "Error reading command from client FIFO.\n";
            if (write(client_fd, response, strlen(response) + 1) == -1) {
                fprintf(stderr, "Error writing response: %s\n", strerror(errno));
                close(client_fd);
                unlink(client_fifo);
                exitFailure();
            }
            close(client_fd);
            continue;
        }
        close(client_fd);

        if (strcmp(command, "help") == 0) {
            printf("\nhelp command from client_%d\n", new_client.pid);
            client_fd = open(client_fifo, O_WRONLY);
            if (client_fd == -1) {
                fprintf(stderr, "Error opening client FIFO: %s\n", strerror(errno));
                exitFailure();
            }
            char* response = "Available commands are:\n\nhelp, list, readF, writeT, upload, download, archServer, killServer, quit\n";
            if (write(client_fd, response, strlen(response)+1) == -1) {
                fprintf(stderr, "Error writing response: %s\n", strerror(errno));
                close(client_fd);
                unlink(client_fifo);
                exitFailure();
            }
            close(client_fd);
        } else if (strcmp(command,"help readF")==0){
            printf("\nhelp readF command from client_%d\n", new_client.pid);
            client_fd = open(client_fifo, O_WRONLY);
            if (client_fd == -1) {
                fprintf(stderr, "Error opening client FIFO: %s\n", strerror(errno));
                exitFailure();
            }
            char* response = "\n\nreadF <file> <line #>\nDisplay the #th line of the <file>, returns with an error if <file> does not exists.\n\n";
            if (write(client_fd, response, strlen(response)+1) == -1) {
                fprintf(stderr, "Error writing response: %s\n", strerror(errno));
                close(client_fd);
                unlink(client_fifo);
                exitFailure();
            }
            close(client_fd);
        } else if (strcmp(command,"help writeT")==0){
            printf("\nhelp writeT command from client_%d\n", new_client.pid);
            client_fd = open(client_fifo, O_WRONLY);
            if (client_fd == -1) {
                fprintf(stderr, "Error opening client FIFO: %s\n", strerror(errno));
                exitFailure();
            }
            char* response = "\n\nwriteT <file> <line #> <string>\nWrite the  content of “string” to the  #th  line the <file>, if the line # is not given writes to the end of file. If the file does not exists in Servers directory creates and edits thefile at the same time.\n\n";
            if (write(client_fd, response, strlen(response)+1) == -1) {
                fprintf(stderr, "Error writing response: %s\n", strerror(errno));
                close(client_fd);
                unlink(client_fifo);
                exitFailure();
            }
            close(client_fd);
        } else if (strcmp(command,"help upload")==0){
            printf("\nhelp upload command from client_%d\n", new_client.pid);
            client_fd = open(client_fifo, O_WRONLY);
            if (client_fd == -1) {
                fprintf(stderr, "Error opening client FIFO: %s\n", strerror(errno));
                exitFailure();
            }
            char* response = "\n\nupload <file>\nUploads the file from the current working directory of client to the Servers directory.\n\n";
            if (write(client_fd, response, strlen(response)+1) == -1) {
                fprintf(stderr, "Error writing response: %s\n", strerror(errno));
                close(client_fd);
                unlink(client_fifo);
                exitFailure();
            }
            close(client_fd);
        } else if (strcmp(command,"help download")==0){
            printf("\nhelp download command from client_%d\n", new_client.pid);
            client_fd = open(client_fifo, O_WRONLY);
            if (client_fd == -1) {
                fprintf(stderr, "Error opening client FIFO: %s\n", strerror(errno));
                exitFailure();
            }
            char* response = "\n\ndownload <file>\nReceive <file> from Servers directory to client side\n\n";
            if (write(client_fd, response, strlen(response)+1) == -1) {
                fprintf(stderr, "Error writing response: %s\n", strerror(errno));
                close(client_fd);
                unlink(client_fifo);
                exitFailure();
            }
            close(client_fd);
        } else if (strcmp(command,"help archServer")==0){
            printf("\nhelp archServer command from client_%d\n", new_client.pid);
            client_fd = open(client_fifo, O_WRONLY);
            if (client_fd == -1) {
                fprintf(stderr, "Error opening client FIFO: %s\n", strerror(errno));
                exitFailure();
            }
            char* response = "\n\narchServer <fileName>.tar\nUsing fork, exec and tar utilities create a child process that will collect all the files currently available on the the  Server side and store them in the <filename>.tar archive\n\n";
            if (write(client_fd, response, strlen(response)+1) == -1) {
                fprintf(stderr, "Error writing response: %s\n", strerror(errno));
                close(client_fd);
                unlink(client_fifo);
                exitFailure();
            }
            close(client_fd);
        } else if (strcmp(command,"help killServer")==0){
            printf("\nhelp killServer command from client_%d\n", new_client.pid);
            client_fd = open(client_fifo, O_WRONLY);
            if (client_fd == -1) {
                fprintf(stderr, "Error opening client FIFO: %s\n", strerror(errno));
                exitFailure();
            }
            char* response = "\n\nkillServer\nSends a kill request to the Server\n\n";
            if (write(client_fd, response, strlen(response)+1) == -1) {
                fprintf(stderr, "Error writing response: %s\n", strerror(errno));
                close(client_fd);
                unlink(client_fifo);
                exitFailure();
            }
            close(client_fd);
        } else if (strcmp(command, "list") == 0) {
            // List files in server_directory
            printf("\nlist command from client_%d\n", new_client.pid);
            client_fd = open(client_fifo, O_WRONLY);
            if (client_fd == -1) {
                fprintf(stderr, "Error opening client FIFO: %s\n", strerror(errno));
                exitFailure();
            }

            DIR *dir;
            struct dirent *entry;

            // Open the server directory
            if ((dir = opendir(server_dir)) == NULL) {
                fprintf(stderr, "Error opening server directory: %s\n", strerror(errno));
                int client_fd = open(client_fifo, O_WRONLY);
                if (client_fd == -1) {
                    fprintf(stderr, "Error opening client FIFO: %s\n", strerror(errno));
                    exitFailure();
                }
                char* response= "Error opening server directory.\n";
                if (write(client_fd, response, strlen(response) + 1) == -1) {
                    fprintf(stderr, "Error writing response: %s\n", strerror(errno));
                    close(client_fd);
                    unlink(client_fifo);
                    exitFailure();
                }
                close(client_fd);
                continue;
            }
            char response[4096] = "";
            while ((entry = readdir(dir)) != NULL) {
                // Exclude "." and ".." entries
                if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                    // Concatenate each filename to the response string
                    strcat(response, entry->d_name);
                    strcat(response, "\n");
                }
            }
            closedir(dir);

            // Write the response string to the client
            if (write(client_fd, response, strlen(response) + 1) == -1) {
                fprintf(stderr, "Error writing file list to client: %s\n", strerror(errno));
                close(client_fd);
                unlink(client_fifo);
                exitFailure();
            }
            close(client_fd);
        } else if (strcmp(command, "quit") == 0) {
            break;
        } else if (strcmp(command, "killServer") == 0) { 
            printf("\nkillServer command from client_%d\nTerminating...\n", new_client.pid);
            client_fd = open(client_fifo, O_WRONLY);
            if (client_fd == -1) {
                fprintf(stderr, "Error opening client FIFO: %s\n", strerror(errno));
                exitFailure();
            }

            char* response = "\nKilling the server...\n";
            if (write(client_fd, response, strlen(response)+1) == -1) {
                fprintf(stderr, "Error writing response: %s\n", strerror(errno));
                close(client_fd);
                unlink(client_fifo);
                exitFailure();
            }
            close(client_fd);
            unlink(client_fifo);
            char log_msg[256];
            sprintf(log_msg, "Kill signal from child with PID %d\nTerminating...\nbye ;)\n", new_client.pid);
            write_log(log_msg);
            kill(getppid(),SIGINT);
            unlink(server_fifo_path);
        } else{
            char *token;
            char *rest = command;
            char command_name[256];

            token = strtok_r(rest, " ", &rest);
            if (token == NULL) {
                token="invalid";
            }
            strcpy(command_name, token);

            if (strcmp(command_name, "readF") == 0) {
                // Send a request to the server to read file
                printf("\nreadF command from client_%d\n", new_client.pid);
                int valid = 1, line_number = -1;
                char file_name[256];
                char *file_token = strtok_r(rest, " ", &rest);
                if (file_token == NULL) {
                    // no file name provided
                    printf("Invalid command format for readF: %s\n", command);
                    valid = 0;
                } else {
                    strcpy(file_name, file_token);
                    // Parse line number if provided
                    char *line_token = strtok_r(NULL, " ", &rest);
                    if (line_token != NULL) line_number = atoi(line_token);
                }
                if (line_number==0) line_number=-1;
                File* sfile;
                if ((sfile = get_file(sd, file_name)) == NULL && (sfile = init_file(file_name, sd)) == NULL)  exitFailure(); 
                reader_enter(sfile);

                int file_fd = open(file_name, O_RDONLY);
                if (file_fd == -1) {
                    // Unable to open the file
                    perror("Error opening file");
                    int client_fd = open(client_fifo, O_WRONLY);
                    if (client_fd == -1) {
                        fprintf(stderr, "Error opening client FIFO: %s\n", strerror(errno));
                        exitFailure();
                    }
                    char* response= "Error opening file to read\n";
                    if (write(client_fd, response, strlen(response) + 1) == -1) {
                        fprintf(stderr, "Error writing response: %s\n", strerror(errno));
                        close(client_fd);
                        unlink(client_fifo);
                        exitFailure();
                    }
                    close(client_fd);
                    reader_exit(sfile);
                    continue;
                }

                int client_fd = open(client_fifo, O_WRONLY);
                if (client_fd == -1) {
                    fprintf(stderr, "Error opening client FIFO: %s\n", strerror(errno));
                    close(file_fd);
                    exitFailure();
                }
                if (valid == 0) {
                    char *response = "Invalid command format for readF\n";
                    if (write(client_fd, response, strlen(response) + 1) == -1) {
                        fprintf(stderr, "Error writing response: %s\n", strerror(errno));
                        close(file_fd);
                        close(client_fd);
                        unlink(client_fifo);
                        exitFailure();
                    }
                    close(client_fd);
                    reader_exit(sfile);
                // If line_number is not provided (-1), read and send the entire file
                } else if (line_number == -1) {
                    // Read and send the entire file
                    char buffer[4096];
                    ssize_t bytes_read;
                    while ((bytes_read = read(file_fd, buffer, sizeof(buffer))) > 0) {
                        if (write(client_fd, buffer, bytes_read) == -1) {
                            // Handle error: Unable to write to client FIFO
                            fprintf(stderr, "Error writing to client FIFO: %s\n", strerror(errno));
                            close(file_fd);
                            close(client_fd);
                            unlink(client_fifo);
                            exitFailure();
                        }
                    }
                    if (bytes_read == -1) {
                        // Unable to read from file
                        perror("Error reading from file");
                        close(file_fd);
                        int client_fd = open(client_fifo, O_WRONLY);
                        if (client_fd == -1) {
                            fprintf(stderr, "Error opening client FIFO: %s\n", strerror(errno));
                            exitFailure();
                        }
                        char* response= "Error reading from file.\n";
                        if (write(client_fd, response, strlen(response) + 1) == -1) {
                            fprintf(stderr, "Error writing response: %s\n", strerror(errno));
                            close(client_fd);
                            unlink(client_fifo);
                            exitFailure();
                        }
                        close(client_fd);
                    }
                    close(file_fd);
                    close(client_fd);
                    reader_exit(sfile);
                } else {
                    // Read and send the specified line number
                    int current_line = 1;
                    off_t offset = 0;
                    lseek(file_fd, offset, SEEK_SET);
                    ssize_t char_read;
                    char ch;
                    char line[4096];
                    int i =0;
                    while ((char_read = read(file_fd, &ch, 1)) > 0) {
                        if(ch=='\n'){
                            if(current_line==line_number){
                                line[i]=ch;
                                i++;
                                if (write(client_fd, line, i) == -1) {
                                    // Unable to write to client FIFO
                                    fprintf(stderr, "Error writing to client FIFO: %s\n", strerror(errno));
                                    close(file_fd);
                                    close(client_fd);
                                    unlink(client_fifo);
                                    exitFailure();
                                }
                                reader_exit(sfile);
                                break;
                            }else{
                                memset(line, '\0', sizeof(line));
                                i=0;
                            }
                            current_line++;
                        }else{
                            line[i]=ch;
                            i++;
                        }
                    }
                    //Line number exceeds the number of lines in the file
                    if(current_line<line_number || line_number<1){
                        fprintf(stderr, "Error: Line number %d exceeds the number of lines in the file.\n", line_number);
                        char *response = "Line number exceeds the number of lines in the file.\n";
                        if (write(client_fd, response, strlen(response) + 1) == -1) {
                            fprintf(stderr, "Error writing response: %s\n", strerror(errno));
                            close(file_fd);
                            close(client_fd);
                            unlink(client_fifo);
                            exitFailure();
                        }
                    }
                    close(file_fd);
                    close(client_fd);
                    reader_exit(sfile);
                }
            }else if (strcmp(command_name, "writeT") == 0) { 
                printf("\nwriteT command from client_%d\n", new_client.pid);
                int line_number=-1, placed = 0;
                char file_name[256], str[4096];
                char *file_token = strtok_r(rest, " ", &rest);
                if (file_token == NULL) {
                    // no file name provided
                    printf("Invalid command format for writeT: %s\n", command);
                    int client_fd = open(client_fifo, O_WRONLY);
                    if (client_fd == -1) {
                        fprintf(stderr, "Error opening client FIFO: %s\n", strerror(errno));
                        exitFailure();
                    }
                    char* response= "Invalid file name.\n";
                    if (write(client_fd, response, strlen(response) + 1) == -1) {
                        fprintf(stderr, "Error writing response: %s\n", strerror(errno));
                        close(client_fd);
                        exitFailure();
                    }
                    close(client_fd);
                    continue;
                } else {
                    strcpy(file_name, file_token);
                    // Parse line number if provided
                    char *line_token = strtok_r(NULL, " ", &rest);
                    if (line_token != NULL){
                        // Check if the token contains only digits
                        int is_digit = 1;
                        for(int i = 0; i < strlen(line_token); i++) {
                            if (!isdigit(line_token[i])) {
                                is_digit = 0;
                                break;
                            }
                        }
                        
                        if (is_digit) {
                            // If the token consists only of digits, set line_number
                            line_number = atoi(line_token);
                            if(line_number == 0) 
                                line_number = -1;
                        } else {
                            // If the token contains non-digits, treat it as part of the string
                            strcpy(str, line_token);
                            strcat(str, " "); // Add space to separate from subsequent arguments
                            char *next_token = strtok_r(NULL, "", &rest);
                            if (next_token != NULL) {
                                strcat(str, next_token);
                            }
                        }
                    }
                }

                // Set string_to_write based on the processed arguments
                char *string_to_write;
                if (line_number >= 0) {
                    // If line number is provided, concatenate the remaining tokens to form the string
                    string_to_write = strtok_r(NULL, "", &rest);
                } else {
                    // If no line number is provided, concatenate all tokens to form the string
                    if (str[0] != '\0') {
                        string_to_write = str;
                    } else {
                        // If str is empty, concatenate all remaining tokens
                        string_to_write = rest;
                    }
                }

                printf("file_name: %s\nline_token: %d\nstring: %s\n",file_name,line_number,string_to_write);
                File* sfile;
                if ((sfile = get_file(sd, file_name)) == NULL && (sfile = init_file(file_name, sd)) == NULL)  exitFailure(); 

                if (line_number == -1) {
                    writer_enter(sfile);
                    // Open the file in append mode
                    int file_fd = open(file_name, O_WRONLY | O_APPEND | O_CREAT, 0666);
                    if (file_fd == -1) {
                        perror("Error opening file");
                        int client_fd = open(client_fifo, O_WRONLY);
                        if (client_fd == -1) {
                            fprintf(stderr, "Error opening client FIFO: %s\n", strerror(errno));
                            exitFailure();
                        }
                        char* response= "Error opening file to write.\n";
                        if (write(client_fd, response, strlen(response) + 1) == -1) {
                            fprintf(stderr, "Error writing response: %s\n", strerror(errno));
                            close(client_fd);
                            exitFailure();
                        }
                        close(client_fd);
                        writer_exit(sfile);
                        continue;
                    }

                    // Append the string to the end of the file
                    if (write(file_fd, string_to_write, strlen(string_to_write)) == -1) {
                        perror("Error writing string to file");
                        close(file_fd);
                        int client_fd = open(client_fifo, O_WRONLY);
                        if (client_fd == -1) {
                            fprintf(stderr, "Error opening client FIFO: %s\n", strerror(errno));
                            exitFailure();
                        }
                        char* response= "Error writing string into the file.\n";
                        if (write(client_fd, response, strlen(response) + 1) == -1) {
                            fprintf(stderr, "Error writing response: %s\n", strerror(errno));
                            close(client_fd);
                            exitFailure();
                        }
                        close(client_fd);
                        writer_exit(sfile);
                        continue;
                    }
                    if (write(file_fd, "\n", 1) == -1) {
                        perror("Error writing string to file");
                        close(file_fd);
                        int client_fd = open(client_fifo, O_WRONLY);
                        if (client_fd == -1) {
                            fprintf(stderr, "Error opening client FIFO: %s\n", strerror(errno));
                            exitFailure();
                        }
                        char* response= "Error writing string into the file.\n";
                        if (write(client_fd, response, strlen(response) + 1) == -1) {
                            fprintf(stderr, "Error writing response: %s\n", strerror(errno));
                            close(client_fd);
                            unlink(client_fifo);
                            exitFailure();
                        }
                        close(client_fd);
                        writer_exit(sfile);
                        continue;
                    }

                    // Close the file
                    close(file_fd);
                    writer_exit(sfile);
                    client_fd = open(client_fifo, O_WRONLY | O_APPEND);
                    if (client_fd == -1) {
                        fprintf(stderr, "Error opening client FIFO: %s\n", strerror(errno));
                        exitFailure();
                    }
                    char* response= "Succesfully written into the file.\n";
                    if (write(client_fd, response, strlen(response) + 1) == -1) {
                        fprintf(stderr, "Error writing response: %s\n", strerror(errno));
                        close(client_fd);
                        unlink(client_fifo);
                        exitFailure();
                    }
                    close(client_fd);                    
                }else{
                    reader_enter(sfile);
                    FILE *file = fopen(file_name, "r");
                    if (file == NULL) {
                        perror("Error opening file");
                        int client_fd = open(client_fifo, O_WRONLY);
                        if (client_fd == -1) {
                            fprintf(stderr, "Error opening client FIFO: %s\n", strerror(errno));
                            exitFailure();
                        }
                        char* response= "Error opening file to write.\n";
                        if (write(client_fd, response, strlen(response) + 1) == -1) {
                            fprintf(stderr, "Error writing response: %s\n", strerror(errno));
                            close(client_fd);
                            unlink(client_fifo);
                            exitFailure();
                        }
                        close(client_fd);
                        reader_exit(sfile);
                        continue;
                    }

                    // Create a temporary file to write modified content
                    FILE *temp_file = tmpfile();
                    if (temp_file == NULL) {
                        perror("Error creating temporary file");
                        fclose(file);
                        int client_fd = open(client_fifo, O_WRONLY);
                        if (client_fd == -1) {
                            fprintf(stderr, "Error opening client FIFO: %s\n", strerror(errno));
                            exitFailure();
                        }
                        char* response= "Error opening file to write.\n";
                        if (write(client_fd, response, strlen(response) + 1) == -1) {
                            fprintf(stderr, "Error writing response: %s\n", strerror(errno));
                            close(client_fd);
                            unlink(client_fifo);
                            exitFailure();
                        }
                        close(client_fd);
                        continue;
                    }

                    char buffer[4096];
                    int current_line = 0;
                    // Read lines from the original file and write to the temporary file
                    while (fgets(buffer, sizeof(buffer), file) != NULL) {
                        current_line++;
                        if (line_number == current_line) {
                            // Replace content if we're at the specified line
                            fputs(string_to_write, temp_file);
                            fputs("\n", temp_file); // Append a newline
                            placed=1;
                        } else {
                            // Copy the original line
                            fputs(buffer, temp_file);
                        }
                    }

                    // Close original file
                    fclose(file);
                    reader_exit(sfile);
                    if(placed == 0){
                        fclose(temp_file);
                        int client_fd = open(client_fifo, O_WRONLY);
                        if (client_fd == -1) {
                            fprintf(stderr, "Error opening client FIFO: %s\n", strerror(errno));
                            exitFailure();
                        }
                        char* response= "Line number exceeds the number of lines in the file.\n";
                        if (write(client_fd, response, strlen(response) + 1) == -1) {
                            fprintf(stderr, "Error writing response: %s\n", strerror(errno));
                            close(client_fd);
                            unlink(client_fifo);
                            exitFailure();
                        }
                        close(client_fd);
                    }else{
                        writer_enter(sfile);
                        rewind(temp_file);

                        // Reopen original file in write mode (to overwrite)
                        file = fopen(file_name, "w+");
                        if (file == NULL) {
                            perror("Error opening file for writing");
                            fclose(temp_file);
                            int client_fd = open(client_fifo, O_WRONLY);
                            if (client_fd == -1) {
                                fprintf(stderr, "Error opening client FIFO: %s\n", strerror(errno));
                                exitFailure();
                            }
                            char* response= "Error opening file to write.\n";
                            if (write(client_fd, response, strlen(response) + 1) == -1) {
                                fprintf(stderr, "Error writing response: %s\n", strerror(errno));
                                close(client_fd);
                                unlink(client_fifo);
                                exitFailure();
                            }
                            close(client_fd);
                            writer_exit(sfile);
                            continue;
                        }

                        // Copy content from temporary file to the original file
                        while (fgets(buffer, sizeof(buffer), temp_file) != NULL) {
                            fputs(buffer, file);
                        }

                        // Close both files
                        fclose(file);
                        writer_exit(sfile);
                        fclose(temp_file);

                        //Send response to client
                        int client_fd = open(client_fifo, O_WRONLY);
                        if (client_fd == -1) {
                            fprintf(stderr, "Error opening client FIFO: %s\n", strerror(errno));
                            exitFailure();
                        }
                        char* response= "Succesfully written into the file.\n";
                        if (write(client_fd, response, strlen(response) + 1) == -1) {
                            fprintf(stderr, "Error writing response: %s\n", strerror(errno));
                            close(client_fd);
                            unlink(client_fifo);
                            exitFailure();
                        }
                        close(client_fd);
                    }
                }
            } else if (strcmp(command_name, "upload") == 0) {
                // Parse command parameters
                printf("\nupload command from client_%d\n", new_client.pid);
                char file_name[256];
                char *file_token = strtok_r(rest, " ", &rest);
                if (file_token == NULL) {
                    client_fd = open(client_fifo, O_WRONLY);
                    if (client_fd == -1) {
                        fprintf(stderr, "Error opening client FIFO: %s\n", strerror(errno));
                        exitFailure();
                    }
                    char response[] = "Error opening file for upload\n";
                    if (write(client_fd, response, strlen(response) + 1) == -1) {
                        fprintf(stderr, "Error writing response: %s\n", strerror(errno));
                        close(client_fd);
                        unlink(client_fifo);
                        exitFailure();
                    }
                    continue;
                }
                strcpy(file_name, file_token);

                File* sfile;
                if ((sfile = get_file(sd, file_name)) == NULL && (sfile = init_file(file_name, sd)) == NULL)  exitFailure(); 
                reader_enter(sfile);
                // Open the specified file in the client's current working directory for reading
                int file_to_upload = open(file_name, O_RDONLY);
                if (file_to_upload == -1) {
                    fprintf(stderr, "Error opening file for upload: %s\n", strerror(errno));
                    // Send error response to client
                    client_fd = open(client_fifo, O_WRONLY);
                    if (client_fd == -1) {
                        fprintf(stderr, "Error opening client FIFO: %s\n", strerror(errno));
                        exitFailure();
                    }
                    char* response = "Error opening file for upload\n";
                    if (write(client_fd, response, strlen(response) + 1) == -1) {
                        fprintf(stderr, "Error writing response: %s\n", strerror(errno));
                        close(client_fd);
                        unlink(client_fifo);
                        exitFailure();
                    }
                    close(client_fd);
                    reader_exit(sfile);
                    continue;
                }

                // Check if a file with the same name already exists in the server's directory
                char server_file_path[260];
                snprintf(server_file_path, sizeof(server_file_path), "%s/%s", server_dir, file_name);
                if (access(server_file_path, F_OK) != -1) {
                    // File already exists in the server's directory
                    fprintf(stderr, "File '%s' already exists in the server's directory\n", file_name);
                    // Send error response to client
                    client_fd = open(client_fifo, O_WRONLY);
                    if (client_fd == -1) {
                        fprintf(stderr, "Error opening client FIFO: %s\n", strerror(errno));
                        close(file_to_upload);
                        exitFailure();
                    }
                    char response[] = "File already exists in the server's directory\n";
                    if (write(client_fd, response, strlen(response) + 1) == -1) {
                        fprintf(stderr, "Error writing response: %s\n", strerror(errno));
                        close(client_fd);
                        close(file_to_upload);
                        unlink(client_fifo);
                        exitFailure();
                    }
                    close(client_fd);
                    close(file_to_upload);
                    reader_exit(sfile);
                    continue;
                }

                // Open a new file in the server's directory with the same name for writing
                int server_file = open(server_file_path, O_WRONLY | O_CREAT | O_EXCL, 0666);
                if (server_file == -1) {
                    fprintf(stderr, "Error creating file in the server's directory: %s\n", strerror(errno));
                    close(file_to_upload);
                    // Send error response to client
                    client_fd = open(client_fifo, O_WRONLY);
                    if (client_fd == -1) {
                        fprintf(stderr, "Error opening client FIFO: %s\n", strerror(errno));
                        exitFailure();
                    }
                    char response[] = "Error creating file in the server's directory\n";
                    if (write(client_fd, response, strlen(response) + 1) == -1) {
                        fprintf(stderr, "Error writing response: %s\n", strerror(errno));
                        close(client_fd);
                        unlink(client_fifo);
                        exitFailure();
                    }
                    close(client_fd);
                    reader_exit(sfile);
                    continue;
                }

                // Read data from the client's file and write it to the server's file
                char buffer[4096];
                int err=0;
                ssize_t bytes_read;
                size_t total_bytes_transferred = 0;
                while ((bytes_read = read(file_to_upload, buffer, sizeof(buffer))) > 0) {
                    ssize_t bytes_written = write(server_file, buffer, bytes_read);
                    if (bytes_written != bytes_read) {
                        fprintf(stderr, "Error writing data to the server's file: %s\n", strerror(errno));
                        close(file_to_upload);
                        close(server_file);
                        // Send error response to client
                        client_fd = open(client_fifo, O_WRONLY);
                        if (client_fd == -1) {
                            fprintf(stderr, "Error opening client FIFO: %s\n", strerror(errno));
                            exitFailure();
                        }
                        char response[] = "Error writing data to the server's file\n";
                        if (write(client_fd, response, strlen(response) + 1) == -1) {
                            fprintf(stderr, "Error writing response: %s\n", strerror(errno));
                            close(client_fd);
                            unlink(client_fifo);
                            exitFailure();
                        }
                        close(client_fd);
                        err=1;
                        reader_exit(sfile);
                        break;
                    }
                    total_bytes_transferred += bytes_read;
                }
                if(err==1) continue;
                // Close both files
                close(file_to_upload);
                reader_exit(sfile);
                close(server_file);

                // Send success response to client
                client_fd = open(client_fifo, O_WRONLY);
                if (client_fd == -1) {
                    fprintf(stderr, "Error opening client FIFO: %s\n", strerror(errno));
                    exitFailure();
                }
                char response[128];
                snprintf(response, sizeof(response), "File uploaded successfully. Total bytes transferred: %zu\n", total_bytes_transferred);
                if (write(client_fd, response, strlen(response) + 1) == -1) {
                    fprintf(stderr, "Error writing response: %s\n", strerror(errno));
                    close(client_fd);
                    unlink(client_fifo);
                    exitFailure();
                }
                close(client_fd);
            }else if (strcmp(command_name, "download") == 0) {
                // Parse command parameters
                printf("\ndownload command from client_%d\n", new_client.pid);
                char file_name[256];
                char *file_token = strtok_r(rest, " ", &rest);
                if (file_token == NULL) {
                    // If no file name provided, send an error response to the client
                    client_fd = open(client_fifo, O_WRONLY);
                    if (client_fd == -1) {
                        fprintf(stderr, "Error opening client FIFO: %s\n", strerror(errno));
                        exitFailure();
                    }
                    char response[] = "No file name provided for download\n";
                    if (write(client_fd, response, strlen(response) + 1) == -1) {
                        fprintf(stderr, "Error writing response: %s\n", strerror(errno));
                        close(client_fd);
                        unlink(client_fifo);
                        exitFailure();
                    }
                    close(client_fd);
                    continue;
                }
                strcpy(file_name, file_token);

                // Check if the file exists in the server's directory
                char server_file_path[260];
                snprintf(server_file_path, sizeof(server_file_path), "%s/%s", server_dir, file_name);
                if (access(server_file_path, F_OK) == -1) {
                    // If the file does not exist, send an error response to the client
                    fprintf(stderr, "File '%s' does not exist in the server's directory\n", file_name);
                    client_fd = open(client_fifo, O_WRONLY);
                    if (client_fd == -1) {
                        fprintf(stderr, "Error opening client FIFO: %s\n", strerror(errno));
                        exitFailure();
                    }
                    char response[] = "File does not exist in the server's directory\n";
                    if (write(client_fd, response, strlen(response) + 1) == -1) {
                        fprintf(stderr, "Error writing response: %s\n", strerror(errno));
                        close(client_fd);
                        unlink(client_fifo);
                        exitFailure();
                    }
                    close(client_fd);
                    continue;
                }

                // Open the file in the server's directory for reading
                int server_file = open(server_file_path, O_RDONLY);
                if (server_file == -1) {
                    // If there's an error opening the file, send an error response to the client
                    fprintf(stderr, "Error opening file for download: %s\n", strerror(errno));
                    client_fd = open(client_fifo, O_WRONLY);
                    if (client_fd == -1) {
                        fprintf(stderr, "Error opening client FIFO: %s\n", strerror(errno));
                        exitFailure();
                    }
                    char response[] = "Error opening file for download\n";
                    if (write(client_fd, response, strlen(response) + 1) == -1) {
                        fprintf(stderr, "Error writing response: %s\n", strerror(errno));
                        close(client_fd);
                        unlink(client_fifo);
                        exitFailure();
                    }
                    close(client_fd);
                    continue;
                }

                // Read data from the server's file and write it to the client's file
                char buffer[4096];
                ssize_t bytes_read;
                size_t total_bytes_transferred = 0;
                int err =0;
                if (access(file_name, F_OK) != -1) {
                    // File already exists
                    fprintf(stderr, "File '%s' already exists in client's directory.\n", file_name);
                    client_fd = open(client_fifo, O_WRONLY);
                    if (client_fd == -1) {
                        fprintf(stderr, "Error opening client FIFO: %s\n", strerror(errno));
                        exitFailure();
                    }
                    char response[] = "File already exists in the client's directory\n";
                    if (write(client_fd, response, strlen(response) + 1) == -1) {
                        fprintf(stderr, "Error writing response: %s\n", strerror(errno));
                        close(client_fd);
                        unlink(client_fifo);
                        exitFailure();
                    }
                    close(client_fd);
                    continue;
                }
                File* sfile;
                if ((sfile = get_file(sd, file_name)) == NULL && (sfile = init_file(file_name, sd)) == NULL)  exitFailure(); 
                writer_enter(sfile);
                int new_file_fd = open(file_name, O_CREAT | O_WRONLY, 0666);
                if (new_file_fd == -1) {
                    fprintf(stderr, "Failed to download file: %s\n", strerror(errno));
                    client_fd = open(client_fifo, O_WRONLY);
                    if (client_fd == -1) {
                        fprintf(stderr, "Error opening client FIFO: %s\n", strerror(errno));
                        exitFailure();
                    }
                    char response[] = "Failed to download file.\n";
                    if (write(client_fd, response, strlen(response) + 1) == -1) {
                        fprintf(stderr, "Error writing response: %s\n", strerror(errno));
                        close(client_fd);
                        unlink(client_fifo);
                        exitFailure();
                    }
                    close(client_fd);
                    writer_exit(sfile);
                    continue;
                }
                while ((bytes_read = read(server_file, buffer, sizeof(buffer))) > 0) {
                    ssize_t bytes_written = write(new_file_fd, buffer, bytes_read);
                    if (bytes_written != bytes_read) {
                        fprintf(stderr, "Error writing data to the client's file: %s\n", strerror(errno));
                        close(server_file);
                        close(new_file_fd);
                        // Send error response to client
                        client_fd = open(client_fifo, O_WRONLY);
                        if (client_fd == -1) {
                            fprintf(stderr, "Error opening client FIFO: %s\n", strerror(errno));
                            exitFailure();
                        }
                        char response[] = "Error occured while downloading the file.\n";
                        if (write(client_fd, response, strlen(response) + 1) == -1) {
                            fprintf(stderr, "Error writing response: %s\n", strerror(errno));
                            close(client_fd);
                            unlink(client_fifo);
                            exitFailure();
                        }
                        close(client_fd);
                        err=1;
                        writer_exit(sfile);
                        break;
                    }
                    total_bytes_transferred += bytes_read;
                }
                if (err==1) continue;

                // Close the server's file
                close(server_file);
                close(new_file_fd);
                writer_exit(sfile);

                // Send success response to client
                client_fd = open(client_fifo, O_WRONLY);
                if (client_fd == -1) {
                    fprintf(stderr, "Error opening client FIFO: %s\n", strerror(errno));
                    exitFailure();
                }
                char response[128];
                snprintf(response, sizeof(response), "File downloaded successfully. Total bytes transferred: %zu\n", total_bytes_transferred);
                if (write(client_fd, response, strlen(response) + 1) == -1) {
                    fprintf(stderr, "Error writing response: %s\n", strerror(errno));
                    close(client_fd);
                    unlink(client_fifo);
                    exitFailure();
                }
                close(client_fd);
            }else if (strcmp(command_name, "archServer") == 0) {
                // Create a tar archive containing all files in the server's directory
                printf("\narchServer command from client_%d\n", new_client.pid);
                char archiveName[256];
                char *file_token = strtok_r(rest, " ", &rest);
                if (file_token == NULL) {
                    // If no file name provided, send an error response to the client
                    client_fd = open(client_fifo, O_WRONLY);
                    if (client_fd == -1) {
                        fprintf(stderr, "Error opening client FIFO: %s\n", strerror(errno));
                        exitFailure();
                    }
                    char response[] = "No file name provided for archServer\n";
                    if (write(client_fd, response, strlen(response) + 1) == -1) {
                        fprintf(stderr, "Error writing response: %s\n", strerror(errno));
                        close(client_fd);
                        unlink(client_fifo);
                        exitFailure();
                    }
                    close(client_fd);
                    continue;
                }
                strcpy(archiveName, file_token);
                size_t len = strlen(archiveName);
                if (len < 4 || strcmp(archiveName + len - 4, ".tar") != 0) {
                    client_fd = open(client_fifo, O_WRONLY);
                    if (client_fd == -1) {
                        fprintf(stderr, "Error opening client FIFO: %s\n", strerror(errno));
                        exitFailure();
                    }
                    char response[] = "Invalid filename. Must end with \".tar\"\n";
                    if (write(client_fd, response, strlen(response) + 1) == -1) {
                        fprintf(stderr, "Error writing response: %s\n", strerror(errno));
                        close(client_fd);
                        unlink(client_fifo);
                        exitFailure();
                    }
                    close(client_fd);
                    continue;
                }

                pid_t pid = fork();

                if (pid == -1) {
                    // Fork failed
                    fprintf(stderr, "Error: Fork failed\n");
                    exit(EXIT_FAILURE);
                } else if (pid == 0) {
                    // Child process
                    // Change directory to the server directory
                    if (chdir(server_dir) == -1) {
                        // Error changing directory
                        perror("Error: Could not change directory to server directory");
                        exit(EXIT_FAILURE);
                    }
                    // Execute tar command to create archive
                    if (execlp("tar", "tar", "-cf", archiveName, ".", NULL) == -1) {
                        perror("execlp");
                        exit(EXIT_FAILURE);
                    }

                    exit(EXIT_SUCCESS);
                } else {
                    // Parent process
                    int status;
                    waitpid(pid, &status, 0);

                    if (WIFEXITED(status) && WEXITSTATUS(status) == EXIT_SUCCESS) {
                        printf("Archive created successfully: %s\n", archiveName);
                        client_fd = open(client_fifo, O_WRONLY);
                        if (client_fd == -1) {
                            fprintf(stderr, "Error opening client FIFO: %s\n", strerror(errno));
                            exitFailure();
                        }
                        char response[] = "Succesfully archived server directory, created .tar file is in server directory.\n";
                        if (write(client_fd, response, strlen(response) + 1) == -1) {
                            fprintf(stderr, "Error writing response: %s\n", strerror(errno));
                            close(client_fd);
                            unlink(client_fifo);
                            exitFailure();
                        }
                        close(client_fd);
                    } else {
                        fprintf(stderr, "Error: Failed to create archive\n");
                        client_fd = open(client_fifo, O_WRONLY);
                        if (client_fd == -1) {
                            fprintf(stderr, "Error opening client FIFO: %s\n", strerror(errno));
                            exitFailure();
                        }
                        char response[] = "Failed to archive server directory.\n";
                        if (write(client_fd, response, strlen(response) + 1) == -1) {
                            fprintf(stderr, "Error writing response: %s\n", strerror(errno));
                            close(client_fd);
                            unlink(client_fifo);
                            exitFailure();
                        }
                        close(client_fd);
                        continue;
                    }
                }
            } else {
                printf("Invalid command: %s\n", command);
                client_fd = open(client_fifo, O_WRONLY);
                if (client_fd == -1) {
                    fprintf(stderr, "Error opening client FIFO: %s\n", strerror(errno));
                    exitFailure();
                }
                char* response = "\ninvalid command\n";
                if (write(client_fd, response, strlen(response)+1) == -1) {
                    fprintf(stderr, "Error writing response: %s\n", strerror(errno));
                    close(client_fd);
                    exitFailure();
                }
                close(client_fd);
            }
        }
    }
}

void child_process(struct client new_client){
    char client_fifo[256];
    snprintf(client_fifo, FILENAME_MAX, "client_%d.fifo", new_client.pid);
    pid_t pid;
    pid = fork();
    if (pid == -1) {
        printf("Error while forking.\n");
        exitFailure();
    } else if (pid == 0) { //child
        char log_msg[256];
        sprintf(log_msg, "Client PID %d connected as 'client_%d'\n", new_client.pid, clients.size + 1);
        write(log_fd, log_msg, strlen(log_msg));
        printf("Client PID %d connected as 'client_%d'\n", new_client.pid, clients.size + 1);
        request(new_client, client_fifo);
        num_clients--;
        printf("Client with PID: %d disconnected.\n", new_client.pid);
        char log_msg2[256];
        sprintf(log_msg2, "Client with PID: %d disconnected.\n", new_client.pid);
        write_log(log_msg2);
        exitSuccess();// Exit child process
    } else { //parent
        enqueue(&clients, &new_client);
    }
}

void accept_connections(int max_clients) {
    int server_fd, client_fd, dummy_fd;
    char client_fifo[256];
    struct client new_client;

    int pid = getpid();
    char log_msg[256];
    sprintf(log_msg, "Server Started PID %d\n", pid);
    write_log(log_msg);

    printf("Server Started PID %d\n", pid);
    printf("Opening server FIFO for reading.\n");

    // Open server FIFO for reading
    server_fd = open(server_fifo_path, O_RDONLY | O_NONBLOCK);
    if (server_fd == -1) {
        fprintf(stderr, "Error opening server FIFO: %s\n", strerror(errno));
        exitFailure();
    }
    
    if ((dummy_fd = open(server_fifo_path, O_WRONLY)) == -1){
        close(server_fd);
        exitFailure();
    }

    printf("Server FIFO opened for reading.\nWaiting for clients...\n");

    // Accept connections from clients
    while (1) {
        // If there is empty space but queue is not empty then take the client in order
        if (num_clients < max_clients && wait_que.size > 0) {
            struct client *dequeued_client = dequeue(&wait_que);
            if (dequeued_client != NULL ) {
                num_clients++;
                child_process(*dequeued_client);
                free(dequeued_client); // Free the memory allocated by dequeue
                continue; // Continue to the next iteration to avoid reading new connection requests
            }
        }
        // Read client from server FIFO
        if (read(server_fd, &new_client, sizeof(struct client)) != -1) {
            //printf("\nclient fifo: %s\n", client_fifo);
            //printf("\nis tryConnect?: %d\n", new_client.tryConnect);
            if (client_fd != -1) {
                if (num_clients < max_clients && wait_que.size == 0){
                    num_clients++;
                    if(new_client.tryConnect==1){
                        snprintf(client_fifo, FILENAME_MAX, "client_%d.fifo", new_client.pid);
                        client_fd = open(client_fifo, O_WRONLY);
                        if (client_fd == -1) {
                            fprintf(stderr, "Error opening client FIFO: %s\n", strerror(errno));
                            exitFailure();
                        }
                        close(client_fd);
                    }
                    child_process(new_client);
                }else{ //Clients are full
                    printf("Connection request PID %d failed. Queue is FULL.\n", new_client.pid);
                    char log_msg[256];
                    sprintf(log_msg, "Connection request PID %d failed. Queue is FULL.\n", new_client.pid);
                    write_log(log_msg);
                    if(new_client.tryConnect==0){
                        enqueue(&wait_que, &new_client);
                        printf("Client with PID %d waits...\n", new_client.pid);
                        char log_msg[256];
                        sprintf(log_msg, "Client with PID %d waits...\n", new_client.pid);
                        write_log(log_msg);
                    }else{
                        int full = 1;
                        snprintf(client_fifo, FILENAME_MAX, "client_%d.fifo", new_client.pid);
                        client_fd = open(client_fifo, O_WRONLY);
                        if (client_fd == -1) {
                            fprintf(stderr, "Error opening client FIFO: %s\n", strerror(errno));
                            exitFailure();
                        }
                        if(write(client_fd,&full,sizeof(int))== -1){
                            fprintf(stderr, "Error writing into client FIFO: %s\n", strerror(errno));
                            close(client_fd);
                            unlink(client_fifo);
                            exitFailure();
                        }
                        close(client_fd);
                    }
                }
            }
        }
    }

    printf("\nClosing resources...\n");
    close(log_fd);
    close(server_fd);
    unlink(server_fifo_path);
    close(dummy_fd);
    queue_destructor(&wait_que);
    queue_destructor(&clients);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server_directory> <max_clients>\n", argv[0]);
        exitFailure();
    }
    struct sigaction sa;
    // Set up handler for SIGCHLD
    sa.sa_handler = client_termination_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("Error setting up SIGCHLD handler");
        exitFailure();
    }
    // Set up handler for SIGINT
    sa.sa_handler = kill_server;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("Error setting up SIGINT handler");
        exitFailure();
    }
    // handler for SIGTSTP
    sa.sa_handler = kill_server;
    if (sigaction(SIGTSTP, &sa, NULL) == -1) {
        perror("Error setting up SIGTSTP handler");
        exitFailure();
    }

    int shm_fd, shm_size;
    if ((shm_fd = shm_open(SHARED, O_CREAT | O_RDWR, 0666)) == -1) exitFailure();

    // Shared memory size
    shm_size = sizeof(int) * 2 + sizeof(File) * 64;
    if (ftruncate(shm_fd, shm_size) == -1) exitFailure();
    if ((sd = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0)) == MAP_FAILED) exitFailure();
    initialize(sd);

    queue_constructor(&wait_que);
    queue_constructor(&clients);

    server_dir = argv[1];
    int max_clients = atoi(argv[2]);

    setup_server_directory();
    setup_log_file();
    setup_server_fifo();

    accept_connections(max_clients);
    return 0;
}
