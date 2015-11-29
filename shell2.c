/****************************************************************
*	Author: James Combs
*	Date Created: 11/28/2015
*	Description:
*		This program simulates a shell that can handle multiple
*		pipelined commands as well as I/O redirection and I/O
*		redirection inside of the pipelined commands
*	Valid tested input:
*		command
*		command -options
*		command > file
*		command -options > file
*		command | command
*		command | command | ...
*		command -options | command -options > file
*	Known Issues:
*		running command (-options) > file
*		or command (-options) | command (-options) | ... > file
*		will result in undesired results inside the file we are
*		outputing to.
****************************************************************/

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

typedef struct command
{
	char **argv;
	int fdIn, fdOut;

} Command;

int countPipes(char *);
void deleteNewline(char **);
int tokenize(char **, Command *);
void closeFd(int);
void runPipe(Command *, int, int);
void redirect(int, int);

int main(int argc, char **argv)
{
	const int CMD_LIMIT = 1024;
	char *input = malloc(sizeof(char) * BUFSIZ);
	int numPipes, numCmds;
	Command cmds[CMD_LIMIT];

	while (fgets(input, sizeof(char) * BUFSIZ, stdin) != NULL)
	{
		/* Delete the newline at end of input, count the pipes
			and then tokenize the input */
		deleteNewline(&input);
		numPipes = countPipes(input);
		numCmds = tokenize(&input, cmds);

		/* Run the pipe */
		runPipe(cmds, numCmds, numPipes);

		/* Reset input for next set of commands 
		CUREENTLY DOES NOT CONTINUE TO THIS POINT */
		memset(input, 0, sizeof(input));
		fflush(NULL);

		/* Free command's argument vectors before continuing since it has memory
			being allocated in the tokenize function each iteration */
		int i = 0;
		for (i; i < numCmds; i++)
		{
			free(cmds[i].argv);
		}
	}
	free(input);
}

/****************************************************************
*	Count the number of '|' found in the input command
****************************************************************/
int countPipes(char *command)
{
	int i = 0, pipes = 0;

	for (i; i < strlen(command); i++)
	{
		if (command[i] == '|')
		{
			pipes++;
		}
	}

	return pipes;
}

/****************************************************************
*	Gets rid of newline character at end of input
****************************************************************/
void deleteNewline(char **input)
{
	if ((*input)[strlen(*input) - 1] == '\n')
	{
		(*input)[strlen(*input) - 1] = '\0';
	}
}

/****************************************************************
*	Tokenizes the inpout command, opens any files found in the
*	command as necessary. After the command line input has
*	been tokenized, the tokens are then formatted for the
*	execvp() function and added to the Command structure that
*	will be used to execute the commands in the pipeline
****************************************************************/
int tokenize(char **command, Command *cmds)
{
	const char *DELIMS = " ";
	char **tokenV = malloc(sizeof(char) * BUFSIZ);	// Temporary token vector to store tokens before
													// adding to the command's argument vector
	char *token;
	int i = 0, j = 0;	// Counters for loops and indices
	int cmdAfterPipe = 0, fileAfterR = 0, cmdAfterS = 0;	// Used to catch commands after pipes and semilcolons
															// as well as any file after a redirection
	int isInR = 0, isOutR = 0, isApp = 0;	// Recognizes input or output file redirection 
											// as well as appension to a file
	int numCmds = 1, numTokens = 0;	// numCmds counts commands, starts at one since there
									//	is always one command

	token = strtok(*command, DELIMS);
	while (token != NULL)
	{
		switch (token[0])
		{
		case '-':
		{
			printf("Options: %s\n", token);
			break;
		}
		case '|':
		{
			numCmds++;	// Once we hit a pipe, we have finished a command.
			cmdAfterPipe = 1;
			break;
		}
		case '<':
		{
			printf("Redirection: %s\n", token);
			fileAfterR = 1;
			isInR = 1;
			token = strtok(NULL, DELIMS);
			continue;	// skip input redirection token, we dont need this in the command
		}
		case '>':
		{
			printf("Redirection: %s\n", token);
			
			/* Check to see if we are appending to the file */
			if (token[1] && token[1] == '>')
			{
				isApp = 1;
			}

			fileAfterR = 1;
			isOutR = 1;
			token = strtok(NULL, DELIMS);
			continue;	// skip output redirection token, we dont need this in the command
		}
		case ';':
		{
			cmdAfterS = 1;
			token = strtok(NULL, DELIMS);
			continue;	// skip semicolon token, we dont need this in the command
		}
		default:
		{
			/* First token is always a command */
			if (i == 0 || cmdAfterPipe)
			{
				printf("Command: %s\n", token);
				cmdAfterPipe = 0;
				i++;
			}
			/* Recognize the file after a redirection has happened */
			else if (fileAfterR)
			{
				printf("Redirection File: %s\n", token);

				/* If input redirection happens, open input file if it exists */
				if (isInR)
				{
					if ((cmds[numCmds - 1].fdIn = open(token, O_RDONLY)) == -1)
					{
						perror("Input file failure: ");
						exit(EXIT_FAILURE);
					}
					isInR = 0;
				}
				/* If output redirection happens */
				else if (isOutR)
				{	
					/* If we are appending to the file, open it as an appension */
					if (isApp)
					{
						printf("Appending\n");
						/* If it doesn't exist, create it and append */
						if ((cmds[numCmds - 1].fdOut = open(token, O_WRONLY | O_APPEND, 0744)) == -1)
						{
							if ((cmds[numCmds - 1].fdOut = open(token, O_WRONLY | O_CREAT | O_APPEND, 0744)) == -1)
							{
								perror(token);
							}
						}
						/* Otherwise the file exists, so just append to it */
						else
						{
							closeFd(cmds[numCmds - 1].fdOut);
							if ((cmds[numCmds - 1].fdOut = open(token, O_WRONLY | O_APPEND, 0744)) == -1)
							{
								perror(token);
							}
							
						}
					}
					/* If we are not appending to the file, simply create it and truncate it */
					else
					{
						if ((cmds[numCmds - 1].fdOut = open(token, O_WRONLY | O_CREAT | O_TRUNC, 0744)) == -1)
						{
							perror(token);
						}
					}
					isOutR = 0;
				}

				fileAfterR = 0;
			}
			/* Recognize the next command after a semicolon */
			else if (cmdAfterS)
			{
				printf("Redirection File: %s\n", token);
				cmdAfterS = 0;
			}
			/* Recognize an argument in a command */
			else
			{
				printf("Argument: %s\n", token);
			}
		}
		}

		/* Make sure the token has a null terminator and no newlines one more time, being lazy here */
		if (token[strlen(token) - 1] == '\n')
		{
			printf("FOUND NEWLINE - DELETING IT\n");
			token[strlen(token) - 1] = '\0';
		}
		/* Be double sure we catch the newline, this is most liklely unecessary */
		else if (token[strlen(token)] == '\n')
		{
			printf("FOUND NEWLINE - DELETING IT\n");
			token[strlen(token)] = '\0';
		}
		token[strlen(token)] = '\0';
		
		/* Add the token to the temporary token vector */
		tokenV[numTokens] = token;
		numTokens++;

		token = strtok(NULL, DELIMS);
	}
	
	/* Allocate space for argument vectors */
	for (i = 0; i < numCmds; i++)
	{
		cmds[i].argv = malloc(sizeof(char) * BUFSIZ);
	}
	
	/* Build the commands for the argument vectors and add them too it*/
	i = 0;
	int argvIdx = 0;
	for (j = 0; j < numTokens; j++)
	{
		if (tokenV[j][0] == '|')
		{
			argvIdx = 0;
			i++;
		}
		else
		{
			cmds[i].argv[argvIdx] = tokenV[j];
			argvIdx++;
		}
	}
	
	return numCmds;
}

/****************************************************************
*	Runs the multi pipelined "shell"
****************************************************************/
void runPipe(Command *cmds, int numCmds, int numPipes)
{
	int i = 0;
	int status;
	int in = STDIN_FILENO;
	
	/* Fork for the number of pipes */
	for (i; i < numPipes; i++)
	{
		int fds[2];
		pid_t childpid;

		if (pipe(fds) == -1)
		{
			perror("Piping: ");
			exit(EXIT_FAILURE);
		}

		if ((childpid = fork()) == -1)
		{
			perror("Fork: ");
			exit(EXIT_FAILURE);

		}

		if (childpid == 0)	// Child
		{
			closeFd(fds[0]);
			redirect(in, STDIN_FILENO);
			redirect(fds[1], STDOUT_FILENO);

			/* Child executing the parent's command */
			execvp(cmds[i].argv[0],(char * const * ) cmds[i].argv);

		}
		else	// Parent
		{
			closeFd(fds[1]);
			closeFd(in);
			in = fds[0];
		}
	}
	
	/* The children have done their job. Now the parent needs to execute the last
	stage of the pipeline. Also, we need to find any i/o redirection that may have 
	happened and redirect command's output to there. Currently not handling input redirection */
	int j = 0;
	for (j; j < numCmds; j++)
	{
		if (cmds[j].fdIn && cmds[j].fdIn != STDIN_FILENO)
		{
			redirect(in, cmds[j].fdIn);
			redirect(STDOUT_FILENO, STDOUT_FILENO);
			execvp(cmds[i].argv[0], cmds[i].argv);
		}
		else if (cmds[j].fdOut && cmds[j].fdOut != STDOUT_FILENO)
		{
			redirect(in, STDIN_FILENO);
			redirect(cmds[j].fdOut, STDOUT_FILENO);
			execvp(cmds[i].argv[0], cmds[i].argv);
		}
	}

	redirect(in, STDIN_FILENO);
	redirect(STDOUT_FILENO, STDOUT_FILENO);
	execvp(cmds[i].argv[0],(char * const *) cmds[i].argv);
}

/****************************************************************
*	Closes a file descriptor
****************************************************************/
void closeFd(int fd)
{
	if (close(fd) == -1)
	{
		perror(fd);
		exit(EXIT_FAILURE);
	}
}

/****************************************************************
*	Redirects file descriptors
****************************************************************/
void redirect(int oldFd, int newFd)
{
	if (oldFd != newFd)
	{
		if (dup2(oldFd, newFd) == -1)
		{
			perror("dup2: ");
			exit(EXIT_FAILURE);
		}
		else
		{
			if (close(oldFd) == -1)
			{
				perror(oldFd);
				exit(EXIT_FAILURE);
			}
		}
	}
}
