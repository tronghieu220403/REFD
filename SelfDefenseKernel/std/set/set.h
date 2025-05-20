#ifndef SET_H_
#define SET_H_

#include "../memory/memory.h"
#include "../memory/pair.h"
#include "../iterator/iterator.h"

// Custom initializer list
template <typename T>
class InitializerList {
private:
    const T* data_;
    size_t size_;

public:
    using iterator = const T*;
    using const_iterator = const T*;

    InitializerList(const T* data, size_t size) : data_(data), size_(size) {}

    const_iterator Begin() const { return data_; }
    const_iterator End() const { return data_ + size_; }
    size_t Size() const { return size_; }
};

// Set class definition
template <typename Key, typename Compare = bool (*)(const Key&, const Key&)>
class Set {
private:
    // Node structure for the Red-Black Tree
    struct Node {
        Key key;
        bool is_red;
        Node* parent;
        Node* left;
        Node* right;

        explicit Node(const Key& k)
            : key(k), is_red(true), parent(nullptr), left(nullptr), right(nullptr) {}
    };

    Node* root_;
    Node* nil_;  // Sentinel node for leaf nodes
    size_t size_;
    Compare compare_;

    // Helper functions for Red-Black Tree operations
    void LeftRotate(Node* x);
    void RightRotate(Node* y);
    void InsertFixup(Node* z);
    void DeleteFixup(Node* x);
    void Transplant(Node* u, Node* v);
    Node* Minimum(Node* node) const;
    Node* Maximum(Node* node) const;
    Node* Successor(Node* node) const;
    Node* Predecessor(Node* node) const;
    void Clear(Node* node);
    Node* Copy(Node* node, Node* parent);

public:
    // Type definitions
    using key_type = Key;
    using value_type = Key;
    using key_compare = Compare;
    using value_compare = Compare;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = value_type*;
    using const_pointer = const value_type*;

    // Iterator class
    class Iterator : public IteratorBase<value_type, BidirectionalIteratorTag> {
    private:
        Node* node_;
        const Set* set_;

    public:
        using value_type = value_type;
        using difference_type = ptrdiff_t;
        using pointer = const value_type*;
        using reference = const value_type&;
        using iterator_category = BidirectionalIteratorTag;

        Iterator() : node_(nullptr), set_(nullptr) {}
        Iterator(Node* node, const Set* set) : node_(node), set_(set) {}

        reference operator*() const { return node_->key; }
        pointer operator->() const { return &(node_->key); }

        Iterator& operator++();
        Iterator operator++(int);
        Iterator& operator--();
        Iterator operator--(int);

        bool operator==(const Iterator& other) const { return node_ == other.node_; }
        bool operator!=(const Iterator& other) const { return node_ != other.node_; }

        friend class Set;
    };

    using iterator = Iterator;
    using const_iterator = Iterator;  // For simplicity, use the same iterator
    using reverse_iterator = ReverseIterator<iterator>;
    using const_reverse_iterator = ReverseIterator<const_iterator>;

    // Constructors
    Set();
    explicit Set(const key_compare& comp);
    explicit Set(Compare comp, void* /*alloc placeholder*/);
    template <class InputIterator>
    Set(InputIterator first, InputIterator last, const key_compare& comp = key_compare());
    template <class InputIterator>
    Set(InputIterator first, InputIterator last, void* /*alloc placeholder*/);
    Set(const Set& other);
    Set(const Set& other, void* /*alloc placeholder*/);
    Set(Set&& other);
    Set(Set&& other, void* /*alloc placeholder*/);
    Set(InitializerList<value_type> il, const key_compare& comp = key_compare());
    Set(InitializerList<value_type> il, void* /*alloc placeholder*/);

    // Destructor
    ~Set();

    // Assignment operators
    Set& operator=(const Set& other);
    Set& operator=(Set&& other);
    Set& operator=(InitializerList<value_type> il);

    // Iterators
    iterator Begin() const;
    iterator End() const;
    reverse_iterator RBegin() const;
    reverse_iterator REnd() const;
    const_iterator CBegin() const;
    const_iterator CEnd() const;
    const_reverse_iterator CRBegin() const;
    const_reverse_iterator CREnd() const;

    // Capacity
    bool Empty() const;
    size_type Size() const;
    size_type MaxSize() const;

    // Modifiers
    Pair<iterator, bool> Insert(const value_type& val);
    Pair<iterator, bool> Insert(value_type&& val);
    iterator Insert(const_iterator position, const value_type& val);
    iterator Insert(const_iterator position, value_type&& val);
    template <class InputIterator>
    void Insert(InputIterator first, InputIterator last);
    void Insert(InitializerList<value_type> il);
    iterator Erase(const_iterator position);
    size_type Erase(const value_type& val);
    iterator Erase(const_iterator first, const_iterator last);
    void Swap(Set& other);
    void Clear();

    // Observers
    key_compare KeyComp() const;
    value_compare ValueComp() const;

    // Operations
    iterator Find(const value_type& val) const;
    size_type Count(const value_type& val) const;
    iterator LowerBound(const value_type& val) const;
    iterator UpperBound(const value_type& val) const;
    Pair<iterator, iterator> EqualRange(const value_type& val) const;
};

// Implementation details

// Helper compare function if none is provided
template <typename Key>
bool DefaultCompare(const Key& a, const Key& b) {
    return a < b;
}

// Constructors

template <typename Key, typename Compare>
Set<Key, Compare>::Set() : size_(0) {
    compare_ = DefaultCompare;
    nil_ = new Node(Key());
    nil_->is_red = false;
    nil_->left = nil_->right = nil_->parent = nil_;
    root_ = nil_;
}

template <typename Key, typename Compare>
Set<Key, Compare>::Set(const key_compare& comp) : size_(0), compare_(comp) {
    nil_ = new Node(Key());
    nil_->is_red = false;
    nil_->left = nil_->right = nil_->parent = nil_;
    root_ = nil_;
}

template <typename Key, typename Compare>
Set<Key, Compare>::Set(Compare comp, void* /*alloc placeholder*/) : size_(0), compare_(comp) {
    nil_ = new Node(Key());
    nil_->is_red = false;
    nil_->left = nil_->right = nil_->parent = nil_;
    root_ = nil_;
}

template <typename Key, typename Compare>
template <class InputIterator>
Set<Key, Compare>::Set(InputIterator first, InputIterator last, const key_compare& comp)
    : Set(comp) {
    Insert(first, last);
}

template <typename Key, typename Compare>
template <class InputIterator>
Set<Key, Compare>::Set(InputIterator first, InputIterator last, void* /*alloc placeholder*/)
    : Set() {
    Insert(first, last);
}

template <typename Key, typename Compare>
Set<Key, Compare>::Set(const Set& other) : Set(other.compare_) {
    if (other.root_ != other.nil_) {
        root_ = Copy(other.root_, nil_);
        size_ = other.size_;
    }
}

template <typename Key, typename Compare>
Set<Key, Compare>::Set(const Set& other, void* /*alloc placeholder*/) : Set(other) {}

template <typename Key, typename Compare>
Set<Key, Compare>::Set(Set&& other)
    : root_(other.root_), nil_(other.nil_), size_(other.size_), compare_(other.compare_) {
    other.root_ = nullptr;
    other.nil_ = nullptr;
    other.size_ = 0;
}

template <typename Key, typename Compare>
Set<Key, Compare>::Set(Set&& other, void* /*alloc placeholder*/) : Set(static_cast<Set&&>(other)) {}

template <typename Key, typename Compare>
Set<Key, Compare>::Set(InitializerList<value_type> il, const key_compare& comp)
    : Set(comp) {
    Insert(il.Begin(), il.End());
}

template <typename Key, typename Compare>
Set<Key, Compare>::Set(InitializerList<value_type> il, void* /*alloc placeholder*/) : Set() {
    Insert(il.Begin(), il.End());
}

// Destructor

template <typename Key, typename Compare>
Set<Key, Compare>::~Set() {
    Clear();
    delete nil_;
}

// Assignment operators

template <typename Key, typename Compare>
Set<Key, Compare>& Set<Key, Compare>::operator=(const Set& other) {
    if (this != &other) {
        Clear();
        compare_ = other.compare_;
        if (other.root_ != other.nil_) {
            root_ = Copy(other.root_, nil_);
            size_ = other.size_;
        }
        else {
            root_ = nil_;
            size_ = 0;
        }
    }
    return *this;
}

template <typename Key, typename Compare>
Set<Key, Compare>& Set<Key, Compare>::operator=(Set&& other) {
    if (this != &other) {
        Clear();
        delete nil_;
        root_ = other.root_;
        nil_ = other.nil_;
        size_ = other.size_;
        compare_ = other.compare_;

        other.root_ = nullptr;
        other.nil_ = nullptr;
        other.size_ = 0;
    }
    return *this;
}

template <typename Key, typename Compare>
Set<Key, Compare>& Set<Key, Compare>::operator=(InitializerList<value_type> il) {
    Clear();
    Insert(il.Begin(), il.End());
    return *this;
}

// Iterators

template <typename Key, typename Compare>
typename Set<Key, Compare>::iterator Set<Key, Compare>::Begin() const {
    return iterator(Minimum(root_), this);
}

template <typename Key, typename Compare>
typename Set<Key, Compare>::iterator Set<Key, Compare>::End() const {
    return iterator(nil_, this);
}

template <typename Key, typename Compare>
typename Set<Key, Compare>::reverse_iterator Set<Key, Compare>::RBegin() const {
    return reverse_iterator(End());
}

template <typename Key, typename Compare>
typename Set<Key, Compare>::reverse_iterator Set<Key, Compare>::REnd() const {
    return reverse_iterator(Begin());
}

template <typename Key, typename Compare>
typename Set<Key, Compare>::const_iterator Set<Key, Compare>::CBegin() const {
    return Begin();
}

template <typename Key, typename Compare>
typename Set<Key, Compare>::const_iterator Set<Key, Compare>::CEnd() const {
    return End();
}

template <typename Key, typename Compare>
typename Set<Key, Compare>::const_reverse_iterator Set<Key, Compare>::CRBegin() const {
    return RBegin();
}

template <typename Key, typename Compare>
typename Set<Key, Compare>::const_reverse_iterator Set<Key, Compare>::CREnd() const {
    return REnd();
}

// Capacity

template <typename Key, typename Compare>
bool Set<Key, Compare>::Empty() const {
    return size_ == 0;
}

template <typename Key, typename Compare>
typename Set<Key, Compare>::size_type Set<Key, Compare>::Size() const {
    return size_;
}

template <typename Key, typename Compare>
typename Set<Key, Compare>::size_type Set<Key, Compare>::MaxSize() const {
    return static_cast<size_type>(-1) / sizeof(Node);
}

// Modifiers

template <typename Key, typename Compare>
Pair<typename Set<Key, Compare>::iterator, bool> Set<Key, Compare>::Insert(
    const value_type& val) {
    Node* z = new Node(val);
    Node* y = nil_;
    Node* x = root_;

    while (x != nil_) {
        y = x;
        if (compare_(z->key, x->key)) {
            x = x->left;
        }
        else if (compare_(x->key, z->key)) {
            x = x->right;
        }
        else {
            delete z;
            return Pair<iterator, bool>(iterator(x, this), false);
        }
    }

    z->parent = y;
    if (y == nil_) {
        root_ = z;
    }
    else if (compare_(z->key, y->key)) {
        y->left = z;
    }
    else {
        y->right = z;
    }

    z->left = nil_;
    z->right = nil_;
    z->is_red = true;

    InsertFixup(z);
    ++size_;
    return Pair<iterator, bool>(iterator(z, this), true);
}

template <typename Key, typename Compare>
Pair<typename Set<Key, Compare>::iterator, bool> Set<Key, Compare>::Insert(value_type&& val) {
    return Insert(val);  // For simplicity, treating rvalue same as lvalue
}

template <typename Key, typename Compare>
typename Set<Key, Compare>::iterator Set<Key, Compare>::Insert(const_iterator /*position*/,
    const value_type& val) {
    // Hint is ignored for simplicity
    return Insert(val).first;
}

template <typename Key, typename Compare>
typename Set<Key, Compare>::iterator Set<Key, Compare>::Insert(const_iterator /*position*/,
    value_type&& val) {
    return Insert(val);  // For simplicity, treating rvalue same as lvalue
}

template <typename Key, typename Compare>
template <class InputIterator>
void Set<Key, Compare>::Insert(InputIterator first, InputIterator last) {
    for (; first != last; ++first) {
        Insert(*first);
    }
}

template <typename Key, typename Compare>
void Set<Key, Compare>::Insert(InitializerList<value_type> il) {
    Insert(il.Begin(), il.End());
}

template <typename Key, typename Compare>
typename Set<Key, Compare>::iterator Set<Key, Compare>::Erase(const_iterator position) {
    if (position == End()) {
        return iterator();
    }
    Node* z = position.node_;
    Node* y = z;
    Node* x = nullptr;
    bool y_original_color = y->is_red;

    if (z->left == nil_) {
        x = z->right;
        Transplant(z, z->right);
    }
    else if (z->right == nil_) {
        x = z->left;
        Transplant(z, z->left);
    }
    else {
        y = Minimum(z->right);
        y_original_color = y->is_red;
        x = y->right;
        if (y->parent == z) {
            x->parent = y;
        }
        else {
            Transplant(y, y->right);
            y->right = z->right;
            y->right->parent = y;
        }
        Transplant(z, y);
        y->left = z->left;
        y->left->parent = y;
        y->is_red = z->is_red;
    }

    delete z;
    --size_;

    if (!y_original_color) {
        DeleteFixup(x);
    }

    return iterator(x, this);
}

template <typename Key, typename Compare>
typename Set<Key, Compare>::size_type Set<Key, Compare>::Erase(const value_type& val) {
    iterator it = Find(val);
    if (it != End()) {
        Erase(it);
        return 1;
    }
    return 0;
}

template <typename Key, typename Compare>
typename Set<Key, Compare>::iterator Set<Key, Compare>::Erase(const_iterator first,
    const_iterator last) {
    while (first != last) {
        first = Erase(first);
    }
    return iterator(last.node_, this);
}

template <typename Key, typename Compare>
void Set<Key, Compare>::Swap(Set& other) {
    Node* temp_root = root_;
    Node* temp_nil = nil_;
    size_t temp_size = size_;
    Compare temp_compare = compare_;

    root_ = other.root_;
    nil_ = other.nil_;
    size_ = other.size_;
    compare_ = other.compare_;

    other.root_ = temp_root;
    other.nil_ = temp_nil;
    other.size_ = temp_size;
    other.compare_ = temp_compare;
}

template <typename Key, typename Compare>
void Set<Key, Compare>::Clear() {
    Clear(root_);
    root_ = nil_;
    size_ = 0;
}

// Observers

template <typename Key, typename Compare>
typename Set<Key, Compare>::key_compare Set<Key, Compare>::KeyComp() const {
    return compare_;
}

template <typename Key, typename Compare>
typename Set<Key, Compare>::value_compare Set<Key, Compare>::ValueComp() const {
    return compare_;
}

// Operations

template <typename Key, typename Compare>
typename Set<Key, Compare>::iterator Set<Key, Compare>::Find(const value_type& val) const {
    Node* current = root_;
    while (current != nil_) {
        if (compare_(val, current->key)) {
            current = current->left;
        }
        else if (compare_(current->key, val)) {
            current = current->right;
        }
        else {
            return iterator(current, this);
        }
    }
    return End();
}

template <typename Key, typename Compare>
typename Set<Key, Compare>::size_type Set<Key, Compare>::Count(const value_type& val) const {
    return Find(val) != End() ? 1 : 0;
}

template <typename Key, typename Compare>
typename Set<Key, Compare>::iterator Set<Key, Compare>::LowerBound(const value_type& val) const {
    Node* current = root_;
    Node* result = nil_;
    while (current != nil_) {
        if (!compare_(current->key, val)) {
            result = current;
            current = current->left;
        }
        else {
            current = current->right;
        }
    }
    return iterator(result, this);
}

template <typename Key, typename Compare>
typename Set<Key, Compare>::iterator Set<Key, Compare>::UpperBound(const value_type& val) const {
    Node* current = root_;
    Node* result = nil_;
    while (current != nil_) {
        if (compare_(val, current->key)) {
            result = current;
            current = current->left;
        }
        else {
            current = current->right;
        }
    }
    return iterator(result, this);
}

template <typename Key, typename Compare>
Pair<typename Set<Key, Compare>::iterator, typename Set<Key, Compare>::iterator>
Set<Key, Compare>::EqualRange(const value_type& val) const {
    return Pair<iterator, iterator>(LowerBound(val), UpperBound(val));
}

// Helper Functions

template <typename Key, typename Compare>
void Set<Key, Compare>::LeftRotate(Node* x) {
    Node* y = x->right;
    x->right = y->left;
    if (y->left != nil_) {
        y->left->parent = x;
    }
    y->parent = x->parent;
    if (x->parent == nil_) {
        root_ = y;
    }
    else if (x == x->parent->left) {
        x->parent->left = y;
    }
    else {
        x->parent->right = y;
    }
    y->left = x;
    x->parent = y;
}

template <typename Key, typename Compare>
void Set<Key, Compare>::RightRotate(Node* y) {
    Node* x = y->left;
    y->left = x->right;
    if (x->right != nil_) {
        x->right->parent = y;
    }
    x->parent = y->parent;
    if (y->parent == nil_) {
        root_ = x;
    }
    else if (y == y->parent->left) {
        y->parent->left = x;
    }
    else {
        y->parent->right = x;
    }
    x->right = y;
    y->parent = x;
}

template <typename Key, typename Compare>
void Set<Key, Compare>::InsertFixup(Node* z) {
    while (z->parent->is_red) {
        if (z->parent == z->parent->parent->left) {
            Node* y = z->parent->parent->right;
            if (y->is_red) {
                z->parent->is_red = false;
                y->is_red = false;
                z->parent->parent->is_red = true;
                z = z->parent->parent;
            }
            else {
                if (z == z->parent->right) {
                    z = z->parent;
                    LeftRotate(z);
                }
                z->parent->is_red = false;
                z->parent->parent->is_red = true;
                RightRotate(z->parent->parent);
            }
        }
        else {
            Node* y = z->parent->parent->left;
            if (y->is_red) {
                z->parent->is_red = false;
                y->is_red = false;
                z->parent->parent->is_red = true;
                z = z->parent->parent;
            }
            else {
                if (z == z->parent->left) {
                    z = z->parent;
                    RightRotate(z);
                }
                z->parent->is_red = false;
                z->parent->parent->is_red = true;
                LeftRotate(z->parent->parent);
            }
        }
    }
    root_->is_red = false;
}

template <typename Key, typename Compare>
void Set<Key, Compare>::DeleteFixup(Node* x) {
    while (x != root_ && !x->is_red) {
        if (x == x->parent->left) {
            Node* w = x->parent->right;
            if (w->is_red) {
                w->is_red = false;
                x->parent->is_red = true;
                LeftRotate(x->parent);
                w = x->parent->right;
            }
            if (!w->left->is_red && !w->right->is_red) {
                w->is_red = true;
                x = x->parent;
            }
            else {
                if (!w->right->is_red) {
                    w->left->is_red = false;
                    w->is_red = true;
                    RightRotate(w);
                    w = x->parent->right;
                }
                w->is_red = x->parent->is_red;
                x->parent->is_red = false;
                w->right->is_red = false;
                LeftRotate(x->parent);
                x = root_;
            }
        }
        else {
            Node* w = x->parent->left;
            if (w->is_red) {
                w->is_red = false;
                x->parent->is_red = true;
                RightRotate(x->parent);
                w = x->parent->left;
            }
            if (!w->left->is_red && !w->right->is_red) {
                w->is_red = true;
                x = x->parent;
            }
            else {
                if (!w->left->is_red) {
                    w->right->is_red = false;
                    w->is_red = true;
                    LeftRotate(w);
                    w = x->parent->left;
                }
                w->is_red = x->parent->is_red;
                x->parent->is_red = false;
                w->left->is_red = false;
                RightRotate(x->parent);
                x = root_;
            }
        }
    }
    x->is_red = false;
}

template <typename Key, typename Compare>
void Set<Key, Compare>::Transplant(Node* u, Node* v) {
    if (u->parent == nil_) {
        root_ = v;
    }
    else if (u == u->parent->left) {
        u->parent->left = v;
    }
    else {
        u->parent->right = v;
    }
    v->parent = u->parent;
}

template <typename Key, typename Compare>
typename Set<Key, Compare>::Node* Set<Key, Compare>::Minimum(Node* node) const {
    while (node->left != nil_) {
        node = node->left;
    }
    return node;
}

template <typename Key, typename Compare>
typename Set<Key, Compare>::Node* Set<Key, Compare>::Maximum(Node* node) const {
    while (node->right != nil_) {
        node = node->right;
    }
    return node;
}

template <typename Key, typename Compare>
typename Set<Key, Compare>::Node* Set<Key, Compare>::Successor(Node* node) const {
    if (node->right != nil_) {
        return Minimum(node->right);
    }
    Node* y = node->parent;
    while (y != nil_ && node == y->right) {
        node = y;
        y = y->parent;
    }
    return y;
}

template <typename Key, typename Compare>
typename Set<Key, Compare>::Node* Set<Key, Compare>::Predecessor(Node* node) const {
    if (node->left != nil_) {
        return Maximum(node->left);
    }
    Node* y = node->parent;
    while (y != nil_ && node == y->left) {
        node = y;
        y = y->parent;
    }
    return y;
}

template <typename Key, typename Compare>
void Set<Key, Compare>::Clear(Node* node) {
    if (node != nil_) {
        Clear(node->left);
        Clear(node->right);
        delete node;
    }
}

template <typename Key, typename Compare>
typename Set<Key, Compare>::Node* Set<Key, Compare>::Copy(Node* node, Node* parent) {
    if (node == nil_) {
        return nil_;
    }
    Node* new_node = new Node(node->key);
    new_node->is_red = node->is_red;
    new_node->parent = parent;
    new_node->left = Copy(node->left, new_node);
    new_node->right = Copy(node->right, new_node);
    return new_node;
}

// Iterator implementation

template <typename Key, typename Compare>
typename Set<Key, Compare>::Iterator& Set<Key, Compare>::Iterator::operator++() {
    if (node_ == set_->nil_) {
        return *this;
    }
    node_ = set_->Successor(node_);
    return *this;
}

template <typename Key, typename Compare>
typename Set<Key, Compare>::Iterator Set<Key, Compare>::Iterator::operator++(int) {
    Iterator temp = *this;
    ++(*this);
    return temp;
}

template <typename Key, typename Compare>
typename Set<Key, Compare>::Iterator& Set<Key, Compare>::Iterator::operator--() {
    if (node_ == set_->nil_) {
        node_ = set_->Maximum(set_->root_);
        return *this;
    }
    node_ = set_->Predecessor(node_);
    return *this;
}

template <typename Key, typename Compare>
typename Set<Key, Compare>::Iterator Set<Key, Compare>::Iterator::operator--(int) {
    Iterator temp = *this;
    --(*this);
    return temp;
}

#endif  // SET_H_
