# Makefile

CFLAGS = -Wpedantic -g

all: server subscriber

# Compile server.c
server: server.cpp

# Compile subscriber.c
subscriber: subscriber.cpp

.PHONY: clean run_server run_subscriber

clean:
	rm -f server subscriber
