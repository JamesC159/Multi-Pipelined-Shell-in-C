#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#define BUF_SIZE 1024

/****************************************************************
 *  Capture input from the user. Returns the input from the
 *  standard input file descriptor.
 ***************************************************************/
char * getInput (char **buffer, size_t buflen)
{
    size_t bufsize = BUF_SIZE;
    *buffer = malloc(sizeof(char) * bufsize + 1);   // allocate space for the buffer
    memset(buffer, '\0', sizeof(*buffer));
    
    if (!*buffer)
    {
        fprintf(stderr, "Shell: buffer allocation error\n");
        exit(EXIT_FAILURE);
    }
    
    printf("$$ ");
    
    fflush(NULL);
    
    int bytesRead = getline(&(*buffer), &bufsize, stdin);
    if (bytesRead < 0)
    {
        printf("Getline error\n");
        exit(EXIT_FAILURE);
    }
    
    return *buffer; // Not capturing return value right now
 }

/****************************************************************
 *  Tokenize the buffer input from stdin
 ***************************************************************/
char ** splitLine(char *line, int *numCmds)
{
    const char *DELIMS = "|\r\t\n";
    int bufsize = BUF_SIZE;
    int pos = 0;
    char **tokens = malloc (sizeof(char) * BUF_SIZE + 1);
    char *token;
    *numCmds = 0;
    
    if (!tokens)
    {
        fprintf(stderr, "Shell: buffer allocation error\n");
        exit(EXIT_FAILURE);
    }
    
    /* Tokenize the line */
    token = strtok(line, DELIMS);
    while (token != NULL)
    {
        tokens[pos] = token;
        numCmds++;
        pos++;
        if (pos > bufsize)
        {
            bufsize += BUF_SIZE;
            tokens = realloc(tokens, bufsize * sizeof(char) + 1);
            if (!tokens)
            {
                fprintf(stderr, "Shell: buffer allocation error\n");
                exit(EXIT_FAILURE);
            }
        }
        token = strtok(NULL, DELIMS);   // continue grabbing tokens
    }
    
    tokens[pos] = NULL;
    return tokens;
}

void loopPipe(char ***cmds)
{
    int fd[2];
    pid_t pid;
    int fd_in = 0;
    int i = 0;
    
    while (*cmds != NULL)
    {
        pipe(fd);
        
        if ((pid = fork()) == -1)
        {
            exit(EXIT_FAILURE);
        }
        
        if (pid == 0)
        {
            close(0);
            dup2(fd_in, 0); // Change input according to old one
            dup2(fd[0], 0);
            close(fd[0]);
            close(fd[1]);
            execvp((cmds[i])[0], cmds[i]);
            exit(EXIT_FAILURE);
        }
        else
        {
            close(fd[0]);
            dup2(fd[1], 1);
            write(fd[1], cmds[i], strlen());
            fd_in = fd[0];  // save input for next command
            close(fd[1]);
            execvp((cmds[i])[0], cmds[i]);
            i++;
        }
    }
}

/****************************************************************
 *  Main function
 ***************************************************************/
int main (int argc, char **argv)
{
    const char *DELIMS = "|\r\t\n";
	char *buf;     // buffer to hold user input from standard input stream.
	pid_t pid;          // Parent id of the current process
    pid_t childpid;     // pid of child process
    pid_t forkResult;
	int status;
    int fd[2];      // Contains file descriptors returned from pipe()
    int dataPocessed;
    int numCmds;

    /* Loop while the user is getting input */
	while (getInput(&buf, sizeof(buf)))
    {
        char **args = splitLine(buf, &numCmds);
        char ***cmds = malloc(sizeof(char*) * BUF_SIZE + 1);
        char * tempBuf = malloc (sizeof(char) * BUF_SIZE + 1);
        int i;
        
        for (i = 0; i < numCmds; i++)
        {
            int len = strlen(*args);
            (args[i])[len - 1] = '\0';
        }
        
        cmds = &args;
        
        loopPipe(&args);
        
        /* Print tokens just to check if we are processing them correctly */
//        while (1)
//        {
//            char *token = args[i++];
//            if (token != NULL)
//                printf("Token #%d: %s\n", i, token);
//            else
//                break;
//        }
//        
//        fflush(NULL);
//        
//        if (pipe(fd) == 0)
//        {
//            forkResult = fork();
//            if (forkResult == -1)
//            {
//                /* Failed to fork */
//                fprintf(stderr, "Shell cannot fork: %s\n", strerror(errno));
//                continue;
//            }
//            
//            
//            if (forkResult == 0)
//            {
//                dup2(fd[0], 0);
//                close(fd[0]);
//                read(1, tempBuf, BUF_SIZE);
//                close(fd[1]);
//                execvp(cmds[2][0], cmds[2]);
//                fprintf(stderr, "Shell: couldn't execute %s: %s\n   ", buf, strerror(errno));
//                exit(EX_DATAERR);
//                
//            }
//            else
//            {
//                dup2(fd[1], 1);
//                close(fd[0]);
//                dataPocessed = write(fd[1], cmds[1], strlen(cmds[1]));
//                execvp(cmds[1][0], cmds[1]);
//                close(fd[1]);
//            }
//        }
//        
//        free(tempBuf);
        
//        /* Fork and execute command in the shell */
//		pid = fork();
//		switch(pid)
//        {
//            case -1:
//            {
//                /* Failed to fork */
//                fprintf(stderr, "Shell cannot fork: %s\n", strerror(errno));
//                continue;
//            }
//            case 0: // Remember the child inherits open file descriptors
//            {
//                dup2(fd[0], 0);
//                close(fd[1]);   // Close the output file descriptor
//                /* Child so run the command */
//                execvp(args[0], args);        // Should not ever return otherwise there was an error
//                fprintf(stderr, "Shell: couldn't execute %s: %s\n   ", buf, strerror(errno));
//                exit(EX_DATAERR);
//            }
//		}
//        
//        dup2(fd[1], 1);
//        close(fd[0]);   // Close input file descriptor for the parent
        
        
        /* Suspend execution of calling process until receiving a status message from the child process
            or a signal is received. On return of waitpid, status contains the termination
            information about the process that exited. The pid parameter specifies the set of child
            process for which to wait for */
//		if ((pid = waitpid(pid, &status, 0) < 0))
//        {
//			fprintf(stderr, "Shell: waitpid error: %s\n", strerror(errno));
//        }
        
        free(args);
	}
    
    free(buf);

	exit(EX_OK);
}
