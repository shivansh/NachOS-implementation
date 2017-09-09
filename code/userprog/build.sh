#!/bin/bash

set -e

cd ../threads
make depend && make

cd ../userprog
make depend && make

cd ../test
make
