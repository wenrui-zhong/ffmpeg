APP = test

# VPATH=../common:../utils:../manager:../wnode:../frame

OBJS =  main.o

CXXFLAGS += -std=c++11

TARGET_CFLAGS = -Wall -I/home/zhong/ffmpeg_build/include

LDFLAGS += -L/home/zhong/ffmpeg_build/lib

LDLIBS = -lavformat -lavutil -lavcodec

.PHONY:all clean

all: $(APP)

$(APP): $(OBJS)
	$(CXX) $(TARGET_CFLAGS) $(LDFLAGS) $(OBJS) -o $@ $(LDLIBS)

%.o: %.c
	$(CC) -c $(TARGET_CFLAGS) $^ -o $@

%.o: %.cpp
	$(CXX) -c $(CXXFLAGS) $(TARGET_CFLAGS) $^ -o $@

clean:
	rm -f $(APP)
	rm -f $(OBJS)
