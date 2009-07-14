#ifndef __SOURCE_H
#define __SOURCE_H


#include <stack>

#include "charset.h"
#include "common.h"
#include "runtime.h"


struct EParser: public emessage
{
    EParser(const str& ifilename, int ilinenum, const str& msg);
};


struct ENotFound: public EParser
{
    ENotFound(const str& ifilename, int ilinenum, const str& ientry);
};



enum Token
{
    tokUndefined = -1,
    // Blocks: for the compiler, these tokens are transparent wrt to C-style
    // vs. Python-style modes
    tokBlockBegin, tokBlockEnd, tokSep, tokIndent,
    tokEof,
    tokIdent, tokIntValue, tokStrValue,

    tokModule, tokConst, tokDef, tokTypeOf,
    tokEnum, tokEcho, tokAssert, tokSizeOf, tokBegin, tokIf, tokElif, tokElse,
    tokWhile, tokBreak, tokContinue, tokCase, tokReturn, tokFinally,
    
    // Term level
    tokMul, tokDiv, tokMod,
    // Arithm level
    tokPlus, tokMinus,
    // Cat level (simple expr)
    tokCat,
    // Rel level: the order should be in sync with comparison opcodes
    tokEqual, tokLessThan, tokLessEq, tokGreaterEq, tokGreaterThan, tokNotEq,
    // NOT level
    tokNot,
    // AND level
    tokAnd, tokShl, tokShr,
    // OR level
    tokOr, tokXor,
    
    tokIn, tokIs, tokAs,

    // Special chars and sequences
    tokComma, tokPeriod, tokRange,
    tokLSquare, tokRSquare, tokLParen, tokRParen, /* tokLCurly, tokRCurly, */
    tokAssign, tokExclam,
    
    // Aliases; don't define new consts after this
    tokLAngle = tokLessThan, tokRAngle = tokGreaterThan,
    tokCmpFirst = tokEqual, tokCmpLast = tokNotEq
};


enum SyntaxMode { syntaxIndent, syntaxCurly };


class Parser
{
protected:
    str fileName;
    objptr<fifo_intf> input;
    bool newLine;
    std::stack<int> indentStack;
    int linenum;
    int indent;
    bool singleLineBlock; // if a: b = c
    int curlyLevel;

    str errorLocation() const;
    void parseStringLiteral();
    void skipMultilineComment();
    void skipSinglelineComment();

public:
    Token token;
    str strValue;
    uinteger intValue;
    
    Parser(const str&, fifo_intf*);
    ~Parser();
    
    Token next();

    bool isAssignment()
            { return token == tokAssign; }
    void error(const str& msg);
    void errorWithLoc(const str& msg);
    void error(const char*);
    void errorWithLoc(const char*);
    void errorNotFound(const str& ident);
    void skipSep();
    void skip(Token tok, const char* errName);
    bool skipIf(Token tok)
            { if (token == tok) { next(); return true; } return false; }
    void skipBlockBegin();
    void skipBlockEnd();
    str getIdent();
    
    str getFileName() const { return fileName; }
    int getLineNum() const { return linenum; }
    int getIndent() const { return indent; }
    
    void skipEol();
};


str extractFileName(str filepath);
str mkPrintable(char c);
str mkPrintable(const str&);

// Exposed for unit tests
extern const charset wsChars;
extern const charset identFirst;
extern const charset identRest;
extern const charset digits;
extern const charset hexDigits;
extern const charset printableChars;


#endif

