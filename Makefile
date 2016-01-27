CC = gcc

ifeq ($(OS),Windows_NT)
	READLINE = 
else
	READLINE = -ledit
endif


all: myLisp

myLisp: main.c
	$(CC) $(READLINE) -std=c99 -Wall main.c -o myLisp

