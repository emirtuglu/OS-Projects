#include <stdio.h>
#include "tsl.h"
#include "tcb.h"

// Forward declarations of functions you've defined in other files
int enqueueReady(TCB* thread);
TCB* dequeueReady();

// Test function declarations
void testQueueOperations();
void testInitialization();

int main() {
    // Run the tests
    testInitialization();
    testQueueOperations();

    return 0;
}

void testInitialization() {
    printf("Testing Initialization...\n");
    int initResult = tsl_init(ALG_FCFS); // Example test for FCFS algorithm
    if (initResult == TSL_SUCCESS) {
        printf("Initialization successful.\n");
    } else {
        printf("Initialization failed with error code: %d\n", initResult);
    }
}

void testQueueOperations() {
    printf("Testing Queue Operations...\n");

    // Initialize queues if not already done in tsl_init
    initializeQueue();

    // Create a few test TCBs
    TCB testTCBs[5];
    for (int i = 0; i < 5; i++) {
        testTCBs[i].tid = i + 1; // Assign unique tid
        testTCBs[i].state = READY; // Set state to READY for testing
        int enqueueResult = enqueueReady(&testTCBs[i]);
        if (enqueueResult == TSL_SUCCESS) {
            printf("Successfully enqueued TCB with tid: %d\n", testTCBs[i].tid);
        } else {
            printf("Failed to enqueue TCB with tid: %d\n", testTCBs[i].tid);
        }
    }

    // Now dequeue and check if the TCBs come out in the expected order
    for (int i = 0; i < 5; i++) {
        TCB* dequeued = dequeueReady();
        if (dequeued != NULL && dequeued->tid == i + 1) {
            printf("Successfully dequeued TCB with tid: %d\n", dequeued->tid);
        } else {
            printf("Dequeue test failed or dequeued wrong TCB.\n");
        }
    }
}