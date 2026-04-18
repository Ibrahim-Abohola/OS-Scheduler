
int receive_process_FCFS2CPUs(int msqid,Queue * q1,Queue * q2, int * generator_done)
{
    ProcessMsg msg;
    int received = 0;
    while(msgrcv(msqid, &msg, sizeof(ProcessMsg) - sizeof(long), 0, IPC_NOWAIT) != -1) {
        if(msg.mtype == END_OF_STREAM_MSG_TYPE) {
            *generator_done = 1;
        } else {
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
            printf("At time %d Process added to Queue %d: PID=%d, Arrival=%d, Runtime=%d, Priority=%d\n", getClk(), pcb->cpuid, pcb->id, pcb->arrivalTime, pcb->runTime, pcb->priority);
            received++;
        }
    }
    return received;
}

/*======================================== FCFS 1 CPU ===============================================================================*/
void schedule_FCFS(int msqid,int semid,int TotalProcesses) {
    Queue  readyQueue;
    initQueue(&readyQueue);
    PCB * currentProcess = NULL;
    int done_count = 0; // count how many processes have finished
    int last_time = -1; // to track clock cycles
    int n = pcbTableSize; // total number of processes
    int generator_done = 0; // flag to indicate generator has finished sending all processes
    scheduler_log = fopen("scheduler.log", "w");
    while (!generator_done || currentProcess != NULL || readyQueue.size > 0) {
         int currentTime = getClk();
        if(currentTime == last_time)    
            continue;
        last_time = currentTime;
        down(semid); 
        receive_process_FCFS(msqid, &readyQueue, 1, &generator_done);

        if(currentProcess) 
        {
            currentProcess->remainingTime--;
            int s1 = semget(PROC_SEM_KEY + currentProcess->id, 1, 0666);
            if(s1 == -1) { perror("semget failed"); exit(1); }
            up(s1); // signal the process to run for one time unit
        }
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
void schedule_FCFS_2CPUs(int msqid,int semid,int TotalProcesses, int N, int M)
{
    printf("Starting FCFS with 2 CPUs, N=%d, M=%d\n", N, M);
    Queue q1; initQueue(&q1);
    Queue q2; initQueue(&q2);
    PCB * currentProcess1 = NULL;
    PCB * currentProcess2 = NULL;
    int done_count = 0; // count how many processes have finished
    int overhead1 = 0; // stealing overhead for CPU 1
    int overhead2 = 0; // stealing overhead for CPU 2
    int steal_timer = 0; // counts up to N to steal then acc to diff threshold M
    int last_time= -1; // tick every clock tick to check for stealing
    int generator_done = 0; // flag to indicate generator has finished sending all processes
    scheduler1_log = fopen("scheduler1.log", "w");
    scheduler2_log = fopen("scheduler2.log", "w");
    while (!generator_done || currentProcess1 != NULL || currentProcess2 != NULL || q1.size > 0 || q2.size > 0) {
        int currentTime = getClk();
        if(currentTime == last_time) 
            continue; 
        last_time = currentTime;
        down(semid);
        receive_process_FCFS2CPUs(msqid, &q1, &q2, &generator_done);

        //finish check
        pid_t finished_pid;
        while((finished_pid = waitpid(-1, NULL, WNOHANG)) > 0)
        {
            if(currentProcess1 && finished_pid == currentProcess1->pid) {
                int s = semget(PROC_SEM_KEY + currentProcess1->id, 1, 0666);
                semctl(s, 0, IPC_RMID);   // clean up semaphore
                currentProcess1->finishTime = currentTime;
                currentProcess1->remainingTime = 0;
                currentProcess1->state = 'F';
                log_scheduler_event(scheduler1_log, currentTime, currentProcess1, "finished");
                done_count++;
                currentProcess1 = NULL;
            }
            if(currentProcess2 && finished_pid == currentProcess2->pid) {
                int s = semget(PROC_SEM_KEY + currentProcess2->id, 1, 0666);
                semctl(s, 0, IPC_RMID);   // clean up semaphore
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
                continue;
            }
        }
      
        bool cpu1_stalled = overhead1 > 0;
        bool cpu2_stalled = overhead2 > 0;
        if (!cpu1_stalled && currentProcess1 == NULL && q1.size > 0) {
            currentProcess1 = dequeue(&q1);
            start_process(scheduler1_log, currentProcess1,currentTime);
        }
        if (!cpu2_stalled && currentProcess2 == NULL && q2.size > 0) {
            currentProcess2 = dequeue(&q2);
            start_process(scheduler2_log, currentProcess2,currentTime);
        }
     
        if(currentProcess1 && !cpu1_stalled){
         currentProcess1->remainingTime--;
         int s1 = semget(PROC_SEM_KEY + currentProcess1->id, 1, 0666);
            if(s1 == -1) { perror("semget failed"); exit(1); }
            up(s1); // signal the process to run for one time unit
        }

        if(currentProcess2 && !cpu2_stalled) {
            currentProcess2->remainingTime--;
            int s2 = semget(PROC_SEM_KEY + currentProcess2->id, 1, 0666);
            if(s2 == -1) { perror("semget failed"); exit(1); }
            up(s2); // signal the process to run for one time unit
        }
        if(overhead1 > 0) overhead1--;
        if(overhead2 > 0) overhead2--; 
          if(overhead1 == 0 && cpu1_stalled && currentProcess1) {
            kill(currentProcess1->pid, SIGCONT);
        }
        if(overhead2 == 0 && cpu2_stalled && currentProcess2) {
            kill(currentProcess2->pid, SIGCONT);
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