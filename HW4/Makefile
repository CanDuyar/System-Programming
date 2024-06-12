all: program

program: program.c
	gcc program.c -o program -lrt -lpthread -g -Wall

clean:
	find . -type f | xargs touch
	rm -rf $(OBJS)
	rm program
