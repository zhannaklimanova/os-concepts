/*
Parent first reads whats on the command line and invokes a separate
child process that performs the command. The parent process waits for
child to exit before continuing. But you can run concurrently with &.
*/

/**
 * @file A1.c
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2022-09-19
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#define SPACE 32
#define MAX_INPUT_ARGS 20
#define MAX_NUM_JOBS 1000
#define TRUE 1
#define FALSE 0
#define NULL_CHARACTER '\0'
#define GET_LENGTH(array) (sizeof(array) / sizeof(*array))
#define PERMS 0666

/* -- Includes -- */
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <stdlib.h>
#include <stdarg.h>
#include <signal.h>
#include <limits.h>
#include <sys/wait.h>
#include <fcntl.h>

/* -- Definitions -- */
int jobsQueue[MAX_NUM_JOBS];
const char *writeFailMsg = "write: failed to write to output\n";
const char *openFileFailMsg = "open: failed to open file\n";
const char *dup2ErrorMessage = "dup2: failed to perform dup2 operation\n";

/* -- Functions -- */
/**
 * @brief performs error checking on input and parses the user command in args 
 *        and returns the number of entries in the array. .
 * 
 * @param prompt 
 * @param args 
 * @param background 
 * @return int 
 */
int getcmd(char *prompt, char *args[], int *background);

/**
 * @brief 
 * 
 * @return int 
 */
void jobs(int jobsQueue[MAX_NUM_JOBS]);

/**
 * @brief 
 * 
 * @return int 
 */
void checkForSignals();

/**
 * @brief 
 * 
 * @return int 
 */
void handle_sigint(int sigNum);

/**
 * @brief 
 * 
 * @return int 
 */
void handle_sigtstp(int sigNum);

/**
 * @brief 
 * 
 */
int echo(char *args[MAX_INPUT_ARGS]);

/**
 * @brief 
 * 
 */
void redirect(char *fileName);

int main(void)
{
    char *args[MAX_INPUT_ARGS];
    // int redirection; // flag for output redirection
    // int piping; // flag for piping
    int bg; // flag for running processes in background
    int cnt; // count the number of arguments in command


    // Clear jobs
    memset(jobsQueue, NULL_CHARACTER, sizeof(jobs));
    checkForSignals();

    while(TRUE)
    {
        // Reset flags
        // redirection = 0;
        // piping = 0;
        bg = 0;

        if (cnt = getcmd("\n>> ", args, &bg) >= 1) {
            int pid = fork();

            if (pid == 0) { // child process
                // if (execvp(args[0], args) < 0) { // execute the command // https://www.digitalocean.com/community/tutorials/execvp-function-c-plus-plus
                //     printf("ERROR: exec failed\n");
                //     exit(1);
                // }
                // exit(0);
                         
                if (strcmp("echo", args[0]) == 0) {
                    echo(args);
                }
                else if (strcmp("cd", args[0]) == 0) {
                    printf("cd\n");
                }
                else if (strcmp("pwd", args[0]) == 0) {
                    printf("pwd\n");
                }
                else if (strcmp("exit", args[0]) == 0) {
                    printf("exit\n");

                }
                else if (strcmp("fg", args[0]) == 0) {
                    printf("fg\n");

                }
                else if (strcmp("jobs", args[0]) == 0) {
                    jobs(jobsQueue);

                }
                else {
                    printf("jk");
                    for (int arg = 0; arg < MAX_INPUT_ARGS; arg++)
                    {
                        if (strcmp(">", args[arg]) == 0) {
                            // char *redirectionArg = args[arg+1];
                            // redirect(redirectionArg);
                        //   printf("%s", args[arg]);
                        
                        // printf("%s", args[3]); // ???????????

                        int fd, fd2;

                        if (fd = open(args[arg+1], O_WRONLY | O_CREAT, PERMS) < 0) {
                            printf("%s", openFileFailMsg);
                        }
                        if (fd2 = dup2(fd, STDOUT_FILENO) < 0) {
                            printf("%s", dup2ErrorMessage);
                        }
                        close(fd);
                        close(fd2);

                        execvp(args[0], args);
                        }
                    }
                }
            }
            else if (pid == -1) {
                printf("ERROR: fork failed\n");
                exit(1);
            }
            else 
            { // parent process pid > 0
                if (bg == 0) { // if the child process is not in bg => wait for it
                    waitpid(pid, NULL, 0);
                }
                else {
                    int jobNum = 0;
                    while (jobNum < MAX_NUM_JOBS) 
                    {
                        if (jobsQueue[jobNum] == NULL_CHARACTER) {
                            jobsQueue[jobNum++] = pid;
                            break;
                        }
                    }
                }
            }
        }
        
    }
    // Free each arg space in args
    for (int i = 0; i < MAX_INPUT_ARGS; i++) free(args[i]);
}

int getcmd(char *prompt, char *args[], int *background)
{
    int length, flag, i = 0;
    char *token, *loc;
    char *line = NULL;
    size_t linecap = 0;

    printf("%s", prompt);
    
    length = getline(&line, &linecap, stdin);
    if (length == 0) { // check for invalid input
        return 0;
    }
    else if (length == -1) { // check for CTRL+D
        exit(0);
    }

    // Check if background is specified
    if ((loc = index(line, '&')) != NULL) {
        *background = 1;
        *loc = ' ';
    } 
    else {
        *background = 0;
    }

    // Clear args
    memset(args, NULL_CHARACTER, MAX_INPUT_ARGS);
    

    // Split the command and insert tokens inside args
    while ((token = strsep(&line, " \t\n")) != NULL && i < MAX_INPUT_ARGS - 1) 
    {
        for (int j = 0; j < strlen(token); j++) 
        {
            if (token[j] <= SPACE) {
                token[j] = NULL_CHARACTER;
            }
        }

        if (strlen(token) > 0) {
            args[i] = calloc(strlen(token)+1, sizeof(*token)); // +1 to account for '\0' termination
            strcpy(args[i], token);
            i++;
        }
    }

    return i;
}

void jobs(int jobsQueue[MAX_NUM_JOBS]) 
{
    printf("Job [#]\t PID\n");
    for (int jobNum = 0; jobNum < MAX_NUM_JOBS; jobNum++)
    {
        if (jobsQueue[jobNum] > 0) {
            printf("[%d]\t %d\n", jobNum, jobsQueue[jobNum]);
        }
        else {
            break;
        }
    }
}

void checkForSignals() 
{
    if (signal(SIGINT, &handle_sigint) == SIG_ERR) {
        printf("ERROR: could not bind signal handler for SIGINT\n");
        exit(1);
    }

    if (signal(SIGTSTP, &handle_sigtstp) == SIG_ERR) {
        printf("ERROR: could not bind signal handler for SIGTSTP\n");
        exit(1);
    }  
}

void handle_sigint(int sigNum)
{
    int pid;
    for (int jobNum = 0; jobNum < MAX_NUM_JOBS; jobNum++)
    {
        if (jobsQueue[jobNum] != NULL_CHARACTER) {
            pid = jobsQueue[jobNum];
            kill(pid, SIGTERM); // SIGTERM instead of SIGKILL. Src: https://linuxhandbook.com/sigterm-vs-sigkill/#:~:text=SIGTERM%20gracefully%20kills%20the%20process,the%20child%20processes%20as%20well.
        }
    }
    exit(0);
}


void handle_sigtstp(int sigNum)
{
    return; 
}

int echo(char *args[MAX_INPUT_ARGS])
{
    int argLength;
    char emptyChar = ' ';
    char newLine = '\n';

    for (int i = 1; i < MAX_INPUT_ARGS; i++)
    {
        if (args[i] != NULL) {
            argLength = strlen(args[i]); 

            if (write(STDOUT_FILENO, args[i], argLength) != argLength) {
                printf("%s", writeFailMsg);

            }
            if (write(STDOUT_FILENO, &emptyChar, sizeof(char)) != sizeof(char)) {
                printf("%s", writeFailMsg);
            }
        }
    }
    if (write(STDOUT_FILENO, &newLine, sizeof(char)) != sizeof(char)) {
        printf("%s", writeFailMsg);
    }
}

void redirect(char *fileName) // https://www.youtube.com/watch?v=5fnVr-zH-SE
{
    // printf("%s", fileName);
    int fd, fd2;

    if (fd = open(*fileName, O_WRONLY | O_CREAT, PERMS) < 0) {
        printf("%s", openFileFailMsg);
    }
    if (fd2 = dup2(fd, STDOUT_FILENO) < 0) {
        printf("%s", dup2ErrorMessage);
    }
    close(fd);
    close(fd2);
}
