
#include "vm.h"


evoidfunc::evoidfunc() throw() { }
evoidfunc::~evoidfunc() throw() { }
const char* evoidfunc::what() throw() { return "Void function called"; }


CodeGen::CodeGen(CodeSeg& c, Module* m, State* treg, bool compileTime)
    : module(m), codeOwner(c.getStateType()), typeReg(treg), codeseg(c), locals(0),
      lastOp(opInv), prevLoaderOffs(-1), primaryLoaders()
{
    assert(treg != NULL);
    if (compileTime != (codeOwner == NULL))
        fatal(0x6003, "CodeGen: invalid codeOwner");
}


CodeGen::~CodeGen()
    { }


void CodeGen::error(const char* msg)
    { throw emessage(msg); }


void CodeGen::error(const str& msg)
    { throw emessage(msg); }


void CodeGen::addOp(Type* type, OpCode op)
{
    memint offs = getCurrentOffs();
    simStack.push_back(SimStackItem(type, offs));
    if (isPrimaryLoader(op))
        primaryLoaders.push_back(offs);
    if (getStackLevel() > codeseg.stackSize)
        codeseg.stackSize = getStackLevel();
    addOp(op);
}


Type* CodeGen::stkPop()
{
    const SimStackItem& s = simStack.back();
    prevLoaderOffs = s.loaderOffs;
    if (!primaryLoaders.empty() && s.loaderOffs < primaryLoaders.back())
        primaryLoaders.pop_back();
    Type* result = s.type;
    simStack.pop_back();
    return result;
}


void CodeGen::undoSubexpr()
{
    // This works based on the assumption that at any stack level there is a 
    // corresponding primary loader starting from which all code can be safely
    // discarded. I think this should work provided that any instruction
    // pushes not more than one value onto the stack (regardless of how many
    // were pop'ed off the stack). See also stkPop().
    memint from;
    primaryLoaders.pop_back(from); // get & pop
    codeseg.erase(from);
    simStack.pop_back();
    prevLoaderOffs = -1;
}


bool CodeGen::lastWasFuncCall()
{
    return isCaller(codeseg[stkLoaderOffs()]);
}


void CodeGen::stkReplaceType(Type* t)
{
    simStack.backw().type = t;
}


bool CodeGen::tryImplicitCast(Type* to)
{
    Type* from = stkType();

    if (from == to)
        return true;

    if (to->isVariant() || from->canAssignTo(to))
    {
        // canAssignTo() should take care of polymorphic typecasts
        stkReplaceType(to);
        return true;
    }

    // Vector elements are automatically converted to vectors when necessary,
    // e.g. char -> str
    if (to->isAnyVec() && from->identicalTo(PContainer(to)->elem))
    {
        elemToVec(PContainer(to));
        return true;
    }

    if (from->isNullCont() && to->isAnyCont())
    {
        undoSubexpr();
        loadEmptyConst(to);
        return true;
    }

    return false;
}


void CodeGen::implicitCast(Type* to, const char* errmsg)
{
    // TODO: better error message, something like <type> expected; use Type::dump()
    if (!tryImplicitCast(to))
        error(errmsg == NULL ? "Type mismatch" : errmsg);
}


void CodeGen::explicitCast(Type* to)
{
    if (tryImplicitCast(to))
        return;

    Type* from = stkType();

    if (from->isAnyOrd() && to->isAnyOrd())
        stkReplaceType(to);

    else if (from->isVariant())
    {
        stkPop();
        addOp<Type*>(to, opCast, to);
    }

    // TODO: better error message with type defs
    else
        error("Invalid explicit typecast");
}


void CodeGen::isType(Type* to)
{
    Type* from = stkType();
    if (from->canAssignTo(to))
    {
        undoSubexpr();
        loadConst(queenBee->defBool, 1);
    }
    else if (from->isAnyState() || from->isVariant())
    {
        stkPop();
        addOp<Type*>(queenBee->defBool, opIsType, to);
    }
    else
    {
        undoSubexpr();
        loadConst(queenBee->defBool, 0);
    }
}


void CodeGen::mkRange()
{
    Type* left = stkType(2);
    if (!left->isAnyOrd())
        error("Non-ordinal range bounds");
    implicitCast(left, "Incompatible range bounds");
    stkPop();
    stkPop();
    addOp(POrdinal(left)->getRangeType(), opMkRange);
}


void CodeGen::deinitLocalVar(Variable* var)
{
    // TODO: don't generate POPs if at the end of a function in RELEASE mode
    assert(var->isLocalVar());
    assert(locals == getStackLevel());
    if (var->id != locals - 1)
        fatal(0x6002, "Invalid local var id");
    popValue();
    locals--;
}


void CodeGen::deinitFrame(memint baseLevel)
{
    memint topLevel = getStackLevel();
    for (memint i = topLevel; i > baseLevel; i--)
    {
        bool isPod = stkType(topLevel - i + 1)->isPod();
        addOp(isPod ? opPopPod : opPop);
    }
}


void CodeGen::popValue()
{
    bool isPod = stkPop()->isPod();
    addOp(isPod ? opPopPod : opPop);
}


Type* CodeGen::tryUndoTypeRef()
{
    memint offs = stkLoaderOffs();
    if (codeseg[offs] == opLoadTypeRef)
    {
        Type* type = codeseg.at<Type*>(offs + 1);
        undoSubexpr();
        return type;
    }
    else
        return NULL;
}


bool CodeGen::deref()
{
    Type* type = stkType();
    if (type->isReference())
    {
        type = type->getValueType();
        if (type->isDerefable())
        {
            stkPop();
            addOp(type, opDeref);
        }
        else
            notimpl();
        return true;
    }
    return false;
}


void CodeGen::mkref()
{
    Type* type = stkType();
    if (!type->isReference())
    {
        if (codeseg[stkLoaderOffs()] == opDeref)
            error("Superfluous automatic dereference");
        if (type->isDerefable())
        {
            stkPop();
            addOp(type->getRefType(), opMkRef);
        }
        else
            error("Can't convert to reference");
    }
}


void CodeGen::nonEmpty()
{
    Type* type = stkType();
    if (!type->isBool())
    {
        stkPop();
        addOp(queenBee->defBool, opNonEmpty);
    }
}


void CodeGen::loadTypeRef(Type* type)
{
    addOp<Type*>(defTypeRef, opLoadTypeRef, type);
}


void CodeGen::loadConst(Type* type, const variant& value)
{
    // NOTE: compound consts should be held by a smart pointer somewhere else
    switch(value.getType())
    {
    case variant::VOID:
        addOp(type, opLoadNull);
        return;
    case variant::ORD:
        {
            assert(type->isAnyOrd());
            integer i = value._int();
            if (i == 0)
                addOp(type, opLoad0);
            else if (i == 1)
                addOp(type, opLoad1);
            else if (uinteger(i) <= 255)
                addOp<uchar>(type, opLoadByte, i);
            else
                addOp<integer>(type, opLoadOrd, i);
        }
        return;
    case variant::REAL:
        notimpl();
        break;
    case variant::VARPTR:
        break;    
    case variant::STR:
        assert(type->isByteVec());
        addOp<object*>(type, opLoadStr, value._str().obj);
        return;
    case variant::RANGE:
    case variant::VEC:
    case variant::SET:
    case variant::ORDSET:
    case variant::DICT:
    case variant::REF:
        break;
    case variant::RTOBJ:
        if (value._rtobj()->getType()->isTypeRef())
        {
            loadTypeRef(cast<Type*>(value._rtobj()));
            return;
        }
        break;
    }
    fatal(0x6001, "Unknown constant literal");
}


void CodeGen::loadDefinition(Definition* def)
{
    Type* aliasedType = def->getAliasedType();
    if (aliasedType && aliasedType->isAnyState())
    {
        State* stateType = PState(aliasedType);
        OpCode op = opInv;
        if (isCompileTime())
            op = opLoadNullFuncPtr;
        else if (stateType->parent == codeOwner->parent)
            op = opLoadOuterFuncPtr;
        else if (stateType->parent == codeOwner)
            op = opLoadSelfFuncPtr;
        else
            error("Invalid context for a function pointer");
        addOp<State*>(stateType->getFuncPtr(), op, stateType);
    }
    else if (aliasedType || def->type->isVoid()
            || def->type->isAnyOrd() || def->type->isByteVec())
        loadConst(def->type, def->value);
    else
        addOp<Definition*>(def->type, opLoadConst, def);
}


static variant::Type typeToVarType(Type* t)
{
    // TYPEREF, VOID, VARIANT, REF,
    //    BOOL, CHAR, INT, ENUM,
    //    NULLCONT, VEC, SET, DICT,
    //    FIFO, PROTOTYPE, SELFSTUB, STATE
    // VOID, ORD, REAL, VARPTR,
    //      STR, VEC, SET, ORDSET, DICT, REF, RTOBJ
    switch (t->typeId)
    {
    case Type::TYPEREF:
        return variant::RTOBJ;
    case Type::VOID:
    case Type::NULLCONT:
    case Type::VARIANT:
        return variant::VOID;
    case Type::REF:
        return variant::REF;
    case Type::RANGE:
        return variant::RANGE;
    case Type::BOOL:
    case Type::CHAR:
    case Type::INT:
    case Type::ENUM:
        return variant::ORD;
    case Type::VEC:
        return t->isByteVec() ? variant::STR : variant::VEC;
    case Type::SET:
        return t->isByteSet() ? variant::ORDSET : variant::SET;
    case Type::DICT:
        return t->isByteDict() ? variant::VEC : variant::DICT;
    case Type::FIFO:
    case Type::PROTOTYPE:
    case Type::STATE:
    case Type::FUNCPTR:
        return variant::RTOBJ;
    case Type::SELFSTUB:
        throw emessage("'self' incomplete");
    }
    return variant::VOID;
}


void CodeGen::loadEmptyConst(Type* type)
    { addOp<uchar>(type, opLoadEmptyVar, typeToVarType(type)); }


void CodeGen::loadSymbol(Symbol* sym)
{
    if (sym->isAnyDef())
        loadDefinition(PDefinition(sym));
    else if (sym->isAnyVar())
        loadVariable(PVariable(sym));
    else
        notimpl();
}


void CodeGen::loadLocalVar(LocalVar* var)
{
    assert(var->id >= -128 && var->id <= 127);
    addOp<char>(var->type, opLoadStkVar, var->id);
}


void CodeGen::loadVariable(Variable* var)
{
    assert(var->host != NULL);
    if (isCompileTime())
        // Load an error message generator in case it gets executed; however
        // this may be useful in expressions like typeof, where the value
        // is not needed:
        addOp(var->type, opConstExprErr);
    else if (var->isLocalVar() && var->host == codeOwner)
    {
        loadLocalVar(PLocalVar(var));
    }
    else if (var->isSelfVar() && var->host == codeOwner)
    {
        assert(var->id >= 0 && var->id <= 255);
        addOp<uchar>(var->type, opLoadSelfVar, var->id);
    }
    else if (var->isSelfVar() && var->host == codeOwner->parent)
    {
        assert(var->id >= 0 && var->id <= 255);
        addOp<uchar>(var->type, opLoadOuterVar, var->id);
    }
    else if (var->isSelfVar() && var->host == module)
    {
        loadDataSeg();
        loadMember(var);
    }
    else
        error("'" + var->name  + "' is not accessible within this context");
}


void CodeGen::loadMember(const str& ident)
{
    Type* stateType = stkType();
    if (!stateType->isAnyState())
        error("Invalid member selection");
    loadMember(PState(stateType)->findShallow(ident));
}


void CodeGen::loadMember(Symbol* sym)
{
    Type* type = stkType();
    // TODO: see if it's a FuncPtr, discard the whole designator and retrieve State*
    if (!type->isAnyState())
        error("Invalid member selection");
    if (sym->host != PState(type))  // shouldn't happen
        fatal(0x600c, "Invalid member selection");
    if (sym->isAnyVar())
        loadMember(PVariable(sym));
    else if (sym->isAnyDef())
    {
        Definition* def = PDefinition(sym);
        Type* stateType = def->getAliasedType();
        if (stateType && stateType->isAnyState())
        {
            stkPop();
            // Most of the time opMkFuncPtr is replaced by opMethodCall
            addOp<State*>(PState(stateType)->getFuncPtr(), opMkFuncPtr, PState(stateType));
        }
        else
        {
            undoSubexpr();
            loadDefinition(def);
        }
    }
    else
        notimpl();
}


void CodeGen::loadMember(Variable* var)
{
    Type* stateType = stkPop();
    if (isCompileTime())
        addOp(var->type, opConstExprErr);
    else
    {
        if (!stateType->isAnyState() || var->host != stateType || !var->isSelfVar())
            error("Invalid member selection");
        assert(var->id >= 0 && var->id <= 255);
        addOp<uchar>(var->type, opLoadMember, var->id);
    }
}


void CodeGen::loadThis()
{
    if (isCompileTime())
        error("'this' is not available in const expressions");
    else if (codeOwner->parent && codeOwner->parent->isConstructor())
        addOp(codeOwner->parent, opLoadOuterObj);
    else
        error("'this' is not available within this context");
}


void CodeGen::loadDataSeg()
{
    if (isCompileTime())
        error("Static data can not be accessed in const expressions");
    addOp(module, opLoadDataSeg);
}


void CodeGen::initLocalVar(LocalVar* var)
{
    if (var->host != codeOwner)
        fatal(0x6005, "initLocalVar(): not my var");
    // Local var simply remains on the stack, so just check the types.
    assert(var->id >= 0 && var->id <= 127);
    if (locals != getStackLevel() - 1 || var->id != locals)
        fatal(0x6004, "initLocalVar(): invalid var id");
    locals++;
    implicitCast(var->type, "Variable type mismatch");
}


void CodeGen::initSelfVar(SelfVar* var)
{
    if (var->host != codeOwner)
        fatal(0x6005, "initSelfVar(): not my var");
    implicitCast(var->type, "Variable type mismatch");
    stkPop();
    assert(var->id >= 0 && var->id <= 255);
    addOp<uchar>(opInitSelfVar, var->id);
}


void CodeGen::incLocalVar(LocalVar* var)
{
    assert(var->id >= 0 && var->id <= 127);
    addOp<char>(opIncStkVar, var->id);
}


void CodeGen::loadContainerElem()
{
    // This is square brackets op - can be string, vector, array or dictionary.
    OpCode op = opInv;
    Type* contType = stkType(2);
    if (contType->isAnyVec())
    {
        implicitCast(queenBee->defInt, "Vector index must be integer");
        op = contType->isByteVec() ? opStrElem : opVecElem;
    }
    else if (contType->isAnyDict())
    {
        implicitCast(PContainer(contType)->index, "Dictionary key type mismatch");
        op = contType->isByteDict() ? opByteDictElem : opDictElem;
    }
    else if (contType->isAnySet())
    {
        // Selecting a set element thorugh [] returns void, because that's the
        // element type for sets. However, [] selection is used with operator del,
        // that's why we need the opcode opSetElem, which actually does nothing.
        // (see CodeGen::deleteContainerElem())
        implicitCast(PContainer(contType)->index, "Set element type mismatch");
        op = contType->isByteSet() ? opByteSetElem : opSetElem;
    }
    else
        error("Vector/dictionary/set expected");
    stkPop();
    stkPop();
    addOp(PContainer(contType)->elem, op);
}


void CodeGen::loadKeyByIndex()
{
    // For non-byte dicts and sets, used internally by the for loop parser
    Type* contType = stkType(2);
    if (!stkType()->isAnyOrd())
        fatal(0x6008, "loadContainerElemByIndex(): invalid index");
    stkPop();
    stkPop();
    if (contType->isAnyDict() && !contType->isByteDict())
        addOp(PContainer(contType)->index, opDictKeyByIdx);
    else if (contType->isAnySet() && !contType->isByteSet())
        addOp(PContainer(contType)->index, opSetKey);
    else
        fatal(0x6009, "loadContainerElemByIndex(): invalid container type");
}


void CodeGen::loadDictElemByIndex()
{
    // Used internally by the for loop parser
    Type* contType = stkType(2);
    if (!stkType()->isAnyOrd())
        fatal(0x6008, "loadContainerElemByIndex(): invalid index");
    stkPop();
    stkPop();
    if (contType->isAnyDict() && !contType->isByteDict())
        addOp(PContainer(contType)->elem, opDictElemByIdx);
    else
        fatal(0x6009, "loadDictKeyByIndex(): invalid container type");
}


void CodeGen::loadSubvec()
{
    Type* contType = stkType(3);
    Type* left = stkType(2);
    Type* right = stkType();
    bool tail = right->isVoid();
    if (!tail)
        implicitCast(left);
    if (contType->isAnyVec())
    {
        if (!left->isAnyOrd())
            error("Non-ordinal range bounds");
        stkPop();
        stkPop();
        stkPop();
        addOp(contType, contType->isByteVec() ? opSubstr : opSubvec);
    }
    else
        error("Vector/string type expected");
}


void CodeGen::length()
{
    // NOTE: # for sets and dicts is not a language feature, it's needed for 'for' loops
    // TODO: maybe then we should allow # on these only internally
    Type* type = stkType();
    if (type->isNullCont())
    {
        undoSubexpr();
        loadConst(queenBee->defInt, 0);
    }
    else if (type->isByteSet())
    {
        undoSubexpr();
        loadConst(queenBee->defInt, POrdinal(PContainer(type)->index)->getRange());
    }
    else
    {
        OpCode op = opInv;
        if (type->isAnySet())
            op = opSetLen;
        else if (type->isAnyVec() || type->isByteDict())
            op = type->isByteVec() ? opStrLen : opVecLen;
        else if (type->isAnyDict())
            op = opDictLen;
        else
            error("'#' expects vector or string");
        stkPop();
        addOp(queenBee->defInt, op);
    }
}


Container* CodeGen::elemToVec(Container* vecType)
{
    Type* elemType = stkType();
    if (vecType)
    {
        if (!vecType->isAnyVec())
            error("Vector type expected");
        implicitCast(vecType->elem, "Vector/string element type mismatch");
    }
    else
        vecType = elemType->deriveVec(typeReg);
    stkPop();
    addOp(vecType, vecType->isByteVec() ? opChrToStr : opVarToVec);
    return vecType;
}


void CodeGen::elemCat()
{
    Type* vecType = stkType(2);
    if (!vecType->isAnyVec())
        error("Vector/string type expected");
    implicitCast(PContainer(vecType)->elem, "Vector/string element type mismatch");
    stkPop();
    addOp(vecType->isByteVec() ? opChrCat: opVarCat);
}


void CodeGen::cat()
{
    Type* vecType = stkType(2);
    if (!vecType->isAnyVec())
        error("Left operand is not a vector");
    implicitCast(vecType, "Vector/string types do not match");
    stkPop();
    addOp(vecType->isByteVec() ? opStrCat : opVecCat);
}


Container* CodeGen::elemToSet()
{
    Type* elemType = stkType();
    Container* setType = elemType->deriveSet(typeReg);
    stkPop();
    addOp(setType, setType->isByteSet() ? opElemToByteSet : opElemToSet);
    return setType;
}


Container* CodeGen::rangeToSet()
{
    Type* left = stkType(2);
    if (!left->isAnyOrd())
        error("Non-ordinal range bounds");
    if (!left->canAssignTo(stkType()))
        error("Incompatible range bounds");
    Container* setType = left->deriveSet(typeReg);
    if (!setType->isByteSet())
        error("Invalid element type for ordinal set");
    stkPop();
    stkPop();
    addOp(setType, opRngToByteSet);
    return setType;
}


void CodeGen::setAddElem()
{
    Type* setType = stkType(2);
    if (!setType->isAnySet())
        error("Set type expected");
    implicitCast(PContainer(setType)->index, "Set element type mismatch");
    stkPop();
    addOp(setType->isByteSet() ? opByteSetAddElem : opSetAddElem);
}


void CodeGen::checkRangeLeft()
{
    Type* setType = stkType(2);
    if (!setType->isByteSet())
        error("Byte set type expected");
    implicitCast(PContainer(setType)->index, "Set element type mismatch");
}


void CodeGen::setAddRange()
{
    Type* setType = stkType(3);
    if (!setType->isByteSet())
        error("Byte set type expected");
    implicitCast(PContainer(setType)->index, "Set element type mismatch");
    stkPop();
    stkPop();
    addOp(opByteSetAddRng);
}


Container* CodeGen::pairToDict()
{
    Type* val = stkType();
    Type* key = stkType(2);
    Container* dictType = val->deriveContainer(typeReg, key);
    stkPop();
    stkPop();
    addOp(dictType, dictType->isByteDict() ? opPairToByteDict : opPairToDict);
    return dictType;
}


void CodeGen::checkDictKey()
{
    Type* dictType = stkType(2);
    if (!dictType->isAnyDict())
        error("Dictionary type expected");
    implicitCast(PContainer(dictType)->index, "Dictionary key type mismatch");
}


void CodeGen::dictAddPair()
{
    Type* dictType = stkType(3);
    if (!dictType->isAnyDict())
        error("Dictionary type expected");
    implicitCast(PContainer(dictType)->elem, "Dictionary element type mismatch");
    stkPop();
    stkPop();
    addOp(dictType->isByteDict() ? opByteDictAddPair : opDictAddPair);
}


void CodeGen::inCont()
{
    Type* contType = stkPop();
    Type* elemType = stkPop();
    OpCode op = opInv;
    if (contType->isAnySet())
        op = contType->isByteSet() ? opInByteSet : opInSet;
    else if (contType->isAnyDict())
        op = contType->isByteDict() ? opInByteDict : opInDict;
    else
        error("Set/dict type expected");
    if (!elemType->canAssignTo(PContainer(contType)->index))
        error("Key type mismatch");
    addOp(queenBee->defBool, op);
}


void CodeGen::inBounds()
{
    Type* type = tryUndoTypeRef();
    if (type == NULL)
        error("Const type reference expected");
    Type* elemType = stkPop();
    if (!elemType->isAnyOrd())
        error("Ordinal type expected");
    if (!type->isAnyOrd())
        error("Ordinal type reference expected");
    addOp<Ordinal*>(queenBee->defBool, opInBounds, POrdinal(type));
}


void CodeGen::inRange()
{
    Type* right = stkPop();
    Type* left = stkPop();
    if (!right->isRange())
        error("Range type expected");
    if (!left->canAssignTo(PRange(right)->elem))
        error("Range element type mismatch");
    addOp(queenBee->defBool, opInRange);
}


void CodeGen::inRange2(bool isCaseLabel)
{
    Type* right = stkPop();
    Type* left = stkPop();
    Type* elem = isCaseLabel ? stkType() : stkPop();
    if (!left->canAssignTo(right))
        error("Incompatible range bounds");
    if (!elem->canAssignTo(left))
        error("Element type mismatch");
    if (!elem->isAnyOrd() || !left->isAnyOrd() || !right->isAnyOrd())
        error("Ordinal type expected");
    addOp(queenBee->defBool, isCaseLabel ? opCaseRange : opInRange2);
}


void CodeGen::arithmBinary(OpCode op)
{
    assert(op >= opAdd && op <= opBitShr);
    Type* right = stkPop();
    Type* left = stkPop();
    if (!right->isInt() || !left->isInt())
        error("Operand types do not match binary operator");
    addOp(left->identicalTo(right) ? left : queenBee->defInt, op);
}


void CodeGen::arithmUnary(OpCode op)
{
    assert(op >= opNeg && op <= opNot);
    Type* type = stkType();
    if (!type->isInt())
        error("Operand type doesn't match unary operator");
    addOp(op);
}


void CodeGen::cmp(OpCode op)
{
    assert(isCmpOp(op));
    Type* left = stkType(2);
    implicitCast(left, "Type mismatch in comparison");
    Type* right = stkType();
    if (left->isAnyOrd() && right->isAnyOrd())
        addOp(opCmpOrd);
    else if (left->isByteVec() && right->isByteVec())
        addOp(opCmpStr);
    else
    {
        if (op != opEqual && op != opNotEq)
            error("Only equality can be tested for this type");
        addOp(opCmpVar);
    }
    stkPop();
    stkPop();
    addOp(queenBee->defBool, op);
}


void CodeGen::caseCmp()
{
    Type* left = stkType(2);
    implicitCast(left, "Type mismatch in comparison");
    Type* right = stkPop();
    if (left->isAnyOrd() && right->isAnyOrd())
        addOp(queenBee->defBool, opCaseOrd);
    else if (left->isByteVec() && right->isByteVec())
        addOp(queenBee->defBool, opCaseStr);
    else
        addOp(queenBee->defBool, opCaseVar);
}


void CodeGen::_not()
{
    Type* type = stkType();
    if (type->isInt())
        addOp(opBitNot);
    else
    {
        implicitCast(queenBee->defBool, "Boolean or integer operand expected");
        addOp(opNot);
    }
}


void CodeGen::localVarCmp(LocalVar* var, OpCode op)
{
    // implicitCast(var->type, "Type mismatch in comparison");
    if (!stkType()->isAnyOrd() || !var->type->isAnyOrd())
        fatal(0x6007, "localVarCmp(): unsupported type");
    stkPop();
    if (op == opGreaterThan)
        op = opStkVarGt;
    else if (op == opGreaterEq)
        op = opStkVarGe;
    else
        fatal(0x6007, "localVarCmp(): unsupported opcode");
    assert(var->id >= 0 && var->id <= 127);
    addOp<char>(queenBee->defBool, op, var->id);
}


void CodeGen::localVarCmpLength(LocalVar* var, LocalVar* contVar)
{
    // TODO: optimize (single instruction?)
    loadLocalVar(contVar);
    length();
    localVarCmp(var, opGreaterEq);
}


void CodeGen::boolJump(memint target, OpCode op)
{
    assert(isBoolJump(op));
    implicitCast(queenBee->defBool, "Boolean expression expected");
    stkPop();
    _jump(target, op);
}


memint CodeGen::boolJumpForward(OpCode op)
{
    assert(isBoolJump(op));
    implicitCast(queenBee->defBool, "Boolean expression expected");
    stkPop();
    return jumpForward(op);
}


memint CodeGen::jumpForward(OpCode op)
{
    assert(isJump(op));
    memint pos = getCurrentOffs();
    addOp<jumpoffs>(op, 0);
    return pos;
}


void CodeGen::resolveJump(memint target)
{
    assert(target <= getCurrentOffs() - 1 - memint(sizeof(jumpoffs)));
    assert(isJump(codeseg[target]));
    memint offs = getCurrentOffs() - (target + 1 + memint(sizeof(jumpoffs)));
    if (offs > 32767)
        error("Jump target is too far away");
    codeseg.atw<jumpoffs>(target + 1) = offs;
}


void CodeGen::_jump(memint target, OpCode op)
{
    assert(target <= getCurrentOffs() - 1 - memint(sizeof(jumpoffs)));
    memint offs = target - (getCurrentOffs() + 1 + memint(sizeof(jumpoffs)));
    if (offs < -32768)
        error("Jump target is too far away");
    addOp<jumpoffs>(op, jumpoffs(offs));
}


void CodeGen::linenum(integer n)
{
    if (lastOp != opLineNum)
        addOp<integer>(opLineNum, n);
}


void CodeGen::assertion(const str& cond)
{
    implicitCast(queenBee->defBool, "Boolean expression expected for 'assert'");
    stkPop();
    addOp(opAssert, cond.obj);
}


void CodeGen::dumpVar(const str& expr)
{
    Type* type = stkPop();
    addOp(opDump, expr.obj);
    add(type);
}


void CodeGen::programExit()
{
    stkPop();
    addOp(opExit);
}


// --- ASSIGNMENTS --------------------------------------------------------- //


static void errorLValue()
    { throw emessage("Not an l-value"); }

static void errorNotAddressableElem()
    { throw emessage("Not an addressable container element"); }

static void errorNotInsertableElem()
    { throw emessage("Not an insertable location"); }


static OpCode loaderToStorer(OpCode op)
{
    switch (op)
    {
        case opLoadSelfVar:     return opStoreSelfVar;
        case opLoadOuterVar:    return opStoreOuterVar;
        case opLoadStkVar:      return opStoreStkVar;
        case opLoadMember:      return opStoreMember;
        case opDeref:           return opStoreRef;
        // end grounded loaders
        case opStrElem:         return opStoreStrElem;
        case opVecElem:         return opStoreVecElem;
        case opDictElem:        return opStoreDictElem;
        case opByteDictElem:    return opStoreByteDictElem;
        default:
            errorLValue();
            return opInv;
    }
}


static OpCode loaderToLea(OpCode op)
{
    switch (op)
    {
        case opLoadSelfVar:     return opLeaSelfVar;
        case opLoadOuterVar:    return opLeaOuterVar;
        case opLoadStkVar:      return opLeaStkVar;
        case opLoadMember:      return opLeaMember;
        case opDeref:           return opLeaRef;
        default:
            errorLValue();
            return opInv;
    }
}


static OpCode loaderToInserter(OpCode op)
{
    switch (op)
    {
        case opStrElem:   return opStrIns;
        case opVecElem:   return opVecIns;
        case opSubstr:    return opSubstrIns;
        case opSubvec:    return opSubvecIns;
        default:
            errorNotInsertableElem();
            return opInv;
    }
}


static OpCode loaderToDeleter(OpCode op)
{
    switch (op)
    {
        case opStrElem:       return opDelStrElem;
        case opVecElem:       return opDelVecElem;
        case opSubstr:        return opDelSubstr;
        case opSubvec:        return opDelSubvec;
        case opDictElem:      return opDelDictElem;
        case opByteDictElem:  return opDelByteDictElem;
        case opSetElem:       return opDelSetElem;
        case opByteSetElem:   return opDelByteSetElem;
        default:
            errorNotAddressableElem();
            return opInv;
    }
}


str CodeGen::lvalue()
{
    memint offs = stkLoaderOffs();
    OpCode loader = codeseg[offs];
    if (isGroundedLoader(loader))
    {
        // Plain assignment to a "grounded" variant: remove the loader and
        // return the corresponding storer to be appended later at the end
        // of the assignment statement.
    }
    else
    {
        // A more complex assignment case: look at the previous loader - it 
        // should be a grounded one, transform it to its LEA equivalent, then
        // transform/move the last loader like in the previous case.
        memint prev = stkPrevLoaderOffs();
        codeseg.replaceOp(prev, loaderToLea(codeseg[prev]));
    }
    OpCode storer = loaderToStorer(loader);
    codeseg.replaceOp(offs, storer);
    return codeseg.cutOp(offs);
}


str CodeGen::arithmLvalue(Token tok)
{
    assert(tok >= tokAddAssign && tok <= tokModAssign);
    OpCode op = OpCode(opAddAssign + (tok - tokAddAssign));
    memint offs = stkLoaderOffs();
    codeseg.replaceOp(offs, loaderToLea(codeseg[offs]));
    offs = getCurrentOffs();
    codeseg.append(op);
    return codeseg.cutOp(offs);
}


void CodeGen::catLvalue()
{
    if (!stkType()->isAnyVec())
        error("'|=' expects vector/string type");
    memint offs = stkLoaderOffs();
    codeseg.replaceOp(offs, loaderToLea(codeseg[offs]));
}


str CodeGen::insLvalue()
{
    memint offs = stkLoaderOffs();
    OpCode inserter = loaderToInserter(codeseg[offs]);
    memint prev = stkPrevLoaderOffs();
    codeseg.replaceOp(prev, loaderToLea(codeseg[prev]));
    codeseg.replaceOp(offs, inserter);
    return codeseg.cutOp(offs);
}


void CodeGen::assignment(const str& storerCode)
{
    assert(!storerCode.empty());
    Type* dest = stkType(2);
    if (dest->isVoid())  // Don't remember why it's here. Possibly because of set elem selection
        error("Destination is void type");
    implicitCast(dest, "Type mismatch in assignment");
    codeseg.append(storerCode);
    stkPop();
    stkPop();
}


void CodeGen::deleteContainerElem()
{
    memint offs = stkLoaderOffs();
    OpCode deleter = loaderToDeleter(codeseg[offs]);
    memint prev = stkPrevLoaderOffs();
    codeseg.replaceOp(prev, loaderToLea(codeseg[prev]));
    codeseg.replaceOp(offs, deleter);
    stkPop();
}


void CodeGen::catAssign()
{
    Type* left = stkType(2);
    if (!left->isAnyVec())
        error("'|=' expects vector/string type");
    Type* right = stkType();
    if (right->canAssignTo(PContainer(left)->elem))
        addOp(left->isByteVec() ? opChrCatAssign : opVarCatAssign);
    else
    {
        implicitCast(left, "Type mismatch in in-place concatenation");
        addOp(left->isByteVec() ? opStrCatAssign : opVecCatAssign);
    }
    stkPop();
    stkPop();
}


void CodeGen::fifoPush()
{
    Type* left = stkType(2);
    if (!left->isAnyFifo())
        error("'<<' expects FIFO type");
    Type* right = stkType();
    // TODO: call a builtin
    // TODO: what about conversions like in C++? probably Nah.
    if (right->isAnyVec() && PContainer(right)->elem->identicalTo(PFifo(left)->elem))
    {
        notimpl();
    }
    else if (tryImplicitCast(PFifo(left)->elem))
    {
        notimpl();
    }
    else
        error("FIFO element type mismatch");
    stkPop();
    // Leave the FIFO object
}


// --- FUNCTIONS, CALLS ---------------------------------------------------- //


memint CodeGen::prolog()
{
    memint offs = getCurrentOffs();
    if (isCompileTime())
        ;
    else if (codeOwner->isConstructor())
        addOp<State*>(opEnterCtor, codeOwner);
    else if (codeOwner->isStatic())
        notimpl();
    else
        // All other functions need to create their frames. The size of the frame
        // though is not known at this point, will be resolved later in epilog()
        addOp<uchar>(opEnter, 0);
    return offs;
}


void CodeGen::epilog(memint prologOffs)
{
    memint selfVarCount = codeOwner->selfVarCount();
    if (isCompileTime())
        ;
    else if (codeOwner->isConstructor())
        ;
    else if (codeOwner->isStatic())
        notimpl();
    else
    {
        if (selfVarCount == 0)
            codeseg.eraseOp(prologOffs);
        else
        {
#ifdef DEBUG
            // The local stack frame is cleaned up automatically anyway; in
            // DEBUG mode we just need to verify correctness of compilation
            addOp<uchar>(opLeave, selfVarCount);
            codeseg.atw<uchar>(prologOffs + 1) = uchar(selfVarCount);
#endif
        }
    }
}


void CodeGen::call(FuncPtr* funcPtr)
{
    // TODO: indirect call (callee == NULL)

    State* callee = funcPtr->derivedFrom;
    Prototype* proto = callee->prototype;

    // Pop arguments and the return value off the simulation stack
    for (memint i = proto->formalArgs.size(); i--; )
    {
#ifdef DEBUG
        if (!stkType()->canAssignTo(proto->formalArgs[i]->type))
            error("Argument type mismatch");  // shouldn't happen, checked by the compiler earlier
#endif
        stkPop();
    }
    if (!proto->returnType->isVoid())
        stkPop();

    // Remove the opMk*FuncPtr and append a corresponding caller
    assert(stkType()->isFuncPtr());
    OpCode op = opInv;
    memint offs = stkLoaderOffs();
    switch (codeseg[offs])
    {
        case opLoadOuterFuncPtr: op = opSiblingCall; break;
        case opLoadSelfFuncPtr: op = opChildCall; break;
        case opMkFuncPtr: op = opMethodCall; break;
        default: notimpl();
    }
    stkPop(); // funcptr
    codeseg.eraseOp(offs); // erase funcptr loader

    // Finally, leave the return value (if any) on the simulation stack. At
    // run-time, however, in case of opMethodCall we have the 'this' object
    // for which the method was called and only then on top of it the return
    // value. The VM discards the object pointer and puts the ret value instead
    // so that everything looks like the method call has just returned a value.
    if (proto->returnType->isVoid())
    {
        addOp<State*>(op, callee);
        throw evoidfunc();
    }
    else
        addOp<State*>(callee->prototype->returnType, op, callee);
}


void CodeGen::end()
{
    codeseg.close();
    assert(getStackLevel() == locals);
}

