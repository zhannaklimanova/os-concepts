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
#define MAX_INPUT_ARGS 5
#define MAX_NUM_JOBS 5
#define TRUE 1
#define FALSE 0
#define NULL_CHARACTER '\0'
#define EXIT_SUCCESSFUL 0
#define WFORFINISH 0
#define PERMS 0666
#define FG_PID_DEFAULT INT_MIN

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
#include <assert.h>

/* -- Definitions -- */
const char *writeFailMsg = "write: write fail.\n";
const char *openFileFailMsg = "open: failed to open file.\n";
const char *execErrorMessage = "execvp: invalid command.\n";
const char *pipeErrorMessage = "pipe: error occured with opening the pipe.\n";
const char *closeErrorMessage = "close: error closing the file pointed to by file descriptor.\n";
const char *forkErrorMessage = "fork: process forking failed.\n";
const char *dup2ErrorMessage = "dup2: error creating copy of file descriptor.\n";
int jobsQueue[MAX_NUM_JOBS];
int bg = 0;
pid_t fgPid = FG_PID_DEFAULT;

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
 */
void freeArgs(char *args[], int numArgs);
/**
 * @brief 
 * 
 */
void exec_shelly(const char *cmd, char *const cmdArgs[]);
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
 */
void addNewJob(int pid);
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
/**
 * @brief 
 * 
 */
void checkForRedirectionOrPiping(char *args[MAX_INPUT_ARGS], int inputArgs, int *redirectionFlag, int *pipingFlag);
void kill_fg(int sig);
/**
 * @brief 
 * 
 * @return int 
 */
void redirection_shelly(char *args[MAX_INPUT_ARGS], int redirectionIndex);
/**
 * @brief 
 * 
 * @return int 
 */
void piping_shelly(char *args[MAX_INPUT_ARGS], int redirectionIndex);

int main(void)
{
    printf("%s\n", "Mini shell version <shelly>. Created September 2022.");
    
    char *args[MAX_INPUT_ARGS];
    int cnt; // count the number of arguments in command
    int redirection; /* flag for output redirection */
    int piping; /* flag for piping */

    // Clear jobs
    memset(jobsQueue, NULL_CHARACTER, MAX_NUM_JOBS);

    checkForSignals();

    while(TRUE)
    {
        bg = 0;
        fgPid = FG_PID_DEFAULT;
        redirection = 0;
        piping = 0;

        checkForSignals();

        if ((cnt = getcmd("\nshelly >> ", args)) > 0) {
            // Built-in commands
            checkForRedirectionOrPiping(args, cnt, &redirection, &piping);
            printf("%d\n", redirection);
            printf("%d\n", piping);
            
            if (strcmp("echo", args[0]) == 0) {
                echo_shelly(args);
                // printf("in echo");
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
                pid_t pid = fork();
                printf("args[0]%s", args[0]);

                if (pid == 0) {
                    // Child
                    for (int i = 0; i < cnt; ++i) {
                        if (strcmp(args[i], ">") == 0) {
                            redirection_shelly(args, i);
                        }
                        if (strcmp(args[i], "|") == 0) {
                            piping_shelly(args, i);
                        }
                    }
                    exec_shelly(args[0], args);
                }
                else if (pid == -1) {      
                    printf("%s", forkErrorMessage);
                    exit(1);
                }
                else {
                    // Parent
                    if (bg == 0) {  // if the child process is not in the bg => wait for it
                        fgPid = pid; // parent/shelly is in froground and waiting for command that is happening in forground.
                        waitpid(pid, NULL, WFORFINISH);
                        fgPid = FG_PID_DEFAULT;
                    }
                    else { // add jobs to the background b/c & is specified
                        fgPid = FG_PID_DEFAULT; // not waiting for child; everything is in background and parent/shelly process is in forground. 
                        addNewJob(pid);    
                    }
                }
            }
        }
        freeArgs(args, cnt);
        redirection = 0;
        piping = 0;
    }
    // Free each arg space in args because malloc was used before    
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
    } 
    else {
        bg = 0;
    }

    for (int i = 0; i < MAX_INPUT_ARGS; ++i) {
        args[i] = NULL;
    }


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
            args[i] = malloc((strlen(token)+1) * sizeof(*token)); // +1 to account for '\0' termination
            strcpy(args[i], token);
            i++;
        }
    }
    // Args are now filled up with the new input arguments

    return i;
}

void freeArgs(char *args[], int numArgs)
{
    for (int i = 0; i < numArgs; i++) {
        if (args[i] != NULL) {
            free(args[i]);
            args[i] = NULL;
        }
    }
}

void exec_shelly(const char *cmd, char *const cmdArgs[]) 
{
    int execReturnCode = execvp(cmd, cmdArgs);
    if (execReturnCode < 0) {
       printf("%s", execErrorMessage);
       exit(1);
    }
}

void checkForSignals() 
{
    if (signal(SIGINT, handle_sigint) == SIG_ERR) { // CTRL-C
        printf("ERROR: could not bind signal handler for SIGINT.\n");
        exit(1);
    }

    if (signal(SIGTSTP, handle_sigtstp) == SIG_ERR) { // CTRL-Z
        printf("ERROR: could not bind signal handler for SIGTSTP.\n");
        exit(1);
    }  
}

void handle_sigint(int sigNum)
{
    if (fgPid != FG_PID_DEFAULT) {
        for (int jobNum = 0; jobNum < MAX_NUM_JOBS; jobNum++)
        {
            if (jobsQueue[jobNum] == fgPid) {
                printf("%d, %d", jobNum, fgPid);
                kill(jobsQueue[jobNum], SIGTERM);
                jobsQueue[jobNum] = 0;
                return;
            }
        }
    }
    // When fgPID == FG_PID_DEFAULT
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
        if (jobsQueue[jobNum] != 0) {
            pid = jobsQueue[jobNum];
            kill(pid, SIGTERM); // SIGTERM instead of SIGKILL. Src: https://linuxhandbook.com/sigterm-vs-sigkill/#:~:text=SIGTERM%20gracefully%20kills%20the%20process,the%20child%20processes%20as%20well.
        }
    }
    exit(code);
}

void addNewJob(int pid)
{
    for (int jobNum = 0; jobNum < MAX_NUM_JOBS; jobNum++) {
        if (jobsQueue[jobNum] == 0) {
            jobsQueue[jobNum] = pid;
            return;
        }
    }
    printf("ERROR: max number of jobs in the the jobsQueue.\n");
}

void jobs_shelly()
{
    // removeFinishedJobs(); // do not display jobs that are done/dead ??? broken
    printf("Job [#]\t PID\n");
    for (int jobNum = 0; jobNum < MAX_NUM_JOBS; jobNum++)
    {
        if (jobsQueue[jobNum] != 0) {
            printf("[%d]\t %d\n", jobNum, jobsQueue[jobNum]);
        }
    }
}

void removeFinishedJobs() {
    for (int jobNum = 0; jobNum < MAX_NUM_JOBS; jobNum++) {
        if (jobsQueue[jobNum] != 0) {
            printf("rmv %d\n", jobNum);
            if (waitpid(jobsQueue[jobNum], NULL, WNOHANG) != 0) { // WNOHANG: allow process to move on to other tasks
                printf("rmv %d\n", jobsQueue[jobNum]);
                printf("finished");
                jobsQueue[jobNum] = 0;
            }
        }
    }
}

void fg_shelly(char *args[MAX_INPUT_ARGS])
{
    fgPid = FG_PID_DEFAULT; // reset forground pid
    int jobNum = strtol(args[1], NULL, 10); // any character/invalid input will return 0

    // Select job pid for forground execution
    if (0 <= jobNum && jobNum < MAX_NUM_JOBS) {
        if (jobsQueue[jobNum] != 0) {
            fgPid = jobsQueue[jobNum];
        }
    }
    else {
        printf("ERROR: invalid job [#] input. Taking any available job from jobQueue.\n");
        for (int jobNum = 0; jobNum < MAX_NUM_JOBS; jobNum++) 
        {
            if (jobsQueue[jobNum] != 0) {
                fgPid = jobsQueue[jobNum];
                break;
            }
        }
    }
    if (fgPid == FG_PID_DEFAULT) {
        printf("ERROR: no available jobs in queue.\n");
        return;
    }

    // Put selected job into forground and wait for it to finish
    waitpid(fgPid, NULL, WFORFINISH);
    jobsQueue[jobNum] = 0;
    fgPid = FG_PID_DEFAULT;
}

void checkForRedirectionOrPiping(char *args[MAX_INPUT_ARGS], int inputArgs, int *redirectionFlag, int *pipingFlag)
{
    for (int i = 0; i < inputArgs; i++) 
    {
        if (strcmp(">", args[i]) == 0) {
            *redirectionFlag = 1;
        }
        else if (strcmp("|", args[i]) == 0) {
            *pipingFlag = 1;
        }
    }
}

void redirection_shelly(char *args[MAX_INPUT_ARGS], int redirectionIndex)
{
    // Have output be redirected to file instead of STDOUT
    int fd;

    fd = open(args[redirectionIndex+1], O_WRONLY | O_CREAT | O_TRUNC, PERMS);
    if (dup2(fd, STDOUT_FILENO) >= 0 && fd >= 0) {
        args[redirectionIndex] = NULL;
        args[redirectionIndex+1] = NULL;
        if (close(fd) == -1) {
            printf("%s", closeErrorMessage);
            return;
        }
    }
    exec_shelly(args[0], args);
}

void piping_shelly(char *args[MAX_INPUT_ARGS], int redirectionIndex) 
{
    args[redirectionIndex] = NULL;
    int pipefd[2];

    if (pipe(pipefd) == -1) {
        printf("%s", pipeErrorMessage);
        return;
    }

    int pid = fork(); // file descriptors get copied over when forking
    if (pid == 0) {
        // Child: pipe write end
        int closeReturnVal;
        int dup2ReturnVal;

        closeReturnVal = close(pipefd[0]); // close read end b/c not used
        if (closeReturnVal == -1) printf("%s", closeErrorMessage);

        dup2ReturnVal = dup2(pipefd[1], STDOUT_FILENO);
        if (dup2ReturnVal == -1) printf("%s", dup2ErrorMessage);

        close(pipefd[1]); // close write end so as to not have duplicate file descriptors
        if (closeReturnVal == -1) printf("%s", closeErrorMessage);

        exec_shelly(args[0], &args[0]);
    }
    else if (pid == -1) {
        printf("%s", forkErrorMessage);
        return;
    }
    else {
        // Parent: pipe read end
        int closeReturnVal;
        int dup2ReturnVal;

        closeReturnVal = close(pipefd[1]); // close write end b/c not used
        if (closeReturnVal == -1) printf("%s", closeErrorMessage);

        dup2ReturnVal = dup2(pipefd[0], STDIN_FILENO);
        if (dup2ReturnVal == -1) printf("%s", dup2ErrorMessage);

        closeReturnVal = close(pipefd[0]); // close read end so as to not have duplicate file descriptors
        if (closeReturnVal == -1) printf("%s", closeErrorMessage);
  
        exec_shelly(args[redirectionIndex+1], &args[redirectionIndex+1]);
    }
}
