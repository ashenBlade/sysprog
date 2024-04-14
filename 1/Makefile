GCC_FLAGS = -Wextra -Werror -Wall -Wno-gnu-folding-constant -I.

libcoro.o: libcoro.h
	gcc $(GCC_FLAGS) -c libcoro.c

solution.o: solution.c
	gcc $(GCC_FLAGS) -c solution.c 

utils.o: utils.c
	gcc $(GCC_FLAGS) -c utils.c

priority_queue.o: priority_queue.c
	gcc $(GCC_FLAGS) -c priority_queue.c

merge_files.o: merge_files.c
	gcc $(GCC_FLAGS) -c merge_files.c

insertion_sort.o: insertion_sort.c
	gcc $(GCC_FLAGS) -c insertion_sort.c

external_sort.o: external_sort.c
	gcc $(GCC_FLAGS) -c external_sort.c

number_file_reader.o: number_file_reader.c
	gcc $(GCC_FLAGS) -c number_file_reader.c

all: libcoro.o solution.o utils.o priority_queue.o merge_files.o insertion_sort.o external_sort.o number_file_reader.o
	gcc $(GCC_FLAGS) libcoro.o solution.o utils.o priority_queue.o merge_files.o insertion_sort.o external_sort.o number_file_reader.o -o a.out

clean:
	rm a.out *.o
