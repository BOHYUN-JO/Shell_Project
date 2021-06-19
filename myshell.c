/* $begin shellmain */
#include "myshell.h"
#include <errno.h>
#define MAXARGS   128

/* Function prototypes */
void eval(char *cmdline);
int parseline(char *buf, char **argv, int* argc);
int builtin_command(char **argv); 
void do_pipe(char* arg1[], char* arg2[], char* arg3[], int bg, char* cmd);
void sigchild_handler(int sig);
void sigint_handler(int sig);
void sigtstp_handler(int sig);
void change_job_status(pid_t pid, int stat);
void print_done_list();
void print_jobs();
void addjob(pid_t pid, char* cmd, int state);
void deletejob();
int getjid(pid_t pid);
pid_t getpid_usingjid(int jid);
void addfgjob(pid_t pid, char* cmd);
pid_t getpid_usingstate();
char* getcmd(pid_t pid);


/* Global variables*/
char* pipe_arg1[MAXARGS];   // for piping  
char* pipe_arg2[MAXARGS];
char* pipe_arg3[MAXARGS];
int num, bp1, bp2,mode, pipe_cnt=0;
sigset_t mask, prev;
int print_flag = 0; // if there is somgthing to print : 1 else : 0
Job* head = NULL;   // job list head

int main() 
{
    char cmdline[MAXLINE]; /* Command line */
    char* msg;
    Signal(SIGCHLD, sigchild_handler);
    Signal(SIGINT, sigint_handler);
    Signal(SIGTSTP, sigtstp_handler);

    while (1) {
    	/* Read */
    	printf("CSE4100:P4-myshell> ");                   
    	msg = fgets(cmdline, MAXLINE, stdin);
    	if (feof(stdin))
    	    exit(0);
    
    	/* Evaluate */
    	eval(cmdline);
        if(print_flag){ // if there is something to print
            print_flag = 0;
            print_done_list();
            deletejob();
        }
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

    if(strcmp(argv[0], "exit")==0 ){    // exit shell
        exit(0);
    }

    if(!strcmp(argv[0], "cd")){ // change directory
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
    else if(!strcmp(argv[0], "jobs")){  // print background tasks
        print_jobs();
        deletejob();
    }
    else if(!strcmp(argv[0], "kill")){  // kill specific process
        char temp[4];
        strcpy(temp, ++argv[1]);
        int jid = atoi(temp);
        pid = getpid_usingjid(jid);
        kill(pid, SIGINT);
    }else if(!strcmp(argv[0], "bg")){   // stopped background -> running background job
        char temp[4];
        char tmpcmd[30];
        strcpy(temp, ++argv[1]);
        int jid = atoi(temp);   // get job id from user input
        pid = getpid_usingjid(jid);     // get pid by job id
        strcpy(tmpcmd, getcmd(pid));
        tmpcmd[strlen(tmpcmd)-1] = '\0';
        strcat(tmpcmd, " &");   
        printf("[%d]  %s\n", jid, tmpcmd);
        kill(pid, SIGCONT);     // send SIGCONT signal 
        change_job_status(pid, 4);  // change job status
    }
    else if (!builtin_command(argv)) { //quit -> exit(0), & -> ignore, other -> run        
        if(sigemptyset(&mask) != 0){    // empty signal set
            unix_error("error");
        }
        if(sigaddset(&mask, SIGCHLD) != 0){  // add SIGCHLD signal
            unix_error("error");
        }
        if(sigaddset(&mask, SIGTSTP) != 0){  // add SIGTSTP signal
            unix_error("error");
        }
        if(sigprocmask(SIG_BLOCK, &mask, NULL) != 0){   // block signal
            unix_error("error");
        }
        
        if(mode == 1){  // mode1 = normal mode
            if( (pid = Fork()) == 0 ){  // Child runs user job
                if(sigprocmask(SIG_UNBLOCK, &mask, NULL) != 0){ // unblock signal
                    unix_error("error");
                }
                
                if (execve(argv[0], argv, environ) < 0) {	//ex) /bin/ls ls -al &
                    printf("%s: Command not found.\n", argv[0]);
                    exit(0);
                }
            }
            if(!bg){
                addjob(pid, cmdline, 1);    // add foreground job
            }else{
                addjob(pid, cmdline, 2);    // add background job
            }

            if(sigprocmask(SIG_UNBLOCK, &mask, NULL) != 0){ // unblock signal
                unix_error("error");
            }

           /* Parent waits for foreground job to terminate */
	        if (!bg){ 
	            int status;
                if(waitpid(pid, &status, WUNTRACED|0) < 0)
                    unix_error("waitfg : waitpid error");
	        }   
	        else {//when there is background process!
	           int status;
               printf("[%d] %d\n", getjid(pid), pid);
               waitpid(pid, &status, WNOHANG);
            }

        }else if(mode == 2){    // mode2 = pipe mode
            if(sigprocmask(SIG_UNBLOCK, &mask, NULL) != 0){
                    unix_error("error");
            }
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
           
            do_pipe(pipe_arg1, pipe_arg2, pipe_arg3, bg, cmdline);   // piping
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

    if(strcmp(argv[0], "exit") && strcmp(argv[0], "quit") && strcmp(argv[0], "cd") && strcmp(argv[0], "jobs") && strcmp(argv[0], "kill") && strcmp(argv[0], "bg") ){
        strcat(temp, argv[0]);
        argv[0] = temp;
    }
        
    /* Should the job run in the background? */
    if ((bg = (*argv[*argc-1] == '&')) != 0 ){
        argv[--(*argc)] = NULL;
    }
    else if( (bg = ( argv[*argc-1][strlen(argv[*argc-1])-1] == '&' )) != 0){
       argv[*argc-1][strlen(argv[*argc-1])-1] = '\0'; 
    }
    return bg;
}
/* $end parseline */

/*$begin do_pipe*/
/* do_pipe - run arguments through pipes */
void do_pipe(char* arg1[], char* arg2[], char* arg3[], int bg, char* cmd){
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
        /* Parent waits for foreground job to terminate */
	        if (!bg){ 
	            int status;
                if(waitpid(pid1, &status, 0) < 0)
                   unix_error("waitfg : waitpid error");
                if(waitpid(pid2, &status, 0) < 0)
                    unix_error("waitfg : waitpid error");    
	        }   
	        else {//when there is background process!
               addjob(pid2, cmd, 2);
               printf("[%d] %d\n", getjid(pid2), pid2);
               waitpid(pid2, &status, WNOHANG);
            }

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
        
        if (!bg){ 
            if(waitpid(pid1, &status, 0) < 0)
                unix_error("waitfg : waitpid error");
            if(waitpid(pid2, &status, 0) < 0)
                unix_error("waitfg : waitpid error");   
            if(waitpid(pid3, &status, 0) < 0)
                unix_error("waitfg : waitpid error");     
	    }   
        else {//when there is background process!
            addjob(pid3, cmd, 2);
            printf("[%d] %d\n", getjid(pid3), pid3);
            waitpid(pid3, &status, WNOHANG);
        }  
    }
}
/*$end do_pipe*/

/*$begin sigchild_handler*/
/*sigchild_handler - handle SIGCHILD signal */
void sigchild_handler(int sig){
    int status;
    pid_t pid;
    while( (pid = waitpid(-1, &status, WUNTRACED|WNOHANG)) > 0 ){
        if(WIFSIGNALED(status) != 0){
            change_job_status(pid, 2);
            print_flag = 1;
        }
        else if(WIFSTOPPED(status)){    // error point...
            change_job_status(pid, 3);
            print_flag = 1;
        }
        else{
            change_job_status(pid, 1);
            print_flag = 1;
        }
            
    }
    return; 
}
/*$end sigchild_handler*/

/*$begin sigint_handler*/
/*sigint_handler - handle SIGINT signal */
void sigint_handler(int sig){
    sio_puts("\n");
}
/*$end sigint_handler*/

/*$begin sigtstp_handler*/
/*sigtstp_handler - handle SIGTSTP signal */
void sigtstp_handler(int sig){
    pid_t pid;
    sio_puts("\n");
    pid = getpid_usingstate();  // get pid (return last fg command`s pid)
    kill(pid, SIGTSTP);
    change_job_status(pid, 3);  // it is bad code
    print_flag = 1;
    return;
}
/*$end sigtstp_handler*/

/*$begin change_job_status*/
/*change_job_status - change job status*/
void change_job_status(pid_t pid, int stat){
    Job* ptr; 
    int fgcnt = 0;
    int bgcnt = 0;
    for(ptr = head; ptr ; ptr= ptr->next){
        if(ptr->state == 1){    // foreground job
            fgcnt++;
        }else{  // background job
            bgcnt++;
        }

        if(ptr->pid == pid){
            if(stat == 1){  // running -> done
                strcpy(ptr->status, "Done");
                break; 
            }
            else if(stat == 2){ // running -> terminated
                strcpy(ptr->status, "Terminated");
                break;
            }else if(stat == 3){    // foreground -> background, stopped
                ptr->state = 2;
                ptr->job_id = bgcnt+1;
                strcpy(ptr->status, "Stopped");
                break;
            }else if(stat == 4){    // stopped -> running
                strcpy(ptr->status, "Running");
                break;    
            }
        }
    }
}
/*$end change_job_status*/

/*$begin addjob*/
/*addjob - add job to job list*/
void addjob(pid_t pid, char* cmd, int state){
    Job* newNode = (Job*)malloc(sizeof(Job));
    Job* ptr;
    int fgcnt = 1;
    int bgcnt = 1;
    newNode->pid = pid;
    newNode->next = NULL;
    newNode->prev = NULL;
    newNode->state = state;
    strcpy(newNode->cmd, cmd);
    strcpy(newNode->status, "Running"); // default - running
    newNode->job_id = 1;
    if(head == NULL){   // if list is empty
        head = newNode;
    }
    else{
        for(ptr = head; ptr->next; ptr = ptr->next){
            if(ptr->state == 1){    // fg
                fgcnt++;
            }else{  // bg
                bgcnt++;
            }
        }
        if(state == 1){ // fg
            newNode->job_id = fgcnt +1;
        }else{  // bg
            newNode->job_id = bgcnt +1;
        }
        newNode->prev = ptr;
        ptr->next = newNode;
    }
}
/*$end addjob*/

/*$begin print_done_list*/
/* print_done_list - print specific job list*/
void print_done_list(){
    Job* ptr;
    if(head == NULL){
        return;
    }
    for(ptr = head; ptr ; ptr= ptr->next){
        if(ptr->state == 2 && (!strcmp(ptr->status, "Done") || !strcmp(ptr->status, "Terminated") || !strcmp(ptr->status, "Stopped"))){
            printf("[%d] %s\t\t\t%s", ptr->job_id, ptr->status, ptr->cmd);
        }
    }
}
/*$end print_done_list*/

/*$begin print_jobs*/
/* print_jobs - print job list */
void print_jobs(){
    Job* ptr;
    if(head == NULL){
        return;
    }
    for(ptr= head; ptr; ptr = ptr->next){
        if(ptr->state == 2)
            printf("[%d] %s\t\t\t%s", ptr->job_id, ptr->status, ptr->cmd);
    }
}
/*$end print_jobs */

/*$begin deletejob*/
/*deletejob - delete specific jobs*/
void deletejob(){
    Job* ptr = head;
   
    if(ptr == NULL){
        return;
    }
    if(ptr->next == NULL){  // only one Node
        if(!strcmp(ptr->status, "Done") || !strcmp(ptr->status, "Terminated")){
            free(ptr);
            head = NULL;
        }
    }
    else{   // more than two Nodes
        while(ptr != NULL){
            Job* del;
            if(ptr == head){    // ptr == head
                if(!strcmp(ptr->status, "Done") || !strcmp(ptr->status, "Terminated")){
                    if(ptr->next == NULL){
                        free(ptr);
                        head = NULL;
                    }else{
                        del = ptr;
                        head = ptr->next;
                        ptr = ptr->next;
                        free(del);
                    }
                }else{
                    ptr = ptr->next;
                }
            }
            else{   // ptr != head
                if(!strcmp(ptr->status, "Done") || !strcmp(ptr->status, "Terminated")){
                    if(ptr->next == NULL){
                        del = ptr;
                        ptr->prev->next = NULL;
                        ptr = ptr->next;
                        free(del);
                    }else{
                        del = ptr;
                        ptr->prev->next = ptr->next;
                        ptr->next->prev = ptr->prev;
                        ptr = ptr->next;
                        free(del);
                    }
                }else{
                    ptr = ptr->next;
                }
            }
        }
    }
}
/*$end deletejob*/

/*$begin getjid*/
/* get job_id from joblist using pid*/
int getjid(pid_t pid){
    Job* ptr;
    for(ptr=head; ptr ; ptr = ptr->next){
        if(ptr->pid == pid){
            return ptr->job_id;
        }
    }
    return -1;  // if there is no such pid
}
/*$end getjid */

/*$begin getpid_usingjid*/
/* getpid_usingjid - get pid from joblist using job_id*/
pid_t getpid_usingjid(int jid){
    Job* ptr;
    for(ptr=head; ptr ; ptr = ptr->next){
        if(ptr->job_id == jid ){
            return ptr->pid;
        }
    }
    
    return -1;  // if there is no such job_id
}
/*$end getpid_usingjid*/

/*$begin getpid_usingstate*/
/* getpid_usingstate - get pid from joblist using state*/
pid_t getpid_usingstate(){
    Job* ptr;
    pid_t pid = -1;
    for(ptr=head; ptr; ptr = ptr->next){
        if(ptr->state == 1){
            pid = ptr->pid;
        }
    }
    
    return pid;
}
/*$end getpid_usingstate*/

/*$begin getcmd*/
/* getcmd - get command from joblist using pid*/
char* getcmd(pid_t pid){
    Job* ptr;
    for(ptr=head; ptr; ptr = ptr->next){
        if(ptr->pid == pid){
            return ptr->cmd;
        }
    }
}
/*end getcmd*/