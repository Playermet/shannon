
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "charset.h"
#include "str.h"
#include "baseobj.h"


// ------------------------------------------------------------------------ //
// --- TEXT FILE READER --------------------------------------------------- //
// ------------------------------------------------------------------------ //

#define INTEXT_BUFSIZE 8192
#define TAB_SIZE 8

class charset;

class InText
{
    string filename;
    int  fd;
    char buf[INTEXT_BUFSIZE];
    int  bufsize;
    int  bufpos;
    int  linenum;
    int  indent;
    bool newline;
    bool eof;
    
    void error(int code) throw(ESysError);
    void validateBuffer();
    void doSkipEol();
    void token(const charset& chars, string& result, bool skip);

public:
    InText(const string& filename);
    ~InText();
    
    char preview();
    char get();
    void skipEol();
    string token(const charset& chars) throw(ESysError);
    void skip(const charset& chars) throw(ESysError);
};


InText::InText(const string& ifilename)
    : filename(ifilename), fd(0), bufsize(0), bufpos(0), linenum(0),
      indent(0), newline(true), eof(false)
{
}


InText::~InText()
{
    if (fd >= 0)
    {
        close(fd);
        eof = true;
        fd = -1;
    }
}


void InText::error(int code) throw(ESysError)
{
    eof = true;
    throw ESysError(code);
}


void InText::validateBuffer()
{
    if (!eof && fd < 0)
    {
        fd = open(filename.c_str(), O_RDONLY);
        if (fd < 0)
            error(errno);
    }
    if (!eof && bufpos == bufsize)
    {
        int result = read(fd, buf, INTEXT_BUFSIZE);
        if (result < 0)
            error(errno);
        bufpos = 0;
        bufsize = result;
        eof = result == 0;
    }
}


char InText::preview()
{
    if (bufpos == bufsize)
        validateBuffer();
    if (eof)
        return 0;
    return buf[bufpos];
}


char InText::get()
{
    if (bufpos == bufsize)
        validateBuffer();
    if (eof)
        return 0;
    assert(bufpos < bufsize);
    return buf[bufpos++];
}


void InText::doSkipEol()
{
    newline = true;
    linenum++;
    indent = 0;
}


void InText::skipEol()
{
    char c = preview();
    if (c == '\r')
    {
        get();
        c = preview();
    }
    if (c == '\n')
    {
        get();
        doSkipEol();
    }
}


void InText::token(const charset& chars, string& result, bool noresult)
{
    do
    {
        if (bufpos == bufsize)
            validateBuffer();
        if (eof)
            return;
        assert(bufpos < bufsize);
        const char* b = buf + bufpos;
        register const char* p = b;
        register const char* e = buf + bufsize;
        while (p < e && chars[*p])
        {
            switch (*p)
            {
                case '\t': if (newline) indent = ((indent / TAB_SIZE) + 1) * TAB_SIZE; break;
                case '\n': doSkipEol(); break;
                case ' ': if (newline) indent++; break;
                default: newline = false; break; // freeze indent calculation
            }
            p++;
        }
        if (!noresult && p > b)
            result.append(b, p - b);
    }
    while (bufpos == bufsize);
}


string InText::token(const charset& chars) throw(ESysError)
{
    string result;
    token(chars, result, false);
    return result;
}


void InText::skip(const charset& chars) throw(ESysError)
{
    string result;
    token(chars, result, true);
}


// ------------------------------------------------------------------------ //
// --- FIFO --------------------------------------------------------------- //
// ------------------------------------------------------------------------ //


template <class T>
class Array: public PodArray<T>
{
protected:
    bool makeunique()
    {
        if (!string::empty() && string::refcount() > 1)
        {
            PodArray<T> old = *this;
            PodArray<T>::_alloc(old.bytesize());
            for (int i = 0; i < PodArray<T>::size(); i++)
                PodArray<T>::operator[] (i) = old[i];
            PodArray<T>::finalize(old);
            return true;
        }
        return false;
    }


public:

    void clear()
    {
        if (makeunique())
            ;
        PodArray<T>::clear();
    }
};


#define FIFO_CHUNK_COUNT 16
#define FIFO_CHUNK_SIZE (sizeof(quant) * FIFO_CHUNK_COUNT)

typedef char FifoChunk[FIFO_CHUNK_SIZE];


class fifoimpl: private PodArray<quant>
// A really dirty trick: the first element is the shift (int) the rest is a 
// dynamic list of pointers to chunks. We are trying to save memory and 
// allocations.
{
protected:
    int shift()  { return operator[] (0).ord; }

public:
    fifoimpl();
    ~fifoimpl();
};


fifoimpl::fifoimpl()
    : PodArray<quant>()  { PodArray<quant>::add().ord = 0; }
fifoimpl::~fifoimpl()  { }




int main()
{
    Array<int> a;
    a.add(1);
    a.add(2);
    a.clear();
    return 0;
}

