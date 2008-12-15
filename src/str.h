#ifndef __STRING_H
#define __STRING_H

#include <string.h>


#ifndef __PORT_H
#include "port.h"
#endif


// dynamic string class with thread-safe ref-counted buffer

struct _strrec 
{
    int refcount;
    int length;
};
typedef _strrec* _pstrrec;


#define STR_BASE(x)      (_pstrrec(x)-1)
#define STR_REFCOUNT(x)  (STR_BASE(x)->refcount)
#define STR_LENGTH(x)    (STR_BASE(x)->length)

#define PTR_TO_PSTRING(p)   (pstring(&(p)))
#define PTR_TO_STRING(p)    (*PTR_TO_PSTRING(p))


extern char* emptystr;

class string 
{
protected:
    char* data;

    static void idxerror();

    void _alloc(int);
    void _realloc(int);
    void _free();

    void initialize()  { data = emptystr; }
    void initialize(const char*, int);
    void initialize(const char*);
    void initialize(char);
    void initialize(const string& s);
    void initialize(const char*, int, const char*, int);
    void finalize();

#ifdef CHECK_BOUNDS
    void idx(int index) const  { if (unsigned(index) >= unsigned(STR_LENGTH(data))) idxerror(); }
#else
    void idx(int) const        { }
#endif

    string(const char* s1, int len1, const char* s2, int len2)  { initialize(s1, len1, s2, len2); }

public:
    string()                                      { initialize(); }
    string(const char* sc, int initlen)           { initialize(sc, initlen); }
    string(const char* sc)                        { initialize(sc); }
    string(char c)                                { initialize(c); }
    string(const string& s)                       { initialize(s); }
    ~string()                                     { finalize(); }

    void assign(const char*, int);
    void assign(const char*);
    void assign(const string&);
    void assign(char);
    string& operator= (const char* sc)            { assign(sc); return *this; }
    string& operator= (char c)                    { assign(c); return *this; }
    string& operator= (const string& s)           { assign(s); return *this; }

    int    size()                                 { return STR_LENGTH(data); }
    int    length()                               { return STR_LENGTH(data); }
    int    refcount()                             { return STR_REFCOUNT(data); }
    void   clear()                                { finalize(); }
    bool   empty()                                { return STR_LENGTH(data) == 0; }

    char*  resize(int);
    char*  unique();
    string dup() const                            { return string(data); }
    char&  operator[] (int i)                     { idx(i); return unique()[i]; }
    const char& operator[] (int i) const          { idx(i); return data[i]; }
    operator const char*() const                  { return data; }
    operator const uchar*() const                 { return (uchar*)data; }
    const char* c_str() const                     { return data; }

    void append(const char* sc, int catlen);
    void append(const char* s);
    void append(char c);
    void append(const string& s);
    string& operator+= (const char* sc)           { append(sc); return *this; }
    string& operator+= (char c)                   { append(c); return *this; }
    string& operator+= (const string& s)          { append(s); return *this; }

    string copy(int from, int cnt) const;
    string operator+ (const char* sc) const;
    string operator+ (char c) const;
    string operator+ (const string& s) const;
    friend string operator+ (const char* sc, const string& s);
    friend string operator+ (char c, const string& s);

    bool operator== (const char* sc) const        { return strcmp(data, sc) == 0; }
    bool operator== (char) const;
    bool operator== (const string&) const;
    bool operator!= (const char* sc) const        { return !(*this == sc); }
    bool operator!= (char c) const                { return !(*this == c); }
    bool operator!= (const string& s) const       { return !(*this == s); }
    
    bool operator < (const string& s) const       { return strcmp(data, s) < 0; }

    friend bool operator== (const char*, const string&);
    friend bool operator== (char, const string&);
    friend bool operator!= (const char*, const string&);
    friend bool operator!= (char, const string&);
};


string operator+ (const char* sc, const string& s);
string operator+ (char c, const string& s);
inline bool operator== (const char* sc, const string& s) { return s == sc; }
inline bool operator== (char c, const string& s)         { return s == c; }
inline bool operator!= (const char* sc, const string& s) { return s != sc; }
inline bool operator!= (char c, const string& s)         { return s != c; }

inline int hstrlen(const char* p) // some Unix systems do not accept NULL
    { return p == NULL ? 0 : (int)strlen(p); }


extern int stralloc;

extern string nullstring;

typedef string* pstring;


string itostring(large value, int base, int width = 0, char padchar = 0);
string itostring(ularge value, int base, int width = 0, char padchar = 0);
string itostring(int value, int base, int width = 0, char padchar = 0);
string itostring(unsigned value, int base, int width = 0, char padchar = 0);
string itostring(large v);
string itostring(ularge v);
string itostring(int v);
string itostring(unsigned v);

ularge stringtou(const char* str, bool* error, bool* overflow, int base = 10);


#endif
