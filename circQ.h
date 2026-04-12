#ifndef CIRCQ_H
#define CIRCQ_H
#include "DataStructures.c"

typedef struct circQ {
    PCB *buffer;
    int  head;
    int  tail;
    int  size;
    int  capacity;
} CircQ;

CircQ *initCircQ (int capacity);
void   enqueue   (CircQ *q, PCB item);
PCB    dequeue   (CircQ *q);
PCB    peekFront (CircQ *q);
int    isEmpty   (CircQ *q);
int    isFull    (CircQ *q);

#endif