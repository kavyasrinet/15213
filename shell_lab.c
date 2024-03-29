/*
 * Name - Kavya Srinet
 * andrew_id : ksrinet 
 * tsh - A tiny shell program with job control
 * 
 * <Put your name and login ID here>
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <errno.h>

/* Misc manifest constants */
#define MAXLINE    1024   /* max line size */
#define MAXARGS     128   /* max args on a command line */
#define MAXJOBS      16   /* max jobs at any point in time */
#define MAXJID    1<<16   /* max job ID */

/* Job states */
#define UNDEF         0   /* undefined */
#define FG            1   /* running in foreground */
#define BG            2   /* running in background */
#define ST            3   /* stopped */

/* 
 * Jobs states: FG (foreground), BG (background), ST (stopped)
 * Job state transitions and enabling actions:
 *     FG -> ST  : ctrl-z
 *     ST -> FG  : fg command
 *     ST -> BG  : bg command
 *     BG -> FG  : fg command
 * At most 1 job can be in the FG state.
 */

/* Parsing states */
#define ST_NORMAL   0x0   /* next token is an argument */
#define ST_INFILE   0x1   /* next token is the input file */
#define ST_OUTFILE  0x2   /* next token is the output file */


/* Global variables */
extern char **environ;      /* defined in libc */
char prompt[] = "tsh> ";    /* command line prompt (DO NOT CHANGE) */
int verbose = 0;            /* if true, print additional output */
int nextjid = 1;            /* next job ID to allocate */
char sbuf[MAXLINE];         /* for composing sprintf messages */

struct job_t {              /* The job struct */
    pid_t pid;              /* job PID */
    int jid;                /* job ID [1, 2, ...] */
    int state;              /* UNDEF, BG, FG, or ST */
    char cmdline[MAXLINE];  /* command line */
};
struct job_t job_list[MAXJOBS]; /* The job list */

struct cmdline_tokens {
    int argc;               /* Number of arguments */
    char *argv[MAXARGS];    /* The arguments list */
    char *infile;           /* The input file */
    char *outfile;          /* The output file */
    enum builtins_t {       /* Indicates if argv[0] is a builtin command */
        BUILTIN_NONE,
        BUILTIN_QUIT,
        BUILTIN_JOBS,
        BUILTIN_BG,
        BUILTIN_FG} builtins;
};

/* End global variables */


/* Function prototypes */
void eval(char *cmdline);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);

/* Here are helper routines that we've provided for you */
int parseline(const char *cmdline, struct cmdline_tokens *tok); 
void sigquit_handler(int sig);

void clearjob(struct job_t *job);
void initjobs(struct job_t *job_list);
int maxjid(struct job_t *job_list); 
int addjob(struct job_t *job_list, pid_t pid, int state, char *cmdline);
int deletejob(struct job_t *job_list, pid_t pid); 
pid_t fgpid(struct job_t *job_list);
struct job_t *getjobpid(struct job_t *job_list, pid_t pid);
struct job_t *getjobjid(struct job_t *job_list, int jid); 
int pid2jid(pid_t pid); 
void listjobs(struct job_t *job_list, int output_fd);

void usage(void);
void unix_error(char *msg);
void app_error(char *msg);
typedef void handler_t(int);
handler_t *Signal(int signum, handler_t *handler);

int builtin_cmd(struct cmdline_tokens inp_tok);
void dup_redirect(struct cmdline_tokens* inp_tok);


/*
 * main - The shell's main routine 
 */
int 
main(int argc, char **argv) 
{
    char c;
    char cmdline[MAXLINE];    /* cmdline for fgets */
    int emit_prompt = 1; /* emit prompt (default) */

    /* Redirect stderr to stdout (so that driver will get all output
     * on the pipe connected to stdout) */
    dup2(1, 2);

    /* Parse the command line */
    while ((c = getopt(argc, argv, "hvp")) != EOF) {
        switch (c) {
        case 'h':             /* print help message */
            usage();
            break;
        case 'v':             /* emit additional diagnostic info */
            verbose = 1;
            break;
        case 'p':             /* don't print a prompt */
            emit_prompt = 0;  /* handy for automatic testing */
            break;
        default:
            usage();
        }
    }

    /* Install the signal handlers */

    /* These are the ones you will need to implement */
    Signal(SIGINT,  sigint_handler);   /* ctrl-c */
    Signal(SIGTSTP, sigtstp_handler);  /* ctrl-z */
    Signal(SIGCHLD, sigchld_handler);  /* Terminated or stopped child */
    Signal(SIGTTIN, SIG_IGN);
    Signal(SIGTTOU, SIG_IGN);

    /* This one provides a clean way to kill the shell */
    Signal(SIGQUIT, sigquit_handler); 

    /* Initialize the job list */
    initjobs(job_list);


    /* Execute the shell's read/eval loop */
    while (1) {

        if (emit_prompt) {
            printf("%s", prompt);
            fflush(stdout);
        }
        if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
            app_error("fgets error");
        if (feof(stdin)) { 
            /* End of file (ctrl-d) */
            printf ("\n");
            fflush(stdout);
            fflush(stderr);
            exit(0);
        }
        
        /* Remove the trailing newline */
        cmdline[strlen(cmdline)-1] = '\0';
        
        /* Evaluate the command line */
        eval(cmdline);
        
        fflush(stdout);
        fflush(stdout);
    } 
    
    exit(0); /* control never reaches here */
}

/*builtin_cmd - Performs required operations for the builtin commands
 * Returns a value of 0 if it is not one of the specified built in commands
 * Returns a value of 1 if it is a recognized built in command
 * Performs required operations for each of the commands
 */
int builtin_cmd(struct cmdline_tokens inp_tok){
	int file_desc_out, job_id, pid, file_desc_out_n;
	sigset_t mask_2;
	struct job_t *job_bg;/*background job*/
	/*Implement the builtin and executable commands*/
	switch(inp_tok.builtins){
	case BUILTIN_NONE:
		/*If the command is not one of the specified built in commands*/
		return 0;
	case BUILTIN_QUIT:
		/*quit command, terminate the shell*/
		exit(0);
	case BUILTIN_JOBS:
		/*Implement jobs command, list all background jobs*/
		if(inp_tok.outfile != NULL){
			/*Redirect the output and then  remap it back to stdout*/
			file_desc_out = open(inp_tok.outfile, STDOUT_FILENO);
			dup2(STDOUT_FILENO,file_desc_out); /* redirect the descriptor to stdout */
			file_desc_out_n = open(inp_tok.outfile, O_RDWR); /* get the decriptor */
			dup2(file_desc_out_n, STDOUT_FILENO); /*redirect stdout to descriptor */
			listjobs(job_list, STDOUT_FILENO); //list all jobs with stdout descriptor
			dup2(file_desc_out, STDOUT_FILENO);//Redirect stdout back to desciptor
			close(file_desc_out);
		}
		else{ /*If no outfile in tok*/
			listjobs(job_list, STDOUT_FILENO); /*If the outfile is null, list  jobs with stdout descriptor*/
		}
		return 1;
	case BUILTIN_BG: 
		/*Restart a stopped background job in background*/
		if(inp_tok.argv[1]){ /*if arguments are provided*/
		/*If jid has been specified, fetch the job using  its jid*/
		if(inp_tok.argv[inp_tok.argc-1][0] == '%'){
			/*Get job id*/
			job_id = atoi((inp_tok.argv[inp_tok.argc-1] +1));
			/*Get pointer to the job*/
			job_bg = getjobjid(job_list, job_id);
			if(job_bg == NULL){/*if no such job*/
				printf("The job with the job id %d does not exist \n", job_id);
				return 1;
			}
		}
		else{ /*Jid has not been specified, use pid to get the pointer to the job*/
			/*Get pid*/
			pid = atoi(inp_tok.argv[inp_tok.argc-1]);
			/*get pointer to the job*/
			job_bg = getjobjid(job_list, pid);
			if(job_bg == NULL){/*if no such process*/
				printf("The process with process id %d does not exist \n",pid);
				return 1;
			}
		}
		/*Check if the job is a background job in stopped state*/
		if((job_bg->state != BG) && (job_bg->state != ST)){
			printf("The job is either not a background job or not in stopped state");
		}
		/*Otherwise update job state to BG*/
		job_bg->state = BG;
		/*Send the continue signal to get the stopped job running*/
		kill(job_bg->pid, SIGCONT);
		printf("[%d] (%d) %s\n", job_bg->jid, pid, job_bg->cmdline);
		}
		else{ /*if no arguments have been given with the command*/
			printf("bg command requires either pid or %%jobid argument \n");
		}
		return 1;
	case BUILTIN_FG:
		/*Change a stopped or running background job to running foreground job*/
		if(inp_tok.argv[1]){/*If arguments are provided*/
                	/*If jid has been specified, fetch the job using its jid*/
			if(inp_tok.argv[inp_tok.argc-1][0] == '%'){
                        	/*Get job id*/
                        	job_id = atoi((inp_tok.argv[inp_tok.argc-1] +1));
                        	/*Get pointer to the job*/
                        	job_bg = getjobjid(job_list, job_id);
                        	if(job_bg == NULL){/*if no such job*/
                        		printf("The job with the job id %d does not exist \n", job_id);
                        		return 1;
                        	}
                	}
			else{ /*Jid has not been specified, use pid to get the pointer to the job*/
                        	/*Get pid*/
                        	pid = atoi(inp_tok.argv[inp_tok.argc-1]);
                        	/*get pointer to the job*/
                        	job_bg = getjobjid(job_list, pid);
                        	if(job_bg == NULL){/*if no such process*/
                        		printf("The process with process id %d does not exist \n",pid);
                        		return 1;
                        	}
               		}
                	/*Check if the job is a background job in stopped state*/
                	if((job_bg->state != BG) && (job_bg->state != ST)){
                        	printf("The job is either not a background job or not in stopped state");
                	}
			sigemptyset(&mask_2);
			/*Otherwise, first Wait for the current foreground job to finish or change its state*/
			while((getjobpid(job_list, job_bg->pid)->state != ST) && (fgpid(job_list) != 0) ){
				sigsuspend(&mask_2);//keep it suspended
			}
			/*Now update the state to Foreground*/
			job_bg->state = FG;
			/*Send the SIGCONT signal to get the stopped job running*/
			kill(job_bg->pid, SIGCONT);
		}
		else{/*if no arguments*/
        		printf("fg command requires either pid or %%jobid argument \n");
        	}	
		return 1;
	default:
		/*not a builtin command*/
		return 0;
	}
}
/*
 * dup_redirect - Redirects the file descriptors
 * The function redirects the input and output of the job for built in commands and executable files
 */
void dup_redirect(struct cmdline_tokens* inp_tok){
	int file_desc_in, file_desc_out;
	/*If outfile is not null, we need to redirect the output*/
	if(inp_tok->outfile != NULL){
		file_desc_out = open(inp_tok->outfile, O_RDWR); /*Open the file inread/write mode and get the file descriptor*/
		/*duplicate the file descriptor and make stdout point to our descriptor*/
		dup2(file_desc_out, STDOUT_FILENO);
		close(file_desc_out);
	}
	/*If we need to redirect the input*/
	if(inp_tok->infile != NULL){
		file_desc_in = open(inp_tok->infile, O_RDONLY); /*Open the file in read only mode and get the file descriptor*/
		/*Duplicate the file descriptor*/
		dup2(file_desc_in, STDIN_FILENO);
		close(file_desc_in);/*Now close the file*/
	}
	return;
}
/* 
 * eval - Evaluate the command line that the user has just typed in
 * 
 * If the user has requested a built-in command (quit, jobs, bg or fg)
 * then execute it immediately. Otherwise, fork a child process and
 * run the job in the context of the child. If the job is running in
 * the foreground, wait for it to terminate and then return.  Note:
 * each child process must have a unique process group ID so that our
 * background children don't receive SIGINT (SIGTSTP) from the kernel
 * when we type ctrl-c (ctrl-z) at the keyboard.  
 */
void 
eval(char *cmdline) 
{
	int bg;              /* should the job run in bg or fg? */
	struct cmdline_tokens tok;
	sigset_t mask, mask_1;
	int pid;
	/* Parse command line */
	bg = parseline(cmdline, &tok); 
	if (bg == -1) /* parsing error */
        return;
	if (tok.argv[0] == NULL) /* ignore empty lines */
        return;

	/*If its not a builtin command , fork a child and execute the executable*/
	if(!builtin_cmd(tok)){
		/*Initialize the sigset and add signals SIGINT, SIGTSTP and SIGCHLD to the set*/
		if(sigemptyset(&mask_1) == -1){
			printf("error in initializing");
			return;
		}
		if(sigemptyset(&mask) == -1){
			printf("Error in initilaizing set");
			return;
		}
		/*Add the signals SIGCHLD, SIGINT and SIFTSTP to the empty set*/
		if(sigaddset(&mask, SIGINT) == -1 || sigaddset(&mask, SIGTSTP) == -1 ||  sigaddset(&mask, SIGCHLD) == -1){
			printf("Error in adding signals to the set");
			return;
		}
		/*Block the signals to avoid race condition */
		if(sigprocmask(SIG_BLOCK, &mask, NULL) == -1){
			printf("Error in blocking signal");
			return;
		}
		/*Fork a child*/
		if((pid = fork())== 0){
			/*Inside a child process*/
			/*change the group id of the child to pid*/
			if(setpgid(0,0) == -1){
				printf("Error in changing groupid");
				return;
			}
			/*Manage I/O Redirection before running the program*/
			dup_redirect(&tok);
			/*As the child inherits its parent's blocked vectors, unblock the blocked signal first*/
			if(sigprocmask(SIG_UNBLOCK, &mask, NULL) ==-1){
				printf("Error in blocking signal");
	                        return;
	                }
			/*Now execute the executable using execve*/
			if((execve(tok.argv[0], tok.argv, environ)) < 0){
				printf("The command %s is invalid or not found \n", tok.argv[0]);
				exit(0);
			}
		}
		/*The parent process*/
		if(bg){/*If a background job*/
			/*Add the job as a background job to the job list*/
			if(addjob(job_list, pid, BG, cmdline) == 0){
				printf("Error in adding the job");
				return;
			}
		} 
		else{
			/*Add the job as a foreground job to the job list*/
			if(addjob(job_list, pid, FG, cmdline) == 0){
				 printf("Error in adding the job");
                                return;
                        }
		}
		/*Unblock the blocked signal now*/
		if(sigprocmask(SIG_UNBLOCK, &mask, NULL) == -1){
			printf("Error in blocking signal");
                        return;
                }
		/*If a background job, display jid, pid and cmdline*/
		if(bg){
			printf("[%d] (%d) %s\n", getjobpid(job_list, pid)->jid, pid, cmdline);
		}
		/*If a foreground job, suspend and wait for a signal*/
		if(!bg){
			/*Wait for the current foreground job to finish or change its state*/
			while((fgpid(job_list) != 0) && (getjobpid(job_list, pid)->state != ST)){
				sigsuspend(&mask_1); /*suspend the job*/
			}
		}
	}	
    	return;
}

/* 
 * parseline - Parse the command line and build the argv array.
 * 
 * Parameters:
 *   cmdline:  The command line, in the form:
 *
 *                command [arguments...] [< infile] [> oufile] [&]
 *
 *   tok:      Pointer to a cmdline_tokens structure. The elements of this
 *             structure will be populated with the parsed tokens. Characters 
 *             enclosed in single or double quotes are treated as a single
 *             argument. 
 * Returns:
 *   1:        if the user has requested a BG job
 *   0:        if the user has requested a FG job  
 *  -1:        if cmdline is incorrectly formatted
 * 
 * Note:       The string elements of tok (e.g., argv[], infile, outfile) 
 *             are statically allocated inside parseline() and will be 
 *             overwritten the next time this function is invoked.
 */
int 
parseline(const char *cmdline, struct cmdline_tokens *tok) 
{

    static char array[MAXLINE];          /* holds local copy of command line */
    const char delims[10] = " \t\r\n";   /* argument delimiters (white-space) */
    char *buf = array;                   /* ptr that traverses command line */
    char *next;                          /* ptr to the end of the current arg */
    char *endbuf;                        /* ptr to end of cmdline string */
    int is_bg;                           /* background job? */

    int parsing_state;                   /* indicates if the next token is the
                                            input or output file */

    if (cmdline == NULL) {
        (void) fprintf(stderr, "Error: command line is NULL\n");
        return -1;
    }

    (void) strncpy(buf, cmdline, MAXLINE);
    endbuf = buf + strlen(buf);

    tok->infile = NULL;
    tok->outfile = NULL;

    /* Build the argv list */
    parsing_state = ST_NORMAL;
    tok->argc = 0;

    while (buf < endbuf) {
        /* Skip the white-spaces */
        buf += strspn (buf, delims);
        if (buf >= endbuf) break;

        /* Check for I/O redirection specifiers */
        if (*buf == '<') {
            if (tok->infile) {
                (void) fprintf(stderr, "Error: Ambiguous I/O redirection\n");
                return -1;
            }
            parsing_state |= ST_INFILE;
            buf++;
            continue;
        }
        if (*buf == '>') {
            if (tok->outfile) {
                (void) fprintf(stderr, "Error: Ambiguous I/O redirection\n");
                return -1;
            }
            parsing_state |= ST_OUTFILE;
            buf ++;
            continue;
        }

        if (*buf == '\'' || *buf == '\"') {
            /* Detect quoted tokens */
            buf++;
            next = strchr (buf, *(buf-1));
        } else {
            /* Find next delimiter */
            next = buf + strcspn (buf, delims);
        }
        
        if (next == NULL) {
            /* Returned by strchr(); this means that the closing
               quote was not found. */
            (void) fprintf (stderr, "Error: unmatched %c.\n", *(buf-1));
            return -1;
        }

        /* Terminate the token */
        *next = '\0';

        /* Record the token as either the next argument or the i/o file */
        switch (parsing_state) {
        case ST_NORMAL:
            tok->argv[tok->argc++] = buf;
            break;
        case ST_INFILE:
            tok->infile = buf;
            break;
        case ST_OUTFILE:
            tok->outfile = buf;
            break;
        default:
            (void) fprintf(stderr, "Error: Ambiguous I/O redirection\n");
            return -1;
        }
        parsing_state = ST_NORMAL;

        /* Check if argv is full */
        if (tok->argc >= MAXARGS-1) break;

        buf = next + 1;
    }

    if (parsing_state != ST_NORMAL) {
        (void) fprintf(stderr,
                       "Error: must provide file name for redirection\n");
        return -1;
    }

    /* The argument list must end with a NULL pointer */
    tok->argv[tok->argc] = NULL;

    if (tok->argc == 0)  /* ignore blank line */
        return 1;

    if (!strcmp(tok->argv[0], "quit")) {                 /* quit command */
        tok->builtins = BUILTIN_QUIT;
    } else if (!strcmp(tok->argv[0], "jobs")) {          /* jobs command */
        tok->builtins = BUILTIN_JOBS;
    } else if (!strcmp(tok->argv[0], "bg")) {            /* bg command */
        tok->builtins = BUILTIN_BG;
    } else if (!strcmp(tok->argv[0], "fg")) {            /* fg command */
        tok->builtins = BUILTIN_FG;
    } else {
        tok->builtins = BUILTIN_NONE;
    }

    /* Should the job run in the background? */
    if ((is_bg = (*tok->argv[tok->argc-1] == '&')) != 0)
        tok->argv[--tok->argc] = NULL;

    return is_bg;
}


/*****************
 * Signal handlers
 *****************/

/* 
 * sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
 *     a child job terminates (becomes a zombie), or stops because it
 *     received a SIGSTOP, SIGTSTP, SIGTTIN or SIGTTOU signal. The 
 *     handler reaps all available zombie children, but doesn't wait 
 *     for any other currently running children to terminate.  
 */
void 
sigchld_handler(int sig) 
{
	int status;
	pid_t  pid;
	/*Reap the children, one child per iteration with no blocking, terminate if not reaped*/
	while((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0){
		/*Determine the reason for the termination of child*/
		if(WIFSIGNALED(status)){
			/*If it terminated by a signal, delete the job from the job list*/
			printf("Job [%d] (%d) %s by signal %d\n",pid2jid(pid), pid,"terminated", WTERMSIG(status));
			deletejob(job_list, pid);
		}
		else if(WIFSTOPPED(status)){
			/* If it was stopped, get the job from the list and change its state to stopped */
			printf("Job [%d] (%d) %s by signal %d\n",pid2jid(pid), pid,"stopped", WSTOPSIG(status));
			getjobpid(job_list,pid)->state = ST;
		}
		else if(WIFEXITED(status)){
			/*If it exited normally, delete the job from the job list*/
			deletejob(job_list, pid);
		}
	}	
	return;
}

/* 
 * sigint_handler - The kernel sends a SIGINT to the shell whenver the
 *    user types ctrl-c at the keyboard.  Catch it and send it along
 *    to the foreground job.  
 */
void 
sigint_handler(int sig) 
{
	/*Handle the interrupt signal*/
	pid_t fg_pid;
        /*Get the pid of the current foreground job*/
	fg_pid = fgpid(job_list);
        /*Send the interrupt signal to each process in the foreground job*/
        if(fg_pid != 0){
		kill(-fg_pid, sig);
	}
        return;
}

/*
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *     foreground job by sending it a SIGTSTP.  
 */
void 
sigtstp_handler(int sig) 
{
	/*Handle the stop signal*/
	pid_t fg_pid;
	/*Get the pid of the current foreground job*/
	fg_pid = fgpid(job_list);
	/*Send the Stop signal to each process in the foreground job*/
	if(fg_pid != 0){
		kill(-fg_pid, sig);
	}	
	
	return;
}

/*********************
 * End signal handlers
 *********************/

/***********************************************
 * Helper routines that manipulate the job list
 **********************************************/

/* clearjob - Clear the entries in a job struct */
void 
clearjob(struct job_t *job) {
    job->pid = 0;
    job->jid = 0;
    job->state = UNDEF;
    job->cmdline[0] = '\0';
}

/* initjobs - Initialize the job list */
void 
initjobs(struct job_t *job_list) {
    int i;

    for (i = 0; i < MAXJOBS; i++)
        clearjob(&job_list[i]);
}

/* maxjid - Returns largest allocated job ID */
int 
maxjid(struct job_t *job_list) 
{
    int i, max=0;

    for (i = 0; i < MAXJOBS; i++)
        if (job_list[i].jid > max)
            max = job_list[i].jid;
    return max;
}

/* addjob - Add a job to the job list */
int 
addjob(struct job_t *job_list, pid_t pid, int state, char *cmdline) 
{
    int i;

    if (pid < 1)
        return 0;

    for (i = 0; i < MAXJOBS; i++) {
        if (job_list[i].pid == 0) {
            job_list[i].pid = pid;
            job_list[i].state = state;
            job_list[i].jid = nextjid++;
            if (nextjid > MAXJOBS)
                nextjid = 1;
            strcpy(job_list[i].cmdline, cmdline);
            if(verbose){
                printf("Added job [%d] %d %s\n",
                       job_list[i].jid,
                       job_list[i].pid,
                       job_list[i].cmdline);
            }
            return 1;
        }
    }
    printf("Tried to create too many jobs\n");
    return 0;
}

/* deletejob - Delete a job whose PID=pid from the job list */
int 
deletejob(struct job_t *job_list, pid_t pid) 
{
    int i;

    if (pid < 1)
        return 0;

    for (i = 0; i < MAXJOBS; i++) {
        if (job_list[i].pid == pid) {
            clearjob(&job_list[i]);
            nextjid = maxjid(job_list)+1;
            return 1;
        }
    }
    return 0;
}

/* fgpid - Return PID of current foreground job, 0 if no such job */
pid_t 
fgpid(struct job_t *job_list) {
    int i;

    for (i = 0; i < MAXJOBS; i++)
        if (job_list[i].state == FG)
            return job_list[i].pid;
    return 0;
}

/* getjobpid  - Find a job (by PID) on the job list */
struct job_t 
*getjobpid(struct job_t *job_list, pid_t pid) {
    int i;

    if (pid < 1)
        return NULL;
    for (i = 0; i < MAXJOBS; i++)
        if (job_list[i].pid == pid)
            return &job_list[i];
    return NULL;
}

/* getjobjid  - Find a job (by JID) on the job list */
struct job_t *getjobjid(struct job_t *job_list, int jid) 
{
    int i;

    if (jid < 1)
        return NULL;
    for (i = 0; i < MAXJOBS; i++)
        if (job_list[i].jid == jid)
            return &job_list[i];
    return NULL;
}

/* pid2jid - Map process ID to job ID */
int 
pid2jid(pid_t pid) 
{
    int i;

    if (pid < 1)
        return 0;
    for (i = 0; i < MAXJOBS; i++)
        if (job_list[i].pid == pid) {
            return job_list[i].jid;
        }
    return 0;
}

/* listjobs - Print the job list */
void 
listjobs(struct job_t *job_list, int output_fd) 
{
    int i;
    char buf[MAXLINE];

    for (i = 0; i < MAXJOBS; i++) {
        memset(buf, '\0', MAXLINE);
        if (job_list[i].pid != 0) {
            sprintf(buf, "[%d] (%d) ", job_list[i].jid, job_list[i].pid);
            if(write(output_fd, buf, strlen(buf)) < 0) {
                fprintf(stderr, "Error writing to output file\n");
                exit(1);
            }
            memset(buf, '\0', MAXLINE);
            switch (job_list[i].state) {
            case BG:
                sprintf(buf, "Running    ");
                break;
            case FG:
                sprintf(buf, "Foreground ");
                break;
            case ST:
                sprintf(buf, "Stopped    ");
                break;
            default:
                sprintf(buf, "listjobs: Internal error: job[%d].state=%d ",
                        i, job_list[i].state);
            }
            if(write(output_fd, buf, strlen(buf)) < 0) {
                fprintf(stderr, "Error writing to output file\n");
                exit(1);
            }
            memset(buf, '\0', MAXLINE);
            sprintf(buf, "%s\n", job_list[i].cmdline);
            if(write(output_fd, buf, strlen(buf)) < 0) {
                fprintf(stderr, "Error writing to output file\n");
                exit(1);
            }
        }
    }
}
/******************************
 * end job list helper routines
 ******************************/


/***********************
 * Other helper routines
 ***********************/

/*
 * usage - print a help message
 */
void 
usage(void) 
{
    printf("Usage: shell [-hvp]\n");
    printf("   -h   print this message\n");
    printf("   -v   print additional diagnostic information\n");
    printf("   -p   do not emit a command prompt\n");
    exit(1);
}

/*
 * unix_error - unix-style error routine
 */
void 
unix_error(char *msg)
{
    fprintf(stdout, "%s: %s\n", msg, strerror(errno));
    exit(1);
}

/*
 * app_error - application-style error routine
 */
void 
app_error(char *msg)
{
    fprintf(stdout, "%s\n", msg);
    exit(1);
}

/*
 * Signal - wrapper for the sigaction function
 */
handler_t 
*Signal(int signum, handler_t *handler) 
{
    struct sigaction action, old_action;

    action.sa_handler = handler;  
    sigemptyset(&action.sa_mask); /* block sigs of type being handled */
    action.sa_flags = SA_RESTART; /* restart syscalls if possible */

    if (sigaction(signum, &action, &old_action) < 0)
        unix_error("Signal error");
    return (old_action.sa_handler);
}

/*
 * sigquit_handler - The driver program can gracefully terminate the
 *    child shell by sending it a SIGQUIT signal.
 */
void 
sigquit_handler(int sig) 
{
    printf("Terminating after receipt of SIGQUIT signal\n");
    exit(1);
}

