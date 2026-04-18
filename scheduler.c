#include "headers.h"
#include "DataStructures.h"
void HPF (int msg_id,int sem_id,int total_processes);
void RR (int msg_id, int sem_id, int total_processes, int quantum);

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

    int sem_id = semget(PROC_SEM_KEY + pcb->id, 1, IPC_CREAT | 0666);
    if(sem_id == -1) { perror("semget failed"); exit(-1); }
    union Semun sem_un;
    sem_un.val = 0;
    semctl(sem_id, 0, SETVAL, sem_un);
    
    pid_t pid = fork();
    if (pid == -1)
    {
        perror("Fork failed");
        return;
    }
    else if (pid == 0)
    {
        char remainingTimeStr[10];
        char semIdStr[10];
        sprintf(remainingTimeStr, "%d", pcb->remainingTime);
        sprintf(semIdStr, "%d", sem_id);
        execl("./process.out", "process.out", remainingTimeStr, semIdStr, NULL);
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
int receive_process_FCFS(int msqid,Queue * readyQueue,int cpu_id, int * generator_done)
{
    ProcessMsg msg;
    int received = 0;
    while(msgrcv(msqid, &msg, sizeof(ProcessMsg) - sizeof(long), 0, IPC_NOWAIT) != -1) {
        if(msg.mtype == END_OF_STREAM_MSG_TYPE) {
            *generator_done = 1;
        } else {
            PCB * pcb = new_process(msg.id, msg.arrival, msg.runtime, msg.priority);
            pcb->cpuid = cpu_id;
            enqueue(readyQueue, pcb);
            printf("Process added to Queue %d: PID=%d, Arrival=%d, Runtime=%d, Priority=%d\n", cpu_id, pcb->id, pcb->arrivalTime, pcb->runTime, pcb->priority);
            received++;
        }
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


void schedule_FCFS_single_with_stealing(int cpu_id, int msqid, int semid,
    StealShm* shm, int steal_sem_id, int TotalProcesses, int N, int M)
{
    Queue readyQueue;
    initQueue(&readyQueue);
    PCB*  currentProcess = NULL;
    int   done_count     = 0;
    int   last_time      = -1;
    int   overhead       = 0;
    int  generator_done = 0;
    int next_steal_check = N;    
    bool pending_recheck = false;

    FILE* log_file = fopen(cpu_id == 1 ? "scheduler1.log" : "scheduler2.log", "w");

    while (!generator_done || currentProcess != NULL || readyQueue.size > 0) {
        bool finished = false;
        int currentTime = getClk();
        if(currentTime == last_time)  continue; 
        last_time = currentTime;

        down(semid); 
        
        
       receive_process_FCFS(msqid, &readyQueue, cpu_id, &generator_done);

        // ── Check if current process finished ───────────
        if(currentProcess) {
            pid_t finished_pid = waitpid(-1, NULL, WNOHANG);
            if(finished_pid > 0 && finished_pid == currentProcess->pid) {
                finished = true;
                int ps = semget(PROC_SEM_KEY + currentProcess->id, 1, 0666);
                semctl(ps, 0, IPC_RMID);

                currentProcess->finishTime  = currentTime;
                currentProcess->state       = 'F';
                currentProcess->remainingTime = 0;
                log_scheduler_event(log_file, currentTime, currentProcess, "finished");
                done_count++;
                currentProcess = NULL;
            }
        }

         
        if(currentTime == next_steal_check) {
            bool steal_occurred = false;
            int my_remaining = totalRemainingTime(&readyQueue)
                            + (currentProcess ? currentProcess->remainingTime : 0);
            if(cpu_id == 1) { shm->remaining1 = my_remaining; shm->cpu1_arrived = 1; }
            else            { shm->remaining2 = my_remaining; shm->cpu2_arrived  = 1; }

            // Barrier: wait until both CPUs write remaining time
            while(!(shm->cpu1_arrived && shm->cpu2_arrived));

            // ── CPU1 = coordinator ────────────────────────────────────────────────
            if(cpu_id == 1) {
                int steals = 0;
                int r1 = shm->remaining1;
                int r2 = shm->remaining2;
                if(abs(r1 - r2) > M) {
                    int steal_time = currentTime + steals * 3;
                    
                    if(r1 > r2) {
                        // steal from CPU1's own queue → give to CPU2
                        if(readyQueue.size == 0) break;
                        PCB* stolen     = steal(&readyQueue);
                        stolen->cpuid   = 2;
                        shm->stolen_process = *stolen;
                        shm->steal_from = 1;
                        shm->steal_ready = 1;
                        fprintf(log_file, "At time %d process %d was stolen\n",
                                steal_time, stolen->id);
                        fflush(log_file);
                        up_n(steal_sem_id, SEM_REQUEST);    
                        down_n(steal_sem_id, SEM_DONE);   
                        r1 -= shm->stolen_process.remainingTime;
                        r2 += shm->stolen_process.remainingTime;

                    } else {
                        // steal from CPU2's queue → give to CPU1
                        shm->steal_from  = 2;
                        shm->steal_ready = 0;
                        up_n(steal_sem_id, SEM_REQUEST);    
                        down_n(steal_sem_id, SEM_DONE);     

                        if(!shm->steal_ready) break;        

                        PCB* received = new_process(
                            shm->stolen_process.id,
                            shm->stolen_process.arrivalTime,
                            shm->stolen_process.runTime,
                            shm->stolen_process.priority);
                        received->remainingTime = shm->stolen_process.remainingTime;
                        received->waitingTime   = shm->stolen_process.waitingTime;
                        received->cpuid = 1;
                        enqueue(&readyQueue, received);
                        r1 += shm->stolen_process.remainingTime;
                        r2 -= shm->stolen_process.remainingTime;
                    }
                    steals++;
                }
              

                shm->steal_checkpoint = steals;
                shm->steal_from = 0;                        
                up_n(steal_sem_id, SEM_REQUEST);           
                down_n(steal_sem_id, SEM_DONE);             

                // Apply overhead to this CPU
                if(steals > 0) {
                    overhead = steals * 3;
                    if(currentProcess) {
                        currentProcess->waitingTime += steals * 3;
                        kill(currentProcess->pid, SIGSTOP);
                    }
                    
                }
                steal_occurred = steals > 0;
                // Reset barrier flags for the next N-tick check
                shm->cpu1_arrived = 0;
                shm->cpu2_arrived = 0;
           

            // ── CPU2 = worker ─────────────
            } else {
                int steals_done = 0;

                while(1) {
                    down_n(steal_sem_id, SEM_REQUEST);      

                    if(shm->steal_from == 0) {
                        steals_done = shm->steal_checkpoint;
                        up_n(steal_sem_id, SEM_DONE);       
                        break;

                    } else if(shm->steal_from == 1) {
                        PCB* received = new_process(
                            shm->stolen_process.id,
                            shm->stolen_process.arrivalTime,
                            shm->stolen_process.runTime,
                            shm->stolen_process.priority);
                        received->remainingTime = shm->stolen_process.remainingTime;
                        received->waitingTime   = shm->stolen_process.waitingTime;
                        received->cpuid = 2;
                        enqueue(&readyQueue, received);
                        steals_done++;
                        up_n(steal_sem_id, SEM_DONE);      

                    } else if(shm->steal_from == 2) {
                        if(readyQueue.size > 0) {
                            PCB* stolen   = steal(&readyQueue);
                            stolen->cpuid = 1;
                            shm->stolen_process = *stolen;
                            shm->steal_ready = 1;
                            int steal_time = currentTime + steals_done * 3;
                            fprintf(log_file, "At time %d process %d was stolen\n",
                                    steal_time, stolen->id);
                            fflush(log_file);
                            steals_done++;
                        } else {
                            shm->steal_ready = 0;           
                        }
                        up_n(steal_sem_id, SEM_DONE);      
                    }
                }

                if(steals_done > 0) {
                    overhead = steals_done * 3;
                    if(currentProcess) {
                        currentProcess->waitingTime += steals_done * 3;
                        kill(currentProcess->pid, SIGSTOP);
                    }
                }
               steal_occurred =steal_occurred || steals_done > 0;
            }
            next_steal_check = steal_occurred ? currentTime + 3 : (currentTime / N + 1) * N ;
            if(steal_occurred) continue; 
        } 
      
        bool is_stalled = overhead > 0;
        if(overhead > 0) overhead--;
        if(overhead == 0 && is_stalled && currentProcess){
            kill(currentProcess->pid, SIGCONT);
        }

        if(!is_stalled && currentProcess == NULL && readyQueue.size > 0 && !finished) {
            currentProcess = dequeue(&readyQueue);
            start_process(log_file, currentProcess, currentTime);
        }

        if(currentProcess && !is_stalled) {
            currentProcess->remainingTime--;
            int ps = semget(PROC_SEM_KEY + currentProcess->id, 1, 0666);
            if(ps == -1) { perror("semget proc sem"); exit(1); }
            up(ps);
            
        }
        
        
    }

    FILE* perf_file = fopen(cpu_id == 1 ? "scheduler1.perf" : "scheduler2.perf", "w");
    calculate_performance(perf_file, cpu_id);
    fclose(log_file);
    fclose(perf_file);
}

int main(int argc, char * argv[])
{
    initClk();
    signal(SIGUSR1, SIG_IGN);
    
    if(argc < 3) {
        fprintf(stderr, "Usage: %s <cpu_id> <algorithm> <total_processes> [parameters]\n", argv[0]);
        exit(1);
    }
    
    int cpu_id = atoi(argv[1]);
    int algo = atoi(argv[2]);
    int TotalProcesses = atoi(argv[3]);
    int quantum = argc > 4 ? atoi(argv[4]) : 0;
    int N = argc > 4 ? atoi(argv[4]) : 0;
    int M = argc > 5 ? atoi(argv[5]) : 0;
    
    if(algo == ALGO_RR && argc < 5) {
        fprintf(stderr, "Usage for RR: %s <cpu_id> 2 <total> <quantum>\n", argv[0]);
        exit(1);
    }
    else if(algo == ALGO_FCFS_2CPUS && argc < 6) {
        fprintf(stderr, "Usage for 2CPUs: %s <cpu_id> 3 <total> <N> <M>\n", argv[0]);
        exit(1);
    }

    key_t key = ftok("keyfile", cpu_id == 1 ? MSGKEY1 : MSGKEY2);
    if(key == -1) { perror("ftok failed"); exit(-1); }
    int msqid = msgget(key, 0666);
    if(msqid == -1) {
        perror("msgget failed");
        exit(1);
    }
    
    int sem_id = semget(SEMKEY, 1, 0666);
    if(sem_id == -1) { perror("semget sem failed"); exit(1); }
    
    
    int steal_sem_id = -1;
    StealShm* shm = NULL;
    if(algo == ALGO_FCFS_2CPUS) {
        int shmid = shmget(STEAL_SHM_KEY, sizeof(StealShm), IPC_CREAT | 0666);
        if(shmid == -1) { perror("shmget failed"); exit(1); }
        
        shm = (StealShm*)shmat(shmid, 0, 0);
        if((long)shm == -1) { perror("shmat failed"); exit(1); }
        
        if(cpu_id == 1) {
            shm->remaining1 = 0;
            shm->remaining2 = 0;
            shm->steal_from = 0;
            shm->steal_ready = 0;
            shm->barrier_count = 0;
            shm->steal_checkpoint = 0;
            shm->cpu1_arrived = 0;
            shm->cpu2_arrived = 0;
        }
        
        steal_sem_id = semget(STEAL_SEM_KEY, 3, IPC_CREAT | 0666);
        if(steal_sem_id == -1) { perror("semget steal failed"); exit(1); }
        
        if(cpu_id == 1) {
            union Semun sem_un;
            sem_un.val = 0;
            semctl(steal_sem_id, SEM_BARRIER, SETVAL, sem_un);
            semctl(steal_sem_id, SEM_REQUEST, SETVAL, sem_un);
            semctl(steal_sem_id, SEM_DONE, SETVAL, sem_un);
        }
    
    }
    
    switch(algo) {
        case ALGO_RR:
            RR(msqid, sem_id, TotalProcesses, quantum);
            break;
        case ALGO_HPF:  
            HPF(msqid, sem_id, TotalProcesses);
            break;
        case ALGO_FCFS_2CPUS: 
            schedule_FCFS_single_with_stealing(cpu_id, msqid, sem_id, shm, steal_sem_id, TotalProcesses, N, M);
            break;
        default:
            fprintf(stderr, "Unknown algorithm ID: %d\n", algo);
            exit(1);
    }
    
    if(algo == ALGO_FCFS_2CPUS && cpu_id == 1) {
        int shmid = shmget(STEAL_SHM_KEY, 0, 0666);
        if(shmid != -1) {
            shmctl(shmid, IPC_RMID, NULL);
            printf("Shared memory removed by CPU1.\n");
        }
        
        int steal_sem_id_cleanup = semget(STEAL_SEM_KEY, 0, 0666);
        if(steal_sem_id_cleanup != -1) {
            semctl(steal_sem_id_cleanup, 0, IPC_RMID);
            printf("Steal semaphores removed by CPU1.\n");
        }
    }
    
    return 0;
}
