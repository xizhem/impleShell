#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <sys/wait.h>
#include "../include/shell.h"
#include <errno.h>
#include "../include/helpers.h"
#define COLOR "\x1b[36m"
#define RESET "\x1b[0m"

static char buffer[BUFFER_SIZE];
pid_t bgpid = -1;
extern int errno;
const max_path_len = 1024;
const max_host_len = 256;

void sigusr2_handler(int sig)
{
	int olderrno = errno;
	write(1, "Hi User!\n", 9);
	errno = olderrno;
}



void Sigemptyset(sigset_t *set)
{
	if(sigemptyset(set) == -1)
		perror("sigsetempty");
}

void Sigaddset(sigset_t* set, int signo)
{
	if(sigaddset(set, signo) == -1)
		perror("sigaddset");
}

void Sigprocmask(int how, const sigset_t* set, sigset_t* oset)
{
	if(sigprocmask(how, set, oset) == -1)
		perror("sigprocmask");
}
		 
	

int main(int argc, char *argv[])
{
	int i; //loop counter
	char *pos; //used for removing newline character
	char *args[MAX_TOKENS + 1];
	int exec_result;
	int exit_status;
	int bg ;
	pid_t pid;
	pid_t wait_result;
	sigset_t mask;
	time_t timer;
	struct tm* timeinfo;
	char time_buf[26];
	
	
	Sigemptyset(&mask);
	Sigaddset(&mask, SIGTSTP);
	Sigprocmask(SIG_BLOCK, &mask, NULL);	
	if(signal(SIGUSR2, sigusr2_handler) == SIG_ERR)
		perror("signal");
	
	char pathname[max_path_len];
	char hostname[max_host_len];
	char buffer[100];
	int k, index;	
	for(;;){
		if(bgpid != -1)
		{
			wait_result = waitpid(bgpid, &exit_status, WNOHANG); 
			if(wait_result == -1)
			{	
				printf("An error ocured while waiting for the process.\n");
				exit(EXIT_FAILURE);
			}
			else if(wait_result > 0)
				bgpid = -1;
		}
		
		//shell prompt
		time(&timer);
		timeinfo = localtime(&timer);
		strftime(time_buf, 26,"%m/%d/%Y %H:%M:%S" ,timeinfo);
		getcwd(pathname, max_path_len);
		gethostname(hostname, max_host_len);
		
		k = 0;
		index = 0;
		while(pathname[k] != '\0')
		{
			if (pathname[k] == '/')
			{
				index = 0;
			}
			buffer[index++] = pathname[k++];
		}
		buffer[index] = '\0';
		k = 0;
		while(hostname[k] != '\0')
		{
			if(hostname[k] == '.')
			{
				hostname[k] = '\0';
				break;
			}
			k++;
		}

		printf(COLOR "%s <xizhem@%s>: %s$ " RESET, time_buf, hostname ,buffer);
		fgets(buffer, BUFFER_SIZE, stdin);
		if( (pos = strchr(buffer, '\n')) != NULL){
			*pos = '\0'; //Removing the newline character and replacing it with NULL
		}
		// Handling empty strings.
		if(strcmp(buffer, "")==0)
			continue;
		// Parsing input string into a sequence of tokens
		size_t numTokens;
		*args = NULL;
		numTokens = tokenizer(buffer, args, MAX_TOKENS);
		if(!numTokens) {continue;}
		args[numTokens] = NULL;
		if(strcmp(args[0],"exit") == 0) // Terminating the shell
			return 0;
		if(strcmp(args[0], "cd") == 0)
		{
			if(args[1] == NULL)
				fprintf(stderr, "cd needs to follow by dir.\n");
			else if(chdir(args[1]) == -1)
				perror("cd error");
			continue;
		}
		if(strcmp(args[0], "fg") == 0) 
		{
			if (bgpid != -1)
			{
				printf("%d", bgpid);
				wait_result = waitpid(atoi(args[1]), &exit_status, 0);
				if(wait_result == -1)
					printf("The given background process' pid is not valid.\n");
				else
					bgpid = -1;
				
			}
			else
				printf("no background job currently\n");
			continue;
		}
		bg = 0;	
		if(strcmp(args[numTokens-1] , "&") == 0) //fg or bg
		{	
			if(bgpid == -1)
				bg = 1;
			else
				continue; 
			numTokens--;
			args[numTokens] = NULL;
		}
				
		pid = fork();
		if (pid == 0){ //If zero, then it's the child process
				int i;
				int fd_out = 0;
				int fd_err = 0;
				int fd_in = 0;
				int state_bothout = 0;
				int hasPipe = 0;
				int fd[2];
				char* args2[MAX_TOKENS + 1];

				for(i = numTokens-1; i > 0; i--)
				{
					if(strcmp(args[i], ">") == 0) 
					{
						if(args[i+1] == NULL)
						{
							fprintf(stderr, "Redirection file not found.\n");
							exit(EXIT_FAILURE);
						}
						fd_out = open(args[i+1], O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
						if(fd_out < 0)
						{
								fprintf(stderr,"fail to open file.\n");
								exit(EXIT_FAILURE);
						}
	
						if((dup2(fd_out, 1)) == -1)
							exit(EXIT_FAILURE);
						close(fd_out);
						args[i] = NULL;
					}
					else if(strcmp(args[i], "&>") == 0)
					{
						if(args[i+1] == NULL)
						{
							fprintf(stderr,"Redirection file not found.\n");
							exit(EXIT_FAILURE);
						}
						fd_out = open(args[i+1], O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
						if(fd_out < 0)
						{
								fprintf(stderr,"fail to open file.\n");
								exit(EXIT_FAILURE);
						}

						if((dup2(fd_out,1)) == -1)
							exit(EXIT_FAILURE);
						if((dup2(fd_out,2)) == -1)
							exit(EXIT_FAILURE);
						args[i] = NULL;
						close(fd_out);
					}
					else if(strcmp(args[i], ">>") == 0 )
					{
						if(args[i+1] == NULL)
						{
							fprintf(stderr,"Redirection file not found.\n");
							exit(EXIT_FAILURE);
						}
						fd_out = open(args[i+1], O_WRONLY| O_APPEND | O_CREAT, S_IRWXU);
						if(fd_out < 0)
						{
								fprintf(stderr,"fail to open file.\n");
								exit(EXIT_FAILURE);
						}

						if ((dup2(fd_out,1)) == -1)
							exit(EXIT_FAILURE);
						args[i] = NULL;
						close(fd_out);
					}
					else if( strcmp(args[i], "<") == 0)
					{	
						if(args[i+1] == NULL)
						{
							fprintf(stderr,"Redirection file not found.\n");
							exit(EXIT_FAILURE);
						}

						fd_in = open(args[i+1], O_RDONLY, 0);
						if(fd_in < 0)
						{
								fprintf(stderr,"fail to open file.\n");
								exit(EXIT_FAILURE);
						}

						if((dup2(fd_in, 0)) == -1)
							exit(EXIT_FAILURE);
						args[i] = NULL;
						close(fd_in);
					}
					else if( strcmp(args[i], "2>") == 0)
					{
						if(args[i+1] == NULL)
						{
							fprintf(stderr,"Redirection file not found.\n");
							exit(EXIT_FAILURE);
						}

						fd_err = open(args[i+1], O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
						if(fd_err < 0)
						{
								fprintf(stderr,"fail to open file.\n");
								exit(EXIT_FAILURE);
						}

						if ((dup2(fd_err, 2)) == -1)
							exit(EXIT_FAILURE);	
						args[i] = NULL;
						close(fd_err);
					}
					else if(strcmp(args[i], "2>&1") == 0)
					{
						state_bothout = 1;
						args[i] = NULL;
					}
					else if(strcmp(args[i], "|") == 0)
					{
						hasPipe = i;
						args[i] = NULL;
						i++;
						int j = 0;
						while(args[i] != NULL)
						{
							args2[j++] = args[i++];
						}
						args2[j] = NULL;
						break;
					}
						
				}
				if(state_bothout)
			        {
					if((dup2(1,2)) == -1)
						exit(EXIT_FAILURE);
				}

				if(hasPipe)
				{
					pid_t pid2;
					if(pipe(fd) == -1) //check for error
					{
						printf("Fail to establish pipe.\n");
						exit(EXIT_FAILURE);
					}
					pid2 = fork();
					if(pid2 == -1)
					{
						printf("fail to the second fork.\n");
						exit(EXIT_FAILURE);
					}
					else if(pid2 == 0) //child
					{
						close(fd[0]);
						dup2(fd[1],1);
						close(fd[1]);
						exec_result = execvp(args[0], &args[0]);
						if(exec_result == -1){ //Error checking
							printf("Cannot execute %s. An error occured.\n", args[0]);
						exit(EXIT_FAILURE);
						}
					}
					else //parent
					{
						wait_result = waitpid(pid2, NULL, 0);	
						if (wait_result == -1)
						{
							printf("An error ocured while waiting for the second child.\n");
							exit(EXIT_FAILURE);
						}
						close(fd[1]);
						dup2(fd[0], 0);
						close(fd[0]);
					}
				}
			//	if (fd_in < 0 || fd_out < 0 || fd_err< 0)
			//	{
			//		printf("fail to open file.\n");
			//		exit(EXIT_FAILURE);
			//	}
		
				
				//close(fd_out);
				//close(fd_err);
				//close(fd_in);
			if(hasPipe)
				exec_result = execvp(args2[0], &args2[0]);
			else		
				exec_result = execvp(args[0], &args[0]);
			if(exec_result == -1){ //Error checking
				printf("Cannot execute %s. An error occured.\n", args[0]);
				exit(EXIT_FAILURE);
			}
		exit(EXIT_SUCCESS);
		}
		else{ // Parrent Process
			if(!bg)
			{
				wait_result = waitpid(pid, &exit_status, 0);
				if(wait_result == -1){
					printf("An error ocured while waiting for the process.\n");
				exit(EXIT_FAILURE);
				}
			}
			else
				bgpid = pid;
			
 		    }
	}
	return 0;
}


size_t tokenizer(char *buffer, char *argv[], size_t argv_size)
{
    char *p, *wordStart;
    int c;
    enum mode { DULL, IN_WORD, IN_STRING } currentMode = DULL;
    size_t argc = 0;
    for (p = buffer; argc < argv_size && *p != '\0'; p++) {
        c = (unsigned char) *p;
        switch (currentMode) {
        case DULL:
            if (isspace(c))
                continue;
            // Catching "
            if (c == '"') {
                currentMode = IN_STRING;
                wordStart = p + 1;
                continue;
            }
            currentMode = IN_WORD;
            wordStart = p;
            continue;
        // Catching "
        case IN_STRING:
            if (c == '"') {
                *p = 0;
                argv[argc++] = wordStart;
                currentMode = DULL;
            }
            continue;
        case IN_WORD:
            if (isspace(c)) {
                *p = 0;
                argv[argc++] = wordStart;
                currentMode = DULL;
            }
            continue;
        }
    }
    if (currentMode != DULL && argc < argv_size)
        argv[argc++] = wordStart;
    return argc;
}
