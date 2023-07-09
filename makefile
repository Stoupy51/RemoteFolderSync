
####################################################################################################
# Universal Makefile for C projects
# Made for Windows, Linux and MacOS
####################################################################################################
#
# This makefile handles the compilation of the project by doing the following instructions:
# - Recursively search for all .c files in src/ and subdirectories
# - Compile all .c files into .o files if they have been modified or
# their dependencies have been modified by checking timestamps
#
# - Recursively search for all .c files in programs/
# - Compile all .c files into executables if they have been modified or
# their dependencies have been modified by checking timestamps
#
####################################################################################################
# Author: 	Stoupy51 (COLLIGNON Alexandre)
####################################################################################################

all:
	@./maker.exe

init:
	gcc maker.c -o maker.exe
	@./maker.exe

clean:
	@./maker.exe clean
	@rm -f maker.exe

restart:
	@clear
	@make clean --no-print-directory
	@make init --no-print-directory

