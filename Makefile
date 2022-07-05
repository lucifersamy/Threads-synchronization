all : hw5

hw5 : hw5.o
	gcc -Wall -pthread -o hw5 hw5.o -lm

hw5.o : hw5.c
	gcc -c hw5.c 

clean:
	rm -f hw5 *.o