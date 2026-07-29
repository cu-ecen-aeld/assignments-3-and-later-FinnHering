#include "systemcalls.h"
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <syslog.h>

/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
*/
bool do_system(const char *cmd)
{
    int status = system(cmd);
    return WIFEXITED(status) && (WEXITSTATUS(status) == 0);
}

/**
* @param count -The numbers of variables passed to the function. The variables are command to execute.
*   followed by arguments to pass to the command
*   Since exec() does not perform path expansion, the command to execute needs
*   to be an absolute path.
* @param ... - A list of 1 or more arguments after the @param count argument.
*   The first is always the full path to the command to execute with execv()
*   The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
*   using the execv() call, false if an error occurred, either in invocation of the
*   fork, waitpid, or execv() command, or if a non-zero return value was returned
*   by the command issued in @param arguments with the specified arguments.
*/

bool do_exec(int count, ...)
{
    openlog(NULL, 0, LOG_USER);
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    command[count] = command[count];

/*
 * TODO:
 *   Execute a system command by calling fork, execv(),
 *   and wait instead of system (see LSP page 161).
 *   Use the command[0] as the full path to the command to execute
 *   (first argument to execv), and use the remaining arguments
 *   as second argument to the execv() command.
 *
*/
    int pid = fork();
    if (pid == -1) {
        perror("Unable to fork!");
        return false;
    } else if (pid) {
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            syslog(LOG_INFO, "Child exited with status: %d", WEXITSTATUS(status));
            return WEXITSTATUS(status) == 0;
        }
    } else {
        // We need to pass all of command including that path to the binary as argv as it is expected by POSIX that argv[0] is the path to binary...
        execv(command[0], command);
        exit(1); // In case execv fails return 1 error code for parent to see that we failed...
    }
    
    va_end(args);
    return true;
}

/**
* @param outputfile - The full path to the file to write with command output.
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    command[count] = command[count];


/*
 * TODO
 *   Call execv, but first using https://stackoverflow.com/a/13784315/1446624 as a refernce,
 *   redirect standard out to a file specified by outputfile.
 *   The rest of the behaviour is same as do_exec()
 *
*/

    int pid = fork();
    if (pid == -1) {
        perror("Unable to fork!");
        return false;
    } else if (pid) {
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            syslog(LOG_INFO, "Child exited with status: %d", WEXITSTATUS(status));
            return WEXITSTATUS(status) == 0;
        }
    } else {
        // As per POSIX a newly created FD always gets the lowest (!) possible non-negative number. In this case it will be 1 (stdout) since we closed it.
        close(STDOUT_FILENO);
        open(outputfile, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        
        // We need to pass all of command including that path to the binary as argv as it is expected by POSIX that argv[0] is the path to binary...
        execv(command[0], command);
        exit(1); // In case execv fails return 1 error code for parent to see that we failed...
    }

    va_end(args);

    return true;
}
