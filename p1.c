
#include "usloss.h"
#include "phase5.h"




void
p1_fork(int pid)
{
    if (debugFlag)
        USLOSS_Console("p1_fork(): called: pid = %d\n", pid);
    if(!vmInitialized){
        USLOSS_Console("p1_fork(): VM not yet initialized, doing nothing!\n");
        return;
    }

    /*
     *  allocates a Process element, stores pertinent information in it and commits it
     *  to the table
     */


    // Creating Page Table for Process being forked
    PTE *temp = malloc(sizeof(struct PTE));
    temp->nextPage = NULL;
    temp->diskBlock = -1;
    temp->state = UNUSED;
    processes[getpid()%MAXPROC].pageTable = temp;
    
} /* p1_fork */

/*
 p1_switch():
 • Unloads all the mappings from the old process (if any), and loads all valid mappings (if any) from the new
 process.
 • A valid mapping is one where there is a frame for the page.
 • If there is no frame, the MMU should not have a mapping for that page.
 • When there is a reference in the code to a non-existing page, there will be an MMU interrupt (page fault) and a
 page will then be allocated.
 */
void
p1_switch(int old, int new)
{
    if (debugFlag)
        USLOSS_Console("p1_switch() called: old = %d, new = %d\n", old, new);
    if(!vmInitialized){
        USLOSS_Console("p1_switch(): VM not yet initialized, doing nothing!\n");
       return;
    }
    
    /* 
     -Iterate through the old process' page table and unload all mappings from MMU & write all
     those pages to disk.
     -Then read all the pages from disk from the new process and map them to frames & map new frames
     */
    PTE *temp;
    // Iterate through all the old processes' pages
    for(temp = processes[old%MAXPROC].pageTable; temp != NULL; temp = temp->nextPage){
        int *framePtr;
        int *protPtr;
        int reply;
        // If a page is in memory, unmap
        if(temp->state == INCORE || temp->state == INBOTH){
            reply = USLOSS_MmuGetMap(TAG, temp->pageNum, framePtr, protPtr);
            if(debugFlag){
                if(reply == USLOSS_MMU_ERR_NOMAP){
                    USLOSS_Console("p1_switch(): No mapping found for the page %d for process # %d\n", temp->pageNum, old);
                }
            }
            USLOSS_MmuUnmap(TAG, temp->pageNum);
            int i;
            for(i=0; i<numBlocks; i++){
                if(diskBlocks[i])
            }
            
        }
        
    }
    
    
} /* p1_switch */

void
p1_quit(int pid)
{
    if(!vmInitialized){
        USLOSS_Console("p1_quit(): VM not yet initialized, doing nothing!\n");
        return;
    }
    
    /* Iterate through all the entries in the quitting processes' Page Table */
    for(;processes[getpid()%MAXPROC].pageTable!=NULL;){
        
        /* Check diskBlocks array to see if they are still in use, mark them UNUSED*/
        if(processes[getpid()%MAXPROC].pageTable->diskBlock != -1){
            diskBlocks[processes[getpid()%MAXPROC].pageTable->diskBlock] = UNUSED;
        }
        /* If page is in memory, unload the mappings from page to frame */
        if(processes[getpid()%MAXPROC].pageTable->state == INCORE || processes[getpid()%MAXPROC].pageTable->state == INBOTH){
            USLOSS_MmuUnmap(TAG, processes[getpid()%MAXPROC].pageTable->pageNum);
        }
        
        /* Check frame tables and mark frames used by the process as UNUSED*/
        FTE * temp;
        temp = frameTable;
        for(;temp->next!= NULL && temp->frame!=processes[getpid()%MAXPROC].pageTable->frame; temp = temp->next);
        temp->useBit=0;
        
        /* Free'ing all the page entries */
        PTE *next = processes[getpid()%MAXPROC].pageTable->nextPage;
        free(processes[getpid()%MAXPROC].pageTable);
        processes[getpid()%MAXPROC].pageTable = next;
    }
    // Setting numPages to 0
    processes[getpid()%MAXPROC].numPages = 0;
    
    /* Free'ing the page table */
    free(&processes[getpid()%MAXPROC]);
    
    if (debugFlag)

        USLOSS_Console("p1_quit() called: pid = %d\n", pid);
} /* p1_quit */

