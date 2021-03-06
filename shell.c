//
//  shell.c
//  
//
//  Created by James Combs on 11/26/15.
//
//

#include <unistd.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <fcntl.h>

/***********************************************************
 *  Structures
 **********************************************************/
typedef struct command
{
	char **argv;
	int fdIn, fdOut;
	int numRedirections, numCmdTokens;

} CMD;

/***********************************************************
 *  Function Prototypes
 **********************************************************/
char ** tokenize(char *, int *);
int getInput(char **);
int parseTokens(char **, int);
int pipeline(CMD *, int, int);
void closeFD(int);
void redirect(int, int);

/***********************************************************
 *  Main Function
 **********************************************************/
int main(int argc, char *argv[])
{
	char *buf;      // Contains input from stdin
	CMD cmds[sizeof(CMD) * BUFSIZ];
	int result;
	int numPipes = 0;
	int in, out;

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
		char **tempV = malloc(sizeof(char) * BUFSIZ);	// Temp vector to holder commands
		j = 0;


		/* Setup commands */
		for (i = 0; i < numTokens; i++)
		{
			char *temp = malloc(sizeof(char) * BUFSIZ);	// Temp buffer to hold a command

			strcpy(temp, "");

			/* Ignore pipes and redirections
				Loop until we hit a pipe, redirection or no more tokens
				setup up the temp buffer with a correct command */
			while (tokens[i] != NULL && tokens[i][0] != '|')
			{
				strcat(temp, tokens[i]);
				strcat(temp, " ");
				i++;
			}

			tempV[j] = temp;	// Add the command to the temp vector
			tempV[j][strlen(temp)] = NULL;	// Set the last character in the command to NULL
			j++;
			numCmds++;
		}

		j = 0;

		/* Allocate space for the argument vectors in the command structure
		and initialize some variables */
		for (i = 0; i < numCmds; i++)
		{
			cmds[i].argv = malloc(sizeof(char) * BUFSIZ);
			cmds[i].numCmdTokens = 0;
			cmds[i].numRedirections = 0;
		}

		/* Add the tokens to the argument vectors in command structure */
		for (i = 0; i < numCmds; i++)
		{
			char *token = strtok(tempV[i], " ");
			while (token != NULL)
			{
				cmds[i].argv[j] = token;
				cmds[i].numCmdTokens++;

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

			free(token);
		}

		/* Look for redirection symbols in the argument vectors */
		for (i = 0; i < numCmds; i++)
		{
			for (j = 0; j < cmds[i].numCmdTokens; j++)
			{
				if (strcmp(cmds[i].argv[j], "<") == 0)
				{
					/* Open input file if it exists */
					if ((cmds[i].fdIn = open(cmds[i].argv[j + 1], O_RDONLY)) == -1)
					{
						perror(cmds[i].argv[j + 1]);
						exit(EXIT_FAILURE);
					}

					cmds[i].argv[j] = NULL;
					cmds[i].numRedirections++;
				}
				else if (strcmp(cmds[i].argv[j], ">") == 0)
				{
					/* open output file. if it doesnt exist, create it. */
					if ((cmds[i].fdOut = open(cmds[i].argv[j + 1], O_WRONLY | O_CREAT | O_TRUNC)) == -1)
					{
						perror(cmds[i].argv[j + 1]);
						exit(EXIT_FAILURE);
					}

					cmds[i].argv[j] = NULL;
					cmds[i].numRedirections++;
				}
			}
		}

		/* run the multipipelined command shell */
		pipeline(cmds, numPipes, numCmds);

		/* Free Memory */
		for (i = 0; i < numCmds; i++)
		{
			free(cmds[i].argv);
			free(tempV[i]);
		}
		free(tokens);
		free(tempV);
		free(buf);
	}

	exit(EXIT_SUCCESS);
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

	printf("Enter Command: ");
	bytesRead = getline(&(*buffer), &numBytes, stdin);
	if (bytesRead < 0)
	{
		fprintf(stderr, "Error reading input\n");
		return -1;
	}
	printf("\n");

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
	while (token != NULL)
	{
		tokens[pos] = token;
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

	return tokens;

}

/***********************************************************
 *  Parses the tokens processed by tokenize()
 *  Also counts the number of pipes it comes accross
 *  And returns the number of pipes it found
 *	JUST FOR DISPLAY IN THE ASSIGNMENT
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

	free(tokenHolder);
	return numPipes;
}

/***********************************************************
 *  Implementation of multi-pipelined shell
 **********************************************************/
int pipeline(CMD *cmds, int numPipes, int numCmds)
{
	int i, j;
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

		if ((pid = fork()) == -1)
		{
			perror("fork");
			exit(EXIT_FAILURE);
		}

		if (pid == 0)   // Child
		{
			closeFD(fd[0]);
			if (cmds[i].fdIn && cmds[i].fdIn != STDIN_FILENO)
			{
				redirect(in, cmds[i].fdIn);
				redirect(fd[1], STDOUT_FILENO);
			}
			else if (cmds[i].fdOut && cmds[i].fdOut != STDOUT_FILENO)
			{
				redirect(in, STDIN_FILENO);
				redirect(cmds[i].fdOut, STDOUT_FILENO);
			}
			else
			{
				redirect(in, STDIN_FILENO);
				redirect(fd[1], STDOUT_FILENO);
			}

			return execvp(cmds[i].argv[0], (char * const *)cmds[i].argv);
		}
		else    // Parent
		{
			assert(pid > 0);
			closeFD(fd[1]);
			closeFD(in);
			in = fd[0];
		}
	}

	// Need to implement something of the sort for I/O redirection
	//if (file_fd != 1) { dup2(file_fd, 1); close(file_fd); }

	/* Redirect file descriptors one more time */
	for (i = 0; i < numCmds; i++)
	{
		if (cmds[i].fdIn && cmds[i].fdIn != STDIN_FILENO)
		{
			redirect(in, cmds[i].fdIn);
			redirect(STDOUT_FILENO, STDOUT_FILENO);
			for (i = 0; i < numPipes; i++)
			{
				wait(NULL);
			}
			return execvp(cmds[i].argv[0], (char * const *)cmds[i].argv);
		}
		else if (cmds[i].fdOut && cmds[i].fdOut != STDOUT_FILENO)
		{
			redirect(in, STDIN_FILENO);
			redirect(cmds[i].fdOut, STDOUT_FILENO);
			for (i = 0; i < numPipes; i++)
			{ 
				wait(NULL);
			}
			return execvp(cmds[i].argv[0], (char * const *)cmds[i].argv);
		}
	}

	redirect(in, STDIN_FILENO);
	redirect(STDOUT_FILENO, STDOUT_FILENO);

	/* Not sure if this is the correct place - It works though */
	for (i = 0; i < numPipes; i++)
	{
		wait(NULL);
	}

	/* Execute the last stage with the current process. */
	return execvp(cmds[i].argv[0], (char * const *)cmds[i].argv);
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
void redirect(int oldfd, int newfd)
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