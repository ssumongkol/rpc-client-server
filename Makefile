# Define C compiler & flags
CC = gcc
CFLAGS = -Wall -g

# Define libraries to be linked (for example -lm)
LIB = 

# object file
RPC_SYSTEM=rpc.o

.PHONY: format all

all: $(RPC_SYSTEM)

$(RPC_SYSTEM): rpcAlone.o function.o
	ld -r $^ -o $(RPC_SYSTEM)

rpcAlone.o: rpc.c rpc.h function.h
	$(CC) $(CFLAGS) -c $< -o $@

function.o: function.c function.h
	$(CC) $(CFLAGS) -c $< -o $@

# RPC_SYSTEM_A=rpc.a
# $(RPC_SYSTEM_A): rpc.o
#   ar rcs $(RPC_SYSTEM_A) $(RPC_SYSTEM)

clean:
	rm -f *.o

format:
	clang-format -style=file -i *.c *.h