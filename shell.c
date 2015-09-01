#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <math.h>
#include <signal.h>

#define FALSE 0
#define TRUE 1
#define INPUT_STRING_SIZE 80
#define PATH_MAX 80
#define MAX_TABLE_WIDTH 60
#define COLUMN 20

#include "io.h"
#include "parse.h"
#include "process.h"
#include "shell.h"

process *first_process = NULL;

int cmd_quit(tok_t arg[]) {
  printf("Bye\n");
  exit(0);
  return 1;
}

int cmd_help(tok_t arg[]);

int cmd_chDir(tok_t arg[]);

int cmd_listProcs(tok_t arg[]);

int cmd_wait();

char * toArray(int number) {
	int n = log10(number) + 1;
	int i;
	char *numberArray = calloc(n, sizeof(char));

	for ( i = 0; i < n; ++i, number /= 10 ) {
		numberArray[i] = number % 10;
	}
	return numberArray;
}

void fillLine(int remainder) {
	int i;
	for (i=0; i<remainder; i++) {
		fprintf(stdout," ");
	}
	fprintf(stdout, "|\n");
}

/* Command Lookup table */
typedef int cmd_fun_t (tok_t args[]); /* cmd functions take token array and return int */
typedef struct fun_desc {
  cmd_fun_t *fun;
  char *cmd;
  char *doc;
} fun_desc_t;

fun_desc_t cmd_table[] = {
	{cmd_help, "?", "show this help menu"},
	{cmd_quit, "quit", "quit the command shell"},
	{cmd_chDir, "cd", "change directory to argv"},
	{cmd_listProcs, "listProcs", "list all processes started by this shell"},
	{cmd_wait, "wait", "wait until all background processes have completed"}
	//{cmd_fg, "fg", "move process with pid given by argv to foreground"},
	//{cmd_bg, "bg", "move process with pid given by argv to background"}
};

//Traverse linked list recursively, waiting for each process in bg to finish
int waitForProcesses(process *curr) {
	int count = 0;
	if(curr->next != NULL) {
		if ( (curr->next->completed == 'N') && (curr->next->stopped =='N') && (curr->next->background == 'Y')) {
			waitpid(curr->next->pid, 0, 0);
			curr->next->completed = 'Y';
			count++;
		}
		return count + waitForProcesses(curr->next);
	}
	return 0;
}

int cmd_wait() {
	fprintf(stdout, "%d background processes completed.\n", waitForProcesses(first_process));
	return 1;
}

int cmd_listProcs(tok_t arg[]) {
	process *curr = first_process;
	int i = 0;
	int printed = 0;
	int k;
	//Printing process structure
	while (curr->next != NULL) {
		fprintf(stdout, "|----------------------------------------------------------|\n");
		if ( (curr->next->completed == 'Y') || (curr->next->stopped == 'Y') )
			{fprintf(stdout, "|Process number %2d (Inactive)                              |\n", i);}
		else
			{fprintf(stdout, "|Process number %2d (Active)                                |\n", i);} 
		fprintf(stdout, "|------------------+---------------------------------------|\n");
		fprintf(stdout, "|PID:              | %d%n", curr->next->pid, &printed);
		fillLine( (MAX_TABLE_WIDTH-printed)-1 );
			/* --FOR PRINTING FILE DESCRIPTORS:--
			fprintf(stdout, "|Stdin:            | %d%n", curr->next->stdin, &printed);
			fillLine( (MAX_TABLE_WIDTH-printed)-1 );
			fprintf(stdout, "|Stdout:           | %d%n", curr->next->stdout, &printed);
			fillLine( (MAX_TABLE_WIDTH-printed)-1 );
			fprintf(stdout, "|Stderr:           | %d%n", curr->next->stderr, &printed);
			fillLine( (MAX_TABLE_WIDTH-printed)-1 );
			*/
		fprintf(stdout, "|Completed:        | %c%n", curr->next->completed, &printed);
		fillLine( (MAX_TABLE_WIDTH-printed)-1 );
		fprintf(stdout, "|Stopped:          | %c%n", curr->next->stopped, &printed);
		fillLine( (MAX_TABLE_WIDTH-printed)-1 );
		fprintf(stdout, "|Background:       | %c%n", curr->next->background, &printed);
		fillLine( (MAX_TABLE_WIDTH-printed)-1 );
		fprintf(stdout, "|Arguments:        | %d%n", curr->next->argc, &printed);
		fillLine( (MAX_TABLE_WIDTH-printed)-1 );
		for (k = 0; k < curr->next->argc; k++) {
			fprintf(stdout, "|Arg number [%d]:   | %s%n", k, curr->next->argv[k], &printed);
			fillLine( (MAX_TABLE_WIDTH-printed) - 1);
		}
		fprintf(stdout, "|------------------+---------------------------------------|\n\n");
		curr = curr->next;
		i++;
	}
	return 1;
}

int cmd_chDir(tok_t arg[]) {
  chdir(arg[0]);
  return 1;
}

int cmd_help(tok_t arg[]) {
  int i;
  for (i=0; i < (sizeof(cmd_table)/sizeof(fun_desc_t)); i++) {
    printf("%s - %s\n",cmd_table[i].cmd, cmd_table[i].doc);
  }
  return 1;
}

int lookup(char cmd[]) {
  int i;
  for (i=0; i < (sizeof(cmd_table)/sizeof(fun_desc_t)); i++) {
    if (cmd && (strcmp(cmd_table[i].cmd, cmd) == 0)) return i;
  }
  return -1;
}

void init_shell()
{
  /* Check if we are running interactively */
  shell_terminal = STDIN_FILENO;

  /** Note that we cannot take control of the terminal if the shell
      is not interactive */
  shell_is_interactive = isatty(shell_terminal);

  if(shell_is_interactive){

    /* force into foreground */
    while(tcgetpgrp (shell_terminal) != (shell_pgid = getpgrp()))
      kill( - shell_pgid, SIGTTIN);

    shell_pgid = getpid();
    /* Put shell in its own process group */
    if(setpgid(shell_pgid, shell_pgid) < 0){
      perror("Couldn't put the shell in its own process group");
      exit(1);
    }

    /* Take control of the terminal */
    tcsetpgrp(shell_terminal, shell_pgid);
    tcgetattr(shell_terminal, &shell_tmodes);
  }
  /** YOUR CODE HERE */
}

void completionListener(process* foo) {
	pid_t thispid = fork();
	
	if (thispid == 0) {
		waitpid(foo->pid, 0, 0);
		foo->completed = 'Y';
	}
}

//Recursive function to add process to linked list.
void add_process(process *curr, process *p)
{	
	if (curr->next == NULL) {
		curr->next = p;
		p->prev = curr;
	}
	else
	{add_process(curr->next, p);}
}

process* create_process(char* path, char** argvIN)
{
	process *foo;
	int i;
	FILE *newInput, *newOutput;
	char** newargvIN = malloc(MAXTOKS*sizeof(char*));
  		for (i=0; i<MAXTOKS; i++) newargvIN[i] = NULL;
	int newStart = 0;
	foo = malloc(sizeof(process));
	//Setting standard file desc in process structure
	foo->stdin = 0;
	foo->stdout = 1;
	foo->stderr = 2;
	
	pid_t pid = fork();
	//IO redirection (and updating file desc)
	if (argvIN[1] != NULL) {
		if ( strcmp(argvIN[1],"&") == 0) {
			foo->background = 'Y';
			argvIN[newStart+1] = argvIN[0];
			newStart++;
		}
		else foo->background = 'N';
		if ( (argvIN[newStart+1] != NULL) && (strcmp(argvIN[newStart+1],">") == 0) ) {
			if (pid == 0) {
				newOutput = fopen(argvIN[newStart+2], "a");
				dup2(fileno(newOutput), 1);
			}
			foo->stdout = fileno(newOutput);
			argvIN[newStart+2] = argvIN[newStart];
			newStart += 2;
		}
		if ( (argvIN[newStart+1] != NULL) && (strcmp(argvIN[newStart+1],"<") == 0) ) {
			if (pid == 0) {
				newInput = fopen(argvIN[newStart+2], "r");
				dup2(fileno(newInput), 0);
			}
			foo->stdin = fileno(newInput);
			argvIN[newStart+2] = argvIN[newStart];
			newStart += 2;
		}
	}
	
	if (pid == 0) {
		execv(path, &argvIN[newStart]);
		exit(-1);
	}
	else {
		if (pid == -1) {
			return NULL;
		}

		else {
			//Building process structure
			foo->argv = &argvIN[newStart];
			foo->completed = 'N';
			foo->stopped = 'N';
			if ((foo->background != 'N') && (foo->background != 'Y')) foo->background = 'Y';
			foo->status = 1;
			foo->pid = pid;
			foo->next = NULL;
			foo->prev = NULL;
			i = newStart;
			while (argvIN[i] != NULL)
				{i++;}
			foo->argc = i - newStart;
			add_process(first_process, foo);
			if (foo->background == 'N') {
				waitpid(pid,0,0);
				foo->completed = 'Y';
			}
			else completionListener(foo);
		}	
	}
	return foo;
}

//Recursive function to kill process
int killProcesses(process *curr, int signal) {
	int count = 0;
	if(curr->next != NULL) {
		if ( (curr->next->completed == 'N') && (curr->next->stopped =='N') && (curr->next->background == 'N')) {
			kill(curr->next->pid, signal);
			curr->next->stopped = 'Y';
			count++;
		}
		return count + killProcesses(curr->next, signal);
	}
	return 0;
}


static void handler(int signum) {
	
	if (signum == SIGINT)
		{fprintf(stdout, "\n%d processes ended.\n", killProcesses(first_process, signum));}
	else if (signum == SIGTSTP)
		{fprintf(stdout, "\n%d processes stopped.\n", killProcesses(first_process, signum));}
	
}

//Move all elements (starting at and including "index") right one index
//Pretty elegant solution if I do say so myself....
void shiftTokensRight(int index, tok_t *tokens) {
	tok_t temp[2];
	int i = index;
	
	temp[1] = tokens[i];
	temp[0] = tokens[i+1];
	i++;

	while (tokens[i] != NULL) {
		tokens[i] = temp[(i-1)%2];
		temp[(i-1)%2] = tokens[i+1];
		i++;
	}
	tokens[i] = temp[(i-1)%2];
	tokens[index] = NULL;	
}

int shell (int argc, char *argv[]) {
	char *s = malloc(INPUT_STRING_SIZE+1);	/* user input string */
	tok_t *t;						/* tokens parsed from input */
	
	int fundex = -1;
	pid_t pid = getpid();			/* get current processes PID */
	pid_t ppid = getppid();			/* get parents PID */
	char *buf = NULL;
	char *cwd = getcwd(buf, PATH_MAX);
	char *path = getenv("PATH");
	tok_t *paths = getToks(path);
	int k = 0;
	int isExecutable;
	int background;
	struct sigaction sa;

    	sa.sa_handler = handler;
    	sigemptyset(&sa.sa_mask);
    	sa.sa_flags = SA_RESTART;

	first_process = malloc(sizeof(process));
	first_process->next = NULL;
	first_process->prev = NULL;

    	if (sigaction(SIGINT, &sa, NULL) == -1)
		{fprintf(stdout, "Failed to send request.");}
	else if (sigaction(SIGTSTP, &sa, NULL) == -1)
		{fprintf(stdout, "Failed to send request.");}

	
	init_shell();

	printf("%s running as PID %d under %d\n",argv[0],pid,ppid);

	fprintf(stdout, "%s: ", cwd);
	while ((s = freadln(stdin))){
		char *newCommand = malloc(INPUT_STRING_SIZE*sizeof(char));
		t = getToks(s);			/* break the line into tokens */
		
		k = 0;
		background = 0;
		while ((t[0][k] != '&') && (t[0][k] != '\0')) {
			if (t[0][k+1] == '&') background = 1;
			newCommand[k] = t[0][k];
			k++;
		}
		t[0] = newCommand;
		if (background == 1) {
			shiftTokensRight(1,t);
			t[1] = "&";
		}
		fundex = lookup(t[0]);	/* Is first token a shell literal */
		if(fundex >= 0)	{
			if ( (sizeof(t)/sizeof(char**) > 1) && (t[1][0] == '&') ) cmd_table[fundex].fun(&t[2]);
			else cmd_table[fundex].fun(&t[1]);
			
		}		
		else {
			if (strstr(t[0], "/") != NULL) {	//executable with path defined
				create_process(t[0], t);
			}
			else {					//executable with no path defined
				char *pathResolution = malloc(PATH_MAX*sizeof(char));
				k = 0;
				isExecutable = 0;
				while ( (paths[k] != NULL) && (isExecutable == 0)) {
					strcpy(pathResolution, paths[k]);
					strcat(pathResolution, "/");
        				strcat(pathResolution, t[0]);
					if (access(pathResolution, F_OK|X_OK) == 0)
						{isExecutable = 1;}
					k++;
				}
	
				if (isExecutable == 1) {
					t[0] = pathResolution;
					create_process(pathResolution, t);
				}
				else {
        	  			fprintf(stdout, "Couldn't resolve path for executable '%s'.\n", t[0]);
				}
			}

		}
		cwd = getcwd(buf, PATH_MAX);
		fprintf(stdout, "%s: ", cwd);

	}
  return 0;
}


