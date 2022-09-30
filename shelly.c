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

#define SPACE_NON_PRINTABLE_CHARS_DIVIDER 32
#define MAX_INPUT_ARGS 1000
#define MAX_NUM_JOBS 1000
#define TRUE 1
#define FALSE 0
#define NULL_CHARACTER '\0'
#define EXIT_SUCCESSFUL 0
#define WFORFINISH 0
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
const char *writeFailMsg = "write: write fail\n";
const char *openFileFailMsg = "open: failed to open file\n";
const char *dup2ErrorMessage = "dup2: failed to perform dup2 operation\n";
int jobsQueue[MAX_NUM_JOBS];
int bg = 0;
int fgVal = -1; 

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
int getcmd(char *prompt, char *args[]);
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
void echo_shelly(char *args[MAX_INPUT_ARGS]);
/**
 * @brief 
 * 
 */
void cd_shelly(char *args[MAX_INPUT_ARGS]);
/**
 * @brief 
 * 
 * @return int 
 */
void pwd_shelly();
/**
 * @brief 
 * 
 * @return int 
 */
void exit_shelly();
/**
 * @brief 
 * 
 * @return int 
 */
void fg_shelly(char *args[MAX_INPUT_ARGS]);
/**
 * @brief 
 * 
 * @return int 
 */
void jobs_shelly();
/**
 * @brief 
 * 
 */
void removeFinishedJobs();
void kill_fg(int sig);

int main(void)
{
    printf("%s\n", "Mini shell version <shelly>. Created September 2022.");
    
    char *args[MAX_INPUT_ARGS];
    int redirection; // flag for output redirection
    int piping; // flag for piping
    int cnt; // count the number of arguments in command

    // Clear jobs
    memset(jobsQueue, NULL_CHARACTER, MAX_NUM_JOBS);
    checkForSignals();

    while(TRUE)
    {
        bg = 0;
        fgVal = -1;

        checkForSignals();
        removeFinishedJobs(); // so that we don't display jobs that are done/dead
        printf("%d ", bg);

        if ((cnt = getcmd("\nshelly >> ", args)) > 0) {
            // Built-in commands
            
            if (strcmp("echo", args[0]) == 0) {
                echo_shelly(args);
                printf("in echo");
            }
            else if (strcmp("cd", args[0]) == 0) {
                printf("%d", cnt);
                cd_shelly(args);
                printf("in cd");
            }
            else if (strcmp("pwd", args[0]) == 0) {
                pwd_shelly();
                printf("in pwd");
            }
            else if (strcmp("exit", args[0]) == 0) {
                exit_shelly(EXIT_SUCCESSFUL);
                printf("in exit");
            }
            else if (strcmp("fg", args[0]) == 0) { // broken ???
                fg_shelly(args);
            }
            else if (strcmp("jobs", args[0]) == 0) {
                jobs_shelly();
            }
            else {
                // External commands
                int pid = fork();
                    if (pid == 0) {
                            // Child           
                    // if (strcmp("echo", args[0]) == 0) {
                    //     echo_shelly(args);
                    //     printf("in echo");
                    // }
                    // else if (strcmp("cd", args[0]) == 0) {
                    //     printf("%d", cnt);
                    //     cd_shelly(args);
                    //     printf("in cd");
                    // }
                    // else if (strcmp("pwd", args[0]) == 0) {
                    //     pwd_shelly();
                    //     printf("in pwd");
                    // }
                    // else if (strcmp("exit", args[0]) == 0) {
                    //     exit_shelly(EXIT_SUCCESSFUL);
                    //     printf("in exit");
                    // }
                    // else if (strcmp("fg", args[0]) == 0) { // broken ???
                    //     fg_shelly(args);
                    // }
                    // else if (strcmp("jobs", args[0]) == 0) {
                    //     jobs_shelly();
                    // }
                    // else {
                    for (int argNum = 0; argNum < MAX_INPUT_ARGS; argNum++) 
                    {
                        if (strcmp(">", args[argNum]) == 0) {
                            printf("in >"); // why doesn't print?
                            int fd;
                            
                            // Have output be redirected to file instead of STDOUT
                            // if (fd = open(args[argNum+1], O_WRONLY | O_CREAT | O_TRUNC, PERMS) < 0) {
                            //     printf("%s", openFileFailMsg);
                            // }
                            // if (dup2(fd, STDOUT_FILENO) < 0) {///  Why this doesn't work????
                            //     printf("%s", dup2ErrorMessage);
                            // }
                            fd = open(args[argNum+1], O_WRONLY | O_CREAT | O_TRUNC, PERMS);
                            dup2(fd, STDOUT_FILENO);
                            args[argNum] = NULL_CHARACTER;
                            args[argNum+1] = NULL_CHARACTER;

                            execvp(args[0], args);
                        }       
                        else if (strcmp("|", args[argNum]) == 0) {
                            printf("in |");

                            args[argNum] = NULL_CHARACTER;

                            int fd[2];
                            if (pipe(fd) == -1) return 1;

                            pid = fork();
                            if (pid > 0) {
                                close(fd[1]);
                                dup2(fd[0], STDIN_FILENO);
                                execvp(args[argNum + 1], &args[argNum + 1]);
                            } 
                            else {
                                close(fd[0]);
                                dup2(fd[1], STDOUT_FILENO);
                                execvp(args[0], &args[0]);
                            }

                        }
                        // else {
                        //     printf("some other ext command");
                        //     execvp(args[0], &args[0]);
                        // }
                    }
                    // execvp(args[0], &args[0]);
                    // // }
                }
                else if (pid == -1) {      
                    printf("ERROR: fork failed\n");
                    exit(1);
                }
                else {
                    // Parent
                    if (bg == 0) {  // if the child process is not in the bg => wait for it
                        waitpid(pid, NULL, WFORFINISH);
                    }
                    else { // add jobs to the background
                        printf("%d", bg);
                        for (int jobNum = 0; jobNum < MAX_NUM_JOBS; jobNum++) {
                            if (jobsQueue[jobNum] <= NULL_CHARACTER) {
                                jobsQueue[jobNum] = pid;
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
    // Free each arg space in args because calloc was used before
    // DOUBLE CHECK IF THIS IS THE RIGHT PLACE TO PUT IT???
    for (int i = 0; i < MAX_INPUT_ARGS; i++) free(args[i]);
}

int getcmd(char *prompt, char *args[])
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
        printf("\n");
        exit(0);
    }

    // Check if background is specified
    if ((loc = index(line, '&')) != NULL) {
        bg = 1;
        *loc = ' ';
        printf("%d", bg);
    } 
    else {
        bg = 0;
    }

    // Clear args
    memset(args, NULL_CHARACTER, MAX_INPUT_ARGS);

    // Split the command and insert tokens inside args
    while ((token = strsep(&line, " \t\n")) != NULL && i < MAX_INPUT_ARGS - 1) 
    {
        for (int j = 0; j < strlen(token); j++) 
        {
            if (token[j] <= SPACE_NON_PRINTABLE_CHARS_DIVIDER) {
                token[j] = NULL_CHARACTER;
            }
        }

        if (strlen(token) > 0) {
            // args[i] = calloc(strlen(token)+1, sizeof(*token)); // +1 to account for '\0' termination
            args[i] = malloc((strlen(token)+1) * sizeof(*token)); // +1 to account for '\0' termination
            strcpy(args[i], token);
            i++;
        }
    }
    // Args are now filled up with the new input arguments

    return i;
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
{ // HERE ITS ONLY SUPPOSED TO EXIT IF ITS RUNNING A child PROCESS ????
    // if (bg == 1) { // with this it clears all the jobs
    //     exit_shelly(EXIT_SUCCESSFUL); // if not in normal shell
    //     bg = 0;
    // }
    printf("\n");
    printf("To kill shelly, press CTRL+D. Otherwise, press Enter to continue...");
    return;
}


void handle_sigtstp(int sigNum)
{
    return; 
}

void echo_shelly(char *args[MAX_INPUT_ARGS])
{
    int argLength;
    const char emptyChar = ' ';
    const char newLine = '\n';

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
        else {
            break;
        }
    }
    if (write(STDOUT_FILENO, &newLine, sizeof(char)) != sizeof(char)) {
        printf("%s", writeFailMsg);
    }
}

void cd_shelly(char *args[MAX_INPUT_ARGS])
{
    if (args[1] != NULL) {
        if (chdir(args[1]) != 0) {
            printf("chdir to %s failed \n", args[1]);
        }
    } 
    else {
        pwd_shelly();
    }
}

void pwd_shelly()
{
    char cwd[PATH_MAX];
    int argLength;
    char newLine = '\n';

    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        argLength = strlen(cwd);
        if (write(STDOUT_FILENO, cwd, argLength) && write(STDOUT_FILENO, &newLine, sizeof(char)) != sizeof(char)) {
           printf("%s", writeFailMsg);
        }
    }
}

void exit_shelly(int code) 
{
    int pid;

    for (int jobNum = 0; jobNum < MAX_NUM_JOBS; jobNum++)
    {
        if (jobsQueue[jobNum] != NULL_CHARACTER) {
            pid = jobsQueue[jobNum];
            kill(pid, SIGTERM); // SIGTERM instead of SIGKILL. Src: https://linuxhandbook.com/sigterm-vs-sigkill/#:~:text=SIGTERM%20gracefully%20kills%20the%20process,the%20child%20processes%20as%20well.
        }
    }
    exit(code);
}

void jobs_shelly()
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

void removeFinishedJobs() {
    for (int i = 0; i < MAX_NUM_JOBS; ++i) {
        if (jobsQueue[i] > NULL_CHARACTER) {
            if (waitpid(jobsQueue[i], NULL, WNOHANG) > 0) { // WNOHANG: allow process to move on to other tasks
                jobsQueue[i] = 0;
            }
        }
    }
}

void fg_shelly(char *args[MAX_INPUT_ARGS])
{
    int jobNum;
    if (args[1] > 0) {
        jobNum = atoi(args[1]);

        if (0 <= jobNum && jobNum < MAX_NUM_JOBS) {
            fgVal = jobsQueue[jobNum];
        }
        else {
            printf("ERROR: invalid Job [#]. Taking most recent job.\n");
        }
    }
    else {
        for (int jobNum = 0; jobNum < MAX_NUM_JOBS; jobNum++) 
        {
            if (jobsQueue[jobNum] == NULL_CHARACTER) {
                fgVal = jobsQueue[jobNum-1]; // take the pid of the last job in list
                break;
            }
        }
    }
    printf("%d", fgVal);
    if (fgVal != -1) {
        // kill(fgVal, SIGTERM);
        if (signal(SIGINT, kill_fg) == SIG_ERR) {
            printf("error");
            exit(1);
        }
        waitpid(fgVal, NULL, WFORFINISH); // put selected job into forground
        jobsQueue[jobNum] = NULL_CHARACTER;
        if (signal(SIGINT, handle_sigint) == SIG_ERR) {
            printf("error");
            exit(1);
        }
    }
    
}
void kill_fg(int sig) { kill(fgVal, SIGTERM); }
