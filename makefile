# Makefile for CS 372 Project 2 - FTP with sockets
CXX = gcc

CFLAGS = -Wall

TARGET = ftserver

SRCS = ftserver.c

HEADERS =

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CFLAGS) -o $(TARGET) $(SRCS)

clean:
	rm $(TARGET)
