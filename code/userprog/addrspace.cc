// addrspace.cc
//	Routines to manage address spaces (executing user programs).
//
//	In order to run a user program, you must:
//
//	1. link with the -N -T 0 option
//	2. run coff2noff to convert the object file to Nachos format
//		(Nachos object code format is essentially just a simpler
//		version of the UNIX executable object code format)
//	3. load the NOFF file into the Nachos file system
//		(if you haven't implemented the file system yet, you
//		don't need to do this last step)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "addrspace.h"
#include "noff.h"

//----------------------------------------------------------------------
// SwapHeader
// 	Do little endian to big endian conversion on the bytes in the
//	object file header, in case the file was generated on a little
//	endian machine, and we're now running on a big endian machine.
//----------------------------------------------------------------------

static void
SwapHeader (NoffHeader *noffH)
{
   noffH->noffMagic = WordToHost(noffH->noffMagic);
   noffH->code.size = WordToHost(noffH->code.size);
   noffH->code.virtualAddr = WordToHost(noffH->code.virtualAddr);
   noffH->code.inFileAddr = WordToHost(noffH->code.inFileAddr);
   noffH->initData.size = WordToHost(noffH->initData.size);
   noffH->initData.virtualAddr = WordToHost(noffH->initData.virtualAddr);
   noffH->initData.inFileAddr = WordToHost(noffH->initData.inFileAddr);
   noffH->uninitData.size = WordToHost(noffH->uninitData.size);
   noffH->uninitData.virtualAddr = WordToHost(noffH->uninitData.virtualAddr);
   noffH->uninitData.inFileAddr = WordToHost(noffH->uninitData.inFileAddr);
}

//----------------------------------------------------------------------
// ProcessAddressSpace::ProcessAddressSpace
// 	Create an address space to run a user program.
//	Load the program from a file "executable", and set everything
//	up so that we can start executing user instructions.
//
//	Assumes that the object code file is in NOFF format.
//
//	First, set up the translation from program memory to physical
//	memory.  For now, this is really simple (1:1), since we are
//	only uniprogramming, and we have a single unsegmented page table
//
//	"executable" is the file containing the object code to load into memory
//----------------------------------------------------------------------

ProcessAddressSpace::ProcessAddressSpace(OpenFile *executable)
{
   NoffHeader noffH;
   unsigned int i, size;
   unsigned vpn, offset;
   TranslationEntry *entry;
   unsigned int pageFrame;

   executable->ReadAt((char *)&noffH, sizeof(noffH), 0);
   if ((noffH.noffMagic != NOFFMAGIC) &&
	 (WordToHost(noffH.noffMagic) == NOFFMAGIC))
      SwapHeader(&noffH);
   ASSERT(noffH.noffMagic == NOFFMAGIC);

   // how big is address space?
   size = noffH.code.size + noffH.initData.size + noffH.uninitData.size
      + UserStackSize;	// we need to increase the size
   // to leave room for the stack
   numVirtualPages = divRoundUp(size, PageSize);
   size = numVirtualPages * PageSize;

   ASSERT(numVirtualPages+numPagesAllocated <= NumPhysPages);		// check we're not trying
   // to run anything too big --
   // at least until we have
   // virtual memory

   DEBUG('a', "Initializing address space, num pages %d, size %d\n",
	 numVirtualPages, size);
   // first, set up the translation
   KernelPageTable = new TranslationEntry[numVirtualPages];
   for (i = 0; i < numVirtualPages; i++) {
      KernelPageTable[i].virtualPage = i;
      KernelPageTable[i].physicalPage = i+numPagesAllocated;
      KernelPageTable[i].valid = TRUE;
      KernelPageTable[i].use = FALSE;
      KernelPageTable[i].dirty = FALSE;
      KernelPageTable[i].readOnly = FALSE;  // if the code segment was entirely on
      // a separate page, we could set its
      // pages to be read-only
      KernelPageTable[i].shared = FALSE;
   }
   // zero out the entire address space, to zero the unitialized data segment
   // and the stack segment
   bzero(&machine->mainMemory[numPagesAllocated*PageSize], size);

   numPagesAllocated += numVirtualPages;

   // then, copy in the code and data segments into memory
   if (noffH.code.size > 0) {
      DEBUG('a', "Initializing code segment, at 0x%x, size %d\n",
	    noffH.code.virtualAddr, noffH.code.size);
      vpn = noffH.code.virtualAddr/PageSize;
      offset = noffH.code.virtualAddr%PageSize;
      entry = &KernelPageTable[vpn];
      pageFrame = entry->physicalPage;
      executable->ReadAt(&(machine->mainMemory[pageFrame * PageSize + offset]),
	    noffH.code.size, noffH.code.inFileAddr);
   }
   if (noffH.initData.size > 0) {
      DEBUG('a', "Initializing data segment, at 0x%x, size %d\n",
	    noffH.initData.virtualAddr, noffH.initData.size);
      vpn = noffH.initData.virtualAddr/PageSize;
      offset = noffH.initData.virtualAddr%PageSize;
      entry = &KernelPageTable[vpn];
      pageFrame = entry->physicalPage;
      executable->ReadAt(&(machine->mainMemory[pageFrame * PageSize + offset]),
	    noffH.initData.size, noffH.initData.inFileAddr);
   }
}

//----------------------------------------------------------------------
// ProcessAddressSpace::ProcessAddressSpace (ProcessAddressSpace*) is called by a forked thread.
//      We need to duplicate the address space of the parent.
//----------------------------------------------------------------------

ProcessAddressSpace::ProcessAddressSpace(ProcessAddressSpace *parentSpace)
{
   numVirtualPages = parentSpace->GetNumPages();
   TranslationEntry *parentPageTable = parentSpace->GetPageTable();
   unsigned startAddrParent = parentPageTable[0].physicalPage*PageSize;
   unsigned i, j, numSharedPages = 0, size;
   int k;

   for (i = 0; i < numVirtualPages; i++)
      if (parentPageTable[i].shared)
         numSharedPages++;

   // TODO Are the shared pages supposed to be counted ?
   size = numVirtualPages * PageSize;

   // Check we're not trying to run anything too big --
   // at least until we have virtual memory.
   ASSERT(numVirtualPages+numPagesAllocated <= NumPhysPages);

   DEBUG('a', "Initializing address space, num pages %d, size %d\n",
	 numVirtualPages, size);
   // First, set up the translation
   KernelPageTable = new TranslationEntry[numVirtualPages];

   for (i = 0, j = 0; i < numVirtualPages; i++) {
      KernelPageTable[i].virtualPage = i;
      KernelPageTable[i].valid = parentPageTable[i].valid;
      KernelPageTable[i].use = parentPageTable[i].use;
      KernelPageTable[i].dirty = parentPageTable[i].dirty;
      // If the code segment was entirely on a separate
      // page, we could set its pages to be read-only.
      KernelPageTable[i].readOnly = parentPageTable[i].readOnly;
      KernelPageTable[i].shared = parentPageTable[i].shared;

      if (!parentPageTable[i].shared) {
         // The page is not shared. Allocate a new page and
         // copy the contents from the parent's physical page.
         KernelPageTable[i].physicalPage = numPagesAllocated;

         // Copy the contents.
         for (k = 0; k < PageSize; k++) {
            // The variables 'j' and 'numPagesAllocate' are used to
            // keep track of the physical addresses to write to for
            // the child and parent respectively.
            machine->mainMemory[(numPagesAllocated*PageSize)+ k] =
               machine->mainMemory[startAddrParent + (j*PageSize) +k];
         }

         j++;
         numPagesAllocated++;
      }
      else {
         // The page is shared. Copy the physical page from
         // parent without allocating a new physical page.
         KernelPageTable[i].physicalPage = parentPageTable[i].physicalPage;
      }
   }
}

//----------------------------------------------------------------------
// ProcessAddressSpace::SharedAddressSpace
// 	Sets up a shared address space and returns the starting
// 	virtual address of the first shared page.
//----------------------------------------------------------------------
int
ProcessAddressSpace::SharedAddressSpace(int spaceSize)
{
   DEBUG('a', "Initializing a shared address space,");

   // Historical note: Took an entire day to catch this
   // off-by-one error. The interesting thing is that there
   // is a pre-defined directive for the required result :P
   unsigned numSharedPages = divRoundUp(spaceSize, PageSize);
   TranslationEntry *sharedPageTable = new TranslationEntry[numVirtualPages + numSharedPages];

   // The shared page table contains the entries from the old page
   // table as well as the newly created shared entries.
   // Copy the existing page table entries from the old page table.
   for (unsigned i = 0; i < numVirtualPages; i++) {
      sharedPageTable[i].virtualPage = KernelPageTable[i].virtualPage;
      sharedPageTable[i].physicalPage = KernelPageTable[i].physicalPage;
      sharedPageTable[i].valid = KernelPageTable[i].valid;
      sharedPageTable[i].use = KernelPageTable[i].use;
      sharedPageTable[i].dirty = KernelPageTable[i].dirty;
      sharedPageTable[i].readOnly = KernelPageTable[i].readOnly;
      sharedPageTable[i].shared = KernelPageTable[i].shared;
   }

   // Set up the shared pages.
   for (unsigned i = numVirtualPages; i < numSharedPages + numVirtualPages; i++) {
      sharedPageTable[i].virtualPage = i;
      sharedPageTable[i].physicalPage = i + numPagesAllocated;
      sharedPageTable[i].valid = TRUE;
      sharedPageTable[i].use = FALSE;
      sharedPageTable[i].dirty = FALSE;
      sharedPageTable[i].readOnly = FALSE;
      sharedPageTable[i].shared = TRUE;
   }

   numPagesAllocated += numSharedPages;
   numVirtualPages += numSharedPages;

   // Replace the old page table by the newly created page table.
   // NOTE The old page table needs to be deleted to prevent overflow.
   delete KernelPageTable;
   KernelPageTable = sharedPageTable;

   // Tell the machine where to find the newly created page table.
   RestoreContextOnSwitch();

   // Return the starting virtual address of the first shared page.
   return (numVirtualPages-numSharedPages) * PageSize;
}

//----------------------------------------------------------------------
// ProcessAddressSpace::~ProcessAddressSpace
// 	Dealloate an address space.  Nothing for now!
//----------------------------------------------------------------------

ProcessAddressSpace::~ProcessAddressSpace()
{
   delete KernelPageTable;
}

//----------------------------------------------------------------------
// ProcessAddressSpace::InitUserModeCPURegisters
// 	Set the initial values for the user-level register set.
//
// 	We write these directly into the "machine" registers, so
//	that we can immediately jump to user code.  Note that these
//	will be saved/restored into the currentThread->userRegisters
//	when this thread is context switched out.
//----------------------------------------------------------------------

void
ProcessAddressSpace::InitUserModeCPURegisters()
{
   int i;

   for (i = 0; i < NumTotalRegs; i++)
      machine->WriteRegister(i, 0);

   // Initial program counter -- must be location of "Start"
   machine->WriteRegister(PCReg, 0);

   // Need to also tell MIPS where next instruction is, because
   // of branch delay possibility
   machine->WriteRegister(NextPCReg, 4);

   // Set the stack register to the end of the address space, where we
   // allocated the stack; but subtract off a bit, to make sure we don't
   // accidentally reference off the end!
   machine->WriteRegister(StackReg, numVirtualPages * PageSize - 16);
   DEBUG('a', "Initializing stack register to %d\n", numVirtualPages * PageSize - 16);
}

//----------------------------------------------------------------------
// ProcessAddressSpace::SaveContextOnSwitch
// 	On a context switch, save any machine state, specific
//	to this address space, that needs saving.
//
//	For now, nothing!
//----------------------------------------------------------------------

void ProcessAddressSpace::SaveContextOnSwitch()
{}

//----------------------------------------------------------------------
// ProcessAddressSpace::RestoreContextOnSwitch
// 	On a context switch, restore the machine state so that
//	this address space can run.
//
//      For now, tell the machine where to find the page table.
//----------------------------------------------------------------------

void ProcessAddressSpace::RestoreContextOnSwitch()
{
   machine->KernelPageTable = KernelPageTable;
   machine->KernelPageTableSize = numVirtualPages;
}

unsigned
ProcessAddressSpace::GetNumPages()
{
   return numVirtualPages;
}

TranslationEntry*
ProcessAddressSpace::GetPageTable()
{
   return KernelPageTable;
}
