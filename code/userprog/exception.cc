// exception.cc
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"     // threads/
#include "syscall.h"    // userprog/
#include "console.h"    // machine/
#include "synch.h"      // threads/

//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2.
//
// And don't forget to increment the pc before returning. (Or else you'll
// loop making the same system call forever!
//
//	"which" is the kind of exception.  The list of possible exceptions
//	are in machine.h.
//----------------------------------------------------------------------
static Semaphore *readAvail;
static Semaphore *writeDone;
static void ReadAvail(int arg) { readAvail->V(); }
static void WriteDone(int arg) { writeDone->V(); }

static void
ConvertIntToHex (unsigned v, Console *console)
{
    unsigned x;
    if (v == 0) return;
    ConvertIntToHex (v/16, console);
    x = v % 16;
    if (x < 10) {
        writeDone->P() ;
        console->PutChar('0'+x);
    }
    else {
        writeDone->P() ;
        console->PutChar('a'+x-10);
    }
}

//----------------------------------------------------------------------
// Since at the time a thread is switched out it is executing in the
// _SWITCH() function, every scheduled thread would start execution right
// after _SWITCH() when it is selected for running at a later point in time.
// This is, however, not true for a thread T that is scheduled for the first
// time. Before T is scheduled, the currently running thread will call the
// ScheduleThread() method of the ProcessScheduler class (as usual), which
// will invoke _SWITCH(), as usual. However, after the _SWITCH() call returns,
// the new thread T will start running at the function that you passed as the
// first argument to CreateThreadStack(). Hence, the function passed to
// CreateThreadStack() must carry out the things that the ScheduleThread()
// method does after _SWITCH() returns.
//----------------------------------------------------------------------

void
FirstFunctionAfterFork(int dummy)
{
    DEBUG('t', "Now in thread \"%s\"\n", currentThread->getName());

    // If the old thread gave up the processor because it was finishing,
    // we need to delete its carcass.  Note we cannot delete the thread
    // before now (for example, in NachOSThread::FinishThread()), because up to this
    // point, we were still running on the old thread's stack!
    if (threadToBeDestroyed != NULL) {
        delete threadToBeDestroyed;
	threadToBeDestroyed = NULL;
    }

#ifdef USER_PROGRAM
    // If there is an address space to restore, do it.
    if (currentThread->space != NULL) {
        currentThread->RestoreUserState();
	currentThread->space->RestoreContextOnSwitch();
    }
#endif

    // Start running the user thread in the context of the
    // just scheduled NachOS thread.
    machine->Run();
}

void
ExceptionHandler(ExceptionType which)
{
    int type = machine->ReadRegister(2);
    int memval, vaddr, printval, tempval, exitcode, exp;
    unsigned printvalus;        // Used for printing in hex
    int ticksUntilNow;
    IntStatus oldLevel;
    NachOSThread *nextThread;
    NachOSThread *child;

    if (!initializedConsoleSemaphores) {
        readAvail = new Semaphore("read avail", 0);
        writeDone = new Semaphore("write done", 1);
        initializedConsoleSemaphores = true;
    }
    Console *console = new Console(NULL, NULL, ReadAvail, WriteDone, 0);;

    if ((which == SyscallException) && (type == SysCall_Halt)) {
        DEBUG('a', "Shutdown, initiated by user program.\n");
        interrupt->Halt();
    }
    else if ((which == SyscallException) && (type == SysCall_PrintInt)) {
        printval = machine->ReadRegister(4);
        if (printval == 0) {
            writeDone->P() ;
            console->PutChar('0');
        }
        else {
            if (printval < 0) {
                writeDone->P() ;
                console->PutChar('-');
                printval = -printval;
            }
            tempval = printval;
            exp=1;
            while (tempval != 0) {
                tempval = tempval/10;
                exp = exp*10;
            }
            exp = exp/10;
            while (exp > 0) {
                writeDone->P() ;
                // TODO Why a '0' ?
                console->PutChar('0'+(printval/exp));
                printval = printval % exp;
                exp = exp/10;
            }
        }
        // Advance program counters.
        // NOTE: This is to be done at the end of every system call
        // where it's expected that the user program continues execution.
        machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
        machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
        machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == SysCall_PrintChar)) {
        writeDone->P() ;
        console->PutChar(machine->ReadRegister(4));   // echo it!
        // Advance program counters.
        machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
        machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
        machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == SysCall_PrintString)) {
        vaddr = machine->ReadRegister(4);
        machine->ReadMem(vaddr, 1, &memval);
        while ((*(char*)&memval) != '\0') {
            writeDone->P() ;
            console->PutChar(*(char*)&memval);
            vaddr++;
            machine->ReadMem(vaddr, 1, &memval);
        }
        // Advance program counters.
        machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
        machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
        machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == SysCall_PrintIntHex)) {
        printvalus = (unsigned)machine->ReadRegister(4);
        writeDone->P() ;
        console->PutChar('0');
        writeDone->P() ;
        console->PutChar('x');
        if (printvalus == 0) {
            writeDone->P() ;
            console->PutChar('0');
        }
        else {
            ConvertIntToHex (printvalus, console);
        }
        // Advance program counters.
        machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
        machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
        machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }

    else if ((which == SyscallException) && (type == SysCall_NumInstr)) {
        // Returns the total instruction count.
        machine->WriteRegister(2, currentThread->currentInstrCount());

        // Advance program counters.
        machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
        machine->WriteRegister(PCReg,     machine->ReadRegister(NextPCReg));
        machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg) + 4);
    }

    else if ((which == SyscallException) && (type == SysCall_GetPA)) {
        // Return the physical address of the corresponding
        // virtual address passed as the argument.
        //
        // Currently, we exploit the fact that the mapping b/w virtual
        // page entry and physical page entry is one-one. If this was
        // not the case, then the search would have to be performed to
        // find the entry in the KernelPageTable which contains the
        // virtual address passed as the argument, and the physical
        // address of that entry would then be the return value.
        // tempval = machine->KernelPageTable[machine->ReadRegister(4)].physicalPage;
        vaddr = machine->ReadRegister(4);
        int physicalAddr;
        bool writing = FALSE;
        ExceptionType exceptionName = machine->Translate((unsigned)vaddr, &physicalAddr, 100, writing);

        if (exceptionName == AddressErrorException ||
                exceptionName == PageFaultException ||
                exceptionName == BusErrorException)
            machine->WriteRegister(2, -1);
        else
            machine->WriteRegister(2, physicalAddr);

        // Advance program counters.
        machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
        machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
        machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }

    else if ((which == SyscallException) && (type == SysCall_GetReg)) {
        // Returns the contents of the processor register,
        // number of which is passed as the argument.
        // TODO Effects of fflush()

        // Register 4 contains the register number, hence loaded first.
        printval = machine->ReadRegister(machine->ReadRegister(4));
        // TODO Contents of the register can be of any type ???
        // Is it necessary to use console->PutChar() ? Not advisable.

        machine->WriteRegister(2, printval);

        // Advance program counters.
        machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
        machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
        machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }

    else if ((which == SyscallException) && (type == SysCall_GetPID)) {
        // Returns the ID of the calling thread.
        machine->WriteRegister(2, currentThread->getPID());

        // Advance program counters.
        machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
        machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
        machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }

    else if ((which == SyscallException) && (type == SysCall_GetPPID)) {
        // Returns the ID of the parent of the calling thread.
        machine->WriteRegister(2, currentThread->getPPID());

        // Advance program counters.
        machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
        machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
        machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }

    else if ((which == SyscallException) && (type == SysCall_Time)) {
        // Returns the total ticks at present.
        // TODO ticks not in seconds ???
        machine->WriteRegister(2, stats->totalTicks);

        // Advance program counters.
        machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
        machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
        machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }

    else if ((which == SyscallException) && (type == SysCall_Yield)) {
        currentThread->YieldCPU();

        // Advance program counters.
        machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
        machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
        machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }

    else if ((which == SyscallException) && (type == SysCall_Sleep)) {
        // Puts the calling thread to sleep for
        // the number of ticks passed as argument.
        ticksUntilNow = stats->totalTicks;        // Ticks until now.
        tempval = machine->ReadRegister(4);
        oldLevel = interrupt->SetLevel(IntOff);  // Disable interrupts.

        if (tempval == 0)
            currentThread->YieldCPU();
        else {
            scheduler->InsertToSleepList((void*)currentThread,
                    ticksUntilNow + tempval);
            currentThread->PutThreadToSleep();
        }

        interrupt->SetLevel(oldLevel);

        // Advance program counters.
        machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
        machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
        machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }

    else if ((which == SyscallException) && (type == SysCall_Exec)) {
        // Runs a new executable in the current address space.
        // The value in register "$4" is the virtual memory
        // address where the name of executable is located.
        tempval = machine->ReadRegister(4);
        int offset = 0;
        int tempchar;
        // We assume that the name of the executable will not
        // be exceeding 1024 bytes which is a quite relaxed value.
        char executable_name[1024];

        // Read byte-by-byte from memory.
        machine->ReadMem(tempval + offset, 1, &tempchar);
        while((*(char*)&tempchar) != '\0') {
            // Perform a safe pointer recast.
            executable_name[offset] = *(char*)&tempchar;
            offset++;
            machine->ReadMem(tempval + offset, 1, &tempchar);
        }
        executable_name[offset] = *(char*)&tempchar;

        // Load the executable and start running it.
        LaunchUserProcess(executable_name);
    }

    else if ((which == SyscallException) && (type == SysCall_Fork)) {
        // Forks and creates a new child process.
        //
        // The child should start executing from the next PC, hence
        // the program counters are advanced in advance (hah!)
        machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
        machine->WriteRegister(PCReg,     machine->ReadRegister(NextPCReg));
        machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg) + 4);

        tempval = currentThread->getPID();
        child = new NachOSThread("Forked child");
        child->space = new ProcessAddressSpace(currentThread->space);

        // Copy the register set of parent(the current machine registers)
        // into the child's saved user context.
        child->SaveUserState();

        // Fork returns 0 to the child.
        child->ResetReturnValue();

        child->CreateThreadStack(FirstFunctionAfterFork, 0);

        // Move the child to ready queue.
        oldLevel = interrupt->SetLevel(IntOff);    // disable interrupts
        scheduler->MoveThreadToReadyQueue(child);
        interrupt->SetLevel(oldLevel);             // re-enable interrupts.

        // Fork returns the child's PID to the parent.
        machine->WriteRegister(2, child->getPID());
    }

    else if ((which == SyscallException) && (type == SysCall_Join)) {
        // TODO INCOMPLETE! Added to avoid SIGSEGV
        // Advance program counters.
        machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
        machine->WriteRegister(PCReg,     machine->ReadRegister(NextPCReg));
        machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg) + 4);
    }

    else if ((which == SyscallException) && (type == SysCall_Exit)) {
        // Cleanly exit while staying in the kernel-space.
        exitcode = machine->ReadRegister(4);
        tempval = currentThread->getPID();

        // Log exit status in stderr.
        fprintf(stderr, "[pid %d]: Encountered exit status %d\n",
                        tempval, exitcode);

        // Update minFreePID.
        minFreePID = (tempval < minFreePID) ? tempval : minFreePID;

        // Update maxPID.
        if (tempval == maxPID) {
            int i = tempval - 1;

            while (i >= MINPID && !pidTable[i])
                i--;

            maxPID = i;
        }

        threadToBeDestroyed = currentThread;
        oldLevel = interrupt->SetLevel(IntOff);

        machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
        machine->WriteRegister(PCReg,     machine->ReadRegister(NextPCReg));
        machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg) + 4);

        // Schedule the next thread from ready queue.
        // TODO: Validate this.
        DEBUG('t', "Removing thread \"%s\"\n", currentThread->getName());
        nextThread = scheduler->SelectNextReadyThread();

        if (nextThread != NULL) {
            fprintf(stderr, "Updating currentThread\n");
            scheduler->ScheduleThread(nextThread);
            currentThread = nextThread;
            delete threadToBeDestroyed;
        }

        (void) interrupt->SetLevel(oldLevel);

        // NOTE: It makes sense to have a cleanup procedure in the exit()
        // system call as we are not supposed to get back to the userspace.
        // It is not possible to de-allocate the thread data structure
        // as we're still running in the thread. Instead, we set "threadToBeDestroyed"
        // so that ProcessScheduler::ScheduleThread() will call the destructor, once
        // we're running in the context of a different thread.
        // DEBUG('t', "Thread marked destroyable \"%s\"\n", currentThread->getName());
        // threadToBeDestroyed = currentThread;
    }

    else {
        printf("Unexpected user mode exception %d %d\n", which, type);
        ASSERT(FALSE);
    }
}
