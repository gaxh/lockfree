#ifndef __FIXED_QUEUE_H__
#define __FIXED_QUEUE_H__

#include <stdlib.h>
#include <assert.h>

#include <utility>
#include <atomic>
#include <new>

template<typename element_type, size_t capacity>
class FixedQueue {
public:
    using ElementType = element_type;
    static constexpr size_t Capacity = capacity;

    bool Push(const ElementType &elem) {
        ElementType copied(elem);
        return PushInternel(std::move(copied));
    }

    bool Push(ElementType &&elem) {
        return PushInternel(std::move(elem));
    }

    bool Pop(ElementType *elem) {
        return PopInternel(elem);
    }

    bool Empty() const {
        return Size() == 0;
    }

    size_t Size() const {
        return m_tail.load() - m_head.load() - (size_t)1;
    }

    FixedQueue() {
        m_pool = (ElementNode *)malloc( sizeof(m_pool[0]) * Capacity);
        assert(m_pool);

        for(size_t i = 0; i < Capacity; ++i) {
            ElementNode *node = &m_pool[i];
            new (&(node->has_elem)) std::atomic_bool(false);
        }
    }

    ~FixedQueue() {
        for(size_t i = 0; i < Capacity; ++i) {
            ElementNode *node = &m_pool[i];

            if(node->has_elem) {
                node->elem.~ElementType();
            }

            node->has_elem.std::atomic_bool::~atomic_bool();
        }
        free(m_pool);
    }

    FixedQueue(const FixedQueue &) = delete;
    FixedQueue(FixedQueue &&) = delete;
    FixedQueue &operator=(const FixedQueue &) = delete;
    FixedQueue &operator=(FixedQueue &&) = delete;

private:

    bool PushInternel(ElementType &&elem) {
        size_t tail = m_tail.load();
        size_t new_tail;
        size_t tail_index;
        do {
            tail_index = Index(tail);
            if(tail_index == Index(m_head.load())) {
                // queue is full
                return false;
            }

            new_tail = tail + (size_t)1;
        } while(!m_tail.compare_exchange_strong(tail, new_tail));

        ElementNode *node = &m_pool[tail_index];

        // wait until data has been consumed (wait for Pop finished)
        while(node->has_elem.load() == true);

        // set value to index of tail
        new(&(node->elem)) ElementType(std::move(elem));
        node->has_elem.store(true);

        return true;
    }

    bool PopInternel(ElementType *elem) {
        size_t head = m_head.load();
        size_t new_head;
        size_t new_head_index;

        do {
            new_head = head + (size_t)1;
            new_head_index = Index(new_head);
            if(new_head_index == Index(m_tail.load())) {
                // queue is empty
                return false;
            }
        } while(!m_head.compare_exchange_strong(head, new_head));

        // get value from index of head
        ElementNode *node = &m_pool[new_head_index];

        // wait until data is ready (wait for Push finished)
        while(node->has_elem.load() == false);

        if(elem) {
            *elem = std::move(node->elem);
        }
        node->elem.~ElementType();
        node->has_elem.store(false);

        return true;
    }

    struct ElementNode {
        ElementType elem;
        std::atomic_bool has_elem;
    };
    ElementNode *m_pool;

    size_t Index(size_t index) {
        static_assert(Capacity > (size_t)0);
        return index % Capacity;
    }

    alignas(64) std::atomic<size_t> m_head = ATOMIC_VAR_INIT(0); // pop from head
    alignas(64) std::atomic<size_t> m_tail = ATOMIC_VAR_INIT(1); // push to tail
};


#endif
