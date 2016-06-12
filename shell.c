#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "parse.h"
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

/* 
	Cameron Fabbri
	Operating Systems Lab	
	shell.c
	2-20-2015

	Linux Shell written in C. Uses parse.h for a parser.
*/ 

/* Function Declarations */
void help          ();
void history       ();
void checkJobs     ();
void quitShell     ();
char *buildPrompt  ();
void initializeJobs();

void killProcess     (char *pid);
int  isBuiltInCommand(char *cmd);
char *trim_          (char *cmdLine);
void executeHistory  (char *lineNumber, parseInfo *info);
int  executeCmd      (char *cmd       , parseInfo *info);
void addJob          (pid_t childPid, char *cmd);
int  backgroundJob   (parseInfo *info);


/* array to store jobs and job id */
char *jobs[100][2];

/* built in commands */
enum BUILTIN_COMMANDS {NO_SUCH_BUILTIN=0, EXIT, JOBS, HISTORY, CD, KILL, HELP, EXECHIST};

/* various declarations */
int infile    ;
int outfile   ;
int stdoutback;
int stdinback ;
int status    ;
int null_file ;
 
int main (int argc, char **argv)
{
	int  childPid;
  	char cwd[1024];
	int  counter = 1;
	char *buf;
	
	char               *cmdLine;
 	struct commandType *com;  /*com stores command name and Arg list for one command.*/
  	parseInfo          *info; /*info stores all the information returned by parser.*/


	buf = (char *)malloc(10*sizeof(char));
	buf = getlogin();

	/* removes history file for a fresh file on each usage of the shell */
	remove("/tmp/.c_history");
	initializeJobs();

	printf("\nShell written for Linux in C.  Type help to bring up the help screen.\n\n");
  	
	while(1)
 	{
		FILE *f = fopen("/tmp/.c_history", "a");
	
		/* retrieves and prints the current working directory */	
		getcwd (cwd, sizeof(cwd)); 
		printf("%s@%s", buf, cwd);
		
		#ifdef UNIX
    		cmdLine = readline(buildPrompt());
			if (cmdLine == NULL) 
			{
	      	fprintf(stderr, "Unable to read command\n");
		      continue;
		    }
		#endif
					
		if (f == NULL)
		{
			printf("Error opening history file");
			exit(1);
		}

    	/*calls the parser*/
    	info = parse(cmdLine);
    	if (info == NULL)
		{
      	free(cmdLine);
      	continue;
    	}

		/* Manages STDIN and STDOUT */
		if (info->boolInfile == 1)
		{
			infile    = open(info->inFile, O_RDONLY, 0666);
			stdinback = dup(fileno(stdin));

			dup2 (infile, fileno(stdin));
			close(infile);	
		}
		if (info->boolOutfile == 1)
		{
			outfile    = open(info->outFile, O_CREAT | O_RDWR, 0666);
			stdoutback = dup(fileno(stdout));

			dup2 (outfile, fileno(stdout));
			close(outfile);
		}

	   /*com contains the info. of the command before the first "|"*/
	   com=&info->CommArray[0];
		
		if ((com == NULL)  || (com->command == NULL)) 
		{
      	free_info(info);
	      free(cmdLine);
   	   continue;
    	}
 
		/*com->command tells the command name of com*/
 		if      (isBuiltInCommand(com->command) == KILL    ) killProcess(com->VarList[1]);
		else if (isBuiltInCommand(com->command) == CD      ) chdir(com->VarList[1]);
		else if (isBuiltInCommand(com->command) == JOBS    ) checkJobs();
 		else if (isBuiltInCommand(com->command) == EXIT    ) quitShell();
		else if (isBuiltInCommand(com->command) == HISTORY ) history();
		else if (isBuiltInCommand(com->command) == HELP    ) help();
 		else if (isBuiltInCommand(com->command) == EXECHIST) 
		{
			parseInfo *info;
			char      *cmdLine_;
			char      *split_ = strtok(cmdLine, "!");

			/* Executes the previous command */
		   if (strncmp(split_, "-1", strlen("-1")) == 0)
			{
				char  tmp[1024];
				char *cmdLine;
				FILE *fp = fopen("/tmp/.c_history", "r");
				
				while(!feof(fp)) fgets(tmp, 1024, fp);
				
				cmdLine  = trim_(tmp);
				cmdLine_ = strtok(cmdLine, " ");

	         info = parse(cmdLine_);
				executeHistory(cmdLine_, info);
				
			} else executeHistory(split_, info);			

		}
		else
		{
			executeCmd(cmdLine, info);
	
			/* Returns to shell after STDIN and STDOUT */	
			if(info->boolInfile == 1)
			{
				dup2(stdinback, fileno(stdin));
				close(stdinback);
			}
			if(info->boolOutfile == 1)
			{
				dup2(stdoutback, fileno(stdout));
				close(stdoutback);
			}
		}

		fprintf(f, "%d %s\n", counter, cmdLine);
		fclose(f);
		counter++;

    	free_info(info);
		free(cmdLine);
  }/* while(1) */
}

char *buildPrompt() { return  "$ "; }

int isBuiltInCommand(char * cmd)
{ 
	if ( strncmp(cmd, "exit"   , strlen("exit"   )) == 0) return EXIT;
  	if ( strncmp(cmd, "history", strlen("history")) == 0) return HISTORY;
  	if ( strncmp(cmd, "cd"     , strlen("cd"     )) == 0) return CD;
  	if ( strncmp(cmd, "kill"   , strlen("kill"   )) == 0) return KILL;
  	if ( strncmp(cmd, "help"   , strlen("help"   )) == 0) return HELP;
  	if ( strncmp(cmd, "job"    , strlen("job"    )) == 0) return JOBS;
  	if ( strncmp(cmd, "!"      , strlen("!"      )) == 0) return EXECHIST;
	
	return NO_SUCH_BUILTIN;
}

void help()
{
	printf("\n HELP AND USAGE:                                          \n\n");
	printf(" This is a shell written in C to be used with Linux.          \n");
	printf(" Commands in your $PATH variable are available to use.      \n\n");
	printf(" Functions:                                                 \n\n");
	printf(" help: shows this help screen                                 \n");
	printf(" exit: exits the shell. If there are background processes     \n");
	printf("       still running, you will need to kill those first.      \n");
	printf(" history: shows the history of the current session, store     \n");
	printf("          in /tmp/.c_history.                                 \n");
	printf("    !: you can execute a command from your history using the !\n"); 
	printf("    command follwed by the history number                     \n");
	printf("    Typing in !-1 will run the previous command.            \n\n");
	printf(" cd <directory>: changes the current directory.             \n\n");
	printf(" kill <pid>: kills the process with that pid number         \n\n");
	printf(" background: putting a & at the end of a command will       \n\n");
	printf(" place it in the background                                 \n\n");
	printf(" job: lists the jobs running in the background with their     \n");
	printf("      respective pid.                                       \n\n");
}


void history()
{			
	int   c;
	FILE *file;
	file = fopen("/tmp/.c_history", "r");

	if (file)
	{
		while ((c = getc(file)) != EOF) putchar(c);
		fclose(file);
	}
}

void checkJobs()
{
	int i, j;
		
	for (i =  0; jobs[i][1] != "..."; i++) {}
	if  (i == 0) 
	{
		printf("No jobs running!\n");
	}
	else
	{
		fprintf(stderr, "\nID     Process\n\n");
			
		for (j = 0; j < i; j++)
		{
			printf("%s  ", jobs[j][0]);
			printf("%s\n", jobs[j][1]);
		}
	}
}

void quitShell()
{
	int i;

	for (i = 0; jobs[i][1] == "..."; i++){}
	
	if (i == 100) exit(1);
	else printf("You still have jobs running in the background!\n");
}

void killProcess(char *pid)
{
	int status;
	int result; 
	int i, j;
	int kill_ID = atoi(pid);

	for (i = 0; i < 100; i++)
	{
		if (strncmp(jobs[i][0], pid, strlen(pid)) == 0)
		{
			waitpid(kill_ID, &status, WNOHANG);
			result = kill(kill_ID, SIGKILL);
			if (result == -1) printf("Job %s killed successfully.\n", jobs[i][1]);
			jobs[i][0] = "0";
			jobs[i][1] = "...";
		}
	} 
}

int backgroundJob(parseInfo *info)
{
	return info->boolBackground;
}

void executeHistory(char * lineNumber, parseInfo *info)
{
	FILE *histFile = fopen("/tmp/.c_history", "r");
	char *cmdLine;

	char tmp[256] = {0x0};
	
	while (histFile != '\0' && fgets(tmp, sizeof(tmp), histFile) != '\0')
	{
		if (strstr(tmp, lineNumber)) 
		{
			const char *arguments[2];
			char *cmdLine = strtok(tmp, lineNumber);
			cmdLine       = trim_ (cmdLine);
	      info          = parse (cmdLine);
			executeCmd(cmdLine, info);		
			break;
		}
	}
}

char *trim_(char *str)
{
	char *end;

	/* Trim leading space */
	while(isspace(*str)) str++;
  
	if(*str == 0) return str;

	/* Trim trailing space */
	end = str + strlen(str) - 1;
	while(end > str && isspace(*end)) end--;

	/* Write new null terminator */
	*(end+1) = 0;

	return str;
}

void initializeJobs()
{
	int i;
	for (i = 0; i < 100; i++)
	{
		jobs[i][0] = malloc(10);
		jobs[i][0] = "0";
		
		jobs[i][1] = malloc(100);
		jobs[i][1] = "...";
	}
}

void addJob(pid_t childPid, char *cmd)
{
	char *pid = malloc(10);
	char *cmd_ = malloc(100);
	int  i;

   memcpy(cmd_, cmd, strlen(cmd)+1);
	sprintf(pid, "%d", childPid);

	for (i = 0; jobs[i][1] != "..."; i++){}
	
	jobs[i][0] = pid;
	jobs[i][1] = cmd_;
	
	fprintf(stderr, "added job: %s with pid: %s\n", cmd_, pid);
}

int executeCmd(char *cmd, parseInfo *info)
{
	pid_t childPid;
	int childStatus;
	struct commandType *com;

	com = &info->CommArray[0];	
	childPid = fork();

	if (childPid == 0)
	{
		if (backgroundJob(info))
		{
			setpgid(0,0);
			null_file = open("/dev/null", O_RDWR);
			dup2(null_file, fileno(stdout));
			close(null_file);
		}
		
		execvp(com->command, com->VarList);
		exit(0);
	}
	else
	{
		char *cmd;
		pid_t tpid;
		
		cmd = (char*)com->command;
		
		if (backgroundJob(info))
		{
			addJob(childPid, cmd);
		}
		else
		{
			while (tpid != childPid)
			{
				tpid = wait(&childStatus);
			}
			return childStatus;
		}
	}
}  
