GCC_FLAGS = -Wextra -Werror -Wall -Wno-gnu-folding-constant

libcoro.o: libcoro.h
	gcc $(GCC_FLAGS) -c libcoro.c

solution.o: solution.c
	gcc $(GCC_FLAGS) -c solution.c 

utils.o: utils.c
	gcc $(GCC_FLAGS) -c utils.c

all: libcoro.o solution.o utils.o
	gcc $(GCC_FLAGS) libcoro.o solution.o utils.o -o a.out

clean:
	rm a.out
