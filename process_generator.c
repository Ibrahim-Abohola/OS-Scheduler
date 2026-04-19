#include "DataStructures.h"
#include "headers.h"
#define MAX_PROCESSES 1000

typedef struct {
    int arrivaltime;
    int priority;
    int runningtime;
    int id;
} processData;


processData processes[MAX_PROCESSES];
int process_count = 0;
int msgqid = -1;
int msgqid1 = -1;
int msgqid2 = -1; 
int sem_id = -1;

void clearResources(int signum);

int main(int argc, char *argv[])
{
    signal(SIGINT, clearResources);

    FILE *f = fopen("processes.txt", "r");
    if (f == NULL) {
        perror("Cannot open processes.txt — run test_generator first!");
        exit(-1);
    }

    char line[256];
    while (fgets(line, sizeof(line), f))
    {
        if (line[0] == '#') continue;

        processData p = {0}; 
        if (sscanf(line, "%d\t%d\t%d\t%d",
                   &p.id, &p.arrivaltime, &p.runningtime, &p.priority) == 4)
        {
            processes[process_count++] = p;
        }
    }
    fclose(f);
    printf("Read %d processes.\n", process_count);

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

    key_t key1 = ftok("keyfile", MSGKEY1);
    key_t key2 = ftok("keyfile", MSGKEY2);
    if(key1 == -1 || key2 == -1) { perror("ftok failed"); exit(-1); }
    msgqid1 = msgget(key1, IPC_CREAT | 0666);
    msgqid2 = msgget(key2, IPC_CREAT | 0666);
    if (msgqid1 == -1 || msgqid2 == -1) { perror("msgget failed"); exit(-1); }
    msgqid = msgqid1;


    sem_id = semget(SEMKEY, 1, IPC_CREAT | 0666);
    if(sem_id == -1) { perror("semget failed"); exit(-1); }
    union Semun sem_un;
    sem_un.val = 0;
    semctl(sem_id, 0, SETVAL, sem_un);
   

   
    pid_t clk_pid = fork();
    if (clk_pid == -1) { perror("fork clock failed"); exit(-1); }
    if (clk_pid == 0) {
        execl("./clk.out", "clk.out", NULL);
        perror("execl clock failed");
        exit(-1);
    }
  
   
    initClk();            

    char algo_s[10], quantum_s[10], N_s[10], M_s[10], total_s[10];
    sprintf(algo_s,    "%d", algo);
    sprintf(quantum_s, "%d", quantum);
    sprintf(N_s,       "%d", N);
    sprintf(M_s,       "%d", M);
    sprintf(total_s,   "%d", process_count);

   pid_t sch1_pid = -1, sch2_pid = -1;

    sch1_pid = fork();
    if(sch1_pid == -1) { perror("fork scheduler1"); exit(-1); }
    if(sch1_pid == 0) {
        if(algo == ALGO_HPF)
            execl("./scheduler.out","scheduler.out","1",algo_s,total_s,NULL);
        else if(algo == ALGO_RR)
            execl("./scheduler.out","scheduler.out","1",algo_s,total_s,quantum_s,NULL);
        else if(algo == ALGO_FCFS_2CPUS)
            execl("./scheduler.out","scheduler.out","1",algo_s,total_s,N_s,M_s,NULL);
        perror("execl sch1"); exit(1);
    }

    if(algo == ALGO_FCFS_2CPUS) {
        sch2_pid = fork();
        if(sch2_pid == -1) { perror("fork scheduler2"); exit(-1); }
        if(sch2_pid == 0) {
            execl("./scheduler.out","scheduler.out","2",algo_s,total_s,N_s,M_s,NULL);
            perror("execl sch2"); exit(1);
        }
    }
   
    int sent = 0;
    int prev_clk = -1;
    int q1_count = 0, q2_count = 0;
    bool end_sent = false;
    bool s1_done = false, s2_done = false;
    while(true) {
        int now = getClk();
        if(now == prev_clk)  continue; 
        prev_clk = now;

        for(int i = 0; i < process_count; i++) {
            if(processes[i].arrivaltime != now) continue;

            ProcessMsg message;
            message.mtype    = PROCESS_MSG_TYPE;
            message.id       = processes[i].id;
            message.arrival  = processes[i].arrivaltime;
            message.runtime  = processes[i].runningtime;
            message.priority = processes[i].priority;

            if(algo == ALGO_FCFS_2CPUS) {
                if(q1_count <= q2_count) {
                    msgsnd(msgqid1, &message, sizeof(ProcessMsg)-sizeof(long), 0);
                    printf("Sent process %d at time %d to CPU1\n", processes[i].id, now);
                    q1_count++;
                } else {
                    msgsnd(msgqid2, &message, sizeof(ProcessMsg)-sizeof(long), 0);
                    printf("Sent process %d at time %d to CPU2\n", processes[i].id, now);
                    q2_count++;
                }
            } else {
                msgsnd(msgqid1, &message, sizeof(ProcessMsg)-sizeof(long), 0);
                printf("Sent process %d at time %d\n", processes[i].id, now);
            }
            sent++;
        }

        
        up(sem_id);
        if(algo == ALGO_FCFS_2CPUS) up(sem_id);  
        int sem_val = semctl(sem_id, 0, GETVAL);
        if(sent >= process_count && !end_sent) {
            end_sent = true;
            ProcessMsg end_msg;
            end_msg.mtype    = END_OF_STREAM_MSG_TYPE;
            end_msg.id       = -1;
            end_msg.arrival  = -1;
            end_msg.runtime  = -1;
            end_msg.priority = -1;

            if(algo == ALGO_FCFS_2CPUS) {
                msgsnd(msgqid1, &end_msg, sizeof(ProcessMsg)-sizeof(long), 0);
                msgsnd(msgqid2, &end_msg, sizeof(ProcessMsg)-sizeof(long), 0);
            } else {
                msgsnd(msgqid1, &end_msg, sizeof(ProcessMsg)-sizeof(long), 0);
            }
        }

        if(end_sent) {
            if(algo == ALGO_FCFS_2CPUS) { 
                s1_done = s1_done || waitpid(sch1_pid, NULL, WNOHANG) > 0;
                s2_done = s2_done || waitpid(sch2_pid, NULL, WNOHANG) > 0;
                if(s1_done && s2_done) break;
                }
            else {
                if(waitpid(sch1_pid, NULL, WNOHANG) > 0)
                    break;
            }
        }
    }
    printf("At time %d: All processes sent and schedulers finished. Cleaning up resources...\n", getClk());
    clearResources(0);
    return 0;
}

void clearResources(int signum)
{
    signal(SIGINT, SIG_DFL); 
    if (msgqid1 != -1) {
        msgctl(msgqid1, IPC_RMID, NULL); 
        printf("Message queue 1 removed.\n");
    }
    if (msgqid2 != -1) {
        msgctl(msgqid2, IPC_RMID, NULL); 
        printf("Message queue 2 removed.\n");
    }
    
    destroyClk(true);
}