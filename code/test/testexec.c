#include "syscall.h"

int
main()
{
    syscall_wrapper_PrintString("PID of the current process: ");
    syscall_wrapper_PrintInt(syscall_wrapper_GetPID());
    syscall_wrapper_PrintString("\nBefore calling Exec.\n");
    syscall_wrapper_Exec("../test/vectorsum");
    syscall_wrapper_PrintString("Returned from Exec.\n"); // Should never return
    return 0;
}
