/*
 * vm.h
 */


/*
 * All processes use the same tag.
 */
#define TAG 0

/*
 * Different states for a page.
 */
#define UNUSED 500
#define INCORE 501
#define INDISK 502
#define INBOTH 503

// different states for datablocks
#define DB_UNUSED	1401
#define DB_INUSE	1402

// states for Frames
#define FR_UNUSED 1501
#define FR_INUSE 1502

/*
 * Page table entry.
 */
typedef struct PTE {
    int  state;      // See above.
    int  frame;      // Frame that stores the page (if any). -1 if none.
    int  diskBlock;  // Disk block that stores the page (if any). -1 if none.
    // Add more stuff here
    int pageNum;
    struct PTE *nextPage;
} PTE;

/*
 * Frame table entry
 */
typedef struct FTE {
	void * frame;	// the frame; NULL if none
	void * page;    // the page that references this frame (if any); NULL if none
	int useBit;     // status of frame; FR_UNUSED if free, FR_INUSE if not free
    struct FTE * next;

}FTE;

/*
 * Per-process information.
 */
typedef struct Process {
    int  numPages;   // Size of the page table.
    PTE  *pageTable; // The page table for the process.
    /* Add more stuff here */
    int procMbox;    // a mailbox on which the proc can wait for fault resolution
} Process;

/*
 * Information about page faults. This message is sent by the faulting
 * process to the pager to request that the fault be handled.
 */
typedef struct FaultMsg {
    int  pid;        // Process with the problem.
    void *addr;      // Address of the page that caused the fault.
    int  replyMbox;  // Mailbox to send reply.
} FaultMsg;

#define CheckMode() assert(USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE)
