//
//  shell.c
//  
//
//  Created by James Combs on 11/26/15.
//
//

#include <assert.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
#include <regex.h>

/***********************************************************
 *  Structures
 **********************************************************/
struct command
{
    char **argv;
};

/***********************************************************
 *  Function Prototypes
 **********************************************************/
int getInput(char **);
int parseTokens(char **, int);
int createCommands(struct command **, char **, int);
int spawnProcess(int, int, struct command *);
void pipeline (struct command *, int);
void closeFD(int);
void redirect (int, int);
char ** tokenize(char *, int *);

/***********************************************************
 *  Main Function
 **********************************************************/
int main (int argc, char *argv[])
{
    char *buf;      // Contains input from stdin
    struct command cmds[BUFSIZ];
    int result;
    int numPipes = 0;
    
    while ((result = getInput(&buf)) != -1)
    {
        int numTokens = 0;
        
        /* Tokenize and Parse Command before creating pipes */
        char **tokens = tokenize(buf, &numTokens);
        numPipes = parseTokens(tokens, numTokens);
        
        /* NOTE- this code below is left in the main function to simplify
            creating the commands */
        int i, j;
        int numCmds = 0;
        char **tempV = malloc (sizeof(char) * BUFSIZ);
        j = 0;
        
        /* Setup commands */
        for (i = 0; i < numTokens; i++)
        {
            char *temp = malloc (sizeof(char) * BUFSIZ);
            
            strcpy(temp, "");
            while (tokens[i] != NULL && tokens[i][0] != '|')
            {
                strcat(temp, tokens[i]);
                strcat(temp, " ");
                i++;
            }
            
            tempV[j] = temp;
            tempV[j][strlen(temp)] = NULL;
            j++;
            numCmds++;
        }
        j = 0;
        
        /* Allocate space for the vectors in the command structure */
        for (i = 0; i < numCmds; i++)
        {
            cmds[i].argv = malloc(sizeof(char) * BUFSIZ);
        }
        
        /* Add the tokens to the vectors in command structure */
        for (i = 0; i < numCmds; i++)
        {
            char *token = strtok(tempV[i], " ");
            while (token != NULL)
            {
                cmds[i].argv[j] = token;
                
                /* get rid of the newline character */
                if (cmds[i].argv[j][strlen(token) - 1] == '\n')
                {
                    cmds[i].argv[j][strlen(token) - 1] = NULL;
                }
                
                cmds[i].argv[j][strlen(token)] = NULL;
                
                j++;
                token = strtok(NULL, " ");
            }
            j = 0;
        }
        
        /* run the multipipelined command shell */
        pipeline(cmds, numPipes);
        
        free(tokens);
        free(tempV);
        for (i = 0; i < numCmds; i++)
        {
            free(cmds[i].argv);
        }
    }
    
    free(buf);
}

/***********************************************************
 *  Get line of input from stdin
 **********************************************************/
int getInput(char **buffer)
{
    int bytesRead;
    size_t numBytes = BUFSIZ;
    *buffer = malloc(sizeof(char) * BUFSIZ);
    
    if (!*buffer)
    {
        fprintf(stderr, "Buffer allocation error\n");
        exit(EXIT_FAILURE);
    }
    
    bytesRead = getline(&(*buffer), &numBytes, stdin);
    if (bytesRead < 0)
    {
        fprintf(stderr, "Error reading input\n");
        return -1;
    }
    
    return 0;
}

/***********************************************************
 *  Tokenize the line of input processed by getInput()
 *  Delimiter character is just a space
 **********************************************************/
char ** tokenize(char *command, int *numTokens)
{
    const char *DELIMS = " ";
    char **tokens = malloc(sizeof(char) * BUFSIZ);
    char *token;
    int pos = 0;
    int buffersize = BUFSIZ;
    
    if (!tokens)
    {
        fprintf(stderr, "Buffer allocation error\n");
        exit(EXIT_FAILURE);
    }
    
    token = strtok(command, DELIMS);
    while(token != NULL)
    {
        tokens[pos] = token;
        
        /* Add null terminator at the end of a token
         Needed later for implementing the shell */
        tokens[pos][strlen(token)] = NULL;
        pos++;
        (*numTokens)++;
        
        /* Check to see if command is longer than allocated space and
            reallocate as necessary */
        if (pos > buffersize)
        {
            buffersize += BUFSIZ;
            tokens = realloc(tokens, sizeof(char) * buffersize);
            
            if (!tokens)
            {
                fprintf(stderr, "Buffer allocation error\n");
                exit(EXIT_FAILURE);
            }
        }
        
        token = strtok(NULL, DELIMS);
    }
    
    /* Places null terminator at the end of the vector
        For use later in the shell */
    tokens[pos] = NULL;
    
    return tokens;
    
}

/***********************************************************
 *  Parses the tokens processed by tokenize()
 *  Also counts the number of pipes it comes accross
 *  And returns the number of pipes it found
 **********************************************************/
int parseTokens(char **tokens, int numTokens)
{
    int i, j;
    int numPipes = 0;
    int hasPipe = 0, isFirstCommand = 1, cmdAfterPipe = 0;
    int hasColon = 0, cmdAfterColon = 0, hasRedirection = 0, fileAfterRed = 0;
    char *tokenHolder = malloc(sizeof(char) * BUFSIZ);
        
    if (!tokenHolder)
    {
        fprintf(stderr, "Buffer allocation error\n");
        exit(EXIT_FAILURE);
    }
    
    /* Loop tokens */
    for (i = 0; i < numTokens; i++)
    {
        /* Loop bytes in token */
        for (j = 0; j < strlen(tokens[i]); j++)
        {
            tokenHolder[j] = tokens[i][j];
            
            if (tokenHolder[j] == '|')
            {
                hasPipe = 1;
                numPipes++;
            }
            else if (tokenHolder[j] == ';')
            {
                hasColon = 1;
            }
            else if (tokenHolder[j] == '<' || tokenHolder[j] == '>')
            {
                hasRedirection = 1;
            }
        }
        
        /* The first token is always a command */
        if (isFirstCommand)
        {
            printf("Command: %s\n", tokenHolder);
            isFirstCommand = 0;
        }
        /* Handle command arguments */
        else if (tokenHolder[0] == '-')
        {
            printf("Options: %s\n", tokenHolder);
        }
        /* Checks to see if the current token is a pipe */
        else if (strcmp(tokenHolder, "|") == 0)
        {
            printf("Pipe\n");
            cmdAfterPipe = 1;   // Ready for command after pipe
            hasPipe = 0;
        }
        /* If we have a colon, get ready for the command after the next iteration */
        else if (hasColon)
        {
            cmdAfterColon = 1;
            hasColon = 0;
        }
        /* If there is redirection, get ready for the file afterwards */
        else if (hasRedirection)
        {
            printf("File Redirection: %s\n", tokenHolder);
            fileAfterRed = 1;
            hasRedirection = 0;
        }
        /* Recognize the command after a pipe */
        else if (cmdAfterPipe)
        {
            printf("Command: %s\n", tokenHolder);
            cmdAfterPipe = 0;
        }
        /* Recognize the command after a colon */
        else if (cmdAfterColon) // Catch the command after the colon
        {
            printf("Command: %s\n", tokenHolder);
            cmdAfterColon = 0;
        }
        /* Recognize the file after a redirection */
        else if (fileAfterRed)
        {
            printf("File: %s\n", tokenHolder);
            fileAfterRed = 0;
        }
        /* If none of the above is true, it must be an argument */
        else
        {
            printf("Arguments: %s\n", tokenHolder);
        }
        
        /* Reset tokenHolder for next token */
        memset(tokenHolder, '\0', strlen(tokenHolder));
        fflush(NULL);
    }
    
    return numPipes;
}

/***********************************************************
 *  Implementation of multi-pipelined shell
 **********************************************************/
void pipeline(struct command *cmds, int numPipes)
{
    int status;
    int i;
    int in = STDIN_FILENO;
    
    /* Loop for number of pipes */
    for (i = 0; i < numPipes; i++)
    {
        
        int fd[2];
        pid_t pid;  // Child's pid
        
        if (pipe(fd) == -1)
        {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
        else if ((pid = fork()) == -1)
        {
            perror("fork");
            exit(EXIT_FAILURE);
        }
        else if (pid == 0)   // Child
        {
            closeFD(fd[0]);
            redirect(in, STDIN_FILENO);
            redirect(fd[1], STDOUT_FILENO);
            
            execvp(cmds[i].argv[0], (char * const *)cmds[i].argv);
        }
        else    // Parent
        {
            assert(pid > 0);
            
            close(fd[1]);
            close(in);
            in = fd[0];
            
            wait(&status);
        }
    }
    
    //if (file_fd != 1) { dup2(file_fd, 1); close(file_fd); }
    
    redirect(in, STDIN_FILENO);
    redirect(STDOUT_FILENO,STDOUT_FILENO);
    
    wait(&status);
    
    /* Execute the last stage with the current process. */
    execvp (cmds[i].argv[0], (char * const *)cmds[i].argv);
}

/***********************************************************
 *  Closes file descriptors for pipeline
 **********************************************************/
void closeFD(int fd)
{
    if ((close(fd)) == -1)
    {
        perror("close");
        exit(EXIT_FAILURE);
    }
}

/***********************************************************
 *  Redirects file descriptors
 **********************************************************/
void redirect (int oldfd, int newfd)
{
    if (oldfd != newfd)
    {
        if (dup2(oldfd, newfd) != -1)
        {
            if ((close(oldfd)) == -1)
            {
                perror("close");
                exit(EXIT_FAILURE);
            }
        }
        else
        {
            perror("dup2");
            exit(EXIT_FAILURE);
        }
    }
}