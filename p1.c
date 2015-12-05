
#include "usloss.h"
#include "phase5.h"
#define DEBUG 0
extern int debugflag;



void
p1_fork(int pid)
{
    if (DEBUG && debugflag)
        USLOSS_Console("p1_fork(): called: pid = %d\n", pid);
    if(!vmInitialized){
        USLOSS_Console("p1_fork(): VM not yet initialized, doing nothing!\n");
        return;
    }
    Process * temp = malloc(sizeof(Process));
    temp->numPages = 0;
    temp->pageTable = NULL;
    processes[getpid()%MAXPROC] = *temp;
    
    
    
} /* p1_fork */

void
p1_switch(int old, int new)
{
    if (DEBUG && debugflag)
        USLOSS_Console("p1_switch() called: old = %d, new = %d\n", old, new);
} /* p1_switch */

void
p1_quit(int pid)
{
    /* Free'ing all the page entries & Setting numPages to 0 */
    for(;processes[getpid()%MAXPROC].pageTable!=NULL;){
        PTE *next = processes[getpid()%MAXPROC].pageTable->nextPage;
        free(processes[getpid()%MAXPROC].pageTable);
        processes[getpid()%MAXPROC].pageTable = next;
        
    }
    processes[getpid()%MAXPROC].numPages = 0;
    
    /* Free'ing the page table */
    free(&processes[getpid()%MAXPROC]);
    
    if (DEBUG && debugflag)
        USLOSS_Console("p1_quit() called: pid = %d\n", pid);
} /* p1_quit */

