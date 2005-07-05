/*****
 * dec.cc
 * Andy Hammerlindl 2002/8/29
 *
 * Represents the abstract syntax tree for declarations in the language.
 * Also included is an abstract syntax for types as they are most often
 * used with declarations.
 *****/

#include "errormsg.h"
#include "coenv.h"
#include "dec.h"
#include "stm.h"
#include "exp.h"
#include "camp.tab.h"  // For qualifiers.
#include "runtime.h"

namespace absyntax {

using namespace trans;
using namespace types;

void nameTy::prettyprint(ostream &out, int indent)
{
  prettyname(out, "nameTy",indent);

  id->prettyprint(out, indent+1);
}

types::ty *nameTy::trans(coenv &e, bool tacit)
{
  return id->typeTrans(e, tacit);
}


void arrayTy::prettyprint(ostream &out, int indent)
{
  prettyname(out, "arrayTy",indent);

  cell->prettyprint(out, indent+1);
  dims->prettyprint(out, indent+1);
}

function *arrayTy::opType(types::ty* t)
{
  function *ft = new function(primBoolean());
  ft->add(t);
  ft->add(t);

  return ft;
}

function *arrayTy::arrayType(types::ty* t)
{
  function *ft = new function(t);
  ft->add(t);

  return ft;
}

function *arrayTy::array2Type(types::ty* t)
{
  function *ft = new function(t);
  ft->add(t);
  ft->add(t);

  return ft;
}

function *arrayTy::cellIntType(types::ty* t)
{
  function *ft = new function(t);
  ft->add(primInt());
  
  return ft;
}
  
function *arrayTy::sequenceType(types::ty* t, types::ty *ct)
{
  function *ft = new function(t);
  function *fc = cellIntType(ct);
  ft->add(fc);
  ft->add(primInt());

  return ft;
}

function *arrayTy::cellTypeType(types::ty* t)
{
  function *ft = new function(t);
  ft->add(t);
  
  return ft;
}
  
function *arrayTy::mapType(types::ty* t, types::ty *ct)
{
  function *ft = new function(t);
  function *fc = cellTypeType(ct);
  ft->add(fc);
  ft->add(t);

  return ft;
}

void arrayTy::addOps(coenv &e, types::ty* t, types::ty *ct)
{
  function *ft = opType(t);
  function *ftarray = arrayType(t);
  function *ftarray2 = array2Type(t);
  function *ftsequence = sequenceType(t,ct);
  function *ftmap = mapType(t,ct);
  
  e.e.addVar(getPos(), symbol::trans("alias"),
      new varEntry(ft,new bltinAccess(run::arrayAlias)),true);

  if(dims->size() == 1) {
    e.e.addVar(getPos(), symbol::trans("copy"),
	       new varEntry(ftarray,new bltinAccess(run::arrayCopy)),true);
    e.e.addVar(getPos(), symbol::trans("concat"),
	       new varEntry(ftarray2,new bltinAccess(run::arrayConcat)),true);
    e.e.addVar(getPos(), symbol::trans("sequence"),
	       new varEntry(ftsequence,new bltinAccess(run::arraySequence)),
	       true);
    e.e.addVar(getPos(), symbol::trans("map"),
	       new varEntry(ftmap,new bltinAccess(run::arrayFunction)),true);
  }
  if(dims->size() == 2) {
    e.e.addVar(getPos(), symbol::trans("copy"),
	       new varEntry(ftarray,new bltinAccess(run::array2Copy)),true);
    e.e.addVar(getPos(), symbol::trans("transpose"),
	       new varEntry(ftarray,new bltinAccess(run::array2Transpose)),
	       true);
  }
}

types::ty *arrayTy::trans(coenv &e, bool tacit)
{
  types::ty *ct = cell->trans(e, tacit);
  assert(ct);

  types::ty *t = dims->truetype(ct);
  assert(t);
  
  addOps(e,t,ct);
  
  return t;
}


void dec::prettyprint(ostream &out, int indent)
{
  prettyname(out, "dec", indent);
}


void modifierList::prettyprint(ostream &out, int indent)
{
  prettyindent(out,indent);
  out << "modifierList (";
  
  for (list<int>::iterator p = mods.begin(); p != mods.end(); ++p) {
    if (p != mods.begin())
      out << out << ", ";
    switch (*p) {
      case STATIC:
	out << "static";
	break;
#if 0	
      case DYNAMIC:
	out << "dynamic";
	break;
#endif	
      default:
	out << "invalid code:" << *p;
    }
  }

  out << ")\n";
}

bool modifierList::staticSet()
{
  for (list<int>::iterator p = mods.begin(); p != mods.end(); ++p)
//    if (*p == STATIC || *p == DYNAMIC)
    if (*p == STATIC)
      return true;
  return false;
}

bool modifierList::isStatic()
{
  int uses = 0;
  bool result = false;

  for (list<int>::iterator p = mods.begin(); p != mods.end(); ++p) {
    switch (*p) {
      case STATIC:
	++uses;
	result = true;
	break;
#if 0	
      case DYNAMIC:
	++uses;
	break;
#endif	
      case PUBLIC_TOK:
      case PRIVATE_TOK:
	break;
      default:
	em->compiler(getPos());
	*em << "bad token in modifier list";
    }
  }

  if (uses > 1) {
    em->error(getPos());
    *em << "too many modifiers";
  }

  return result;
}

permission modifierList::getPermission()
{
  int uses = 0;
  permission result = READONLY;

  for (list<int>::iterator p = mods.begin(); p != mods.end(); ++p) {
    switch (*p) {
      case PUBLIC_TOK:
	++uses;
	result = PUBLIC;
	break;
      case PRIVATE_TOK:
	++uses;
	result = PRIVATE;
	break;
      case STATIC:
//      case DYNAMIC:
	break;
      default:
	em->compiler(getPos());
	*em << "bad token in modifier list";
    }
  }

  if (uses > 1) {
    em->error(getPos());
    *em << "too many modifiers";
  }

  return result;
}


void modifiedRunnable::prettyprint(ostream &out, int indent)
{
  prettyname(out, "modifierRunnable",indent);

  mods->prettyprint(out, indent+1);
  body->prettyprint(out, indent+1);
}

void modifiedRunnable::trans(coenv &e)
{
  transAsField(e,0);
}

void modifiedRunnable::transAsField(coenv &e, record *r)
{
  if (mods->staticSet())
    e.c.pushModifier(mods->isStatic() ? EXPLICIT_STATIC : EXPLICIT_DYNAMIC);

  permission p = mods->getPermission();
  if (p != READONLY && (!r || !body->allowPermissions())) {
    em->error(pos);
    *em << "invalid permission modifier";
  }
  e.c.setPermission(p);

  if (r)
    body->transAsField(e,r);
  else
    body->trans(e);

  e.c.clearPermission();
  if (mods->staticSet())
    e.c.popModifier();
}


void decidstart::prettyprint(ostream &out, int indent)
{
  prettyindent(out, indent);
  out << "decidstart '" << *id << "'\n";

  if (dims)
    dims->prettyprint(out, indent+1);
}

types::ty *decidstart::getType(types::ty *base, coenv &, bool)
{
  return dims ? dims->truetype(base) : base;
}


void fundecidstart::prettyprint(ostream &out, int indent)
{
  prettyindent(out, indent);
  out << "fundecidstart '" << *id << "'\n";

  if (dims)
    dims->prettyprint(out, indent+1);
  if (params)
    params->prettyprint(out, indent+1);
}

types::ty *fundecidstart::getType(types::ty *base, coenv &e, bool tacit)
{
  types::ty *result = decidstart::getType(base, e, tacit);

  if (params) {
    return params->getType(result, e, true, tacit);
  }
  else {
    types::ty *t = new function(base);
    return t;
  }
}


void decid::prettyprint(ostream &out, int indent)
{
  prettyname(out, "decid",indent);

  start->prettyprint(out, indent+1);
  if (init)
    init->prettyprint(out, indent+1);
}

void decid::trans(coenv &e, types::ty *base)
{
  transAsField(e,0,base);
}

void decid::transAsField(coenv &e, record *r, types::ty *base)
{
  // give the field a location.
  access *a = r ? r->allocField(e.c.isStatic(), e.c.getPermission()) :
                  e.c.allocLocal();
                

  symbol *id = start->getName();
  types::ty *t = start->getType(base, e);
  assert(t);
  if (t->kind == ty_void) {
    em->compiler(getPos());
    *em << "can't declare variable of type void";
  }

  varEntry *ent = new varEntry(t, a);

  if (init)
    init->transToType(e, t);
  else {
    definit d(getPos());
    d.transToType(e, t);
  }
  
  // Add to the record so it can be accessed when qualified; add to the
  // environment so it can be accessed unqualified in the scope of the
  // record definition.
  if (r)
    r->addVar(id, ent);
  e.e.addVar(getPos(), id, ent);
  
  a->encodeWrite(getPos(), e.c);
  e.c.encode(inst::pop);
}

void decid::transAsTypedef(coenv &e, types::ty *base)
{
  types::ty *t = start->getType(base, e);
  assert(t);

  if (init) {
    em->error(getPos());
    *em << "type definition cannot have initializer";
  }
   
  // Add to type environment.
  e.e.addType(getPos(), start->getName(), t);
}

void decid::transAsTypedefField(coenv &e, types::ty *base, record *r)
{
  types::ty *t = start->getType(base, e);
  assert(t);

  if (init) {
    em->error(getPos());
    *em << "type definition cannot have initializer";
  }
   
  // Add to type to record and environment.
  r->addType(start->getName(), t);
  e.e.addType(getPos(), start->getName(), t);
}


void decidlist::prettyprint(ostream &out, int indent)
{
  prettyname(out, "decidlist",indent);

  for (list<decid *>::iterator p = decs.begin(); p != decs.end(); ++p)
    (*p)->prettyprint(out, indent+1);
}

void decidlist::trans(coenv &e, types::ty *base)
{
  for (list<decid *>::iterator p = decs.begin(); p != decs.end(); ++p)
    (*p)->trans(e, base);
}

void decidlist::transAsField(coenv &e, record *r, types::ty *base)
{
  for (list<decid *>::iterator p = decs.begin(); p != decs.end(); ++p)
    (*p)->transAsField(e, r, base);
}

void decidlist::transAsTypedef(coenv &e, types::ty *base)
{
  for (list<decid *>::iterator p = decs.begin(); p != decs.end(); ++p)
    (*p)->transAsTypedef(e, base);
}

void decidlist::transAsTypedefField(coenv &e, types::ty *base, record *r)
{
  for (list<decid *>::iterator p = decs.begin(); p != decs.end(); ++p)
    (*p)->transAsTypedefField(e, base, r);
}


void vardec::prettyprint(ostream &out, int indent)
{
  prettyname(out, "vardec",indent);

  base->prettyprint(out, indent+1);
  decs->prettyprint(out, indent+1);
}

void vardec::transAsTypedef(coenv &e)
{
  decs->transAsTypedef(e, base->trans(e));
}

void vardec::transAsTypedefField(coenv &e, record *r)
{
  decs->transAsTypedefField(e, base->trans(e), r);
}

void importdec::initialize(coenv &e, record *m, access *a)
{
  // Put the enclosing frame on the stack.
  if (!e.c.encode(m->getLevel()->getParent())) {
    em->error(getPos());
    *em << "import of struct '" << *m << "' is not in a valid scope";
  }
 
  // Encode the allocation. 
  e.c.encode(inst::makefunc,m->getInit());
  e.c.encode(inst::popcall);

  // Put the module into its memory location.
  a->encodeWrite(getPos(), e.c);
  e.c.encode(inst::pop);
}



void importdec::prettyprint(ostream &out, int indent)
{
  prettyindent(out, indent);
  out << "importdec (" << *id << ")\n";
}

void importdec::loadFailed(coenv &)
{
  em->warning(getPos());
  *em << "could not load module of name '" << *id << "'";
  em->sync();
}

void importdec::trans(coenv &e)
{
  transAsField(e,0);
}

void importdec::transAsField(coenv &e, record *r)
{
  record *m = e.e.getModule(id);
  if (m == 0) {
    loadFailed(e);
    return;
  }

  // PRIVATE as only the body of a record, may refer to an imported record.
  access *a = r ? r->allocField(e.c.isStatic(), PRIVATE) :
                  e.c.allocLocal(PRIVATE);

  import *i = new import(m, a);

  // While the import is allocated as a field of the record, it is
  // only accessible inside the initializer of the record (and
  // nested functions and initializers), so there is no need to add it
  // to the environment maintained by the record.
  e.e.addImport(getPos(), id, i);

  // Add the initializer for the record.
  initialize(e, m, a);
}


void typedec::prettyprint(ostream &out, int indent)
{
  prettyname(out, "typedec",indent);

  body->prettyprint(out, indent+1);
}


void formal::prettyprint(ostream &out, int indent)
{
  prettyname(out, "formal",indent);
  
  base->prettyprint(out, indent+1);
  if (start) start->prettyprint(out, indent+1);
  if (defval) defval->prettyprint(out, indent+1);
}

types::formal formal::trans(coenv &e, bool encodeDefVal, bool tacit) {
  return types::formal(getType(e,tacit),
                       getName(),
                       encodeDefVal ? getDefaultValue() : 0,
                       getExplicit());
}

types::ty *formal::getType(coenv &e, bool tacit) {
  types::ty *t = start ? start->getType(base->trans(e), e, tacit)
    : base->trans(e, tacit);
  if (t->kind == ty_void && !tacit) {
    em->compiler(getPos());
    *em << "can't declare parameters of type void";
    return primError();
  }
  return t;
}
  
void formals::prettyprint(ostream &out, int indent)
{
  prettyname(out, "formals",indent);

  for(list<formal *>::iterator p = fields.begin(); p != fields.end(); ++p)
    (*p)->prettyprint(out, indent+1);
}

void formals::addToSignature(signature& sig,
                             coenv &e, bool encodeDefVal, bool tacit) {
  for(list<formal *>::iterator p = fields.begin(); p != fields.end(); ++p)
    sig.add((*p)->trans(e, encodeDefVal, tacit));

  if (rest)
    sig.addRest(rest->trans(e, encodeDefVal, tacit));
}

// Returns the types of each parameter as a signature.
// encodeDefVal means that it will also encode information regarding
// the default values into the signature
signature *formals::getSignature(coenv &e, bool encodeDefVal, bool tacit)
{
  signature *sig = new signature;
  addToSignature(*sig,e,encodeDefVal,tacit);
  return sig;
}


// Returns the corresponding function type, assuming it has a return
// value of types::ty *result.
function *formals::getType(types::ty *result, coenv &e,
                           bool encodeDefVal,
			   bool tacit)
{
  function *ft = new function(result);
  addToSignature(ft->sig,e,encodeDefVal,tacit);
  return ft;
}

void formal::transAsVar(coenv &e, int index) {
  symbol *name = getName();
  if (name) {
    trans::access *a = e.c.accessFormal(index);
    assert(a);

    // Suppress error messages because they will already be reported
    // when the formals are translated to yield the type earlier.
    types::ty *t = getType(e, true);
    varEntry *v = new varEntry(t, a);

    e.e.addVar(getPos(), name, v);
  }
}

void formals::trans(coenv &e)
{
  int index = 0;

  for (list<formal *>::iterator p=fields.begin(); p!=fields.end(); ++p) {
    (*p)->transAsVar(e, index);
    ++index;
  }

  if (rest) {
    rest->transAsVar(e, index);
    ++index;
  }
}

void formals::reportDefaults()
{
  for(list<formal *>::iterator p = fields.begin(); p != fields.end(); ++p)
    if ((*p)->reportDefault())
      return;
  
  if (rest)
    rest->reportDefault();
}

void fundef::prettyprint(ostream &out, int indent)
{
  result->prettyprint(out, indent+1);
  params->prettyprint(out, indent+1);
  body->prettyprint(out, indent+1);
}

function *fundef::getType(coenv &e, bool tacit) {
  return params->getType(result->trans(e, tacit), e, tacit);
}

void fundef::trans(coenv &e) {
  function *ft=getType(e, false);
  
  // Create a new function environment.
  coder fc = e.c.newFunction(ft);
  coenv fe(fc,e.e);

  // Translate the function.
  fe.e.beginScope();
  params->trans(fe);
  
  body->trans(fe);

  types::ty *rt = ft->result;
  if (rt->kind != ty_void &&
      rt->kind != ty_error &&
      !body->returns()) {
    em->error(body->getPos());
    *em << "function must return a value";
  }

  fe.e.endScope();

  // Put an instance of the new function on the stack.
  vm::lambda *l = fe.c.close();
  e.c.encode(inst::pushclosure);
  e.c.encode(inst::makefunc, l);
}

void fundec::prettyprint(ostream &out, int indent)
{
  prettyindent(out, indent);
  out << "fundec '" << *id << "'\n";

  fun.prettyprint(out, indent);
}

function *fundec::opType(function *f)
{
  function *ft = new function(primBoolean());
  ft->add(f);
  ft->add(f);

  return ft;
}

void fundec::addOps(coenv &e, function *f)
{
  function *ft = opType(f);
  e.e.addVar(getPos(), symbol::trans("=="),
      new varEntry(ft, new bltinAccess(run::boolFuncEq)),true);
  e.e.addVar(getPos(), symbol::trans("!="),
      new varEntry(ft, new bltinAccess(run::boolFuncNeq)),true);
}

void fundec::trans(coenv &e)
{
  transAsField(e,0);
}

void fundec::transAsField(coenv &e, record *r)
{
  function *ft = fun.getType(e, true);
  assert(ft);

  addOps(e,ft);
  
  // Give the variable a location.
  access *a = r ? r->allocField(e.c.isStatic(), e.c.getPermission()) :
                  e.c.allocLocal();
  varEntry *ent = new varEntry(ft, a);
  if (r)
    r->addVar(id, ent);
  e.e.addVar(getPos(), id, ent);

  // Push the function on to the stack.
  fun.trans(e);

  // Write the new function to the variable location.
  a->encodeWrite(getPos(), e.c);
  e.c.encode(inst::pop);
} 


void recorddec::prettyprint(ostream &out, int indent)
{
  prettyindent(out, indent);
  out << "structdec '" << *id << "'\n";

  body->prettyprint(out, indent+1);
}

function *recorddec::opType(record *r)
{
  function *ft = new function(primBoolean());
  ft->add(r);
  ft->add(r);

  return ft;
}

void recorddec::addOps(coenv &e, record *r)
{
  function *ft = opType(r);
  e.e.addVar(getPos(), symbol::trans("alias"),
      new varEntry(ft, new bltinAccess(run::boolMemEq)));
}

void recorddec::trans(coenv &e)
{
  transAsField(e,0);
}  

void recorddec::transAsField(coenv &e, record *parent)
{
  record *r = parent ? parent->newRecord(id, e.c.isStatic()) :
                       e.c.newRecord(id);
                     
  if (parent)
    parent->addType(id, r);
  e.e.addType(getPos(), id, r);
  addOps(e,r);

  // Start translating the initializer.
  coder c=e.c.newRecordInit(r);
  coenv re(c,e.e);
  
  body->transAsRecordBody(re, r);
}  

  
} // namespace absyntax
