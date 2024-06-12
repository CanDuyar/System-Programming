all: player

player: player.c
	gcc player.c -o player -lrt -lpthread -g -Wall

clean:
	find . -type f | xargs touch
	rm -rf $(OBJS)
	rm player
