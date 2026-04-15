#include "PrioQueue.h"

void HPF (int msg_id,int sem_id,int total_processes) {   
    prioQueue* pq=create_pq(100);
    PCB running_process;
    int finished_processes=0;
    bool isRunning=false;
    FILE* log_file=fopen("HPFscheduler.log","w");
    
    float total_wta = 0;
    float total_waiting = 0;
    float total_wta_square = 0;
    int total_runtime = 0;

    int current_time=getClk();
    int last_time=-1;
    int context_switching=0;
    while(finished_processes<total_processes){
        current_time=getClk();
        if(current_time>last_time){
            down(sem_id); 
            if(context_switching>0){
                context_switching--;
            }
            else if (isRunning){
                running_process.remainingTime--;
                total_runtime++;
                if(running_process.remainingTime==0){
                    int ta=current_time-running_process.arrivalTime;
                    float wta=(float)ta/running_process.runTime;
                    total_wta += wta;
                    total_wta_square += wta * wta;
                    total_waiting += running_process.waitingTime;
                    fprintf(log_file, "At time %d process %d finished arr %d total %d remain 0 wait %d ta %d wta %.2f\n",
                            current_time, running_process.id, running_process.arrivalTime, 
                            running_process.runTime, running_process.waitingTime, ta, wta);
                    isRunning = false;
                    context_switching++;
                    finished_processes++;
                }
            }
            last_time=current_time;
        }

        ProcessMsg message;
        while(msgrcv(msg_id,&message,sizeof(message),0,IPC_NOWAIT)!=-1){
            PCB new_process;
            new_process.remainingTime=message.runtime;
            new_process.arrivalTime=message.arrival;
            new_process.runTime=message.runtime;
            new_process.priority=message.priority;
            new_process.id=message.id;
            new_process.state='W';
            if(isRunning&&new_process.priority<running_process.priority&&context_switching==0){
                kill(running_process.pid,SIGSTOP);
                running_process.state='S';
                running_process.lastRun=current_time;
                fprintf(log_file, "At time %d process %d stopped arr %d total %d remain %d wait %d\n",
                        current_time, running_process.id, running_process.arrivalTime, 
                        running_process.runTime, running_process.remainingTime, running_process.waitingTime);
                    insert(pq,running_process);
                    isRunning=false;
                    context_switching=1;
                }
            insert(pq,new_process);
        }

        if(pq->size>0&&!isRunning&&context_switching==0){
            running_process=extractMax(pq);
            if(running_process.state=='W'&&running_process.remainingTime==running_process.runTime){
                int processID=fork();
                if(processID==0){
                    char remainingTimeStr[10];
                    sprintf(remainingTimeStr, "%d", running_process.remainingTime);
                    execl("./process.out", "process.out", remainingTimeStr, NULL);
                }
                else{
                    running_process.startTime=current_time;
                    running_process.pid=processID;
                    running_process.state='R';
                    running_process.waitingTime+=current_time-running_process.arrivalTime;
                    fprintf(log_file, "At time %d process %d started arr %d total %d remain %d wait %d\n",
                            current_time, running_process.id, running_process.arrivalTime, running_process.runTime
                            ,running_process.remainingTime, running_process.waitingTime);
                    isRunning=true;
                }
            }
            else if(running_process.state=='S'){
                kill(running_process.pid,SIGCONT);
                running_process.waitingTime+=current_time-running_process.lastRun;
                running_process.state='R';
                fprintf(log_file, "At time %d process %d started arr %d total %d remain %d wait %d\n",
                            current_time, running_process.id, running_process.arrivalTime, running_process.runTime
                            ,running_process.remainingTime, running_process.waitingTime);
                isRunning=true;
            }
        }

        
    }
    FILE* perf_file = fopen("HPFscheduler.perf", "w");
    float avg_wta = total_wta / total_processes;
    float avg_wait = total_waiting / total_processes;
    float std_dev = sqrt((total_wta_square / total_processes) - (avg_wta * avg_wta));
    float cpu_util = ((float)total_runtime / getClk()) * 100;

    fprintf(perf_file, "CPU utilization = %.2f\n", cpu_util);
    fprintf(perf_file, "Avg WTA = %.2f\n", avg_wta);
    fprintf(perf_file, "Avg Waiting = %.2f\n", avg_wait);
    fprintf(perf_file, "Std WTA = %.2f\n", std_dev);
    
    fclose(log_file);
    fclose(perf_file);
}