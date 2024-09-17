#include <stdint.h>
#include <string.h>

#define MAX_DATA_SIZE 1024

typedef enum {
    CONREQUEST = 1,
    CONREPLY,
    COMLINE,
    COMRESULT,
    QUITREQ,
    QUITREPLY,
    QUITALL
} MessageType;

typedef struct {
    uint32_t length;
    uint8_t type; 
    char padding[3];
    char *data;
} Message;

struct item {
    char astr[64];
    int pid;
    char sc[64];
    char cs[64];
    int wsize;
};

#define MQNAME "/justaname"
