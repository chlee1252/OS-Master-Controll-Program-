/*
Author: Marc Lee (Changhwan)
duckid: clee3
Title: CIS415 Project 1 Part3

Part1-This is my own work except getting ideas of tokenize and
fork() concpets in google, stackoverflow
Part2-This is my own work
Part3-This is my own work except decide time quantum from textbook
and Getting idea of setitimer from https://stackoverflow.com/questions/2086126/need-programs-that-illustrate-use-of-settimer-and-alarm-functions-in-gnu-c
circular idea from Textbook
*/

#include "mcp.h"
#ifndef PART3_C
#define PART3_C

#define QUANTUM 100                   //100 milliseconds

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
void handler2(int signal){/*Do Nothing*/}

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
            pcb[getIndex(pid)].HasExited = 1;
            active--;
            kill(mypid, SIGUSR2);
        }
    }
}

/*Handler for SIGALRM */
void onalarm(int signal)
{
    int prev;
    if (num == 0) {
        prev = active-1;
    }
    else {
        prev = num-1;
    }

    //if prev pcb[prev] == Started then stop
    if (!pcb[prev].HasExited)
    {
        if (pcb[prev].status == STARTED)
            kill(pcb[prev].pid, SIGSTOP);
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

void execute(int file)
{
    char buffer[MAX_SIZE];
    int n;

    //if child process got SIGUSR1, do handler, but
    //if signal() return SIG_ERR, then exit program
    if (signal(SIGUSR1, handler) == SIG_ERR)
    {
        fprintf(stderr, "Error on make SIGUSR1");
        return;

    }

    if (signal(SIGUSR2, handler2) == SIG_ERR)
    {
        fprintf(stderr, "Error on make SIGUSR2");
        return;
    }
    mypid = getpid();

    if (signal(SIGCHLD, onchild) == SIG_ERR)
    {
        fprintf(stderr, "Error on make SIGCHLD");
        return;
    }

    if (signal(SIGALRM, onalarm) == SIG_ERR)
    {
        fprintf(stderr, "Error on make SIGALRM");
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
                fprintf(stderr, "Error with execvp!\n");
                exit(EXIT_FAILURE);
            }
            else
            {
                if (pcb[i].pid < 0)
                {
                    fprintf(stderr, "FORK ERROR\n");
                    exit(EXIT_FAILURE);
                }
                else
                {
                    pcb[i].status = READY;
                    pcb[i].HasExited = 0;
                    freeProg(&pcb[i]);
                }
                
            }
        }
        active = argnum;

        /* Set Timer for millisecond. Used alarm() with time 1 sec and not concurrent*/
        /* Information from stackoverflow link provided at the top */
        /* Accroding to OSE book, best time quantum is 10ms ~ 100ms */
        /* alarm cannot take milisecond */
        it_val.it_value.tv_sec = QUANTUM/1000;
        it_val.it_value.tv_usec = (QUANTUM*1000) % 1000000;
        it_val.it_interval = it_val.it_value;

        if (setitimer(ITIMER_REAL, &it_val, NULL) == -1)
        {
            fprintf(stderr, "Error calling setitimer()");
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