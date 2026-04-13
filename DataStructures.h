#ifndef DATASTRUCTURES_H
#define DATASTRUCTURES_H

#include "headers.h"


/* =========================== Process Control Block =============================*/
typedef struct 
{
    int id; //process id from file
    pid_t pid; //process id in the system
    int arrivalTime;
    int runTime;
    int priority;
    int waitingTime;
    int remainingTime;
    int startTime;
    int finishTime;
    int lastRun; //the last time the process was running, used to calculate waiting time
    int cpuid; // the cpu that the process is running on, used for performance calculations
    char state; //R, W, S, F
} PCB;
/* ============================================================================== */
/* ==================== FCFS Queue for one and two CPUs ========================= */
typedef struct QNode
{
    PCB  * pcb;
    struct QNode* next;
} QNode;

typedef struct Queue
{
    struct QNode* head;
    struct QNode* tail;
    int size;
} Queue;

void initQueue(Queue* q);
void enqueue(Queue* q, PCB* pcb);
PCB * dequeue(Queue* q);
void freeQueue(Queue* q);
// steal the last node from q and return the pcb
PCB* steal(Queue* q);
// sum of the remaining time of all processes in the queue
int totalRemainingTime(Queue* q);

#endif

/* ======================================================================= */