#include "gentype.h"
#include "genname.h"
#include "../pkg/package.h"
#include "../type/reify.h"
#include <string.h>
#include <stdio.h>
#include <assert.h>

static bool codegen_struct(compile_t* c, LLVMTypeRef type, ast_t* def,
  ast_t* typeargs)
{
  ast_t* typeparams = ast_childidx(def, 1);
  ast_t* members = ast_childidx(def, 4);
  ast_t* member;

  member = ast_child(members);
  int count = 0;

  while(member != NULL)
  {
    switch(ast_id(member))
    {
      case TK_FVAR:
      case TK_FLET:
        count++;
        break;

      default: {}
    }

    member = ast_sibling(member);
  }

  LLVMTypeRef elements[count];
  member = ast_child(members);
  int index = 0;

  while(member != NULL)
  {
    switch(ast_id(member))
    {
      case TK_FVAR:
      case TK_FLET:
      {
        ast_t* ftype = ast_type(member);
        ftype = reify(ftype, typeparams, typeargs);
        LLVMTypeRef ltype = codegen_type(c, ftype);
        ast_free_unattached(ftype);

        if(ltype == NULL)
          return false;

        elements[index] = ltype;
        index++;
        break;
      }

      default: {}
    }

    member = ast_sibling(member);
  }

  LLVMStructSetBody(type, elements, count, false);

  // TODO: create a trace function
  return true;
}

static bool codegen_class(compile_t* c, LLVMTypeRef type, ast_t* def,
  ast_t* typeargs)
{
  if(!codegen_struct(c, type, def, typeargs))
    return false;

  // TODO: create a type descriptor
  return true;
}

static bool codegen_actor(compile_t* c, LLVMTypeRef type, ast_t* def,
  ast_t* typeargs)
{
  if(!codegen_struct(c, type, def, typeargs))
    return false;

  // TODO: create an actor descriptor, message type function, dispatch function
  return true;
}

static LLVMTypeRef codegen_nominal(compile_t* c, ast_t* ast)
{
  assert(ast_id(ast) == TK_NOMINAL);

  // check for primitive types
  const char* name = ast_name(ast_childidx(ast, 1));

  if(!strcmp(name, "True") || !strcmp(name, "False"))
    return LLVMInt1Type();

  if(!strcmp(name, "I8") || !strcmp(name, "U8"))
    return LLVMInt8Type();

  if(!strcmp(name, "I16") || !strcmp(name, "U16"))
    return LLVMInt16Type();

  if(!strcmp(name, "I32") || !strcmp(name, "U32"))
    return LLVMInt32Type();

  if(!strcmp(name, "I64") || !strcmp(name, "U64"))
    return LLVMInt64Type();

  if(!strcmp(name, "I128") || !strcmp(name, "U128"))
    return LLVMIntType(128);

  if(!strcmp(name, "F16"))
    return LLVMHalfType();

  if(!strcmp(name, "F32"))
    return LLVMFloatType();

  if(!strcmp(name, "F64"))
    return LLVMDoubleType();

  name = codegen_typename(ast);

  if(name == NULL)
    return NULL;

  LLVMTypeRef type = LLVMGetTypeByName(c->module, name);

  if(type != NULL)
    return LLVMPointerType(type, 0);

  type = LLVMStructCreateNamed(LLVMGetGlobalContext(), name);

  ast_t* def = ast_data(ast);
  ast_t* typeargs = ast_childidx(ast, 2);

  switch(ast_id(def))
  {
    case TK_TRAIT:
    {
      ast_error(ast, "not implemented (codegen for trait)");
      return NULL;
    }

    case TK_CLASS:
      if(!codegen_class(c, type, def, typeargs))
        return NULL;
      break;

    case TK_ACTOR:
      if(!codegen_actor(c, type, def, typeargs))
        return NULL;
      break;

    default:
      assert(0);
      return NULL;
  }

  return LLVMPointerType(type, 0);
}

static LLVMTypeRef codegen_union(compile_t* c, ast_t* ast)
{
  ast_t* left = ast_child(ast);
  ast_t* right = ast_sibling(left);

  LLVMTypeRef tleft = codegen_type(c, left);
  LLVMTypeRef tright = codegen_type(c, right);

  if(tleft == tright)
  {
    switch(LLVMGetTypeKind(tleft))
    {
      case LLVMIntegerTypeKind:
      {
        // (i1 | i1) => i1
        // this will occur for a Bool. integer types don't get unioned with
        // themselves in any other situation.
        assert(LLVMGetIntTypeWidth(tleft) == 1);
        return tleft;
      }

      case LLVMPointerTypeKind:
      {
        // TODO:
        break;
      }

      default:
      {
        // other types don't get unioned with themselves
        assert(0);
        return NULL;
      }
    }
  }

  ast_error(ast, "not implemented (codegen for uniontype)");
  return NULL;
}

/**
 * TODO: structural types
 * assemble a list of all structural types in the program
 * eliminate duplicates
 * for all reified types in the program, determine if they can be each
 * structural type. if they can, add the structural type (with a vtable) to the
 * list of types that a type can be.
 */

LLVMTypeRef codegen_type(compile_t* c, ast_t* ast)
{
  switch(ast_id(ast))
  {
    case TK_UNIONTYPE:
      return codegen_union(c, ast);

    case TK_ISECTTYPE:
    {
      ast_error(ast, "not implemented (codegen for isecttype)");
      return NULL;
    }

    case TK_TUPLETYPE:
    {
      ast_error(ast, "not implemented (codegen for tupletype)");
      return NULL;
    }

    case TK_NOMINAL:
      return codegen_nominal(c, ast);

    case TK_STRUCTURAL:
    {
      ast_error(ast, "not implemented (codegen for structural)");
      return NULL;
    }

    case TK_ARROW:
      return codegen_type(c, ast_childidx(ast, 1));

    case TK_TYPEPARAMREF:
    {
      ast_error(ast, "not implemented (codegen for typeparamref)");
      return NULL;
    }

    default: {}
  }

  assert(0);
  return NULL;
}
