#include "circQ.h" 
#include <math.h> 

void RR(int msg_id, int total_processes, int quantum) {
    initClk();  /* initialize this TU's static shmaddr */
    CircQ *q = initCircQ(100);
    PCB running_process;
    int finished_processes = 0;
    bool isRunning = false;
    FILE *log_file = fopen("scheduler.log", "w");

    float total_wta        = 0;
    float total_waiting    = 0;
    float total_wta_square = 0;
    int   total_runtime    = 0;
    int   last_finish_time = 0; /* denominator for CPU utilisation */

    int current_time    = getClk();
    int last_time       = current_time;
    int context_switch  = 0;
    int quantum_remain  = 0;

    while (finished_processes < total_processes) {
        current_time = getClk();

        if (current_time > last_time) {
            if (context_switch > 0) {
                /* Burning the 1-sec context-switch overhead — CPU is idle */
                context_switch--;
            } else if (isRunning) {
                running_process.remainingTime--;
                quantum_remain--;
                total_runtime++;

                if (running_process.remainingTime == 0) {
                    /* Process finished — compute stats and log */
                    last_finish_time           = current_time;
                    int   ta  = current_time - running_process.arrivalTime;
                    float wta = (float)ta / running_process.runTime;
                    total_wta        += wta;
                    total_wta_square += wta * wta;
                    total_waiting    += running_process.waitingTime;

                    /* TA and WTA must be uppercase — output is auto-compared */
                    fprintf(log_file,
                            "At time %d process %d finished arr %d total %d remain 0 wait %d TA %d WTA %.2f\n",
                            current_time, running_process.id,
                            running_process.arrivalTime, running_process.runTime,
                            running_process.waitingTime, ta, wta);

                    isRunning = false;
                    finished_processes++;
                    
                    /* Add context switching overhead if there are more processes waiting (Q.28, Q.30) */
                    if (q->size > 0) {
                        context_switch = 1;
                    }

                } else if (quantum_remain == 0) {
                    /* Quantum expired — preempt, re-enqueue, pay context-switch cost */
                    kill(running_process.pid, SIGSTOP);
                    running_process.state   = 'S';
                    running_process.lastRun = current_time;

                    fprintf(log_file,
                            "At time %d process %d stopped arr %d total %d remain %d wait %d\n",
                            current_time, running_process.id,
                            running_process.arrivalTime, running_process.runTime,
                            running_process.remainingTime, running_process.waitingTime);

                    enqueueCircQ(q, running_process);
                    isRunning      = false;
                    
                    /* Add context switching overhead only if there are other processes (Q.9)
                     * "If it's the only process in the queue, then it should continue 
                     * running without context switching (no 1 sec overhead)." */
                    if (q->size > 1) {
                        context_switch = 1;
                    }
                }
            }
            last_time = current_time;
        }

        /* Drain any newly arrived processes from the message queue */
        ProcessMsg message;
        while (msgrcv(msg_id, &message, sizeof(ProcessMsg) - sizeof(long), 0, IPC_NOWAIT) != -1) {
            /* Extract PCB data from the received message */
            PCB new_process;
            new_process.id            = message.id;
            new_process.arrivalTime   = message.arrival;
            new_process.runTime       = message.runtime;
            new_process.priority      = message.priority;
            new_process.remainingTime = new_process.runTime;
            new_process.state         = 'W';
            new_process.waitingTime   = 0;   /* Will be updated when process starts */
            enqueueCircQ(q, new_process);
        }

        /* Dispatch next process if CPU is free and no context-switch overhead pending */
        if (q->size > 0 && !isRunning && context_switch == 0) {
            running_process = dequeueCircQ(q);
            quantum_remain  = quantum;

            if (running_process.state == 'W' &&
                running_process.remainingTime == running_process.runTime) {
                /* First time this process runs — fork it */
                int processID = fork();
                if (processID == 0) {
                    char remainingTimeStr[10];
                    sprintf(remainingTimeStr, "%d", running_process.remainingTime);
                    execl("./process.out", "process.out", remainingTimeStr, NULL);
                    perror("execl process.out failed");
                    exit(-1);
                } else {
                    running_process.startTime = current_time;
                    running_process.pid       = processID;
                    running_process.state     = 'R';
                    
                    /* Track initial waiting time from arrival to start (Q.3, Q.7)
                     * Waiting time is accumulated time the process hasn't been running */
                    running_process.waitingTime += current_time - running_process.arrivalTime;
                    
                    fprintf(log_file,
                            "At time %d process %d started arr %d total %d remain %d wait %d\n",
                            current_time, running_process.id,
                            running_process.arrivalTime, running_process.runTime,
                            running_process.remainingTime, running_process.waitingTime);
                    isRunning = true;
                }
            } else if (running_process.state == 'S') {
                /* Resuming a previously stopped process */
                kill(running_process.pid, SIGCONT);
                running_process.waitingTime += current_time - running_process.lastRun;
                running_process.state        = 'R';
                fprintf(log_file,
                        "At time %d process %d resumed arr %d total %d remain %d wait %d\n",
                        current_time, running_process.id,
                        running_process.arrivalTime, running_process.runTime,
                        running_process.remainingTime, running_process.waitingTime);
                isRunning = true;
            }
        }
    }

    /* --- Write performance summary --- */
    FILE  *perf_file = fopen("scheduler.perf", "w");
    float  avg_wta   = total_wta / total_processes;
    float  avg_wait  = total_waiting / total_processes;
    float  std_dev   = sqrt((total_wta_square / total_processes) - (avg_wta * avg_wta));

    /* Denominator is the last finish time, matching the spec's definition:
     * "total CPU running time = time at which last process finished"          */
    float  cpu_util  = ((float)total_runtime / last_finish_time) * 100;

    /* Leading space and %% match the spec's auto-compared output format */
    fprintf(perf_file, " CPU utilization = %.2f%%\n", cpu_util);
    fprintf(perf_file, " Avg WTA = %.2f\n", avg_wta);
    fprintf(perf_file, " Avg Waiting = %.2f\n", avg_wait);
    fprintf(perf_file, "Std WTA = %.2f\n", std_dev);

    fclose(log_file);
    fclose(perf_file);
}