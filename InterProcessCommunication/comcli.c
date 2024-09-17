#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <mqueue.h>
#include <errno.h>
#include <stdlib.h>
#include  <signal.h>
#include "mqItem.h"

char *strstrip(char *s);
void sendMessage(MessageType type, char *data, size_t wsize, int sc);
void SIGQUIT_handler(int sig);

struct item *itemp;
char *bufferp;
int   bufferlen;
char myfifo1 [32];
char myfifo2 [32];

int main(int argc, char** argv){
    if (signal(SIGQUIT, SIGQUIT_handler) == SIG_ERR) {
        printf("SIGQUIT install error\n");
        exit(2);
    }

    char mqname[99] ;
    char comfile [99];
    int wsize = 64;
    sprintf(myfifo1, "scpipe_%d", getpid());
    sprintf(myfifo2, "cspipe_%d", getpid());
    int bExists = 0;
    int sExists = 0;

    if (argc < 2 && argc > 6) {
        printf("Usage: comcli MQNAME [-b COMFILE] [-s WSIZE]\n");
        return 1;
    }
    else if ( argc == 2) {
        strcpy(mqname, argv[1]);
    }
    else if ( argc == 4) {
        strcpy(mqname, argv[1]);
        if (strcmp(argv[2], "-b") == 0) {
            strcpy(comfile, argv[3]);
            bExists = 1;
        }
        else if (strcmp(argv[2], "-s") == 0) {
            wsize = atoi(argv[3]);
            sExists = 1;
        }
    }
    else if ( argc == 6) {
        strcpy(mqname, argv[1]);
        bExists = 1;
        sExists = 1;
        if (strcmp(argv[4], "-b") == 0) {
            strcpy(comfile, argv[5]);
            wsize = atoi(argv[3]);
        }
        else if (strcmp(argv[4], "-s") == 0) {{
                wsize = atoi(argv[5]);
                strcpy(comfile, argv[3]);
            }}

    }
    else {
        printf("Usage: comcli MQNAME [-b COMFILE] [-s WSIZE]\n");
        return 1;
    }
    mqd_t mq;
    struct mq_attr mq_attr;
    int n;
    int i;
    mq = mq_open(mqname, O_RDWR);
    if (mq == -1) {
        perror("can not open msg queue\n");
        exit(1);
    }
    printf("mq opened, mq id = %d\n\n", (int) mq);

    mq_getattr(mq, &mq_attr);
    bufferlen = mq_attr.mq_msgsize;
    bufferp = (char *) malloc(bufferlen);

    itemp = (struct item *) bufferp;
    itemp->pid = getpid();
    strcpy(itemp->cs, myfifo2);
    strcpy(itemp->sc, myfifo1);
    itemp->wsize = wsize;

    n = mq_send(mq, bufferp, sizeof(struct item), 0);
    if (n == -1) {
        perror("mq_send failed\n");
        exit(1);
    }

    int sc, cs;
    mkfifo(myfifo1, 0666);
    mkfifo(myfifo2, 0666);

    char scBuffer[80], csBuffer[80];
    Message* receivedMessage = (Message*)malloc(sizeof(Message));
    while(1){
        // Read the output coming from the server, then print it
        sc = open(myfifo1, O_RDONLY);
        char lengthString[99];
        char typeString [99];

        char responseString[10000];
        memset(responseString, 0, sizeof(responseString));
        do {
            read(sc, scBuffer, wsize);
            memcpy(lengthString, scBuffer, 4 * sizeof(char));
            memcpy(typeString , scBuffer + 4, 1 * sizeof(char));
            receivedMessage->length = atoi(lengthString);
            receivedMessage->type = atoi(typeString);
            memcpy(receivedMessage->padding, scBuffer + 5, 3 * sizeof(char));
            receivedMessage->data = (char*) malloc(sizeof(char) * (receivedMessage->length - 8));
            memcpy(receivedMessage->data , scBuffer + 8, (receivedMessage->length - 8) * sizeof(char));

            memcpy(responseString + strlen(responseString), receivedMessage->data, strlen(receivedMessage->data) - 1);
        } while(receivedMessage->length == wsize);

        responseString[strlen(responseString)] = '\0';
        sprintf(responseString, responseString, strlen(responseString));
        strcpy(responseString, strstrip(responseString));
        printf("%s\n", responseString);

        if(receivedMessage->type == QUITREPLY){
            unlink(myfifo1);
            unlink(myfifo2);
            return 0;
        }

        // Take the next command from user via the command line, then write it to the pipe
        cs = open(myfifo2, O_WRONLY);
        if(bExists){

            FILE *file;
            file = fopen(comfile,"r");
            if (file == NULL) {
                perror("Error opening file");
                return;
            }

            while (fgets(csBuffer, wsize, file) != NULL) {
                sendMessage(COMLINE, csBuffer, wsize, cs);
                // Read the output coming from the server, then print it
                sc = open(myfifo1, O_RDONLY);
                char lengthString[99];
                char typeString [99];

                char responseString[10000];
                memset(responseString, 0, sizeof(responseString));
                do {
            read(sc, scBuffer, wsize);
            memcpy(lengthString, scBuffer, 4 * sizeof(char));
            memcpy(typeString , scBuffer + 4, 1 * sizeof(char));
            receivedMessage->length = atoi(lengthString);
            receivedMessage->type = atoi(typeString);
            memcpy(receivedMessage->padding, scBuffer + 5, 3 * sizeof(char));
            receivedMessage->data = (char*) malloc(sizeof(char) * (receivedMessage->length - 8));
            memcpy(receivedMessage->data , scBuffer + 8, (receivedMessage->length - 8) * sizeof(char));

            memcpy(responseString + strlen(responseString), receivedMessage->data, strlen(receivedMessage->data) - 1);
        } while(receivedMessage->length == wsize);

        responseString[strlen(responseString)] = '\0';
        sprintf(responseString, responseString, strlen(responseString));
        strcpy(responseString, strstrip(responseString));
        printf("%s\n", responseString);
            }
            // Send quit request message
            sendMessage(QUITREQ, "quit", wsize, cs);

            exit(0);
        }
        else{
            printf("type command: ");
            fgets(csBuffer, 80, stdin);
            // Replace \n that fgets puts at the end of the message with \0
            if (strlen(csBuffer) > 0) {
                csBuffer[strlen(csBuffer) - 1] = '\0';
            }

            if (strcmp(csBuffer, "quit") == 0) {
                sendMessage(QUITREQ, "quit", wsize, cs);
            }
            else if (strcmp(csBuffer, "quitall") == 0) {
                sendMessage(QUITALL, "quitall", wsize, cs);
            }
            else {
                sendMessage(COMLINE, csBuffer, wsize, cs);
            }
            
        }

    }
    mq_close(mq);
    return 0;
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
        msg->data[strlen(data)] = '\0'; // putting null terminator at the end of the

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
    remove(myfifo1);
    remove(myfifo2);
    close(myfifo1);
    close(myfifo2);
    signal(sig, SIG_IGN); 
    exit(1);
}