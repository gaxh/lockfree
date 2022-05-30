#ifndef __LINKED_QUEUE_H__
#define __LINKED_QUEUE_H__

#include <stdlib.h>
#include <assert.h>

#include <utility>
#include <atomic>
#include <new>

template<typename element_type>
class LinkedQueue {
public:
    using ElementType = element_type;

    bool Push(const ElementType &elem) {
        ElementType copied(elem);
        return PushInternel(copied);
    }

    bool Push(ElementType &&elem) {
        return PushInternel(std::move(elem));
    }

    bool Pop(ElementType *elem) {
        return PopInternel(elem);
    }

    bool Empty() const {
        return m_head.load().pointer->next == NULL;
    }

    size_t Size() const {
        return m_size.load();
    }

    LinkedQueue() {
        ElementNode *first = AcquireNode();
        first->next = NULL;
        first->version = NextVersion();

        m_head = { first->version, first };
        m_tail = { first->version, first };
    }

    ~LinkedQueue() {
        // "first" node has no element
        ElementNode *first = m_head.load().pointer;

        for(ElementNode *n = first->next; n; ) {
            ElementNode *next = n->next;

            n->element.~ElementType();
            ReleaseNode(n);

            n = next;
        }

        ReleaseNode(first);
    }

    LinkedQueue(const LinkedQueue &) = delete;
    LinkedQueue(LinkedQueue &&) = delete;
    LinkedQueue &operator=(const LinkedQueue &) = delete;
    LinkedQueue &operator=(LinkedQueue &&) = delete;

private:

    bool PushInternel(ElementType &&elem) {
        ElementNode *node = AcquireNode();
        node->next = NULL;
        node->version = NextVersion();
        new (&(node->element)) ElementType(std::move(elem));

        VersionNode new_tail;
        new_tail.version = node->version;
        new_tail.pointer = node;
        VersionNode old_tail = m_tail.load();

        for(;;) {
            if(m_tail.compare_exchange_strong(old_tail, new_tail)) {
                // new_tail has been set
                // link new node
                old_tail.pointer->next = node;
                m_size.fetch_add(1);
                break;
            }
        }

        return true;
    }

    bool PopInternel(ElementType *elem) {
        VersionNode old_head;
        VersionNode new_head;

        old_head = m_head.load();

        do {
            if(old_head.pointer->next == NULL) {
                // queue is empty
                return false;
            }

            new_head.pointer = old_head.pointer->next;
            new_head.version = old_head.pointer->next->version;
        } while(!m_head.compare_exchange_strong(old_head, new_head));

        m_size.fetch_sub(1);

        // delete old "first" node
        ReleaseNode(old_head.pointer);

        if(elem) {
            *elem = std::move(new_head.pointer->element);
        }

        // make old_head.pointer->next the new "first" node
        new_head.pointer->element.~ElementType();
        return true;
    }

    unsigned long NextVersion() {
        return m_version.fetch_add(1);
    }

    struct ElementNode {
        ElementNode *next;
        unsigned long version;
        ElementType element;
    };

    ElementNode *AcquireNode() {
        return (ElementNode *)malloc(sizeof(ElementNode));
    }

    void ReleaseNode(ElementNode *e) {
        free(e);
    }

    struct alignas(16) VersionNode {
        unsigned long version;
        ElementNode *pointer;
    };

    alignas(64) std::atomic<size_t> m_size = ATOMIC_VAR_INIT(0);
    alignas(64) std::atomic<unsigned long> m_version = ATOMIC_VAR_INIT(0);

    alignas(64) std::atomic<VersionNode> m_head; // pop from head
    alignas(64) std::atomic<VersionNode> m_tail; // push to tail
};


#endif
