# Multi-Pipelined-Shell-in-C
Implementation of a multi-pipelined shell in C code
Use valid input if you want to be sure to see results that are intended.

To compile this code on a UNIX machine, type in terminal 'make -f Makefile' and to run the code type './driver'
To clean up the object files and executables, type in the terminal 'make clean'

#Current Problems:
Input should loop and continue infinitely until pressing ctrl-c to end the program. Although, current implementation does not accomplish this. If you have a solution, feel free to let me know
File I/O redirection for commands in the pipeline has not been implemented yet.
example: ls -l | wc -c > output.txt    Will not work. If you have a solution, feel free to let me know