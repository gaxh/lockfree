#ifndef __LINKED_QUEUE_64_H__
#define __LINKED_QUEUE_64_H__

#include <stdlib.h>
#include <assert.h>

#include <utility>
#include <atomic>
#include <new>

template<typename element_type>
class LinkedQueue64 {
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
        return Pointer(m_head.load())->next == NULL;
    }

    size_t Size() const {
        return m_size.load();
    }

    LinkedQueue64() {
        ElementNode *first = AcquireNode();
        first->next = NULL;
        first->version = NextVersion();

        m_head = Merge(first, first->version);
        m_tail = Merge(first, first->version);
    }

    ~LinkedQueue64() {
        // "first" node has no element
        ElementNode *first = Pointer(m_head.load());

        for(ElementNode *n = first->next; n; ) {
            ElementNode *next = n->next;

            n->element.~ElementType();
            ReleaseNode(n);

            n = next;
        }

        ReleaseNode(first);
    }

    LinkedQueue64(const LinkedQueue64 &) = delete;
    LinkedQueue64(LinkedQueue64 &&) = delete;
    LinkedQueue64 &operator=(const LinkedQueue64 &) = delete;
    LinkedQueue64 &operator=(LinkedQueue64 &&) = delete;

private:

    bool PushInternel(ElementType &&elem) {
        ElementNode *node = AcquireNode();
        node->next = NULL;
        node->version = NextVersion();
        new (&(node->element)) ElementType(std::move(elem));

        unsigned long new_tail = Merge(node, node->version);
        unsigned long old_tail = m_tail.load();

        for(;;) {
            if(m_tail.compare_exchange_strong(old_tail, new_tail)) {
                // new_tail has been set
                // link new node
                Pointer(old_tail)->next = node;
                m_size.fetch_add(1);
                break;
            }
        }

        return true;
    }

    bool PopInternel(ElementType *elem) {
        unsigned long old_head = m_head.load();
        unsigned long new_head;

        do {
            if(Pointer(old_head)->next == NULL) {
                // queue is empty
                return false;
            }

            new_head = Merge(Pointer(old_head)->next, Pointer(old_head)->next->version);
        } while(!m_head.compare_exchange_strong(old_head, new_head));

        m_size.fetch_sub(1);

        // delete old "first" node
        ReleaseNode(Pointer(old_head));

        if(elem) {
            *elem = std::move(Pointer(new_head)->element);
        }

        // make old_head.pointer->next the new "first" node
        Pointer(new_head)->element.~ElementType();
        return true;
    }

    unsigned long NextVersion() {
        return m_version.fetch_add(1) << VERSION_SHIFT;
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

    /*
     * 64bit-linux virtual memory space: 0x0000'00000000~0x7fff'ffffffff and 0xFFFF8000'00000000~0xFFFFFFFF'FFFFFFFF
     * 64bit windows virtual memory space: 0x000'00000000~0x7FFF'FFFFFFFF
     * */
    static constexpr unsigned long POINTER_MASK = 0xffffffffffff;
    static constexpr unsigned long VERSION_MASK = 0xffff000000000000;
    static constexpr unsigned long VERSION_SHIFT = 48;
    static constexpr unsigned long POINTER_DECIDER = 0x800000000000;
    
    static unsigned long Merge(ElementNode *pointer, unsigned long version) {
        static_assert(sizeof(unsigned long) == 8, "sizeof long must = 8");
        return ((unsigned long)pointer & POINTER_MASK) | version;
    }

    static ElementNode *Pointer(unsigned long merged) {
        static_assert(sizeof(unsigned long) == 8, "sizeof long must = 8");
        unsigned long p = merged & POINTER_MASK;
        unsigned long decider = merged & POINTER_DECIDER;
        return (ElementNode *)(p | (~(decider - (unsigned long)1)));
    }

    static unsigned long Version(unsigned long merged) {
        static_assert(sizeof(unsigned long) == 8, "sizeof long must = 8");
        return merged & VERSION_MASK;
    }

    alignas(64) std::atomic<size_t> m_size = ATOMIC_VAR_INIT(0);
    alignas(64) std::atomic<unsigned long> m_version = ATOMIC_VAR_INIT(0);

    alignas(64) std::atomic<unsigned long> m_head; // pop from head
    alignas(64) std::atomic<unsigned long> m_tail; // push to tail
};


#endif
