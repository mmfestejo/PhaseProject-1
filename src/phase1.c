/* ------------------------------------------------------------------------
   phase1.c
   CSCV 452
   ------------------------------------------------------------------------ */
#include <stdlib.h>
#include <strings.h>
#include <stdio.h>
#include <phase1.h>
#include <usloss.h>
#include "kernel.h"

/* ------------------------- Prototypes ----------------------------------- */
int sentinel(void *);
extern int start1(char *);
void dispatcher(void);
void launch();
static void enableInterrupts();
static void check_deadlock();


/* -------------------------- Globals ------------------------------------- */
/* Patrick's debugging global variable... */
int debugflag = 1;

/* the process table */
proc_struct ProcTable[MAXPROC];

/* Process lists  */
static procPtr ReadyList;

/* current process ID */
proc_ptr Current;

/* the next pid to be assigned */
unsigned int next_pid = SENTINELPID;

/* number of processes running*/
int procCount;
/* -------------------------- Functions ----------------------------------- */
/* ------------------------------------------------------------------------
   Name - startup
   Purpose - Initializes process lists and clock interrupt vector.
         Start up sentinel process and the test process.
   Parameters - none, called by USLOSS
   Returns - nothing
   Side Effects - lots, starts the whole thing
   ----------------------------------------------------------------------- */
void startup() {
    int i;      /* loop index */
    int result; /* value returned by call to fork1() */
    /* initialize the process table */
    if (DEBUG && debugflag) {
        console("startup(): initializing process table, ProcTable[]\n");
    }
    for (i = 0; i < MAXPROC; i++) {
        ProcTable[i].next_proc_ptr = NO_CURRENT_PROCESS;
        ProcTable[i].child_proc_ptr = NO_CURRENT_PROCESS;
        ProcTable[i].next_sibling_ptr = NO_CURRENT_PROCESS;
        ProcTable[i].quit_child_ptr = NO_CURRENT_PROCESS;
        ProcTable[i].name[0] = '\0';
        ProcTable[i].start_arg[0] = '\0';
        ProcTable[i].pid = EMPTY;
        ProcTable[i].status = EMPTY;

    }

    /* Initialize the Ready list, etc. */
    if (DEBUG && debugflag) {
        console("startup(): initializing the Ready & Blocked lists\n");
    }

    ReadyList = NULL;
    /* Initialize the clock interrupt handler */
    /* startup a sentinel process */
    if (DEBUG && debugflag) {
        console("startup(): calling fork1() for sentinel\n");
    }
    result = fork1("sentinel", sentinel, NULL, USLOSS_MIN_STACK,
                   SENTINELPRIORITY);
    if (result < 0) {
        if (DEBUG && debugflag)
            console("startup(): fork1 of sentinel returned error, halting...\n");
        halt(1);
    }

    /* start the test process */
    if (DEBUG && debugflag) {
        console("startup(): calling fork1() for start1\n");
    }
    result = fork1("start1", start1, NULL, 2 * USLOSS_MIN_STACK, 1);
    if (result < 0) {
        console("startup(): fork1 for start1 returned an error, halting...\n");
        halt(1);
    }
    console("startup(): Should not see this message! ");
    console("Returned from fork1 call that created start1\n");
    return;
} /* startup */
/* ------------------------------------------------------------------------
   Name - finish
   Purpose - Required by USLOSS
   Parameters - none
   Returns - nothing
   Side Effects - none
   ----------------------------------------------------------------------- */
void finish() {
    if (DEBUG && debugflag) {
        console("in finish...\n");
    }
} /* finish */
/* ------------------------------------------------------------------------
   Name - fork1
   Purpose - Gets a new process from the process table and initializes
             information of the process.  Updates information in the
             parent process to reflect this child process creation.
   Parameters - the process procedure address, the size of the stack and
                the priority to be assigned to the child process.
   Returns - the process id of the created child or -1 if no child could
             be created or if priority is not between max and min priority.
   Side Effects - ReadyList is changed, ProcTable is changed, Current
                  process information changed
   ------------------------------------------------------------------------ */
int fork1(char *name, int (*f)(void *), void *arg, int stacksize, int priority) {
    int proc_slot;
    if (DEBUG && debugflag) {
        console("fork1(): creating process %s\n", name);
    }
    /* test if in kernel mode; halt if in user mode */

    checkKernelMode("startup()");
    /* Return if stack size is too small */
    /* find an empty slot in the process table */
    /* fill-in entry in process table */
    if (strlen(name) >= (MAXNAME - 1)) {
        console("fork1(): Process name is too long.  Halting...\n");
        halt(1);
    }
    strcpy(ProcTable[proc_slot].name, name);
    ProcTable[proc_slot].start_func = f;
    if (arg == NULL)
        ProcTable[proc_slot].start_arg[0] = '\0';
    else if (strlen(arg) >= (MAXARG - 1)) {
        console("fork1(): argument too long.  Halting...\n");
        halt(1);
    } else
        strcpy(ProcTable[proc_slot].start_arg, arg);
    /* Initialize context for this process, but use launch function pointer for
     * the initial value of the process's program counter (PC)
     */
    context_init(&(ProcTable[proc_slot].state), psr_get(),
                 ProcTable[proc_slot].stack,
                 ProcTable[proc_slot].stacksize, launch);
    /* for future phase(s) */
    p1_fork(ProcTable[proc_slot].pid);
} /* fork1 */
/* ------------------------------------------------------------------------
   Name - launch
   Purpose - Dummy function to enable interrupts and launch a given process
             upon startup.
   Parameters - none
   Returns - nothing
   Side Effects - enable interrupts
   ------------------------------------------------------------------------ */
void launch() {
    int result;
    if (DEBUG && debugflag)
        console("launch(): started\n");
    /* Enable interrupts */
    enableInterrupts();
    /* Call the function passed to fork1, and capture its return value */
    result = Current->start_func(Current->start_arg);
    if (DEBUG && debugflag)
        console("Process %d returned to launch\n", Current->pid);
    quit(result);
} /* launch */
/* ------------------------------------------------------------------------
   Name - join
   Purpose - Wait for a child process (if one has been forked) to quit.  If
             one has already quit, don't wait.
   Parameters - a pointer to an int where the termination code of the
                quitting process is to be stored.
   Returns - the process id of the quitting child joined on.
        -1 if the process was zapped in the join
        -2 if the process has no children
   Side Effects - If no child process has quit before join is called, the
                  parent is removed from the ready list and blocked.
   ------------------------------------------------------------------------ */
int join(int *code) {
} /* join */
/* ------------------------------------------------------------------------
   Name - quit
   Purpose - Stops the child process and notifies the parent of the death by
             putting child quit info on the parents child completion code
             list.
   Parameters - the code to return to the grieving parent
   Returns - nothing
   Side Effects - changes the parent of pid child completion status list.
   ------------------------------------------------------------------------ */
void quit(int code) {
    p1_quit(Current->pid);
} /* quit */
/* ------------------------------------------------------------------------
   Name - dispatcher
   Purpose - dispatches ready processes.  The process with the highest
             priority (the first on the ready list) is scheduled to
             run.  The old process is swapped out and the new process
             swapped in.
   Parameters - none
   Returns - nothing
   Side Effects - the context of the machine is changed
   ----------------------------------------------------------------------- */
void dispatcher(void) {
    proc_ptr next_process;
    p1_switch(Current->pid, next_process->pid);
} /* dispatcher */
/* ------------------------------------------------------------------------
   Name - sentinel
   Purpose - The purpose of the sentinel routine is two-fold.  One
             responsibility is to keep the system going when all other
         processes are blocked.  The other is to detect and report
         simple deadlock states.
   Parameters - none
   Returns - nothing
   Side Effects -  if system is in deadlock, print appropriate error
           and halt.
   ----------------------------------------------------------------------- */
int sentinel(void *dummy) {
    if (DEBUG && debugflag) {
        console("sentinel(): called\n");
    }
    while (1) {
        check_deadlock();
        waitint();
    }
} /* sentinel */

/* check to determine if deadlock has occurred... */
static void check_deadlock() {
    int procReady = 0;
    int procActive = 0;
    int i;

    for (i = 0; i < MAXPROC; i++) {
        if (ProcTable[i].status == READY) {
            procReady++;
            procActive++;
        }

        if (ProcTable[i].status == JOIN_BLOCK || ProcTable[i].status == ZAP_BLOCK) {
            procActive++;
        }
    }

    if (procReady == 1) {
        /* No deadlock. quitting...*/
        if (procActive == 1) {
            console("All process completed.");
            halt(0);
        } else {
            console("check_deadlock(): Processes Active = %d\n", procActive);
            console("check_deadlock(): Processes still present. Halting...\n")
            halt(1);
        }
    } else {
        return;
    }
} /* check_deadlock */

/*
 * Disables the interrupts.
 */
void disableInterrupts() {
    /* turn the interrupts OFF iff we are in kernel mode */
    if ((PSR_CURRENT_MODE & psr_get()) == 0) {
        //not in kernel mode
        console("Kernel Error: Not in kernel mode, may not disable interrupts\n");
        halt(1);
    } else
        /* We ARE in kernel mode */
        psr_set(psr_get() & ~PSR_CURRENT_INT);
} /* disableInterrupts */



//Function to check in kernel mode.
void checkKernelMode(char *name) {
    if ((PSR_CURRENT_MODE & PsrGet()) == 0) {
        console("%s: called while in user mode, by process %d. Halting...\n", name, Current->pid);
        halt(1);
        return 0;
    } else {
        return 1;
    }
}