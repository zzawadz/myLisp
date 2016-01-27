CC = gcc

all: myLisp

myLisp: main.c
	$(CC) -std=c99 -Wall main.c -o myLisp

