/* $begin shellmain */
#include "myshell.h"
#include <errno.h>
#define MAXARGS   128

/* Function prototypes */
void eval(char *cmdline);
int parseline(char *buf, char **argv, int* argc);
int builtin_command(char **argv); 
void do_pipe(char* arg1[], char* arg2[], char* arg3[]);

/* Global variables*/
char* pipe_arg1[MAXARGS];   // for piping  
char* pipe_arg2[MAXARGS];
char* pipe_arg3[MAXARGS];
int num, bp1, bp2,mode, pipe_cnt=0;

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
    int argc,i;            /* argument count */

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
        if(mode == 1){  // mode1 = normal mode
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
	        else {//when there is background process!
	           int status;
               printf("%d %s", pid, cmdline);
               waitpid(pid, &status, WNOHANG);
           }

        }else if(mode == 2){    // mode2 = pipe mode
            if(pipe_cnt == 1){  // one pipe
                for(i=0 ; i<num-bp1; i++){
                    pipe_arg2[i] = argv[i+bp1];
                }
                pipe_arg2[num-bp1] = (char*)0;
            }
            else if(pipe_cnt == 2){ // two pipes
                for(i=0; i<num-bp2; i++){
                    pipe_arg3[i] = argv[i+bp2];
                }
                pipe_arg3[num-bp2] = (char*)0;
            }
            
            do_pipe(pipe_arg1, pipe_arg2, pipe_arg3);   // piping
        }
              
	    
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
    int i;
    num = bp1 = bp2 = pipe_cnt= 0;

    buf[strlen(buf)-1] = ' ';  /* Replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* Ignore leading spaces */
	    buf++;
    mode = 1;
    /* Build the argv list */
    *argc = 0;
    while ((delim = strchr(buf, ' '))) {
	    argv[(*argc)++] = buf;
        num++;
	    *delim = '\0';
        if(argv[*argc-1][strlen(argv[*argc-1])-1] == '\"'){ // remove "
           argv[*argc-1][strlen(argv[*argc-1])-1] = '\0';
        }  
	    buf = delim + 1;
	    while (*buf && (*buf == ' ')) /* Ignore spaces */
            buf++;
        if(*buf == '|'){    // if pipe exist
            pipe_cnt++;
            mode=2;
            if(pipe_cnt == 1){  // one pipe
                bp1 = num;
                for(i=0; i< bp1; i++){
                    pipe_arg1[i] = argv[i];
                }
                pipe_arg1[bp1] = (char*)0;
            }else if(pipe_cnt == 2){    // two pipes
                bp2 = num;
                for(i=bp1; i<bp2; i++){
                    pipe_arg2[i-bp1] = argv[i]; 
                }
                pipe_arg2[bp2] = (char*)0;
            }
            buf++;
        }
        while (*buf && (*buf == ' ')) /* Ignore spaces */
            buf++;
        if(*buf == '\"'){   // remove "
            buf++;
        }    
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

/*$begin do_pipe*/
/* do_pipe - run arguments through pipes */
void do_pipe(char* arg1[], char* arg2[], char* arg3[]){
    int fd[2], fd2[2];  /* file descriptor */
    int status; /* waiting staus */
    pid_t pid1, pid2, pid3;   /* process id */

    if(pipe_cnt == 1){  // one pipe - ex) ls -al | grep ...
        if(pipe(fd) == -1){
        unix_error("pipe : pipe error");
        exit(1);
        }

        if( (pid1= Fork()) == 0){   // run  first argument using first output
            dup2(fd[1], 1);
            close(fd[0]);
            execvp(arg1[0], arg1);
            unix_error("error");
            exit(1);
        } 
        if( (pid2 = Fork() ) == 0){ // run second argument
            dup2(fd[0], 0);
            close(fd[1]);
            execvp(arg2[0], arg2);
            unix_error("error");
            exit(1);
        }
       close(fd[0]);
       close(fd[1]);

       if(waitpid(pid1, &status, 0) < 0)
           unix_error("waitfg : waitpid error");
       if(waitpid(pid2, &status, 0) < 0)
           unix_error("waitfg : waitpid error"); 

    }else{  // two pipes - ex) cat filename | grep -v "abc" | sort -r
        if(pipe(fd) == -1){
            unix_error("pipe error");
            exit(1);
        }    
        if(pipe(fd2) == -1){
            unix_error("pipe error");
            exit(1);
        }   
        if( (pid1= Fork()) == 0){ // run first argument   
            dup2(fd[1], 1);
            close(fd[0]);
            close(fd[1]);
            close(fd2[1]);
            close(fd2[0]);
            execvp(arg1[0], arg1);
            unix_error("error");
            exit(1);
        } 
        if( (pid2 = Fork() ) == 0){ // run second argument using first output
            dup2(fd[0], 0);
            dup2(fd2[1], 1);
            close(fd[0]);
            close(fd[1]);
            close(fd2[1]);
            close(fd2[0]);
            execvp(arg2[0], arg2);
            unix_error("error");
            exit(1);
        }
        if( (pid3 = Fork()) ==0){   // run third argument using second output
            dup2(fd2[0], 0);
            close(fd[0]);
            close(fd[1]);
            close(fd2[1]);
            close(fd2[0]);
            execvp(arg3[0], arg3);
            exit(1);
        }

        close(fd[0]);
        close(fd[1]);
        close(fd2[1]);
        close(fd2[0]);
        
        if(waitpid(pid1, &status, 0) < 0)
           unix_error("waitfg : waitpid error");
        if(waitpid(pid2, &status, 0) < 0)
           unix_error("waitfg : waitpid error");
        if(waitpid(pid3, &status, 0) < 0)
            unix_error("waitfg : waitpid error");

    }

}
/*$end do_pipe*/
