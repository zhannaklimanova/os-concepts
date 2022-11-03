# CC=gcc
# CFLAGS=-pthread

# TEST1=test1
# TEST2=test2
# TEST3=test3
# TEST4=test4
# TEST5=test5

# tests: queue.c sut.c test1.c test2.c test3.c test4.c test5.c
# 	$(CC) -o $(TEST1) $(CFLAGS) test1.c sut.c queue.c
# 	$(CC) -o $(TEST2) $(CFLAGS) test2.c sut.c queue.c
# 	$(CC) -o $(TEST3) $(CFLAGS) test3.c sut.c queue.c
# 	# $(CC) -o $(TEST4) $(CFLAGS) test4.c sut.c queue.c
# 	# $(CC) -o $(TEST5) $(CFLAGS) test5.c sut.c queue.c

# clean:
# 	rm -rf $(TEST1) $(TEST2) $(TEST3)

CC=gcc
CFLAGS=-pthread

TEST1=test1
TEST2=test2
TEST3=test3
TEST4=test4
TEST5=test5

tests: queue.c sut2.c test1.c test2.c test3.c test4.c test5.c
	$(CC) -o $(TEST1) $(CFLAGS) test1.c sut2.c queue.c
	$(CC) -o $(TEST2) $(CFLAGS) test2.c sut2.c queue.c
	$(CC) -o $(TEST3) $(CFLAGS) test3.c sut2.c queue.c
	$(CC) -o $(TEST4) $(CFLAGS) test4.c sut2.c queue.c
	$(CC) -o $(TEST5) $(CFLAGS) test5.c sut2.c queue.c

clean:
	rm -rf $(TEST1) $(TEST2) $(TEST3) $(TEST4) $(TEST5)