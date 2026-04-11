#include "PrioQueue.h"

static void swap(PCB* a, PCB* b) {
    PCB temp = *a;
    *a = *b;
    *b = temp;
}

static int parent(int i) { return (i - 1) / 2; }
static int left(int i) { return 2 * i + 1; }  
static int right(int i) { return 2 * i + 2; }

static void heapify_down(prioQueue* pq, int i) {
    int max_index = i;
    int l = left(i);
    int r = right(i);

    if (l < pq->size && pq->arr[l].priority < pq->arr[max_index].priority) {
        max_index = l;
    }

    if (r < pq->size && pq->arr[r].priority < pq->arr[max_index].priority) {
        max_index = r;
    }

    if (i != max_index) {
        swap(&pq->arr[i], &pq->arr[max_index]);
        heapify_down(pq, max_index);
    }
}

prioQueue* create_pq(int capacity) {
    prioQueue* pq = (prioQueue*)malloc(sizeof(prioQueue));
    pq->capacity = capacity;
    pq->size = 0;
    pq->arr = (PCB*)malloc(capacity * sizeof(PCB));
    return pq;
}

void insert(prioQueue* pq, PCB process) {
    if (pq->size == pq->capacity) {
        printf("Priority Queue is full!\n");
        return;
    }

    int i = pq->size;
    pq->arr[i] = process; 
    pq->size++;

    while (i != 0 && pq->arr[parent(i)].priority > pq->arr[i].priority) {
        swap(&pq->arr[i], &pq->arr[parent(i)]);
        i = parent(i);
    }
}
PCB extractMax(prioQueue* pq) {
    if (pq->size == 0) {
        PCB empty_process = {0}; 
        return empty_process;
    }

    PCB max_process = pq->arr[0];
    pq->arr[0] = pq->arr[pq->size - 1];
    pq->size--;
    heapify_down(pq, 0);

    return max_process;
}

int peekPrio(prioQueue* pq) {
    if (pq->size == 0) {
        return -1; 
    }
    return pq->arr[0].priority;
}
