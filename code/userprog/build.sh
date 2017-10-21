#!/bin/bash

set -e

cd ../bin
make

cd ../threads
make depend && make

cd ../userprog
make depend && make

cd ../test
make
