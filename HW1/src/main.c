#include <stdio.h>

#include <ctype.h>

#include <stdlib.h>

#include <string.h>

#include <unistd.h>

#include <sys/wait.h>

#include <fcntl.h>

#include <time.h>

#define MAX_NAME_LENGTH 30
#define MAX_GRADE_LENGTH 4
#define MAX_LINE_LENGTH 35
#define MAX_COMMAND_LENGTH 90
#define MAX_STUDENTS 100
#define MAX_LOG_LENGTH 200

typedef struct {
  char name[MAX_NAME_LENGTH];
  char grade[MAX_GRADE_LENGTH];
}
Student;

void createFile(const char * filename);
void addStudentGrade(const char * filename, const char * name, const char * grade);
void searchStudent(const char * filename, const char * name);
void sortAll(const char * filename);
void sortByNameAscending(Student * students, int numStudents);
void sortByNameDescending(Student * students, int numStudents);
void sortByGradeAscending(Student * students, int numStudents);
void sortByGradeDescending(Student * students, int numStudents);
void showAll(const char * filename);
void listGrades(const char * filename);
void listSome(int numofEntries, int pageNumber, const char * filename);
void logFile(const char message[MAX_LOG_LENGTH]);

int main() {
  char command[MAX_COMMAND_LENGTH];
  char * filename = NULL;
  char * old_brk = sbrk(0);

  for (;;) {
    printf("\nEnter command: ");
    fflush(stdout);
    ssize_t bytes_read = read(STDIN_FILENO, command, sizeof(command));
    if (bytes_read == -1) {
      perror("\tError reading input");
      continue;
    }
    command[bytes_read - 1] = '\0'; // Remove trailing newline
    char writelog[MAX_LOG_LENGTH];
    snprintf(writelog, sizeof(writelog), "INPUT: %s", command);
    logFile(writelog);
    char * token = strtok(command, "\"");
    if (strcmp(token, "gtuStudentGrades ") == 0) {
      token = strtok(NULL, "\"");
      if (token == NULL) {
        logFile("Invalid command format is entered.");
        printf("\tInvalid command format.\n");
        continue;
      }
      int length = strlen(token) + 1;
      if (brk(old_brk + length) == -1) { // Adjust the data segment size
        logFile("memory allocation using brk failed.");
        perror("\tbrk failed");
        brk(old_brk);
        exit(1);
      }
      filename = old_brk;
      strcpy(filename, token); // Copy token to filename
      createFile(filename);
    }else if (strcmp(token, "quit") == 0) {
        brk(old_brk);
        if (filename != NULL) brk(filename);
        return 0;
    }else if (strcmp(token, "gtuStudentGrades") == 0) { //user manual
      printf("***USER MANUAL***\n---------------------------------------------------\n");
      printf("gtuStudentGrades \"filename\"\nThis command is used to create or open a file.\nNOTE!!: To add students to a file or to search a student in file first you need to open a file using this command. Changes will be done into last opened file.\n---------------------------------------------------\n");
      printf("addStudentGrade \"Name Surname\" \"Grade\"\nThis command is used to add a student and their grade into the file.\nNOTE!!: Only aa, ba, bb, cb, cc, dc, dd, ff, na, vf grades are allowed\n---------------------------------------------------\n");
      printf("searchStudent \"Name Surname\"\nThis command is used to find a student and their grade from the file.\n---------------------------------------------------\n");
      printf("sortAll \"filename\"\nThis command is used to sort all students and their grades inside the chosen file.\nThe sorting options are: 1)Name ascending\t2)Name descending\t3)Grade ascending\t4)Grade descending\n---------------------------------------------------\n");
      printf("showAll \"filename\"\nThis command is used to print all students and their grades inside the file.\n---------------------------------------------------\n");
      printf("listGrades \"filename\"\nThis command is used to print first 5 students and their grades inside the file.\n---------------------------------------------------\n");
      printf("listSome \"numberOfEntries\" \"pageNumber\" \"filename\"\nThis command is used to divide all students into pages each with numberOfEntries. pageNumber variable is for printing the page with entered pageNumber.\nEXAMPLE!!: listSome \"5\" \"2\" \"grades.txt\" will list entries from 6th to 10th.\n\tlistSome \"4\" \"3\" \"grades.txt\" will list entries from 9th to 12th.\n---------------------------------------------------\n");
      printf("defaultTest\nEnter this command to begin with default test demo.\n---------------------------------------------------\n");
      printf("quit\nEnter this command to quit program.\n---------------------------------------------------\n");
      printf("All error and success records are printed into the file 'event.log'\n");
      logFile("USER MANUAL IS PRINTED");
    } else if (strcmp(token, "defaultTest") == 0) { //default test demo
      printf("Create file 'default.txt'\n---------------------------------------------------\n");
      sleep(2);
      createFile("default.txt");
      printf("\nAdd 16 students with grades\n---------------------------------------------------\n");
      sleep(2);
      addStudentGrade("default.txt", "student a", "CC");
      addStudentGrade("default.txt", "student b", "CB");
      addStudentGrade("default.txt", "student c", "BA");
      addStudentGrade("default.txt", "student d", "BB");
      addStudentGrade("default.txt", "student e", "FF");
      addStudentGrade("default.txt", "student f", "NA");
      addStudentGrade("default.txt", "student g", "VF");
      addStudentGrade("default.txt", "student h", "BA");
      addStudentGrade("default.txt", "student i", "AA");
      addStudentGrade("default.txt", "student j", "CC");
      addStudentGrade("default.txt", "student k", "CA");
      addStudentGrade("default.txt", "student l", "FF");
      addStudentGrade("default.txt", "student m", "BA");
      addStudentGrade("default.txt", "student n", "BB");
      addStudentGrade("default.txt", "student o", "AA");
      addStudentGrade("default.txt", "student p", "DD");
      printf("\nSearch for students: a h and p\n---------------------------------------------------\n");
      sleep(2);
      searchStudent("default.txt", "student a");
      searchStudent("default.txt", "student h");
      searchStudent("default.txt", "student p");
      printf("\nSort students, choose every option.\n---------------------------------------------------\n");
      sleep(2);
      sortAll("default.txt");
      sortAll("default.txt");
      sortAll("default.txt");
      sortAll("default.txt");
      printf("\nList all students\n---------------------------------------------------\n");
      sleep(2);
      showAll("default.txt");
      printf("\nList first 5 grades\n---------------------------------------------------\n");
      sleep(2);
      listGrades("default.txt");
      printf("\nlistSome 5 1, listSome 8 2, listSome 4 3\n---------------------------------------------------\n");
      sleep(2);
      listSome(5, 1, "default.txt");
      listSome(8, 2, "default.txt");
      listSome(4, 3, "default.txt");
      logFile("DEMO FINISHED");
    } else if (strcmp(token, "addStudentGrade ") == 0) { //add student
      char * name = strtok(NULL, "\"");
      strtok(NULL, "\"");
      char * grade = strtok(NULL, "\"");
      if (name == NULL || grade == NULL) {
        logFile("Invalid command format is entered.");
        printf("\tInvalid command format.\n");
        continue;
      }
      for (int i = 0; name[i]; i++) name[i] = tolower(name[i]);
      for (int i = 0; grade[i]; i++) grade[i] = toupper(grade[i]);
      if (strcmp(grade, "AA") != 0 && strcmp(grade, "BA") != 0 && strcmp(grade, "BB") != 0 &&
        strcmp(grade, "CB") != 0 && strcmp(grade, "CC") != 0 && strcmp(grade, "DC") != 0 &&
        strcmp(grade, "DD") != 0 && strcmp(grade, "FF") != 0 && strcmp(grade, "NA") != 0 &&
        strcmp(grade, "VF") != 0) {
        logFile("Grade is not valid.");
        printf("\tGrade is not valid.\n");
        continue;
      }
      addStudentGrade(filename, name, grade);
    } else if (strcmp(token, "searchStudent ") == 0) { //search student
      char * name = strtok(NULL, "\"");
      if (name == NULL) {
        logFile("Invalid command format is entered.");
        printf("\tInvalid command format.\n");
        continue;
      }
      for (int i = 0; name[i]; i++) name[i] = tolower(name[i]);
      searchStudent(filename, name);
    } else if (strcmp(token, "sortAll ") == 0) {
      token = strtok(NULL, "\"");
      if (token == NULL) {
        logFile("Invalid command format is entered.");
        printf("\tInvalid command format.\n");
        continue;
      }
      int length = strlen(token) + 1;
      if (brk(old_brk + length) == -1) { 
        logFile("memory allocation using brk failed.");
        perror("\tbrk failed");
        brk(old_brk);
        exit(1);
      }
      filename = old_brk;
      strcpy(filename, token); 
      sortAll(filename);
    } else if (strcmp(token, "showAll ") == 0) {
      token = strtok(NULL, "\"");
      if (token == NULL) {
        logFile("Invalid command format is entered.");
        printf("\tInvalid command format.\n");
        continue;
      }
      int length = strlen(token) + 1;
      if (brk(old_brk + length) == -1) { 
        logFile("memory allocation using brk failed.");
        perror("\tbrk failed");
        brk(old_brk);
        exit(1);
      }
      filename = old_brk;
      strcpy(filename, token); 
      showAll(filename);
    } else if (strcmp(token, "listGrades ") == 0) {
      token = strtok(NULL, "\"");
      if (token == NULL) {
        logFile("Invalid command format is entered.");
        printf("\tInvalid command format.\n");
        continue;
      }
      int length = strlen(token) + 1;
      if (brk(old_brk + length) == -1) { 
        logFile("memory allocation using brk failed.");
        perror("\tbrk failed");
        brk(old_brk);
        exit(1);
      }
      filename = old_brk;
      strcpy(filename, token); 
      listGrades(filename);
    } else if (strcmp(token, "listSome ") == 0) {
      int number1 = 0, number2 = 0;
      char * token = strtok(NULL, "\"");
      number1 = atoi(token) + number1;
      strtok(NULL, "\"");
      token = strtok(NULL, "\"");
      number2 = atoi(token) + number2;
      if (number1 == 0 || number2 == 0) {
        continue;
      }
      strtok(NULL, "\"");
      token = strtok(NULL, "\"");
      if (token == NULL) {
        logFile("Invalid command format is entered.");
        printf("\tInvalid command format.\n");
        continue;
      }
      int length = strlen(token) + 1;
      if (brk(old_brk + length) == -1) { 
        logFile("memory allocation using brk failed.");
        perror("\tbrk failed");
        brk(old_brk);
        exit(1);
      }
      filename = old_brk;
      strcpy(filename, token); 
      listSome(number1, number2, filename);
    } else {
      printf("\tInvalid command.\n");
    }
  }
  brk(old_brk);
  if (filename != NULL) brk(filename);
  return 0;
}

void createFile(const char * filename) {
  // Fork a child process to handle file creation
  pid_t pid = fork();

  if (pid == -1) {
    // Error occurred
    perror("\tFork failed");
    logFile("Fork failed");
    exit(EXIT_FAILURE);
  } else if (pid == 0) {
    // Child process
    int fd = open(filename, O_WRONLY | O_CREAT, 0666);
    if (fd == -1) {
      perror("\tError opening file");
      logFile("Error opening file");
      exit(EXIT_FAILURE);
    }
    close(fd); // Close the file descriptor
    char writelog[MAX_LOG_LENGTH];
    printf("\tFile '%s' opened successfully\n", filename);
    snprintf(writelog, sizeof(writelog), "File '%s' opened successfully", filename);
    logFile(writelog);
    exit(EXIT_SUCCESS);
  } else {
    //Parent process
    int status;
    pid_t child_pid = waitpid(pid, & status, 0);

    if (child_pid == -1) {
      perror("waitpid failed");
      logFile("waitpid failed");
      exit(EXIT_FAILURE);
    }

    if (WIFEXITED(status)) {
      printf("Child process exited with status: %d\n", WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
      // Child process terminated by a signal
      printf("Child process terminated by signal: %d\n", WTERMSIG(status));
      char writelog[MAX_LOG_LENGTH];
      snprintf(writelog, sizeof(writelog), "Child process terminated by signal: %d", WTERMSIG(status));
      logFile(writelog);
    }
  }
}

void addStudentGrade(const char * filename,
  const char * name,
    const char * grade) {
  // Fork a child process to handle file manipulation
  pid_t pid = fork();

  if (pid == -1) {
    // Error occurred
    perror("\tFork failed");
    logFile("Fork failed");
    exit(EXIT_FAILURE);
  } else if (pid == 0) {
    // Child process
    int fd = open(filename, O_WRONLY | O_APPEND, 0666);
    if (fd == -1) {
      perror("\tError opening file");
      logFile("Error opening file");
      exit(EXIT_FAILURE);
    }

    // Write student name and grade to the file
    char buffer[MAX_LINE_LENGTH];
    int len = snprintf(buffer, sizeof(buffer), "%s, %s\n", name, grade);
    if (write(fd, buffer, len) != len) {
      perror("\tError writing to file");
      logFile("Error writing to file");
      close(fd);
      exit(EXIT_FAILURE);
    }

    // Close the file descriptor      
    close(fd);

    printf("\tStudent grade added successfully.\n");
    char writelog[MAX_LOG_LENGTH];
    snprintf(writelog, sizeof(writelog), "Student grade added succesfully: '%s', '%s'.", name, grade);
    logFile(writelog);
    exit(EXIT_SUCCESS);
  } else {
    //Parent process
    int status;
    pid_t child_pid = waitpid(pid, & status, 0);

    if (child_pid == -1) {
      perror("waitpid failed");
      logFile("waitpid failed");
      exit(EXIT_FAILURE);
    }

    if (WIFEXITED(status)) {
      printf("Child process exited with status: %d\n", WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
      // Child process terminated by a signal
      printf("Child process terminated by signal: %d\n", WTERMSIG(status));
      char writelog[MAX_LOG_LENGTH];
      snprintf(writelog, sizeof(writelog), "Child process terminated by signal: %d", WTERMSIG(status));
      logFile(writelog);
    }
  }
}
void searchStudent(const char * filename,
  const char * name) {
  pid_t pid = fork();

  if (pid == -1) {
    // Error occurred
    perror("\tFork failed");
    logFile("Fork failed");
    exit(EXIT_FAILURE);
  } else if (pid == 0) {
    // Child process
    // Open the file for reading
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
      perror("\tError opening file");
      logFile("Error opening file");
      exit(EXIT_FAILURE);
    }
    // Buffer for reading file content
    char buffer[MAX_LINE_LENGTH];
    int bytes_read;
    char ch;
    int index = 0;
    int found = 0;
    // Read file character by character
    while ((bytes_read = read(fd, & ch, 1)) > 0) {
      if (ch == '\n' || index >= MAX_LINE_LENGTH - 1) {
        // End of line or buffer is full, terminate buffer
        buffer[index] = '\0';

        // Tokenize the line
        char * token = strtok(buffer, ",");
        if (token != NULL && strcmp(token, name) == 0) {
          found = 1;
          printf("\tStudent: %s\n", token);
          token = strtok(NULL, "\n");
          printf("\tGrade: %s\n", token);
          break; // Stop searching after finding the first occurrence
        }
        // Reset buffer index for the next line
        index = 0;
      } else {
        // Store character in buffer
        buffer[index++] = ch;
      }
    }
    // Close the file descriptor
    close(fd);
    if (!found) {
      printf("\tStudent not found\n");
      char writelog[MAX_LOG_LENGTH];
      snprintf(writelog, sizeof(writelog), "Student named '%s' is not found inside file: %s.", name, filename);
      logFile(writelog);
    } else {
      char writelog[MAX_LOG_LENGTH];
      snprintf(writelog, sizeof(writelog), "Student named '%s' is found inside file: %s.", name, filename);
      logFile(writelog);
    }
    exit(EXIT_SUCCESS);
  } else {
    //Parent process
    int status;
    pid_t child_pid = waitpid(pid, & status, 0);

    if (child_pid == -1) {
      perror("waitpid failed");
      logFile("waitpid failed");
      exit(EXIT_FAILURE);
    }

    if (WIFEXITED(status)) {
      printf("Child process exited with status: %d\n", WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
      // Child process terminated by a signal
      printf("Child process terminated by signal: %d\n", WTERMSIG(status));
      char writelog[MAX_LOG_LENGTH];
      snprintf(writelog, sizeof(writelog), "Child process terminated by signal: %d", WTERMSIG(status));
      logFile(writelog);
    }
  }
}
void sortAll(const char * filename) {
  char inputBuffer[5];
  Student students[MAX_STUDENTS];
  int choice, valid, numStudents = 0;
  do {
    valid = 1;
    printf("Select Sort Type:\n\t1)Name ascending\t2)Name descending\n\t3)Grade ascending\t4)Grade descending\n");
    logFile("Getting sort type from user...");
    ssize_t bytes_read = read(STDIN_FILENO, inputBuffer, sizeof(inputBuffer));
    if (bytes_read <= 0) {
      perror("Error reading input");
      logFile("Error reading input");
      exit(EXIT_FAILURE);
    }
    // Null-terminate the string
    inputBuffer[bytes_read - 1] = '\0'; // Remove trailing newline
    // Convert string to integer
    choice = atoi(inputBuffer);
    if (choice < 1 || choice > 4) {
      printf("Invalid choice\n");
      logFile("User entered invalid choice");
      valid = 0;
    }
  } while (valid == 0);

  pid_t pid = fork();
  if (pid == -1) {
    // Error occurred
    perror("\tFork failed");
    logFile("Fork failed");
    exit(EXIT_FAILURE);
  } else if (pid == 0) {
    // Child process
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
      perror("Error opening file");
      logFile("Error opening file");
      exit(EXIT_FAILURE);
    }
    char buffer[MAX_LINE_LENGTH];
    int index = 0;
    ssize_t bytes_read;
    char ch;

    while ((bytes_read = read(fd, & ch, 1)) > 0) {
      if (ch == '\n' || index >= MAX_LINE_LENGTH - 1) {
        buffer[index] = '\0';
        // Tokenize the line
        char * token = strtok(buffer, ",");
        if (token != NULL) {
          strncpy(students[numStudents].name, token, MAX_NAME_LENGTH - 1);
          students[numStudents].name[MAX_NAME_LENGTH - 1] = '\0';
          token = strtok(NULL, ",");
          if (token != NULL) {
            strncpy(students[numStudents].grade, token, MAX_GRADE_LENGTH);
            students[numStudents].grade[MAX_GRADE_LENGTH - 1] = '\0';
            numStudents++;
          }
        }
        // Reset buffer index for the next line
        index = 0;
      } else {
        // Store character in buffer
        buffer[index++] = ch;
      }
    }

    if (bytes_read == -1) {
      perror("Error reading file");
      logFile("Error reading file");
      close(fd);
    }

    // Close the file
    close(fd);

    switch (choice) {
    case 1:
      sortByNameAscending(students, numStudents);
      logFile("Sorted by 'Name' 'Ascending'.");
      break;
    case 2:
      sortByNameDescending(students, numStudents);
      logFile("Sorted by 'Name' 'Descending'.");
      break;
    case 3:
      sortByGradeAscending(students, numStudents);
      logFile("Sorted by 'Grade' 'Ascending'.");
      break;
    case 4:
      sortByGradeDescending(students, numStudents);
      logFile("Sorted by 'Grade' 'Descending'.");
      break;
    default:
      exit(EXIT_FAILURE);
      logFile("An error occured.");

    }

    for (int i = 0; i < numStudents; i++) {
      printf("%d:  %s, %s\n", i, students[i].name, students[i].grade);
    }
    exit(EXIT_SUCCESS);
  } else {
    // Parent process
    int status;
    pid_t child_pid = waitpid(pid, & status, 0);

    if (child_pid == -1) {
      perror("waitpid failed");
      logFile("waitpid failed");
      exit(EXIT_FAILURE);
    }

    if (WIFEXITED(status)) {
      printf("Child process exited with status: %d\n", WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
      // Child process terminated by a signal
      printf("Child process terminated by signal: %d\n", WTERMSIG(status));
      char writelog[MAX_LOG_LENGTH];
      snprintf(writelog, sizeof(writelog), "Child process terminated by signal: %d", WTERMSIG(status));
      logFile(writelog);
    }
  }
}

void sortByNameDescending(Student * data, int length) {
  for (int i = 0; i < length - 1; i++) {
    for (int j = 0; j < length - i - 1; j++) {
      if (strcmp(data[j].name, data[j + 1].name) > 0) {
        Student temp = data[j];
        data[j] = data[j + 1];
        data[j + 1] = temp;
      }
    }
  }
}
void sortByNameAscending(Student * data, int length) {
  for (int i = 0; i < length - 1; i++) {
    for (int j = 0; j < length - i - 1; j++) {
      if (strcmp(data[j].name, data[j + 1].name) < 0) {
        Student temp = data[j];
        data[j] = data[j + 1];
        data[j + 1] = temp;
      }
    }
  }
}
void sortByGradeDescending(Student * data, int length) {
  for (int i = 0; i < length - 1; i++) {
    for (int j = 0; j < length - i - 1; j++) {
      if (strcmp(data[j].grade, data[j + 1].grade) > 0) {
        Student temp = data[j];
        data[j] = data[j + 1];
        data[j + 1] = temp;
      }
    }
  }
}
void sortByGradeAscending(Student * data, int length) {
  for (int i = 0; i < length - 1; i++) {
    for (int j = 0; j < length - i - 1; j++) {
      if (strcmp(data[j].grade, data[j + 1].grade) < 0) {
        Student temp = data[j];
        data[j] = data[j + 1];
        data[j + 1] = temp;
      }
    }
  }
}
void showAll(const char * filename) {
  pid_t pid = fork();

  if (pid == -1) {
    printf("Error: Fork failed.\n");
    logFile("Fork failed");
    return;
  }

  if (pid == 0) { // Child process
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
      printf("Error: Unable to open file %s\n", filename);
      logFile("Error opening file");
      exit(EXIT_FAILURE);
    }
    char buffer[MAX_LINE_LENGTH];
    ssize_t bytes_read;
    while ((bytes_read = read(fd, buffer, MAX_LINE_LENGTH)) > 0) {
      write(STDOUT_FILENO, buffer, bytes_read);
    }

    close(fd);
    char writelog[MAX_LOG_LENGTH];
    snprintf(writelog, sizeof(writelog), "All students insde '%s' are listed succesfully.", filename);
    logFile(writelog);
    exit(EXIT_SUCCESS);
  } else {
    // Parent process
    int status;
    pid_t child_pid = waitpid(pid, & status, 0);

    if (child_pid == -1) {
      perror("waitpid failed");
      logFile("waitpid failed");
      exit(EXIT_FAILURE);
    }

    if (WIFEXITED(status)) {
      printf("Child process exited with status: %d\n", WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
      // Child process terminated by a signal
      printf("Child process terminated by signal: %d\n", WTERMSIG(status));
      char writelog[MAX_LOG_LENGTH];
      snprintf(writelog, sizeof(writelog), "Child process terminated by signal: %d", WTERMSIG(status));
      logFile(writelog);
    }
  }
}
void listGrades(const char * filename) {
  pid_t pid = fork();

  if (pid == -1) {
    printf("\tFork failed.\n");
    logFile("Fork failed");
    return;
  }

  if (pid == 0) { // Child process
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
      printf("\tError: Unable to open file %s\n", filename);
      logFile("Error opening file");
      exit(EXIT_FAILURE);
    }
    // Only print while pointing to a line which is less than 5
    char buffer[MAX_LINE_LENGTH];
    ssize_t bytes_read;
    int lineNumber = 0;
    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0 && lineNumber < 5) {
      for (int i = 0; i < bytes_read && lineNumber < 5; i++) {
        printf("%c", buffer[i]);
        if (buffer[i] == '\n') {
          lineNumber++;
        }
      }
    }

    if (bytes_read == -1) {
      printf("\tFailed to read from file %s\n", filename);
      logFile("Error reading file");
      close(fd);
      exit(EXIT_FAILURE);
    }
    close(fd);
    char writelog[MAX_LOG_LENGTH];
    snprintf(writelog, sizeof(writelog), "First 5 students in '%s' are listed succesfully.", filename);
    logFile(writelog);
    exit(EXIT_SUCCESS);
  } else { 
    // Parent process
    int status;
    pid_t child_pid = waitpid(pid, & status, 0);

    if (child_pid == -1) {
      perror("waitpid failed");
      logFile("waitpid failed");
      exit(EXIT_FAILURE);
    }

    if (WIFEXITED(status)) {
      printf("Child process exited with status: %d\n", WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
      // Child process terminated by a signal
      printf("Child process terminated by signal: %d\n", WTERMSIG(status));
      char writelog[MAX_LOG_LENGTH];
      snprintf(writelog, sizeof(writelog), "Child process terminated by signal: %d", WTERMSIG(status));
      logFile(writelog);
    }
  }
}
void listSome(int numofEntries, int pageNumber,
  const char * filename) {
  pid_t pid = fork();

  if (pid == -1) {
    printf("\tFork failed.\n");
    logFile("Fork failed");
    return;
  }

  if (pid == 0) { // Child process
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
      printf("\tError: Unable to open file %s\n", filename);
      logFile("Error opening file");
      exit(EXIT_FAILURE);
    }

    char buffer[MAX_LINE_LENGTH];
    ssize_t bytes_read;
    int lineNumber = 0;
    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0 && lineNumber < numofEntries * pageNumber) {
      for (int i = 0; i < bytes_read && lineNumber < numofEntries * pageNumber; i++) {
        if (buffer[i] == '\n') {
          lineNumber++;
        }
        if (lineNumber >= (numofEntries * pageNumber) - numofEntries) {
          printf("%c", buffer[i]);
        }
      }
    }

    if (bytes_read == -1) {
      printf("\tError: Failed to read from file %s\n", filename);
      logFile("Error reading file");
      close(fd);
      exit(EXIT_FAILURE);
    }
    close(fd);
    char writelog[MAX_LOG_LENGTH];
    snprintf(writelog, sizeof(writelog), "In '%s' students from %dth to %dth entries are listed.", filename, (numofEntries * pageNumber) - numofEntries + 1, numofEntries * pageNumber);
    logFile(writelog);
    exit(EXIT_SUCCESS);
  } else {
    // Parent process
    int status;
    pid_t child_pid = waitpid(pid, & status, 0);

    if (child_pid == -1) {
      perror("waitpid failed");
      exit(EXIT_FAILURE);
    }

    if (WIFEXITED(status)) {
      printf("Child process exited with status: %d\n", WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
      // Child process terminated by a signal
      printf("Child process terminated by signal: %d\n", WTERMSIG(status));
      char writelog[MAX_LOG_LENGTH];
      snprintf(writelog, sizeof(writelog), "Child process terminated by signal: %d", WTERMSIG(status));
      logFile(writelog);
    }
  }
}

void logFile(const char * task) {
  pid_t pid = fork();

  if (pid == -1) {
    printf("\tFork failed.\n");
    logFile("Fork failed");
    return;
  }

  if (pid == 0) {
    // Open the log file in append mode
    int log_fd = open("events.log", O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
    if (log_fd == -1) {
      printf("Error: Unable to open or create log file %s\n", "events.log");
      exit(EXIT_FAILURE);
    }

    // Write the log entry to the file
    char log_entry[MAX_LOG_LENGTH];
    snprintf(log_entry, MAX_LOG_LENGTH, "%s\n", task);
    ssize_t bytes_written = write(log_fd, log_entry, strlen(log_entry));
    if (bytes_written == -1) {
      printf("Error: Unable to write to log file %s\n", "events.log");
    }
    close(log_fd);
    exit(EXIT_SUCCESS);
  } else {
    // Parent process
    int status;
    pid_t child_pid = waitpid(pid, & status, 0);

    if (child_pid == -1) {
      perror("waitpid failed");
      exit(EXIT_FAILURE);
    }
  }
}