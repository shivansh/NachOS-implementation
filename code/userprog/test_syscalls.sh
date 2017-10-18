#!/bin/bash

echo "~~~~ForkJoin~~~~"
./nachos -x ../test/forkjoin
echo -e "\n===============================\n"
echo "~~~~PrintTest~~~~"
./nachos -x ../test/printtest
echo -e "\n===============================\n"
echo "~~~~TestExec~~~~"
./nachos -x ../test/testexec
echo -e "\n===============================\n"
echo "~~~~TestRegPA~~~~"
./nachos -x ../test/testregPA
echo -e "\n===============================\n"
echo "~~~~TestYield~~~~"
./nachos -x ../test/testyield
echo -e "\n===============================\n"
echo "~~~~VectorSum~~~~"
./nachos -x ../test/vectorsum
echo -e "\n===============================\n"
