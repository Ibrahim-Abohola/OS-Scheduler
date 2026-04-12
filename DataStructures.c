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

// message struct used by message queue between generator and scheduler
struct msg {
    long mtype;    // must be > 0, always 1
    PCB process;   // the process data
};