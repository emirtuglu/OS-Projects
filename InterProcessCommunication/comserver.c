#include <stdlib.h>
#include <mqueue.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include  <signal.h>

#include "mqItem.h"

char *strstrip(char *s);
void sendOutputToClient(int sc);
void sendMessage(MessageType type, char *data, size_t wsize, int sc);
void SIGQUIT_handler(int sig);

char *bufferp; 
int bufferlen; 
struct item *itemp;

int MAXCLIENTS = 12;
char* OUTPUTFILE = "output.txt";

int main(int argc, char *argv[])
{
    if (signal(SIGQUIT, SIGQUIT_handler) == SIG_ERR) {
        printf("SIGQUIT install error\n");
        exit(1);
    }

    if (argc != 2) {
        printf("Usage: comserver MQNAME\n");
    }

    mqd_t mq;
    struct mq_attr mq_attr;
    int n;

    int childrenArray [MAXCLIENTS + 1];
    int clientArray [MAXCLIENTS];
    memset(childrenArray, 0, MAXCLIENTS);
    memset(clientArray, 0, MAXCLIENTS);
    childrenArray[0] = getpid();
    int childrenArrayIndex = 1;
    int clientArrayIndex = 0;

    mq = mq_open(argv[1], O_RDWR | O_CREAT, 0666, NULL);
    if (mq == -1)
    {
        perror("can not create msg queue\n");
        exit(1);
    }
    printf("mq created, mq id = %d\n\n", (int)mq);

    mq_getattr(mq, &mq_attr);

    // allocate large enough space for the buffer to store an incoming message
    bufferlen = mq_attr.mq_msgsize;
    bufferp = (char *)malloc(bufferlen);

    while (1)
    {
        n = mq_receive(mq, bufferp, bufferlen, NULL);
        if (n == -1)
        {
            perror("mq_receive failed\n");
            exit(1);
        }

        itemp = (struct item *)bufferp;

        printf("server main: CONREQUEST message received: pid=%d, cs=%s, sc=%s, wsize=%d\n\n", itemp->pid, itemp->cs, itemp->sc, itemp->wsize);
        clientArray[clientArrayIndex] = itemp->pid;
        clientArrayIndex++;

        int pid = fork();
        childrenArray[childrenArrayIndex] = pid;
        childrenArrayIndex++;
        if (pid < 0)
        {
            printf("error\n");
        }
        else if (pid == 0)
        {
            // Take pipe names from the mq message coming from the client
            int sc, cs;
            char myfifo1[99];
            strcpy(myfifo1, itemp->sc);
            char myfifo2[99];
            strcpy(myfifo2, itemp->cs);
            char arr1[itemp->wsize], arr2[itemp->wsize];
            strcpy(arr2, "Success. Connection has been established");
            Message *receivedMessage = (Message *)malloc(sizeof(Message));
            sleep(1);
            // Send the success message to the client through the sc pipe
            sc = open(myfifo1, O_WRONLY);

            sendMessage(COMRESULT, arr2, itemp->wsize, sc);

            while (1)
            {
                // Read the command coming from the client
                cs = open(myfifo2, O_RDONLY);
                char lengthString[99];
                char typeString[99];

                char dataString[500];
                memset(dataString, 0, sizeof(dataString));
                do
                {
                    read(cs, arr1, itemp->wsize);
                    memcpy(lengthString, arr1, 4 * sizeof(char));
                    memcpy(typeString, arr1 + 4, 1 * sizeof(char));
                    receivedMessage->length = atoi(lengthString);
                    receivedMessage->type = atoi(typeString);
                    memcpy(receivedMessage->padding, arr1 + 5, 3 * sizeof(char));
                    receivedMessage->data = (char *)malloc(sizeof(char) * (receivedMessage->length - 8));
                    memcpy(receivedMessage->data, arr1 + 8, (receivedMessage->length - 8) * sizeof(char));

                    memcpy(dataString + strlen(dataString), receivedMessage->data, strlen(receivedMessage->data));
                } while (receivedMessage->length == itemp->wsize);

                sprintf(dataString, dataString, strlen(dataString));
                strcpy(dataString, strstrip(dataString));

                // If the incoming command is quit, send a quit-ack response
                if (receivedMessage->type == QUITREQ)
                {
                    printf("server child: QUITREQ message received: len=%d, type=%d, data=%s\n", strlen(dataString) + 8, receivedMessage->type, dataString);
                    sendMessage(QUITREPLY, "quit-ack", itemp->wsize, sc);
                    exit(0);
                }

                if (receivedMessage->type == QUITALL) {
                    printf("server child: QUITALL message received: len=%d, type=%d, data=%s\n", strlen(dataString) + 8, receivedMessage->type, dataString);
                    free(bufferp);
                    mq_close(mq);
                    // Kill clients
                    for (int i = 0; i < clientArrayIndex; i++) {
                        if (clientArray[i] != 0) {
                            kill(clientArray[i], SIGQUIT);
                        }
                    }
                    // Kill server childs except this child which is busy killing others
                    for (int i = 0; i < childrenArrayIndex; i++) {
                        if (getpid() != childrenArray[i] && 0 != childrenArray[i]) {
                            kill(childrenArray[i], SIGQUIT);
                        }
                    }
                    exit(0);
                }
                printf("server child: COMLINE message received: len=%d, type=%d, data=%s\n", strlen(dataString) + 8, receivedMessage->type, dataString);

                // Open up the output file
                int file_fd = open(OUTPUTFILE, O_WRONLY | O_APPEND | O_CREAT | O_TRUNC, 0666);
                if (file_fd == -1)
                {
                    printf("open error");
                    exit(1);
                }

                // Check whether incoming message containts 1 command or 2 commands
                char *pipe_position = strchr(dataString, '|');
                if (pipe_position != NULL) // 2 commands case
                {                          
                    *pipe_position = '\0'; // Replace '|' with null terminator to split the string

                    char *first_command = dataString;
                    char *second_command = pipe_position + 1;
                    first_command = strstrip(first_command);
                    second_command = strstrip(second_command);

                    printf("First command: %s\n", first_command);
                    printf("Second command: %s\n", second_command);

                    // Create an unnamed pipe
                    int pipe_fd[2];
                    if (pipe(pipe_fd) == -1)
                    {
                        perror("pipe");
                        exit(EXIT_FAILURE);
                    }

                    // Fork first runner child process
                    int first_runner_pid = fork();
                    if (first_runner_pid == -1)
                    {
                        perror("fork");
                        exit(EXIT_FAILURE);
                    }
                    else if (first_runner_pid == 0)
                    {
                        // Close read end of the pipe
                        close(pipe_fd[0]);

                        // Redirect stdout to the write end of the pipe
                        if (dup2(pipe_fd[1], STDOUT_FILENO) == -1)
                        {
                            perror("dup2");
                            exit(EXIT_FAILURE);
                        }

                        // Close unused write end of the pipe
                        close(pipe_fd[1]);

                        // Execute the first command
                        char *command = strtok(first_command, " ");
                        char *arguments = strtok(NULL, "");

                        execlp(command, command, arguments, NULL);

                        perror("execlp");
                        exit(EXIT_FAILURE);
                    }
                    else
                    {
                        // Fork second runner child process
                        int second_runner_pid = fork();
                        if (second_runner_pid == -1)
                        {
                            perror("fork");
                            exit(EXIT_FAILURE);
                        }
                        else if (second_runner_pid == 0)
                        {
                            // Close write end of the pipe
                            close(pipe_fd[1]);

                            // Redirect stdin to the read end of the pipe
                            if (dup2(pipe_fd[0], STDIN_FILENO) == -1)
                            {
                                perror("dup2");
                                exit(EXIT_FAILURE);
                            }

                            // Close unused read end of the pipe
                            close(pipe_fd[0]);

                            // Redirect stdout to the output file
                            if (dup2(file_fd, STDOUT_FILENO) == -1)
                            {
                                perror("dup2");
                                exit(EXIT_FAILURE);
                            }

                            // Close the file descriptor
                            close(file_fd);

                            // Execute the second command
                            char *command = strtok(second_command, " ");
                            char *arguments = strtok(NULL, "");

                            execlp(command, command, arguments, NULL);
                            perror("execlp");
                            exit(EXIT_FAILURE);
                        }
                        else
                        {
                            // Close both ends of the pipe in the parent process
                            close(pipe_fd[0]);
                            close(pipe_fd[1]);

                            // Wait for both runner child processes to finish
                            waitpid(first_runner_pid, NULL, 0);
                            waitpid(second_runner_pid, NULL, 0);
                            printf("command execution finished\n\n");
                            sendOutputToClient(sc);
                        }
                    }
                }
                else // 1 command case
                { 
                    // Fork a child process
                    int runnerChildPid = fork();

                    if (runnerChildPid < 0)
                    {
                        printf("runner child fork error");
                        exit(1);
                    }
                    else if (runnerChildPid == 0)
                    {
                        // Redirect stdout to the file
                        if (dup2(file_fd, STDOUT_FILENO) == -1)
                        {
                            printf("dup2 error");
                            exit(1);
                        }
                        close(file_fd);

                        // Parse command and arguments
                        char *command = strtok(dataString, " ");
                        char *arguments = strtok(NULL, "");

                        execlp(command, command, arguments, NULL);

                        // If execlp fails, this part will be reached
                        perror("execlp");
                        printf("Command couldn't be executed.\n");
                        exit(1);
                    }
                    else
                    {
                        // Parent process
                        // Wait for the child to finish
                        wait(NULL);
                        printf("command execution finished\n\n");
                        sendOutputToClient(sc);

                    }
                }
            }
        }
    }
    free(bufferp);
    mq_close(mq);
    return 0;
}

void sendOutputToClient(int sc) {
    // Open output file for reading
    int file_fd = open(OUTPUTFILE, O_RDONLY);
    if (file_fd == -1)
    {
        printf("open error");
        exit(1);
    }

    // Determine the size of output file
    off_t file_size = lseek(file_fd, 0, SEEK_END);
    if (file_size == -1)
    {
        printf("lseek error");
        exit(1);
    }

    // Allocate memory for the buffer based on file size
    char *output_buffer = (char *)malloc(file_size + 1); // +1 for null terminator
    if (output_buffer == NULL)
    {
        printf("malloc error");
        exit(1);
    }

    // Reset file position to the beginning
    if (lseek(file_fd, 0, SEEK_SET) == -1)
    {
        printf("lseek error");
        exit(1);
    }

    // Read the content of output file into the buffer
    ssize_t read_bytes = read(file_fd, output_buffer, file_size);
    if (read_bytes == -1)
    {
        printf("read error");
        exit(1);
    }

    // Null-terminate the buffer to make it a valid C string
    output_buffer[read_bytes] = '\0';

    // Delete and close the file descriptor
    remove(OUTPUTFILE);
    close(file_fd);

    sendMessage(4, output_buffer, itemp->wsize, sc);

    // Free the dynamically allocated memory
    free(output_buffer);
}

void sendMessage(MessageType type, char *data, size_t wsize, int sc) {
    Message* msg = (Message*)malloc(sizeof(Message));
    msg->type = type;
    memset(msg->padding, 0, sizeof(msg->padding)); // Clear padding

    // Dynamically allocate memory for data
    msg->data = (char* )malloc((wsize - 8) * sizeof(char)); // +1 for null terminator, if needed
    memcpy(msg->data, data, wsize - 8);

    // Check if data is longer than wsize. If so, split the message.
    if (strlen(data) > wsize - 8) {
        msg->length = wsize;
        memcpy(msg->data, data, wsize - 8); // -9 since we need 1 space for null terminator
        msg->data[strlen(data) - 1] = '\0'; // putting null terminator at the end of the

        char* message = (char* )malloc((wsize) * sizeof(char));
        char lengthString[4];
        char typeString[1];
        char paddingString[3];
        sprintf(lengthString, "%u", msg->length);
        sprintf(typeString, "%u", msg->type);
        sprintf(paddingString, "%s", msg->padding);

        memcpy(message, lengthString, 4);
        memcpy(message + 4, typeString, 1);
        memcpy(message + 5, paddingString, 3);
        memcpy(message + 8, msg->data, strlen(msg->data));

        write(sc, message, wsize );
        data = data + sizeof(char) * (wsize - 8);
        sendMessage(type, data, wsize, sc);
    }
    else {
        msg->length = 8 + strlen(msg->data);
        msg->data[strlen(data)] = '\0';
        char message[msg->length];
        char lengthString[4];
        char typeString[1];
        char paddingString[3];
        sprintf(lengthString, "%u", msg->length);
        sprintf(typeString, "%u", msg->type);
        sprintf(paddingString, "%s", msg->padding);

        memcpy(message, lengthString, 4);
        memcpy(message + 4, typeString, 1);
        memcpy(message + 5, paddingString, 3);
        memcpy(message + 8, msg->data, strlen(msg->data));
        write(sc, message, msg->length);
    }
}

char *strstrip(char *s)
{
    size_t size;
    char *end;

    size = strlen(s);

    if (!size)
        return s;

    end = s + size - 1;
    while (end >= s && isspace(*end))
        end--;
    *(end + 1) = '\0';

    while (*s && isspace(*s))
        s++;

    return s;
}

void  SIGQUIT_handler(int sig)
{
    signal(sig, SIG_IGN); 
    exit(1);
}