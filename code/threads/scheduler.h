// scheduler.h
//	Data structures for the thread dispatcher and scheduler.
//	Primarily, the list of threads that are ready to run.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "copyright.h"
#include "list.h"
#include "thread.h"

//----------------------------------------------------------------------
// SchedulingAlgo
// 	Enum for all supported scheduling algorithms.
//----------------------------------------------------------------------
enum SchedulingAlgo {
    NPtvNachOS = 1, 	 	// Non pre-emptive NachOS algo
    NPtvShortestNextBurst = 2,	// Non pre-emptive shortest next CPU burst first
    PtvRoundRobin = 3,   	// Pre-emptive fixed quantum round robin
    PtvPrioritySched = 4 	// Unix like pre-emptive priority scheduling
};


// The following class defines the scheduler/dispatcher abstraction --
// the data structures and operations needed to keep track of which
// thread is running, and which threads are ready but not running.

class ProcessScheduler {
    public:
	ProcessScheduler();	// Initialize list of ready threads
	~ProcessScheduler();	// De-allocate ready list

	void MoveThreadToReadyQueue(NachOSThread* thread);	// Thread can be dispatched.
	// Dequeue first thread on the ready
	// list, if any, and return thread.
	NachOSThread* SelectNextReadyThread();
	void ScheduleThread(NachOSThread* nextThread);	// Cause nextThread to start running
	void Print();		   // Print contents of ready list

	void Tail();		   // Used by fork()

	SchedulingAlgo schedAlgo;  // Selected scheduling algorithm

    private:
	// queue of threads that are ready to run,
	// but not running
	List *listOfReadyThreads;
};

#endif // SCHEDULER_H
