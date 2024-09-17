#include "tsl.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define NUM_WORKERS 3

// Worker thread function
void *workerThread(void *arg) {
    int myId = *(int *)arg;
    printf("Worker Thread %d started.\n", myId);

    // Simulate varying workloads with sleep.
    sleep(myId % 3 + 1);  // Vary sleep time based on ID to introduce variability.

    printf("Worker Thread %d completed.\n", myId);
    tsl_exit(); // Exit the thread
}

// Dependent thread function
void *dependentThread(void *arg) {
    
    printf("Dependent Thread started, waiting for worker threads.\n");

    // Wait for all worker threads to complete
    for (int i = 1; i <= NUM_WORKERS; i++) {
        tsl_join(i); // Join with each worker thread

    }

    printf("Dependent Thread completed after all worker threads.\n");
    tsl_exit(); // Exit the thread
}

int main() {
    // Initialize the thread scheduling library with First-Come, First-Served (FCFS) algorithm
    tsl_init(ALG_FCFS);

    // Create worker threads
    int workerIds[NUM_WORKERS];
    for (int i = 0; i < NUM_WORKERS; i++) {
        workerIds[i] = i + 1; // Assign IDs starting from 1
        tsl_create_thread(workerThread, &workerIds[i]);
        printf("Worker thread %d created.\n", workerIds[i]);
    }

    // Create dependent thread
    tsl_create_thread(dependentThread, NULL);
    printf("Dependent thread created.\n");

    // Wait for all threads to complete
    while (tsl_gettid() != TID_MAIN) {
        tsl_yield(TSL_ANY); // Yield to other threads
    }

    printf("All threads have completed. Main thread exiting.\n");

    // Exit the main thread
    tsl_exit(); 

    return 0;
}