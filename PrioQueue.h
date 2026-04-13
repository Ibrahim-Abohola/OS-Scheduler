#ifndef PRIOQUEUE_H
#define PRIOQUEUE_H
#include "DataStructures.h"

typedef struct prioQueue{
    PCB* arr;
    int size;
    int capacity;
}prioQueue;

prioQueue* create_pq(int capacity);
void insert(prioQueue* pq, PCB item);
PCB extractMax(prioQueue* pq);
int peekPrio(prioQueue*pq);
#endif