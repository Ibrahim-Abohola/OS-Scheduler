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

void initQueue(Queue* q)
{
    q->head = NULL;
    q->tail = NULL;
    q->size = 0;
}

void enqueue(Queue* q, PCB* pcb)
{
    QNode* newNode = (QNode*)malloc(sizeof(QNode));
    newNode->pcb = pcb;
    newNode->next = NULL;

    if (q->tail == NULL) {
        q->head = newNode;
        q->tail = newNode;
    } else {
        q->tail->next = newNode;
        q->tail = newNode;
    }
    q->size++;
}

PCB * dequeue(Queue* q)
{
    if (q->head == NULL) {
        return NULL; 
    }
    QNode* temp = q->head;
    PCB* pcb = temp->pcb;
    q->head = q->head->next;
    if (q->head == NULL) {
        q->tail = NULL; 
    }
    free(temp);
    q->size--;
    return pcb;
}

void freeQueue(Queue* q)
{
    while (q->head != NULL) {
        dequeue(q);
    }
}
// steal the last node from q and return the pcb
PCB* steal(Queue* q)
{
    if (q->head == NULL) {
        return NULL; 
    }
    if (q->head == q->tail) {
        PCB* pcb = q->head->pcb;
        free(q->head);
        q->head = NULL;
        q->tail = NULL;
        q->size--;
        return pcb;
    }
    QNode* current = q->head;
    while (current->next != q->tail) {
        current = current->next;
    }
    PCB* pcb = q->tail->pcb;
    free(q->tail);
    current->next = NULL;
    q->tail = current;
    q->size--;
    return pcb;
}

// sum of the remaining time of all processes in the queue
int totalRemainingTime(Queue* q)
{
    int total = 0;
    QNode* current = q->head;
    while (current != NULL) {
        total += current->pcb->remainingTime;
        current = current->next;
    }
    return total;
}

/* ======================================================================= */