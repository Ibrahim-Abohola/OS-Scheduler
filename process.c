#include "headers.h"

/* Modify this file as needed*/
int remainingtime;

int main(int agrc, char * argv[])
{
    // scheduler forks us with only 1 argument: remaining time
    // execl("process.out", "process.out", remainingTimeStr, NULL)
    remainingtime = atoi(argv[1]);

    initClk();
    
    int last_time = getClk() - 1;  // Start with -1 so first clock detection causes immediate decrement

    //TODO it needs to get the remaining time from somewhere
    //remainingtime = ??;
    while (remainingtime > 0)
    {
       int currentTime = getClk();
        if(currentTime == last_time) 
        {
            usleep(50000);
            continue; 
        }
        remainingtime--;
        last_time = currentTime;
    }
    
    destroyClk(false);
    
    return 0;
}