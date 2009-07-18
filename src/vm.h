#ifndef __VM_H
#define __VM_H


#include "common.h"
#include "runtime.h"
#include "typesys.h"

#include <stack>


enum OpCode
{
    opInv,  // to detect corrupt code segments
    opEnd,
    opNop,
    
    // Const loaders
    opLoadNull,
    opLoadFalse,
    opLoadTrue,
    opLoadChar,         // [8]
    opLoad0,
    opLoad1,
    opLoadInt,          // [int]
    opLoadNullStr,
    opLoadNullRange,
    opLoadNullVec,
    opLoadNullDict,
    opLoadNullOrdset,
    opLoadNullSet,
    opLoadConst,        // [const-index: 8] // compound values only
    opLoadConst2,       // [const-index: 16] // compound values only
    opLoadTypeRef,      // [Type*]
    
    opPop,              // -var
    opSwap,

    // Safe typecasts
    opToBool,
    opToStr,
    opToType,           // [Type*] -var, +var
    opToTypeRef,        // -type, -var, +var
    opIsType,           // [Type*] -var, +bool
    opIsTypeRef,        // -type, -var, +bool

    // Arithmetic binary: -ord, -ord, +ord
    opAdd, opSub, opMul, opDiv, opMod, opBitAnd, opBitOr, opBitXor, opBitShl, opBitShr,
    // Arithmetic unary: -ord, +ord
    opNeg, opBitNot, opNot,

    // Vector/string concatenation
    opCharToStr,        // -char, +str
    opCharCat,          // -char, -str, +str
    opStrCat,           // -str, -str, +str
    opVarToVec,         // [Vector*] -var, +vec
    opVarCat,           // -var, -vec, +vec
    opVecCat,           // -var, -vec, +vec

    // Range operations (work for all ordinals)
    opMkRange,          // [Ordinal*] -right-int, -left-int, +range
    opInRange,          // -range, -int, +{0,1}

    // Comparators
    opCmpOrd,           // -ord, -ord, +{-1,0,1}
    opCmpStr,           // -str, -str, +{-1,0,1}
    opCmpVar,           // -var, -var, +{0,1}

    // Compare the stack top with 0 and replace it with a bool value.
    // The order of these opcodes is in sync with tokens tokEqual..tokGreaterEq
    opEqual, opNotEq, opLessThan, opLessEq, opGreaterThan, opGreaterEq,
    
    // Loaders: each of these can be replaced by a corresponding storer if
    // the object turns out to be a L-value.
    opLoadRet,          // +var
    opLoadLocal,        // [stack-index: signed-8 (args(-N), temp(N))]
    opLoadThis,         // [var-index: 8]
    opLoadOuter,        // [level: 8, var-index: 8]
    opLoadStatic,       // [module: 8, var-index: 8]
    opLoadStrElem,      // -str, -index, +char
    opLoadVecElem,      // -vector, -index, +val
    opLoadDictElem,     // -dict, -key, +val
    opLoadMember,       // [var-index: 8] -obj, +val

    // Storers
    opStoreRet,         // -var
    opStoreLocal,       // [stack-index: signed-8]
    opStoreThis,        // [var-index: 8]
    opStoreOuter,       // [level: 8, var-index: 8]
    opStoreStatic,      // [module: 8, var-index: 8]
    opStoreStrElem,     // -index, -char, -str
    opStoreVecElem,     // -index, -val, -vector
    opStoreDictElem,    // -key, -val, -dict
    opStoreMember,      // [var-index: 8] -val, -obj

    // Case labels: cmp the top element with the arg and leave 0 or 1 for
    // further boolean jump
    opCase,             // -var, +{0,1}
    opCaseRange,        // -int, +{0,1}
    
    // Jumps; [dst] is a relative offset -128..127
    //   short bool evaluation: pop if jump, leave it otherwise
    // TODO: 16-bit versions of these
    opJumpOr, opJumpAnd,                // [dst 8] (-)bool
    opJumpTrue, opJumpFalse, opJump,    // [dst 8]

    // Function call
    opCall,             // [Type*]

    // Helpers
    opEcho, opEchoLn,
    opAssert,           // [line-num: 16]
    opLinenum,          // [line-num: 16]
    
    opMaxCode
};


inline bool isLoadOp(OpCode op)
    { return (op >= opLoadNull && op <= opLoadTypeRef)
        || (op >= opLoadLocal && op <= opLoadMember); }

inline bool isCmpOp(OpCode op)
    { return op >= opEqual && op <= opGreaterEq; }


// --- CODE SEGMENT ------------------------------------------------------- //

class Context;


class CodeSeg: noncopyable
{
    friend class CodeGen;
    friend class StateBody;

    // This object can be duplicated if necessary with a different context
    // assigned; the code segment is a refcounted string, so copying would
    // be efficient.
protected:
    str code;
    varlist consts;
    mem stksize;
    // These can't be refcounted as it will introduce circular references. Both
    // can be NULL if this is a const expression executed at compile time.
    State* state;
    Context* context;
#ifdef DEBUG
    bool closed;
#endif

    // Code generation
    void add8(uint8_t i);
    void add16(uint16_t i);
    void addInt(integer i);
    void addPtr(void* p);
    void close(mem _stksize);
    void resize(mem s)
        { code.resize(s); }

    // Execution
    static void varToVec(Vector* type, const variant& elem, variant* result);
    static void varCat(const variant& elem, variant* vec);
    static void vecCat(const variant& vec2, variant* vec1);

    void run(langobj* self, varstack& stack, variant* result) const;

public:
    CodeSeg(State*, Context*);
    ~CodeSeg();

    // For unit tests:
    void clear();
    bool empty() const
        { return code.empty(); }
    mem size() const
        { return code.size(); }
};


class ConstCode: public CodeSeg
{
public:
    ConstCode(): CodeSeg(NULL, NULL) { }
    void run(variant&) const;
};


class StateBody: public object, public CodeSeg
{
    friend class Context;
protected:
    void finalize(langobj* self, varstack& stack)
        { final.run(self, stack, NULL); }
public:
    CodeSeg final;

    StateBody(State*, Context*);
    ~StateBody();
};


class Context: protected BaseTable<ModuleAlias>
{
protected:
    List<Module> types;
    List<StateBody> bodies;
    List<ModuleAlias> defs;
    List<langobj> datasegs;

    ModuleAlias* registerModule(const str& name, Module*);   // for built-in modules

public:
    Context();
    ~Context();
    ModuleAlias* addModule(const str& name);
    void run(varstack&); // <--- this is where execution starts
};


// --- CODE GENERATOR ------------------------------------------------------ //


class CodeGen: noncopyable
{
protected:

    struct stkinfo
    {
        Type* type;
        stkinfo(Type* t): type(t) { }
    };

    CodeSeg& codeseg;
    mem lastLoadOp;

    mem addOp(OpCode);
    void revertLastLoad();

    std::stack<stkinfo> genStack;
    mem stkMax;
#ifdef DEBUG
    mem stkSize;
#endif

    void stkPush(Type* t, const variant& v);
    void stkPush(Type* t)
            { stkPush(t, null); }
    void stkPush(Constant* c)
            { stkPush(c->type, c->value); }
    const stkinfo& stkTop() const;
    Type* stkTopType() const
            { return stkTop().type; }
    Type* stkPop();

    bool tryCast(Type*, Type*);

public:
    CodeGen(CodeSeg&);
    ~CodeGen();

    void end();
    void endConstExpr(Type*);
    void assignRetVal(Type*);
    void loadNone();
    void loadBool(bool b)
            { loadConst(queenBee->defBool, b); }
    void loadChar(uchar c)
            { loadConst(queenBee->defChar, c); }
    void loadInt(integer i)
            { loadConst(queenBee->defInt, i); }
    void loadConst(Type*, const variant&);
    void discard();
    void swap();
    void implicitCastTo(Type*);
    void explicitCastTo(Type*);
    void dynamicCast();
    void testType(Type*);
    void testType();
    void arithmBinary(OpCode);
    void arithmUnary(OpCode);
    void elemToVec();
    void elemCat();
    void cat();
    void mkRange();
    void inRange();
    void cmp(OpCode op);
};


#endif // __VM_H

