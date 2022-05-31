#include "linked_queue_64.h"
#include <stdio.h>
#include <thread>
#include <vector>
#include <assert.h>
#include <signal.h>

struct Element {
    unsigned long long value = 0;
    std::string tag;
};

static volatile int stop = 0;

static void sig_handler(int sig) {
    stop = 1;
}

int main() {

    LinkedQueue64<Element> q;

    std::vector<std::thread *> pushers;
    std::vector<std::thread *> popers;

    std::atomic<unsigned long long> push_counter(1);

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
    
    for(int i = 0; i < 1; ++i) {
        pushers.emplace_back( new std::thread([&q, &push_counter]() {
                    for(;!stop;) {
                        unsigned long long v = push_counter.fetch_add(1);
                        Element e{v, "__TAG__"};

                        bool ok;
                        do {
                            ok = q.Push(std::move(e));
                        } while(!ok);
                    }
                    }) );
    }

    for(int i = 0; i < 1; ++i) {
        popers.emplace_back( new std::thread([&q]() {
                    unsigned long long last_value = (unsigned long long)0;
                    for(;!stop;) {
                        Element e;

                        bool ok;
                        do {
                            ok = q.Pop(&e);
                        } while(!ok);

                        if(e.value <= last_value) {
                            printf("value is invalid, value=%llu, last_value=%llu\n",
                                    e.value, last_value);
                            assert(false);
                        }

                        last_value = e.value;

                        if((last_value & (unsigned long long)0xfffff) == 0) {
                            printf("current value=%llu\n", last_value);
                        }
                    }
                    }) );
    }

    for(std::thread *t: pushers) {
        t->join();
        delete t;
    }

    for(std::thread *t: popers) {
        t->join();
        delete t;
    }
    
    return 0;
}
