#include "headers.h"
#include "DataStructures.h"
void HPF (int msg_id,int sem_id,int total_processes);
void RR (int msg_id, int total_processes, int quantum);

/*==================== PCB Table ============================= */
PCB pcbTable[1000]; // assuming maximum 100 processes
int pcbTableSize = 0;  // how many processes in the table so far

PCB * new_process(int id, int arrivalTime, int runTime, int priority) {
    PCB * pcb = &pcbTable[pcbTableSize++];
    pcb->id = id;
    pcb->arrivalTime = arrivalTime;
    pcb->runTime = runTime;
    pcb->priority = priority;
    pcb->waitingTime = 0;
    pcb->remainingTime = runTime;
    pcb->startTime = -1;
    pcb->finishTime = -1; 
    pcb->lastRun = -1;
    pcb->state = 'W'; // Ready state
    pcb->cpuid = 1; // Default 1 CPU
    return pcb;
}

/* =================== Log Files and functions =================== */
FILE * scheduler_log = NULL;
FILE * scheduler1_log = NULL;
FILE * scheduler2_log = NULL;

void log_scheduler_event(FILE * log_file, int time, PCB * pcb, const char * event) {
   if (log_file == NULL) {
        log_file = fopen("scheduler.log", "w");
    }
     fprintf(log_file, "At time %d process %d %s arr %d total %d remain %d wait %d",
            time, pcb->id, event, pcb->arrivalTime, pcb->runTime, pcb->remainingTime, pcb->waitingTime);
    if (strcmp(event, "finished") == 0) {
        int TA = pcb->finishTime - pcb->arrivalTime; // Turnaround time
       float WTA = (float)TA / pcb->runTime;
        fprintf(log_file, " TA %d WTA %.2f", TA, WTA);
    }
    fprintf(log_file, "\n");
    fflush(log_file);
}
/*=========================================================================*/

void start_process(FILE * log_file, PCB * pcb, int currentTime) {
    
    pid_t pid = fork();
    if (pid == -1)
    {
        perror("Fork failed");
        return;
    }
    else if (pid == 0)
    {
        char remainingTimeStr[10];
        sprintf(remainingTimeStr, "%d", pcb->remainingTime);
        execl("./process.out", "process.out", remainingTimeStr, NULL);
        perror("execl failed");
        exit(1);
    }
    // Parent process: update PCB and log the event
    if (pcb->startTime == -1) {
        pcb->startTime = currentTime;
        pcb->waitingTime += currentTime - pcb->arrivalTime;
    }
    pcb->pid = pid;
    pcb->state = 'R'; // Running state
    pcb->lastRun = currentTime;
    log_scheduler_event(log_file, currentTime, pcb, "started");
}
/*=========================== Receive new Processes from process generator by the message queue ===========================================*/
int receive_process_FCFS(int msqid,Queue * readyQueue,int cpu_id)
{
    ProcessMsg msg;
    int received = 0;
    while(msgrcv(msqid, &msg, sizeof(ProcessMsg) - sizeof(long), 0, IPC_NOWAIT) != -1) {
        PCB * pcb = new_process(msg.id, msg.arrival, msg.runtime, msg.priority);
        pcb->cpuid = cpu_id;
        enqueue(readyQueue, pcb);
        printf("Process added to Queue %d: PID=%d, Arrival=%d, Runtime=%d, Priority=%d\n", cpu_id, pcb->id, pcb->arrivalTime, pcb->runTime, pcb->priority);
        received++;
    }
    return received;
}
int receive_process_FCFS2CPUs(int msqid,Queue * q1,Queue * q2)
{
    ProcessMsg msg;
    int received = 0;
    while(msgrcv(msqid, &msg, sizeof(ProcessMsg) - sizeof(long), 0, IPC_NOWAIT) != -1) {
        PCB * pcb = new_process(msg.id, msg.arrival, msg.runtime, msg.priority);
        if(q1->size <= q2->size)
        { 
            pcb->cpuid = 1;
            enqueue(q1, pcb);
        } 
        else
        {
            pcb->cpuid = 2;
            enqueue(q2, pcb);
        }
        printf("Process added to Queue %d: PID=%d, Arrival=%d, Runtime=%d, Priority=%d\n", pcb->cpuid, pcb->id, pcb->arrivalTime, pcb->runTime, pcb->priority);
        received++;
    }
    return received;
}
/*============================================ Performance Calculations  ==============================================================================================*/
void calculate_performance(FILE * perfFile,int cpuid) {
    double total_WTA = 0;
    double total_waiting = 0;
    int last_finish = 0;
    int first_start = INT_MAX;
    int total_runtime = 0;
    int n = 0;

    for(int i =0; i < pcbTableSize; i++) {
        PCB * pcb = &pcbTable[i];
        if (pcb->cpuid == cpuid) {
            int TA = pcb->finishTime - pcb->arrivalTime; // Turnaround time
            float WTA = (float)TA / pcb->runTime;
            total_WTA += WTA;
            total_waiting += pcb->waitingTime;
            if (pcb->finishTime > last_finish) {
                last_finish = pcb->finishTime;
            }
            if (pcb->startTime < first_start) {
                first_start = pcb->startTime;
            }
            total_runtime += pcb->runTime;
            n++;
        }
    }
    if(n == 0) return;
    double avg_WTA = total_WTA / n;
    double avg_waiting = total_waiting / n;
    double cpu_utilization = (double)total_runtime / (last_finish - first_start) * 100;
    /* standard deviation of WTA */
    double sum_squared_diff = 0;
    for(int i =0; i < pcbTableSize; i++) {
        PCB * pcb = &pcbTable[i];
        if (pcb->cpuid == cpuid) {
            int TA = pcb->finishTime - pcb->arrivalTime; 
            float WTA = (float)TA / pcb->runTime;
            sum_squared_diff += (WTA - avg_WTA) * (WTA - avg_WTA);
        }
    }
    double stddev_WTA = sqrt(sum_squared_diff / n);
    fprintf(perfFile, "CPU utilization = %.2f%%\n", cpu_utilization);
    fprintf(perfFile, "Avg WTA = %.2f\n",           avg_WTA);
    fprintf(perfFile, "Avg Waiting = %.2f\n",        avg_waiting);
    fprintf(perfFile, "Std WTA = %.2f\n",             stddev_WTA);
    fflush(perfFile);

}
/*======================================== FCFS 1 CPU ===============================================================================*/
void schedule_FCFS(int msqid,int TotalProcesses) {
    Queue  readyQueue;
    initQueue(&readyQueue);
    PCB * currentProcess = NULL;
    int done_count = 0; // count how many processes have finished
    int last_time = -1; // to track clock cycles
    int n = pcbTableSize; // total number of processes
    scheduler_log = fopen("scheduler.log", "w");
    while (done_count < TotalProcesses || currentProcess != NULL || readyQueue.size > 0) {
         int currentTime = getClk();
        if(currentTime == last_time)    
        {
            usleep(50000);
            continue; // only check for stealing every clock tick
        }
        last_time = currentTime;
        if(currentProcess) currentProcess->remainingTime--;
        receive_process_FCFS(msqid, &readyQueue, 1);
        if (currentProcess == NULL && readyQueue.size > 0) {
            currentProcess = dequeue(&readyQueue);
            start_process(scheduler_log, currentProcess, currentTime);
        }
        if(currentProcess) {
            pid_t finished_pid = waitpid(-1, NULL, WNOHANG);
            if(finished_pid > 0 && finished_pid == currentProcess->pid) {
                currentProcess->finishTime = currentTime;
                currentProcess->state = 'F'; // Finished state
                currentProcess->remainingTime = 0;
                log_scheduler_event(scheduler_log, currentProcess->finishTime, currentProcess, "finished");
                done_count++;
                currentProcess = NULL;
                }
            }
        if(!currentProcess && readyQueue.size > 0)
        {
            currentProcess = dequeue(&readyQueue);
            start_process(scheduler_log, currentProcess, currentTime);
        }
    }
    FILE * scheduler_perf = fopen("scheduler.perf", "w");
    calculate_performance(scheduler_perf, 1);
    fclose(scheduler_log);
    fclose(scheduler_perf);
}
/*=================================== FCFS 2 CPUs ===============================================================*/
void schedule_FCFS_2CPUs(int msqid,int TotalProcesses, int N, int M)
{
    printf("Starting FCFS with 2 CPUs, N=%d, M=%d\n", N, M);
    Queue q1; initQueue(&q1);
    printf("Initialized Queue 1\n");
    Queue q2; initQueue(&q2);
    printf("Initialized Queue 2\n");
    PCB * currentProcess1 = NULL;
    PCB * currentProcess2 = NULL;
    int done_count = 0; // count how many processes have finished
    int overhead1 = 0; // stealing overhead for CPU 1
    int overhead2 = 0; // stealing overhead for CPU 2
    int steal_timer = 0; // counts up to N to steal then acc to diff threshold M
    int last_time= -1; // tick every clock tick to check for stealing
    scheduler1_log = fopen("scheduler1.log", "w");
    scheduler2_log = fopen("scheduler2.log", "w");
    while (done_count < TotalProcesses || currentProcess1 != NULL || currentProcess2 != NULL || q1.size > 0 || q2.size > 0) {
        int currentTime = getClk();
        if(currentTime == last_time) 
        {
            usleep(50000);
            continue; 
        }
        last_time = currentTime;
        bool cpu1_stalled = overhead1 > 0;
        bool cpu2_stalled = overhead2 > 0;
        if(currentProcess1 && !cpu1_stalled) currentProcess1->remainingTime--;
        if(currentProcess2 && !cpu2_stalled) currentProcess2->remainingTime--;
        steal_timer++;
        if(overhead1 > 0) overhead1--;
        if(overhead2 > 0) overhead2--;

        receive_process_FCFS2CPUs(msqid, &q1, &q2);

        if(overhead1 == 0 && cpu1_stalled && currentProcess1) {
            kill(currentProcess1->pid, SIGCONT);
        }
        if(overhead2 == 0 && cpu2_stalled && currentProcess2) {
            kill(currentProcess2->pid, SIGCONT);
        }
     
        if (!cpu1_stalled && currentProcess1 == NULL && q1.size > 0) {
            currentProcess1 = dequeue(&q1);
            start_process(scheduler1_log, currentProcess1,currentTime);
        }
        if (!cpu2_stalled && currentProcess2 == NULL && q2.size > 0) {
            currentProcess2 = dequeue(&q2);
            start_process(scheduler2_log, currentProcess2,currentTime);
        }
        pid_t finished_pid;
        while((finished_pid = waitpid(-1, NULL, WNOHANG)) > 0)
        {
            if(currentProcess1 && finished_pid == currentProcess1->pid) {
                currentProcess1->finishTime = currentTime;
                currentProcess1->remainingTime = 0;
                currentProcess1->state = 'F';
                log_scheduler_event(scheduler1_log, currentTime, currentProcess1, "finished");
                done_count++;
                currentProcess1 = NULL;
            }
            if(currentProcess2 && finished_pid == currentProcess2->pid) {
                currentProcess2->finishTime = currentTime;
                currentProcess2->remainingTime = 0;
                currentProcess2->state = 'F';
                log_scheduler_event(scheduler2_log, currentTime, currentProcess2, "finished");
                done_count++;
                currentProcess2 = NULL;
            }
        }       
        // steal logic: every N time units, check the difference in queue remaining time and steal if the difference is greater than M
        if(currentTime % N == 0 && currentTime != 0)
        {
            steal_timer = 0;
            int q1_remaining_time = totalRemainingTime(&q1) + (currentProcess1 ? currentProcess1->remainingTime : 0);
            int q2_remaining_time = totalRemainingTime(&q2) + (currentProcess2 ? currentProcess2->remainingTime : 0);
            int steals = 0;
            int steal_start_time = currentTime;   // snapshot time at start of steal check
            while(abs(q1_remaining_time - q2_remaining_time) > M)
            {
                int steal_time = steal_start_time + steals * 3; // each steal adds 3s overhead, so next steal can only happen after that
                if(q1_remaining_time > q2_remaining_time) 
                {
                // Steal from q1 to q2
                PCB * process = steal(&q1);
                process->cpuid = 2;
                enqueue(&q2, process);
                fprintf(scheduler1_log, "At time %d process %d was stolen\n", steal_time, process->id);
                steals++;
                }
                else
                {
                // Steal from q2 to q1
                PCB * process = steal(&q2);
                process->cpuid = 1;
                enqueue(&q1, process);
                fprintf(scheduler2_log, "At time %d process %d was stolen\n", steal_time, process->id);   
                steals++;
                }
                q1_remaining_time = totalRemainingTime(&q1) + (currentProcess1 ? currentProcess1->remainingTime : 0);
                q2_remaining_time = totalRemainingTime(&q2) + (currentProcess2 ? currentProcess2->remainingTime : 0);

            }
            if(steals > 0){
                // Stealing overhead of 3s for both CPUs
                overhead1 = steals * 3;
                overhead2 = steals * 3;
                if(currentProcess1) {
                    currentProcess1->waitingTime += 3 * steals; // add the overhead to waiting time
                    kill(currentProcess1->pid, SIGSTOP);  // freeze it
                }
                if(currentProcess2) {
                    currentProcess2->waitingTime += 3 * steals; // add the overhead to waiting time
                    kill(currentProcess2->pid, SIGSTOP);  // freeze it
                }
            }
        }
      
    }
    FILE * scheduler1_perf = fopen("scheduler1.perf", "w");
    FILE * scheduler2_perf = fopen("scheduler2.perf", "w");
    calculate_performance(scheduler1_perf, 1);
    calculate_performance(scheduler2_perf, 2);
    fclose(scheduler1_log);
    fclose(scheduler2_log);
    fclose(scheduler1_perf);
    fclose(scheduler2_perf);
}
     

int main(int argc, char * argv[])
{
    initClk();
    signal(SIGUSR1, SIG_IGN);
    //TODO implement the scheduler :)
    //upon termination release the clock resources.
    if(argc < 2) {
        fprintf(stderr, "Usage: %s <algorithm> [parameters]\n", argv[0]);
        exit(1);
    }
    int algorithm = atoi(argv[1]);
    if(algorithm == 2 && argc < 4) {
        fprintf(stderr, "Usage for hpf: %s 2 <n> <q> \n", argv[0]);
        exit(1);
    }
    else if(algorithm == 3 && argc < 5) {
        fprintf(stderr, "Usage for 2 CPUs: %s 3 <n> <N> <M> \n", argv[0]);
        exit(1);
    }
    int algo = atoi(argv[1]);
    int TotalProcesses = atoi(argv[2]);
    int quantum = argc > 3 ? atoi(argv[3]) : 0;
    int N = argc > 3 ? atoi(argv[3]) : 0;
    int M = argc > 4 ? atoi(argv[4]) : 0;

    key_t key = ftok("keyfile", MSGKEY);
    int msqid = msgget(key, 0666 | IPC_CREAT);
    if(msqid == -1) {
        perror("msgget failed");
        exit(1);
    }
    int sem_id = semget(SEMKEY, 1, 0666);
    if(sem_id == -1) { perror("semget sem failed"); exit(1); }
    switch(algo) {
        case ALGO_RR:
            RR(msqid, TotalProcesses, quantum);
            break;
        case ALGO_HPF:  
            HPF(msqid, sem_id, TotalProcesses);
            break;
        case ALGO_FCFS_2CPUS: 
            schedule_FCFS_2CPUs(msqid, TotalProcesses, N, M);
            break;
        default:
            fprintf(stderr, "Unknown algorithm ID: %d\n", algo);
            exit(1);
    }
    destroyClk(true);
    return 0;
}
