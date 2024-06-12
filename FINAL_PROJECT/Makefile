FLAGS = -Wall -Werror -Wextra -pedantic -g -fsanitize=address

EXEC = server client

all: EXEC

EXEC:
	gcc server.c -o server -pthread
	gcc client.c -o client

clean:
	rm -rf $(EXEC)