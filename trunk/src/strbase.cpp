
#include "port.h"
#include "str.h"


const int strrecsize = sizeof(_strrec);


static void stringoverflow() 
{
    fatal(CRIT_FIRST + 21, "String overflow");
}


void string::idxerror()
{
    fatal(CRIT_FIRST + 20, "String index overflow");
}


int stralloc;

char   emptystrbuf[strrecsize + 4];
char*  emptystr = emptystrbuf + strrecsize;


string nullstring;


inline int quantize(int numchars) 
{
	return memquantize(numchars + 1 + strrecsize);
}


void string::_alloc(int numchars) 
{
    if (numchars <= 0)
        stringoverflow();
    size_t a = quantize(numchars);
#ifdef DEBUG
    stralloc += a;
#endif
    data = (char*)(memalloc(a)) + strrecsize;
    STR_LENGTH(data) = numchars;
    STR_REFCOUNT(data) = 1;
    data[numchars] = 0;
}


void string::_realloc(int newchars) 
{
#ifdef DEBUG
    if (newchars <= 0 || STR_LENGTH(data) <= 0)
        stringoverflow();
#endif
    int oldchars = STR_LENGTH(data);
    if (newchars > oldchars || newchars < oldchars / 2) // grow faster, shrink slower
    {
        int a = quantize(newchars);
        int b = quantize(oldchars);
        if (a != b)
        {
#ifdef DEBUG
            stralloc += a - b;
#endif
            data = (char*)(memrealloc(data - strrecsize, a)) + strrecsize;
        }
    }
    STR_LENGTH(data) = newchars;
    data[newchars] = 0;
}


inline void _freestrbuf(char* data)
{
#ifdef DEBUG
    stralloc -= quantize(STR_LENGTH(data));
#endif
    memfree((char*)(STR_BASE(data)));
}


void string::_free() 
{
    _freestrbuf(data);
    data = emptystr;
}


void string::initialize(const char* s1, int len1, const char* s2, int len2)
{
    if (len1 <= 0)
        initialize(s2, len2);
    else if (len2 <= 0)
        initialize(s1, len1);
    else
    {
        _alloc(len1 + len2);
        memcpy(data, s1, len1);
        memcpy(data + len1, s2, len2);
    }
}


void string::initialize(const char* sc, int initlen) 
{
    if (initlen <= 0 || sc == NULL)
        data = emptystr; 
    else 
    {
        _alloc(initlen);
        memmove(data, sc, initlen);
    }
}


void string::initialize(const char* sc) 
{
    initialize(sc, hstrlen(sc));
}


void string::initialize(char c) 
{
    _alloc(1);
    data[0] = c;
}


void string::initialize(const string& s)
{
    data = s.data;
#ifdef SINGLE_THREADED
    STR_REFCOUNT(data)++;
#else
    pincrement(&STR_REFCOUNT(data));
#endif
}


void string::finalize() 
{
    if (STR_LENGTH(data) != 0)
    {

#ifdef SINGLE_THREADED
        if (--STR_REFCOUNT(data) == 0)
#else
        if (pdecrement(&STR_REFCOUNT(data)) == 0)
#endif
            _freestrbuf(data);

        data = emptystr;
    }
}


void string::assign(const char* sc, int initlen) 
{
    if (STR_LENGTH(data) > 0 && initlen > 0 && STR_REFCOUNT(data) == 1)
    {
        // reuse data buffer if unique
        _realloc(initlen);
        memmove(data, sc, initlen);
    }
    else
    {
        finalize();
        if (initlen == 1)
            initialize(sc[0]);
        else if (initlen > 1)
            initialize(sc, initlen);
    }
}


void string::assign(const char* sc) 
{
    assign(sc, hstrlen(sc));
}


void string::assign(char c) 
{
    assign(&c, 1);
}


void string::assign(const string& s) 
{
    if (data != s.data)
    {
        finalize();
        initialize(s);
    }
}


char* string::resize(int newlen)
{
    if (newlen < 0)
        return NULL;

    int curlen = STR_LENGTH(data);

    // if becoming empty
    if (newlen == 0)
        finalize();

    // if otherwise string was empty before
    else if (curlen == 0)
        _alloc(newlen);

    // if length is not changing, return a unique string
    else if (newlen == curlen)
        unique();

    // non-unique reallocation
    else if (STR_REFCOUNT(data) > 1)
    {
        char* odata = data;
        _alloc(newlen);
        int copylen = imin(curlen, newlen);
        memcpy(data, odata, copylen);
#ifdef SINGLE_THREADED
        STR_REFCOUNT(odata)--;
#else
        if (pdecrement(&STR_REFCOUNT(odata)) == 0)
            _freestrbuf(odata);
#endif
    }

    // unique reallocation
    else
        _realloc(newlen);

    return data;
}


char* string::unique()
{
    if (STR_LENGTH(data) > 0 && STR_REFCOUNT(data) > 1)
    {
        char* odata = data;
        _alloc(STR_LENGTH(data));
        memcpy(data, odata, STR_LENGTH(data));
#ifdef SINGLE_THREADED
        STR_REFCOUNT(odata)--;
#else
        if (pdecrement(&STR_REFCOUNT(odata)) == 0)
            _freestrbuf(odata);
#endif
    }
    return data;
}


char* string::append(int cnt)
{
    if (cnt <= 0)
        return NULL;
    int oldlen = STR_LENGTH(data);
    if (oldlen == 0)
    {
        resize(cnt);
        return data;
    }
    resize(oldlen + cnt);
    return data + oldlen;
}


void string::append(const char* sc, int catlen)
{
    int oldlen = STR_LENGTH(data);
    if (data == sc && catlen > 0 && oldlen > 0) // append itself
    {
        resize(oldlen + catlen);
        memmove(data + oldlen, data, catlen);
    }
    else
    {
        char* p = append(catlen);
        if (p != NULL)
            memmove(data + oldlen, sc, catlen);
    }
}


void string::append(const char* sc)
{
    append(sc, hstrlen(sc));
}


void string::append(char c)
{
    if (STR_LENGTH(data) == 0)
        assign(c);
    else 
    {
        resize(STR_LENGTH(data) + 1);
        data[STR_LENGTH(data) - 1] = c;
    }
}


void string::append(const string& s1)
{
    if (STR_LENGTH(data) == 0)
        assign(s1);
    else if (STR_LENGTH(s1.data) > 0)
        append(s1.data, STR_LENGTH(s1.data));
}


string string::copy(int from, int cnt) const
{
    if (STR_LENGTH(data) > 0 && from >= 0 && from < STR_LENGTH(data)) 
    {
        int l = imin(cnt, STR_LENGTH(data) - from);
        if (from == 0 && l == STR_LENGTH(data))
            return *this;
        else if (l > 0) 
            return string(data + from, l);
    }
    return string();
}


string string::operator+ (const char* sc) const  
{
    if (STR_LENGTH(data) == 0)
        return string(sc);
    else
        return string(data, STR_LENGTH(data), sc, hstrlen(sc));
}


string string::operator+ (char c) const
{ 
    if (STR_LENGTH(data) == 0)
        return string(c);
    else
        return string(data, STR_LENGTH(data), &c, 1);
}


string string::operator+ (const string& s) const
{
    if (STR_LENGTH(data) == 0)
        return s;
    else if (STR_LENGTH(s.data) == 0)
        return *this;
    else
        return string(data, STR_LENGTH(data), s.data, STR_LENGTH(s.data));
}


string operator+ (const char* sc, const string& s)
{
    if (STR_LENGTH(s.data) == 0)
        return string(sc);
    else
        return string(sc, hstrlen(sc), s.data, STR_LENGTH(s.data));
}


string operator+ (char c, const string& s)
{
    if (STR_LENGTH(s.data) == 0)
        return string(c);
    else
        return string(&c, 1, s.data, STR_LENGTH(s.data));
}


bool string::operator== (const string& s) const 
{
    return (STR_LENGTH(data) == STR_LENGTH(s.data))
        && ((STR_LENGTH(data) == 0) || (memcmp(data, s.data, STR_LENGTH(data)) == 0));
}


bool string::operator== (char c) const 
{
    return (STR_LENGTH(data) == 1) && (data[0] == c);
}


void string::del(int from, int cnt)
{
    int l = STR_LENGTH(data);
    int d = l - from;
    if (from >= 0 && d > 0 && cnt > 0) 
    {
        if (cnt < d)
        {
            unique();
            memmove(data + from, data + from + cnt, d - cnt);
        }
        else
            cnt = d;
        resize(l - cnt);
    }
}


char* string::ins(int where, int len)
{
    int curlen = STR_LENGTH(data);
    if (len > 0 && where >= 0 && where <= curlen) 
    {
        if (curlen == 0)
        {
            resize(len);
            return data;
        }
        resize(curlen + len);
        char* p = data + where;
        int movelen = STR_LENGTH(data) - where - len;
        if (movelen > 0) 
            memmove(p + len, p, movelen);
        return p;
    }
    return NULL;
}


void string::ins(int where, const char* what, int len)
{
    char* p = ins(where, len);
    if (p != NULL)
        memmove(p, what, len);
}


void string::ins(int where, const char* what)
{
    ins(where, what, hstrlen(what));
}


void string::ins(int where, const string& what)
{
    ins(where, what, STR_LENGTH(what.data));
}


