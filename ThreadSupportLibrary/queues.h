// queues.h
#ifndef QUEUES_H
#define QUEUES_H

#include <stdio.h>
#include <stdlib.h>
#include "tcb.h" 
#include "tsl.h"

void initializeQueue();
int enqueueReady(TCB* thread);
TCB *dequeueReady();
int isReadyEmpty();
TCB* getReadyThread( int tid );
TCB* getNextTCB(int selectedAlgorithm);
TCB *getNextTCB_FCFS();
TCB *getNextTCB_Random();
int isWaiting(int tid);
void removeReadyThread(int tid);

#endif // QUEUES_H