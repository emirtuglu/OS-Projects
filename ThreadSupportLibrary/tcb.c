#include "tcb.h"
#include <stdlib.h>

static int count = 1; // 1 is main thread id, so we start from 2.

void killTCB(TCB *tcb)
{
    if (tcb == NULL)
    {
        return;
    }

    // Free the allocated stack space.
    if (tcb->stack != NULL)
    {
        free(tcb->stack);
        tcb->stack = NULL; // Correctly nullify the pointer after freeing
    }

    // Finally, free the TCB itself.
    free(tcb);
}

int generateID()
{
    return ++count;
}