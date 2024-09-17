#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <assert.h>

#ifndef __USE_GNU
#define __USE_GNU
#endif /* __USE_GNU */

#include <ucontext.h>
#include "tsl.h"
#include "tcb.h"
#include "queues.c"
#include "queues.h"

int initialized = 0;              // To check if the library has been initialized
int selectedAlgorithm = ALG_FCFS; // Stores the selected algorithm, default is FCFS
int numThreads = 0;               // Number of active threads
TCB *runningTCB = NULL;           // for storing the currently running TCB

// helper functions
void stub(void (*tsf)(void *), void *targ);
int exitAnotherTCB(TCB *tcb);
TCB *getTCBbyId(int tid);
void wakeupWaitingThreads(int tid);

int tsl_init(int salg)
{
    if (initialized)
    {
        // Library already initialized, return error
        printf("Library already initialized\n");
        return TSL_ERROR;
    }

    // Validate the selected scheduling algorithm
    if (salg != ALG_FCFS && salg != ALG_RANDOM)
    {
        // Invalid scheduling algorithm selected
        printf("Invalid scheduling algorithm selected\n");
        return TSL_ERROR;
    }
    selectedAlgorithm = salg;

    initializeQueue(); // Initialize queues for thread management
    srand(time(NULL)); // Seed the random number generator.

    // Create TCB for main thread
    TCB *mainThreadTCB = (TCB *)malloc(sizeof(TCB));
    if (mainThreadTCB == NULL)
    {
        // Memory allocation for the main thread's TCB failed
        return TSL_ERROR;
    }

    // Initialize the context for the main thread. No need to allocate a new stack.
    if (getcontext(&mainThreadTCB->context) == -1)
    {
        // Failed to get the current context
        free(mainThreadTCB);
        return TSL_ERROR;
    }

    // Update the main thread's TCB attributes
    mainThreadTCB->tid = TID_MAIN; // Use the predefined constant for clarity
    mainThreadTCB->state = RUNNING;
    mainThreadTCB->stack = NULL; // Main thread uses the OS-allocated stack

    // Set the running thread to the main thread and update global variables
    runningTCB = mainThreadTCB;
    initialized = 1;
    numThreads = 1;

    return TSL_SUCCESS; // Initialization succeeded
}

int tsl_create_thread(void (*tsf)(void *), void *targ)
{
    // Check if maximum thread limit is reached
    if (numThreads >= TSL_MAXTHREADS)
    {
        printf("Maximum number of threads reached\n");
        return TSL_ERROR;
    }

    // Allocate memory for the new TCB.
    TCB *newTCB = (TCB *)malloc(sizeof(TCB));

    if (!newTCB)
    {
        return TSL_ERROR;
    }

    newTCB->tid = generateID(); // Assign a unique thread identifier (tid)
    newTCB->state = READY;      // Initialize the TCB state to READY
    newTCB->waitingTid = -1;    // No thread is waiting for this one

    // Allocate memory for the new thread's stack
    newTCB->stack = (char *)malloc(TSL_STACKSIZE);
    if (!newTCB->stack)
    {
        killTCB(newTCB);
        return TSL_ERROR;
    }

    // Initialize the context for the new thread
    if (getcontext(&newTCB->context) == -1)
    {
        killTCB(newTCB);
        return TSL_ERROR; // Failed to get context
    }

    newTCB->context.uc_mcontext.gregs[REG_EIP] = (unsigned long)stub;
    newTCB->context.uc_mcontext.gregs[REG_ESP] = (unsigned long)newTCB->stack + TSL_STACKSIZE;

    *((unsigned int *)newTCB->context.uc_mcontext.gregs[REG_ESP]) = (unsigned long)targ;
    newTCB->context.uc_mcontext.gregs[REG_ESP] -= 4;
    *((unsigned int *)newTCB->context.uc_mcontext.gregs[REG_ESP]) = (unsigned long)tsf;
    newTCB->context.uc_mcontext.gregs[REG_ESP] -= 4;

    enqueueReady(newTCB);
    numThreads++;

    return newTCB->tid;
}

int tsl_yield(int tid)
{

    TCB *currentThread = runningTCB;
    // Save the current thread's context
    printf("Saving context of thread with ID %d\n", currentThread->tid);
    getcontext(&currentThread->context);
    printf("Context saved for thread with ID %d\n", currentThread->tid);

    // If state of the thread is ready, skip yield function since this is the second execution.
    if (currentThread->state != READY)
    {
        // Enqueue previous thread to ready queue if it is not ended
        if (currentThread->state != ENDED)
        {
            currentThread->state = READY;
            enqueueReady(currentThread);
        }
        // Get the next thread
        TCB *nextThread;
        if (tid > 0)
        {
            nextThread = getReadyThread(tid); // Find the thread with the given tid
        }
        else if (tid == TSL_ANY)
        {
            nextThread = getNextTCB(selectedAlgorithm); // Select the next thread based on the scheduling algorithm
        }
        else
        {
            return TSL_ERROR; // Invalid tid
        }

        // Ensure there's a next thread to run
        if (nextThread == NULL)
        {
            return TSL_ERROR;
        }
        // Check if the next thread is waiting for another thread
        while (isWaiting(nextThread->tid))
        {
            printf("Thread with ID %d is in WAITING state, getting next thread\n", nextThread->tid);
            enqueueReady(nextThread);                   // re-schedule waiting thread
            nextThread = getNextTCB(selectedAlgorithm); // Select the next thread based on the scheduling algorithm
            // Ensure there's a next thread to run
            if (nextThread == NULL)
            {
                return TSL_ERROR;
            }
        }

        printf("Next thread selected: ID %d\n", nextThread->tid);
        runningTCB = nextThread; // Update the running thread
        printf("Switching to thread with ID %d\n", nextThread->tid);
        setcontext(&nextThread->context); // Switch to the next thread's context
    }
    else
    {
        printf("Thread with ID %d is not in RUNNING state, skipping yield\n", currentThread->tid);
    }

    runningTCB->state = RUNNING;
    printf("Returned from yield\n");
    return (runningTCB->tid);
}

int tsl_exit()
{
    // Mark the current thread as terminated
    int tid = runningTCB->tid;
    runningTCB->state = ENDED;
    wakeupWaitingThreads(tid); // Wake up any threads waiting on this one.

    // If this is the main thread or the last thread, the application should terminate
    if (runningTCB->tid == TID_MAIN || numThreads == 1)
    {
        exit(0); // Exit the entire process
    }

    // Decrement the global thread count
    numThreads--;

    // Run the next thread
    tsl_yield(TSL_ANY);

    return 0;
}

int tsl_join(int tid)
{
    if (tid <= 0 || tid == TID_MAIN)
    {
        // Protect against joining on invalid thread IDs or the main thread.
        return TSL_ERROR;
    }

    TCB *target = getTCBbyId(tid);
    if (target == NULL)
    {
        // Target thread not found.
        return TSL_ERROR;
    }

    target->waitingTid = runningTCB->tid;

    while (target->state != ENDED)
    {
        tsl_yield(target->tid);
    }

    // Target thread has already finished; proceed with cleanup.
    removeReadyThread(target->tid);
    killTCB(target);
    return tid;
}

int tsl_cancel(int tid)
{
    // find the thread with given ID
    TCB *tcbToCancel = getTCBbyId(tid);

    // ID check. If not found, return -1
    if (tcbToCancel == NULL)
    {
        printf("TSL Error: No thread been found with the given ID!\n");
        return (TSL_ERROR);
    }

    // if the target was the running thread return -1
    if (tid == runningTCB->tid)
    {
        printf("A thread cannot cancel itself. Call tsl_exit instead.\n");
        return (TSL_ERROR);
    }

    // calls the exit helper function on the thread with the given ID
    return (exitAnotherTCB(tcbToCancel));
}

int exitAnotherTCB(TCB *tcb)
{
    if (tcb == NULL)
    {
        printf("TSL Error: Attempted to exit an unvalid thread!\n");
        return TSL_ERROR;
    }

    // set the exited threads state to ENDED
    tcb->state = ENDED;
    wakeupWaitingThreads(tcb->tid); // Wake up any threads waiting on this one.

    if (runningTCB->tid == TID_MAIN || numThreads == 1)
    {
        exit(0); // Exit the entire process
    }

    // Decrement the global thread count
    numThreads--;

    // Run the next thread
    tsl_yield(TSL_ANY);

    return 0;
}

int tsl_gettid()
{
    if (runningTCB <= 0)
    {
        printf("Error, running TCB instance is unvalid!\n");
        return (TSL_ERROR);
    }
    return runningTCB->tid;
}

// This is taken from project specification file. New threads start their execution here.
void stub(void (*tsf)(void *), void *targ)
{
    // new thread will start its execution here
    // then we will call the thread start function
    tsf(targ); // calling the thread start function
    // tsf will retun to here
    // now ask for termination
    tsl_exit(); // terminate
}

// Makes the state of the threads waiting for the given thread ID to READY
void wakeupWaitingThreads(int tid)
{
    TCB *completedThread = getTCBbyId(tid);
    if (completedThread->waitingTid != -1)
    {
        TCB *waitingThread = getTCBbyId(completedThread->waitingTid);
        if (waitingThread != NULL)
        {
            waitingThread->state = READY;
            printf("Thread with ID %d is no longer in waiting state.\n", waitingThread->tid);
        }
        completedThread->waitingTid = -1; // Reset the waiting thread ID
    }
}

// Returns the TCB with the specified tid from the ready queue or waiting queue.
TCB *getTCBbyId(int tid)
{

    if (tid == runningTCB->tid)
    {
        return (runningTCB);
    }

    // find the thread with given ID
    return getReadyThread(tid);
}