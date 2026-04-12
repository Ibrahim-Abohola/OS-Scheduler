#include "headers.h"

/* Modify this file as needed*/
int remainingtime;

int main(int agrc, char * argv[])
{
    // scheduler forks us with only 1 argument: remaining time
    // execl("process.out", "process.out", remainingTimeStr, NULL)
    remainingtime = atoi(argv[1]);

    initClk();
    
    int last_clk = getClk();

    //TODO it needs to get the remaining time from somewhere
    //remainingtime = ??;
    while (remainingtime > 0)
    {
        int now = getClk();
        if (now != last_clk)
        {
            remainingtime -= (now - last_clk); // subtract elapsed ticks
            last_clk = now;
        }
    }
    
    destroyClk(false);
    
    return 0;
}