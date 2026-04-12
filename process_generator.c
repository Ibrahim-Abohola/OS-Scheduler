#include "DataStructures.h"

#define MAX_PROCESSES 1000

PCB processes[MAX_PROCESSES];
int process_count = 0;
int msgqid = -1;

void clearResources(int signum);

int main(int argc, char *argv[])
{
    signal(SIGINT, clearResources);

    //1
    FILE *f = fopen("processes.txt", "r");
    if (f == NULL) {
        perror("Cannot open processes.txt — run test_generator first!");
        exit(-1);
    }

    char line[256];
    while (fgets(line, sizeof(line), f))
    {
        if (line[0] == '#') continue; // skip comment lines

        PCB p = {0}; // zero-initialize
        if (sscanf(line, "%d\t%d\t%d\t%d",
                   &p.id, &p.arrivalTime, &p.runTime, &p.priority) == 4)
        {
            p.remainingTime = p.runTime;
            p.state = 'W'; // starts as waiting
            processes[process_count++] = p;
        }
    }
    fclose(f);
    printf("Read %d processes.\n", process_count);

    //2
    int algo = 0, quantum = 0, N = 0, M = 0;

    printf("\nChoose scheduling algorithm:\n");
    printf("  1 = Preemptive Highest Priority First (HPF)\n");
    printf("  2 = Round Robin (RR)\n");
    printf("  3 = 2-CPU FCFS with work stealing\n");
    printf("Choice: ");
    scanf("%d", &algo);

    if (algo == 2) {
        printf("Enter quantum: ");
        scanf("%d", &quantum);
    }
    if (algo == 3) {
        printf("Enter N (check stealing every N cycles): ");
        scanf("%d", &N);
        printf("Enter M (stealing threshold): ");
        scanf("%d", &M);
    }

    //3
    pid_t clk_pid = fork();
    if (clk_pid == -1) { perror("fork clock failed"); exit(-1); }
    if (clk_pid == 0) {
        execl("./clk.out", "clk.out", NULL);
        perror("execl clock failed");
        exit(-1);
    }

    //4
    char algo_s[10], quantum_s[10], N_s[10], M_s[10], total_s[10];
    sprintf(algo_s,    "%d", algo);
    sprintf(quantum_s, "%d", quantum);
    sprintf(N_s,       "%d", N);
    sprintf(M_s,       "%d", M);
    sprintf(total_s,   "%d", process_count); // scheduler needs total to know when to stop

    pid_t sch_pid = fork();
    if (sch_pid == -1) { perror("fork scheduler failed"); exit(-1); }
    if (sch_pid == 0) {
        execl("./scheduler.out", "scheduler.out",
              algo_s, quantum_s, N_s, M_s, total_s, NULL);
        perror("execl scheduler failed");
        exit(-1);
    }

    //5
    initClk();

    //6
    msgqid = msgget(MSGKEY, IPC_CREAT | 0666);
    if (msgqid == -1) { perror("msgget failed"); exit(-1); }
    printf("Message queue created. ID = %d\n", msgqid);

    //7
    int sent = 0;
    int prev_clk = -1;

    while (sent < process_count)
    {
        int now = getClk();

        if (now == prev_clk) {
            usleep(100000); // sleep 0.1s to avoid busy spin
            continue;
        }
        prev_clk = now;

        for (int i = 0; i < process_count; i++)
        {
            if (processes[i].arrivalTime != now) continue;

            struct msg message;
            message.mtype   = 1;            // mtype must be > 0
            message.process = processes[i]; // copy full PCB

            if (msgsnd(msgqid, &message, sizeof(message.process), 0) == -1) {
                perror("msgsnd failed");
            } else {
                printf("Sent process %d to scheduler at time %d\n", processes[i].id, now);
                sent++;
            }
        }
    }

    printf("All %d processes sent. Waiting for scheduler...\n", process_count);
    
    waitpid(sch_pid, NULL, 0);

    clearResources(0);
    return 0;
}

void clearResources(int signum)
{
    if (msgqid != -1) {
        msgctl(msgqid, IPC_RMID, NULL); // delete message queue
        printf("Message queue removed.\n");
    }
    destroyClk(true);
}