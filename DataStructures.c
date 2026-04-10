#include "headers.h"

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
    char state; //R, W, S
} PCB;