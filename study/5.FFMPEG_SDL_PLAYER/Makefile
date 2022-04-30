CC=gcc


.PHONY:all clean

LIB=-L /usr/local/ffmpeg/lib/ -L /usr/local/lib/ -lSDL2 -lavcodec -lavformat -lswscale -lavutil

all:player


player:
	gcc -g simplest_ffmpeg_player_su.cpp -I /usr/local/ffmpeg/include/ $(LIB) -o $@

clean:
	rm -f player
