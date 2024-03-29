// stats.h
//	Routines for managing statistics about Nachos performance.
//
// DO NOT CHANGE -- these stats are maintained by the machine emulation.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "utility.h"
#include "stats.h"
#include <climits>

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))

//----------------------------------------------------------------------
// Statistics::Statistics
// 	Initialize performance metrics to zero, at system startup.
//----------------------------------------------------------------------

Statistics::Statistics()
{
    totalTicks = idleTicks = systemTicks = userTicks = 0;
    numDiskReads = numDiskWrites = 0;
    numConsoleCharsRead = numConsoleCharsWritten = 0;
    numPageFaults = numPacketsSent = numPacketsRecvd = 0;
    cpuBusyTime = cpuUtilization = 0;
    maxCPUBurst = maxFinishTime = INT_MIN;
    minCPUBurst = minFinishTime = INT_MAX;
    avgCPUBurst = totalCPUBursts = avgWaitingTime = avgFinishTime = 0;
    timerInterruptTicks = 100;
    errorCPUBurst = 0;
    a = 0.5;
}

//----------------------------------------------------------------------
// Statistics::trackCPUBurst
// 	Update statistics corresponding to CPU burst
//----------------------------------------------------------------------
void
Statistics::trackCPUBurst(int currentBurst)
{
#ifdef USER_PROGRAM
    if (currentBurst > 0) {
	avgCPUBurst = avgCPUBurst*totalCPUBursts + currentBurst;
	totalCPUBursts++;
	avgCPUBurst /= totalCPUBursts;

	maxCPUBurst = max(maxCPUBurst, currentBurst);
	minCPUBurst = min(minCPUBurst, currentBurst);
    }
#endif
}

//----------------------------------------------------------------------
// Statistics::trackWaitTime
//----------------------------------------------------------------------
void
Statistics::trackWaitTime(int currentWaitTime)
{
    avgWaitingTime = avgWaitingTime*executableCount + currentWaitTime;
    avgWaitingTime /= executableCount;
}

//----------------------------------------------------------------------
// Statistics::trackFinishTime
//----------------------------------------------------------------------
void
Statistics::trackFinishTime(int currentFinishTime)
{
    finishTimeArray[totalCompletions] = currentFinishTime;
    avgFinishTime = avgFinishTime*totalCompletions + currentFinishTime;
    totalCompletions++;
    avgFinishTime /= totalCompletions;

    maxFinishTime = max(maxFinishTime, currentFinishTime);
    minFinishTime = min(minFinishTime, currentFinishTime);
}

//----------------------------------------------------------------------
// Statistics::evaluateVariance
// 	Evaluate variance of thread completion times.
//----------------------------------------------------------------------
float
Statistics::evaluateVariance()
{
    float varianceFinishTimes = 0;

    // Evaluate variance of thread completion times.
    for (int index = 0; index < 10; index++)
        varianceFinishTimes += (finishTimeArray[index]-avgFinishTime)
			     * (finishTimeArray[index]-avgFinishTime);

    varianceFinishTimes /= totalCompletions;
    return varianceFinishTimes;
}

//----------------------------------------------------------------------
// Statistics::Print
// 	Print performance metrics, when we've finished everything
//	at system shutdown.
//----------------------------------------------------------------------

void
Statistics::Print()
{
    printf("Ticks: total %d, idle %d, system %d, user %d\n", totalTicks,
	idleTicks, systemTicks, userTicks);
    printf("Disk I/O: reads %d, writes %d\n", numDiskReads, numDiskWrites);
    printf("Console I/O: reads %d, writes %d\n", numConsoleCharsRead,
	numConsoleCharsWritten);
    printf("Paging: faults %d\n", numPageFaults);
    printf("Network I/O: packets received %d, sent %d\n", numPacketsRecvd,
	numPacketsSent);

    printf("\n------------------"
	   "\n Statistical Data"
	   "\n------------------\n\n");
    printf("Total CPU busy time: %d\n", systemTicks+userTicks);
    printf("Total execution time: %d\n", totalTicks - simulationStartTime);

    // NOTE: (systemTicks+userTicks) can be replaced by the variable
    // 'avgCPUBurst' which is updated at the end of each CPU burst.
    printf("CPU utilization: %f\n", (systemTicks+userTicks)*1.0 / (totalTicks-simulationStartTime));
    printf("Maximum CPU burst: %d\n", maxCPUBurst);
    printf("Minimum CPU burst: %d\n", minCPUBurst);
    printf("Average CPU burst: %f\n", avgCPUBurst);
    // FIXME Hacky solution for printing error when only using SJF
    if (errorCPUBurst > 0)
	printf("Error in CPU burst estimation: %f\n", errorCPUBurst / (avgCPUBurst*totalCPUBursts));
    printf("Number of non-zero CPU bursts: %d\n", totalCPUBursts);
    printf("Average waiting time in ready queue: %f\n", avgWaitingTime);
    printf("Maximum thread completion time: %d\n", maxFinishTime);
    printf("Minimum thread completion time: %d\n", minFinishTime);
    printf("Average thread completion time: %f\n", avgFinishTime);
    printf("Variance of thread completion times: %f\n", evaluateVariance());
}
