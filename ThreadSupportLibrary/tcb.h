#ifndef TCB_H
#define TCB_H
#include <ucontext.h>

// define state variables
#define RUNNING  0
#define READY    1
#define ENDED 11 // so that it won't be mistakenly choosen by misclick on 0,1

// a struct to represent Thread Control Blocks
typedef struct TCB {
    int tid; // thread identifier
    unsigned int state; // thread state
    ucontext_t context; // pointer to context structure
    char *stack; // pointer to stack
    int waitingTid; // thread id that is waiting FOR THIS THREAD. -1 if no thread is waiting.
} TCB; // this can be used as a default constructor with following the way: "TCB myTCB = TCB_init"

// a function to deallocate pointers inside the TCB instance and the TCB pointer itself
// is called as: 
//      TCB* tcb = ...
//          ...
//      killTCB( &tcb );
void killTCB( TCB* tcb );
int generateID();
#endif // TCB_H