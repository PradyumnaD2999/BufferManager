CC = gcc
FLAGS = -I -g -Wall

all: test_assign2

test_assign2: test_assign2_1.c test_helper.h dberror.c dberror.h storage_mgr.c storage_mgr.h buffer_mgr.c buffer_mgr.h buffer_mgr_stat.c buffer_mgr_stat.h dt.h 
	$(CC) $(FLAGS) dberror.c storage_mgr.c buffer_mgr.c buffer_mgr_stat.c test_assign2_1.c -o test_assign2

clean:
	rm -rf test_assign2.exe

run:
	./test_assign2