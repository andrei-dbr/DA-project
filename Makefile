CC = mpicc
MPIEXEC = mpiexec
CFLAGS = -Wall -Wextra
LDFLAGS =

SRCS = main.c neighbors_config.c node_config.c
OBJS = $(SRCS:.c=.o)
EXEC = main

NUM_NODES = 10

.PHONY: all clean

build: $(EXEC)

run: $(EXEC)
	$(MPIEXEC) -n $(NUM_NODES) ./$(EXEC)

$(EXEC): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(EXEC) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(EXEC)
