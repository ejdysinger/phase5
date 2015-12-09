#include <assert.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <phase4.h>
#include <phase5.h>
#include <usyscall.h>
#include <libuser.h>
#include <vm.h>
#include <math.h>
#include <string.h>

extern void mbox_create(systemArgs *args_ptr);
extern void mbox_release(systemArgs *args_ptr);
extern void mbox_send(systemArgs *args_ptr);
extern void mbox_receive(systemArgs *args_ptr);
extern void mbox_condsend(systemArgs *args_ptr);
extern void mbox_condreceive(systemArgs *args_ptr);
static int Pager(char *buf);

Process processes[MAXPROC];
int vmInitialized = 0;
int debugFlag = 0;

FaultMsg faults[MAXPROC]; /* Note that a process can have only
                           * one fault at a time, so we can
                           * allocate the messages statically
                           * and index them by pid. */
VmStats  vmStats;

// clock hand position
extern int clockHand;

// frame table and the size of the frame table
extern FTE * frameTable;
extern int frameTableSize;

// instance variable to signal pager death
int pagerkill = 0;

// integer array for disk contents
extern int *diskBlocks;
int numBlocks;
int DBPerTrack;
int sectsPerDB;

static void
FaultHandler(int  type,  // USLOSS_MMU_INT
             void *arg); // Offset within VM region

static void vmInit(systemArgs *sysargsPtr);
static void vmDestroy(systemArgs *sysargsPtr);

int faultMailBox;
int clockHandMbox;

int pagerHouses[MAXPAGERS]; /* keeps track of the pager PIDs to facilitate
							 * murdering them later
							 */

/*
 *----------------------------------------------------------------------
 *
 * start4 --
 *
 * Initializes the VM system call handlers.
 *
 * Results:
 *      MMU return status
 *
 * Side effects:
 *      The MMU is initialized.
 *
 *----------------------------------------------------------------------
 */
int
start4(char *arg)
{
    int pid;
    int result;
    int status;

    /* to get user-process access to mailbox functions */
    systemCallVec[SYS_MBOXCREATE]      = mbox_create;
    systemCallVec[SYS_MBOXRELEASE]     = mbox_release;
    systemCallVec[SYS_MBOXSEND]        = mbox_send;
    systemCallVec[SYS_MBOXRECEIVE]     = mbox_receive;
    systemCallVec[SYS_MBOXCONDSEND]    = mbox_condsend;
    systemCallVec[SYS_MBOXCONDRECEIVE] = mbox_condreceive;

    /* user-process access to VM functions */
    systemCallVec[SYS_VMINIT]    = vmInit;
    systemCallVec[SYS_VMDESTROY] = vmDestroy;

    result = Spawn("Start5", start5, NULL, 8*USLOSS_MIN_STACK, 2, &pid);
    if (result != 0) {
        console("start4(): Error spawning start5\n");
        Terminate(1);
    }
    result = Wait(&pid, &status);
    if (result != 0) {
        console("start4(): Error waiting for start5\n");
        Terminate(1);
    }
    Terminate(0);
    return 0; // not reached

} /* start4 */

/*
 *----------------------------------------------------------------------
 *
 * VmInit --
 *
 * Stub for the VmInit system call.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      VM system is initialized.
 *
 *----------------------------------------------------------------------
 */
static void
vmInit(systemArgs *sysargsPtr)
{
    // verifies that the
	CheckMode();
	void* vmAddress;
	/*
	 * Input
		arg1: number of mappings the MMU should hold
		arg2: number of virtual pages to use
		arg3: number of physical page frames to use
		arg4: number of pager daemons
		Output
		arg1: address of the VM region
		arg4: -1 if illegal values are given as input or the VM region has already been
		initialized; 0 otherwise.
	*/

	// verify the parameters provided are legal
	if(sysargsPtr->arg1 != sysargsPtr->arg2 || sysargsPtr->arg4 > MAXPAGERS){
		sysargsPtr->arg4 = -1;
		return;
	}

	// calls the real vmInit
	vmAddress = vmInitReal(sysargsPtr->arg1, sysargsPtr->arg2, sysargsPtr->arg3, sysargsPtr->arg4);

	// places the vm address in arg1 and returns
	sysargsPtr->arg1 = vmAddress;

	// set extern indicator variable = 1
	vmInitialized = 1;

} /* vmInit */


/*
 *----------------------------------------------------------------------
 *
 * vmDestroy --
 *
 * Stub for the VmDestroy system call.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      VM system is cleaned up.
 *
 *----------------------------------------------------------------------
 */

static void
vmDestroy(systemArgs *sysargsPtr)
{
   CheckMode();
} /* vmDestroy */


/*
 *----------------------------------------------------------------------
 *
 * vmInitReal --
 *
 * Called by vmInit.
 * Initializes the VM system by configuring the MMU and setting
 * up the page tables.
 *
 * Results:
 *      Address of the VM region.
 *
 * Side effects:
 *      The MMU is initialized.
 *
 *----------------------------------------------------------------------
 */
void *
vmInitReal(int mappings, int pages, int frames, int pagers)
{
   int status;
   int dummy;
   int i, sec, track, disk;
   char bufferName[50];

   CheckMode();
   status = USLOSS_MmuInit(mappings, pages, frames);
   if (status != USLOSS_MMU_OK) {
      USLOSS_Console("vmInitReal: couldn't init MMU, status %d\n", status);
      abort();
   }
   // assign the page fault handler function to the interrupt vector table
   USLOSS_IntVec[USLOSS_MMU_INT] = FaultHandler;

   // Initialize page tables.
   USLOSS_MmuInit(mappings,pages,frames);

   // create page tables and fault mailboxes for each process
   for (i=0; i<MAXPROC; i++){
	   Process * temp = malloc(sizeof(Process));
	   temp->numPages = 0;
	   temp->pageTable = NULL;             // create the page table
	   temp->procMbox = MboxCreate(0,0);   // create a mailbox for fault resolution
	   processes[i] = *temp;
   }

   // malloc the frame table, initialize each frame within it and store its dimension
   frameTable = malloc(frames * sizeof(FTE));
   frameTableSize = frames;
   for(i = 0; i < frames; i++){
	   frameTable[i].frame = malloc(sizeof(USLOSS_MmuPageSize()));
	   frameTable[i].useBit = FR_UNUSED;
   }

   //. create disk occupancy table and calculate global disk params based on size of pages from MMU
   int numTracks;
   int numSects;
   int sectSize;
   DiskSize(1, &sectSize,&numSects,&numTracks);
   numBlocks = numTracks * numSects * sectSize / USLOSS_MmuPageSize();
    DBPerTrack = numBlocks/numTracks;
   diskBlocks = malloc(numBlocks * sizeof(int));
    sectsPerDB = (int)ceil(USLOSS_MmuPageSize()/sectSize);
   for (i=0; i < numBlocks; i++)
	   diskBlocks[i] = DB_UNUSED;

   // Create the zero slot fault mailbox so that fault objects can be passed to pagers
   faultMailBox = MboxCreate(MAXPROC, 0);

   // Create zero slot clockHand mailbox
   clockHandMbox = MboxCreate(0,0);

   // Fork the pagers.
   for(i = 0; i < MAXPAGERS; i++){
	   sprintf(bufferName,"Pager %d", i+1);
	   pagerHouses[i] = fork1(bufferName,Pager,NULL,USLOSS_MIN_STACK,PAGER_PRIORITY);
   }

   // Zero out, then initialize, the vmStats structure
   memset((char *) &vmStats, 0, sizeof(VmStats));
   vmStats.pages = pages;
   vmStats.frames = frames;
   if ((i = DiskSize(1, &sec, &track, &disk)) != -1)
	   vmStats.diskBlocks = disk / USLOSS_MmuPageSize();
   vmStats.freeDiskBlocks = vmStats.diskBlocks;

   return USLOSS_MmuRegion(&dummy);
} /* vmInitReal */


/*
 *----------------------------------------------------------------------
 *
 * PrintStats --
 *
 *      Print out VM statistics.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      Stuff is printed to the USLOSS_Console.
 *
 *----------------------------------------------------------------------
 */
void
PrintStats(void)
{
     USLOSS_Console("VmStats\n");
     USLOSS_Console("pages:          %d\n", vmStats.pages);
     USLOSS_Console("frames:         %d\n", vmStats.frames);
     USLOSS_Console("diskBlocks:     %d\n", vmStats.diskBlocks);
     USLOSS_Console("freeFrames:     %d\n", vmStats.freeFrames);
     USLOSS_Console("freeDiskBlocks: %d\n", vmStats.freeDiskBlocks);
     USLOSS_Console("switches:       %d\n", vmStats.switches);
     USLOSS_Console("faults:         %d\n", vmStats.faults);
     USLOSS_Console("new:            %d\n", vmStats.new);
     USLOSS_Console("pageIns:        %d\n", vmStats.pageIns);
     USLOSS_Console("pageOuts:       %d\n", vmStats.pageOuts);
     USLOSS_Console("replaced:       %d\n", vmStats.replaced);
} /* PrintStats */


/*
 *----------------------------------------------------------------------
 *
 * vmDestroyReal --
 *
 * Called by vmDestroy.
 * Frees all of the global data structures
 *
 * Results:
 *      None
 *
 * Side effects:
 *      The MMU is turned off.
 *
 *----------------------------------------------------------------------
 */
void
vmDestroyReal(void)
{
   int i;
   CheckMode();
   USLOSS_MmuDone();

   // Kill the pagers here; zapping them to interrupt
   for(i = 0; i < MAXPAGERS; i++){
	   if(pagerHouses[i] != NULL)
		   zap(pagerHouses[i]);
   }


   /*
    * Print vm statistics.
    */
   console("vmStats:\n");
   console("pages: %d\n", vmStats.pages);
   console("frames: %d\n", vmStats.frames);
   console("blocks: %d\n", vmStats.diskBlocks);
   /* and so on... */

} /* vmDestroyReal */

/*
 *----------------------------------------------------------------------
 *
 * FaultHandler
 *
 * Handles an MMU interrupt. Simply stores information about the
 * fault in a queue, wakes a waiting pager, and blocks until
 * the fault has been handled.
 *
 * Results:
 * None.
 *
 * Side effects:
 * The current process is blocked until the fault is handled.
 *
 *----------------------------------------------------------------------
 */
static void
FaultHandler(int  type /* USLOSS_MMU_INT */,
             void *arg  /* Offset within VM region */)
{
   int cause;

   int offset = (int) (long) arg;

   assert(type == USLOSS_MMU_INT);
   cause = USLOSS_MmuGetCause();
   assert(cause == USLOSS_MMU_FAULT);
   vmStats.faults++;
   /*
    * Fill in faults[pid % MAXPROC], send it to the pagers, and wait for the
    * reply.
    */
   faults[getpid() % MAXPROC].addr = arg;
   faults[getpid() % MAXPROC].pid = getpid();
   // perform the send with the
   MboxSend(faultMailBox,faults[getpid() % MAXPROC].addr, sizeof(void*));

   // blocks on the process's mbox until the fault is resolved
   // MboxReceive();

   // create new mapping of page generated by pager demon

} /* FaultHandler */

/*
 *----------------------------------------------------------------------
 *
 * Pager
 *
 * Kernel process that handles page faults and does page replacement.
 *
 * Results:
 * None.
 *
 * Side effects:
 * None.
 *
 *----------------------------------------------------------------------
 */
static int
Pager(char *buf)
{
	FaultMsg * faultObj = malloc(sizeof(struct FaultMsg));
	int iter;
	void * freeFrameFound;   // a pointer to a free frame in memory
	char * pageBuff;         // a buffer for a page that has been written and needs to be transferred to disk
	char * dummy;            // a dummy buffer for mailbox operations
	PTE * pgPtr;             // a pointer to the page table entry for an occupied frame
	int pageNum;             // the page
	int axBits;              // the access bits for a particular page

	// allocate the page buffer and zero it out
	pageBuff = malloc(USLOSS_MmuPageSize());


    while(1) {
        /* Wait for fault to occur (receive from mailbox) */
    	MboxReceive(faultMailBox, faultObj, sizeof(void*));
    	/* if on returning from the mbox the pager daemon is to be terminated, terminate it */
    	if(pagerkill) break;

    	//calculate the page number
		pageNum = faultObj->addr / USLOSS_MmuPageSize();

    	/* Look for free frame in the frameTable */
    	for(iter = 0; iter < frameTableSize; iter++){
    		if(frameTable[iter].useBit == DB_UNUSED){
    			freeFrameFound = frameTable[iter];
    			break;
    		}
    	}

    	// If a free frame isn't found then use clock algorithm to replace a page (perhaps write to disk)
    	if(freeFrameFound == NULL){
    		for(;;clockHand = ++clockHand % frameTableSize){
    			// enter clockHand mutex to check bits and possibly do assignment
    			MboxReceive(clockHandMbox, dummy, 0);

    			// retrieve the use bits to see if they have been referenced recently
				USLOSS_MmuGetAccess(frameTable[clockHand]->frame,axBits);
				// if it has been referenced, change the marking to zero and continue
				if(axBits & USLOSS_MMU_REF){
					// set the reference bit to unread
					axBits = axBits & USLOSS_MMU_DIRTY; // preserve value of dirty bit, set ref bit to zero
					USLOSS_MmuSetAccess(frameTable[clockHand]->frame, axBits);
				}
				// if the frame has not been referenced
				else{
					// if the frame is dirty, move the page contents to the temporary buffer to be written to disk
					if(axBits & USLOSS_MMU_DIRTY){
						// set the pointer to that frame's page table entry
						pgPtr = getPageTableEntry(faultObj->pid, frameTable[clockHand]->page);
						// copy the frame into the temporary buffer
						memcpy(pageBuff,frameTable[clockHand]->frame, USLOSS_MmuPageSize());
						// erase the information stored in the frame's location
						memset(frameTable[clockHand]->frame,0,USLOSS_MmuPageSize());
					}
				}
				// exit clockHand mutex
				MboxSend(clockHandMbox, dummy, 0);
    		}
    	}

        // Load page into frame from disk, if necessary

    	// load the mapping information into the faulted process's page table

    	// Unblock waiting (faulting) process
    	MboxSend(faultObj->replyMbox, buf, 0);
    }
    return 0;
} /* Pager */



PTE * getPageTableEntry(){
	PTE * target;
	// starting at the top of the process's page table
	target = processes

} /* getPageTableEntry */
