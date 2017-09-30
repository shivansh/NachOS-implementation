Description of the implemented system calls


Syscall_GetReg : Used WriteRegister function from Machine class to write the contents of the argument register given in $4 in $2.


Syscall_GetPA : Read virtual address from $4. Then used Translate function to get the physical address in physAddr variable and exception type in in variable exceptionName. If exception came out to be AddressErrorException or BusErrorException or PageFaultException, then returned -1 else return physAddr.


Syscall_NumInstr : Initialised a variable keeping count whenever thread is created to 0. Then increased that variable in Run() function in mipssim.cc. Then return that variable whenever the fucntion is called in test/user program.


Syscall_Time : Returning the totalTicks variable to the return register or $2.


Syscall_Yield: On invocation of this system call, the method YieldCPU() of currentThread object is invoked.


Syscall_GetPID : We add a getter in the class NachOSThread which returns the PID of the current thread.
**Assigning a unique PID to the newly created threads**
We maintain an array "pidTable" which keeps track of all the assigned
PID values. For assigning a new PID, the least free PID is chosen which
is stored in "minFreePID". When a process/thread exits, "pidTable" and
other variables are appropriately updated.


Average running time complexities:
Insertion: O(1)
Deletion (relinquishing the PID of the dying thread): O(1)


Currently the number of entries in "pidTable" is stored in the macro
THREADLIMIT which is 256 ; this is subjected to change.
It is currently debatable as to what should be the minimum value of the
PID that can be assigned to a new process/thread ; in "real" systems the
swapper has PID 0 and init has PID 1, however it is not yet confirm if
these exist currently in NachOS during runtime.


Syscall_GetPPID : We add a getter in the class NachOSThread which returns the PID of the parent of the current thread.
Syscall_Exec : The value in register $4 to this system call is the location in the virtual address space where the argument to the system call (the name of the executable) is present. We copy the name of the executable from the given address byte-by-byte and then invoke LauncUserProcess() which creates a new address space and starts executing the executable.


Syscall_Sleep : We maintain a List() of key value pairs for all the sleeping threads (listOfSleepingThreads), of the form (NachOSThread*, timeUntilWakeup). When this system call is invoked, the current thread is inserted (sorted insert) to the list, with the value being the sum of total timer ticks until now and the argument to the sytem call.
On every timer tick, a callback is triggered. In this callback, we remove the thread with least wakeup time from listOfSleepingThreads and check whether the total ticks until this instant are more than it’s wakeup time ; if they are then the thread is put on the ready queue, else it is inserted back (sorted insert) to the listOfSleepingThreads.


Syscall_Exit : The pointer to currently scheduled thread is saved, and all interrupts are disabled. Then, the next thread in the ready queue is scheduled, and the thread object (which was previously schedule) pointed to by the saved pointer is deleted. The program counters are appropriately incremented.

________________________________________________________________________________
