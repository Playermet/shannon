
#define __STDC_LIMIT_MACROS


#include "common.h"
#include "runtime.h"
#include "source.h"
#include "typesys.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>


#define fail(e) \
    (printf("%s:%u: test failed `%s'\n", __FILE__, __LINE__, e), exit(200))

#define check(e) \
    { if (!(e)) fail(#e); }

#define check_throw(a) \
    { bool chk_throw = false; try { a; } catch(std::exception&) { chk_throw = true; } check(chk_throw); }

#define check_nothrow(a) \
    { try { a; } catch(...) { fail("exception thrown"); } }

#define XSTR(s) _STR(s)
#define _STR(s) #s

#ifdef SH64
#  define INTEGER_MAX_STR "9223372036854775807"
#  define INTEGER_MAX_STR_PLUS "9223372036854775808"
#  define INTEGER_MIN_STR "-9223372036854775808"
#else
#  define INTEGER_MAX_STR "2147483647"
#  define INTEGER_MAX_STR_PLUS "2147483648"
#  define INTEGER_MIN_STR "-2147483648"
#endif

void test_common()
{
    // make sure the assembly code for atomic ops is sane
    int i = 1;
    check(pincrement(&i) == 2);
    check(pdecrement(&i) == 1);

    // string conversion
    check(to_string(integer(0)) == "0");
    check(to_string(integer(-1)) == "-1");
    check(to_string(INTEGER_MAX) == INTEGER_MAX_STR);
    check(to_string(INTEGER_MIN) == INTEGER_MIN_STR);
    check(to_string(1, 10, 4, '0') == "0001");
    check(to_string(integer(123456789)) == "123456789");
    check(to_string(-123, 10, 7, '0') == "-000123");
    check(to_string(0xabcde, 16, 6) == "0ABCDE");
    
    bool e = true, o = true;
    check(from_string("0", &e, &o) == 0);
    check(!o); check(!e);
    check(from_string(INTEGER_MAX_STR, &e, &o) == INTEGER_MAX);
    check(from_string(INTEGER_MAX_STR_PLUS, &e, &o) == uinteger(INTEGER_MAX) + 1);
    check(from_string("92233720368547758070", &e, &o) == 0); check(o);
    check(from_string("-1", &e, &o) == 0 ); check(e);
    check(from_string("89abcdef", &e, &o, 16) == 0x89abcdef);
    check(from_string("afg", &e, &o, 16) == 0); check(e);
    
    // syserror
    check(str(esyserr(2, "arg").what()) == "No such file or directory (arg)");
    check(str(esyserr(2).what()) == "No such file or directory");
}


class test_obj: public object
{
public:
    void dump(fifo_intf& s) const
        { s << "test_obj"; }
    test_obj(): object(NULL)  { }
};


void test_variant()
{
    int save_alloc = object::alloc;
    {
        variant v1 = null;              check(v1.is_null());
        variant v2 = 0;                 check(v2.is(variant::INT));     check(v2.as_int() == 0);
        variant v3 = 1;                 check(v3.as_int() == 1);
        variant v4 = INTEGER_MAX;       check(v4.as_int() == INTEGER_MAX);
        variant v5 = INTEGER_MIN;       check(v5.as_int() == INTEGER_MIN);
        variant v6 = 1.1;               check(v6.as_real() == real(1.1));
        variant v7 = false;             check(!v7.as_bool());
        variant v8 = true;              check(v8.as_bool());
        variant v12 = 'x';              check(v12.as_char() == 'x');
        variant v9 = "";                check(v9.as_str().empty());
        variant v10 = "abc";            check(v10.as_str() == "abc");
        str s1 = "def";
        variant vst = s1;               check(vst.as_str() == s1);
        object* o = new test_obj();
        variant vo = o;                 check(vo.is_object());  check(vo.as_object() == o);

        check_throw(v1.as_int());
        check_throw(v1.as_real());
        check_throw(v1.as_bool());
        check_throw(v1.as_char());
        check_throw(v1.as_str());
        check_throw(v1.as_object());
        
        check(v1.to_string() == "null");
        check(v2.to_string() == "0");
        check(v3.to_string() == "1");
        check(v4.to_string() == INTEGER_MAX_STR);
        check(v5.to_string() == INTEGER_MIN_STR);
        // check(v6.to_string() == "1.1");
        check(v7.to_string() == "false");
        check(v8.to_string() == "true");
        check(v9.to_string() == "''");
        check(v10.to_string() == "'abc'");
        check(vst.to_string() == "'def'");
        check(v12.to_string() == "'x'");
        check(vo.to_string() == "[test_obj]");

        check(v1 < v2); check(!(v2 < v1));
        check(v2 < v3);
        check(v3 != v6); check(v3 < v6); check(!(v6 < v3));
        
        variant v;
        check(v == null);
        v = 0;                 check(v.as_int() == 0);              check(v == 0);
        check(v != null);      check(v != true);                    check(v != "abc");
        v = 1;                 check(v.as_int() == 1);              check(v == 1);
        v = INTEGER_MAX;       check(v.as_int() == INTEGER_MAX);    check(v == INTEGER_MAX);
        v = INTEGER_MIN;       check(v.as_int() == INTEGER_MIN);    check(v == INTEGER_MIN);
        v = 1.1;               check(v.as_real() == real(1.1));     check(v == 1.1);
        v = false;             check(!v.as_bool());                 check(v == false);
        v = true;              check(v.as_bool());                  check(v == true);
        v = 'x';               check(v.as_char() == 'x');           check(v == 'x');    check(v != 'z');
        v = "";                check(v.as_str().empty());           check(v == "");
        v = "abc";             check(v.as_str() == "abc");          check(v == "abc");
        v = s1;                check(v.as_str() == s1);             check(v == s1);
        v = o;                 check(v.as_object() == o);
        check(v != null);
        v = null;
        check(!v.is(variant::OBJECT));
        check(v == null);

/*
        // str
        vst = "";
        check(vst.empty()); check(vst.size() == 0);
        vst.append("abc");
        vst.append("");
        vst.append(s1);
        vst.append("ghijkl");
        check(vst.as_str() == "abcdefghijkl");
        check(vst.substr(5, 3) == "fgh");
        check(vst.substr(5) == "fghijkl");
        vst.erase(1);
        check(vst.as_str() == "acdefghijkl");
        vst.erase(3, 4);
        check(vst.as_str() == "acdijkl");
        check(vst.getch(2) == 'd');
        vst.resize(10, '-');
        check(vst.as_str() == "acdijkl---");
        check(vst.size() == 10);
        vst.resize(5, '-');
        check(vst.as_str() == "acdij");

        // range
        vr = new_range();
        check(vr.empty());
        vr = new_range(-1, 1);
        check(vr.left() == -1 && vr.right() == 1);
        check(vr.has(0));
        check(!vr.has(-2));
        vr = new_range(5, 2);
        check(vr.left() == 5 && vr.right() == 2);
        check(!vr.has(-2));
        variant vr4 = 0;
        vr4.assign(0, 5);
        check(vr4.left() == 0 && vr4.right() == 5);
        check_throw(vr4.has("abc"));
        variant vra = vr;
        check(vra == vr);

        // vector
        check(vt.empty()); check(vt.size() == 0);
        check_throw(vt.insert(1, 0));
        vt.push_back(0);
        vt.push_back("abc");
        vt.insert(vt.size(), 'd');
        vt.push_back(vs);
        vt.insert(1, true);
        check(vt.to_string() == "[0, true, \"abc\", 'd', []]");
        check(!vt.empty()); check(vt.size() == 5);
        check_throw(vt.insert(10, 0));
        check_throw(vt.insert(mem(-1), 0));
        vt.erase(2);
        check(vt.to_string() == "[0, true, 'd', []]");
        check(vt[2] == 'd');
        vt.put(2, "asd");
        check(vt.to_string() == "[0, true, \"asd\", []]");
        check_throw(vt.put(4, 0));
        vt.resize(2);
        check(vt.to_string() == "[0, true]");
        vt.resize(4);
        check(vt.to_string() == "[0, true, null, null]");
        vt.put(2, "abc");
        vt.put(3, new_range(1, 2));
        check(vt.to_string() == "[0, true, \"abc\", [1..2]]");
        check(vt.size() == 4);
        variant vta = vt;
        check(vta == vt);
        vt.put(0, 100);
        check(!vt.is_unique()); check(!vta.is_unique());
        check(vt.to_string() == "[100, true, \"abc\", [1..2]]");
        check(vta.to_string() == "[100, true, \"abc\", [1..2]]");
        vta.unique();
        check(vt.is_unique()); check(vta.is_unique());
        check(vta.to_string() == "[100, true, \"abc\", [1..2]]");
        vta.put(2, "def");
        check(vt.to_string() == "[100, true, \"abc\", [1..2]]");
        check(vta.to_string() == "[100, true, \"def\", [1..2]]");

        // dict
        check(vd.empty()); check(vd.size() == 0);
        vd.tie("k1", "abc");
        vd.tie("k2", true);
        vd.tie("k3", new_set());
        check(vd.to_string() == "[\"k1\": \"abc\", \"k2\": true, \"k3\": []]");
        check(!vd.empty()); check(vd.size() == 3);
        vd.tie("k2", null);
        check(vd.to_string() == "[\"k1\": \"abc\", \"k3\": []]");
        vd.tie("kz", null);
        check(vd.to_string() == "[\"k1\": \"abc\", \"k3\": []]");
        str_fifo vds;
        vforeach(dict, i, vd)
            vds << ' ' << i->first << ": " << i->second;
        check(vds.all() == " \"k1\": \"abc\" \"k3\": []");
        vd.untie("k2");
        check(vd.to_string() == "[\"k1\": \"abc\", \"k3\": []]");
        vd.untie("k3");
        check(vd.to_string() == "[\"k1\": \"abc\"]");
        check(vd["k1"] == "abc");
        check(vd["k2"] == null);
        check(vd["kz"] == null);
        check(vd.has("k1"));
        check(!vd.has("k2"));
        
        variant vda = vd;
        check(vda == vd);
        vda.tie(1, true);
        check(vd.to_string() == "[1: true, \"k1\": \"abc\"]");
        check(vda.to_string() == "[1: true, \"k1\": \"abc\"]");
        vda.unique();
        vda.untie("k1");
        check(vd.to_string() == "[1: true, \"k1\": \"abc\"]");
        check(vda.to_string() == "[1: true]");
        check(vd.is_unique());  check(vd.is_unique());

        variant vdb = new_dict();
        variant vdc = new_dict();
        vdb.tie(1, 2);
        vd.tie(vdb, 100);
        vd.tie(vdc, 101);
        // non-deterministic, depends on addresses: check(vd.to_string() == "[[1: 2]: false, []: true]");
        check(vd[vdb] == 100);
        check(vd[vdc] == 101);

        vd = new_dict();
        vd.tie(new_dict(), 6);
        vd.tie("abc", 5);
        // vd.tie(1.1, 4);
        vd.tie(10, 3);
        vd.tie('a', 2);
        vd.tie(false, 1);
        vd.tie(null, 0);
        check(vd.to_string() == "[null: 0, false: 1, 'a': 2, 10: 3, \"abc\": 5, []: 6]");
        
        // dict[range]
        vd = new_dict();
        vd.tie(new_range(0, 4), new_range(0, 1));
        vd.tie(new_range(5, 6), "abc");
        check(vd.has(new_range(5, 6)));
        check(!vd.has(new_range()));
        check(!vd.has(new_range(0, 6)));
        
        // ordset
        check(vos.empty()); check_throw(vos.size());
        vos.tie(5);
        vos.tie(131);
        vos.tie(255);
        check_throw(vos.tie(256));
        check_throw(vos.tie(1000));
        check_throw(vos.tie(-1));
        check_throw(vos.untie(1000));
        check_throw(vos.untie(-1));
        check_throw(vos.tie("abc"));
        check(vos.to_string() == "[5, 131, 255]");
        check(!vos.empty());
        vos.untie(131);
        vos.untie(30);
        check(vos.to_string() == "[5, 255]");
        check(vos.has(5));
        check(vos.has(char(255)));
        check(!vos.has(26));
        variant vosa = vos;
        check(vosa == vos);
        check(vosa.to_string() == "[5, 255]");
        check(!vosa.is_unique()); check(!vos.is_unique()); 
        vosa.tie(100);
        check(vos.to_string() == "[5, 100, 255]");
        check(vosa.to_string() == "[5, 100, 255]");
        vosa.unique();
        vosa.untie(255);
        check(vos.is_unique()); check(vos.to_string() == "[5, 100, 255]");
        check(vos.is_unique()); check(vosa.to_string() == "[5, 100]");

        // set
        check(vs.empty()); check_throw(vs.size());
        vs.tie(5);
        vs.tie(26);
        vs.tie(127);
        check(vs.to_string() == "[5, 26, 127]");
        str_fifo vdss;
        vforeach(set, i, vs)
            vdss << ' ' << *i;
        check(vdss.all() == " 5 26 127");
        check(!vs.empty());
        vs.untie(26);
        vs.untie(226);
        check(vs.to_string() == "[5, 127]");
        check(vs.has(5));
        check(vs.has(127));
        check(!vs.has(26));
        
        vs.tie("abc");
        vs.tie("abc");
        vs.tie("def");
        check(vs.to_string() == "[5, 127, \"abc\", \"def\"]")
        check(vs.has("abc"));
        // vs.tie(1.1);
        // vs.tie(2.2);
        vs.tie(true);
        vs.tie(null);
        check(vs.to_string() == "[null, true, 5, 127, \"abc\", \"def\"]")
        // check(vs.has(1.1));

        str_fifo ss;
        ss << vs;
        check(ss.all() == "[null, true, 5, 127, \"abc\", \"def\"]");
        
        // ref counting & cloning
        vs = new_set();
        vs.tie(0);
        check(vs.as_set().is_unique());
        variant vss = vs;
        check(vss == vs);
        check(vss.has(0));
        check(!vs.as_set().is_unique());
        check(!vss.as_set().is_unique());
        check(vs.to_string() == "[0]");
        check(vss.to_string() == "[0]");
        vs.tie(1);
        check(vs.to_string() == "[0, 1]");
        check(vss.to_string() == "[0, 1]");
        variant vsa = vs;
        vsa.unique();
        vsa.tie(2);
        check(vsa.to_string() == "[0, 1, 2]");
        check(vs.to_string() == "[0, 1]");
*/        
        variant voa = vo;
        check_throw(voa.unique());
        
        // object
        // vo = vo; // this doesn't work at the moment
        // cout << vo.as_object() << endl;
    
        int save_alloc2 = object::alloc;
        objptr<test_obj> p1 = new test_obj();
        check(object::alloc == save_alloc2 + 1);
        check(p1->is_unique());
        objptr<test_obj> p2 = p1;
        check(!p1->is_unique());
        check(!p2->is_unique());
        check(p1 == p2);
        objptr<test_obj> p3 = new test_obj();
        check(p1 != p3);
        p3 = p3; // see if it doesn't core dump
    }
    check(object::alloc == save_alloc);
}


void test_symbols()
{
    int save_alloc = object::alloc;
    {
        objptr<Base> s = new Base(Base::THISVAR, NULL, "sym", 0);
        check(s->name == "sym");
        BaseTable<Base> t;
        check(t.empty());
        objptr<Base> s1 = new Base(Base::THISVAR, NULL, "abc", 0);
        check(s1->is_unique());
        t.addUnique(s1);
        check(s1->is_unique());
        objptr<Base> s2 = new Base(Base::THISVAR, NULL, "def", 0);
        t.addUnique(s2);
        objptr<Base> s3 = new Base(Base::THISVAR, NULL, "abc", 0);
        check_throw(t.addUnique(s3));
        check(t.find("abc") == s1);
        check(s2 == t.find("def"));
        check(t.find("xyz") == NULL);
        check(!t.empty());

        List<Base> l;
        check(l.size() == 0);
        check(l.empty());
        l.add(s1);
        check(!s1->is_unique());
        l.add(new Base(Base::THISVAR, NULL, "ghi", 0));
        check(l.size() == 2);
        check(!l.empty());
    }
    check(object::alloc == save_alloc);
}


void test_source()
{
    {
        in_text file(NULL, "nonexistent");
        check_throw(file.open());
        check_throw(file.get());
    }
    {
        str_fifo m(NULL, "one\t two\n567");
        check(m.preview() == 'o');
        check(m.get() == 'o');
        check(m.token(identRest) == "ne");
        m.skip(wsChars);
        check(m.token(identRest) == "two");
        check(m.eol());
        check(!m.eof());
        m.skip_eol();
        check(m.token(digits) == "567");
        check(m.eol());
        check(m.eof());
    }
    {
#ifdef XCODE
        const char* fn = "../../src/tests/parser.txt";
#else
        const char* fn = "tests/parser.txt";
#endif
        Parser p(fn, new in_text(NULL, fn));
        static Token expect[] = {
            tokIdent, tokComma, tokSep,
            tokIndent, tokModule, tokSep,
            tokIndent, tokIdent, tokIdent, tokIdent, tokSep,
            tokIdent, tokIdent, tokConst, tokPeriod, tokSep,
            tokBlockEnd,
            tokIntValue, tokSep,
            tokIndent, tokStrValue, tokSep,
            tokIdent, tokComma, tokSep,
            tokIdent, tokSep,
            tokBlockEnd, tokBlockEnd,
            tokIdent, tokBlockBegin, tokIdent, tokBlockEnd,
            tokIdent, tokBlockBegin, tokIdent, tokSep, 
            tokIdent, tokSep, tokBlockEnd,
            tokBlockEnd,
            tokEof
        };
        int i = 0;
        while (p.next() != tokEof && expect[i] != tokEof)
        {
            check(expect[i] == p.token);
            switch (i)
            {
            case 0: check(p.strValue == "Thunder") break;
            case 1: check(p.strValue == ","); break;
            case 17: check(p.intValue == 42); break;
            case 20: check(p.strValue == "Thunder, 'thunder',\tthunder, Thundercats"); break;
            }
            i++;
        }
        check(expect[i] == tokEof && p.token == tokEof);
    }
    {
        Parser p("mem", new str_fifo(NULL,
            INTEGER_MAX_STR"\n  "INTEGER_MAX_STR_PLUS"\n  if\n aaa"
            " 'asd\n'[\\t\\r\\n\\x41\\\\]' '\\xz'"));
        check(p.next() == tokIntValue);
        check(p.intValue == INTEGER_MAX);
        check(p.next() == tokSep);
        check(p.getLineNum() == 2);
        check(p.getIndent() == 2);
        check(p.next() == tokIndent);
        check_throw(p.next()); // integer overflow
        check(p.next() == tokSep);
        check(p.getLineNum() == 3);
        check(p.next() == tokIf);
        check(p.next() == tokSep);
        check(p.getLineNum() == 4);
        check_throw(p.next()); // unmatched unindent
        check(p.next() == tokIndent);
        check(p.next() == tokIdent);
        check_throw(p.next()); // unexpected end of line
        check(p.next() == tokStrValue);
        check(p.strValue == "[\t\r\nA\\]");
        check_throw(p.next()); // bad hex sequence
    }
}


void test_bidir_char_fifo(fifo_intf& fc)
{
    check(fc.is_char_fifo());
    fc.enq("0123456789abcdefghijklmnopqrstuvwxy");
    fc.var_enq(variant('z'));
    fc.var_enq(variant("./"));
    check(fc.preview() == '0');
    check(fc.get() == '0');
    variant v;
    fc.var_deq(v);
    check(v.as_char() == '1');
    v.clear();
    fc.var_preview(v);
    check(v.as_char() == '2');
    check(fc.deq(16) == "23456789abcdefgh");
    check(fc.deq(fifo::CHAR_ALL) == "ijklmnopqrstuvwxyz./");
    check(fc.empty());

    fc.enq("0123456789");
    fc.enq("abcdefghijklmnopqrstuvwxyz");
    check(fc.get() == '0');
    while (!fc.empty())
        fc.deq(fifo_intf::CHAR_SOME);

    fc.enq("0123456789abcdefghijklmnopqrstuvwxyz");
    check(fc.deq("0-9") == "0123456789");
    check(fc.deq("a-z") == "abcdefghijklmnopqrstuvwxyz");
    check(fc.empty());
}


void test_fifos()
{
#ifdef DEBUG
    fifo::CHUNK_SIZE = 2 * sizeof(variant);
#endif

    fifo f(NULL, false);
    objptr<vector> t = new vector(NULL);
    t->push_back(0);
    f.var_enq((vector*)t);
    f.var_enq("abc");
    f.var_enq("def");
    variant w = new range(NULL, 1, 2);
    f.var_enq(w);
    // f.dump(std::cout); std::cout << std::endl;
    variant x;
    f.var_deq(x);
    check(x.is_object());
    f.var_deq(w);
    check(w.is(variant::STR));
    f.var_eat();
    variant vr;
    f.var_preview(vr);
    check(vr.is_object());

    fifo fc(NULL, true);
    test_bidir_char_fifo(fc);
    
    str_fifo fs(NULL);
    test_bidir_char_fifo(fs);
}


void test_typesys()
{
    initTypeSys();
    check(defTypeRef->isTypeRef());
    check(defTypeRef->get_rt() == defTypeRef);
    check(queenBee->defNone->isNone());
    check(queenBee->defNone->get_rt() == defTypeRef);
    check(queenBee->defInt->isInt());
    check(queenBee->defInt->get_rt() == defTypeRef);
    check(queenBee->defInt->isOrdinal());
    check(queenBee->defBool->isBool());
    check(queenBee->defBool->get_rt() == defTypeRef);
    check(queenBee->defBool->isEnum());
    check(queenBee->defBool->isOrdinal());
    check(queenBee->defBool->rangeEq(0, 1));
    check(queenBee->defChar->isChar());
    check(queenBee->defChar->get_rt() == defTypeRef);
    check(queenBee->defChar->isOrdinal());
    check(queenBee->defStr->isString());
    check(queenBee->defStr->get_rt() == defTypeRef);
    check(queenBee->defStr->isContainer());
    check(queenBee->defChar->deriveVector() == queenBee->defStr);

    Base* b = queenBee->deepFind("true");
    check(b != NULL && b->isDefinition());
    check(PConst(b)->value.as_int() == 1);
    check(PConst(b)->type->isBool());

    {
        State state(queenBee, NULL, queenBee->defBool);
        b = state.deepFind("true");
        check(b != NULL && b->isDefinition());
        check(state.deepFind("untrue") == NULL);
        state.addThisVar(queenBee->defInt, "a");
        check_throw(state.addThisVar(queenBee->defInt, "a"));
        check_nothrow(state.addThisVar(queenBee->defInt, "true"));
        state.addTypeAlias(queenBee->defBool, "ool");
        b = state.deepFind("ool");
        check(b != NULL && b->isDefinition());
        check(PConst(b)->isTypeAlias());
        check(PConst(b)->getAlias()->isBool());
        check_nothrow(state.addThisVar(queenBee->defInt, "v"));
        Base* v = state.deepFind("v");
        check(v->isThisVar());
        check(v->id == 2);
        check_nothrow(state.addThisVar(queenBee->defChar, "c1"));
        check_nothrow(state.addThisVar(queenBee->defChar, "c2"));
        v = state.deepFind("c2");
        check(v->isThisVar());
        check(v->id == 4);
        b = state.deepFind("result");
        check(b != NULL);
        check(b->isResultVar());
        check(PVar(b)->type->isBool());
    }

    doneTypeSys();
}


int main()
{
    sio << "short: " << sizeof(short) << "  long: " << sizeof(long)
         << "  long long: " << sizeof(long long) << "  int: " << sizeof(int)
         << "  void*: " << sizeof(void*) << "  float: " << sizeof(float)
         << "  double: " << sizeof(double) << '\n';
    sio << "integer: " << sizeof(integer) << "  mem: " << sizeof(mem)
         << "  real: " << sizeof(real) << "  variant: " << sizeof(variant)
         << "  object: " << sizeof(object) << '\n';

    check(sizeof(int) == 4);
    check(sizeof(mem) >= 4);

#ifdef SH64
    check(sizeof(integer) == 8);
#  ifdef PTR64
    check(sizeof(variant) == 16);
#  else
    check(sizeof(variant) == 12);
#  endif
#else
    check(sizeof(integer) == 4);
    check(sizeof(variant) == 8);
#endif

    try
    {
        test_common();
        test_variant();
        test_symbols();
        test_source();
        test_fifos();
        test_typesys();
    }
    catch (std::exception& e)
    {
        fail(e.what());
    }

    check(object::alloc == 0);

    return 0;
}

