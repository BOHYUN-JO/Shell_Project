/* $begin shellmain */
#include "myshell.h"
#include <errno.h>
#define MAXARGS   128

/* Function prototypes */
void eval(char *cmdline);
int parseline(char *buf, char **argv, int* argc);
int builtin_command(char **argv); 

int main() 
{
    char cmdline[MAXLINE]; /* Command line */
    char* msg;

    while (1) {
	/* Read */
	printf("CSE4100:P4-myshell> ");                   
	msg = fgets(cmdline, MAXLINE, stdin);
	if (feof(stdin))
	    exit(0);

	/* Evaluate */
	eval(cmdline);
    } 
}
/* $end shellmain */
  
/* $begin eval */
/* eval - Evaluate a command line */
void eval(char *cmdline) 
{
    char *argv[MAXARGS]; /* Argument list execve() */
    char buf[MAXLINE];   /* Holds modified command line */
    int bg;              /* Should the job run in bg or fg? */
    pid_t pid;           /* Process id */
    int argc;            /* argument count */

    strcpy(buf, cmdline);
    bg = parseline(buf, argv, &argc); 
    if (argv[0] == NULL)  
	    return;   /* Ignore empty lines */

    if(strcmp(argv[0], "exit")==0 ){
        exit(0);
    }

    if(!strcmp(argv[0], "cd")){
        if(argc == 1){  // cd 
            if(chdir( getenv("HOME"))){
                printf("Can not change directory!\n");
            }
        }else if(argc == 2){    // cd [directory]
            if(chdir(argv[1]))
                printf("Can not change directory!\n");
        }else{  // error
            printf("Enter proper directory!\n");
        }
    }
    else if (!builtin_command(argv)) { //quit -> exit(0), & -> ignore, other -> run
        if( (pid = Fork()) == 0 ){  // Child runs user job
            if (execve(argv[0], argv, environ) < 0) {	//ex) /bin/ls ls -al &
                printf("%s: Command not found.\n", argv[0]);
                exit(0);
            }
        }        
    
	    /* Parent waits for foreground job to terminate */
	    if (!bg){ 
	        int status;
            if(waitpid(pid, &status, 0) < 0)
                unix_error("waitfg : waitpid error");
	    }   
	    else //when there is background process!
	        printf("%d %s", pid, cmdline);
    }
    return;
}

/* If first arg is a builtin command, run it and return true */
int builtin_command(char **argv) 
{
    if (!strcmp(argv[0], "quit") || !strcmp(argv[0], "exit")) /* quit command */
	    exit(0); 
    if (!strcmp(argv[0], "&"))    /* Ignore singleton & */
	    return 1;
    
    return 0;                     /* Not a builtin command */
}
/* $end eval */

/* $begin parseline */
/* parseline - Parse the command line and build the argv array */
int parseline(char *buf, char **argv, int *argc) 
{
    char temp[MAXLINE] = "/bin/";       /* Temp character array*/   
    char *delim;         /* Points to first space delimiter */
    int bg;              /* Background job? */

    buf[strlen(buf)-1] = ' ';  /* Replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* Ignore leading spaces */
	    buf++;
    
    /* Build the argv list */
    *argc = 0;
    while ((delim = strchr(buf, ' '))) {
	    argv[(*argc)++] = buf;
	    *delim = '\0';
	    buf = delim + 1;
	    while (*buf && (*buf == ' ')) /* Ignore spaces */
            buf++;
    }
    argv[*argc] = NULL;
    
    if (*argc == 0)  /* Ignore blank line */
	    return 1;

    if(strcmp(argv[0], "exit") != 0 && strcmp(argv[0], "quit") !=0 && strcmp(argv[0], "cd") != 0 ){
        strcat(temp, argv[0]);
        argv[0] = temp;
    }
        

    /* Should the job run in the background? */
    if ((bg = (*argv[*argc-1] == '&')) != 0)
	    argv[--(*argc)] = NULL;

    return bg;
}
/* $end parseline */


