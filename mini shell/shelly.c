/**
 * @file shelly.c
 * @author Zhanna Klimanova (zhanna.klimanova@mail.mcgill.ca)
 * @brief 
 * @version shelly
 * @date 2022-09-19
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#define SPACE_NON_PRINTABLE_CHARS_DIVIDER 32
#define MAX_INPUT_ARGS 5
#define MAX_NUM_JOBS 5
#define TRUE 1
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
 * @brief prints shelly welcome message for the user. Use case examples
 *        are included.
 * 
 */
void printWelcomeMessage();

/**
 * @brief performs error checking on input, parses the user command in args, 
 *        and returns the number of entries in the array.
 * 
 * @param prompt 
 * @param args 
 * @param background 
 * @return int 
 */
int getcmd(char *prompt, char *args[]);

/**
 * @brief frees and clears used space in args array.
 * 
 * @param args 
 * @param numArgs 
 */
void freeArgs(char *args[], int numArgs);

/**
 * @brief clears all space in the args array by setting to NULL.
 * 
 * @param args 
 */
void resetArgs(char *args[MAX_INPUT_ARGS]);

/**
 * @brief executes execvp and performs error checking to ensure the
 *        execvp command was performed successfully.
 * 
 * @param cmd 
 * @param cmdArgs 
 */
void exec_shelly(const char *cmd, char *const cmdArgs[]);

/**
 * @brief checks for SIGTSTP and SIGINT signals raised by user.
 * 
 */
void checkForSignals();

/**
 * @brief gracefully handles SIGINT or CTRL-C signal by stopping any
 *        current forground process and outputting a message for the user
 *        to continue.
 * 
 * @return int 
 */
void handle_sigint(int sigNum);

/**
 * @brief gracefully handles SIGTSTP or CTRL-Z signal by not performing any
 *        exiting action when issued by a user. The user can press Enter to continue.
 * 
 * @return int 
 */
void handle_sigtstp(int sigNum);

/**
 * @brief shelly built-in command that prints its input back
 *        to STDOUT where the user can observe it.
 * 
 * @param args 
 */
void echo_shelly(char *args[MAX_INPUT_ARGS]);

/**
 * @brief shelly built-in command that changes directory into the one input
 *        by the user.
 * 
 * @param args 
 */
void cd_shelly(char *args[MAX_INPUT_ARGS]);

/**
 * @brief shelly built-in command that prints the user's current working
 *        directory to the STDOUT.
 * 
 */
void pwd_shelly();

/**
 * @brief shelly built-in command that gracefully exits shelly by terminating 
 *        all currently running jobs.
 * 
 */
void exit_shelly();

/**
 * @brief shelly built-in command that takes a job from the jobs queue
 *        and puts it into forground execution. If the job in the jobs queue
 *        has terminated in the background, fg will remove that jobs from the jobs queue
 *        and not execute it in the forground.
 * 
 * @param args 
 */
void fg_shelly(char *args[MAX_INPUT_ARGS]);

/**
 * @brief adds a new job (process id) to the jobs queue.
 * 
 * @param pid 
 */
void addNewJob(int pid);

/**
 * @brief lists all the jobs in the jobs queue. 
 * 
 */
void jobs_shelly();

/**
 * @brief removes finished jobs from the jobs queue.
 * 
 */
void removeFinishedJobs();

/**
 * @brief shelly built-in command that performs output redirection (to another file).
 * 
 * @param args 
 * @param redirectionIndex 
 */
void redirection_shelly(char *args[MAX_INPUT_ARGS], int redirectionIndex);

/**
 * @brief shelly built-in command that does inter-process communication
 *        pipes. It redirects the output of one process into the input of another 
 *        process.
 * 
 * @param args 
 * @param redirectionIndex 
 */
void piping_shelly(char *args[MAX_INPUT_ARGS], int redirectionIndex);


int main(void)
{
    printWelcomeMessage();

    char *args[MAX_INPUT_ARGS];
    int cnt; // count the number of arguments in command

    // Clear jobs
    memset(jobsQueue, NULL_CHARACTER, MAX_NUM_JOBS);

    checkForSignals();

    while(TRUE)
    {
        bg = 0;
        fgPid = FG_PID_DEFAULT;

        checkForSignals();

        if ((cnt = getcmd("\nshelly >> ", args)) > 0) {
            // Built-in commands
            if (strcmp("echo", args[0]) == 0) {
                echo_shelly(args);
            }
            else if (strcmp("cd", args[0]) == 0) {
                printf("%d", cnt);
                cd_shelly(args);
            }
            else if (strcmp("pwd", args[0]) == 0) {
                pwd_shelly();
            }
            else if (strcmp("exit", args[0]) == 0) {
                exit_shelly(EXIT_SUCCESSFUL);
            }
            else if (strcmp("fg", args[0]) == 0) {
                fg_shelly(args);
            }
            else if (strcmp("jobs", args[0]) == 0) {
                jobs_shelly();
            }
            else {
                // External commands
                pid_t pid = fork();

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
        // Free each arg space in args because malloc was used before
        freeArgs(args, cnt);
    }
        
}

void printWelcomeMessage()
{
    printf("%s\n", "Mini shell version <shelly>. Created September 2022.");
    printf("\n");
    printf("%s\n", "EXAMPLES: \n \t1. echo \"hello\" \n \t2. pwd \n \t3. cd .. \n \t4. ls -l | cat \n \t5. ls > shelly_out.txt");
    printf("%s\n", "\t6. sleep 1234 & \n \t7. jobs \n \t8. fg 0 \n \t9. CTRL-C \n \t10. sleep 2 \n \t11. exit");
    printf("_____________________________________________________________________________________________________");
    printf("\n");
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

    resetArgs(args);

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

void resetArgs(char *args[MAX_INPUT_ARGS])
{
    for (int i = 0; i < MAX_INPUT_ARGS; i++) {
        if (args[i] != NULL) {
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
    if (fgPid) {
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
            kill(pid, SIGTERM); // SIGTERM instead of SIGKILL
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
    printf("Job [#]\t PID\n");
    for (int jobNum = 0; jobNum < MAX_NUM_JOBS; jobNum++)
    {
        if (jobsQueue[jobNum] != 0) {
            printf("[%d]\t %d\n", jobNum, jobsQueue[jobNum]);
        }
    }
}

void removeFinishedJobs() {
    pid_t waitReturn;

    for (int jobNum = 0; jobNum < MAX_NUM_JOBS; jobNum++) {
        if (jobsQueue[jobNum] != 0) {
            waitReturn = waitpid(jobsQueue[jobNum], NULL, WNOHANG);
            if (waitReturn != 0) { // WNOHANG: allow process to move on to other tasks
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

void redirection_shelly(char *args[MAX_INPUT_ARGS], int redirectionIndex)
{
    // Have output be redirected to file instead of STDOUT
    int fd;

    fd = open(args[redirectionIndex+1], O_WRONLY|O_CREAT|O_TRUNC, PERMS);
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
