#ifndef __RUNTIME_H
#define __RUNTIME_H


#include "charset.h"
#include "common.h"


// --- rcblock ------------------------------------------------------------- //


// Basic reference-counted, resizable memory block. For internal use.

class rcblock: public noncopyable
{
    friend void test_rcblock();

protected:
    int refcount;

    static rcblock* alloc(memint extra)
        { return new(extra) rcblock(); }
    static rcblock* realloc(rcblock*, memint size);

public:
    static int allocated; // for detecting memory leaks in DEBUG mode

    void* operator new(size_t);
    void* operator new(size_t, memint extra);
    void  operator delete (void* p);

    rcblock(): refcount(0)  { }
    bool unique() const     { return refcount == 1; }
    void ref()              { pincrement(&refcount); }
    bool deref()            { assert(refcount >= 1); return pdecrement(&refcount) == 0; }
};


template <class T>
    inline T* rcref(T* o)           { if (o) o->ref(); return o; }
template <class T>
    inline T* rcref2(T* o)          { o->ref(); return o; }
template <class T>
    inline void rcrelease(T* o)     { if (o && o->deref()) delete o; }
template <class T>
    void rcassign(T*& p, T* o)      { if (p != o) { rcrelease(p); p = rcref(o); } }



// --- rcdynamic ----------------------------------------------------------- //


// Memory block for dynamic strings and vectors; maintains current length
// (used) and allocated memory (capacity). For internal use.

class rcdynamic: public rcblock
{
public:
    memint capacity;
    memint used;

    char* data() const          { return (char*)this + sizeof(rcdynamic); }
    char* data(memint i) const  { assert(i >= 0 && i <= used); return data() + i; }
    template <class T>
        T* data(memint i) const { return (T*)data(i * sizeof(T)); }
    memint empty() const        { return used == 0; }
    memint size() const         { return used; }
    void set_size(memint _used) { assert(unique() && !empty() && _used > 0 && _used <= capacity); used = _used; }
    void dec_size(memint d)     { assert(unique() && used >= d); used -= d; }
    memint memsize() const      { return capacity; }

    static rcdynamic* alloc(memint _capacity, memint _used)
        { return new(_capacity) rcdynamic(_capacity, _used); }
    static rcdynamic* realloc(rcdynamic*, memint _capacity, memint _used);

    rcdynamic(memint _capacity, memint _used)
        : rcblock(), capacity(_capacity), used(_used)
                { assert(_capacity >= 0 && _used >= 0 && _used <= _capacity); }
};


// --- container ----------------------------------------------------------- //


// Generic dynamic copy-on-write buffer for POD data. For internal use.

class container: public noncopyable
{
    friend void test_container();
    friend void test_string();

protected:
    struct _nulldyn: public rcdynamic
    {
        char dummy[16];
        _nulldyn(): rcdynamic(0, 0)  { }
        bool valid()  { return size() == 0 && memsize() == 0 && dummy[0] == 0; }
    };
    static _nulldyn _null;

    rcdynamic* dyn;

    void _init()                            { dyn = &_null; }
    char* _init(memint);
    void _init(const char*, memint);
    void _init(const container& c)          { dyn = rcref2(c.dyn); }
    void _dofin();
    void _fin()                             { if (!empty()) _dofin(); }
    void _replace(rcdynamic* d)             { _fin(); dyn = rcref2(d); }

    static void _overflow();
    static void _idxerr();
    void _idx(memint i) const               { if (unsigned(i) >= unsigned(size())) _idxerr(); }
    void _idxa(memint i) const              { if (unsigned(i) > unsigned(size())) _idxerr(); }
    static memint _calc_capcity(memint newsize);
    bool _can_shrink(memint newsize);
    static rcdynamic* _grow_alloc(memint);
    static rcdynamic* _precise_alloc(memint);
    rcdynamic* _grow_realloc(memint);
    rcdynamic* _precise_realloc(memint);

    memint memsize() const                  { return dyn->memsize(); }
    bool unique() const                     { return empty() || dyn->unique(); }
    char* _mkunique();

public:
    container()                             { _init(); }
    container(const char* buf, memint len)  { _init(buf, len); }
    container(const container& c)           { _init(c); }
    ~container()                            { _fin(); }

    bool empty() const                      { return dyn->empty(); }
    memint size() const                     { return dyn->size(); }
    const char* data() const                { return dyn->data(); }
    const char* data(memint i) const        { return dyn->data(i); }
    const char* at(memint i) const;
    const char* back() const;
    void mkunique()                         { if (!unique()) _mkunique(); }
    void put(memint pos, char c);

    void clear();
    char* assign(memint);
    void assign(const char*, memint);
    void assign(const container&);
    void operator= (const container& c)     { assign(c); }
    char* insert(memint pos, memint len);
    void insert(memint pos, const char*, memint len);
    void insert(memint pos, const container&);
    char* append(memint len);
    void append(const char*, memint len);
    void append(const container&);
    void erase(memint pos, memint len);
    void pop_back(memint);
    char* resize(memint);
    void resize(memint, char);

    template <class T>
        const T& back() const               { if (empty()) _idxerr(); return *(T*)data(size() - sizeof(T)); }
    template <class T>
        void push_back(const T& t)          { *(T*)append(sizeof(T)) = t; }
    template <class T>
        void pop_back()                     { pop_back(sizeof(T)); }
};


// --- string -------------------------------------------------------------- //


// Dynamic copy-on-write strings. Note that c_str() is a bit costly, because
// it's not used a lot in Shannon. Many methods are simply inherited from
// container.

class str: public container
{
protected:
    void _init(const char*);

public:
    str(): container()                      { }
    str(const char* s)                      { _init(s); }
    str(const str& s): container(s)         { }
    str(const char* buf, memint len): container(buf, len)  { }
    ~str()                                  { }

    const char* c_str() const;
    const char& operator[](memint i) const  { return *data(i); }
    const char& at(memint pos) const        { return *container::at(pos); }
    const char& back() const                { return *container::back(); }
    int cmp(const char*, memint) const;
    memint find(char c) const;
    memint rfind(char c) const;

    bool operator== (const char* s) const   { return cmp(s, pstrlen(s)) == 0; }
    bool operator== (const str& s) const    { return cmp(s.data(), s.size()) == 0; }
    bool operator!= (const char* s) const   { return !(*this == s); }
    bool operator!= (const str& s) const    { return !(*this == s); }
    void operator+= (const char* s);
    void operator+= (const str& s)          { container::append(s); }
    str  substr(memint pos, memint len) const;
    str  substr(memint pos) const           { return substr(pos, size() - pos); }

    enum { npos = -1 };
};

inline bool operator== (const char* s1, const str& s2)
    { return s2.cmp(s1, pstrlen(s1)) == 0; }

inline bool operator!= (const char* s1, const str& s2)
    { return !(s1 == s2); }


// --- string utilities ---------------------------------------------------- //


str _to_string(long long value, int base, int width, char fill);
str _to_string(long long);
template<class T>
    inline str to_string(const T& value, int base, int width = 0, char fill = '0')
        { return _to_string((long long)value, base, width, fill); }
template<class T>
    inline str to_string(const T& value)
        { return _to_string((long long)value); }

unsigned long long from_string(const char*, bool* error, bool* overflow, int base = 10);

str remove_filename_path(const str&);
str remove_filename_ext(const str&);


// --- podvec -------------------------------------------------------------- //


template <class T>
class podvec: protected container
{
    friend void test_podvec();

protected:
    enum { Tsize = sizeof(T) };
    T* data(memint i) const                 { return (T*)container::data(i * Tsize); }

public:
    podvec(): container()                   { }
    podvec(const podvec& v): container(v)   { }
    ~podvec()                               { }
    bool empty() const                      { return container::empty(); }
    memint size() const                     { return container::size() / Tsize; }
    const T& operator[] (memint i) const    { return *(T*)container::data(i * Tsize); }
    const T& at(memint i) const             { return *(T*)container::at(i * Tsize); }
    const T& back() const                   { return container::back<T>(); }
    void clear()                            { container::clear(); }
    void operator= (const podvec& v)        { container::assign(v); }
    void push_back(const T& t)              { *(T*)container::append(Tsize) = t; }
    void pop_back()                         { container::pop_back<T>(); }
    void insert(memint pos, const T& t)     { *(T*)container::insert(pos * Tsize, Tsize) = t; }
    void erase(memint pos)                  { container::erase(pos * Tsize, Tsize); }
};


// --- vector -------------------------------------------------------------- //


template <class T>
class vector: public podvec<T>
{
    // Note: the inheritance is public for the sake of simplicity, but be
    // careful when adding new methods to podvec: make sure they work properly
    // in vector too, or otherwise redefine them here.
protected:
    enum { Tsize = sizeof(T) };
    void _fin();
    void _mkunique();
    void mkunique()                         { if (!vector::unique()) _mkunique(); }

public:
    vector(): podvec<T>()                   { }
    vector(const vector& v): podvec<T>(v)   { }
    ~vector()                               { _fin(); }
    void clear()                            { _fin(); vector::_init(); }
    void operator= (const vector& v);
    void push_back(const T& t);
    void pop_back();
    void insert(memint pos, const T& t);
    void erase(memint pos);
};


template <class T>
void vector<T>::_mkunique()
{
    rcdynamic* d = container::_precise_alloc(container::size());
    for (memint i = 0; i < vector::size(); i++)
        ::new(d->data<T>(i)) T(vector::operator[](i));
    vector::_replace(d);
}


template <class T>
void vector<T>::_fin()
{
     if (!vector::empty())
     {
        if (vector::dyn->deref())
        {
            for (memint i = vector::size() - 1; i >= 0; i--)
                vector::operator[](i).~T();
            delete vector::dyn;
        }
        container::_init(); // make the base dtors happy
     }
}


template <class T>
void vector<T>::operator= (const vector& v)
{
    if (vector::dyn != v.dyn)
    {
        _fin();
        container::_init(v);
    }
}


template <class T>
void vector<T>::push_back(const T& t)
{
    mkunique();
    new((T*)container::append(Tsize)) T(t);
}


// The 3 methods below can be improved in terms of performance but will be
// bigger in that case
template <class T>
void vector<T>::pop_back()
{
    mkunique();
    container::back<T>().~T();
    vector::dyn->dec_size(Tsize);
}


template <class T>
void vector<T>::insert(memint pos, const T& t)
{
    mkunique();
    new((T*)container::insert(pos * Tsize, Tsize)) T(t);
}


template <class T>
void vector<T>::erase(memint pos)
{
    mkunique();
    vector::at(pos).~T();
    podvec<T>::erase(pos);
}


#endif // __RUNTIME_H
