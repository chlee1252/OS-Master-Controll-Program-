/*
Author: Marc Lee (Changhwan)
duckid: clee3
Title: CIS415 Project 1 Part5

Part1-This is my own work except getting ideas of tokenize and
fork() concpets in google, stackoverflow
Part2-This is my own work
Part3-This is my own work except decide time quantum (10ms ~ 100ms) from textbook
and Getting idea of setitimer from https://stackoverflow.com/questions/2086126/need-programs-that-illustrate-use-of-settimer-and-alarm-functions-in-gnu-c
circular idea from Textbook
Part4-This is my own work except getting idea of man proc(5) to decide
which information I have to choose.
Part5-This is my own work. But it doesn't seem to work correctly.
*/

#include "mcp.h"
#ifndef PART5_C
#define PART5_C

#define QUANTUM 100            /* 100 Millisecond */

/* These are global Variables */
Program pcb[256];              /* Max Program is 256 */
int run = 0;                   /* Variable for SIGUSR1 Handler */
int argnum = 0;                /* Total number of programs */
pid_t mypid;                   /* store pid by calling getpid() */
int active = 0;                /* Total number of programs that is active */
struct itimerval it_val;       /* Time struct for setitimer() */
int num = 0;                   /* Global Variable for SIGALRM Handler */

/* Handler for SIGUSR1 */
void handler(int signal)
{
    run++;
}

/* Handler for SIGUSR2 */
void handler2(int signal){/* Do Nothing */}

/* Helper function to free Programs 
   input: Program *
   output: None
*/
void freeProg(Program *pcb)
{
    int i;
    for (i = 0; pcb->progs[i] != NULL; i++)
        free(pcb->progs[i]);
    free(pcb->progs);
}

/* Helper function to final tokenize */
/* input: Program*, char* */
/* output: None           */
void tokenize(Program *pcb, char* line)
{
    int s, j;
    int commands = 1;
    for (s = 0; s < strlen(line); s++)
    {
        if (line[s] == ' ')
            commands++;
    }

    char* tok = strtok(line, " ");
    pcb->command = tok;
    pcb->progs = (char **) malloc ((commands+1) * sizeof (char *));
    j = 0;

    while (tok != NULL)
    {
        pcb->progs[j++] = strdup(tok);
        tok = strtok(NULL, " ");
    }
    pcb->progs[j] = NULL;

}

/* Handler for SIGCHLD */
void onchild(int signal)
{
    pid_t pid;
    int status;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
    {
        if (WIFEXITED(status) || WIFSIGNALED(status))
        {
            if (getIndex(pid) == -1)
            {
                fprintf(stderr, "Pid does not exist!");
                exit(EXIT_FAILURE);
            }
            int i = getIndex(pid);
            pcb[i].HasExited = 1;
            printf("PID %u: Done Executing\n", pcb[i].pid);
            active--;
            kill(mypid, SIGUSR2);
        }
    }
}

/*Handler for SIGALRM */
void onalarm(int signal)
{
    sort_and_swap();
    int prev;
    if (num == 0) {
        prev = active-1;
    }
    else {
        prev = num-1;
    }
    /* if previous program has been started, then stop the process */
    if (!pcb[prev].HasExited)
    {
        if (pcb[prev].status == STARTED)
        {
            kill(pcb[prev].pid, SIGSTOP); 
        }
        printProc(&pcb[num]);

    }

    if (!pcb[num].HasExited)
    {
        if (pcb[num].status == READY)
        {
            pcb[num].status = STARTED;
            kill(pcb[num].pid, SIGUSR1);
        } else if (pcb[num].status == STARTED)
        {
            kill(pcb[num].pid, SIGCONT);
        }
    }

    if (num == argnum-1)
        num = 0;
    else
        num++;
}

/* If user time is bigger than other process,
then move process to back, since the CPU bound
has bigger CPU time*/
void sort_and_swap()
{
    int i, j;
    Program tmp;
    for (i = 0; i < argnum; i++)
    {
        for (j = i+1; j < argnum; j++)
        {
            if (pcb[i].usertime > pcb[j].usertime)
            {
                tmp = pcb[i];
                pcb[i] = pcb[j];
                pcb[j] = tmp;
            }
        }
    }
}

/* Print proc information
   1. /proc/[pid]/stat: PID, State, PPID, usrtime, systime
   2. /proc/[pid]/cmdline: args
   3. /proc/[pid]/io: syscr, syscw
*/
void printProc(Program *pcb)
{
    int file, cmdfile, iofile;
    int n, i, total;
    if (pcb->pid == 0) return;
    char buffer[100], cmdbuf[100], iobuf[100];
    char *stats[20], *io[20];
    char *cmd;
    char *print_list[20];

    /* stat file */
    snprintf(buffer, sizeof(buffer), "/proc/%u/stat", pcb->pid);
    file = open(buffer, O_RDONLY);
    if (file == -1) return;
    n = read(file, buffer, sizeof(buffer));
    if (n == 0) return;
    buffer[n-1] = '\0';
    char* token = strtok(buffer, " ");

    i=0;
    while (token != NULL)
    {
        stats[i++] = token;
        token = strtok(NULL, " ");
        total++;
    }
    print_list[0] = stats[0];        //PID
    print_list[1] = stats[2];        //State
    print_list[2] = stats[4];        //PPID
    print_list[3] = stats[14];       //User
    print_list[4] = stats[15];       //Sys

    pcb->usertime = atoi(stats[14]);

    close(file);

    n = 0;
    snprintf(cmdbuf, sizeof(cmdbuf), "/proc/%u/cmdline", pcb->pid);
    cmdfile = open(cmdbuf, O_RDONLY);

    if (cmdfile == -1) return;
    n = 0;
    n = read(cmdfile, cmdbuf, sizeof(cmdbuf));
    if (n == 0) return;
    for (i = 0; i < n; i++)
    {
        if (cmdbuf[i] == '\0')
        {
            if (i+1 == n)
                cmdbuf[i] = '\n';
            else if (cmdbuf[i+1] != '\0')
                cmdbuf[i] = ' ';
        }
    }
    close(cmdfile);
    cmd = strtok(cmdbuf, "\n");
    print_list[5] = cmd;
    
    snprintf(iobuf, sizeof(iobuf), "/proc/%u/io", pcb->pid);
    iofile = open(iobuf, O_RDONLY);

    if (iofile == -1) return;
    n = read(iofile, iobuf, sizeof(iobuf));
    iobuf[n-1] = '\0';
    token = strtok(iobuf, "\n");

    i = 0;
    while (token != NULL)
   {
       io[i++] = token;
       token = strtok(NULL, "\n");
    }

    print_list[6] = io[2];        //syscr
    print_list[7] = io[3];        //syscw

    for (i = 0; i < 8; i++)
    {
        if (i == 0)
            printf("PID: %s", print_list[i]);
        else if (i == 1)
            printf("   State: %s", print_list[i]);
        else if (i == 2)
            printf("   PPID: %s", print_list[i]);
        else if (i == 3)
            printf("   Usertime: %s", print_list[i]);
        else if (i == 4)
            printf("   Systime: %s", print_list[i]);
        else if (i == 5)
            printf("   CMD: %s", print_list[i]);
        else if (i == 6)
            printf("    Sysread: %s", print_list[i]);
        else if (i == 7)
            printf("    Syswrite: %s\n", print_list[i]);
    }
}

/* Dynamic Schedule Algorithm */
void schedule(Program* pcb)
{

}

/* helper function to have index of certain pid 
   input: pid_t
   output: int
*/
int getIndex(pid_t pid)
{
    int i;
    for (i = 0; i < argnum; i++)
    {
        if (pcb[i].pid == pid)
            return i;
    }
    return -1;
}

/* Execute all processes */
void execute(int file)
{
    char buffer[MAX_SIZE];
    int n;

    if (signal(SIGUSR1, handler) == SIG_ERR)
    {
        fprintf(stderr, "Error on make SIGUSR1\n");
        return;

    }

    if (signal(SIGUSR2, handler2) == SIG_ERR)
    {
        fprintf(stderr, "Error on make SIGUSR2\n");
        return;
    }
    mypid = getpid();

    if (signal(SIGCHLD, onchild) == SIG_ERR)
    {
        fprintf(stderr, "Error on make SIGCHLD\n");
        return;
    }

    if (signal(SIGALRM, onalarm) == SIG_ERR)
    {
        fprintf(stderr, "Error on make SIGALRM\n");
        return;
    }
    while ((n = read(file, buffer, MAX_SIZE)) > 0)
    {
        int i, j;
        char *token;
        char *args[128];
        
        buffer[n] = '\0';
        token = strtok(buffer, "\n");
        j = 0;

        while (token != NULL)
        {
            args[j++] = strdup(token);
            token = strtok(NULL, "\n");
            argnum++;
        }

        if (argnum == 256)
        {
            fprintf(stderr, "Too Many processes at the same time! Max 256\n");
            exit(EXIT_FAILURE);
        }

        for (i = 0; i < argnum; i++)
        {
            tokenize(&pcb[i], args[i]);

            pcb[i].pid = fork();
            if (pcb[i].pid == 0)
            {
                while(run == 0)
                {
                    usleep(1);
                }
                execvp(pcb[i].command, pcb[i].progs);
                freeProg(&pcb[i]);
                //freeing args
                for (i = 0; i < argnum; i++)
                {
                    free(args[i]);
                }
                fprintf(stderr, "Error with execvp! PID: %u\n", pcb[i].pid);
                exit(EXIT_FAILURE);
            }
            else
            {
                if (pcb[i].pid < 0)
                {
                    fprintf(stderr, "FORK ERROR: PID: %u\n", pcb[i].pid);
                    exit(EXIT_FAILURE);
                }
                else
                {
                    pcb[i].status = READY;
                    pcb[i].HasExited = 0;
                    pcb[i].usertime = 0;
                    freeProg(&pcb[i]);
                }
                
            }
        }
        active = argnum;
        for (i = 0; i < argnum; i++)
        {
            printProc(&pcb[i]);
        }
        /* Set Timer for millisecond. Used alarm() with time 1 sec and not concurrent*/
        /* Information from stackoverflow link provided at the top */
        /* Accroding to OSE book, best time quantum is 10ms ~ 100ms */
        /* alarm cannot take milisecond */
        it_val.it_value.tv_sec = QUANTUM/1000;
        it_val.it_value.tv_usec = (QUANTUM*1000) % 1000000;
        it_val.it_interval = it_val.it_value;

        if (setitimer(ITIMER_REAL, &it_val, NULL) == -1)
        {
            fprintf(stderr, "Error calling setitimer()\n");
            exit(EXIT_FAILURE);
        }
        /* Done setitimer */

        onalarm(SIGALRM);

        /* pause until there is no active process */
        while (active > 0)
        {
            pause();
        }
        
        for (i = 0; i < argnum; i++)
        {
            free(args[i]);
        }

    }   
}
#endif

int main(int argc, char** argv)
{
    int file;

    if (argc > 2)
    {
        fprintf(stderr, "Usage: %s [wordload_file]\n", *argv);
        exit(EXIT_FAILURE);
    }

    file = open(argv[1], O_RDONLY);

    if (file == -1)
    {
        fprintf(stderr, "NO FILE TO READ!\n");
        exit(EXIT_FAILURE);
    }

    execute(file);
    
    close(file);
    printf("=======================\n");
    printf("OS Successfully Exited!\n");
    printf("=======================\n");
    return 0;
}