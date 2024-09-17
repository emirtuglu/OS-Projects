#include "tsl.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void simpleThreadFunction(void *arg);

int main() {
    int threadIDs[5]; // Array to hold thread IDs for simplicity
    int args[5]; // Arguments to pass to threads

    // Initialize the threading library
    if (tsl_init(ALG_FCFS) != TSL_SUCCESS) {
        printf("Failed to initialize threading library.\n");
        return 1;
    }

    // Create several threads
    for (int i = 0; i < 5; i++) {
        args[i] = i + 1; // Assign a unique argument to each thread
        threadIDs[i] = tsl_create_thread(simpleThreadFunction, &args[i]);
        if (threadIDs[i] == TSL_ERROR) {
            printf("Failed to create thread %d\n", i);
        } else {
            printf("Created thread %d with ID %d\n", i, threadIDs[i]);
        }
    }


    sleep(1); // we will replace with TSL JOIN

    return 0;
}

void simpleThreadFunction(void *arg) {
    int threadNum = *(int*)arg;
    printf("Thread %d is running\n", threadNum);
    tsl_exit(); // Ensure the thread exits properly after finishing its work
}