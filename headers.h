#ifndef HEADERS_H
#define HEADERS_H

#include <stdio.h>      //if you don't use scanf/printf change this include
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <limits.h>
#include <string.h>
#include <math.h>

typedef short bool;
#define true 1
#define false 0

#define SHKEY 300

/* ======== Message queue key (generator <-> scheduler) ============= */         
#define MSGKEY 200 

/* ==================== Algorithm IDs ================================= */
#define ALGO_HPF  1
#define ALGO_RR   2
#define ALGO_FCFS_2CPUS 3   

/* =========================================== */
typedef struct {
    long mtype;    
    int  id;
    int  arrival;
    int  runtime;
    int  priority;
} ProcessMsg;



///==============================
//don't mess with this variable//
extern int * shmaddr;                 //
//===============================


int getClk();

/*
 * All process call this function at the beginning to establish communication between them and the clock module.
 * Again, remember that the clock is only emulation!
*/
void initClk();

/*
 * All process call this function at the end to release the communication
 * resources between them and the clock module.
 * Again, Remember that the clock is only emulation!
 * Input: terminateAll: a flag to indicate whether that this is the end of simulation.
 *                      It terminates the whole system and releases resources.
*/

void destroyClk(bool terminateAll);

#endif
