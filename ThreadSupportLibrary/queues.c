#include "queues.h"
#include "time.h"

TCB **readyQueue;

int readyFront;
int readyRear;

TCB** getQueue()
{
    return readyQueue;
}

void printQ(){
    if (readyFront == -1){
        printf("-------------------------------\nStarting to print the queue\n-------------------------------\n");
        printf("-------------------------------\nThe queue is empty!\n-------------------------------\n");
        printf("-------------------------------\nFinished printing the queue\n-------------------------------\n");
        return; // queue is empty
    }
    int i = readyFront;
    printf("-------------------------------\nStarting to print the queue\n-------------------------------\n");
    // do
    TCB *tcb= readyQueue[i];
    printf("ID: %ld\n", (long)tcb->tid);
    i = ((i + 1) % TSL_MAXTHREADS);
    // while
    for (; i != (readyRear + 1) % TSL_MAXTHREADS ; i = ((i + 1) % TSL_MAXTHREADS))
    {
        TCB *tcb= readyQueue[i];
        printf("ID: %ld\n", (long)tcb->tid);
    }
    printf("-------------------------------\nFinished printing the queue\n-------------------------------\n");
}

void initializeQueue()
{
    // Initialize ready queue
    readyFront = -1;
    readyRear = -1;
    readyQueue = (TCB **)malloc(TSL_MAXTHREADS * sizeof(TCB *));
    if (readyQueue == NULL)
    {
        fprintf(stderr, "Error: Unable to allocate memory for ready queue\n");
        exit(EXIT_FAILURE);
    }
    /*
    for (int i = 0; i < TSL_MAXTHREADS; i++){
        readyQueue[i] = 0;
    }
    */
}

int enqueueReady(TCB *thread)
{
    if ((readyRear + 1) % TSL_MAXTHREADS == readyFront)
    { // Check if the queue is full
        return TSL_ERROR;
    }
    if (readyFront == -1)
    { // If the queue is empty, initialize front and rear
        readyFront = 0;
    }
    readyRear = (readyRear + 1) % TSL_MAXTHREADS; // Circular queue
    readyQueue[readyRear] = thread;
    return TSL_SUCCESS;
}

TCB *dequeueReady()
{
    if (readyFront == -1)
    { // Check if the queue is empty
        return NULL;
    }
    TCB *thread = readyQueue[readyFront];
    if (readyFront == readyRear)
    { // If the queue becomes empty after dequeue
        readyFront = -1;
        readyRear = -1;
    }
    else
    {
        readyFront = (readyFront + 1) % TSL_MAXTHREADS; // Circular queue
    }
    return thread;
}

int isReadyEmpty()
{
    return (readyFront == -1 );
}


// Returns the TCB with the specified tid from the ready queue, if no such thread exists, returns NULL
TCB *getReadyThread(int tid)
{
    if (isReadyEmpty())
    {
        //printf("Ready Queue is empty!\n");
        return NULL;
    }

    int i = readyFront;
    // do
    TCB *rq_ptr= readyQueue[i];
    // the calculation
    if (rq_ptr->tid == tid){
        return (rq_ptr);
    }
    i = ((i + 1) % TSL_MAXTHREADS);
    // while
    for (; i != (readyRear + 1) % TSL_MAXTHREADS ; i = ((i + 1) % TSL_MAXTHREADS))
    {
        rq_ptr = readyQueue[i];
        if (rq_ptr->tid == tid)
        {
            return (rq_ptr);
        }
    }

    return (NULL);
}

// Returns the next TCB from the ready queue according to the selected scheduling algorithm
TCB *getNextTCB(int selectedAlgorithm)
{
    switch (selectedAlgorithm)
    {
        case ALG_FCFS:
            return getNextTCB_FCFS();
        case ALG_RANDOM:
            return getNextTCB_Random();
        default:
            fprintf(stderr, "Unknown scheduling algorithm.\n");
            return NULL;
    }
}

TCB *getNextTCB_FCFS()
{
    if (isReadyEmpty())
    {
        return NULL; // No threads are ready to run.
    }

    // Dequeue the next thread from the ready queue.
    return dequeueReady();
}

TCB *getNextTCB_Random()
{
    if (isReadyEmpty())
    {
        return NULL; // No threads are ready to run.
    }

    int randomIndex = (rand() % (readyRear - readyFront + 1));
    int selectedIndex = (readyFront + randomIndex) % TSL_MAXTHREADS;
    TCB *selectedTCB = readyQueue[selectedIndex];

    int rearDup = readyRear, frontDup = readyFront;

    int i = selectedIndex;;
    // do
    readyQueue[i] = readyQueue[(i + 1) % TSL_MAXTHREADS];
    // while
    for (; i != (readyRear + 1) % TSL_MAXTHREADS ; i = ((i + 1) % TSL_MAXTHREADS))
    {
        readyQueue[i] = readyQueue[(i + 1) % TSL_MAXTHREADS];
    }


    // If the queue becomes empty, reset front and rear pointers.
    if (readyFront == readyRear)
    {
        readyFront = -1;
        readyRear = -1;
    }
    else{
        // Update rear pointer.
        rearDup = readyRear = (readyRear - 1 + TSL_MAXTHREADS) % TSL_MAXTHREADS;
    }
    return selectedTCB;
}

int isWaiting(int tid){

    int i = readyFront;
    // do
    TCB *rq_ptr= readyQueue[i];
    if (rq_ptr == NULL && rq_ptr->waitingTid == tid){
        return (1);
    }
    i = ((i + 1) % TSL_MAXTHREADS);

    // while
    for (; i != (readyRear + 1) % TSL_MAXTHREADS ; i = ((i + 1) % TSL_MAXTHREADS))
    {
        rq_ptr = readyQueue[i];
        if (rq_ptr->waitingTid == tid)
        {
            return (1);
        }
    }

    return (0);
}

void removeReadyThread(int tid)
{
    if (isReadyEmpty())
    {
        printf("Ready Queue is empty!\n");
        return;
    }

    int found = 0;
    for (int i = readyFront; i != readyRear; i = (i + 1) % TSL_MAXTHREADS)
    {
        TCB *thread = readyQueue[i];
        if (thread->tid == tid)
        {
            // Shift elements in the queue to fill the gap caused by removing the thread.
            for (int j = i; j != readyRear; j = (j + 1) % TSL_MAXTHREADS)
            {
                readyQueue[j] = readyQueue[(j + 1) % TSL_MAXTHREADS];
            }
            // Update rear pointer.
            readyRear = (readyRear - 1 + TSL_MAXTHREADS) % TSL_MAXTHREADS;

            // If the queue becomes empty, reset front and rear pointers.
            if (readyFront == readyRear)
            {
                readyFront = -1;
                readyRear = -1;
            }

            found = 1;
            break;
        }
    }

    if (!found)
    {
        printf("Thread with ID %d not found in the ready queue!\n", tid);
    }
}