all : fixed_queue_test.out

fixed_queue_test.out : fixed_queue_test.cpp fixed_queue.h
	${CXX} fixed_queue_test.cpp -o $@ -O3 -g -Wall -lpthread

all : linked_queue_test.out

linked_queue_test.out : linked_queue_test.cpp linked_queue.h
	${CXX} linked_queue_test.cpp -o $@ -O3 -g -Wall -mcx16 -lpthread -latomic

all : linked_queue_64_test.out

linked_queue_64_test.out : linked_queue_64_test.cpp linked_queue_64.h
	${CXX} linked_queue_64_test.cpp -o $@ -O3 -g -Wall -mcx16 -lpthread -latomic

clean:
	rm -f *.out
