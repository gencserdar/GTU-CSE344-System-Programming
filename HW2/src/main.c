#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h> 
#include <time.h>     


#define FIFO1 "fifo1"
#define FIFO2 "fifo2"

int child_counter=0;
pid_t child_pid1, child_pid2, zombie_pid;

int create_fifo(const char *fifo_name) {
    if (mkfifo(fifo_name, 0666) == -1) {
        perror("Error creating FIFO");
        return -1;
    }
    return 0;
}
void sigchld_handler(int signum) {
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        printf("Child process with PID %d exited with status %d\n", pid, WEXITSTATUS(status));
        child_counter++;
    }
}
void sigint_handler(int signum) {
    printf("\nCtrl+C signal detected. Cleaning up and terminating.\n");
    unlink(FIFO1);
    unlink(FIFO2);
    exit(EXIT_FAILURE);
}

void sigtstp_handler(int signum) {
    printf("\nCtrl+Z signal detected. Cleaning up and terminating.\n");
    unlink(FIFO1);
    unlink(FIFO2);
    exit(EXIT_FAILURE);
}

void sigkill_handler(int signum) {
    int status;
    kill(zombie_pid, SIGKILL);
    waitpid(zombie_pid, &status, 0);
    // Check if the child process terminated normally or was killed by a signal
    if (WIFEXITED(status)) {
        printf("Child process with PID %d exited with status %d\n", zombie_pid, WEXITSTATUS(status));
    } else if (WIFSIGNALED(status) && WTERMSIG(status) == SIGKILL) {
        printf("Child process with PID %d was terminated by signal %d\n", zombie_pid, WTERMSIG(status));
    }
    exit(EXIT_FAILURE);
}

int main() {
    int size;
    //signal handlers
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigchld_handler;
    sigaction(SIGCHLD, &sa, NULL);
    sa.sa_handler = sigint_handler;
    sigaction(SIGINT, &sa, NULL);
    sa.sa_handler = sigtstp_handler;
    sigaction(SIGTSTP, &sa, NULL);
    sa.sa_handler = sigkill_handler;
    sigaction(SIGKILL, &sa, NULL);
    // Prompt user for the size of the array
    printf("Enter the size of the array: ");
    scanf("%d", &size);

    // Validate size
    if (size <= 0) {
        printf("Invalid size. Size must be a positive integer.\n");
        return 1; 
    }
    
    if (create_fifo(FIFO1)==-1) exit(EXIT_FAILURE);
    printf("created fifo1\n");
    if (create_fifo(FIFO2)==-1){
        unlink(FIFO1);
        exit(EXIT_FAILURE);
    };
    printf("created fifo2\n");
    
    int *array = (int *)malloc(size * sizeof(int));
    if (array == NULL) {
        printf("Memory allocation for array failed.\n");
        return 1; 
    }

    srand(time(NULL));
    printf("The array is: [ ");
    // Assign random numbers from 1 to 10 into the array
    for (int i = 0; i < size; i++) {
        array[i] = rand() % 10 + 1; // Generate random number between 1 and 10
        printf("%d ",array[i]);
    }
    printf("]\n");

//CHILD 2 **************************************
    pid_t pid2 = fork();
    if (pid2 < 0) {
        perror("Forking for child process 2 failed.");
        free(array);
        exit(EXIT_FAILURE);
    } else if (pid2 == 0) {
        sleep(10);
        free(array);
        printf("CHILD 2: opening FIFO2 to read the array\n");
        int fd2 = open(FIFO2, O_RDONLY);
        if (fd2 == -1) {
            perror("Opening FIFO2 to read array failed.");
            exit(EXIT_FAILURE);
        }
        int *array2 = (int *)malloc(size * sizeof(int));
        if (array2 == NULL) {
            printf("Memory allocation for array2 failed.\n");
            close(fd2);
            return 1; 
        }
        int num;
        for (int i = 0; i < size; i++) {
            if (read(fd2, &num, sizeof(int)) == -1) {
                perror("Reading the array element from FIFO2 failed");
                close(fd2);
                free(array2);
                exit(EXIT_FAILURE);
            }
            array2[i]=num;
        }
        printf("CHILD 2: succesfully read integers from FIFO2\n");
        printf("CHILD 2: reading the command from FIFO2\n");
        char command[20];
        if (read(fd2, command, sizeof(command)) == -1) {
            perror("Read command from FIFO2 failed");
            close(fd2);
            free(array2);
            exit(EXIT_FAILURE);
        }
        printf("CHILD 2: succesfully read command from FIFO2, command is: %s\n", command);
        int result =1;
        close(fd2);
        if (strcmp(command, "multiply") == 0) {
            for (int i = 0; i < size; i++) {
                result *= array2[i];
            }
            free(array2);
        }else{
            printf("Command is invalid, exiting child 2.\n");
            free(array2);
            exit(EXIT_FAILURE);
        }

        printf("CHILD 2: reading the sum from FIFO2\n");
        int sum;
        fd2 = open(FIFO2, O_RDONLY);
        if (fd2 == -1) {
            perror("Opening FIFO2 to read sum failed");
            exit(EXIT_FAILURE);
        }
        if (read(fd2, &sum, sizeof(int)) == -1) {
            perror("Reading sum from FIFO2 failed");
            close(fd2);
            exit(EXIT_FAILURE);
        }
        printf("CHILD 2: succesfully read the sum from FIFO2\n");
        close(fd2);
        printf("sum: %d\nmult: %d\n\n",sum,result);
    }
    else{
//CHILD 1 **************************************
        pid_t pid1 = fork();
        if (pid1 < 0) {
            perror("Forking for child process 1 failed");
            free(array);
            exit(EXIT_FAILURE);
        } else if (pid1 == 0) {
            sleep(10);
            free(array);
            printf("CHILD 1: open FIFO1 to read the array\n");
            int fd1 = open(FIFO1, O_RDONLY);
            if (fd1 == -1) {
                perror("Opening FIFO1 to read array failed");
                exit(EXIT_FAILURE);
            }

            int sum = 0;
            for (int i = 0; i < size; i++) {
                int num;
                if (read(fd1, &num, sizeof(int)) == -1) {
                    perror("Reading array element from FIFO1 failed");
                    close(fd1);
                    exit(EXIT_FAILURE);
                }
                sum += num;
            }
            printf("CHILD 1: succesfully read integers from FIFO1 and the sum is: %d\n", sum);
            close(fd1);
            
            printf("CHILD 1: opening FIFO2 to write the sum\n");
            int fd2 = open(FIFO2, O_WRONLY);
            if (fd2 == -1) {
                perror("Opening FIFO2 to write sum failed");
                exit(EXIT_FAILURE);
            }
            
            if (write(fd2, &sum, sizeof(int)) == -1) {
                perror("Writing sum into FIFO2 failed");
                close(fd2);
                exit(EXIT_FAILURE);
            }
            close(fd2);
            printf("CHILD 1: succesfully wrote the sum into FIFO2\n");
            exit(EXIT_SUCCESS);
        }
        else{
//PARENT **************************************************
            child_pid1 = pid1;
            child_pid2 = pid2; 
            int fd2 = open(FIFO2, O_WRONLY);
            if (fd2 == -1) {
                perror("Opening FIFO2 to write the array failed");
                unlink(FIFO1);
                unlink(FIFO2);
                free(array);
                exit(EXIT_FAILURE);
            }
            for (int i = 0; i < size; i++) {
                if (write(fd2, &array[i], sizeof(int)) == -1) {
                    perror("Writing the array into FIFO2 failed");
                    close(fd2);
                    unlink(FIFO1);
                    unlink(FIFO2);
                    free(array);
                    exit(EXIT_FAILURE);
                }
            }
            const char *command = "multiply";
            if (write(fd2, command, strlen(command) + 1) == -1) {
                perror("Writing command into FIFO2 failed");
                close(fd2);
                unlink(FIFO1);
                unlink(FIFO2);
                free(array);
                exit(EXIT_FAILURE);
            }
            close(fd2);
            int fd1 = open(FIFO1, O_WRONLY);
            if (fd1 == -1) {
                perror("Opening FIFO1 to write the array failed");
                unlink(FIFO1);
                unlink(FIFO2);
                free(array);
                exit(EXIT_FAILURE);
            }
            for (int i = 0; i < size; i++) {
                if (write(fd1, &array[i], sizeof(int)) == -1) {
                    perror("Writing the array into FIFO1 failed");
                    close(fd1);
                    unlink(FIFO1);
                    unlink(FIFO2);
                    free(array);
                    exit(EXIT_FAILURE);
                }
            }
            close(fd1);
            int timeout=0;
            while (child_counter < 2) {
                printf("Proceeding...\n");
                sleep(2);
                timeout++;
                //set timeout to check if there is a zombie process
                if(timeout > 3){
                    printf("\nTimeout reached. Terminating the program.\n");
                    close(fd1);
                    unlink(FIFO1);
                    unlink(FIFO2);
                    free(array);
                    //kill zombie process
                    if (child_pid1 > 0){
                        zombie_pid=child_pid1;
                        sigkill_handler(SIGKILL);
                    }
                    if (child_pid2 > 0){
                        zombie_pid=child_pid2;
                        sigkill_handler(SIGKILL);
                    }
                    exit(EXIT_FAILURE);
                }
            }
            free(array);
            unlink(FIFO1);
            unlink(FIFO2);
        }
    }
}