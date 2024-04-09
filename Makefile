CC = g++
CFLAGS = -std=c++17 -Wall
LDFLAGS = -lncursesw -lformw -lpanelw

all: my_program

my_program: main.o file_panel.o
	$(CC) $(CFLAGS) main.o file_panel.o -o my_program $(LDFLAGS)

main.o: main.cpp file_panel.h
	$(CC) $(CFLAGS) -c main.cpp

file_panel.o: file_panel.cpp file_panel.h
	$(CC) $(CFLAGS) -c file_panel.cpp

clean:
	rm -f *.o my_program
