#pragma once

#include <memory>
#include <vector>
template <size_t chunkSize>
class FixedAllocator
{
private:
    int8_t* current = nullptr;
    const size_t poolSize = 200'000'000;
public:
    int8_t* blockPtr = nullptr;
    FixedAllocator()
    {
        blockPtr = new int8_t[poolSize];
        current = blockPtr;
    }
    ~FixedAllocator()
    {
        delete[] blockPtr;
    }
    void* allocate()
    {
        int8_t* ret = current;
        current += chunkSize;
        return reinterpret_cast<void*>(ret);
    }
};

template <typename T>
class FastAllocator
{
public:
    using value_type = T;
    std::shared_ptr<FixedAllocator<sizeof(T)>> FA;

    FastAllocator()
    {
        FA = std::make_shared<FixedAllocator<sizeof(T)>>();
    }
    FastAllocator(const FastAllocator& other)
    {
        FA = other.FA;
    }
    ~FastAllocator() = default;
    FastAllocator& operator=(const FastAllocator& other)
    {
        if(this == &other)
            return *this;
        FA = other.FA;
        return *this;
    }
    T* allocate(int n)
    {
        if(n * sizeof(T) <= 32)
        {
            return reinterpret_cast<T*>(FA->allocate());
        }
        else
        {
            return reinterpret_cast<T*>(new int8_t[n * sizeof(T)]);
        }
    }
    void deallocate(T* ptr, int n)
    {
        if(n * sizeof(T) > 32)
        {
            delete[] reinterpret_cast<int8_t*>(ptr);
        }
    }
    template<typename U>
    struct rebind
    {
        using other = FastAllocator<U>;
    };

    template <typename U>
    explicit operator FastAllocator<U>() const
    {
        return FastAllocator<U>();
    }
};

template<typename V, typename U>
bool operator==(const FastAllocator<V>& a, const FastAllocator<U>& b)
{
    return std::is_same_v<V, U> && a.FA->blockPtr == b.FA->blockPtr;
}
template<typename V, typename U>
bool operator!=(const FastAllocator<V>& a, const FastAllocator<U>& b)
{
    return !(a == b);
}

template <typename T, typename Allocator = std::allocator<T>>
class List
{
private:
    struct Node
    {
        Node* next = nullptr;
        Node* prev = nullptr;
        T val = nullptr;
        explicit Node(const T& v) : val(v){}
        explicit Node() : val(T()){}
        ~Node() = default;
        T& getVal()
        {
            return val;
        }
        void setVal(const T& v)
        {
            val = v;
        }
    };

    using Alloc = typename std::allocator_traits<Allocator>::template rebind_alloc<Node>;
    Alloc a;
    Node* head = nullptr;
    Node* tail = nullptr;
    size_t sz = 0;
public:
    explicit List(const Allocator& alloc = Allocator())
    {
        a = typename std::allocator_traits<Allocator>::template rebind_alloc<Node>(alloc);
        head = std::allocator_traits<Alloc>::allocate(a, 1);
        tail = std::allocator_traits<Alloc>::allocate(a, 1);
        head->prev = nullptr;
        tail->next = nullptr;
        head->next = tail;
        tail->prev = head;
    }
    List(size_t count, const T& value, const Allocator& alloc = Allocator())
    {
        //From another constructor
        a = typename std::allocator_traits<Allocator>::template rebind_alloc<Node>(alloc);
        head = std::allocator_traits<Alloc>::allocate(a, 1);
        tail = std::allocator_traits<Alloc>::allocate(a, 1);
        head->prev = nullptr;
        tail->next = nullptr;
        head->next = tail;
        tail->prev = head;
        //
        for(size_t i = 0; i < count; ++i)
        {
            Node* nd;
            nd = std::allocator_traits<Alloc>::allocate(a, 1);
            std::allocator_traits<Alloc>::construct(a, nd, value);
            tail->prev->next = nd;
            nd->prev = tail->prev->next;
            nd->next = tail;
            tail->prev = nd;
        }
        sz = count;
    }
    List(size_t count, const Allocator& alloc = Allocator())
    {
        //From another constructor
        a = typename std::allocator_traits<Allocator>::template rebind_alloc<Node>(alloc);
        head = std::allocator_traits<Alloc>::allocate(a, 1);
        tail = std::allocator_traits<Alloc>::allocate(a, 1);
        head->prev = nullptr;
        tail->next = nullptr;
        head->next = tail;
        tail->prev = head;
        //
        for(size_t i = 0; i < count; ++i)
        {
            Node* nd;
            nd = std::allocator_traits<Alloc>::allocate(a, 1);
            std::allocator_traits<Alloc>::construct(a, nd);
            tail->prev->next = nd;
            nd->prev = tail->prev->next;
            nd->next = tail;
            tail->prev = nd;
        }
        sz = count;
    }
    List(const List& other)
    {
        //From other constructor
        a = std::allocator_traits<Alloc>::select_on_container_copy_construction(other.a);
        head = std::allocator_traits<Alloc>::allocate(a, 1);
        tail = std::allocator_traits<Alloc>::allocate(a, 1);
        head->prev = nullptr;
        tail->next = nullptr;
        head->next = tail;
        tail->prev = head;
        //
        sz = other.sz;
        Node* nd = other.head->next;
        Node* curr = head;
        while(nd->next != nullptr)
        {
            //curr->next = nd;
            curr->next = std::allocator_traits<Alloc>::allocate(a, 1);
            std::allocator_traits<Alloc>::construct(a, curr->next, nd->getVal());
            curr->next->prev = curr;
            curr = curr->next;
            nd = nd->next;
        }
        curr->next = tail;
        tail->prev = curr;
    }

    ~List()
    {
        Node* nd = head->next;
        while(nd->next != nullptr)
        {
            Node* buff = nd->next;
            std::allocator_traits<Alloc>::destroy(a, nd);
            std::allocator_traits<Alloc>::deallocate(a, nd, 1);
            nd = buff;
        }
        std::allocator_traits<Alloc>::deallocate(a, head, 1);
        std::allocator_traits<Alloc>::deallocate(a, tail, 1);
    }

    List& operator=(const List& other)
    {
        if(this == &other)
            return *this;
        Node* nd = head->next;
        Node* other_nd = other.head->next;
        while(nd->next != nullptr)
        {
            Node* buff = nd->next;
            std::allocator_traits<Alloc>::destroy(a, nd);
            std::allocator_traits<Alloc>::deallocate(a, nd, 1);
            nd = buff;
        }
        nd = head;
        if(std::allocator_traits<Allocator>::propagate_on_container_copy_assignment::value && a != other.a)
            a = other.a;
        while(other_nd->next != nullptr)
        {
            nd->next = std::allocator_traits<Alloc>::allocate(a, 1);
            std::allocator_traits<Alloc>::construct(a, nd->next, other_nd->val);
            nd->next->prev = nd;
            nd = nd->next;
            other_nd = other_nd->next;
        }
        tail->prev = nd;
        nd->next = tail;
        sz = other.sz;
        return *this;
    }

    Allocator get_allocator() const
    {
        return typename std::allocator_traits<Alloc>::template rebind_alloc<Allocator>(a);
    }

    size_t size() const
    {
        return sz;
    }

    void push_back(const T& elem)
    {

        Node* nd = std::allocator_traits<Alloc>::allocate(a, 1);
        std::allocator_traits<Alloc>::construct(a, nd, elem);
        tail->prev->next = nd;
        nd->prev = tail->prev;
        tail->prev = nd;
        nd->next = tail;
        ++sz;
    }
    void push_front(const T& elem)
    {
        Node* nd = std::allocator_traits<Alloc>::allocate(a, 1);
        std::allocator_traits<Alloc>::construct(a, nd, elem);
        head->next->prev = nd;
        nd->next = head->next;
        head->next = nd;
        nd->prev = head;
        ++sz;
    }

    void pop_back()
    {
        if(sz == 0)
            return;
        Node* buff = tail->prev;
        tail->prev = tail->prev->prev;
        tail->prev->next = tail;
        std::allocator_traits<Alloc>::destroy(a, buff);
        std::allocator_traits<Alloc>::deallocate(a, buff, 1);
        --sz;
    }
    void pop_front()
    {
        if(sz == 0)
            return;
        Node* buff = head->next;
        head->next = head->next->next;
        head->next->prev = head;
        std::allocator_traits<Alloc>::destroy(a, buff);
        std::allocator_traits<Alloc>::deallocate(a, buff, 1);
        --sz;
    }

    template<bool isConst>
    struct common_iterator
    {
        std::conditional_t<isConst, const Node*, Node*> ptr;
        using iterator_category = std::bidirectional_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = T;
        using pointer = std::conditional_t<isConst, const T*, T*>;
        using reference = std::conditional_t<isConst, const T&, T&>;

        common_iterator(std::conditional_t<isConst, const Node*, Node*> nd) : ptr(nd){}

        common_iterator(const common_iterator& other) : ptr(other.ptr){}
        common_iterator() = default;
        ~common_iterator() = default;

        friend bool operator==(const common_iterator& it1, const common_iterator& it2)
        {
            return it1.ptr == it2.ptr;
        }
        friend bool operator!=(const common_iterator& it1, const common_iterator& it2)
        {
            return it1.ptr != it2.ptr;
        }

        common_iterator& operator=(const common_iterator& other)
        {
            if(this == &other)
                return *this;
            ptr = other.ptr;
            return *this;
        }

        common_iterator& operator++()
        {
            ptr = ptr->next;
            return *this;
        }
        common_iterator operator++(int)
        {
            common_iterator it(ptr);
            ptr = ptr->next;
            return it;
        }

        common_iterator& operator--()
        {
            ptr = ptr->prev;
            return *this;
        }
        common_iterator operator--(int)
        {
            common_iterator it(ptr);
            ptr = ptr->prev;
            return it;
        }

        std::conditional_t<isConst, const T&, T&> operator*() const
        {
            return ptr->val;
        }
        std::conditional_t<isConst, const T*, T*> operator->() const
        {
            std::conditional_t<isConst, const T*, T*> p = ptr->val;
            return p;
        }

        Node* getNode()
        {
            Node* nd = const_cast<Node*>(ptr);
            return nd;
        }

        explicit operator common_iterator<true>()
        {
            common_iterator<true> ci;
            ci.ptr = ptr;
            return ci;
        }
    };

    using iterator = common_iterator<false>;
    using const_iterator = common_iterator<true>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    iterator begin()
    {
        return iterator(head->next);
    }
    const_iterator begin() const
    {
        return const_iterator(head->next);
    }
    const_iterator cbegin() const
    {
        return const_iterator(head->next);
    }

    iterator end()
    {
        return iterator(tail);
    }
    const_iterator end() const
    {
        return const_iterator(tail);
    }
    const_iterator cend() const
    {
        return const_iterator(tail);
    }

    reverse_iterator rbegin()
    {
        iterator it(tail);
        return reverse_iterator(it);
    }
    const_reverse_iterator rbegin() const
    {
        const_iterator it(tail);
        return const_reverse_iterator(it);
    }
    const_reverse_iterator crbegin() const
    {
        const_iterator it(tail);
        return const_reverse_iterator(it);
    }

    reverse_iterator rend()
    {
        iterator it(head->next);
        return reverse_iterator(it);
    }
    const_reverse_iterator rend() const
    {
        const_iterator it(head->next);
        return const_reverse_iterator(it);
    }
    const_reverse_iterator crend() const
    {
        const_iterator it(head->next);
        return const_reverse_iterator(it);
    }

    void insert(iterator iter, const T& val)
    {
        Node* node = iter.getNode();
        Node* nd = std::allocator_traits<Alloc>::allocate(a, 1);
        std::allocator_traits<Alloc>::construct(a, nd, val);
        nd->next = node;
        nd->prev = node->prev;
        node->prev->next = nd;
        node->prev = nd;
        ++sz;
    }

    void erase(iterator iter)
    {
        if(sz == 0)
            return;
        Node* node = iter.getNode();
        Node* nd = node->next;
        node->prev->next = nd;
        nd->prev = node->prev;
        std::allocator_traits<Alloc>::destroy(a, node);
        std::allocator_traits<Alloc>::deallocate(a, node, 1);
        --sz;
    }

    void insert(const_iterator iter, const T& val)
    {
        Node* node = iter.getNode();
        Node* nd = std::allocator_traits<Alloc>::allocate(a, 1);
        std::allocator_traits<Alloc>::construct(a, nd, val);
        nd->next = node;
        nd->prev = node->prev;
        node->prev->next = nd;
        node->prev = nd;
        ++sz;
    }

    void erase(const_iterator iter)
    {
        if(sz == 0)
            return;
        Node* node = iter.getNode();
        Node* nd = node->next;
        node->prev->next = nd;
        nd->prev = node->prev;
        std::allocator_traits<Alloc>::destroy(a, node);
        std::allocator_traits<Alloc>::deallocate(a, node, 1);
        --sz;
    }
};
