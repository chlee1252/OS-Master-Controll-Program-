/*
Author: Marc Lee (Changhwan)
duckid: clee3
Title: CIS415 Project 1 Part2

Part1-This is my own work except getting ideas of tokenize and
fork() concpets in google, stackoverflow
Part2-This is my own work
*/

#include "mcp.h"
#ifndef PART2_C
#define PART2_C

/* These are global Variables */
Program pcb[256];              /* Max Program is 256 */
int run = 0;                   /* Variable for SIGUSR1 Handler */


/* Handler for SIGUSR1 */
void handler(int signal)
{
    run++;
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

/* Execute all processes */
void execute(int file)
{
    char buffer[MAX_SIZE];
    int n;

    if (signal(SIGUSR1, handler) == SIG_ERR)
    {
        fprintf(stderr, "Error on make SIGUSR1");
        return;

    }
    while ((n = read(file, buffer, MAX_SIZE)) > 0)
    {
        int i, j;
        int argnum = 0;
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
                    freeProg(&pcb[i]);
                }
                
            }
        }
        for (i = 0; i < argnum; i++)
        {
            kill(pcb[i].pid, SIGUSR1);
        }
        for (i = 0; i < argnum; i++)
        {
            kill(pcb[i].pid, SIGSTOP);
        }

        for (i = 0; i < argnum; i++)
        {
            kill(pcb[i].pid, SIGCONT);
        }
        
        for (i = 0; i < argnum; i++)
        {
            wait(&pcb[i].pid);
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