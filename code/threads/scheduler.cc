// scheduler.cc
//	Routines to choose the next thread to run, and to dispatch to
//	that thread.
//
// 	These routines assume that interrupts are already disabled.
//	If interrupts are disabled, we can assume mutual exclusion
//	(since we are on a uniprocessor).
//
// 	NOTE: We can't use Locks to provide mutual exclusion here, since
// 	if we needed to wait for a lock, and the lock was busy, we would
//	end up calling SelectNextReadyThread(), and that would put us in an
//	infinite loop.
//
// 	Very simple implementation -- no priorities, straight FIFO.
//	Might need to be improved in later assignments.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "scheduler.h"
#include "system.h"

//----------------------------------------------------------------------
// ProcessScheduler::ProcessScheduler
// 	Initialize the list of ready but not running threads to empty.
//----------------------------------------------------------------------

ProcessScheduler::ProcessScheduler()
{
    listOfReadyThreads = new List;
}

//----------------------------------------------------------------------
// ProcessScheduler::~ProcessScheduler
// 	De-allocate the list of ready threads.
//----------------------------------------------------------------------

ProcessScheduler::~ProcessScheduler()
{
    delete listOfReadyThreads;
}

//----------------------------------------------------------------------
// ProcessScheduler::MoveThreadToReadyQueue
// 	Mark a thread as ready, but not running.
//	Put it on the ready list, for later scheduling onto the CPU.
//
//	"thread" is the thread to be put on the ready list.
//----------------------------------------------------------------------

void
ProcessScheduler::MoveThreadToReadyQueue (NachOSThread *thread)
{
    DEBUG('t', "Putting thread %s with PID %d on ready list.\n", thread->getName(), thread->GetPID());

    thread->setStatus(READY);

#ifdef USER_PROGRAM
    thread->statistics->setWaitStartTime(stats->totalTicks);

    if (schedAlgo == 2)
        listOfReadyThreads->SortedInsert(thread,
                                         thread->statistics->getExpectedCPUBurst());
    else if (schedAlgo > 6)
        listOfReadyThreads->SortedInsert(thread,
                                         thread->UNIXPriority);
    else
        listOfReadyThreads->Append((void *)thread);
#else
    listOfReadyThreads->Append((void *)thread);
#endif

}

//----------------------------------------------------------------------
// ProcessScheduler::SelectNextReadyThread
// 	Return the next thread to be scheduled onto the CPU.
//	If there are no ready threads, return NULL.
// Side effect:
//	Thread is removed from the ready list.
//----------------------------------------------------------------------

NachOSThread *
ProcessScheduler::SelectNextReadyThread ()
{
    return (NachOSThread *)listOfReadyThreads->Remove();
}

//----------------------------------------------------------------------
// ProcessScheduler::ScheduleThread
// 	Dispatch the CPU to nextThread.  Save the state of the old thread,
//	and load the state of the new thread, by calling the machine
//	dependent context switch routine, SWITCH.
//
//      Note: we assume the state of the previously running thread has
//	already been changed from running to blocked or ready (depending).
// Side effect:
//	The global variable currentThread becomes nextThread.
//
//	"nextThread" is the thread to be put into the CPU.
//----------------------------------------------------------------------

void
ProcessScheduler::ScheduleThread (NachOSThread *nextThread)
{
    int waitTime;
    NachOSThread *oldThread = currentThread;

#ifdef USER_PROGRAM			// ignore until running user programs
    if (currentThread->space != NULL) {
        // If this thread is a user program, save the user's CPU registers.
        currentThread->SaveUserState();
        currentThread->space->SaveContextOnSwitch();
    }
#endif

    oldThread->CheckOverflow();		    // check if the old thread
    // had an undetected stack overflow

    currentThread = nextThread;		    // switch to the next thread
    currentThread->setStatus(RUNNING);      // nextThread is now running

    // Mark the start of CPU burst.
    waitTime = currentThread->statistics->getWaitTimeAndRun(stats->totalTicks);
    stats->trackWaitTime(waitTime);
    currentThread->statistics->setBurstStartTime(stats->totalTicks);

    DEBUG('t', "Switching from thread \"%s\" with pid %d to thread \"%s\" with pid %d\n",
            oldThread->getName(), oldThread->GetPID(), nextThread->getName(), nextThread->GetPID());

    // This is a machine-dependent assembly language routine defined
    // in switch.s.  You may have to think
    // a bit to figure out what happens after this, both from the point
    // of view of the thread and from the perspective of the "outside world".

    _SWITCH(oldThread, nextThread);

    DEBUG('t', "Now in thread \"%s\" with pid %d\n", currentThread->getName(), currentThread->GetPID());

    // If the old thread gave up the processor because it was finishing,
    // we need to delete its carcass.  Note we cannot delete the thread
    // before now (for example, in NachOSThread::FinishThread()), because up to this
    // point, we were still running on the old thread's stack!
    if (threadToBeDestroyed != NULL) {
        delete threadToBeDestroyed;
        threadToBeDestroyed = NULL;
    }

#ifdef USER_PROGRAM
    if (currentThread->space != NULL) {		// if there is an address space
        currentThread->RestoreUserState();     // to restore, do it.
        currentThread->space->RestoreContextOnSwitch();
    }
#endif
}

//----------------------------------------------------------------------
// ProcessScheduler::Tail
//      This is the portion of ProcessScheduler::ScheduleThread after _SWITCH(). This needs
//      to be executed in the startup function used in fork().
//----------------------------------------------------------------------

void
ProcessScheduler::Tail ()
{
    // If the old thread gave up the processor because it was finishing,
    // we need to delete its carcass.  Note we cannot delete the thread
    // before now (for example, in NachOSThread::FinishThread()), because up to this
    // point, we were still running on the old thread's stack!
    if (threadToBeDestroyed != NULL) {
        delete threadToBeDestroyed;
        threadToBeDestroyed = NULL;
    }

#ifdef USER_PROGRAM
    if (currentThread->space != NULL) {         // if there is an address space
        currentThread->RestoreUserState();     // to restore, do it.
        currentThread->space->RestoreContextOnSwitch();
    }
#endif
}

//----------------------------------------------------------------------
// ProcessScheduler::Print
// 	Print the scheduler state -- in other words, the contents of
//	the ready list.  For debugging.
//----------------------------------------------------------------------
void
ProcessScheduler::Print()
{
    printf("Ready list contents:\n");
    listOfReadyThreads->Mapcar((VoidFunctionPtr) ThreadPrint);
}
