#include "circQ.h"
#include <math.h>

void RR(int msg_id, int sem_id, int total_processes, int quantum) {
    CircQ *q = initCircQ(100);
    PCB running_process;
    int finished_processes = 0;
    bool isRunning = false;
    FILE *log_file = fopen("scheduler.log", "w");

    int process_sem_id = semget(PROC_SEM_KEY, 1, IPC_CREAT | 0666);
    if (process_sem_id == -1) { perror("semget failed"); exit(-1); }
    union Semun sem_un;
    sem_un.val = 0;
    semctl(process_sem_id, 0, SETVAL, sem_un);

    float total_wta = 0;
    float total_waiting = 0;
    float total_wta_square = 0;
    int total_runtime = 0;
    int current_time = getClk();
    int last_time = -1;
    int context_switch = 0;
    int quantum_counter = 0;

    bool pending_quantum_expire = false;
    PCB expiring_process;

    while (finished_processes < total_processes) {
        current_time = getClk();
        if (current_time > last_time) {
            down(sem_id);
            if (context_switch > 0) {
                context_switch--;
            } else if (isRunning) {
                running_process.remainingTime--;
                quantum_counter++;
                total_runtime++;
                up(process_sem_id);
                if (running_process.remainingTime == 0) {
                    int ta = current_time - running_process.arrivalTime;
                    float wta = (float)ta / running_process.runTime;
                    total_wta += wta;
                    total_wta_square += wta * wta;
                    total_waiting += running_process.waitingTime;
                    fprintf(log_file, "At time %d process %d finished arr %d total %d remain 0 wait %d TA %d WTA %.2f\n",
                            current_time, running_process.id, running_process.arrivalTime, running_process.runTime,
                            running_process.waitingTime, ta, wta);
                    isRunning = false;
                    context_switch = 1;
                    finished_processes++;
                    quantum_counter = 0;
                } else if (quantum_counter == quantum) {
                    quantum_counter = 0;
                    expiring_process = running_process;
                    expiring_process.state = 'S';
                    expiring_process.lastRun = current_time;
                    enqueueCircQ(q, expiring_process);
                    isRunning = false;
                    pending_quantum_expire = true;
                }
            }
            last_time = current_time;
        }

        ProcessMsg message;

        while(msgrcv(msg_id, &message, sizeof(message) - sizeof(long), PROCESS_MSG_TYPE, IPC_NOWAIT)!=-1){
            PCB new_process;
            new_process.id = message.id;
            new_process.arrivalTime = message.arrival;
            new_process.runTime = message.runtime;
            new_process.priority = message.priority;
            new_process.remainingTime = new_process.runTime;
            new_process.state = 'W';
            new_process.waitingTime = 0;
            enqueueCircQ(q, new_process);
        }

        if (pending_quantum_expire) {
            if (q->size > 1) {
                kill(expiring_process.pid, SIGSTOP);
                fprintf(log_file, "At time %d process %d stopped arr %d total %d remain %d wait %d\n",
                        current_time, expiring_process.id, expiring_process.arrivalTime, expiring_process.runTime,
                        expiring_process.remainingTime, expiring_process.waitingTime);
                context_switch = 1;
            } else {
                running_process = dequeueCircQ(q);
                running_process.state = 'R';
                isRunning = true;
            }
            pending_quantum_expire = false;
        }

        if (q->size > 0 && !isRunning && context_switch == 0) {
            running_process = dequeueCircQ(q);
            quantum_counter = 0;
            if (running_process.state == 'W' && running_process.remainingTime == running_process.runTime) {
                int pid = fork();
                if (pid == 0) {
                    char remStr[10], semStr[10];
                    sprintf(remStr, "%d", running_process.remainingTime);
                    sprintf(semStr, "%d", process_sem_id);
                    execl("./process.out", "process.out", remStr, semStr, NULL);
                    perror("execl process.out failed");
                    exit(-1);
                } else {
                    running_process.startTime = current_time;
                    running_process.pid = pid;
                    running_process.state = 'R';
                    running_process.waitingTime += current_time - running_process.arrivalTime;
                    fprintf(log_file, "At time %d process %d started arr %d total %d remain %d wait %d\n",
                            current_time, running_process.id, running_process.arrivalTime, running_process.runTime,
                            running_process.remainingTime, running_process.waitingTime);
                    isRunning = true;
                }
            } else if (running_process.state == 'S') {
                kill(running_process.pid, SIGCONT);
                running_process.waitingTime += current_time - running_process.lastRun;
                running_process.state = 'R';
                fprintf(log_file, "At time %d process %d resumed arr %d total %d remain %d wait %d\n",
                        current_time, running_process.id, running_process.arrivalTime, running_process.runTime,
                        running_process.remainingTime, running_process.waitingTime);
                isRunning = true;
            }
        }
    }

    FILE *perf_file = fopen("scheduler.perf", "w");
    float avg_wta = total_wta / total_processes;
    float avg_wait = total_waiting / total_processes;
    float std_dev = sqrt((total_wta_square / total_processes) - (avg_wta * avg_wta));
    float cpu_util = ((float)total_runtime / getClk()) * 100;
    fprintf(perf_file, "CPU utilization = %.2f%%\n", cpu_util);
    fprintf(perf_file, "Avg WTA = %.2f\n", avg_wta);
    fprintf(perf_file, "Avg Waiting = %.2f\n", avg_wait);
    fprintf(perf_file, "Std WTA = %.2f\n", std_dev);
    semctl(process_sem_id, 0, IPC_RMID);
    fclose(log_file);
    fclose(perf_file);
}