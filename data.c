#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <errno.h>

#define MAX_POEM_LENGTH 255
#define DATA_FILE "poems.txt"
#define TEMP_FILE "poem_edit.txt"
#define NUM_BOYS 4
#define MSG_SIZE 2048
#define MSG_TYPE 1

typedef struct {
    long msg_type;
    char poem[MSG_SIZE];
} message;

void addPoem();
void listPoems();
void deletePoem();
void modifyPoem();
void viewPoem();

void sendPoemsToBoy(int pipe_fd[2], char* poem1, char* poem2);
void receivePoemFromBoy(int msg_queue_id);
void performWateringRitual(int pipe_fd[2], int msg_queue_id);
void childProcess(int pipe_fd[2], int msg_queue_id);
void clearMessageQueue(int msg_queue_id);

void extractPoemToFile(int poemNumber, const char* filePath);
void updatePoemInFile(int poemNumber, const char* filePath);

int getPoemCount();

char menu();

int main() {
    int pipe_fd[2];
    int msg_queue_id;
    key_t key;
    pid_t pids[NUM_BOYS];
    // int selected_boy;

    // Setup message queue
    key = ftok("mama_bunny", 65);
    msg_queue_id = msgget(key, 0666 | IPC_CREAT);
    // message.msg_type = 1; // Message type for the message queue

    if (msg_queue_id == -1) {
        perror("Message queue creation failed");
        return 1;
    } else {
        printf("Message queue successfully created with ID: %d\n", msg_queue_id);
    }

    // Setup pipe
    if (pipe(pipe_fd) == -1) {
        perror("Failed to create pipe");
        return 1;
    }

    srand(time(NULL));  // Seed random number generator

    char choice;
    while ((choice = menu()) != 'q') {
        switch (choice) {
            case 'a':
                addPoem();
                break;
            case 'l':
                listPoems();
                break;
            case 'v':
                viewPoem();
                break;
            case 'd':
                deletePoem();
                break;
            case 'm':
                modifyPoem();
                break;
            case 's':  // New case to start sending a bunny boy
                performWateringRitual(pipe_fd, msg_queue_id);
                break;
            default:
                printf("Invalid choice\n");
        }
    }

    // Cleanup
    for (int i = 0; i < NUM_BOYS; i++) {
        wait(NULL);
    }
    msgctl(msg_queue_id, IPC_RMID, NULL);

    return 0;
}

char menu() {
    char choice;
    printf("\nMama Bunny's Poem Manager and Watering Ritual Menu\n");
    printf("a. Add a new poem\n");
    printf("l. List all poems\n");
    printf("v. View a poem\n");
    printf("d. Delete a poem\n");
    printf("m. Modify a poem\n");
    printf("s. Start the Easter Watering Ritual\n");
    printf("q. Quit\n");
    printf("Enter your choice: ");
    scanf(" %c", &choice);
    return choice;
}

int getPoemCount() {
    FILE *file = fopen(DATA_FILE, "r");
    if (file == NULL) {
        return 0;
    }

    int count = 0;
    char line[MAX_POEM_LENGTH];

    while (fgets(line, MAX_POEM_LENGTH, file) != NULL) {
        if (strstr(line, "---------------------------------------------------------") != NULL) {
            count++;
        }
    }
    fclose(file);

    // Each poem is followed by a separator line, so the count of separators equals the count of poems.
    return count;
}

void addPoem() {
    char headline[MAX_POEM_LENGTH];
    char poemLine[MAX_POEM_LENGTH];
    int poemCount = getPoemCount() + 1;

    FILE *file = fopen(DATA_FILE, "a");
    if (file == NULL) {
        perror("Failed to open file");
        return;
    }

    printf("Enter the poem headline: ");
    scanf(" %[^\n]s", headline);
    fprintf(file, "\n%d\n%s\n\n", poemCount, headline);

    printf("Enter the poem, line by line. Enter 'END' on a new line to finish.\n");
    while (1) {
        scanf(" %[^\n]s", poemLine);
        if (strcmp(poemLine, "END") == 0) {
            break;
        }
        fprintf(file, "%s\n", poemLine);
    }
    fprintf(file, "---------------------------------------------------------\n");

    fclose(file);
    printf("Poem added successfully.\n");
}

void listPoems() {
    FILE *file = fopen(DATA_FILE, "r");
    if (file == NULL) {
        perror("Failed to open file");
        return;
    }

    char line[MAX_POEM_LENGTH];
    int poemCount = 0;
    printf("\nList of Poems:\n");
    while (fgets(line, sizeof(line), file) != NULL) {
        if (isdigit(line[0])) {
            poemCount = atoi(line);
            printf("%d ", poemCount);
            fgets(line, sizeof(line), file); // This gets the headline
            printf("%s", line);
        }
    }
    fclose(file);
}

void deletePoem() {
    int targetNo, currentNo = 0;
    printf("Enter the poem number to delete: ");
    scanf("%d", &targetNo);

    FILE *file = fopen(DATA_FILE, "r");
    FILE *tempFile = fopen(TEMP_FILE, "w");
    if (file == NULL || tempFile == NULL) {
        perror("Failed to open file");
        return;
    }

    char line[MAX_POEM_LENGTH];
    int deletePoem = 0;
    while (fgets(line, sizeof(line), file) != NULL) {
        if (isdigit(line[0])) {
            currentNo = atoi(line);
            if (currentNo == targetNo) {
                deletePoem = 1; // Flag to start skipping lines
                continue;
            } else if (deletePoem) {
                // We've reached the next poem, so stop deleting
                deletePoem = 0;
            }
        }

        if (!deletePoem) {
            fprintf(tempFile, "%s", line);
        } else if (strstr(line, "---------------------------------------------------------") != NULL) {
            // Skipping writing the divider line for the poem being deleted
            deletePoem = 0; // Reset deletion flag after skipping the targeted poem
        }
    }

    fclose(file);
    fclose(tempFile);

    remove(DATA_FILE);
    rename(TEMP_FILE, DATA_FILE);

    printf("Poem deleted successfully.\n");
}

void modifyPoem() {
    int poemNumber;
    printf("Enter the poem number to modify: ");
    scanf("%d", &poemNumber);

    // Extracting the poem to be modified to a temporary file
    extractPoemToFile(poemNumber, TEMP_FILE);

    char command[256];
    sprintf(command, "nano %s", TEMP_FILE); // Using nano for editing
    system(command); // Opening the temp file in the editor for modification

    // Updating the original poems.txt with the modified poem from the temp file
    updatePoemInFile(poemNumber, TEMP_FILE);

    // Removing the temporary file after updating the original file
    remove(TEMP_FILE);

    printf("Poem %d modified successfully.\n", poemNumber);
}

void viewPoem() {
    int targetNo, currentNo = 0;
    printf("Enter the poem number to view: ");
    scanf("%d", &targetNo);

    FILE *file = fopen(DATA_FILE, "r");
    if (file == NULL) {
        perror("Failed to open file");
        return;
    }

    char line[MAX_POEM_LENGTH];
    int foundPoem = 0;
    while (fgets(line, sizeof(line), file) != NULL) {
        if (isdigit(line[0])) {
            currentNo = atoi(line);
            if (currentNo == targetNo) {
                foundPoem = 1;
                printf("\n%d\n", currentNo); // Print poem number
                continue;
            }
        }

        if (foundPoem) {
            if (strstr(line, "---------------------------------------------------------") != NULL) {
                break; // End of the poem
            }
            printf("%s", line); // Print the poem line
        }
    }

    if (!foundPoem) {
        printf("Poem number %d not found.\n", targetNo);
    }

    fclose(file);
}

void extractPoemToFile(int poemNumber, const char* filePath) {
    FILE *sourceFile = fopen(DATA_FILE, "r");
    FILE *tempFile = fopen(filePath, "w");
    if (sourceFile == NULL || tempFile == NULL) {
        perror("Error opening file");
        exit(1);
    }

    char line[MAX_POEM_LENGTH];
    int currentPoem = 0;
    int foundPoem = 0;
    while (fgets(line, MAX_POEM_LENGTH, sourceFile) != NULL) {
        if (isdigit(line[0])) {
            currentPoem = atoi(line);
        }
        if (currentPoem == poemNumber) {
            foundPoem = 1;
            fprintf(tempFile, "%s", line);
            while (fgets(line, MAX_POEM_LENGTH, sourceFile) != NULL && !strstr(line, "---------------------------------------------------------")) {
                fprintf(tempFile, "%s", line); // Write the poem lines
            }
            break;
        }
    }
    if (!foundPoem) {
        printf("Poem number %d not found.\n", poemNumber);
    }

    fclose(sourceFile);
    fclose(tempFile);
}

void updatePoemInFile(int poemNumber, const char* filePath) {
    FILE *tempFile = fopen(filePath, "r");
    FILE *sourceFile = fopen(DATA_FILE, "r");
    FILE *newFile = fopen("new_poems.txt", "w");

    if (tempFile == NULL || sourceFile == NULL || newFile == NULL) {
        perror("Error opening file");
        exit(1);
    }

    char line[MAX_POEM_LENGTH];
    int currentPoem = 0;
    int foundPoem = 0;
    while (fgets(line, MAX_POEM_LENGTH, sourceFile) != NULL) {
        if (isdigit(line[0])) {
            currentPoem = atoi(line);
            if (currentPoem == poemNumber) {
                foundPoem = 1;
                // Writing the new poem content from the temp file
                while (fgets(line, MAX_POEM_LENGTH, tempFile) != NULL) {
                    fprintf(newFile, "%s", line);
                }
                // Skipping the old poem content
                while (fgets(line, MAX_POEM_LENGTH, sourceFile) != NULL && !strstr(line, "---------------------------------------------------------"));
                fprintf(newFile, "---------------------------------------------------------\n");
                continue;
            }
        }
        // Copying line from the original file to the new file
        fprintf(newFile, "%s", line);
    }

    fclose(tempFile);
    fclose(sourceFile);
    fclose(newFile);

    if (foundPoem) {
        //remove(DATA_FILE); // Remove the old file
        rename("new_poems.txt", DATA_FILE); // Rename new file to original file name
    } else {
        remove("new_poems.txt"); // Clean up, poem wasn't found, no need to replace
        printf("Poem number %d not found. No changes made.\n", poemNumber);
    }
}

void sendPoemsToBoy(int pipe_fd[2], char* poem1, char* poem2) {
    int bytes_written;

    bytes_written = write(pipe_fd[1], poem1, strlen(poem1) + 1);
    if (bytes_written == strlen(poem1) + 1) {
        printf("Mama Bunny sent the first poem to the Bunny Boy.\n");
        // printf("%s\n", poem1);
    } else {
        printf("Failed to send first poem entirely. Only %d bytes written.\n", bytes_written);
    }

    bytes_written = write(pipe_fd[1], poem2, strlen(poem2) + 1);
    if (bytes_written == strlen(poem2) + 1) {
        printf("Mama Bunny sent the second poem to the Bunny Boy.\n");
        // printf("%s\n", poem2);
    } else {
        printf("Failed to send second poem entirely. Only %d bytes written.\n", bytes_written);
    }

    close(pipe_fd[1]);  // Close the write end
}

void receivePoemFromBoy(int msg_queue_id) {
    message msg;

    // Receiving message
    msgrcv(msg_queue_id, &msg, sizeof(msg.poem), 0, 0);  // 0 as msg_type to receive any type of message
    printf("Mama Bunny received the message.\n");
    
    printf("Bunny Boy recites the poem in Friend Tree:\n%s\nMay I water!\n", msg.poem);
    printf("Bunny Boy waters the girls!\n");
    printf("Bunny Boy returns to Mama Bunny.\n");
    printf("Mama Bunny finishes the Easter Watering Ritual.\n");
}

void sendPoemToMama(int msg_queue_id, char* poem) {
    
    // Preparing the message
    message msg = {1, ""};
    char *chosenPoem = poem;
    printf("Copying poem to message...\n");
    strncpy(msg.poem, chosenPoem, MSG_SIZE - 1);  // Copy the poem into the message structure
    // printf("Message poem: %s\n", msg.poem);
    msg.poem[MSG_SIZE - 1] = '\0'; // Ensure null-termination

    clearMessageQueue(msg_queue_id);  // Clear any existing messages in the queue
    printf("Message queue cleared.\n");
    
    printf("Sending message...\n");

    // Sending the chosen poem back to Mama Bunny
    if (msgsnd(msg_queue_id, &msg, strlen(msg.poem) + 1, IPC_NOWAIT) == -1) {
        perror("Bunny Boy failed to send message");
    } else {
        printf("Message sent successfully from Bunny Boy.\n");
    }
}

void performWateringRitual(int pipe_fd[2], int msg_queue_id) {
    
    printf("Mama Bunny starts the Easter Watering Ritual...\n");
    int numPoems = getPoemCount();
    srand(time(NULL));  // Seed random number generator

    if (numPoems < 2) {
        printf("Not enough poems available.\n");
        return;
    } else {
        printf("Mama Bunny has found %d poems for the Easter Watering Ritual.\n", numPoems);
    }

    // Select two random poem indices
    int poemIndex1 = rand() % numPoems;
    int poemIndex2 = rand() % numPoems;
    while (poemIndex2 == poemIndex1) {  // Ensure two distinct poems
        poemIndex2 = rand() % numPoems;
    }

    FILE *file = fopen(DATA_FILE, "r");
    if (!file) {
        perror("Failed to open poems file");
        return;
    } else {
        printf("Mama Bunny Selected poems %d and %d.\n", (poemIndex1 + 1), (poemIndex2 + 1));
    }

    char poem1[MSG_SIZE] = {0}, poem2[MSG_SIZE] = {0};
    char line[MAX_POEM_LENGTH];
    int currentPoemIndex = 0;
    int readPoemCount = 0;
    char *currentPoem = NULL;

    // Read and accumulate poems from the file
    while (fgets(line, sizeof(line), file) && readPoemCount < 2) {
        if (strstr(line, "---------------------------------------------------------") != NULL) {
            currentPoemIndex++;
            continue;
        }

        if (currentPoemIndex == poemIndex1 || currentPoemIndex == poemIndex2) {
            if (currentPoemIndex == poemIndex1) {
                currentPoem = poem1;
            } else if (currentPoemIndex == poemIndex2) {
                currentPoem = poem2;
            }

            // printf("Reading poem %d...\n", currentPoemIndex);
            // printf("%s", line);  // Print the headline
            // printf("%s", currentPoem);  // Print the poem lines

            if (strlen(currentPoem) + strlen(line) < MSG_SIZE) {
                strcat(currentPoem, line);  // Accumulate poem lines
            }

            if (strstr(line, "---------------------------------------------------------")) {
                readPoemCount++;
            }
        }
    }
    fclose(file);

    pid_t pid = fork();

    if (pid == 0) {  // Child process
        close(pipe_fd[1]);  // Close the write end in the child
        childProcess(pipe_fd, msg_queue_id);
        exit(0);
        printf("Child process finished.\n");
    } else if (pid > 0) {  // Parent process
        
        sendPoemsToBoy(pipe_fd, poem1, poem2);

        wait(NULL);  // Wait for child process to finish

        printf("Mama Bunny waiting to receive the message...\n");
        receivePoemFromBoy(msg_queue_id);

    } else {
        perror("Failed to fork");
    }
}

void childProcess(int pipe_fd[2], int msg_queue_id) {
    close(pipe_fd[1]);  // Close the write end of the pipe
    char buffer[MSG_SIZE * 2] = {0};  // Assuming MSG_SIZE is enough for one poem, buffer is for two
    int bytes_read = read(pipe_fd[0], buffer, sizeof(buffer) - 1);

    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';  // Ensure buffer is null-terminated

        char *currentPoem = buffer;
        int poemIndex = 0;

        // Iterate through buffer to extract and print each poem separately
        while (currentPoem < buffer + bytes_read) {
            if (poemIndex == 0) {
                printf("Bunny Boy received first poem:\n%s\n", currentPoem);
            } else if (poemIndex == 1) {
                printf("Bunny Boy received second poem:\n%s\n", currentPoem);
            }
            currentPoem += strlen(currentPoem) + 1;  // Move to the start of the next poem
            poemIndex++;
        }

        // Reset pointer to start of buffer for selection
        currentPoem = buffer;
        srand(time(NULL));  // Seed the random number generator
        int chosenIndex = rand() % poemIndex;
        while (chosenIndex > 0) {
            currentPoem += strlen(currentPoem) + 1;  // Move to the start of the next poem
            chosenIndex--;
        }

        printf("Bunny Boy's chosen poem to recite:\n%s\n", currentPoem);
        sendPoemToMama(msg_queue_id, currentPoem);

    } else {
        printf("Bunny Boy failed to read any poems.\n");
    }

    close(pipe_fd[0]);  // Close the read end
}

void clearMessageQueue(int msg_queue_id) {
    message buf;  // Assuming 'message' is your message structure
    while (msgrcv(msg_queue_id, &buf, sizeof(buf.poem), 0, IPC_NOWAIT) != -1) {
        // Optionally process or log the discarded message
    }
    if (errno != ENOMSG) { // ENOMSG means no message was left which is expected
        perror("Error clearing message queue");
    }
}
