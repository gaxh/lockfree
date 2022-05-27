all : fixed_queue_test.out

fixed_queue_test.out : fixed_queue_test.cpp fixed_queue.h
	g++ fixed_queue_test.cpp -o $@ -O2 -g -Wall -lpthread

clean:
	rm -f *.out
