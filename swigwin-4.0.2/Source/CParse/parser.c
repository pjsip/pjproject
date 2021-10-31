/* A Bison parser, made by GNU Bison 3.0.4.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "3.0.4"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1




/* Copy the first part of user declarations.  */
#line 25 "parser.y" /* yacc.c:339  */

#define yylex yylex

#include "swig.h"
#include "cparse.h"
#include "preprocessor.h"
#include <ctype.h>

/* We do this for portability */
#undef alloca
#define alloca malloc

/* -----------------------------------------------------------------------------
 *                               Externals
 * ----------------------------------------------------------------------------- */

int  yyparse();

/* NEW Variables */

static Node    *top = 0;      /* Top of the generated parse tree */
static int      unnamed = 0;  /* Unnamed datatype counter */
static Hash    *classes = 0;        /* Hash table of classes */
static Hash    *classes_typedefs = 0; /* Hash table of typedef classes: typedef struct X {...} Y; */
static Symtab  *prev_symtab = 0;
static Node    *current_class = 0;
String  *ModuleName = 0;
static Node    *module_node = 0;
static String  *Classprefix = 0;  
static String  *Namespaceprefix = 0;
static int      inclass = 0;
static Node    *currentOuterClass = 0; /* for nested classes */
static const char *last_cpptype = 0;
static int      inherit_list = 0;
static Parm    *template_parameters = 0;
static int      parsing_template_declaration = 0;
static int      extendmode   = 0;
static int      compact_default_args = 0;
static int      template_reduce = 0;
static int      cparse_externc = 0;
int		ignore_nested_classes = 0;
int		kwargs_supported = 0;
/* -----------------------------------------------------------------------------
 *                            Doxygen Comment Globals
 * ----------------------------------------------------------------------------- */
static String *currentDeclComment = NULL; /* Comment of C/C++ declaration. */
static Node *previousNode = NULL; /* Pointer to the previous node (for post comments) */
static Node *currentNode = NULL; /* Pointer to the current node (for post comments) */

/* -----------------------------------------------------------------------------
 *                            Assist Functions
 * ----------------------------------------------------------------------------- */


 
/* Called by the parser (yyparse) when an error is found.*/
static void yyerror (const char *e) {
  (void)e;
}

static Node *new_node(const_String_or_char_ptr tag) {
  Node *n = Swig_cparse_new_node(tag);
  /* Remember the previous node in case it will need a post-comment */
  previousNode = currentNode;
  currentNode = n;
  return n;
}

/* Copies a node.  Does not copy tree links or symbol table data (except for
   sym:name) */

static Node *copy_node(Node *n) {
  Node *nn;
  Iterator k;
  nn = NewHash();
  Setfile(nn,Getfile(n));
  Setline(nn,Getline(n));
  for (k = First(n); k.key; k = Next(k)) {
    String *ci;
    String *key = k.key;
    char *ckey = Char(key);
    if ((strcmp(ckey,"nextSibling") == 0) ||
	(strcmp(ckey,"previousSibling") == 0) ||
	(strcmp(ckey,"parentNode") == 0) ||
	(strcmp(ckey,"lastChild") == 0)) {
      continue;
    }
    if (Strncmp(key,"csym:",5) == 0) continue;
    /* We do copy sym:name.  For templates */
    if ((strcmp(ckey,"sym:name") == 0) || 
	(strcmp(ckey,"sym:weak") == 0) ||
	(strcmp(ckey,"sym:typename") == 0)) {
      String *ci = Copy(k.item);
      Setattr(nn,key, ci);
      Delete(ci);
      continue;
    }
    if (strcmp(ckey,"sym:symtab") == 0) {
      Setattr(nn,"sym:needs_symtab", "1");
    }
    /* We don't copy any other symbol table attributes */
    if (strncmp(ckey,"sym:",4) == 0) {
      continue;
    }
    /* If children.  We copy them recursively using this function */
    if (strcmp(ckey,"firstChild") == 0) {
      /* Copy children */
      Node *cn = k.item;
      while (cn) {
	Node *copy = copy_node(cn);
	appendChild(nn,copy);
	Delete(copy);
	cn = nextSibling(cn);
      }
      continue;
    }
    /* We don't copy the symbol table.  But we drop an attribute 
       requires_symtab so that functions know it needs to be built */

    if (strcmp(ckey,"symtab") == 0) {
      /* Node defined a symbol table. */
      Setattr(nn,"requires_symtab","1");
      continue;
    }
    /* Can't copy nodes */
    if (strcmp(ckey,"node") == 0) {
      continue;
    }
    if ((strcmp(ckey,"parms") == 0) || (strcmp(ckey,"pattern") == 0) || (strcmp(ckey,"throws") == 0)
	|| (strcmp(ckey,"kwargs") == 0)) {
      ParmList *pl = CopyParmList(k.item);
      Setattr(nn,key,pl);
      Delete(pl);
      continue;
    }
    if (strcmp(ckey,"nested:outer") == 0) { /* don't copy outer classes links, they will be updated later */
      Setattr(nn, key, k.item);
      continue;
    }
    /* defaultargs will be patched back in later in update_defaultargs() */
    if (strcmp(ckey,"defaultargs") == 0) {
      Setattr(nn, "needs_defaultargs", "1");
      continue;
    }
    /* same for abstracts, which contains pointers to the source node children, and so will need to be patch too */
    if (strcmp(ckey,"abstracts") == 0) {
      SetFlag(nn, "needs_abstracts");
      continue;
    }
    /* Looks okay.  Just copy the data using Copy */
    ci = Copy(k.item);
    Setattr(nn, key, ci);
    Delete(ci);
  }
  return nn;
}

static void set_comment(Node *n, String *comment) {
  String *name;
  Parm *p;
  if (!n || !comment)
    return;

  if (Getattr(n, "doxygen"))
    Append(Getattr(n, "doxygen"), comment);
  else {
    Setattr(n, "doxygen", comment);
    /* This is the first comment, populate it with @params, if any */
    p = Getattr(n, "parms");
    while (p) {
      if (Getattr(p, "doxygen"))
	Printv(comment, "\n@param ", Getattr(p, "name"), Getattr(p, "doxygen"), NIL);
      p=nextSibling(p);
    }
  }
  
  /* Append same comment to every generated overload */
  name = Getattr(n, "name");
  if (!name)
    return;
  n = nextSibling(n);
  while (n && Getattr(n, "name") && Strcmp(Getattr(n, "name"), name) == 0) {
    Setattr(n, "doxygen", comment);
    n = nextSibling(n);
  }
}

/* -----------------------------------------------------------------------------
 *                              Variables
 * ----------------------------------------------------------------------------- */

static char  *typemap_lang = 0;    /* Current language setting */

static int cplus_mode  = 0;

/* C++ modes */

#define  CPLUS_PUBLIC    1
#define  CPLUS_PRIVATE   2
#define  CPLUS_PROTECTED 3

/* include types */
static int   import_mode = 0;

void SWIG_typemap_lang(const char *tm_lang) {
  typemap_lang = Swig_copy_string(tm_lang);
}

void SWIG_cparse_set_compact_default_args(int defargs) {
  compact_default_args = defargs;
}

int SWIG_cparse_template_reduce(int treduce) {
  template_reduce = treduce;
  return treduce;  
}

/* -----------------------------------------------------------------------------
 *                           Assist functions
 * ----------------------------------------------------------------------------- */

static int promote_type(int t) {
  if (t <= T_UCHAR || t == T_CHAR || t == T_WCHAR) return T_INT;
  return t;
}

/* Perform type-promotion for binary operators */
static int promote(int t1, int t2) {
  t1 = promote_type(t1);
  t2 = promote_type(t2);
  return t1 > t2 ? t1 : t2;
}

static String *yyrename = 0;

/* Forward renaming operator */

static String *resolve_create_node_scope(String *cname, int is_class_definition);


Hash *Swig_cparse_features(void) {
  static Hash   *features_hash = 0;
  if (!features_hash) features_hash = NewHash();
  return features_hash;
}

/* Fully qualify any template parameters */
static String *feature_identifier_fix(String *s) {
  String *tp = SwigType_istemplate_templateprefix(s);
  if (tp) {
    String *ts, *ta, *tq;
    ts = SwigType_templatesuffix(s);
    ta = SwigType_templateargs(s);
    tq = Swig_symbol_type_qualify(ta,0);
    Append(tp,tq);
    Append(tp,ts);
    Delete(ts);
    Delete(ta);
    Delete(tq);
    return tp;
  } else {
    return NewString(s);
  }
}

static void set_access_mode(Node *n) {
  if (cplus_mode == CPLUS_PUBLIC)
    Setattr(n, "access", "public");
  else if (cplus_mode == CPLUS_PROTECTED)
    Setattr(n, "access", "protected");
  else
    Setattr(n, "access", "private");
}

static void restore_access_mode(Node *n) {
  String *mode = Getattr(n, "access");
  if (Strcmp(mode, "private") == 0)
    cplus_mode = CPLUS_PRIVATE;
  else if (Strcmp(mode, "protected") == 0)
    cplus_mode = CPLUS_PROTECTED;
  else
    cplus_mode = CPLUS_PUBLIC;
}

/* Generate the symbol table name for an object */
/* This is a bit of a mess. Need to clean up */
static String *add_oldname = 0;



static String *make_name(Node *n, String *name,SwigType *decl) {
  String *made_name = 0;
  int destructor = name && (*(Char(name)) == '~');

  if (yyrename) {
    String *s = NewString(yyrename);
    Delete(yyrename);
    yyrename = 0;
    if (destructor  && (*(Char(s)) != '~')) {
      Insert(s,0,"~");
    }
    return s;
  }

  if (!name) return 0;

  if (parsing_template_declaration)
    SetFlag(n, "parsing_template_declaration");
  made_name = Swig_name_make(n, Namespaceprefix, name, decl, add_oldname);
  Delattr(n, "parsing_template_declaration");

  return made_name;
}

/* Generate an unnamed identifier */
static String *make_unnamed() {
  unnamed++;
  return NewStringf("$unnamed%d$",unnamed);
}

/* Return if the node is a friend declaration */
static int is_friend(Node *n) {
  return Cmp(Getattr(n,"storage"),"friend") == 0;
}

static int is_operator(String *name) {
  return Strncmp(name,"operator ", 9) == 0;
}


/* Add declaration list to symbol table */
static int  add_only_one = 0;

static void add_symbols(Node *n) {
  String *decl;
  String *wrn = 0;

  if (inclass && n) {
    cparse_normalize_void(n);
  }
  while (n) {
    String *symname = 0;
    /* for friends, we need to pop the scope once */
    String *old_prefix = 0;
    Symtab *old_scope = 0;
    int isfriend = inclass && is_friend(n);
    int iscdecl = Cmp(nodeType(n),"cdecl") == 0;
    int only_csymbol = 0;
    
    if (inclass) {
      String *name = Getattr(n, "name");
      if (isfriend) {
	/* for friends, we need to add the scopename if needed */
	String *prefix = name ? Swig_scopename_prefix(name) : 0;
	old_prefix = Namespaceprefix;
	old_scope = Swig_symbol_popscope();
	Namespaceprefix = Swig_symbol_qualifiedscopename(0);
	if (!prefix) {
	  if (name && !is_operator(name) && Namespaceprefix) {
	    String *nname = NewStringf("%s::%s", Namespaceprefix, name);
	    Setattr(n,"name",nname);
	    Delete(nname);
	  }
	} else {
	  Symtab *st = Swig_symbol_getscope(prefix);
	  String *ns = st ? Getattr(st,"name") : prefix;
	  String *base  = Swig_scopename_last(name);
	  String *nname = NewStringf("%s::%s", ns, base);
	  Setattr(n,"name",nname);
	  Delete(nname);
	  Delete(base);
	  Delete(prefix);
	}
	Namespaceprefix = 0;
      } else {
	/* for member functions, we need to remove the redundant
	   class scope if provided, as in
	   
	   struct Foo {
	   int Foo::method(int a);
	   };
	   
	*/
	String *prefix = name ? Swig_scopename_prefix(name) : 0;
	if (prefix) {
	  if (Classprefix && (Equal(prefix,Classprefix))) {
	    String *base = Swig_scopename_last(name);
	    Setattr(n,"name",base);
	    Delete(base);
	  }
	  Delete(prefix);
	}
      }
    }

    if (!isfriend && (inclass || extendmode)) {
      Setattr(n,"ismember","1");
    }

    if (extendmode) {
      if (!Getattr(n, "template"))
        SetFlag(n,"isextendmember");
    }

    if (!isfriend && inclass) {
      if ((cplus_mode != CPLUS_PUBLIC)) {
	only_csymbol = 1;
	if (cplus_mode == CPLUS_PROTECTED) {
	  Setattr(n,"access", "protected");
	  only_csymbol = !Swig_need_protected(n);
	} else {
	  Setattr(n,"access", "private");
	  /* private are needed only when they are pure virtuals - why? */
	  if ((Cmp(Getattr(n,"storage"),"virtual") == 0) && (Cmp(Getattr(n,"value"),"0") == 0)) {
	    only_csymbol = 0;
	  }
	  if (Cmp(nodeType(n),"destructor") == 0) {
	    /* Needed for "unref" feature */
	    only_csymbol = 0;
	  }
	}
      } else {
	  Setattr(n,"access", "public");
      }
    }
    if (Getattr(n,"sym:name")) {
      n = nextSibling(n);
      continue;
    }
    decl = Getattr(n,"decl");
    if (!SwigType_isfunction(decl)) {
      String *name = Getattr(n,"name");
      String *makename = Getattr(n,"parser:makename");
      if (iscdecl) {	
	String *storage = Getattr(n, "storage");
	if (Cmp(storage,"typedef") == 0) {
	  Setattr(n,"kind","typedef");
	} else {
	  SwigType *type = Getattr(n,"type");
	  String *value = Getattr(n,"value");
	  Setattr(n,"kind","variable");
	  if (value && Len(value)) {
	    Setattr(n,"hasvalue","1");
	  }
	  if (type) {
	    SwigType *ty;
	    SwigType *tmp = 0;
	    if (decl) {
	      ty = tmp = Copy(type);
	      SwigType_push(ty,decl);
	    } else {
	      ty = type;
	    }
	    if (!SwigType_ismutable(ty) || (storage && Strstr(storage, "constexpr"))) {
	      SetFlag(n,"hasconsttype");
	      SetFlag(n,"feature:immutable");
	    }
	    if (tmp) Delete(tmp);
	  }
	  if (!type) {
	    Printf(stderr,"notype name %s\n", name);
	  }
	}
      }
      Swig_features_get(Swig_cparse_features(), Namespaceprefix, name, 0, n);
      if (makename) {
	symname = make_name(n, makename,0);
        Delattr(n,"parser:makename"); /* temporary information, don't leave it hanging around */
      } else {
        makename = name;
	symname = make_name(n, makename,0);
      }
      
      if (!symname) {
	symname = Copy(Getattr(n,"unnamed"));
      }
      if (symname) {
	if (parsing_template_declaration)
	  SetFlag(n, "parsing_template_declaration");
	wrn = Swig_name_warning(n, Namespaceprefix, symname,0);
	Delattr(n, "parsing_template_declaration");
      }
    } else {
      String *name = Getattr(n,"name");
      SwigType *fdecl = Copy(decl);
      SwigType *fun = SwigType_pop_function(fdecl);
      if (iscdecl) {	
	Setattr(n,"kind","function");
      }
      
      Swig_features_get(Swig_cparse_features(),Namespaceprefix,name,fun,n);

      symname = make_name(n, name,fun);
      if (parsing_template_declaration)
	SetFlag(n, "parsing_template_declaration");
      wrn = Swig_name_warning(n, Namespaceprefix,symname,fun);
      Delattr(n, "parsing_template_declaration");
      
      Delete(fdecl);
      Delete(fun);
      
    }
    if (!symname) {
      n = nextSibling(n);
      continue;
    }
    if (cparse_cplusplus) {
      String *value = Getattr(n, "value");
      if (value && Strcmp(value, "delete") == 0) {
	/* C++11 deleted definition / deleted function */
        SetFlag(n,"deleted");
        SetFlag(n,"feature:ignore");
      }
      if (SwigType_isrvalue_reference(Getattr(n, "refqualifier"))) {
	/* Ignore rvalue ref-qualifiers by default
	 * Use Getattr instead of GetFlag to handle explicit ignore and explicit not ignore */
	if (!(Getattr(n, "feature:ignore") || Strncmp(symname, "$ignore", 7) == 0)) {
	  SWIG_WARN_NODE_BEGIN(n);
	  Swig_warning(WARN_TYPE_RVALUE_REF_QUALIFIER_IGNORED, Getfile(n), Getline(n),
	      "Method with rvalue ref-qualifier %s ignored.\n", Swig_name_decl(n));
	  SWIG_WARN_NODE_END(n);
	  SetFlag(n, "feature:ignore");
	}
      }
    }
    if (only_csymbol || GetFlag(n, "feature:ignore") || Strncmp(symname, "$ignore", 7) == 0) {
      /* Only add to C symbol table and continue */
      Swig_symbol_add(0, n);
      if (!only_csymbol && !GetFlag(n, "feature:ignore")) {
	/* Print the warning attached to $ignore name, if any */
        char *c = Char(symname) + 7;
	if (strlen(c)) {
	  SWIG_WARN_NODE_BEGIN(n);
	  Swig_warning(0,Getfile(n), Getline(n), "%s\n",c+1);
	  SWIG_WARN_NODE_END(n);
	}
	/* If the symbol was ignored via "rename" and is visible, set also feature:ignore*/
	SetFlag(n, "feature:ignore");
      }
      if (!GetFlag(n, "feature:ignore") && Strcmp(symname,"$ignore") == 0) {
	/* Add feature:ignore if the symbol was explicitly ignored, regardless of visibility */
	SetFlag(n, "feature:ignore");
      }
    } else {
      Node *c;
      if ((wrn) && (Len(wrn))) {
	String *metaname = symname;
	if (!Getmeta(metaname,"already_warned")) {
	  SWIG_WARN_NODE_BEGIN(n);
	  Swig_warning(0,Getfile(n),Getline(n), "%s\n", wrn);
	  SWIG_WARN_NODE_END(n);
	  Setmeta(metaname,"already_warned","1");
	}
      }
      c = Swig_symbol_add(symname,n);

      if (c != n) {
        /* symbol conflict attempting to add in the new symbol */
        if (Getattr(n,"sym:weak")) {
          Setattr(n,"sym:name",symname);
        } else {
          String *e = NewStringEmpty();
          String *en = NewStringEmpty();
          String *ec = NewStringEmpty();
          int redefined = Swig_need_redefined_warn(n,c,inclass);
          if (redefined) {
            Printf(en,"Identifier '%s' redefined (ignored)",symname);
            Printf(ec,"previous definition of '%s'",symname);
          } else {
            Printf(en,"Redundant redeclaration of '%s'",symname);
            Printf(ec,"previous declaration of '%s'",symname);
          }
          if (Cmp(symname,Getattr(n,"name"))) {
            Printf(en," (Renamed from '%s')", SwigType_namestr(Getattr(n,"name")));
          }
          Printf(en,",");
          if (Cmp(symname,Getattr(c,"name"))) {
            Printf(ec," (Renamed from '%s')", SwigType_namestr(Getattr(c,"name")));
          }
          Printf(ec,".");
	  SWIG_WARN_NODE_BEGIN(n);
          if (redefined) {
            Swig_warning(WARN_PARSE_REDEFINED,Getfile(n),Getline(n),"%s\n",en);
            Swig_warning(WARN_PARSE_REDEFINED,Getfile(c),Getline(c),"%s\n",ec);
          } else if (!is_friend(n) && !is_friend(c)) {
            Swig_warning(WARN_PARSE_REDUNDANT,Getfile(n),Getline(n),"%s\n",en);
            Swig_warning(WARN_PARSE_REDUNDANT,Getfile(c),Getline(c),"%s\n",ec);
          }
	  SWIG_WARN_NODE_END(n);
          Printf(e,"%s:%d:%s\n%s:%d:%s\n",Getfile(n),Getline(n),en,
                 Getfile(c),Getline(c),ec);
          Setattr(n,"error",e);
	  Delete(e);
          Delete(en);
          Delete(ec);
        }
      }
    }
    /* restore the class scope if needed */
    if (isfriend) {
      Swig_symbol_setscope(old_scope);
      if (old_prefix) {
	Delete(Namespaceprefix);
	Namespaceprefix = old_prefix;
      }
    }
    Delete(symname);

    if (add_only_one) return;
    n = nextSibling(n);
  }
}


/* add symbols a parse tree node copy */

static void add_symbols_copy(Node *n) {
  String *name;
  int    emode = 0;
  while (n) {
    char *cnodeType = Char(nodeType(n));

    if (strcmp(cnodeType,"access") == 0) {
      String *kind = Getattr(n,"kind");
      if (Strcmp(kind,"public") == 0) {
	cplus_mode = CPLUS_PUBLIC;
      } else if (Strcmp(kind,"private") == 0) {
	cplus_mode = CPLUS_PRIVATE;
      } else if (Strcmp(kind,"protected") == 0) {
	cplus_mode = CPLUS_PROTECTED;
      }
      n = nextSibling(n);
      continue;
    }

    add_oldname = Getattr(n,"sym:name");
    if ((add_oldname) || (Getattr(n,"sym:needs_symtab"))) {
      int old_inclass = -1;
      Node *old_current_class = 0;
      if (add_oldname) {
	DohIncref(add_oldname);
	/*  Disable this, it prevents %rename to work with templates */
	/* If already renamed, we used that name  */
	/*
	if (Strcmp(add_oldname, Getattr(n,"name")) != 0) {
	  Delete(yyrename);
	  yyrename = Copy(add_oldname);
	}
	*/
      }
      Delattr(n,"sym:needs_symtab");
      Delattr(n,"sym:name");

      add_only_one = 1;
      add_symbols(n);

      if (Getattr(n,"partialargs")) {
	Swig_symbol_cadd(Getattr(n,"partialargs"),n);
      }
      add_only_one = 0;
      name = Getattr(n,"name");
      if (Getattr(n,"requires_symtab")) {
	Swig_symbol_newscope();
	Swig_symbol_setscopename(name);
	Delete(Namespaceprefix);
	Namespaceprefix = Swig_symbol_qualifiedscopename(0);
      }
      if (strcmp(cnodeType,"class") == 0) {
	old_inclass = inclass;
	inclass = 1;
	old_current_class = current_class;
	current_class = n;
	if (Strcmp(Getattr(n,"kind"),"class") == 0) {
	  cplus_mode = CPLUS_PRIVATE;
	} else {
	  cplus_mode = CPLUS_PUBLIC;
	}
      }
      if (strcmp(cnodeType,"extend") == 0) {
	emode = cplus_mode;
	cplus_mode = CPLUS_PUBLIC;
      }
      add_symbols_copy(firstChild(n));
      if (strcmp(cnodeType,"extend") == 0) {
	cplus_mode = emode;
      }
      if (Getattr(n,"requires_symtab")) {
	Setattr(n,"symtab", Swig_symbol_popscope());
	Delattr(n,"requires_symtab");
	Delete(Namespaceprefix);
	Namespaceprefix = Swig_symbol_qualifiedscopename(0);
      }
      if (add_oldname) {
	Delete(add_oldname);
	add_oldname = 0;
      }
      if (strcmp(cnodeType,"class") == 0) {
	inclass = old_inclass;
	current_class = old_current_class;
      }
    } else {
      if (strcmp(cnodeType,"extend") == 0) {
	emode = cplus_mode;
	cplus_mode = CPLUS_PUBLIC;
      }
      add_symbols_copy(firstChild(n));
      if (strcmp(cnodeType,"extend") == 0) {
	cplus_mode = emode;
      }
    }
    n = nextSibling(n);
  }
}

/* Add in the "defaultargs" attribute for functions in instantiated templates.
 * n should be any instantiated template (class or start of linked list of functions). */
static void update_defaultargs(Node *n) {
  if (n) {
    Node *firstdefaultargs = n;
    update_defaultargs(firstChild(n));
    n = nextSibling(n);
    /* recursively loop through nodes of all types, but all we really need are the overloaded functions */
    while (n) {
      update_defaultargs(firstChild(n));
      if (!Getattr(n, "defaultargs")) {
	if (Getattr(n, "needs_defaultargs")) {
	  Setattr(n, "defaultargs", firstdefaultargs);
	  Delattr(n, "needs_defaultargs");
	} else {
	  firstdefaultargs = n;
	}
      } else {
	/* Functions added in with %extend (for specialized template classes) will already have default args patched up */
	assert(Getattr(n, "defaultargs") == firstdefaultargs);
      }
      n = nextSibling(n);
    }
  }
}

/* Check a set of declarations to see if any are pure-abstract */

static List *pure_abstracts(Node *n) {
  List *abstracts = 0;
  while (n) {
    if (Cmp(nodeType(n),"cdecl") == 0) {
      String *decl = Getattr(n,"decl");
      if (SwigType_isfunction(decl)) {
	String *init = Getattr(n,"value");
	if (Cmp(init,"0") == 0) {
	  if (!abstracts) {
	    abstracts = NewList();
	  }
	  Append(abstracts,n);
	  SetFlag(n,"abstract");
	}
      }
    } else if (Cmp(nodeType(n),"destructor") == 0) {
      if (Cmp(Getattr(n,"value"),"0") == 0) {
	if (!abstracts) {
	  abstracts = NewList();
	}
	Append(abstracts,n);
	SetFlag(n,"abstract");
      }
    }
    n = nextSibling(n);
  }
  return abstracts;
}

/* Recompute the "abstracts" attribute for the classes in instantiated templates, similarly to update_defaultargs() above. */
static void update_abstracts(Node *n) {
  for (; n; n = nextSibling(n)) {
    Node* const child = firstChild(n);
    if (!child)
      continue;

    update_abstracts(child);

    if (Getattr(n, "needs_abstracts")) {
      Setattr(n, "abstracts", pure_abstracts(child));
      Delattr(n, "needs_abstracts");
    }
  }
}

/* Make a classname */

static String *make_class_name(String *name) {
  String *nname = 0;
  String *prefix;
  if (Namespaceprefix) {
    nname= NewStringf("%s::%s", Namespaceprefix, name);
  } else {
    nname = NewString(name);
  }
  prefix = SwigType_istemplate_templateprefix(nname);
  if (prefix) {
    String *args, *qargs;
    args   = SwigType_templateargs(nname);
    qargs  = Swig_symbol_type_qualify(args,0);
    Append(prefix,qargs);
    Delete(nname);
    Delete(args);
    Delete(qargs);
    nname = prefix;
  }
  return nname;
}

/* Use typedef name as class name */

static void add_typedef_name(Node *n, Node *declnode, String *oldName, Symtab *cscope, String *scpname) {
  String *class_rename = 0;
  SwigType *decl = Getattr(declnode, "decl");
  if (!decl || !Len(decl)) {
    String *cname;
    String *tdscopename;
    String *class_scope = Swig_symbol_qualifiedscopename(cscope);
    String *name = Getattr(declnode, "name");
    cname = Copy(name);
    Setattr(n, "tdname", cname);
    tdscopename = class_scope ? NewStringf("%s::%s", class_scope, name) : Copy(name);
    class_rename = Getattr(n, "class_rename");
    if (class_rename && (Strcmp(class_rename, oldName) == 0))
      Setattr(n, "class_rename", NewString(name));
    if (!classes_typedefs) classes_typedefs = NewHash();
    if (!Equal(scpname, tdscopename) && !Getattr(classes_typedefs, tdscopename)) {
      Setattr(classes_typedefs, tdscopename, n);
    }
    Setattr(n, "decl", decl);
    Delete(class_scope);
    Delete(cname);
    Delete(tdscopename);
  }
}

/* If the class name is qualified.  We need to create or lookup namespace entries */

static Symtab *set_scope_to_global() {
  Symtab *symtab = Swig_symbol_global_scope();
  Swig_symbol_setscope(symtab);
  return symtab;
}
 
/* Remove the block braces, { and }, if the 'noblock' attribute is set.
 * Node *kw can be either a Hash or Parmlist. */
static String *remove_block(Node *kw, const String *inputcode) {
  String *modified_code = 0;
  while (kw) {
   String *name = Getattr(kw,"name");
   if (name && (Cmp(name,"noblock") == 0)) {
     char *cstr = Char(inputcode);
     int len = Len(inputcode);
     if (len && cstr[0] == '{') {
       --len; ++cstr; 
       if (len && cstr[len - 1] == '}') { --len; }
       /* we now remove the extra spaces */
       while (len && isspace((int)cstr[0])) { --len; ++cstr; }
       while (len && isspace((int)cstr[len - 1])) { --len; }
       modified_code = NewStringWithSize(cstr, len);
       break;
     }
   }
   kw = nextSibling(kw);
  }
  return modified_code;
}

/*
#define RESOLVE_DEBUG 1
*/
static Node *nscope = 0;
static Node *nscope_inner = 0;

/* Remove the scope prefix from cname and return the base name without the prefix.
 * The scopes required for the symbol name are resolved and/or created, if required.
 * For example AA::BB::CC as input returns CC and creates the namespace AA then inner 
 * namespace BB in the current scope. */
static String *resolve_create_node_scope(String *cname, int is_class_definition) {
  Symtab *gscope = 0;
  Node *cname_node = 0;
  String *last = Swig_scopename_last(cname);
  nscope = 0;
  nscope_inner = 0;  

  if (Strncmp(cname,"::" ,2) != 0) {
    if (is_class_definition) {
      /* Only lookup symbols which are in scope via a using declaration but not via a using directive.
         For example find y via 'using x::y' but not y via a 'using namespace x'. */
      cname_node = Swig_symbol_clookup_no_inherit(cname, 0);
      if (!cname_node) {
	Node *full_lookup_node = Swig_symbol_clookup(cname, 0);
	if (full_lookup_node) {
	 /* This finds a symbol brought into scope via both a using directive and a using declaration. */
	  Node *last_node = Swig_symbol_clookup_no_inherit(last, 0);
	  if (last_node == full_lookup_node)
	    cname_node = last_node;
	}
      }
    } else {
      /* For %template, the template needs to be in scope via any means. */
      cname_node = Swig_symbol_clookup(cname, 0);
    }
  }
#if RESOLVE_DEBUG
  if (!cname_node)
    Printf(stdout, "symbol does not yet exist (%d): [%s]\n", is_class_definition, cname);
  else
    Printf(stdout, "symbol does exist (%d): [%s]\n", is_class_definition, cname);
#endif

  if (cname_node) {
    /* The symbol has been defined already or is in another scope.
       If it is a weak symbol, it needs replacing and if it was brought into the current scope,
       the scope needs adjusting appropriately for the new symbol.
       Similarly for defined templates. */
    Symtab *symtab = Getattr(cname_node, "sym:symtab");
    Node *sym_weak = Getattr(cname_node, "sym:weak");
    if ((symtab && sym_weak) || Equal(nodeType(cname_node), "template")) {
      /* Check if the scope is the current scope */
      String *current_scopename = Swig_symbol_qualifiedscopename(0);
      String *found_scopename = Swig_symbol_qualifiedscopename(symtab);
      if (!current_scopename)
	current_scopename = NewString("");
      if (!found_scopename)
	found_scopename = NewString("");

      {
	int fail = 1;
	List *current_scopes = Swig_scopename_tolist(current_scopename);
	List *found_scopes = Swig_scopename_tolist(found_scopename);
        Iterator cit = First(current_scopes);
	Iterator fit = First(found_scopes);
#if RESOLVE_DEBUG
Printf(stdout, "comparing current: [%s] found: [%s]\n", current_scopename, found_scopename);
#endif
	for (; fit.item && cit.item; fit = Next(fit), cit = Next(cit)) {
	  String *current = cit.item;
	  String *found = fit.item;
#if RESOLVE_DEBUG
	  Printf(stdout, "  looping %s %s\n", current, found);
#endif
	  if (Strcmp(current, found) != 0)
	    break;
	}

	if (!cit.item) {
	  String *subscope = NewString("");
	  for (; fit.item; fit = Next(fit)) {
	    if (Len(subscope) > 0)
	      Append(subscope, "::");
	    Append(subscope, fit.item);
	  }
	  if (Len(subscope) > 0)
	    cname = NewStringf("%s::%s", subscope, last);
	  else
	    cname = Copy(last);
#if RESOLVE_DEBUG
	  Printf(stdout, "subscope to create: [%s] cname: [%s]\n", subscope, cname);
#endif
	  fail = 0;
	  Delete(subscope);
	} else {
	  if (is_class_definition) {
	    if (!fit.item) {
	      /* It is valid to define a new class with the same name as one forward declared in a parent scope */
	      fail = 0;
	    } else if (Swig_scopename_check(cname)) {
	      /* Classes defined with scope qualifiers must have a matching forward declaration in matching scope */
	      fail = 1;
	    } else {
	      /* This may let through some invalid cases */
	      fail = 0;
	    }
#if RESOLVE_DEBUG
	    Printf(stdout, "scope for class definition, fail: %d\n", fail);
#endif
	  } else {
#if RESOLVE_DEBUG
	    Printf(stdout, "no matching base scope for template\n");
#endif
	    fail = 1;
	  }
	}

	Delete(found_scopes);
	Delete(current_scopes);

	if (fail) {
	  String *cname_resolved = NewStringf("%s::%s", found_scopename, last);
	  Swig_error(cparse_file, cparse_line, "'%s' resolves to '%s' and was incorrectly instantiated in scope '%s' instead of within scope '%s'.\n", cname, cname_resolved, current_scopename, found_scopename);
	  cname = Copy(last);
	  Delete(cname_resolved);
	}
      }

      Delete(current_scopename);
      Delete(found_scopename);
    }
  } else if (!is_class_definition) {
    /* A template instantiation requires a template to be found in scope... fail here too?
    Swig_error(cparse_file, cparse_line, "No template found to instantiate '%s' with %%template.\n", cname);
     */
  }

  if (Swig_scopename_check(cname)) {
    Node   *ns;
    String *prefix = Swig_scopename_prefix(cname);
    if (prefix && (Strncmp(prefix,"::",2) == 0)) {
/* I don't think we can use :: global scope to declare classes and hence neither %template. - consider reporting error instead - wsfulton. */
      /* Use the global scope */
      String *nprefix = NewString(Char(prefix)+2);
      Delete(prefix);
      prefix= nprefix;
      gscope = set_scope_to_global();
    }
    if (Len(prefix) == 0) {
      String *base = Copy(last);
      /* Use the global scope, but we need to add a 'global' namespace.  */
      if (!gscope) gscope = set_scope_to_global();
      /* note that this namespace is not the "unnamed" one,
	 and we don't use Setattr(nscope,"name", ""),
	 because the unnamed namespace is private */
      nscope = new_node("namespace");
      Setattr(nscope,"symtab", gscope);;
      nscope_inner = nscope;
      Delete(last);
      return base;
    }
    /* Try to locate the scope */
    ns = Swig_symbol_clookup(prefix,0);
    if (!ns) {
      Swig_error(cparse_file,cparse_line,"Undefined scope '%s'\n", prefix);
    } else {
      Symtab *nstab = Getattr(ns,"symtab");
      if (!nstab) {
	Swig_error(cparse_file,cparse_line, "'%s' is not defined as a valid scope.\n", prefix);
	ns = 0;
      } else {
	/* Check if the node scope is the current scope */
	String *tname = Swig_symbol_qualifiedscopename(0);
	String *nname = Swig_symbol_qualifiedscopename(nstab);
	if (tname && (Strcmp(tname,nname) == 0)) {
	  ns = 0;
	  cname = Copy(last);
	}
	Delete(tname);
	Delete(nname);
      }
      if (ns) {
	/* we will try to create a new node using the namespaces we
	   can find in the scope name */
	List *scopes = Swig_scopename_tolist(prefix);
	String *sname;
	Iterator si;

	for (si = First(scopes); si.item; si = Next(si)) {
	  Node *ns1,*ns2;
	  sname = si.item;
	  ns1 = Swig_symbol_clookup(sname,0);
	  assert(ns1);
	  if (Strcmp(nodeType(ns1),"namespace") == 0) {
	    if (Getattr(ns1,"alias")) {
	      ns1 = Getattr(ns1,"namespace");
	    }
	  } else {
	    /* now this last part is a class */
	    si = Next(si);
	    /*  or a nested class tree, which is unrolled here */
	    for (; si.item; si = Next(si)) {
	      if (si.item) {
		Printf(sname,"::%s",si.item);
	      }
	    }
	    /* we get the 'inner' class */
	    nscope_inner = Swig_symbol_clookup(sname,0);
	    /* set the scope to the inner class */
	    Swig_symbol_setscope(Getattr(nscope_inner,"symtab"));
	    /* save the last namespace prefix */
	    Delete(Namespaceprefix);
	    Namespaceprefix = Swig_symbol_qualifiedscopename(0);
	    /* and return the node name, including the inner class prefix */
	    break;
	  }
	  /* here we just populate the namespace tree as usual */
	  ns2 = new_node("namespace");
	  Setattr(ns2,"name",sname);
	  Setattr(ns2,"symtab", Getattr(ns1,"symtab"));
	  add_symbols(ns2);
	  Swig_symbol_setscope(Getattr(ns1,"symtab"));
	  Delete(Namespaceprefix);
	  Namespaceprefix = Swig_symbol_qualifiedscopename(0);
	  if (nscope_inner) {
	    if (Getattr(nscope_inner,"symtab") != Getattr(ns2,"symtab")) {
	      appendChild(nscope_inner,ns2);
	      Delete(ns2);
	    }
	  }
	  nscope_inner = ns2;
	  if (!nscope) nscope = ns2;
	}
	cname = Copy(last);
	Delete(scopes);
      }
    }
    Delete(prefix);
  }
  Delete(last);

  return cname;
}
 
/* look for simple typedef name in typedef list */
static String *try_to_find_a_name_for_unnamed_structure(const char *storage, Node *decls) {
  String *name = 0;
  Node *n = decls;
  if (storage && (strcmp(storage, "typedef") == 0)) {
    for (; n; n = nextSibling(n)) {
      if (!Len(Getattr(n, "decl"))) {
	name = Copy(Getattr(n, "name"));
	break;
      }
    }
  }
  return name;
}

/* traverse copied tree segment, and update outer class links*/
static void update_nested_classes(Node *n)
{
  Node *c = firstChild(n);
  while (c) {
    if (Getattr(c, "nested:outer"))
      Setattr(c, "nested:outer", n);
    update_nested_classes(c);
    c = nextSibling(c);
  }
}

/* -----------------------------------------------------------------------------
 * nested_forward_declaration()
 * 
 * Nested struct handling for C++ code if the nested classes are disabled.
 * Create the nested class/struct/union as a forward declaration.
 * ----------------------------------------------------------------------------- */

static Node *nested_forward_declaration(const char *storage, const char *kind, String *sname, String *name, Node *cpp_opt_declarators) {
  Node *nn = 0;

  if (sname) {
    /* Add forward declaration of the nested type */
    Node *n = new_node("classforward");
    Setattr(n, "kind", kind);
    Setattr(n, "name", sname);
    Setattr(n, "storage", storage);
    Setattr(n, "sym:weak", "1");
    add_symbols(n);
    nn = n;
  }

  /* Add any variable instances. Also add in any further typedefs of the nested type.
     Note that anonymous typedefs (eg typedef struct {...} a, b;) are treated as class forward declarations */
  if (cpp_opt_declarators) {
    int storage_typedef = (storage && (strcmp(storage, "typedef") == 0));
    int variable_of_anonymous_type = !sname && !storage_typedef;
    if (!variable_of_anonymous_type) {
      int anonymous_typedef = !sname && (storage && (strcmp(storage, "typedef") == 0));
      Node *n = cpp_opt_declarators;
      SwigType *type = name;
      while (n) {
	Setattr(n, "type", type);
	Setattr(n, "storage", storage);
	if (anonymous_typedef) {
	  Setattr(n, "nodeType", "classforward");
	  Setattr(n, "sym:weak", "1");
	}
	n = nextSibling(n);
      }
      add_symbols(cpp_opt_declarators);

      if (nn) {
	set_nextSibling(nn, cpp_opt_declarators);
      } else {
	nn = cpp_opt_declarators;
      }
    }
  }

  if (!currentOuterClass || !GetFlag(currentOuterClass, "nested")) {
    if (nn && Equal(nodeType(nn), "classforward")) {
      Node *n = nn;
      if (!GetFlag(n, "feature:ignore")) {
	SWIG_WARN_NODE_BEGIN(n);
	Swig_warning(WARN_PARSE_NAMED_NESTED_CLASS, cparse_file, cparse_line,"Nested %s not currently supported (%s ignored)\n", kind, sname ? sname : name);
	SWIG_WARN_NODE_END(n);
      }
    } else {
      Swig_warning(WARN_PARSE_UNNAMED_NESTED_CLASS, cparse_file, cparse_line, "Nested %s not currently supported (ignored).\n", kind);
    }
  }

  return nn;
}


Node *Swig_cparse(File *f) {
  scanner_file(f);
  top = 0;
  yyparse();
  return top;
}

static void single_new_feature(const char *featurename, String *val, Hash *featureattribs, char *declaratorid, SwigType *type, ParmList *declaratorparms, String *qualifier) {
  String *fname;
  String *name;
  String *fixname;
  SwigType *t = Copy(type);

  /* Printf(stdout, "single_new_feature: [%s] [%s] [%s] [%s] [%s] [%s]\n", featurename, val, declaratorid, t, ParmList_str_defaultargs(declaratorparms), qualifier); */

  /* Warn about deprecated features */
  if (strcmp(featurename, "nestedworkaround") == 0)
    Swig_warning(WARN_DEPRECATED_NESTED_WORKAROUND, cparse_file, cparse_line, "The 'nestedworkaround' feature is deprecated.\n");

  fname = NewStringf("feature:%s",featurename);
  if (declaratorid) {
    fixname = feature_identifier_fix(declaratorid);
  } else {
    fixname = NewStringEmpty();
  }
  if (Namespaceprefix) {
    name = NewStringf("%s::%s",Namespaceprefix, fixname);
  } else {
    name = fixname;
  }

  if (declaratorparms) Setmeta(val,"parms",declaratorparms);
  if (!Len(t)) t = 0;
  if (t) {
    if (qualifier) SwigType_push(t,qualifier);
    if (SwigType_isfunction(t)) {
      SwigType *decl = SwigType_pop_function(t);
      if (SwigType_ispointer(t)) {
	String *nname = NewStringf("*%s",name);
	Swig_feature_set(Swig_cparse_features(), nname, decl, fname, val, featureattribs);
	Delete(nname);
      } else {
	Swig_feature_set(Swig_cparse_features(), name, decl, fname, val, featureattribs);
      }
      Delete(decl);
    } else if (SwigType_ispointer(t)) {
      String *nname = NewStringf("*%s",name);
      Swig_feature_set(Swig_cparse_features(),nname,0,fname,val, featureattribs);
      Delete(nname);
    }
  } else {
    /* Global feature, that is, feature not associated with any particular symbol */
    Swig_feature_set(Swig_cparse_features(),name,0,fname,val, featureattribs);
  }
  Delete(fname);
  Delete(name);
}

/* Add a new feature to the Hash. Additional features are added if the feature has a parameter list (declaratorparms)
 * and one or more of the parameters have a default argument. An extra feature is added for each defaulted parameter,
 * simulating the equivalent overloaded method. */
static void new_feature(const char *featurename, String *val, Hash *featureattribs, char *declaratorid, SwigType *type, ParmList *declaratorparms, String *qualifier) {

  ParmList *declparms = declaratorparms;

  /* remove the { and } braces if the noblock attribute is set */
  String *newval = remove_block(featureattribs, val);
  val = newval ? newval : val;

  /* Add the feature */
  single_new_feature(featurename, val, featureattribs, declaratorid, type, declaratorparms, qualifier);

  /* Add extra features if there are default parameters in the parameter list */
  if (type) {
    while (declparms) {
      if (ParmList_has_defaultargs(declparms)) {

        /* Create a parameter list for the new feature by copying all
           but the last (defaulted) parameter */
        ParmList* newparms = CopyParmListMax(declparms, ParmList_len(declparms)-1);

        /* Create new declaration - with the last parameter removed */
        SwigType *newtype = Copy(type);
        Delete(SwigType_pop_function(newtype)); /* remove the old parameter list from newtype */
        SwigType_add_function(newtype,newparms);

        single_new_feature(featurename, Copy(val), featureattribs, declaratorid, newtype, newparms, qualifier);
        declparms = newparms;
      } else {
        declparms = 0;
      }
    }
  }
}

/* check if a function declaration is a plain C object */
static int is_cfunction(Node *n) {
  if (!cparse_cplusplus || cparse_externc)
    return 1;
  if (Swig_storage_isexternc(n)) {
    return 1;
  }
  return 0;
}

/* If the Node is a function with parameters, check to see if any of the parameters
 * have default arguments. If so create a new function for each defaulted argument. 
 * The additional functions form a linked list of nodes with the head being the original Node n. */
static void default_arguments(Node *n) {
  Node *function = n;

  if (function) {
    ParmList *varargs = Getattr(function,"feature:varargs");
    if (varargs) {
      /* Handles the %varargs directive by looking for "feature:varargs" and 
       * substituting ... with an alternative set of arguments.  */
      Parm     *p = Getattr(function,"parms");
      Parm     *pp = 0;
      while (p) {
	SwigType *t = Getattr(p,"type");
	if (Strcmp(t,"v(...)") == 0) {
	  if (pp) {
	    ParmList *cv = Copy(varargs);
	    set_nextSibling(pp,cv);
	    Delete(cv);
	  } else {
	    ParmList *cv =  Copy(varargs);
	    Setattr(function,"parms", cv);
	    Delete(cv);
	  }
	  break;
	}
	pp = p;
	p = nextSibling(p);
      }
    }

    /* Do not add in functions if kwargs is being used or if user wants old default argument wrapping
       (one wrapped method per function irrespective of number of default arguments) */
    if (compact_default_args 
	|| is_cfunction(function) 
	|| GetFlag(function,"feature:compactdefaultargs") 
	|| (GetFlag(function,"feature:kwargs") && kwargs_supported)) {
      ParmList *p = Getattr(function,"parms");
      if (p) 
        Setattr(p,"compactdefargs", "1"); /* mark parameters for special handling */
      function = 0; /* don't add in extra methods */
    }
  }

  while (function) {
    ParmList *parms = Getattr(function,"parms");
    if (ParmList_has_defaultargs(parms)) {

      /* Create a parameter list for the new function by copying all
         but the last (defaulted) parameter */
      ParmList* newparms = CopyParmListMax(parms,ParmList_len(parms)-1);

      /* Create new function and add to symbol table */
      {
	SwigType *ntype = Copy(nodeType(function));
	char *cntype = Char(ntype);
        Node *new_function = new_node(ntype);
        SwigType *decl = Copy(Getattr(function,"decl"));
        int constqualifier = SwigType_isconst(decl);
	String *ccode = Copy(Getattr(function,"code"));
	String *cstorage = Copy(Getattr(function,"storage"));
	String *cvalue = Copy(Getattr(function,"value"));
	SwigType *ctype = Copy(Getattr(function,"type"));
	String *cthrow = Copy(Getattr(function,"throw"));

        Delete(SwigType_pop_function(decl)); /* remove the old parameter list from decl */
        SwigType_add_function(decl,newparms);
        if (constqualifier)
          SwigType_add_qualifier(decl,"const");

        Setattr(new_function,"name", Getattr(function,"name"));
        Setattr(new_function,"code", ccode);
        Setattr(new_function,"decl", decl);
        Setattr(new_function,"parms", newparms);
        Setattr(new_function,"storage", cstorage);
        Setattr(new_function,"value", cvalue);
        Setattr(new_function,"type", ctype);
        Setattr(new_function,"throw", cthrow);

	Delete(ccode);
	Delete(cstorage);
	Delete(cvalue);
	Delete(ctype);
	Delete(cthrow);
	Delete(decl);

        {
          Node *throws = Getattr(function,"throws");
	  ParmList *pl = CopyParmList(throws);
          if (throws) Setattr(new_function,"throws",pl);
	  Delete(pl);
        }

        /* copy specific attributes for global (or in a namespace) template functions - these are not templated class methods */
        if (strcmp(cntype,"template") == 0) {
          Node *templatetype = Getattr(function,"templatetype");
          Node *symtypename = Getattr(function,"sym:typename");
          Parm *templateparms = Getattr(function,"templateparms");
          if (templatetype) {
	    Node *tmp = Copy(templatetype);
	    Setattr(new_function,"templatetype",tmp);
	    Delete(tmp);
	  }
          if (symtypename) {
	    Node *tmp = Copy(symtypename);
	    Setattr(new_function,"sym:typename",tmp);
	    Delete(tmp);
	  }
          if (templateparms) {
	    Parm *tmp = CopyParmList(templateparms);
	    Setattr(new_function,"templateparms",tmp);
	    Delete(tmp);
	  }
        } else if (strcmp(cntype,"constructor") == 0) {
          /* only copied for constructors as this is not a user defined feature - it is hard coded in the parser */
          if (GetFlag(function,"feature:new")) SetFlag(new_function,"feature:new");
        }

        add_symbols(new_function);
        /* mark added functions as ones with overloaded parameters and point to the parsed method */
        Setattr(new_function,"defaultargs", n);

        /* Point to the new function, extending the linked list */
        set_nextSibling(function, new_function);
	Delete(new_function);
        function = new_function;
	
	Delete(ntype);
      }
    } else {
      function = 0;
    }
  }
}

/* -----------------------------------------------------------------------------
 * mark_nodes_as_extend()
 *
 * Used by the %extend to mark subtypes with "feature:extend".
 * template instances declared within %extend are skipped
 * ----------------------------------------------------------------------------- */

static void mark_nodes_as_extend(Node *n) {
  for (; n; n = nextSibling(n)) {
    if (Getattr(n, "template") && Strcmp(nodeType(n), "class") == 0)
      continue;
    /* Fix me: extend is not a feature. Replace with isextendmember? */
    Setattr(n, "feature:extend", "1");
    mark_nodes_as_extend(firstChild(n));
  }
}

/* -----------------------------------------------------------------------------
 * add_qualifier_to_declarator()
 *
 * Normally the qualifier is pushed on to the front of the type.
 * Adding a qualifier to a pointer to member function is a special case.
 * For example       : typedef double (Cls::*pmf)(void) const;
 * The qualifier is  : q(const).
 * The declarator is : m(Cls).f(void).
 * We need           : m(Cls).q(const).f(void).
 * ----------------------------------------------------------------------------- */

static String *add_qualifier_to_declarator(SwigType *type, SwigType *qualifier) {
  int is_pointer_to_member_function = 0;
  String *decl = Copy(type);
  String *poppedtype = NewString("");
  assert(qualifier);

  while (decl) {
    if (SwigType_ismemberpointer(decl)) {
      String *memberptr = SwigType_pop(decl);
      if (SwigType_isfunction(decl)) {
	is_pointer_to_member_function = 1;
	SwigType_push(decl, qualifier);
	SwigType_push(decl, memberptr);
	Insert(decl, 0, poppedtype);
	Delete(memberptr);
	break;
      } else {
	Append(poppedtype, memberptr);
      }
      Delete(memberptr);
    } else {
      String *popped = SwigType_pop(decl);
      if (!popped)
	break;
      Append(poppedtype, popped);
      Delete(popped);
    }
  }

  if (!is_pointer_to_member_function) {
    Delete(decl);
    decl = Copy(type);
    SwigType_push(decl, qualifier);
  }

  Delete(poppedtype);
  return decl;
}


#line 1583 "y.tab.c" /* yacc.c:339  */

# ifndef YY_NULLPTR
#  if defined __cplusplus && 201103L <= __cplusplus
#   define YY_NULLPTR nullptr
#  else
#   define YY_NULLPTR 0
#  endif
# endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

/* In a future release of Bison, this section will be replaced
   by #include "y.tab.h".  */
#ifndef YY_YY_Y_TAB_H_INCLUDED
# define YY_YY_Y_TAB_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int yydebug;
#endif

/* Token type.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    ID = 258,
    HBLOCK = 259,
    POUND = 260,
    STRING = 261,
    WSTRING = 262,
    INCLUDE = 263,
    IMPORT = 264,
    INSERT = 265,
    CHARCONST = 266,
    WCHARCONST = 267,
    NUM_INT = 268,
    NUM_FLOAT = 269,
    NUM_UNSIGNED = 270,
    NUM_LONG = 271,
    NUM_ULONG = 272,
    NUM_LONGLONG = 273,
    NUM_ULONGLONG = 274,
    NUM_BOOL = 275,
    TYPEDEF = 276,
    TYPE_INT = 277,
    TYPE_UNSIGNED = 278,
    TYPE_SHORT = 279,
    TYPE_LONG = 280,
    TYPE_FLOAT = 281,
    TYPE_DOUBLE = 282,
    TYPE_CHAR = 283,
    TYPE_WCHAR = 284,
    TYPE_VOID = 285,
    TYPE_SIGNED = 286,
    TYPE_BOOL = 287,
    TYPE_COMPLEX = 288,
    TYPE_TYPEDEF = 289,
    TYPE_RAW = 290,
    TYPE_NON_ISO_INT8 = 291,
    TYPE_NON_ISO_INT16 = 292,
    TYPE_NON_ISO_INT32 = 293,
    TYPE_NON_ISO_INT64 = 294,
    LPAREN = 295,
    RPAREN = 296,
    COMMA = 297,
    SEMI = 298,
    EXTERN = 299,
    INIT = 300,
    LBRACE = 301,
    RBRACE = 302,
    PERIOD = 303,
    CONST_QUAL = 304,
    VOLATILE = 305,
    REGISTER = 306,
    STRUCT = 307,
    UNION = 308,
    EQUAL = 309,
    SIZEOF = 310,
    MODULE = 311,
    LBRACKET = 312,
    RBRACKET = 313,
    BEGINFILE = 314,
    ENDOFFILE = 315,
    ILLEGAL = 316,
    CONSTANT = 317,
    NAME = 318,
    RENAME = 319,
    NAMEWARN = 320,
    EXTEND = 321,
    PRAGMA = 322,
    FEATURE = 323,
    VARARGS = 324,
    ENUM = 325,
    CLASS = 326,
    TYPENAME = 327,
    PRIVATE = 328,
    PUBLIC = 329,
    PROTECTED = 330,
    COLON = 331,
    STATIC = 332,
    VIRTUAL = 333,
    FRIEND = 334,
    THROW = 335,
    CATCH = 336,
    EXPLICIT = 337,
    STATIC_ASSERT = 338,
    CONSTEXPR = 339,
    THREAD_LOCAL = 340,
    DECLTYPE = 341,
    AUTO = 342,
    NOEXCEPT = 343,
    OVERRIDE = 344,
    FINAL = 345,
    USING = 346,
    NAMESPACE = 347,
    NATIVE = 348,
    INLINE = 349,
    TYPEMAP = 350,
    EXCEPT = 351,
    ECHO = 352,
    APPLY = 353,
    CLEAR = 354,
    SWIGTEMPLATE = 355,
    FRAGMENT = 356,
    WARN = 357,
    LESSTHAN = 358,
    GREATERTHAN = 359,
    DELETE_KW = 360,
    DEFAULT = 361,
    LESSTHANOREQUALTO = 362,
    GREATERTHANOREQUALTO = 363,
    EQUALTO = 364,
    NOTEQUALTO = 365,
    ARROW = 366,
    QUESTIONMARK = 367,
    TYPES = 368,
    PARMS = 369,
    NONID = 370,
    DSTAR = 371,
    DCNOT = 372,
    TEMPLATE = 373,
    OPERATOR = 374,
    CONVERSIONOPERATOR = 375,
    PARSETYPE = 376,
    PARSEPARM = 377,
    PARSEPARMS = 378,
    DOXYGENSTRING = 379,
    DOXYGENPOSTSTRING = 380,
    CAST = 381,
    LOR = 382,
    LAND = 383,
    OR = 384,
    XOR = 385,
    AND = 386,
    LSHIFT = 387,
    RSHIFT = 388,
    PLUS = 389,
    MINUS = 390,
    STAR = 391,
    SLASH = 392,
    MODULO = 393,
    UMINUS = 394,
    NOT = 395,
    LNOT = 396,
    DCOLON = 397
  };
#endif
/* Tokens.  */
#define ID 258
#define HBLOCK 259
#define POUND 260
#define STRING 261
#define WSTRING 262
#define INCLUDE 263
#define IMPORT 264
#define INSERT 265
#define CHARCONST 266
#define WCHARCONST 267
#define NUM_INT 268
#define NUM_FLOAT 269
#define NUM_UNSIGNED 270
#define NUM_LONG 271
#define NUM_ULONG 272
#define NUM_LONGLONG 273
#define NUM_ULONGLONG 274
#define NUM_BOOL 275
#define TYPEDEF 276
#define TYPE_INT 277
#define TYPE_UNSIGNED 278
#define TYPE_SHORT 279
#define TYPE_LONG 280
#define TYPE_FLOAT 281
#define TYPE_DOUBLE 282
#define TYPE_CHAR 283
#define TYPE_WCHAR 284
#define TYPE_VOID 285
#define TYPE_SIGNED 286
#define TYPE_BOOL 287
#define TYPE_COMPLEX 288
#define TYPE_TYPEDEF 289
#define TYPE_RAW 290
#define TYPE_NON_ISO_INT8 291
#define TYPE_NON_ISO_INT16 292
#define TYPE_NON_ISO_INT32 293
#define TYPE_NON_ISO_INT64 294
#define LPAREN 295
#define RPAREN 296
#define COMMA 297
#define SEMI 298
#define EXTERN 299
#define INIT 300
#define LBRACE 301
#define RBRACE 302
#define PERIOD 303
#define CONST_QUAL 304
#define VOLATILE 305
#define REGISTER 306
#define STRUCT 307
#define UNION 308
#define EQUAL 309
#define SIZEOF 310
#define MODULE 311
#define LBRACKET 312
#define RBRACKET 313
#define BEGINFILE 314
#define ENDOFFILE 315
#define ILLEGAL 316
#define CONSTANT 317
#define NAME 318
#define RENAME 319
#define NAMEWARN 320
#define EXTEND 321
#define PRAGMA 322
#define FEATURE 323
#define VARARGS 324
#define ENUM 325
#define CLASS 326
#define TYPENAME 327
#define PRIVATE 328
#define PUBLIC 329
#define PROTECTED 330
#define COLON 331
#define STATIC 332
#define VIRTUAL 333
#define FRIEND 334
#define THROW 335
#define CATCH 336
#define EXPLICIT 337
#define STATIC_ASSERT 338
#define CONSTEXPR 339
#define THREAD_LOCAL 340
#define DECLTYPE 341
#define AUTO 342
#define NOEXCEPT 343
#define OVERRIDE 344
#define FINAL 345
#define USING 346
#define NAMESPACE 347
#define NATIVE 348
#define INLINE 349
#define TYPEMAP 350
#define EXCEPT 351
#define ECHO 352
#define APPLY 353
#define CLEAR 354
#define SWIGTEMPLATE 355
#define FRAGMENT 356
#define WARN 357
#define LESSTHAN 358
#define GREATERTHAN 359
#define DELETE_KW 360
#define DEFAULT 361
#define LESSTHANOREQUALTO 362
#define GREATERTHANOREQUALTO 363
#define EQUALTO 364
#define NOTEQUALTO 365
#define ARROW 366
#define QUESTIONMARK 367
#define TYPES 368
#define PARMS 369
#define NONID 370
#define DSTAR 371
#define DCNOT 372
#define TEMPLATE 373
#define OPERATOR 374
#define CONVERSIONOPERATOR 375
#define PARSETYPE 376
#define PARSEPARM 377
#define PARSEPARMS 378
#define DOXYGENSTRING 379
#define DOXYGENPOSTSTRING 380
#define CAST 381
#define LOR 382
#define LAND 383
#define OR 384
#define XOR 385
#define AND 386
#define LSHIFT 387
#define RSHIFT 388
#define PLUS 389
#define MINUS 390
#define STAR 391
#define SLASH 392
#define MODULO 393
#define UMINUS 394
#define NOT 395
#define LNOT 396
#define DCOLON 397

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED

union YYSTYPE
{
#line 1542 "parser.y" /* yacc.c:355  */

  const char  *id;
  List  *bases;
  struct Define {
    String *val;
    String *rawval;
    int     type;
    String *qualifier;
    String *refqualifier;
    String *bitfield;
    Parm   *throws;
    String *throwf;
    String *nexcept;
    String *final;
  } dtype;
  struct {
    const char *type;
    String *filename;
    int   line;
  } loc;
  struct {
    char      *id;
    SwigType  *type;
    String    *defarg;
    ParmList  *parms;
    short      have_parms;
    ParmList  *throws;
    String    *throwf;
    String    *nexcept;
    String    *final;
  } decl;
  Parm         *tparms;
  struct {
    String     *method;
    Hash       *kwargs;
  } tmap;
  struct {
    String     *type;
    String     *us;
  } ptype;
  SwigType     *type;
  String       *str;
  Parm         *p;
  ParmList     *pl;
  int           intvalue;
  Node         *node;

#line 1955 "y.tab.c" /* yacc.c:355  */
};

typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;

int yyparse (void);

#endif /* !YY_YY_Y_TAB_H_INCLUDED  */

/* Copy the second part of user declarations.  */

#line 1972 "y.tab.c" /* yacc.c:358  */

#ifdef short
# undef short
#endif

#ifdef YYTYPE_UINT8
typedef YYTYPE_UINT8 yytype_uint8;
#else
typedef unsigned char yytype_uint8;
#endif

#ifdef YYTYPE_INT8
typedef YYTYPE_INT8 yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef YYTYPE_UINT16
typedef YYTYPE_UINT16 yytype_uint16;
#else
typedef unsigned short int yytype_uint16;
#endif

#ifdef YYTYPE_INT16
typedef YYTYPE_INT16 yytype_int16;
#else
typedef short int yytype_int16;
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif ! defined YYSIZE_T
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned int
# endif
#endif

#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif

#ifndef YY_ATTRIBUTE
# if (defined __GNUC__                                               \
      && (2 < __GNUC__ || (__GNUC__ == 2 && 96 <= __GNUC_MINOR__)))  \
     || defined __SUNPRO_C && 0x5110 <= __SUNPRO_C
#  define YY_ATTRIBUTE(Spec) __attribute__(Spec)
# else
#  define YY_ATTRIBUTE(Spec) /* empty */
# endif
#endif

#ifndef YY_ATTRIBUTE_PURE
# define YY_ATTRIBUTE_PURE   YY_ATTRIBUTE ((__pure__))
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# define YY_ATTRIBUTE_UNUSED YY_ATTRIBUTE ((__unused__))
#endif

#if !defined _Noreturn \
     && (!defined __STDC_VERSION__ || __STDC_VERSION__ < 201112)
# if defined _MSC_VER && 1200 <= _MSC_VER
#  define _Noreturn __declspec (noreturn)
# else
#  define _Noreturn YY_ATTRIBUTE ((__noreturn__))
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(E) ((void) (E))
#else
# define YYUSE(E) /* empty */
#endif

#if defined __GNUC__ && 407 <= __GNUC__ * 100 + __GNUC_MINOR__
/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN \
    _Pragma ("GCC diagnostic push") \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")\
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# define YY_IGNORE_MAYBE_UNINITIALIZED_END \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif


#if ! defined yyoverflow || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* ! defined yyoverflow || YYERROR_VERBOSE */


#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yytype_int16 yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYSIZE_T yynewbytes;                                            \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / sizeof (*yyptr);                          \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, (Count) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYSIZE_T yyi;                         \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  62
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   5637

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  143
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  181
/* YYNRULES -- Number of rules.  */
#define YYNRULES  611
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  1191

/* YYTRANSLATE[YYX] -- Symbol number corresponding to YYX as returned
   by yylex, with out-of-bounds checking.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   397

#define YYTRANSLATE(YYX)                                                \
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, without out-of-bounds checking.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    93,    94,
      95,    96,    97,    98,    99,   100,   101,   102,   103,   104,
     105,   106,   107,   108,   109,   110,   111,   112,   113,   114,
     115,   116,   117,   118,   119,   120,   121,   122,   123,   124,
     125,   126,   127,   128,   129,   130,   131,   132,   133,   134,
     135,   136,   137,   138,   139,   140,   141,   142
};

#if YYDEBUG
  /* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,  1713,  1713,  1725,  1729,  1732,  1735,  1738,  1741,  1746,
    1755,  1759,  1766,  1771,  1772,  1773,  1774,  1775,  1785,  1801,
    1811,  1812,  1813,  1814,  1815,  1816,  1817,  1818,  1819,  1820,
    1821,  1822,  1823,  1824,  1825,  1826,  1827,  1828,  1829,  1830,
    1831,  1838,  1838,  1920,  1930,  1941,  1961,  1985,  2009,  2020,
    2029,  2048,  2054,  2060,  2065,  2072,  2079,  2083,  2096,  2105,
    2120,  2133,  2133,  2189,  2190,  2197,  2216,  2247,  2251,  2261,
    2266,  2284,  2327,  2333,  2346,  2352,  2378,  2384,  2391,  2392,
    2395,  2396,  2403,  2449,  2495,  2506,  2509,  2536,  2542,  2548,
    2554,  2562,  2568,  2574,  2580,  2588,  2589,  2590,  2593,  2598,
    2608,  2644,  2645,  2680,  2697,  2705,  2718,  2743,  2749,  2753,
    2756,  2767,  2772,  2785,  2797,  3095,  3105,  3112,  3113,  3117,
    3117,  3142,  3148,  3158,  3170,  3179,  3259,  3322,  3326,  3351,
    3355,  3366,  3371,  3372,  3373,  3377,  3378,  3379,  3390,  3395,
    3400,  3407,  3413,  3418,  3421,  3421,  3434,  3437,  3440,  3449,
    3452,  3459,  3481,  3510,  3608,  3661,  3662,  3663,  3664,  3665,
    3666,  3671,  3671,  3920,  3920,  4067,  4068,  4080,  4098,  4098,
    4359,  4365,  4371,  4374,  4377,  4380,  4383,  4386,  4391,  4427,
    4431,  4434,  4437,  4442,  4446,  4451,  4461,  4492,  4492,  4550,
    4550,  4572,  4599,  4616,  4621,  4616,  4629,  4630,  4631,  4631,
    4647,  4648,  4665,  4666,  4667,  4668,  4669,  4670,  4671,  4672,
    4673,  4674,  4675,  4676,  4677,  4678,  4679,  4680,  4682,  4685,
    4689,  4701,  4730,  4760,  4793,  4812,  4833,  4855,  4878,  4901,
    4909,  4916,  4923,  4931,  4939,  4942,  4946,  4949,  4950,  4951,
    4952,  4953,  4954,  4955,  4956,  4959,  4970,  4981,  4994,  5005,
    5016,  5030,  5033,  5036,  5037,  5041,  5043,  5051,  5063,  5064,
    5065,  5066,  5067,  5068,  5069,  5070,  5071,  5072,  5073,  5074,
    5075,  5076,  5077,  5078,  5079,  5080,  5081,  5082,  5089,  5100,
    5104,  5111,  5115,  5120,  5124,  5136,  5146,  5156,  5159,  5163,
    5169,  5182,  5186,  5189,  5193,  5197,  5225,  5233,  5246,  5262,
    5273,  5283,  5295,  5299,  5303,  5310,  5332,  5349,  5368,  5387,
    5394,  5402,  5411,  5420,  5424,  5433,  5444,  5455,  5467,  5477,
    5491,  5499,  5508,  5517,  5521,  5530,  5541,  5552,  5564,  5574,
    5584,  5595,  5608,  5615,  5623,  5639,  5647,  5658,  5669,  5680,
    5699,  5707,  5724,  5732,  5739,  5746,  5757,  5769,  5780,  5792,
    5803,  5814,  5834,  5855,  5861,  5867,  5874,  5881,  5890,  5899,
    5902,  5911,  5920,  5927,  5934,  5941,  5949,  5959,  5970,  5981,
    5992,  5999,  6006,  6009,  6026,  6044,  6054,  6061,  6067,  6072,
    6079,  6083,  6088,  6095,  6099,  6105,  6109,  6115,  6116,  6117,
    6123,  6129,  6133,  6134,  6138,  6145,  6148,  6149,  6153,  6154,
    6156,  6159,  6162,  6167,  6178,  6203,  6206,  6260,  6264,  6268,
    6272,  6276,  6280,  6284,  6288,  6292,  6296,  6300,  6304,  6308,
    6312,  6318,  6318,  6334,  6339,  6342,  6348,  6363,  6379,  6380,
    6383,  6384,  6388,  6389,  6399,  6403,  6408,  6418,  6429,  6434,
    6439,  6442,  6448,  6456,  6468,  6483,  6484,  6504,  6508,  6518,
    6524,  6527,  6530,  6534,  6539,  6544,  6545,  6550,  6564,  6580,
    6590,  6608,  6615,  6622,  6629,  6637,  6645,  6649,  6653,  6659,
    6660,  6661,  6662,  6663,  6664,  6665,  6666,  6669,  6673,  6677,
    6681,  6685,  6689,  6693,  6697,  6701,  6705,  6709,  6713,  6717,
    6721,  6735,  6739,  6743,  6749,  6753,  6757,  6761,  6765,  6781,
    6786,  6789,  6794,  6799,  6799,  6800,  6803,  6820,  6829,  6829,
    6847,  6847,  6865,  6866,  6867,  6870,  6874,  6878,  6882,  6888,
    6891,  6895,  6901,  6905,  6909,  6915,  6918,  6923,  6924,  6927,
    6930,  6933,  6936,  6941,  6944,  6949,  6955,  6961,  6967,  6973,
    6979,  6987,  6995,  7000,  7007,  7010,  7020,  7031,  7042,  7052,
    7062,  7070,  7082,  7083,  7086,  7087,  7088,  7089,  7092,  7104,
    7110,  7119,  7120,  7121,  7124,  7125,  7126,  7129,  7130,  7133,
    7138,  7142,  7145,  7148,  7151,  7154,  7159,  7163,  7166,  7173,
    7179,  7182,  7187,  7190,  7196,  7201,  7205,  7208,  7211,  7214,
    7219,  7223,  7226,  7229,  7235,  7238,  7241,  7249,  7252,  7255,
    7259,  7264,  7277,  7281,  7286,  7292,  7296,  7301,  7305,  7312,
    7315,  7320
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || 0
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "ID", "HBLOCK", "POUND", "STRING",
  "WSTRING", "INCLUDE", "IMPORT", "INSERT", "CHARCONST", "WCHARCONST",
  "NUM_INT", "NUM_FLOAT", "NUM_UNSIGNED", "NUM_LONG", "NUM_ULONG",
  "NUM_LONGLONG", "NUM_ULONGLONG", "NUM_BOOL", "TYPEDEF", "TYPE_INT",
  "TYPE_UNSIGNED", "TYPE_SHORT", "TYPE_LONG", "TYPE_FLOAT", "TYPE_DOUBLE",
  "TYPE_CHAR", "TYPE_WCHAR", "TYPE_VOID", "TYPE_SIGNED", "TYPE_BOOL",
  "TYPE_COMPLEX", "TYPE_TYPEDEF", "TYPE_RAW", "TYPE_NON_ISO_INT8",
  "TYPE_NON_ISO_INT16", "TYPE_NON_ISO_INT32", "TYPE_NON_ISO_INT64",
  "LPAREN", "RPAREN", "COMMA", "SEMI", "EXTERN", "INIT", "LBRACE",
  "RBRACE", "PERIOD", "CONST_QUAL", "VOLATILE", "REGISTER", "STRUCT",
  "UNION", "EQUAL", "SIZEOF", "MODULE", "LBRACKET", "RBRACKET",
  "BEGINFILE", "ENDOFFILE", "ILLEGAL", "CONSTANT", "NAME", "RENAME",
  "NAMEWARN", "EXTEND", "PRAGMA", "FEATURE", "VARARGS", "ENUM", "CLASS",
  "TYPENAME", "PRIVATE", "PUBLIC", "PROTECTED", "COLON", "STATIC",
  "VIRTUAL", "FRIEND", "THROW", "CATCH", "EXPLICIT", "STATIC_ASSERT",
  "CONSTEXPR", "THREAD_LOCAL", "DECLTYPE", "AUTO", "NOEXCEPT", "OVERRIDE",
  "FINAL", "USING", "NAMESPACE", "NATIVE", "INLINE", "TYPEMAP", "EXCEPT",
  "ECHO", "APPLY", "CLEAR", "SWIGTEMPLATE", "FRAGMENT", "WARN", "LESSTHAN",
  "GREATERTHAN", "DELETE_KW", "DEFAULT", "LESSTHANOREQUALTO",
  "GREATERTHANOREQUALTO", "EQUALTO", "NOTEQUALTO", "ARROW", "QUESTIONMARK",
  "TYPES", "PARMS", "NONID", "DSTAR", "DCNOT", "TEMPLATE", "OPERATOR",
  "CONVERSIONOPERATOR", "PARSETYPE", "PARSEPARM", "PARSEPARMS",
  "DOXYGENSTRING", "DOXYGENPOSTSTRING", "CAST", "LOR", "LAND", "OR", "XOR",
  "AND", "LSHIFT", "RSHIFT", "PLUS", "MINUS", "STAR", "SLASH", "MODULO",
  "UMINUS", "NOT", "LNOT", "DCOLON", "$accept", "program", "interface",
  "declaration", "swig_directive", "extend_directive", "$@1",
  "apply_directive", "clear_directive", "constant_directive",
  "echo_directive", "except_directive", "stringtype", "fname",
  "fragment_directive", "include_directive", "$@2", "includetype",
  "inline_directive", "insert_directive", "module_directive",
  "name_directive", "native_directive", "pragma_directive", "pragma_arg",
  "pragma_lang", "rename_directive", "rename_namewarn",
  "feature_directive", "stringbracesemi", "featattr", "varargs_directive",
  "varargs_parms", "typemap_directive", "typemap_type", "tm_list",
  "tm_tail", "typemap_parm", "types_directive", "template_directive",
  "warn_directive", "c_declaration", "$@3", "c_decl", "c_decl_tail",
  "initializer", "cpp_alternate_rettype", "cpp_lambda_decl",
  "lambda_introducer", "lambda_body", "lambda_tail", "$@4", "c_enum_key",
  "c_enum_inherit", "c_enum_forward_decl", "c_enum_decl",
  "c_constructor_decl", "cpp_declaration", "cpp_class_decl", "@5", "@6",
  "cpp_opt_declarators", "cpp_forward_class_decl", "cpp_template_decl",
  "$@7", "cpp_template_possible", "template_parms", "templateparameters",
  "templateparameter", "templateparameterstail", "cpp_using_decl",
  "cpp_namespace_decl", "@8", "$@9", "cpp_members", "$@10", "$@11", "$@12",
  "cpp_member_no_dox", "cpp_member", "cpp_constructor_decl",
  "cpp_destructor_decl", "cpp_conversion_operator", "cpp_catch_decl",
  "cpp_static_assert", "cpp_protection_decl", "cpp_swig_directive",
  "cpp_end", "cpp_vend", "anonymous_bitfield", "anon_bitfield_type",
  "extern_string", "storage_class", "parms", "rawparms", "ptail",
  "parm_no_dox", "parm", "valparms", "rawvalparms", "valptail", "valparm",
  "def_args", "parameter_declarator", "plain_declarator", "declarator",
  "notso_direct_declarator", "direct_declarator", "abstract_declarator",
  "direct_abstract_declarator", "pointer", "cv_ref_qualifier",
  "ref_qualifier", "type_qualifier", "type_qualifier_raw", "type",
  "rawtype", "type_right", "decltype", "primitive_type",
  "primitive_type_list", "type_specifier", "definetype", "$@13",
  "default_delete", "deleted_definition", "explicit_default", "ename",
  "constant_directives", "optional_ignored_defines", "enumlist",
  "enumlist_item", "edecl_with_dox", "edecl", "etype", "expr", "exprmem",
  "valexpr", "exprnum", "exprcompound", "ellipsis", "variadic", "inherit",
  "raw_inherit", "$@14", "base_list", "base_specifier", "@15", "@16",
  "access_specifier", "templcpptype", "cpptype", "classkey", "classkeyopt",
  "opt_virtual", "virt_specifier_seq", "virt_specifier_seq_opt",
  "exception_specification", "qualifiers_exception_specification",
  "cpp_const", "ctor_end", "ctor_initializer", "mem_initializer_list",
  "mem_initializer", "less_valparms_greater", "identifier", "idstring",
  "idstringopt", "idcolon", "idcolontail", "idtemplate",
  "idtemplatetemplate", "idcolonnt", "idcolontailnt", "string", "wstring",
  "stringbrace", "options", "kwargs", "stringnum", "empty", YY_NULLPTR
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[NUM] -- (External) token number corresponding to the
   (internal) symbol number NUM (which must be that of a token).  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300,   301,   302,   303,   304,
     305,   306,   307,   308,   309,   310,   311,   312,   313,   314,
     315,   316,   317,   318,   319,   320,   321,   322,   323,   324,
     325,   326,   327,   328,   329,   330,   331,   332,   333,   334,
     335,   336,   337,   338,   339,   340,   341,   342,   343,   344,
     345,   346,   347,   348,   349,   350,   351,   352,   353,   354,
     355,   356,   357,   358,   359,   360,   361,   362,   363,   364,
     365,   366,   367,   368,   369,   370,   371,   372,   373,   374,
     375,   376,   377,   378,   379,   380,   381,   382,   383,   384,
     385,   386,   387,   388,   389,   390,   391,   392,   393,   394,
     395,   396,   397
};
# endif

#define YYPACT_NINF -1036

#define yypact_value_is_default(Yystate) \
  (!!((Yystate) == (-1036)))

#define YYTABLE_NINF -612

#define yytable_value_is_error(Yytable_value) \
  0

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
static const yytype_int16 yypact[] =
{
     719,  4503,  4606,    69,    81,  3947, -1036, -1036, -1036, -1036,
   -1036, -1036, -1036, -1036, -1036, -1036, -1036, -1036, -1036, -1036,
   -1036, -1036, -1036, -1036, -1036, -1036,   149, -1036, -1036, -1036,
   -1036, -1036,   304,   205,   236,   299, -1036, -1036,   226,   241,
     289,  5235,   185,   306,   441,  5518,   699,   174,   699, -1036,
   -1036, -1036,  3303, -1036,   185,   289, -1036,   269, -1036,   463,
     507,  4916, -1036,   382, -1036, -1036, -1036,   521, -1036, -1036,
      39,   535,  5019,   537, -1036, -1036,   535,   546,   579,   590,
       8, -1036, -1036,   613,   562,   624,   217,   293,   298,   343,
     635,   169,   639,   649,   542,  5306,  5306,   640,   654,   698,
     665,   278, -1036, -1036, -1036, -1036, -1036, -1036, -1036, -1036,
   -1036, -1036, -1036, -1036,   535, -1036, -1036, -1036, -1036, -1036,
   -1036, -1036,   857, -1036, -1036, -1036, -1036, -1036, -1036, -1036,
   -1036, -1036, -1036, -1036, -1036, -1036, -1036, -1036, -1036, -1036,
   -1036, -1036, -1036,    82,  5377, -1036,   660, -1036, -1036,   662,
     664,   185,   162,   483,  2266, -1036, -1036, -1036, -1036, -1036,
     699, -1036,  3541,   674,   166,  2402,  3218,    52,   434,  1513,
      48,   185, -1036, -1036,   244,   385,   244,   404,   403,   597,
   -1036, -1036, -1036, -1036, -1036,   202,   223, -1036, -1036, -1036,
     683, -1036,   687, -1036, -1036,   626, -1036, -1036,   483,   154,
     626,   626, -1036,   682,  1158, -1036,    13,   802,    34,   202,
     202, -1036,   626,  4813, -1036, -1036,  4916, -1036, -1036, -1036,
   -1036, -1036, -1036,   185,   272, -1036,   249,   700,   202, -1036,
   -1036,   626,   202, -1036, -1036, -1036,   768,  4916,   730,   381,
     747,   754,   626,   698,   768,  4916,  4916,   185,   698,   358,
     925,  1139,   626,   438,  2013,   688, -1036, -1036,  1158,   185,
    1441,   232, -1036,   759,   764,   772,   202, -1036, -1036,   269,
     711,   706, -1036, -1036, -1036, -1036, -1036, -1036, -1036, -1036,
   -1036, -1036, -1036,  3218,   370,  3218,  3218,  3218,  3218,  3218,
    3218,  3218, -1036,   714, -1036,   786,   790,  1061,  2238,    36,
      21, -1036, -1036,   768,   827, -1036, -1036,  3658,   130,   130,
     797,   803,   892,   733,   798, -1036, -1036, -1036,   799,  3218,
   -1036, -1036, -1036, -1036,  3722, -1036,  2238,   814,  3658,   809,
     185,   440,   404, -1036,   811,   440,   404, -1036,   725, -1036,
   -1036,  4916,  2538, -1036,  4916,  2674,   818,  1935,  2272,   440,
     404,   755,   553, -1036, -1036,   269,   830,  4709, -1036, -1036,
   -1036, -1036,   839,   768,   185, -1036, -1036,   392,   842, -1036,
   -1036,  1447,   244,   447,   318, -1036,   858, -1036, -1036, -1036,
   -1036,   185, -1036,   860,   844,   499,   868,   861, -1036,   874,
     873, -1036,  5448, -1036,   185, -1036,   877,   879, -1036,   880,
     881,  5306, -1036, -1036,   459, -1036, -1036, -1036,  5306, -1036,
   -1036, -1036,   883, -1036, -1036,   566,   261,   885,   823, -1036,
     889, -1036,   165, -1036, -1036,    54,   128,   128,   128,   467,
     819,   895,   147,   894,  4916,  2408,  2492,   828,  2018,  1576,
      50,   863,   242, -1036,  3775,  1576, -1036,   897, -1036,   286,
   -1036, -1036, -1036, -1036,   289, -1036,   483,   942,  2642,  5448,
     900,  3050,  2649, -1036, -1036, -1036, -1036, -1036, -1036,  2266,
   -1036, -1036, -1036,  3218,  3218,  3218,  3218,  3218,  3218,  3218,
    3218,  3218,  3218,  3218,  3218,  3218,  3218,  3218,  3218,  3218,
     947,   953, -1036,   531,   531,  2074,   845,   362, -1036,   519,
   -1036, -1036,   531,   531,   565,   848,  1601,   128,  3218,  2238,
   -1036,  4916,  2007,    11,   910, -1036,  4916,  2810,   918, -1036,
     926, -1036,  4450,   928, -1036,  4760,   922,   923,   440,   404,
     930,   440,   404,  1887,   931,   932,  2544,   440, -1036, -1036,
   -1036,  4916,   687,   354, -1036, -1036,   626,  2722, -1036,   941,
    4916,   946, -1036,   944, -1036,   681,  1465,  2237,   954,  4916,
    1158,   973, -1036,   381,  4060,   979, -1036,  1048,  5306,   186,
     984,   980,  4916,   754,   508,   982,   626,  4916,   126,   935,
    4916, -1036, -1036, -1036,  1601,   276,  1305,    18, -1036,   987,
    2356,   989,   288,   945,   948, -1036, -1036,   638, -1036,   328,
   -1036, -1036, -1036,   920, -1036,   977,  5518,   482, -1036,   995,
     819,   244,   960, -1036, -1036,   993, -1036,   185, -1036,  3218,
    2946,  3082,  3354,    70,   174,   997,   786,   867,   867,  3177,
    3177,  2373,  1277,  3050,  2921,  2785,  2649,   752,   752,   732,
     732, -1036, -1036, -1036, -1036, -1036,   848,   699, -1036, -1036,
   -1036,   531,  1000,  1002,   381,   438,  4863,  1006,   600,   848,
   -1036,  1232,  1305,  1007, -1036,  5395,  1305,   416, -1036,   416,
   -1036,  1305,  1004,  1005,  1010,  1011,  2628,   440,   404,  1018,
    1019,  1020,   440,   687, -1036, -1036, -1036,   768,  4173, -1036,
    1008, -1036,   261,  1012, -1036,  1030, -1036, -1036, -1036, -1036,
     768, -1036, -1036, -1036,  1033, -1036,  1576,   768, -1036,  1022,
      88,   658,  1465, -1036,  1576, -1036,  1034, -1036, -1036,  4286,
      44,  5448,   484, -1036, -1036,  4916, -1036,  1037, -1036,   939,
   -1036,   280,   985, -1036,  1041,  1036, -1036,   185,  1742,   889,
   -1036,   381,  1576,   353,  1305, -1036,  4916,  3218, -1036, -1036,
   -1036, -1036, -1036,  4678, -1036,   474, -1036, -1036,  1023,  3076,
     426, -1036, -1036,  1045, -1036,   804, -1036,  2147, -1036,   244,
    2238,  3218,  3218,  3354,  3845,  3218,  1049,  1050,  1057,  1060,
   -1036,  3218, -1036, -1036,  1063,  1064, -1036, -1036, -1036,   621,
     440, -1036, -1036,   440, -1036, -1036,   440,  1305,  1305,  1054,
    1059,  1062,   440,  1305,  1065,  1066, -1036, -1036,   626,   626,
     416,  2147,  4916,   126,  2722,  1654,   626,  1070, -1036,  1576,
    1046, -1036, -1036,   768,  1158,   173, -1036,  5306, -1036,  1071,
     416,    72,   202,   104, -1036,  2266,   192, -1036,  1067,    39,
     302, -1036, -1036, -1036, -1036, -1036, -1036, -1036,  5090, -1036,
    4399,  1077, -1036,  1083,  2777, -1036, -1036, -1036,   505, -1036,
   -1036, -1036,  4916, -1036,   525,  1023, -1036,    62,  1081,    24,
   -1036,  4916,   318,  1082,  1055, -1036, -1036,  1158, -1036, -1036,
   -1036,   960, -1036, -1036, -1036,   185, -1036, -1036, -1036,  1092,
    1073,  1076,  1078,  1013,  3435,   202, -1036, -1036, -1036, -1036,
   -1036, -1036, -1036, -1036, -1036, -1036, -1036, -1036, -1036, -1036,
   -1036, -1036, -1036, -1036, -1036, -1036, -1036,  1099,  1031,  2147,
   -1036, -1036, -1036, -1036, -1036, -1036, -1036,  5162,  1100,  2147,
   -1036,  2238,  2238,  2238,  3218,  3218, -1036,  5448,  2508, -1036,
   -1036, -1036,   440,   440,  1305,  1107,  1115,   440,  1305,  1305,
   -1036, -1036,   244,  1117,  1128, -1036,   768,  1136, -1036,  1576,
    1768,   126, -1036,  1138, -1036,  1141, -1036, -1036, -1036,   280,
   -1036, -1036,   280,  1084, -1036, -1036,  5448,  4916,  1158,  5448,
    1950, -1036, -1036,   505, -1036, -1036,   244, -1036,  1129, -1036,
   -1036, -1036, -1036,   202,  1023, -1036,  1131,  1774,    38, -1036,
    1149,  1148,   318,   185,   620, -1036,  1576, -1036,  1134,   960,
    2147, -1036, -1036, -1036, -1036,   202, -1036,  1155,  1831, -1036,
   -1036,  1123,  1126,  1133,  1135,  1140,    25,  1153,  2238,  2238,
     174,   440,  1305,  1305,   440,   440,  1161, -1036,  1162, -1036,
    1164, -1036,  1576, -1036, -1036, -1036, -1036, -1036,  1167,   381,
    1108,    17,  3775, -1036,   426,  1576,  1174, -1036, -1036,  3218,
   -1036,  1576,  1023, -1036,   627, -1036,  1181,  1176,  1177,   486,
   -1036, -1036,   244,  1178, -1036, -1036, -1036,   185, -1036,  2147,
    1187,  4916, -1036, -1036,  1576,  3218, -1036,  1831,  1191,   440,
     440, -1036, -1036, -1036,  1190, -1036,  1193, -1036,  4916,  1197,
    1198,     9,  1202, -1036,    45, -1036, -1036,  2238,   244, -1036,
   -1036, -1036, -1036,   185,  1196, -1036, -1036,   426,  1201,  1134,
    1192,  4916,  1204,   244,  2912, -1036, -1036, -1036, -1036,  1210,
    4916,  4916,  4916,  1215,  3076,  5448,   525,   426,  1211,  1212,
   -1036, -1036, -1036, -1036,  1221,  1576,   426, -1036,  1576,  1229,
    1230,  1233,  4916, -1036,  1234, -1036, -1036,  1228, -1036,  2147,
    1576, -1036,   571, -1036, -1036,   606,  1576,  1576,  1576,  1237,
     525,  1239, -1036, -1036, -1036, -1036,   318, -1036, -1036,   318,
   -1036, -1036, -1036,  1576, -1036, -1036,  1242,  1245, -1036, -1036,
   -1036
};

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
     means the default is an error.  */
static const yytype_uint16 yydefact[] =
{
     611,     0,     0,     0,     0,     0,    12,     4,   561,   407,
     415,   408,   409,   412,   413,   410,   411,   397,   414,   396,
     416,   399,   417,   418,   419,   420,     0,   387,   388,   389,
     520,   521,   146,   515,   516,     0,   562,   563,     0,     0,
     573,     0,     0,   287,     0,     0,   385,   611,   392,   402,
     395,   404,   405,   519,     0,   580,   400,   571,     6,     0,
       0,   611,     1,    17,    67,    63,    64,     0,   263,    16,
     258,   611,     0,     0,    85,    86,   611,   611,     0,     0,
     262,   264,   265,     0,   266,     0,   267,   272,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    10,    11,     9,    13,    20,    21,    22,    23,
      24,    25,    26,    27,   611,    28,    29,    30,    31,    32,
      33,    34,     0,    35,    36,    37,    38,    39,    40,    14,
     116,   121,   118,   117,    18,    15,   155,   156,   157,   158,
     159,   160,   124,   259,     0,   277,     0,   148,   147,     0,
       0,     0,     0,     0,   611,   574,   288,   398,   289,     3,
     391,   386,   611,     0,   421,     0,     0,   573,   363,   362,
     379,     0,   304,   284,   611,   313,   611,   359,   353,   340,
     301,   393,   406,   401,   581,     0,     0,   569,     5,     8,
       0,   278,   611,   280,    19,     0,   595,   275,     0,   257,
       0,     0,   602,     0,     0,   390,   580,     0,   611,     0,
       0,    81,     0,   611,   270,   274,   611,   268,   230,   271,
     269,   276,   273,     0,     0,   189,   580,     0,     0,    65,
      66,     0,     0,    54,    52,    49,    50,   611,     0,   611,
       0,   611,   611,     0,   115,   611,   611,     0,     0,     0,
       0,     0,     0,   313,     0,   340,   261,   260,     0,   611,
       0,   611,   286,     0,     0,     0,     0,   575,   582,   572,
       0,   561,   597,   457,   458,   469,   470,   471,   472,   473,
     474,   475,   476,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   295,     0,   290,   611,   446,   390,     0,   451,
     445,   450,   455,   452,   456,   292,   394,   611,   363,   362,
       0,     0,   353,   400,     0,   299,   426,   427,   297,     0,
     423,   424,   425,   370,     0,   445,   300,     0,   611,     0,
       0,   315,   361,   332,     0,   314,   360,   377,   378,   341,
     302,   611,     0,   303,   611,     0,     0,   356,   355,   310,
     354,   332,   364,   579,   578,   577,     0,     0,   279,   283,
     565,   564,     0,   566,     0,   594,   119,   605,     0,    71,
      48,     0,   611,   313,   421,    73,     0,   523,   524,   522,
     525,     0,   526,     0,    77,     0,     0,     0,   101,     0,
       0,   185,     0,   611,     0,   187,     0,     0,   106,     0,
       0,     0,   110,   306,   313,   307,   309,    44,     0,   107,
     109,   567,     0,   568,    57,     0,    56,     0,     0,   178,
     611,   182,   519,   180,   170,     0,     0,     0,     0,   564,
       0,     0,     0,     0,   611,     0,     0,   332,     0,   611,
     340,   611,   580,   429,   611,   611,   503,     0,   502,   401,
     505,   517,   518,   403,     0,   570,     0,     0,     0,     0,
       0,   467,   466,   495,   494,   468,   496,   497,   560,     0,
     291,   294,   498,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   596,   363,   362,   353,   400,     0,   340,     0,
     375,   372,   356,   355,     0,   340,   364,     0,     0,   422,
     371,   611,   353,   400,     0,   333,   611,     0,     0,   376,
       0,   349,     0,     0,   368,     0,     0,     0,   312,   358,
       0,   311,   357,   366,     0,     0,     0,   316,   365,   576,
       7,     0,   611,     0,   171,   611,     0,     0,   601,     0,
     611,     0,    72,     0,    80,     0,     0,     0,     0,     0,
       0,     0,   186,   611,     0,     0,   611,   611,     0,     0,
     111,     0,   611,   611,     0,     0,     0,     0,     0,   168,
       0,   179,   184,    61,     0,     0,     0,     0,    82,     0,
       0,     0,   536,   529,   530,   384,   383,   541,   382,   380,
     537,   542,   544,     0,   545,     0,     0,     0,   150,     0,
     400,   611,   611,   163,   167,     0,   583,     0,   447,   459,
       0,     0,   379,     0,   611,     0,   611,   492,   491,   489,
     490,     0,   488,   487,   483,   484,   482,   485,   486,   477,
     478,   479,   480,   481,   449,   448,     0,   364,   344,   343,
     342,   366,     0,     0,   365,   323,     0,     0,     0,   332,
     334,   364,     0,     0,   337,     0,     0,   351,   350,   373,
     369,     0,     0,     0,     0,     0,     0,   317,   367,     0,
       0,     0,   319,   611,   281,    69,    70,    68,     0,   606,
     607,   610,   609,   603,    46,     0,    45,    41,    79,    76,
      78,   600,    96,   599,     0,    91,   611,   598,    95,     0,
     609,     0,     0,   102,   611,   229,     0,   190,   191,     0,
     258,     0,     0,    53,    51,   611,    43,     0,   108,     0,
     588,   586,     0,    60,     0,     0,   113,     0,   611,   611,
     611,     0,   611,     0,     0,   351,   611,     0,   539,   532,
     531,   543,   381,     0,   141,     0,   149,   151,   611,   611,
       0,   131,   527,   504,   506,   508,   528,     0,   161,   611,
     460,     0,     0,   379,   378,     0,     0,     0,     0,     0,
     293,     0,   345,   347,     0,     0,   298,   352,   335,     0,
     325,   339,   338,   324,   305,   374,   320,     0,     0,     0,
       0,     0,   318,     0,     0,     0,   282,   120,     0,     0,
     351,     0,   611,     0,     0,     0,     0,     0,    93,   611,
       0,   122,   188,   257,     0,   580,   104,     0,   103,     0,
     351,     0,     0,     0,   584,   611,     0,    55,     0,   258,
       0,   172,   173,   176,   175,   169,   174,   177,     0,   183,
       0,     0,    84,     0,     0,   134,   133,   135,   611,   137,
     132,   136,   611,   142,     0,   430,   432,   438,     0,   434,
     433,   611,   421,   542,   611,   154,   130,     0,   127,   129,
     125,   611,   513,   512,   514,     0,   510,   198,   217,     0,
       0,     0,     0,   264,   611,     0,   242,   243,   235,   244,
     215,   196,   240,   236,   234,   237,   238,   239,   241,   216,
     212,   213,   200,   207,   206,   210,   209,     0,   218,     0,
     201,   202,   205,   211,   203,   204,   214,     0,   277,     0,
     285,   463,   462,   461,     0,     0,   453,     0,   493,   346,
     348,   336,   322,   321,     0,     0,     0,   326,     0,     0,
     608,   604,   611,     0,     0,    87,   609,    98,    92,   611,
       0,     0,   100,     0,    74,     0,   112,   308,   589,   587,
     593,   592,   591,     0,    58,    59,     0,   611,     0,     0,
       0,    62,    83,   535,   540,   533,   611,   534,     0,   144,
     143,   140,   431,     0,   611,   440,   442,     0,   611,   435,
       0,     0,     0,     0,     0,   553,   611,   507,   611,   611,
       0,   193,   232,   231,   233,     0,   219,     0,     0,   220,
     192,   397,   396,   399,     0,   395,   400,     0,   465,   464,
     611,   327,     0,     0,   331,   330,     0,    42,     0,    99,
       0,    94,   611,    89,    75,   105,   585,   590,     0,   611,
       0,     0,   611,   538,     0,   611,     0,   441,   439,     0,
     152,   611,   611,   436,     0,   550,     0,   552,   554,     0,
     546,   547,   611,     0,   500,   509,   501,     0,   199,     0,
       0,   611,   165,   164,   611,     0,   208,     0,     0,   329,
     328,    47,    97,    88,     0,   114,     0,   168,   611,     0,
       0,     0,     0,   126,     0,   145,   443,   444,   611,   437,
     548,   549,   551,     0,     0,   558,   559,     0,     0,   611,
       0,   611,     0,   611,     0,   162,   454,    90,   123,     0,
     611,   611,   611,     0,   611,     0,     0,     0,   555,     0,
     128,   499,   511,   194,     0,   611,     0,   251,   611,     0,
       0,     0,   611,   221,     0,   138,   153,     0,   556,     0,
     611,   222,     0,   166,   228,     0,   611,   611,   611,     0,
       0,     0,   195,   223,   245,   247,     0,   248,   250,   421,
     226,   225,   224,   611,   139,   557,     0,     0,   227,   246,
     249
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
   -1036, -1036,  -360, -1036, -1036, -1036, -1036,    56,    67,    -2,
      73, -1036,   743, -1036,    75,    80, -1036, -1036, -1036,    83,
   -1036,    84, -1036,    90, -1036, -1036,    91, -1036,   101,  -553,
    -684,   103, -1036,   105, -1036,  -369,   720,   -91,   107,   110,
     112,   113, -1036,   554,  -913,  -933, -1036, -1036, -1036, -1035,
    -915, -1036,  -130, -1036, -1036, -1036, -1036, -1036,    32, -1036,
   -1036,   213,    35,    46, -1036, -1036,   327, -1036,   727,   570,
     118, -1036, -1036, -1036,  -783, -1036, -1036, -1036,   420, -1036,
     577, -1036,   578,   133, -1036, -1036, -1036, -1036,  -532, -1036,
   -1036, -1036,    14,   -35, -1036,  -501,  1276,    10,   492, -1036,
     702,   866,   -39,  -602,  -548,   197,  1175,    26,  -149,  1127,
     201,  -621,   731,    74, -1036,   -65,    41,   -27,   576,  -714,
    1280, -1036,  -365, -1036,  -163, -1036, -1036, -1036,   472,   344,
    -936, -1036, -1036,   346, -1036,   978, -1036,  -144,  -503, -1036,
   -1036,   221,   893, -1036, -1036, -1036,   460, -1036, -1036, -1036,
    -240,    27, -1036, -1036,   334,  -568, -1036,  -576, -1036,   612,
     212, -1036, -1036,   235,   -24,  1124,  -178, -1036,  1052,  -211,
    -150,  1171, -1036,  -271,  1297, -1036,   628,     7,  -193,  -537,
       0
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     4,     5,   104,   105,   106,   811,   896,   897,   898,
     899,   111,   414,   415,   900,   901,   740,   114,   115,   902,
     117,   903,   119,   904,   699,   210,   905,   122,   906,   705,
     558,   907,   387,   908,   397,   240,   409,   241,   909,   910,
     911,   912,   545,   130,   880,   760,   858,   131,   755,   864,
     991,  1056,    42,   607,   132,   133,   134,   135,   913,   929,
     767,  1083,   914,   915,   738,   845,   418,   419,   420,   581,
     916,   140,   566,   393,   917,  1079,  1159,  1010,   918,   919,
     920,   921,   922,   923,   142,   924,   925,  1161,  1164,   926,
    1024,   143,   927,   310,   191,   358,    43,   192,   293,   294,
     470,   295,   761,   173,   402,   174,   331,   253,   176,   177,
     254,   597,   598,    45,    46,   296,   205,    48,    49,    50,
      51,    52,   318,   319,   360,   321,   322,   441,   866,   867,
     868,   869,   994,   995,  1106,   298,   299,   325,   301,   302,
    1074,  1075,   447,   448,   612,   763,   764,   885,  1009,   886,
      53,    54,   380,   381,   765,   600,   986,   601,   602,  1165,
     875,  1004,  1067,  1068,   184,    55,   367,   412,    56,   187,
      57,   269,   732,   834,   303,   304,   708,   201,   368,   693,
     193
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
       6,   320,   268,   109,   238,   145,   422,   204,   172,   551,
     300,    44,    59,   311,   259,   716,   155,   362,   160,   144,
     711,   751,   778,   369,   748,   736,   190,   817,   953,   376,
     239,   239,   571,   564,   385,   353,   268,   136,   398,   860,
     137,   684,    47,    47,   691,   196,   794,   180,   795,  1132,
     196,   138,   660,  1054,   691,   327,   431,  1098,   455,   660,
     365,   107,  1063,  -296,   411,     8,   998,   374,  1086,  1136,
      60,   202,   108,   175,   433,     8,   202,   211,   110,   260,
     112,    62,    47,   208,   490,   113,   377,   378,   116,   118,
     405,   863,   214,   215,   365,   120,   121,    27,    28,    29,
      72,  -256,    47,   256,   605,   379,   123,     8,   124,    61,
     125,   775,   126,   583,   202,   127,   154,   128,   129,  1170,
     161,   248,   181,   139,   197,  -296,  1109,   661,   247,   197,
     701,     8,   196,     8,   744,   340,  1020,   343,   141,  1117,
    1133,  1103,  -566,   155,   539,  1099,  1027,   491,  1100,   999,
       8,    36,    37,   170,   305,   154,  1135,   198,   311,   818,
     365,    36,    37,  1062,   292,     8,   438,   257,   371,   702,
     162,   261,   703,   229,   180,  1137,   180,     8,   388,   311,
     270,   389,   806,   873,   170,   688,   993,   165,     8,   952,
    1146,   968,   359,    36,    37,   297,   974,   146,   776,   722,
     366,   777,   400,    47,  1140,     8,   719,  -181,   382,   967,
     417,   320,   315,  1025,   162,   230,   964,    36,    37,    36,
      37,  1155,   163,   971,  1156,   364,     8,  1078,   164,   723,
     373,   165,   724,  1163,   306,     8,    36,    37,   975,   406,
     704,   410,   413,    38,   338,    38,   423,   167,   178,   167,
     166,    36,    37,   149,    47,  1184,   421,    47,   172,   443,
     955,   450,    38,    36,    37,   404,    40,   365,   171,  -181,
     171,   316,   317,  1039,    36,    37,   154,   957,    47,     8,
     266,   267,   196,   170,   150,  -428,    47,    47,  -428,    38,
     985,    36,    37,   167,   219,   471,  1120,   180,   164,   220,
      38,     8,   168,   394,    40,   169,   520,   577,   446,   523,
     170,   691,    36,    37,   171,   391,   371,   573,  -428,   252,
     166,    36,    37,   175,   163,   300,   392,   563,   747,   614,
      30,    31,  -611,   549,   497,   499,   239,   221,   504,   151,
     422,   266,   354,   239,   153,   154,     8,    38,    47,    33,
      34,    40,   154,   689,    30,    31,   147,   653,   685,   365,
     196,     8,   446,   312,   196,    36,    37,   542,   152,    47,
     222,   337,   180,    33,    34,   148,  1172,   593,   594,   709,
     850,   246,    47,   734,     8,    47,   185,    36,    37,   225,
     223,    38,   154,     6,   624,   167,   852,   832,    47,   589,
     686,   372,   434,   648,   250,   977,     8,   251,  1043,   609,
     459,   186,   170,    38,   405,  1053,   171,    40,   460,   342,
     582,   162,   833,   316,   317,   341,   538,   876,  1088,   163,
     616,   158,    36,    37,   546,   653,   403,     8,   165,   604,
     178,   608,   342,   328,   344,   604,   547,    36,    37,   450,
     428,   346,   497,   499,   504,   439,   595,   445,    38,   596,
     165,   345,    40,   316,   317,    27,    28,    29,   877,   878,
      36,    37,   879,    38,   328,    47,   657,    40,   434,   292,
     516,   663,   329,   617,   159,    47,   426,   550,   701,   427,
     196,   165,    36,    37,   170,   342,    38,   517,   178,   572,
     167,  1096,   194,   239,   342,   785,   188,  1001,   495,   168,
     297,     8,   169,   599,   862,   695,   342,   170,    38,   599,
     863,   171,    40,    36,    37,   757,  1115,   826,   758,   512,
     703,   347,  1116,   655,   348,    30,    31,   727,   827,   519,
     556,   557,   359,   330,   595,     6,   235,   596,   196,    38,
     189,   683,    47,    40,    33,    34,     8,    47,   873,   434,
     649,   195,   109,   406,   145,   989,     6,   145,   990,   713,
     154,   307,   428,   410,   330,   200,   342,   207,   144,   756,
     654,   721,    47,  -580,  -580,   172,   209,   735,   165,   404,
     421,    47,   785,   432,   593,   594,   136,    36,    37,   137,
      47,   534,    27,    28,    29,   434,   650,   575,   576,  -580,
     138,   180,   766,    47,  1174,   950,   951,  1175,    47,   212,
     107,    47,   342,   729,   180,  1176,   471,   730,  1173,     8,
     213,   108,   196,   586,  1180,  1181,  1182,   110,   709,   112,
     516,   788,    36,    37,   113,   428,   217,   116,   118,  1177,
     175,  1188,  1178,   216,   120,   121,   824,   517,   741,   623,
    1179,   516,   941,  1070,   218,   123,  1071,   124,    38,   125,
    1110,   126,    40,  1111,   127,   228,   128,   129,   517,   231,
     242,   535,   139,   359,   536,   698,   109,   196,   145,   232,
     829,   300,   233,   330,   243,   234,   774,   141,  1046,   815,
     816,  1047,   144,   652,   196,   245,   604,   178,   262,   320,
     263,   853,   264,   352,   604,    36,    37,   109,   591,   145,
     136,   538,   314,   137,   356,   370,   592,   593,   594,   357,
     930,   316,   317,   144,   138,   538,   965,   422,   145,   582,
       6,   599,   604,   599,   107,  1017,   395,   160,    27,    28,
      29,   136,   848,   706,   137,   108,   865,   714,   870,   604,
     403,   110,   239,   112,   178,   138,    47,   928,   113,   180,
     842,   116,   118,   843,   365,   107,   401,   954,   120,   121,
     599,   652,   742,   980,   844,   178,   108,    47,   599,   123,
     407,   124,   110,   125,   112,   126,   408,   259,   127,   113,
     128,   129,   116,   118,   438,     8,   139,   451,   196,   120,
     121,   928,   452,   453,  1187,   456,   599,   457,   468,   604,
     123,   141,   124,   337,   125,   178,   126,   988,   469,   127,
     472,   128,   129,   599,   492,   305,  1000,   139,   500,  1066,
       1,     2,     3,   375,   501,   292,   507,   774,   109,   506,
     145,   784,   141,    47,   511,   178,   508,   514,   987,   518,
       8,   170,   980,   865,   144,  1080,   526,   247,   487,   488,
     489,   533,  1030,   540,  1005,   261,   297,   882,   883,   884,
     543,   766,   136,   548,   599,   137,   485,   486,   487,   488,
     489,    36,    37,   599,   145,     8,   138,   249,   555,   552,
     405,   554,   560,    47,   599,   163,   107,   316,   317,   819,
     559,  1049,    47,  1036,  1051,   561,   562,   108,   567,   928,
     568,   569,   570,   110,   574,   112,   578,   579,     8,   928,
     113,   580,   162,   116,   118,   584,   585,   588,   784,   606,
     120,   121,   178,   613,   590,   618,    36,    37,   625,   165,
     644,   123,   180,   124,   261,   125,   645,   126,   662,   604,
     127,   647,   128,   129,   651,   432,   666,   667,   139,   669,
     671,   672,    38,   329,   337,   519,   167,   423,   673,   679,
     680,    36,    37,   141,   694,   250,   180,   421,   251,   696,
     697,   172,   865,   170,   870,   712,   865,   171,   870,   483,
     484,   485,   486,   487,   488,   489,   604,    38,  1076,   766,
     928,   167,   959,  1186,    36,    37,   320,  1102,    47,   715,
     502,   963,   718,   503,   725,   733,   737,   726,   745,   746,
     180,   753,   171,   599,   754,   749,   759,   750,   762,   768,
      38,   782,   604,   783,    40,   779,  1122,   787,   791,   406,
     808,   603,   797,   798,   809,   604,   175,   611,   799,   800,
     865,   604,   870,  1129,     8,   330,   803,   804,   805,    68,
    1154,   810,   180,   812,  1006,   404,   814,   821,   830,   928,
     599,   831,   836,   837,   604,    72,  1144,   881,   835,   962,
     934,   935,   720,    47,   157,  1149,  1150,  1151,   936,   179,
     937,   162,   944,  -611,   939,   940,   183,   945,   180,   163,
     946,   960,   966,   948,   949,   164,   599,  1169,   165,  1076,
     982,   976,    47,   180,   983,    80,    81,    82,   997,   599,
      84,  1003,    86,    87,   604,   599,  1002,   166,  1011,    47,
     224,   227,     8,   324,   326,   604,  1018,  -197,   604,  1012,
      36,    37,  1013,  1015,  1014,  1032,  1019,  1042,   599,   928,
     604,     8,    47,  1033,  1037,  -611,   604,   604,   604,  1038,
    1055,    47,    47,    47,   255,   439,    38,   445,   816,   432,
     167,  1044,  1073,   604,  1045,  1059,  -611,   334,  1048,   168,
    1064,  1065,   169,    47,  1061,  1081,   206,   170,   371,  -254,
    1087,   171,  -253,   265,  1091,  1092,   163,  1093,   599,  -255,
    1095,  1085,  1097,   226,   313,  1084,  -252,  1105,  1113,   599,
     333,   333,   599,   339,  1112,  1114,  1118,  1121,    36,    37,
     351,   178,  1126,  1127,   599,     8,  1128,  1130,  1131,  1143,
     599,   599,   599,  1134,  1139,  1145,   403,    36,    37,  1141,
     178,  1148,  1101,   428,    38,  1152,   255,   599,    40,  1157,
    1158,   458,  1160,   461,   462,   463,   464,   465,   466,   467,
    1166,  1167,   432,    38,  1168,   390,  1171,   167,  1183,   330,
     863,    27,    28,    29,  1084,  1189,   250,  1185,  1190,   251,
     690,   179,   841,   728,   170,   332,   336,   509,   171,   424,
    1125,   430,   333,   333,  1050,   350,   437,   739,     8,   849,
     440,   157,   255,   449,  1016,   846,   847,   156,   813,   361,
     522,    36,    37,   525,   361,   361,   820,   973,   780,   859,
     752,   361,   182,   383,   384,   626,   361,   992,  1058,  1057,
    1142,  1007,   615,  1077,   335,   432,  1153,    38,  1138,   179,
     828,    40,   396,   349,   851,   361,   399,   355,     0,   496,
     498,   498,     0,     0,   505,     0,   361,   199,     0,     0,
       0,   874,   330,   429,     0,     0,   361,     0,     0,     0,
     513,     0,   515,   442,   473,   474,   475,   476,     0,     0,
     454,   236,     0,     0,    36,    37,   244,     0,     0,   333,
     333,     0,     0,     0,   333,   479,   480,   481,   482,   483,
     484,   485,   486,   487,   488,   489,   544,     0,     0,     0,
      38,     0,     0,   430,    40,     0,   335,     0,     0,   349,
       0,   961,     0,   553,     0,   332,   336,     0,     0,   350,
       0,     0,     0,     0,     8,   330,   565,     0,     0,     0,
       8,   627,   628,   629,   630,   631,   632,   633,   634,   635,
     636,   637,   638,   639,   640,   641,   642,   643,     8,   701,
       0,   196,     0,     0,   529,   532,     0,     0,   498,   498,
     498,   444,     0,     0,   587,     0,   656,   333,   333,   163,
     333,     0,   363,     0,     0,   665,   610,   363,   363,     0,
       0,     0,     0,     0,   363,   371,     0,     0,   702,   363,
       0,   703,     0,   163,     0,     0,     8,     0,     0,     0,
       0,     0,   528,   531,     0,     0,     0,   537,   363,     0,
      36,    37,     0,     0,     0,     0,    36,    37,     0,   363,
     416,     0,     0,     0,     0,   425,   363,   646,     0,   363,
       0,     0,     0,   328,    36,    37,    38,     0,   179,   498,
     167,   334,    38,     0,   659,     0,    40,     0,     0,   250,
     165,  1040,   251,     0,     0,   426,     0,   170,   427,   704,
      38,   171,     0,   170,   167,   333,     0,     0,   333,     0,
       0,     0,     0,   250,     0,     0,   251,   770,   633,   636,
     641,   170,    36,    37,     8,   171,     0,     0,   255,     0,
     528,   531,   255,   537,     0,   179,     0,     0,  1072,     0,
     332,   336,   350,     0,     0,    27,    28,    29,    38,   529,
     532,     0,    40,     0,     0,     0,   179,   255,   333,   350,
       0,   162,   333,     0,     0,     0,     0,     0,     0,   163,
      27,    28,    29,   330,  1094,     0,   591,     8,   165,     0,
     678,     0,     0,     0,   592,   593,   594,  1104,     0,   769,
     361,     0,     0,  1108,     0,     0,   179,     0,     0,     0,
       0,   361,     0,     0,     0,     0,     0,   658,     0,     0,
      36,    37,     0,     0,   371,     0,  1123,   958,   731,     0,
     361,     0,   163,     0,   595,     0,   179,   596,   677,     0,
       0,   682,     0,   333,   333,     0,    38,     0,   333,     0,
     167,     0,     0,   333,     0,   854,     0,     0,   333,   168,
       0,     0,   169,     0,     0,     0,     0,   170,     0,     0,
       0,   171,     0,    36,    37,     0,   874,     0,     0,   931,
     932,   465,     0,   933,     0,     0,     0,  1162,     0,   938,
       0,   658,     0,    68,   255,   677,     0,     0,     0,    38,
       0,     8,     0,   167,     0,     0,     0,     8,   678,     0,
       0,     0,   250,     0,     0,   251,   839,     0,     0,   838,
     170,     0,     0,   179,   171,     0,   333,     0,     0,     0,
       0,     0,     0,     0,     0,   861,     0,     0,   371,     0,
       0,  1041,     0,     0,   371,     0,   163,  1060,     0,    80,
      81,    82,   163,     0,    84,     0,    86,    87,     0,     0,
       0,     0,     0,     0,     8,     0,   789,   790,     0,     0,
     687,   793,     0,   363,   692,   825,   796,     0,     0,   333,
     333,   802,   700,   707,   710,   333,     0,    36,    37,     0,
     840,     0,     0,    36,    37,     0,     0,   255,     0,     0,
       0,   371,     0,   363,  1082,   707,   255,     0,     0,   163,
       0,     0,   743,    38,     0,     0,     0,   167,     0,    38,
       8,     0,     0,   167,     0,     0,   250,     0,     0,   251,
       0,     0,   250,     0,   170,   251,     0,     0,   171,     0,
     170,     0,  1028,  1029,   171,     0,     0,     0,     0,   789,
      36,    37,     0,     0,     0,     0,     0,   328,     0,   255,
       0,     0,   361,   361,     0,   674,     0,  1008,     8,     0,
     361,     0,     0,     0,   165,     0,    38,     0,     0,     0,
     167,     0,     0,     8,     0,   969,   970,   972,     0,   250,
       0,     0,   251,     0,     0,     0,     0,   170,     0,     0,
       0,   171,   942,   943,     0,   328,    36,    37,   947,  1026,
       0,     0,     0,   527,     0,     0,     0,     0,     0,     0,
    1052,   996,   165,     0,     0,     0,   333,     0,   163,     0,
     333,   333,    38,     0,     0,     0,    40,     0,     0,   707,
       8,     0,   255,     0,     0,   675,     8,   823,   676,   707,
       0,     8,     0,     0,    36,    37,     0,   330,     0,     0,
     255,     0,   255,     0,     0,     0,     0,  1107,     0,    36,
      37,     0,     0,     0,     0,     0,     0,   328,     0,   255,
      38,     0,     0,   432,    40,  1069,     0,     0,   432,     0,
       0,   346,     0,  1124,   165,    38,   534,     0,     0,   167,
     255,     0,     0,     0,     0,   330,     0,     8,   250,     0,
       0,   251,   179,     0,   333,   333,   170,     0,     0,     0,
     171,     0,     0,     0,     0,     0,    36,    37,     0,     0,
       0,   179,    36,    37,   610,   363,   363,    36,    37,     0,
     707,   956,     0,   363,   307,     0,     0,   996,     0,  1031,
       0,     0,    38,  1034,  1035,     0,    40,     0,    38,  1119,
       0,   165,    40,    38,     0,   502,   823,    40,   503,   255,
       0,   435,     0,     0,   436,     0,   535,   330,   887,   536,
    -611,    64,     0,   330,     0,    65,    66,    67,   330,     0,
       0,     0,     0,    36,    37,  1069,     0,     0,    68,  -611,
    -611,  -611,  -611,  -611,  -611,  -611,  -611,  -611,  -611,  -611,
    -611,     0,  -611,  -611,  -611,  -611,  -611,     0,     0,    38,
     888,    70,     0,    40,  -611,     0,  -611,  -611,  -611,  -611,
    -611,     0,   502,     0,     0,   503,     0,  1089,  1090,    72,
      73,    74,    75,   889,    77,    78,    79,  -611,  -611,  -611,
     890,   891,   892,     0,    80,   893,    82,     0,    83,    84,
      85,    86,    87,  -611,  -611,     0,  -611,  -611,    88,     0,
       8,     0,    92,   196,    94,    95,    96,    97,    98,    99,
     275,   276,   277,   278,   279,   280,   281,   282,   707,     0,
     100,     0,  -611,     0,     0,   101,  -611,  -611,     0,   271,
       0,   894,   196,   272,     0,     8,     0,   273,   274,   275,
     276,   277,   278,   279,   280,   281,   282,   895,     9,    10,
      11,    12,    13,    14,    15,    16,    17,    18,    19,    20,
       0,    21,    22,    23,    24,    25,   283,     0,     0,     0,
       0,     0,   328,     0,    26,    27,    28,    29,    30,    31,
     530,   284,     0,     0,     0,     0,    36,    37,     0,   165,
       0,     0,     0,     0,     0,     0,    32,    33,    34,     0,
       0,     0,   316,   317,     0,   473,   474,   475,   476,     0,
     477,     0,    35,     0,     0,    36,    37,     0,     0,     8,
       0,    36,    37,     0,     0,   478,   479,   480,   481,   482,
     483,   484,   485,   486,   487,   488,   489,     0,     0,     0,
       0,    38,     0,     0,    39,    40,     0,    38,     0,     0,
      41,    40,     0,     0,   285,     0,   432,   286,     0,     0,
     287,   288,   289,     0,   674,   271,   290,   291,   196,   272,
       0,     8,   330,   273,   274,   275,   276,   277,   278,   279,
     280,   281,   282,     0,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,     0,    21,    22,    23,
      24,    25,   283,     0,     0,    36,    37,     0,   432,   781,
       0,    27,    28,    29,    30,    31,   527,   284,     0,     0,
     323,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    38,    32,    33,    34,    40,     0,     0,     0,     0,
     473,   474,   475,   476,   675,   477,     0,   676,    35,     0,
       0,    36,    37,     0,     0,     8,   330,    36,    37,     0,
     478,   479,   480,   481,   482,   483,   484,   485,   486,   487,
     488,   489,     0,     0,     0,     0,     0,    38,     0,     0,
       0,    40,     0,    38,     0,     0,     0,    40,     0,     0,
     285,     0,   432,   286,     0,     0,   287,   288,   289,     0,
     530,   271,   290,   291,   196,   272,     0,     8,   330,   273,
     274,   275,   276,   277,   278,   279,   280,   281,   282,     0,
       9,    10,    11,    12,    13,    14,    15,    16,    17,    18,
      19,    20,     0,    21,    22,    23,    24,    25,   283,     0,
       0,    36,    37,     0,   432,     0,     0,    27,    28,    29,
      30,    31,   681,   284,     0,     0,   521,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    38,    32,    33,
      34,    40,     0,     0,     0,   473,   474,   475,   476,     0,
       0,     0,     0,     0,    35,     0,     0,    36,    37,     0,
       0,     8,   330,    36,    37,   478,   479,   480,   481,   482,
     483,   484,   485,   486,   487,   488,   489,     0,     0,     0,
       0,     0,     0,    38,     0,     0,     0,    40,     0,    38,
       0,     0,     0,    40,     0,     0,   285,     0,   432,   286,
       0,     0,   287,   288,   289,     0,   801,   271,   290,   291,
     196,   272,     0,   619,   330,   273,   274,   275,   276,   277,
     278,   279,   280,   281,   282,     0,     9,    10,    11,    12,
      13,    14,    15,    16,    17,    18,    19,    20,     0,    21,
      22,    23,    24,    25,   283,     0,     0,    36,    37,     0,
       0,     0,     0,    27,    28,    29,    30,    31,   196,   284,
       0,     0,   524,     0,     0,   275,   276,   277,   278,   279,
     280,   281,   282,    38,    32,    33,    34,    40,     0,   473,
     474,   475,   476,     0,   477,     0,   473,   474,   475,   476,
      35,     0,     0,    36,    37,     0,     0,     0,   330,   478,
     620,   480,   481,   621,   483,   484,   485,   486,   622,   488,
     489,   483,   484,   485,   486,   487,   488,   489,     0,    38,
       0,     0,     0,    40,     0,     0,     0,     0,     0,     0,
       0,     0,   285,     0,     0,   286,     0,     0,   287,   288,
     289,     0,     0,   271,   290,   291,   196,   272,   984,     0,
       0,   273,   274,   275,   276,   277,   278,   279,   280,   281,
     282,     0,     9,    10,    11,    12,    13,    14,    15,    16,
      17,    18,    19,    20,     0,    21,    22,    23,    24,    25,
     283,     0,     0,     0,     0,     0,     0,     0,     0,    27,
      28,    29,    30,    31,     0,   284,     0,     0,   664,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      32,    33,    34,     0,   473,   474,   475,   476,     0,   477,
       0,     0,   473,   474,   475,   476,    35,     0,     0,    36,
      37,     0,     0,     0,   478,   479,   480,   481,   482,   483,
     484,   485,   486,   487,   488,   489,   482,   483,   484,   485,
     486,   487,   488,   489,     0,    38,     0,     0,     0,    40,
       0,     0,     0,     0,     0,     0,     0,     0,   285,     0,
       0,   286,     0,     0,   287,   288,   289,     0,     0,   271,
     290,   291,   196,   272,     0,  1147,     0,   273,   274,   275,
     276,   277,   278,   279,   280,   281,   282,     0,     9,    10,
      11,    12,    13,    14,    15,    16,    17,    18,    19,    20,
       0,    21,    22,    23,    24,    25,   283,   771,     0,     0,
       0,     0,     0,     0,     0,    27,    28,    29,    30,    31,
       0,   284,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    32,    33,    34,   473,
     474,   475,   476,     0,   477,     0,     0,     0,   473,   474,
     475,   476,    35,     0,     0,    36,    37,     0,     0,   478,
     479,   480,   481,   482,   483,   484,   485,   486,   487,   488,
     489,   481,   482,   483,   484,   485,   486,   487,   488,   489,
       0,    38,     0,     0,     0,    40,     0,     0,     0,     0,
       0,     0,     0,     0,   285,     0,     0,   286,     0,     0,
     287,   288,   289,     0,     0,   271,   290,   291,   196,   272,
       0,     0,     0,   273,   274,   275,   276,   277,   278,   279,
     280,   281,   282,     0,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,   871,    21,    22,    23,
      24,    25,   283,   772,     0,    27,    28,    29,     0,     0,
     872,    27,    28,    29,    30,    31,     0,   284,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    32,    33,    34,     0,   591,   473,   474,   475,
     476,     0,     0,     0,   592,   593,   594,     0,    35,     0,
       0,    36,    37,     0,     0,     0,     0,     0,     0,   480,
     481,   482,   483,   484,   485,   486,   487,   488,   489,     0,
       0,     0,     0,     0,     0,     0,     0,    38,     0,     0,
       0,    40,     0,     0,   595,     0,     0,   596,     0,     0,
     285,     0,     0,   286,     0,     0,   287,   288,   289,     0,
       0,   271,   290,   291,   196,   272,     0,     0,     0,   273,
     274,   275,   276,   277,   278,   279,   280,   281,   282,     0,
       9,    10,    11,    12,    13,    14,    15,    16,    17,    18,
      19,    20,     0,    21,    22,    23,    24,    25,   283,     0,
       0,     0,     0,     0,     0,     0,     0,    27,    28,    29,
      30,    31,     0,   284,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   473,   474,     0,     0,    32,    33,
      34,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    35,     0,     0,    36,    37,   483,
     484,   485,   486,   487,   488,   489,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     9,    10,    11,    12,    13,
      14,    15,    16,    38,    18,     0,    20,    40,     0,    22,
      23,    24,    25,     0,     0,     0,   285,     0,     0,   286,
       0,     0,   287,   288,   289,     0,     0,   271,   290,   291,
     196,   272,     0,     0,     0,   273,   274,   275,   276,   277,
     278,   279,   280,   281,   282,     0,     9,    10,    11,    12,
      13,    14,    15,    16,    17,    18,    19,    20,     0,    21,
      22,    23,    24,    25,   283,     0,     0,     0,     0,     0,
       0,     0,     0,    27,    28,    29,    30,    31,     0,   284,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    32,    33,    34,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    64,
      35,     0,     0,    36,    37,    67,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    68,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    38,
       0,     0,     0,    40,     0,     0,     0,     0,   888,    70,
       0,     0,     0,     0,     0,     0,     0,     0,   287,   288,
     773,     0,     0,     0,   290,   291,     0,    72,    73,    74,
      75,     0,    77,    78,    79,     0,     0,     0,   890,   891,
     892,     0,    80,   893,    82,     0,    83,    84,    85,    86,
      87,     0,     0,     0,     0,     0,    88,     0,     0,     0,
      92,     0,    94,    95,    96,    97,    98,    99,     0,     0,
       0,     0,     0,     0,     8,     0,     0,     0,   100,     0,
       0,     0,     0,   101,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     9,    10,    11,    12,    13,    14,    15,
      16,    17,    18,    19,    20,   895,    21,    22,    23,    24,
      25,   307,     0,     0,     0,     0,     0,     0,     0,    26,
      27,    28,    29,    30,    31,     0,     0,     0,   165,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    32,    33,    34,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    35,     0,     0,
      36,    37,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    38,     0,     0,    39,
      40,     8,     0,     0,     0,    41,     0,     0,     0,   308,
       0,     0,   309,     0,     0,     0,     0,   170,     0,     0,
       9,    10,    11,    12,    13,    14,    15,    16,    17,    18,
      19,    20,     0,    21,    22,    23,    24,    25,   307,     0,
       0,     0,     0,     0,     0,     0,    26,    27,    28,    29,
      30,    31,     0,     0,     0,   165,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    32,    33,
      34,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    35,     0,     0,    36,    37,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    38,     0,     0,    39,    40,     8,     0,
     510,     0,    41,     0,     0,     0,   493,     0,     0,   494,
       0,     0,     0,     0,   170,     0,     0,     9,    10,    11,
      12,    13,    14,    15,    16,    17,    18,    19,    20,     0,
      21,    22,    23,    24,    25,     0,     0,     0,     0,     0,
       0,     0,     0,    26,    27,    28,    29,    30,    31,   473,
     474,   475,   476,     0,   477,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    32,    33,    34,     8,   478,
     479,   480,   481,   482,   483,   484,   485,   486,   487,   488,
     489,    35,     0,     0,    36,    37,     0,     9,    10,    11,
      12,    13,    14,    15,    16,    17,    18,    19,    20,     0,
      21,    22,    23,    24,    25,     0,     0,     0,     0,     0,
      38,     0,     0,    39,    40,     0,     0,    30,    31,    41,
       0,     0,     0,   426,     0,     0,   427,     0,     0,     0,
       0,   170,     0,     0,     0,    32,    33,    34,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    35,     0,     0,    36,    37,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    -2,    63,     0,
    -611,    64,     0,     0,     0,    65,    66,    67,     0,     0,
      38,     0,     0,     0,    40,     0,     0,     0,    68,  -611,
    -611,  -611,  -611,  -611,  -611,  -611,  -611,  -611,  -611,  -611,
    -611,   170,  -611,  -611,  -611,  -611,  -611,     0,     0,     0,
      69,    70,     0,     0,     0,     0,  -611,  -611,  -611,  -611,
    -611,     0,     0,    71,     0,     0,     0,     0,     0,    72,
      73,    74,    75,    76,    77,    78,    79,  -611,  -611,  -611,
       0,     0,     0,     0,    80,    81,    82,     0,    83,    84,
      85,    86,    87,  -611,  -611,     0,  -611,  -611,    88,    89,
      90,    91,    92,    93,    94,    95,    96,    97,    98,    99,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     100,    63,  -611,  -611,    64,   101,  -611,     0,    65,    66,
      67,   102,   103,     0,     0,     0,     0,     0,     0,     0,
       0,    68,  -611,  -611,  -611,  -611,  -611,  -611,  -611,  -611,
    -611,  -611,  -611,  -611,     0,  -611,  -611,  -611,  -611,  -611,
       0,     0,     0,    69,    70,     0,     0,   717,     0,  -611,
    -611,  -611,  -611,  -611,     0,     0,    71,     0,     0,     0,
       0,     0,    72,    73,    74,    75,    76,    77,    78,    79,
    -611,  -611,  -611,     0,     0,     0,     0,    80,    81,    82,
       0,    83,    84,    85,    86,    87,  -611,  -611,     0,  -611,
    -611,    88,    89,    90,    91,    92,    93,    94,    95,    96,
      97,    98,    99,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   100,    63,  -611,  -611,    64,   101,  -611,
       0,    65,    66,    67,   102,   103,     0,     0,     0,     0,
       0,     0,     0,     0,    68,  -611,  -611,  -611,  -611,  -611,
    -611,  -611,  -611,  -611,  -611,  -611,  -611,     0,  -611,  -611,
    -611,  -611,  -611,     0,     0,     0,    69,    70,     0,     0,
     807,     0,  -611,  -611,  -611,  -611,  -611,     0,     0,    71,
       0,     0,     0,     0,     0,    72,    73,    74,    75,    76,
      77,    78,    79,  -611,  -611,  -611,     0,     0,     0,     0,
      80,    81,    82,     0,    83,    84,    85,    86,    87,  -611,
    -611,     0,  -611,  -611,    88,    89,    90,    91,    92,    93,
      94,    95,    96,    97,    98,    99,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   100,    63,  -611,  -611,
      64,   101,  -611,     0,    65,    66,    67,   102,   103,     0,
       0,     0,     0,     0,     0,     0,     0,    68,  -611,  -611,
    -611,  -611,  -611,  -611,  -611,  -611,  -611,  -611,  -611,  -611,
       0,  -611,  -611,  -611,  -611,  -611,     0,     0,     0,    69,
      70,     0,     0,   822,     0,  -611,  -611,  -611,  -611,  -611,
       0,     0,    71,     0,     0,     0,     0,     0,    72,    73,
      74,    75,    76,    77,    78,    79,  -611,  -611,  -611,     0,
       0,     0,     0,    80,    81,    82,     0,    83,    84,    85,
      86,    87,  -611,  -611,     0,  -611,  -611,    88,    89,    90,
      91,    92,    93,    94,    95,    96,    97,    98,    99,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   100,
      63,  -611,  -611,    64,   101,  -611,     0,    65,    66,    67,
     102,   103,     0,     0,     0,     0,     0,     0,     0,     0,
      68,  -611,  -611,  -611,  -611,  -611,  -611,  -611,  -611,  -611,
    -611,  -611,  -611,     0,  -611,  -611,  -611,  -611,  -611,     0,
       0,     0,    69,    70,     0,     0,     0,     0,  -611,  -611,
    -611,  -611,  -611,     0,     0,    71,     0,     0,     0,   981,
       0,    72,    73,    74,    75,    76,    77,    78,    79,  -611,
    -611,  -611,     0,     0,     0,     0,    80,    81,    82,     0,
      83,    84,    85,    86,    87,  -611,  -611,     0,  -611,  -611,
      88,    89,    90,    91,    92,    93,    94,    95,    96,    97,
      98,    99,     0,     0,     7,     0,     8,     0,   668,     0,
       0,     0,   100,     0,  -611,     0,     0,   101,  -611,     0,
       0,     0,     0,   102,   103,     9,    10,    11,    12,    13,
      14,    15,    16,    17,    18,    19,    20,     0,    21,    22,
      23,    24,    25,     0,     0,     0,     0,     0,     0,     0,
       0,    26,    27,    28,    29,    30,    31,   473,   474,   475,
     476,     0,   477,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    32,    33,    34,     0,   478,   479,   480,
     481,   482,   483,   484,   485,   486,   487,   488,   489,    35,
       0,     0,    36,    37,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    58,     0,     8,
       0,     0,     0,     0,     0,     0,     0,     0,    38,     0,
       0,    39,    40,     0,     0,     0,     0,    41,     9,    10,
      11,    12,    13,    14,    15,    16,    17,    18,    19,    20,
       0,    21,    22,    23,    24,    25,     0,     0,     0,     0,
       0,     0,     0,     0,    26,    27,    28,    29,    30,    31,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    32,    33,    34,     0,
       0,     8,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    35,     0,     0,    36,    37,     0,     0,     0,
       9,    10,    11,    12,    13,    14,    15,    16,   855,    18,
     856,    20,     8,   857,    22,    23,    24,    25,     0,     0,
       0,    38,     0,     0,    39,    40,     0,     0,     0,     0,
      41,     9,    10,    11,    12,    13,    14,    15,    16,    17,
      18,    19,    20,     0,    21,    22,    23,    24,    25,     0,
       0,     0,     0,     0,     0,     0,     0,    26,    27,    28,
      29,    30,    31,     0,    35,     0,     0,    36,    37,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    32,
      33,    34,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    38,     0,    35,     0,    40,    36,    37,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     8,     0,   670,     0,
       0,     0,     0,     0,    38,     0,   386,    39,    40,     0,
       0,     0,     0,    41,   541,     9,    10,    11,    12,    13,
      14,    15,    16,    17,    18,    19,    20,     0,    21,    22,
      23,    24,    25,     0,     0,     0,     0,     0,     0,     0,
       0,    26,    27,    28,    29,    30,    31,   473,   474,   475,
     476,     0,   477,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    32,    33,    34,     0,   478,   479,   480,
     481,   482,   483,   484,   485,   486,   487,   488,   489,    35,
       0,     0,    36,    37,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     8,
       0,   786,     0,     0,     0,     0,     0,     0,    38,     0,
       0,    39,    40,     0,     0,     0,     0,    41,     9,    10,
      11,    12,    13,    14,    15,    16,    17,    18,    19,    20,
       0,    21,    22,    23,    24,    25,     0,     0,     0,     0,
       0,     0,     0,     0,    26,    27,    28,    29,    30,    31,
     473,   474,   475,   476,     0,   477,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    32,    33,    34,     0,
     478,   479,   480,   481,   482,   483,   484,   485,   486,   487,
     488,   489,    35,     0,     0,    36,    37,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     203,     0,     8,     0,     0,     0,     0,     0,     0,     0,
       0,    38,     0,     0,    39,    40,     0,     0,     0,     0,
      41,     9,    10,    11,    12,    13,    14,    15,    16,    17,
      18,    19,    20,     0,    21,    22,    23,    24,    25,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    27,    28,
      29,    30,    31,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    32,
      33,    34,     0,     8,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    35,     0,     0,    36,    37,
       0,     0,     9,    10,    11,    12,    13,    14,    15,    16,
      17,    18,    19,    20,     0,    21,    22,    23,    24,    25,
       0,     0,     0,     0,    38,     0,     0,     0,    40,    27,
      28,    29,    30,    31,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      32,    33,    34,     0,     0,     8,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    35,   978,     0,    36,
      37,     0,     0,     0,     9,    10,    11,    12,    13,    14,
      15,    16,  1021,    18,  1022,    20,     0,  1023,    22,    23,
      24,    25,     0,     0,     0,    38,     0,     0,     0,    40,
     979,    27,    28,    29,    30,    31,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    32,    33,    34,     0,     0,     0,     8,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    35,   258,
       0,    36,    37,     0,     0,     0,     0,     9,    10,    11,
      12,    13,    14,    15,    16,    17,    18,    19,    20,     0,
      21,    22,    23,    24,    25,     0,     0,    38,     0,     0,
       0,    40,   979,    26,    27,    28,    29,    30,    31,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    32,    33,    34,     0,     8,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    35,     0,     0,    36,    37,     0,     0,     9,    10,
      11,    12,    13,    14,    15,    16,    17,    18,    19,    20,
       0,    21,    22,    23,    24,    25,   237,     0,     0,     0,
      38,     0,     0,    39,    40,    27,    28,    29,    30,    31,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    32,    33,    34,     0,
       8,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    35,     0,     0,    36,    37,     0,     0,     9,
      10,    11,    12,    13,    14,    15,    16,    17,    18,    19,
      20,     0,    21,    22,    23,    24,    25,     0,     0,     0,
       0,    38,     0,     0,     0,    40,    27,    28,    29,    30,
      31,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    32,    33,    34,
       0,     8,     0,   792,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    35,   258,     0,    36,    37,     0,     0,
       9,    10,    11,    12,    13,    14,    15,    16,    17,    18,
      19,    20,     0,    21,    22,    23,    24,    25,     0,     0,
       0,     0,    38,     0,     0,     0,    40,    27,    28,    29,
      30,    31,   473,   474,   475,   476,     0,   477,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    32,    33,
      34,     8,   478,   479,   480,   481,   482,   483,   484,   485,
     486,   487,   488,   489,    35,     0,     0,    36,    37,     0,
       9,    10,    11,    12,    13,    14,    15,    16,    17,    18,
      19,    20,     0,    21,    22,    23,    24,    25,     0,     0,
       0,     0,     0,    38,     0,     0,     0,    40,     0,     0,
      30,    31,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    32,    33,
      34,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    35,     0,     0,    36,    37,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    38,     0,     0,     0,    40
};

static const yytype_int16 yycheck[] =
{
       0,   164,   152,     5,    95,     5,   246,    72,    47,   374,
     154,     1,     2,   162,   144,   563,    40,   195,    45,     5,
     557,   597,   624,   201,   592,   578,    61,   711,   811,   207,
      95,    96,   401,   393,   212,   185,   186,     5,   231,   753,
       5,   542,     1,     2,   547,     6,   667,    47,   669,    40,
       6,     5,    41,   986,   557,     3,   249,    40,   269,    41,
       6,     5,   998,    42,   242,     3,    42,    54,    43,  1104,
       1,    71,     5,    47,   252,     3,    76,    77,     5,   144,
       5,     0,    41,    76,    48,     5,    52,    53,     5,     5,
     239,    46,    84,    85,     6,     5,     5,    49,    50,    51,
      62,    76,    61,    21,    54,    71,     5,     3,     5,    40,
       5,    41,     5,    59,   114,     5,   103,     5,     5,  1154,
      46,   114,    48,     5,    85,   104,  1062,   116,   101,    85,
       4,     3,     6,     3,   116,   174,   919,   176,     5,  1072,
     131,  1054,    54,   167,   355,   128,   929,   111,   131,   125,
       3,    89,    90,   136,   154,   103,   111,   118,   307,   712,
       6,    89,    90,   125,   154,     3,   116,    85,    40,    43,
      40,   144,    46,     4,   174,  1108,   176,     3,   213,   328,
     153,   216,   683,   759,   136,   545,   124,    57,     3,   810,
    1123,   119,   192,    89,    90,   154,     4,    48,   128,   568,
      46,   131,   237,   162,  1117,     3,   566,    42,   208,   830,
     245,   374,    46,   927,    40,    46,    43,    89,    90,    89,
      90,  1136,    48,   119,  1137,   198,     3,  1010,    54,    43,
     204,    57,    46,  1146,   160,     3,    89,    90,    46,   239,
     114,   241,   242,   115,   170,   115,   246,   119,    47,   119,
      76,    89,    90,    48,   213,  1170,   246,   216,   297,   259,
     813,   261,   115,    89,    90,   239,   119,     6,   140,   104,
     140,   105,   106,   957,    89,    90,   103,   814,   237,     3,
     118,   119,     6,   136,    48,    43,   245,   246,    46,   115,
     858,    89,    90,   119,    77,   295,  1079,   297,    54,    82,
     115,     3,   128,    54,   119,   131,   341,    46,    76,   344,
     136,   814,    89,    90,   140,    43,    40,   408,    76,   122,
      76,    89,    90,   297,    48,   469,    54,   392,    40,    43,
      52,    53,    46,   372,   308,   309,   401,    44,   312,    40,
     580,   118,   119,   408,   103,   103,     3,   115,   307,    71,
      72,   119,   103,   546,    52,    53,    52,   506,     4,     6,
       6,     3,    76,   162,     6,    89,    90,   357,   142,   328,
      77,   170,   372,    71,    72,    71,  1159,    89,    90,   557,
     740,   103,   341,   576,     3,   344,   117,    89,    90,    46,
      92,   115,   103,   393,   459,   119,    43,   117,   357,   434,
      46,   204,    40,    41,   128,   103,     3,   131,   961,   444,
      40,   142,   136,   115,   563,   983,   140,   119,    48,    57,
     420,    40,   142,   105,   106,    40,   352,     1,  1030,    48,
     454,   125,    89,    90,    42,   584,   239,     3,    57,   439,
     239,   441,    57,    40,    40,   445,    54,    89,    90,   449,
     249,    48,   426,   427,   428,   258,   128,   260,   115,   131,
      57,    57,   119,   105,   106,    49,    50,    51,    42,    43,
      89,    90,    46,   115,    40,   434,   511,   119,    40,   469,
      40,   516,    48,   456,    43,   444,   128,    40,     4,   131,
       6,    57,    89,    90,   136,    57,   115,    57,   297,    40,
     119,  1049,   120,   568,    57,   654,    43,   872,   307,   128,
     469,     3,   131,   439,    40,   550,    57,   136,   115,   445,
      46,   140,   119,    89,    90,    43,    40,    43,    46,   328,
      46,   128,    46,   507,   131,    52,    53,   572,    54,   338,
      41,    42,   542,   140,   128,   545,     4,   131,     6,   115,
      43,   541,   511,   119,    71,    72,     3,   516,  1134,    40,
      41,    40,   564,   563,   564,    40,   566,   567,    43,   559,
     103,    40,   371,   573,   140,    40,    57,    40,   564,   606,
     506,   567,   541,   116,   117,   624,    40,   577,    57,   563,
     580,   550,   741,    40,    89,    90,   564,    89,    90,   564,
     559,    48,    49,    50,    51,    40,    41,    41,    42,   142,
     564,   611,   612,   572,    43,   808,   809,    46,   577,    40,
     564,   580,    57,   115,   624,    54,   626,   119,  1160,     3,
      40,   564,     6,   432,  1166,  1167,  1168,   564,   816,   564,
      40,    41,    89,    90,   564,   444,    84,   564,   564,    43,
     624,  1183,    46,    40,   564,   564,   721,    57,   584,   458,
      54,    40,    41,    43,    40,   564,    46,   564,   115,   564,
      43,   564,   119,    46,   564,    40,   564,   564,    57,    40,
      40,   128,   564,   683,   131,     4,   688,     6,   688,    40,
     725,   835,    43,   140,    40,    46,   622,   564,   969,    41,
      42,   972,   688,   506,     6,    40,   706,   506,    48,   872,
      48,   746,    48,   116,   714,    89,    90,   719,    80,   719,
     688,   647,    48,   688,    41,    43,    88,    89,    90,    42,
     769,   105,   106,   719,   688,   661,   827,   977,   738,   739,
     740,   667,   742,   669,   688,   895,    46,   774,    49,    50,
      51,   719,   738,   556,   719,   688,   758,   560,   758,   759,
     563,   688,   827,   688,   563,   719,   725,   767,   688,   769,
     738,   688,   688,   738,     6,   719,    46,   812,   688,   688,
     706,   584,   585,   848,   738,   584,   719,   746,   714,   688,
      43,   688,   719,   688,   719,   688,    42,   927,   688,   719,
     688,   688,   719,   719,   116,     3,   688,    48,     6,   719,
     719,   811,    48,    41,  1179,   104,   742,   111,   104,   819,
     719,   688,   719,   622,   719,   624,   719,   862,    42,   719,
      40,   719,   719,   759,     7,   835,   871,   719,    41,  1002,
     121,   122,   123,    41,    41,   835,    48,   773,   850,   116,
     850,   654,   719,   812,    40,   654,    57,    48,   858,    48,
       3,   136,   927,   865,   850,  1015,    48,   840,   136,   137,
     138,   116,   937,    43,   874,   848,   835,    73,    74,    75,
      41,   881,   850,    41,   810,   850,   134,   135,   136,   137,
     138,    89,    90,   819,   894,     3,   850,    40,    54,    41,
    1049,    41,    41,   862,   830,    48,   850,   105,   106,   712,
      42,   976,   871,   952,   979,    41,    43,   850,    41,   919,
      41,    41,    41,   850,    41,   850,    41,   104,     3,   929,
     850,    42,    40,   850,   850,   116,    41,    43,   741,    76,
     850,   850,   741,    46,   116,     3,    89,    90,    48,    57,
       3,   850,   952,   850,   927,   850,     3,   850,    48,   959,
     850,   116,   850,   850,   116,    40,    48,    41,   850,    41,
      48,    48,   115,    48,   773,   774,   119,   977,    48,    48,
      48,    89,    90,   850,    43,   128,   986,   977,   131,    43,
      46,  1030,   994,   136,   994,    41,   998,   140,   998,   132,
     133,   134,   135,   136,   137,   138,  1006,   115,  1008,  1009,
    1010,   119,   815,  1176,    89,    90,  1179,  1052,   977,    46,
     128,   824,    43,   131,    40,    43,    91,    47,    41,    40,
    1030,   111,   140,   959,    57,    90,    41,    89,    78,    46,
     115,    41,  1042,    41,   119,    48,  1081,    41,    41,  1049,
      42,   439,    48,    48,    42,  1055,  1030,   445,    48,    48,
    1062,  1061,  1062,  1098,     3,   140,    48,    48,    48,    21,
    1135,    41,  1072,    40,   877,  1049,    54,    43,    41,  1079,
    1006,   142,    41,    47,  1084,    62,  1121,    42,   103,    43,
      41,    41,    44,  1052,    42,  1130,  1131,  1132,    41,    47,
      40,    40,    48,    42,    41,    41,    54,    48,  1108,    48,
      48,    41,    41,    48,    48,    54,  1042,  1152,    57,  1119,
      43,    54,  1081,  1123,    41,    77,    78,    79,    47,  1055,
      82,    76,    84,    85,  1134,  1061,    54,    76,    46,  1098,
      88,    89,     3,   165,   166,  1145,    47,    47,  1148,    76,
      89,    90,    76,   140,    76,    48,   125,   960,  1084,  1159,
    1160,     3,  1121,    48,    47,   104,  1166,  1167,  1168,    41,
      41,  1130,  1131,  1132,   122,   978,   115,   980,    42,    40,
     119,    43,    48,  1183,    43,    54,   125,    48,   104,   128,
      41,    43,   131,  1152,   997,    40,    72,   136,    40,    76,
      47,   140,    76,   151,    43,    43,    48,    43,  1134,    76,
      43,    76,   104,    89,   162,  1018,    76,    43,    42,  1145,
     168,   169,  1148,   171,    43,    48,    48,    40,    89,    90,
     178,  1030,    41,    43,  1160,     3,    43,    40,    40,    47,
    1166,  1167,  1168,    41,    48,    41,  1049,    89,    90,    48,
    1049,    41,  1051,  1052,   115,    40,   204,  1183,   119,    48,
      48,   283,    41,   285,   286,   287,   288,   289,   290,   291,
      41,    41,    40,   115,    41,   223,    48,   119,    41,   140,
      46,    49,    50,    51,  1087,    43,   128,    48,    43,   131,
     547,   239,   738,   573,   136,   168,   169,   319,   140,   247,
    1087,   249,   250,   251,   977,   178,   254,   580,     3,   739,
     258,   259,   260,   261,   894,   738,   738,    41,   706,   195,
     342,    89,    90,   345,   200,   201,   714,   835,   626,   753,
     599,   207,    52,   209,   210,   469,   212,   865,   994,   993,
    1119,   881,   449,  1009,   169,    40,  1134,   115,  1113,   297,
     722,   119,   228,   178,   742,   231,   232,   186,    -1,   307,
     308,   309,    -1,    -1,   312,    -1,   242,    70,    -1,    -1,
      -1,   759,   140,   249,    -1,    -1,   252,    -1,    -1,    -1,
     328,    -1,   330,   259,   107,   108,   109,   110,    -1,    -1,
     266,    94,    -1,    -1,    89,    90,    99,    -1,    -1,   347,
     348,    -1,    -1,    -1,   352,   128,   129,   130,   131,   132,
     133,   134,   135,   136,   137,   138,   364,    -1,    -1,    -1,
     115,    -1,    -1,   371,   119,    -1,   251,    -1,    -1,   254,
      -1,   819,    -1,   381,    -1,   308,   309,    -1,    -1,   312,
      -1,    -1,    -1,    -1,     3,   140,   394,    -1,    -1,    -1,
       3,   473,   474,   475,   476,   477,   478,   479,   480,   481,
     482,   483,   484,   485,   486,   487,   488,   489,     3,     4,
      -1,     6,    -1,    -1,   347,   348,    -1,    -1,   426,   427,
     428,    40,    -1,    -1,   432,    -1,   508,   435,   436,    48,
     438,    -1,   195,    -1,    -1,   517,   444,   200,   201,    -1,
      -1,    -1,    -1,    -1,   207,    40,    -1,    -1,    43,   212,
      -1,    46,    -1,    48,    -1,    -1,     3,    -1,    -1,    -1,
      -1,    -1,   347,   348,    -1,    -1,    -1,   352,   231,    -1,
      89,    90,    -1,    -1,    -1,    -1,    89,    90,    -1,   242,
     243,    -1,    -1,    -1,    -1,   248,   249,   495,    -1,   252,
      -1,    -1,    -1,    40,    89,    90,   115,    -1,   506,   507,
     119,    48,   115,    -1,   512,    -1,   119,    -1,    -1,   128,
      57,   959,   131,    -1,    -1,   128,    -1,   136,   131,   114,
     115,   140,    -1,   136,   119,   533,    -1,    -1,   536,    -1,
      -1,    -1,    -1,   128,    -1,    -1,   131,   619,   620,   621,
     622,   136,    89,    90,     3,   140,    -1,    -1,   556,    -1,
     435,   436,   560,   438,    -1,   563,    -1,    -1,  1006,    -1,
     493,   494,   495,    -1,    -1,    49,    50,    51,   115,   502,
     503,    -1,   119,    -1,    -1,    -1,   584,   585,   586,   512,
      -1,    40,   590,    -1,    -1,    -1,    -1,    -1,    -1,    48,
      49,    50,    51,   140,  1042,    -1,    80,     3,    57,    -1,
     533,    -1,    -1,    -1,    88,    89,    90,  1055,    -1,   617,
     546,    -1,    -1,  1061,    -1,    -1,   624,    -1,    -1,    -1,
      -1,   557,    -1,    -1,    -1,    -1,    -1,   512,    -1,    -1,
      89,    90,    -1,    -1,    40,    -1,  1084,    43,   574,    -1,
     576,    -1,    48,    -1,   128,    -1,   654,   131,   533,    -1,
      -1,   536,    -1,   661,   662,    -1,   115,    -1,   666,    -1,
     119,    -1,    -1,   671,    -1,   747,    -1,    -1,   676,   128,
      -1,    -1,   131,    -1,    -1,    -1,    -1,   136,    -1,    -1,
      -1,   140,    -1,    89,    90,    -1,  1134,    -1,    -1,   771,
     772,   773,    -1,   775,    -1,    -1,    -1,  1145,    -1,   781,
      -1,   586,    -1,    21,   712,   590,    -1,    -1,    -1,   115,
      -1,     3,    -1,   119,    -1,    -1,    -1,     3,   651,    -1,
      -1,    -1,   128,    -1,    -1,   131,    44,    -1,    -1,   737,
     136,    -1,    -1,   741,   140,    -1,   744,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   753,    -1,    -1,    40,    -1,
      -1,    43,    -1,    -1,    40,    -1,    48,    43,    -1,    77,
      78,    79,    48,    -1,    82,    -1,    84,    85,    -1,    -1,
      -1,    -1,    -1,    -1,     3,    -1,   661,   662,    -1,    -1,
     543,   666,    -1,   546,   547,   721,   671,    -1,    -1,   797,
     798,   676,   555,   556,   557,   803,    -1,    89,    90,    -1,
     118,    -1,    -1,    89,    90,    -1,    -1,   815,    -1,    -1,
      -1,    40,    -1,   576,    43,   578,   824,    -1,    -1,    48,
      -1,    -1,   585,   115,    -1,    -1,    -1,   119,    -1,   115,
       3,    -1,    -1,   119,    -1,    -1,   128,    -1,    -1,   131,
      -1,    -1,   128,    -1,   136,   131,    -1,    -1,   140,    -1,
     136,    -1,   934,   935,   140,    -1,    -1,    -1,    -1,   744,
      89,    90,    -1,    -1,    -1,    -1,    -1,    40,    -1,   877,
      -1,    -1,   808,   809,    -1,    48,    -1,   885,     3,    -1,
     816,    -1,    -1,    -1,    57,    -1,   115,    -1,    -1,    -1,
     119,    -1,    -1,     3,    -1,   831,   832,   833,    -1,   128,
      -1,    -1,   131,    -1,    -1,    -1,    -1,   136,    -1,    -1,
      -1,   140,   797,   798,    -1,    40,    89,    90,   803,   927,
      -1,    -1,    -1,    48,    -1,    -1,    -1,    -1,    -1,    -1,
      40,   867,    57,    -1,    -1,    -1,   944,    -1,    48,    -1,
     948,   949,   115,    -1,    -1,    -1,   119,    -1,    -1,   712,
       3,    -1,   960,    -1,    -1,   128,     3,   720,   131,   722,
      -1,     3,    -1,    -1,    89,    90,    -1,   140,    -1,    -1,
     978,    -1,   980,    -1,    -1,    -1,    -1,  1059,    -1,    89,
      90,    -1,    -1,    -1,    -1,    -1,    -1,    40,    -1,   997,
     115,    -1,    -1,    40,   119,  1003,    -1,    -1,    40,    -1,
      -1,    48,    -1,  1085,    57,   115,    48,    -1,    -1,   119,
    1018,    -1,    -1,    -1,    -1,   140,    -1,     3,   128,    -1,
      -1,   131,  1030,    -1,  1032,  1033,   136,    -1,    -1,    -1,
     140,    -1,    -1,    -1,    -1,    -1,    89,    90,    -1,    -1,
      -1,  1049,    89,    90,  1052,   808,   809,    89,    90,    -1,
     813,   814,    -1,   816,    40,    -1,    -1,   993,    -1,   944,
      -1,    -1,   115,   948,   949,    -1,   119,    -1,   115,  1077,
      -1,    57,   119,   115,    -1,   128,   839,   119,   131,  1087,
      -1,   128,    -1,    -1,   131,    -1,   128,   140,     1,   131,
       3,     4,    -1,   140,    -1,     8,     9,    10,   140,    -1,
      -1,    -1,    -1,    89,    90,  1113,    -1,    -1,    21,    22,
      23,    24,    25,    26,    27,    28,    29,    30,    31,    32,
      33,    -1,    35,    36,    37,    38,    39,    -1,    -1,   115,
      43,    44,    -1,   119,    47,    -1,    49,    50,    51,    52,
      53,    -1,   128,    -1,    -1,   131,    -1,  1032,  1033,    62,
      63,    64,    65,    66,    67,    68,    69,    70,    71,    72,
      73,    74,    75,    -1,    77,    78,    79,    -1,    81,    82,
      83,    84,    85,    86,    87,    -1,    89,    90,    91,    -1,
       3,    -1,    95,     6,    97,    98,    99,   100,   101,   102,
      13,    14,    15,    16,    17,    18,    19,    20,   961,    -1,
     113,    -1,   115,    -1,    -1,   118,   119,   120,    -1,     3,
      -1,   124,     6,     7,    -1,     3,    -1,    11,    12,    13,
      14,    15,    16,    17,    18,    19,    20,   140,    22,    23,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      -1,    35,    36,    37,    38,    39,    40,    -1,    -1,    -1,
      -1,    -1,    40,    -1,    48,    49,    50,    51,    52,    53,
      48,    55,    -1,    -1,    -1,    -1,    89,    90,    -1,    57,
      -1,    -1,    -1,    -1,    -1,    -1,    70,    71,    72,    -1,
      -1,    -1,   105,   106,    -1,   107,   108,   109,   110,    -1,
     112,    -1,    86,    -1,    -1,    89,    90,    -1,    -1,     3,
      -1,    89,    90,    -1,    -1,   127,   128,   129,   130,   131,
     132,   133,   134,   135,   136,   137,   138,    -1,    -1,    -1,
      -1,   115,    -1,    -1,   118,   119,    -1,   115,    -1,    -1,
     124,   119,    -1,    -1,   128,    -1,    40,   131,    -1,    -1,
     134,   135,   136,    -1,    48,     3,   140,   141,     6,     7,
      -1,     3,   140,    11,    12,    13,    14,    15,    16,    17,
      18,    19,    20,    -1,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    -1,    35,    36,    37,
      38,    39,    40,    -1,    -1,    89,    90,    -1,    40,    76,
      -1,    49,    50,    51,    52,    53,    48,    55,    -1,    -1,
      58,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   115,    70,    71,    72,   119,    -1,    -1,    -1,    -1,
     107,   108,   109,   110,   128,   112,    -1,   131,    86,    -1,
      -1,    89,    90,    -1,    -1,     3,   140,    89,    90,    -1,
     127,   128,   129,   130,   131,   132,   133,   134,   135,   136,
     137,   138,    -1,    -1,    -1,    -1,    -1,   115,    -1,    -1,
      -1,   119,    -1,   115,    -1,    -1,    -1,   119,    -1,    -1,
     128,    -1,    40,   131,    -1,    -1,   134,   135,   136,    -1,
      48,     3,   140,   141,     6,     7,    -1,     3,   140,    11,
      12,    13,    14,    15,    16,    17,    18,    19,    20,    -1,
      22,    23,    24,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    -1,    35,    36,    37,    38,    39,    40,    -1,
      -1,    89,    90,    -1,    40,    -1,    -1,    49,    50,    51,
      52,    53,    48,    55,    -1,    -1,    58,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   115,    70,    71,
      72,   119,    -1,    -1,    -1,   107,   108,   109,   110,    -1,
      -1,    -1,    -1,    -1,    86,    -1,    -1,    89,    90,    -1,
      -1,     3,   140,    89,    90,   127,   128,   129,   130,   131,
     132,   133,   134,   135,   136,   137,   138,    -1,    -1,    -1,
      -1,    -1,    -1,   115,    -1,    -1,    -1,   119,    -1,   115,
      -1,    -1,    -1,   119,    -1,    -1,   128,    -1,    40,   131,
      -1,    -1,   134,   135,   136,    -1,    48,     3,   140,   141,
       6,     7,    -1,    41,   140,    11,    12,    13,    14,    15,
      16,    17,    18,    19,    20,    -1,    22,    23,    24,    25,
      26,    27,    28,    29,    30,    31,    32,    33,    -1,    35,
      36,    37,    38,    39,    40,    -1,    -1,    89,    90,    -1,
      -1,    -1,    -1,    49,    50,    51,    52,    53,     6,    55,
      -1,    -1,    58,    -1,    -1,    13,    14,    15,    16,    17,
      18,    19,    20,   115,    70,    71,    72,   119,    -1,   107,
     108,   109,   110,    -1,   112,    -1,   107,   108,   109,   110,
      86,    -1,    -1,    89,    90,    -1,    -1,    -1,   140,   127,
     128,   129,   130,   131,   132,   133,   134,   135,   136,   137,
     138,   132,   133,   134,   135,   136,   137,   138,    -1,   115,
      -1,    -1,    -1,   119,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   128,    -1,    -1,   131,    -1,    -1,   134,   135,
     136,    -1,    -1,     3,   140,   141,     6,     7,    41,    -1,
      -1,    11,    12,    13,    14,    15,    16,    17,    18,    19,
      20,    -1,    22,    23,    24,    25,    26,    27,    28,    29,
      30,    31,    32,    33,    -1,    35,    36,    37,    38,    39,
      40,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    49,
      50,    51,    52,    53,    -1,    55,    -1,    -1,    58,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      70,    71,    72,    -1,   107,   108,   109,   110,    -1,   112,
      -1,    -1,   107,   108,   109,   110,    86,    -1,    -1,    89,
      90,    -1,    -1,    -1,   127,   128,   129,   130,   131,   132,
     133,   134,   135,   136,   137,   138,   131,   132,   133,   134,
     135,   136,   137,   138,    -1,   115,    -1,    -1,    -1,   119,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   128,    -1,
      -1,   131,    -1,    -1,   134,   135,   136,    -1,    -1,     3,
     140,   141,     6,     7,    -1,    43,    -1,    11,    12,    13,
      14,    15,    16,    17,    18,    19,    20,    -1,    22,    23,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      -1,    35,    36,    37,    38,    39,    40,    41,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    49,    50,    51,    52,    53,
      -1,    55,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    70,    71,    72,   107,
     108,   109,   110,    -1,   112,    -1,    -1,    -1,   107,   108,
     109,   110,    86,    -1,    -1,    89,    90,    -1,    -1,   127,
     128,   129,   130,   131,   132,   133,   134,   135,   136,   137,
     138,   130,   131,   132,   133,   134,   135,   136,   137,   138,
      -1,   115,    -1,    -1,    -1,   119,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   128,    -1,    -1,   131,    -1,    -1,
     134,   135,   136,    -1,    -1,     3,   140,   141,     6,     7,
      -1,    -1,    -1,    11,    12,    13,    14,    15,    16,    17,
      18,    19,    20,    -1,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    40,    35,    36,    37,
      38,    39,    40,    41,    -1,    49,    50,    51,    -1,    -1,
      54,    49,    50,    51,    52,    53,    -1,    55,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    70,    71,    72,    -1,    80,   107,   108,   109,
     110,    -1,    -1,    -1,    88,    89,    90,    -1,    86,    -1,
      -1,    89,    90,    -1,    -1,    -1,    -1,    -1,    -1,   129,
     130,   131,   132,   133,   134,   135,   136,   137,   138,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   115,    -1,    -1,
      -1,   119,    -1,    -1,   128,    -1,    -1,   131,    -1,    -1,
     128,    -1,    -1,   131,    -1,    -1,   134,   135,   136,    -1,
      -1,     3,   140,   141,     6,     7,    -1,    -1,    -1,    11,
      12,    13,    14,    15,    16,    17,    18,    19,    20,    -1,
      22,    23,    24,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    -1,    35,    36,    37,    38,    39,    40,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    49,    50,    51,
      52,    53,    -1,    55,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   107,   108,    -1,    -1,    70,    71,
      72,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    86,    -1,    -1,    89,    90,   132,
     133,   134,   135,   136,   137,   138,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    22,    23,    24,    25,    26,
      27,    28,    29,   115,    31,    -1,    33,   119,    -1,    36,
      37,    38,    39,    -1,    -1,    -1,   128,    -1,    -1,   131,
      -1,    -1,   134,   135,   136,    -1,    -1,     3,   140,   141,
       6,     7,    -1,    -1,    -1,    11,    12,    13,    14,    15,
      16,    17,    18,    19,    20,    -1,    22,    23,    24,    25,
      26,    27,    28,    29,    30,    31,    32,    33,    -1,    35,
      36,    37,    38,    39,    40,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    49,    50,    51,    52,    53,    -1,    55,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    70,    71,    72,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,     4,
      86,    -1,    -1,    89,    90,    10,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    21,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   115,
      -1,    -1,    -1,   119,    -1,    -1,    -1,    -1,    43,    44,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   134,   135,
     136,    -1,    -1,    -1,   140,   141,    -1,    62,    63,    64,
      65,    -1,    67,    68,    69,    -1,    -1,    -1,    73,    74,
      75,    -1,    77,    78,    79,    -1,    81,    82,    83,    84,
      85,    -1,    -1,    -1,    -1,    -1,    91,    -1,    -1,    -1,
      95,    -1,    97,    98,    99,   100,   101,   102,    -1,    -1,
      -1,    -1,    -1,    -1,     3,    -1,    -1,    -1,   113,    -1,
      -1,    -1,    -1,   118,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    22,    23,    24,    25,    26,    27,    28,
      29,    30,    31,    32,    33,   140,    35,    36,    37,    38,
      39,    40,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    48,
      49,    50,    51,    52,    53,    -1,    -1,    -1,    57,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    70,    71,    72,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    86,    -1,    -1,
      89,    90,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   115,    -1,    -1,   118,
     119,     3,    -1,    -1,    -1,   124,    -1,    -1,    -1,   128,
      -1,    -1,   131,    -1,    -1,    -1,    -1,   136,    -1,    -1,
      22,    23,    24,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    -1,    35,    36,    37,    38,    39,    40,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    48,    49,    50,    51,
      52,    53,    -1,    -1,    -1,    57,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    70,    71,
      72,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    86,    -1,    -1,    89,    90,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   115,    -1,    -1,   118,   119,     3,    -1,
      58,    -1,   124,    -1,    -1,    -1,   128,    -1,    -1,   131,
      -1,    -1,    -1,    -1,   136,    -1,    -1,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    -1,
      35,    36,    37,    38,    39,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    48,    49,    50,    51,    52,    53,   107,
     108,   109,   110,    -1,   112,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    70,    71,    72,     3,   127,
     128,   129,   130,   131,   132,   133,   134,   135,   136,   137,
     138,    86,    -1,    -1,    89,    90,    -1,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    -1,
      35,    36,    37,    38,    39,    -1,    -1,    -1,    -1,    -1,
     115,    -1,    -1,   118,   119,    -1,    -1,    52,    53,   124,
      -1,    -1,    -1,   128,    -1,    -1,   131,    -1,    -1,    -1,
      -1,   136,    -1,    -1,    -1,    70,    71,    72,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    86,    -1,    -1,    89,    90,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,     0,     1,    -1,
       3,     4,    -1,    -1,    -1,     8,     9,    10,    -1,    -1,
     115,    -1,    -1,    -1,   119,    -1,    -1,    -1,    21,    22,
      23,    24,    25,    26,    27,    28,    29,    30,    31,    32,
      33,   136,    35,    36,    37,    38,    39,    -1,    -1,    -1,
      43,    44,    -1,    -1,    -1,    -1,    49,    50,    51,    52,
      53,    -1,    -1,    56,    -1,    -1,    -1,    -1,    -1,    62,
      63,    64,    65,    66,    67,    68,    69,    70,    71,    72,
      -1,    -1,    -1,    -1,    77,    78,    79,    -1,    81,    82,
      83,    84,    85,    86,    87,    -1,    89,    90,    91,    92,
      93,    94,    95,    96,    97,    98,    99,   100,   101,   102,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     113,     1,   115,     3,     4,   118,   119,    -1,     8,     9,
      10,   124,   125,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    21,    22,    23,    24,    25,    26,    27,    28,    29,
      30,    31,    32,    33,    -1,    35,    36,    37,    38,    39,
      -1,    -1,    -1,    43,    44,    -1,    -1,    47,    -1,    49,
      50,    51,    52,    53,    -1,    -1,    56,    -1,    -1,    -1,
      -1,    -1,    62,    63,    64,    65,    66,    67,    68,    69,
      70,    71,    72,    -1,    -1,    -1,    -1,    77,    78,    79,
      -1,    81,    82,    83,    84,    85,    86,    87,    -1,    89,
      90,    91,    92,    93,    94,    95,    96,    97,    98,    99,
     100,   101,   102,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   113,     1,   115,     3,     4,   118,   119,
      -1,     8,     9,    10,   124,   125,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    21,    22,    23,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    -1,    35,    36,
      37,    38,    39,    -1,    -1,    -1,    43,    44,    -1,    -1,
      47,    -1,    49,    50,    51,    52,    53,    -1,    -1,    56,
      -1,    -1,    -1,    -1,    -1,    62,    63,    64,    65,    66,
      67,    68,    69,    70,    71,    72,    -1,    -1,    -1,    -1,
      77,    78,    79,    -1,    81,    82,    83,    84,    85,    86,
      87,    -1,    89,    90,    91,    92,    93,    94,    95,    96,
      97,    98,    99,   100,   101,   102,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   113,     1,   115,     3,
       4,   118,   119,    -1,     8,     9,    10,   124,   125,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    21,    22,    23,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      -1,    35,    36,    37,    38,    39,    -1,    -1,    -1,    43,
      44,    -1,    -1,    47,    -1,    49,    50,    51,    52,    53,
      -1,    -1,    56,    -1,    -1,    -1,    -1,    -1,    62,    63,
      64,    65,    66,    67,    68,    69,    70,    71,    72,    -1,
      -1,    -1,    -1,    77,    78,    79,    -1,    81,    82,    83,
      84,    85,    86,    87,    -1,    89,    90,    91,    92,    93,
      94,    95,    96,    97,    98,    99,   100,   101,   102,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   113,
       1,   115,     3,     4,   118,   119,    -1,     8,     9,    10,
     124,   125,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      21,    22,    23,    24,    25,    26,    27,    28,    29,    30,
      31,    32,    33,    -1,    35,    36,    37,    38,    39,    -1,
      -1,    -1,    43,    44,    -1,    -1,    -1,    -1,    49,    50,
      51,    52,    53,    -1,    -1,    56,    -1,    -1,    -1,    60,
      -1,    62,    63,    64,    65,    66,    67,    68,    69,    70,
      71,    72,    -1,    -1,    -1,    -1,    77,    78,    79,    -1,
      81,    82,    83,    84,    85,    86,    87,    -1,    89,    90,
      91,    92,    93,    94,    95,    96,    97,    98,    99,   100,
     101,   102,    -1,    -1,     1,    -1,     3,    -1,    58,    -1,
      -1,    -1,   113,    -1,   115,    -1,    -1,   118,   119,    -1,
      -1,    -1,    -1,   124,   125,    22,    23,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    -1,    35,    36,
      37,    38,    39,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    48,    49,    50,    51,    52,    53,   107,   108,   109,
     110,    -1,   112,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    70,    71,    72,    -1,   127,   128,   129,
     130,   131,   132,   133,   134,   135,   136,   137,   138,    86,
      -1,    -1,    89,    90,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,     1,    -1,     3,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   115,    -1,
      -1,   118,   119,    -1,    -1,    -1,    -1,   124,    22,    23,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      -1,    35,    36,    37,    38,    39,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    48,    49,    50,    51,    52,    53,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    70,    71,    72,    -1,
      -1,     3,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    86,    -1,    -1,    89,    90,    -1,    -1,    -1,
      22,    23,    24,    25,    26,    27,    28,    29,    30,    31,
      32,    33,     3,    35,    36,    37,    38,    39,    -1,    -1,
      -1,   115,    -1,    -1,   118,   119,    -1,    -1,    -1,    -1,
     124,    22,    23,    24,    25,    26,    27,    28,    29,    30,
      31,    32,    33,    -1,    35,    36,    37,    38,    39,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    48,    49,    50,
      51,    52,    53,    -1,    86,    -1,    -1,    89,    90,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    70,
      71,    72,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   115,    -1,    86,    -1,   119,    89,    90,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,     3,    -1,    58,    -1,
      -1,    -1,    -1,    -1,   115,    -1,    13,   118,   119,    -1,
      -1,    -1,    -1,   124,   125,    22,    23,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    -1,    35,    36,
      37,    38,    39,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    48,    49,    50,    51,    52,    53,   107,   108,   109,
     110,    -1,   112,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    70,    71,    72,    -1,   127,   128,   129,
     130,   131,   132,   133,   134,   135,   136,   137,   138,    86,
      -1,    -1,    89,    90,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,     3,
      -1,    58,    -1,    -1,    -1,    -1,    -1,    -1,   115,    -1,
      -1,   118,   119,    -1,    -1,    -1,    -1,   124,    22,    23,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      -1,    35,    36,    37,    38,    39,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    48,    49,    50,    51,    52,    53,
     107,   108,   109,   110,    -1,   112,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    70,    71,    72,    -1,
     127,   128,   129,   130,   131,   132,   133,   134,   135,   136,
     137,   138,    86,    -1,    -1,    89,    90,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
       1,    -1,     3,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   115,    -1,    -1,   118,   119,    -1,    -1,    -1,    -1,
     124,    22,    23,    24,    25,    26,    27,    28,    29,    30,
      31,    32,    33,    -1,    35,    36,    37,    38,    39,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    49,    50,
      51,    52,    53,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    70,
      71,    72,    -1,     3,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    86,    -1,    -1,    89,    90,
      -1,    -1,    22,    23,    24,    25,    26,    27,    28,    29,
      30,    31,    32,    33,    -1,    35,    36,    37,    38,    39,
      -1,    -1,    -1,    -1,   115,    -1,    -1,    -1,   119,    49,
      50,    51,    52,    53,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      70,    71,    72,    -1,    -1,     3,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    86,    87,    -1,    89,
      90,    -1,    -1,    -1,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    -1,    35,    36,    37,
      38,    39,    -1,    -1,    -1,   115,    -1,    -1,    -1,   119,
     120,    49,    50,    51,    52,    53,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    70,    71,    72,    -1,    -1,    -1,     3,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    86,    87,
      -1,    89,    90,    -1,    -1,    -1,    -1,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    -1,
      35,    36,    37,    38,    39,    -1,    -1,   115,    -1,    -1,
      -1,   119,   120,    48,    49,    50,    51,    52,    53,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    70,    71,    72,    -1,     3,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    86,    -1,    -1,    89,    90,    -1,    -1,    22,    23,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      -1,    35,    36,    37,    38,    39,    40,    -1,    -1,    -1,
     115,    -1,    -1,   118,   119,    49,    50,    51,    52,    53,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    70,    71,    72,    -1,
       3,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    86,    -1,    -1,    89,    90,    -1,    -1,    22,
      23,    24,    25,    26,    27,    28,    29,    30,    31,    32,
      33,    -1,    35,    36,    37,    38,    39,    -1,    -1,    -1,
      -1,   115,    -1,    -1,    -1,   119,    49,    50,    51,    52,
      53,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    70,    71,    72,
      -1,     3,    -1,    58,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    86,    87,    -1,    89,    90,    -1,    -1,
      22,    23,    24,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    -1,    35,    36,    37,    38,    39,    -1,    -1,
      -1,    -1,   115,    -1,    -1,    -1,   119,    49,    50,    51,
      52,    53,   107,   108,   109,   110,    -1,   112,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    70,    71,
      72,     3,   127,   128,   129,   130,   131,   132,   133,   134,
     135,   136,   137,   138,    86,    -1,    -1,    89,    90,    -1,
      22,    23,    24,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    -1,    35,    36,    37,    38,    39,    -1,    -1,
      -1,    -1,    -1,   115,    -1,    -1,    -1,   119,    -1,    -1,
      52,    53,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    70,    71,
      72,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    86,    -1,    -1,    89,    90,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   115,    -1,    -1,    -1,   119
};

  /* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
static const yytype_uint16 yystos[] =
{
       0,   121,   122,   123,   144,   145,   323,     1,     3,    22,
      23,    24,    25,    26,    27,    28,    29,    30,    31,    32,
      33,    35,    36,    37,    38,    39,    48,    49,    50,    51,
      52,    53,    70,    71,    72,    86,    89,    90,   115,   118,
     119,   124,   195,   239,   240,   256,   257,   259,   260,   261,
     262,   263,   264,   293,   294,   308,   311,   313,     1,   240,
       1,    40,     0,     1,     4,     8,     9,    10,    21,    43,
      44,    56,    62,    63,    64,    65,    66,    67,    68,    69,
      77,    78,    79,    81,    82,    83,    84,    85,    91,    92,
      93,    94,    95,    96,    97,    98,    99,   100,   101,   102,
     113,   118,   124,   125,   146,   147,   148,   150,   151,   152,
     153,   154,   157,   158,   160,   161,   162,   163,   164,   165,
     166,   169,   170,   171,   174,   176,   181,   182,   183,   184,
     186,   190,   197,   198,   199,   200,   201,   205,   206,   213,
     214,   226,   227,   234,   235,   323,    48,    52,    71,    48,
      48,    40,   142,   103,   103,   307,   239,   311,   125,    43,
     260,   256,    40,    48,    54,    57,    76,   119,   128,   131,
     136,   140,   245,   246,   248,   250,   251,   252,   253,   311,
     323,   256,   263,   311,   307,   117,   142,   312,    43,    43,
     236,   237,   240,   323,   120,    40,     6,    85,   118,   317,
      40,   320,   323,     1,   258,   259,   308,    40,   320,    40,
     168,   323,    40,    40,    84,    85,    40,    84,    40,    77,
      82,    44,    77,    92,   311,    46,   308,   311,    40,     4,
      46,    40,    40,    43,    46,     4,   317,    40,   180,   258,
     178,   180,    40,    40,   317,    40,   103,   294,   320,    40,
     128,   131,   248,   250,   253,   311,    21,    85,    87,   195,
     258,   294,    48,    48,    48,   311,   118,   119,   313,   314,
     294,     3,     7,    11,    12,    13,    14,    15,    16,    17,
      18,    19,    20,    40,    55,   128,   131,   134,   135,   136,
     140,   141,   240,   241,   242,   244,   258,   259,   278,   279,
     280,   281,   282,   317,   318,   323,   256,    40,   128,   131,
     236,   251,   253,   311,    48,    46,   105,   106,   265,   266,
     267,   268,   269,    58,   278,   280,   278,     3,    40,    48,
     140,   249,   252,   311,    48,   249,   252,   253,   256,   311,
     245,    40,    57,   245,    40,    57,    48,   128,   131,   249,
     252,   311,   116,   313,   119,   314,    41,    42,   238,   323,
     267,   308,   309,   317,   294,     6,    46,   309,   321,   309,
      43,    40,   248,   250,    54,    41,   309,    52,    53,    71,
     295,   296,   323,   308,   308,   309,    13,   175,   236,   236,
     311,    43,    54,   216,    54,    46,   308,   177,   321,   308,
     236,    46,   247,   248,   250,   251,   323,    43,    42,   179,
     323,   309,   310,   323,   155,   156,   317,   236,   209,   210,
     211,   240,   293,   323,   311,   317,   128,   131,   253,   308,
     311,   321,    40,   309,    40,   128,   131,   311,   116,   248,
     311,   270,   308,   323,    40,   248,    76,   285,   286,   311,
     323,    48,    48,    41,   308,   312,   104,   111,   278,    40,
      48,   278,   278,   278,   278,   278,   278,   278,   104,    42,
     243,   323,    40,   107,   108,   109,   110,   112,   127,   128,
     129,   130,   131,   132,   133,   134,   135,   136,   137,   138,
      48,   111,     7,   128,   131,   253,   311,   250,   311,   250,
      41,    41,   128,   131,   250,   311,   116,    48,    57,   278,
      58,    40,   253,   311,    48,   311,    40,    57,    48,   253,
     236,    58,   278,   236,    58,   278,    48,    48,   249,   252,
      48,   249,   252,   116,    48,   128,   131,   249,   256,   312,
      43,   125,   240,    41,   311,   185,    42,    54,    41,   245,
      40,   265,    41,   311,    41,    54,    41,    42,   173,    42,
      41,    41,    43,   258,   145,   311,   215,    41,    41,    41,
      41,   178,    40,   180,    41,    41,    42,    46,    41,   104,
      42,   212,   323,    59,   116,    41,   253,   311,    43,   236,
     116,    80,    88,    89,    90,   128,   131,   254,   255,   256,
     298,   300,   301,   302,   323,    54,    76,   196,   323,   236,
     311,   302,   287,    46,    43,   285,   307,   294,     3,    41,
     128,   131,   136,   253,   258,    48,   244,   278,   278,   278,
     278,   278,   278,   278,   278,   278,   278,   278,   278,   278,
     278,   278,   278,   278,     3,     3,   311,   116,    41,    41,
      41,   116,   248,   251,   256,   250,   278,   236,   249,   311,
      41,   116,    48,   236,    58,   278,    48,    41,    58,    41,
      58,    48,    48,    48,    48,   128,   131,   249,   252,    48,
      48,    48,   249,   240,   238,     4,    46,   317,   145,   321,
     155,   281,   317,   322,    43,   236,    43,    46,     4,   167,
     317,     4,    43,    46,   114,   172,   248,   317,   319,   309,
     317,   322,    41,   240,   248,    46,   247,    47,    43,   145,
      44,   235,   178,    43,    46,    40,    47,   236,   179,   115,
     119,   308,   315,    43,   321,   240,   172,    91,   207,   211,
     159,   256,   248,   317,   116,    41,    40,    40,   298,    90,
      89,   300,   255,   111,    57,   191,   260,    43,    46,    41,
     188,   245,    78,   288,   289,   297,   323,   203,    46,   311,
     278,    41,    41,   136,   256,    41,   128,   131,   246,    48,
     243,    76,    41,    41,   248,   251,    58,    41,    41,   249,
     249,    41,    58,   249,   254,   254,   249,    48,    48,    48,
      48,    48,   249,    48,    48,    48,   238,    47,    42,    42,
      41,   149,    40,   302,    54,    41,    42,   173,   172,   248,
     302,    43,    47,   317,   258,   308,    43,    54,   319,   236,
      41,   142,   117,   142,   316,   103,    41,    47,   311,    44,
     118,   186,   201,   205,   206,   208,   223,   225,   235,   212,
     145,   302,    43,   236,   278,    30,    32,    35,   189,   261,
     262,   311,    40,    46,   192,   152,   271,   272,   273,   274,
     323,    40,    54,   300,   302,   303,     1,    42,    43,    46,
     187,    42,    73,    74,    75,   290,   292,     1,    43,    66,
      73,    74,    75,    78,   124,   140,   150,   151,   152,   153,
     157,   158,   162,   164,   166,   169,   171,   174,   176,   181,
     182,   183,   184,   201,   205,   206,   213,   217,   221,   222,
     223,   224,   225,   226,   228,   229,   232,   235,   323,   202,
     245,   278,   278,   278,    41,    41,    41,    40,   278,    41,
      41,    41,   249,   249,    48,    48,    48,   249,    48,    48,
     321,   321,   254,   217,   236,   172,   317,   322,    43,   248,
      41,   302,    43,   248,    43,   180,    41,   254,   119,   308,
     308,   119,   308,   241,     4,    46,    54,   103,    87,   120,
     258,    60,    43,    41,    41,   298,   299,   323,   236,    40,
      43,   193,   271,   124,   275,   276,   308,    47,    42,   125,
     236,   265,    54,    76,   304,   323,   248,   289,   311,   291,
     220,    46,    76,    76,    76,   140,   221,   313,    47,   125,
     217,    30,    32,    35,   233,   262,   311,   217,   278,   278,
     258,   249,    48,    48,   249,   249,   245,    47,    41,   173,
     302,    43,   248,   172,    43,    43,   316,   316,   104,   258,
     209,   258,    40,   298,   188,    41,   194,   276,   272,    54,
      43,   248,   125,   273,    41,    43,   267,   305,   306,   311,
      43,    46,   302,    48,   283,   284,   323,   297,   217,   218,
     313,    40,    43,   204,   248,    76,    43,    47,   246,   249,
     249,    43,    43,    43,   302,    43,   247,   104,    40,   128,
     131,   253,   236,   187,   302,    43,   277,   278,   302,   273,
      43,    46,    43,    42,    48,    40,    46,   188,    48,   311,
     217,    40,   236,   302,   278,   204,    41,    43,    43,   236,
      40,    40,    40,   131,    41,   111,   192,   188,   306,    48,
     187,    48,   284,    47,   236,    41,   188,    43,    41,   236,
     236,   236,    40,   303,   258,   193,   187,    48,    48,   219,
      41,   230,   302,   187,   231,   302,    41,    41,    41,   236,
     192,    48,   217,   231,    43,    46,    54,    43,    46,    54,
     231,   231,   231,    41,   193,    48,   267,   265,   231,    43,
      43
};

  /* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint16 yyr1[] =
{
       0,   143,   144,   144,   144,   144,   144,   144,   144,   145,
     145,   145,   145,   146,   146,   146,   146,   146,   146,   146,
     147,   147,   147,   147,   147,   147,   147,   147,   147,   147,
     147,   147,   147,   147,   147,   147,   147,   147,   147,   147,
     147,   149,   148,   150,   151,   152,   152,   152,   152,   153,
     153,   154,   154,   154,   154,   155,   156,   156,   157,   157,
     157,   159,   158,   160,   160,   161,   161,   162,   162,   162,
     162,   163,   164,   164,   165,   165,   166,   166,   167,   167,
     168,   168,   169,   169,   169,   170,   170,   171,   171,   171,
     171,   171,   171,   171,   171,   172,   172,   172,   173,   173,
     174,   175,   175,   176,   176,   176,   177,   178,   179,   179,
     180,   180,   180,   181,   182,   183,   184,   184,   184,   185,
     184,   184,   184,   184,   184,   186,   186,   187,   187,   187,
     187,   188,   189,   189,   189,   189,   189,   189,   190,   190,
     190,   191,   192,   193,   194,   193,   195,   195,   195,   196,
     196,   197,   198,   198,   199,   200,   200,   200,   200,   200,
     200,   202,   201,   203,   201,   204,   204,   205,   207,   206,
     206,   206,   208,   208,   208,   208,   208,   208,   209,   210,
     210,   211,   211,   212,   212,   213,   213,   215,   214,   216,
     214,   214,   217,   218,   219,   217,   217,   217,   220,   217,
     221,   221,   221,   221,   221,   221,   221,   221,   221,   221,
     221,   221,   221,   221,   221,   221,   221,   221,   222,   222,
     222,   223,   224,   224,   225,   225,   225,   225,   225,   226,
     227,   228,   228,   228,   229,   229,   229,   229,   229,   229,
     229,   229,   229,   229,   229,   230,   230,   230,   231,   231,
     231,   232,   233,   233,   233,   233,   233,   234,   235,   235,
     235,   235,   235,   235,   235,   235,   235,   235,   235,   235,
     235,   235,   235,   235,   235,   235,   235,   235,   236,   237,
     237,   238,   238,   238,   239,   239,   239,   240,   240,   240,
     241,   242,   242,   243,   243,   244,   244,   245,   245,   245,
     245,   245,   246,   246,   246,   246,   247,   247,   247,   247,
     248,   248,   248,   248,   248,   248,   248,   248,   248,   248,
     248,   248,   248,   248,   248,   248,   248,   248,   248,   248,
     248,   248,   249,   249,   249,   249,   249,   249,   249,   249,
     250,   250,   250,   250,   250,   250,   250,   250,   250,   250,
     250,   250,   250,   251,   251,   251,   251,   251,   251,   251,
     251,   251,   251,   251,   251,   251,   251,   251,   252,   252,
     252,   252,   252,   252,   252,   252,   253,   253,   253,   253,
     254,   254,   254,   255,   255,   256,   256,   257,   257,   257,
     258,   259,   259,   259,   259,   260,   260,   260,   260,   260,
     260,   260,   260,   261,   262,   263,   263,   264,   264,   264,
     264,   264,   264,   264,   264,   264,   264,   264,   264,   264,
     264,   266,   265,   265,   267,   267,   268,   269,   270,   270,
     271,   271,   272,   272,   273,   273,   273,   273,   273,   274,
     275,   275,   276,   276,   277,   278,   278,   279,   279,   279,
     280,   280,   280,   280,   280,   280,   280,   280,   280,   280,
     280,   280,   280,   280,   280,   280,   280,   280,   280,   281,
     281,   281,   281,   281,   281,   281,   281,   282,   282,   282,
     282,   282,   282,   282,   282,   282,   282,   282,   282,   282,
     282,   282,   282,   282,   282,   282,   282,   282,   282,   283,
     284,   284,   285,   287,   286,   286,   288,   288,   290,   289,
     291,   289,   292,   292,   292,   293,   293,   293,   293,   294,
     294,   294,   295,   295,   295,   296,   296,   297,   297,   298,
     298,   298,   298,   299,   299,   300,   300,   300,   300,   300,
     300,   301,   301,   301,   302,   302,   303,   303,   303,   303,
     303,   303,   304,   304,   305,   305,   305,   305,   306,   306,
     307,   308,   308,   308,   309,   309,   309,   310,   310,   311,
     311,   311,   311,   311,   311,   311,   312,   312,   312,   312,
     313,   313,   314,   314,   315,   315,   315,   315,   315,   315,
     316,   316,   316,   316,   317,   317,   318,   318,   319,   319,
     319,   320,   320,   321,   321,   321,   321,   321,   321,   322,
     322,   323
};

  /* YYR2[YYN] -- Number of symbols on the right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     1,     3,     2,     3,     2,     5,     3,     2,
       2,     2,     1,     1,     1,     1,     1,     1,     1,     2,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     0,     8,     5,     3,     5,     5,     9,     3,     2,
       2,     5,     2,     5,     2,     4,     1,     1,     7,     7,
       5,     0,     7,     1,     1,     2,     2,     1,     5,     5,
       5,     3,     4,     3,     7,     8,     5,     3,     1,     1,
       3,     1,     4,     7,     6,     1,     1,     7,     9,     8,
      10,     5,     7,     6,     8,     1,     1,     5,     4,     5,
       7,     1,     3,     6,     6,     8,     1,     2,     3,     1,
       2,     3,     6,     5,     9,     2,     1,     1,     1,     0,
       6,     1,     6,    10,     1,     6,     9,     1,     5,     1,
       1,     1,     1,     1,     1,     1,     1,     1,    11,    13,
       7,     1,     1,     1,     0,     3,     1,     2,     2,     2,
       1,     5,     8,    11,     6,     1,     1,     1,     1,     1,
       1,     0,     9,     0,     8,     1,     4,     4,     0,     6,
       3,     4,     1,     1,     1,     1,     1,     1,     1,     2,
       1,     1,     1,     3,     1,     3,     4,     0,     6,     0,
       5,     5,     2,     0,     0,     7,     1,     1,     0,     3,
       1,     1,     1,     1,     1,     1,     1,     1,     3,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     2,
       2,     6,     6,     7,     8,     8,     8,     9,     7,     5,
       2,     2,     2,     2,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     2,     4,     2,     2,     4,
       2,     5,     1,     1,     1,     1,     1,     2,     1,     1,
       2,     2,     1,     1,     1,     1,     1,     1,     2,     2,
       2,     2,     1,     2,     2,     2,     2,     1,     1,     2,
       1,     3,     4,     1,     2,     7,     3,     1,     2,     2,
       1,     2,     1,     3,     1,     1,     1,     2,     5,     2,
       2,     1,     2,     2,     1,     5,     1,     1,     5,     1,
       2,     3,     3,     1,     2,     2,     3,     4,     5,     4,
       5,     6,     6,     4,     5,     5,     6,     7,     8,     8,
       7,     7,     1,     2,     3,     4,     5,     3,     4,     4,
       1,     2,     4,     4,     4,     5,     6,     5,     6,     3,
       4,     4,     5,     1,     2,     2,     2,     3,     3,     1,
       2,     2,     1,     1,     2,     3,     3,     4,     3,     4,
       2,     3,     3,     4,     5,     3,     3,     2,     2,     1,
       1,     2,     1,     1,     1,     1,     2,     1,     1,     1,
       1,     2,     1,     2,     3,     1,     1,     1,     2,     1,
       1,     2,     1,     4,     1,     1,     2,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     0,     2,     1,     1,     1,     1,     1,     1,     1,
       1,     2,     1,     1,     1,     2,     3,     4,     1,     3,
       1,     2,     1,     3,     1,     1,     1,     3,     3,     3,
       1,     1,     1,     5,     8,     1,     1,     1,     1,     3,
       4,     5,     5,     5,     6,     6,     2,     2,     2,     1,
       1,     1,     1,     1,     1,     1,     1,     3,     3,     3,
       3,     3,     3,     3,     3,     3,     3,     3,     3,     3,
       3,     3,     3,     5,     2,     2,     2,     2,     2,     3,
       1,     1,     1,     0,     3,     1,     1,     3,     0,     4,
       0,     6,     1,     1,     1,     1,     1,     4,     4,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     2,     2,     1,     1,     4,     1,     1,     5,     2,
       4,     1,     1,     2,     1,     1,     3,     3,     4,     4,
       3,     4,     2,     1,     1,     3,     4,     6,     2,     2,
       3,     1,     1,     1,     1,     1,     1,     1,     1,     2,
       4,     1,     3,     1,     2,     3,     3,     2,     2,     2,
       1,     2,     1,     3,     2,     4,     1,     3,     1,     3,
       3,     2,     2,     2,     2,     1,     2,     1,     1,     1,
       1,     3,     1,     3,     5,     1,     3,     3,     5,     1,
       1,     0
};


#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)
#define YYEMPTY         (-2)
#define YYEOF           0

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                  \
do                                                              \
  if (yychar == YYEMPTY)                                        \
    {                                                           \
      yychar = (Token);                                         \
      yylval = (Value);                                         \
      YYPOPSTACK (yylen);                                       \
      yystate = *yyssp;                                         \
      goto yybackup;                                            \
    }                                                           \
  else                                                          \
    {                                                           \
      yyerror (YY_("syntax error: cannot back up")); \
      YYERROR;                                                  \
    }                                                           \
while (0)

/* Error token number */
#define YYTERROR        1
#define YYERRCODE       256



/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)

/* This macro is provided for backward compatibility. */
#ifndef YY_LOCATION_PRINT
# define YY_LOCATION_PRINT(File, Loc) ((void) 0)
#endif


# define YY_SYMBOL_PRINT(Title, Type, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Type, Value); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*----------------------------------------.
| Print this symbol's value on YYOUTPUT.  |
`----------------------------------------*/

static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
{
  FILE *yyo = yyoutput;
  YYUSE (yyo);
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# endif
  YYUSE (yytype);
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
{
  YYFPRINTF (yyoutput, "%s %s (",
             yytype < YYNTOKENS ? "token" : "nterm", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yytype_int16 *yybottom, yytype_int16 *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yytype_int16 *yyssp, YYSTYPE *yyvsp, int yyrule)
{
  unsigned long int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       yystos[yyssp[yyi + 1 - yynrhs]],
                       &(yyvsp[(yyi + 1) - (yynrhs)])
                                              );
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, Rule); \
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif


#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined __GLIBC__ && defined _STRING_H
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
static YYSIZE_T
yystrlen (const char *yystr)
{
  YYSIZE_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *
yystpcpy (char *yydest, const char *yysrc)
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

# ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYSIZE_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYSIZE_T yyn = 0;
      char const *yyp = yystr;

      for (;;)
        switch (*++yyp)
          {
          case '\'':
          case ',':
            goto do_not_strip_quotes;

          case '\\':
            if (*++yyp != '\\')
              goto do_not_strip_quotes;
            /* Fall through.  */
          default:
            if (yyres)
              yyres[yyn] = *yyp;
            yyn++;
            break;

          case '"':
            if (yyres)
              yyres[yyn] = '\0';
            return yyn;
          }
    do_not_strip_quotes: ;
    }

  if (! yyres)
    return yystrlen (yystr);

  return yystpcpy (yyres, yystr) - yyres;
}
# endif

/* Copy into *YYMSG, which is of size *YYMSG_ALLOC, an error message
   about the unexpected token YYTOKEN for the state stack whose top is
   YYSSP.

   Return 0 if *YYMSG was successfully written.  Return 1 if *YYMSG is
   not large enough to hold the message.  In that case, also set
   *YYMSG_ALLOC to the required number of bytes.  Return 2 if the
   required number of bytes is too large to store.  */
static int
yysyntax_error (YYSIZE_T *yymsg_alloc, char **yymsg,
                yytype_int16 *yyssp, int yytoken)
{
  YYSIZE_T yysize0 = yytnamerr (YY_NULLPTR, yytname[yytoken]);
  YYSIZE_T yysize = yysize0;
  enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
  /* Internationalized format string. */
  const char *yyformat = YY_NULLPTR;
  /* Arguments of yyformat. */
  char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
  /* Number of reported tokens (one for the "unexpected", one per
     "expected"). */
  int yycount = 0;

  /* There are many possibilities here to consider:
     - If this state is a consistent state with a default action, then
       the only way this function was invoked is if the default action
       is an error action.  In that case, don't check for expected
       tokens because there are none.
     - The only way there can be no lookahead present (in yychar) is if
       this state is a consistent state with a default action.  Thus,
       detecting the absence of a lookahead is sufficient to determine
       that there is no unexpected or expected token to report.  In that
       case, just report a simple "syntax error".
     - Don't assume there isn't a lookahead just because this state is a
       consistent state with a default action.  There might have been a
       previous inconsistent state, consistent state with a non-default
       action, or user semantic action that manipulated yychar.
     - Of course, the expected token list depends on states to have
       correct lookahead information, and it depends on the parser not
       to perform extra reductions after fetching a lookahead from the
       scanner and before detecting a syntax error.  Thus, state merging
       (from LALR or IELR) and default reductions corrupt the expected
       token list.  However, the list is correct for canonical LR with
       one exception: it will still contain any token that will not be
       accepted due to an error action in a later state.
  */
  if (yytoken != YYEMPTY)
    {
      int yyn = yypact[*yyssp];
      yyarg[yycount++] = yytname[yytoken];
      if (!yypact_value_is_default (yyn))
        {
          /* Start YYX at -YYN if negative to avoid negative indexes in
             YYCHECK.  In other words, skip the first -YYN actions for
             this state because they are default actions.  */
          int yyxbegin = yyn < 0 ? -yyn : 0;
          /* Stay within bounds of both yycheck and yytname.  */
          int yychecklim = YYLAST - yyn + 1;
          int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
          int yyx;

          for (yyx = yyxbegin; yyx < yyxend; ++yyx)
            if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR
                && !yytable_value_is_error (yytable[yyx + yyn]))
              {
                if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
                  {
                    yycount = 1;
                    yysize = yysize0;
                    break;
                  }
                yyarg[yycount++] = yytname[yyx];
                {
                  YYSIZE_T yysize1 = yysize + yytnamerr (YY_NULLPTR, yytname[yyx]);
                  if (! (yysize <= yysize1
                         && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
                    return 2;
                  yysize = yysize1;
                }
              }
        }
    }

  switch (yycount)
    {
# define YYCASE_(N, S)                      \
      case N:                               \
        yyformat = S;                       \
      break
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
# undef YYCASE_
    }

  {
    YYSIZE_T yysize1 = yysize + yystrlen (yyformat);
    if (! (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
      return 2;
    yysize = yysize1;
  }

  if (*yymsg_alloc < yysize)
    {
      *yymsg_alloc = 2 * yysize;
      if (! (yysize <= *yymsg_alloc
             && *yymsg_alloc <= YYSTACK_ALLOC_MAXIMUM))
        *yymsg_alloc = YYSTACK_ALLOC_MAXIMUM;
      return 1;
    }

  /* Avoid sprintf, as that infringes on the user's name space.
     Don't have undefined behavior even if the translation
     produced a string with the wrong number of "%s"s.  */
  {
    char *yyp = *yymsg;
    int yyi = 0;
    while ((*yyp = *yyformat) != '\0')
      if (*yyp == '%' && yyformat[1] == 's' && yyi < yycount)
        {
          yyp += yytnamerr (yyp, yyarg[yyi++]);
          yyformat += 2;
        }
      else
        {
          yyp++;
          yyformat++;
        }
  }
  return 0;
}
#endif /* YYERROR_VERBOSE */

/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep)
{
  YYUSE (yyvaluep);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YYUSE (yytype);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}




/* The lookahead symbol.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;
/* Number of syntax errors so far.  */
int yynerrs;


/*----------.
| yyparse.  |
`----------*/

int
yyparse (void)
{
    int yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       'yyss': related to states.
       'yyvs': related to semantic values.

       Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* The state stack.  */
    yytype_int16 yyssa[YYINITDEPTH];
    yytype_int16 *yyss;
    yytype_int16 *yyssp;

    /* The semantic value stack.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs;
    YYSTYPE *yyvsp;

    YYSIZE_T yystacksize;

  int yyn;
  int yyresult;
  /* Lookahead token as an internal (translated) token number.  */
  int yytoken = 0;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;

#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  yyssp = yyss = yyssa;
  yyvsp = yyvs = yyvsa;
  yystacksize = YYINITDEPTH;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY; /* Cause a token to be read.  */
  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        YYSTYPE *yyvs1 = yyvs;
        yytype_int16 *yyss1 = yyss;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * sizeof (*yyssp),
                    &yyvs1, yysize * sizeof (*yyvsp),
                    &yystacksize);

        yyss = yyss1;
        yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyexhaustedlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yytype_int16 *yyss1 = yyss;
        union yyalloc *yyptr =
          (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
        if (! yyptr)
          goto yyexhaustedlab;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
                  (unsigned long int) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid lookahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = yylex ();
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the shifted token.  */
  yychar = YYEMPTY;

  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- Do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     '$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 2:
#line 1713 "parser.y" /* yacc.c:1646  */
    {
                   if (!classes) classes = NewHash();
		   Setattr((yyvsp[0].node),"classes",classes); 
		   Setattr((yyvsp[0].node),"name",ModuleName);
		   
		   if ((!module_node) && ModuleName) {
		     module_node = new_node("module");
		     Setattr(module_node,"name",ModuleName);
		   }
		   Setattr((yyvsp[0].node),"module",module_node);
	           top = (yyvsp[0].node);
               }
#line 4845 "y.tab.c" /* yacc.c:1646  */
    break;

  case 3:
#line 1725 "parser.y" /* yacc.c:1646  */
    {
                 top = Copy(Getattr((yyvsp[-1].p),"type"));
		 Delete((yyvsp[-1].p));
               }
#line 4854 "y.tab.c" /* yacc.c:1646  */
    break;

  case 4:
#line 1729 "parser.y" /* yacc.c:1646  */
    {
                 top = 0;
               }
#line 4862 "y.tab.c" /* yacc.c:1646  */
    break;

  case 5:
#line 1732 "parser.y" /* yacc.c:1646  */
    {
                 top = (yyvsp[-1].p);
               }
#line 4870 "y.tab.c" /* yacc.c:1646  */
    break;

  case 6:
#line 1735 "parser.y" /* yacc.c:1646  */
    {
                 top = 0;
               }
#line 4878 "y.tab.c" /* yacc.c:1646  */
    break;

  case 7:
#line 1738 "parser.y" /* yacc.c:1646  */
    {
                 top = (yyvsp[-2].pl);
               }
#line 4886 "y.tab.c" /* yacc.c:1646  */
    break;

  case 8:
#line 1741 "parser.y" /* yacc.c:1646  */
    {
                 top = 0;
               }
#line 4894 "y.tab.c" /* yacc.c:1646  */
    break;

  case 9:
#line 1746 "parser.y" /* yacc.c:1646  */
    {  
                   /* add declaration to end of linked list (the declaration isn't always a single declaration, sometimes it is a linked list itself) */
                   if (currentDeclComment != NULL) {
		     set_comment((yyvsp[0].node), currentDeclComment);
		     currentDeclComment = NULL;
                   }                                      
                   appendChild((yyvsp[-1].node),(yyvsp[0].node));
                   (yyval.node) = (yyvsp[-1].node);
               }
#line 4908 "y.tab.c" /* yacc.c:1646  */
    break;

  case 10:
#line 1755 "parser.y" /* yacc.c:1646  */
    {
                   currentDeclComment = (yyvsp[0].str); 
                   (yyval.node) = (yyvsp[-1].node);
               }
#line 4917 "y.tab.c" /* yacc.c:1646  */
    break;

  case 11:
#line 1759 "parser.y" /* yacc.c:1646  */
    {
                   Node *node = lastChild((yyvsp[-1].node));
                   if (node) {
                     set_comment(node, (yyvsp[0].str));
                   }
                   (yyval.node) = (yyvsp[-1].node);
               }
#line 4929 "y.tab.c" /* yacc.c:1646  */
    break;

  case 12:
#line 1766 "parser.y" /* yacc.c:1646  */
    {
                   (yyval.node) = new_node("top");
               }
#line 4937 "y.tab.c" /* yacc.c:1646  */
    break;

  case 13:
#line 1771 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = (yyvsp[0].node); }
#line 4943 "y.tab.c" /* yacc.c:1646  */
    break;

  case 14:
#line 1772 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = (yyvsp[0].node); }
#line 4949 "y.tab.c" /* yacc.c:1646  */
    break;

  case 15:
#line 1773 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = (yyvsp[0].node); }
#line 4955 "y.tab.c" /* yacc.c:1646  */
    break;

  case 16:
#line 1774 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = 0; }
#line 4961 "y.tab.c" /* yacc.c:1646  */
    break;

  case 17:
#line 1775 "parser.y" /* yacc.c:1646  */
    {
                  (yyval.node) = 0;
		  if (cparse_unknown_directive) {
		      Swig_error(cparse_file, cparse_line, "Unknown directive '%s'.\n", cparse_unknown_directive);
		  } else {
		      Swig_error(cparse_file, cparse_line, "Syntax error in input(1).\n");
		  }
		  SWIG_exit(EXIT_FAILURE);
               }
#line 4975 "y.tab.c" /* yacc.c:1646  */
    break;

  case 18:
#line 1785 "parser.y" /* yacc.c:1646  */
    { 
                  if ((yyval.node)) {
   		      add_symbols((yyval.node));
                  }
                  (yyval.node) = (yyvsp[0].node); 
	       }
#line 4986 "y.tab.c" /* yacc.c:1646  */
    break;

  case 19:
#line 1801 "parser.y" /* yacc.c:1646  */
    {
                  (yyval.node) = 0;
                  skip_decl();
               }
#line 4995 "y.tab.c" /* yacc.c:1646  */
    break;

  case 20:
#line 1811 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = (yyvsp[0].node); }
#line 5001 "y.tab.c" /* yacc.c:1646  */
    break;

  case 21:
#line 1812 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = (yyvsp[0].node); }
#line 5007 "y.tab.c" /* yacc.c:1646  */
    break;

  case 22:
#line 1813 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = (yyvsp[0].node); }
#line 5013 "y.tab.c" /* yacc.c:1646  */
    break;

  case 23:
#line 1814 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = (yyvsp[0].node); }
#line 5019 "y.tab.c" /* yacc.c:1646  */
    break;

  case 24:
#line 1815 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = (yyvsp[0].node); }
#line 5025 "y.tab.c" /* yacc.c:1646  */
    break;

  case 25:
#line 1816 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = (yyvsp[0].node); }
#line 5031 "y.tab.c" /* yacc.c:1646  */
    break;

  case 26:
#line 1817 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = (yyvsp[0].node); }
#line 5037 "y.tab.c" /* yacc.c:1646  */
    break;

  case 27:
#line 1818 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = (yyvsp[0].node); }
#line 5043 "y.tab.c" /* yacc.c:1646  */
    break;

  case 28:
#line 1819 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = (yyvsp[0].node); }
#line 5049 "y.tab.c" /* yacc.c:1646  */
    break;

  case 29:
#line 1820 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = (yyvsp[0].node); }
#line 5055 "y.tab.c" /* yacc.c:1646  */
    break;

  case 30:
#line 1821 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = (yyvsp[0].node); }
#line 5061 "y.tab.c" /* yacc.c:1646  */
    break;

  case 31:
#line 1822 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = (yyvsp[0].node); }
#line 5067 "y.tab.c" /* yacc.c:1646  */
    break;

  case 32:
#line 1823 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = (yyvsp[0].node); }
#line 5073 "y.tab.c" /* yacc.c:1646  */
    break;

  case 33:
#line 1824 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = (yyvsp[0].node); }
#line 5079 "y.tab.c" /* yacc.c:1646  */
    break;

  case 34:
#line 1825 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = (yyvsp[0].node); }
#line 5085 "y.tab.c" /* yacc.c:1646  */
    break;

  case 35:
#line 1826 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = (yyvsp[0].node); }
#line 5091 "y.tab.c" /* yacc.c:1646  */
    break;

  case 36:
#line 1827 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = (yyvsp[0].node); }
#line 5097 "y.tab.c" /* yacc.c:1646  */
    break;

  case 37:
#line 1828 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = (yyvsp[0].node); }
#line 5103 "y.tab.c" /* yacc.c:1646  */
    break;

  case 38:
#line 1829 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = (yyvsp[0].node); }
#line 5109 "y.tab.c" /* yacc.c:1646  */
    break;

  case 39:
#line 1830 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = (yyvsp[0].node); }
#line 5115 "y.tab.c" /* yacc.c:1646  */
    break;

  case 40:
#line 1831 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = (yyvsp[0].node); }
#line 5121 "y.tab.c" /* yacc.c:1646  */
    break;

  case 41:
#line 1838 "parser.y" /* yacc.c:1646  */
    {
               Node *cls;
	       String *clsname;
	       extendmode = 1;
	       cplus_mode = CPLUS_PUBLIC;
	       if (!classes) classes = NewHash();
	       if (!classes_typedefs) classes_typedefs = NewHash();
	       clsname = make_class_name((yyvsp[-1].str));
	       cls = Getattr(classes,clsname);
	       if (!cls) {
	         cls = Getattr(classes_typedefs, clsname);
		 if (!cls) {
		   /* No previous definition. Create a new scope */
		   Node *am = Getattr(Swig_extend_hash(),clsname);
		   if (!am) {
		     Swig_symbol_newscope();
		     Swig_symbol_setscopename((yyvsp[-1].str));
		     prev_symtab = 0;
		   } else {
		     prev_symtab = Swig_symbol_setscope(Getattr(am,"symtab"));
		   }
		   current_class = 0;
		 } else {
		   /* Previous typedef class definition.  Use its symbol table.
		      Deprecated, just the real name should be used. 
		      Note that %extend before the class typedef never worked, only %extend after the class typdef. */
		   prev_symtab = Swig_symbol_setscope(Getattr(cls, "symtab"));
		   current_class = cls;
		   SWIG_WARN_NODE_BEGIN(cls);
		   Swig_warning(WARN_PARSE_EXTEND_NAME, cparse_file, cparse_line, "Deprecated %%extend name used - the %s name '%s' should be used instead of the typedef name '%s'.\n", Getattr(cls, "kind"), SwigType_namestr(Getattr(cls, "name")), (yyvsp[-1].str));
		   SWIG_WARN_NODE_END(cls);
		 }
	       } else {
		 /* Previous class definition.  Use its symbol table */
		 prev_symtab = Swig_symbol_setscope(Getattr(cls,"symtab"));
		 current_class = cls;
	       }
	       Classprefix = NewString((yyvsp[-1].str));
	       Namespaceprefix= Swig_symbol_qualifiedscopename(0);
	       Delete(clsname);
	     }
#line 5167 "y.tab.c" /* yacc.c:1646  */
    break;

  case 42:
#line 1878 "parser.y" /* yacc.c:1646  */
    {
               String *clsname;
	       extendmode = 0;
               (yyval.node) = new_node("extend");
	       Setattr((yyval.node),"symtab",Swig_symbol_popscope());
	       if (prev_symtab) {
		 Swig_symbol_setscope(prev_symtab);
	       }
	       Namespaceprefix = Swig_symbol_qualifiedscopename(0);
               clsname = make_class_name((yyvsp[-4].str));
	       Setattr((yyval.node),"name",clsname);

	       mark_nodes_as_extend((yyvsp[-1].node));
	       if (current_class) {
		 /* We add the extension to the previously defined class */
		 appendChild((yyval.node), (yyvsp[-1].node));
		 appendChild(current_class,(yyval.node));
	       } else {
		 /* We store the extensions in the extensions hash */
		 Node *am = Getattr(Swig_extend_hash(),clsname);
		 if (am) {
		   /* Append the members to the previous extend methods */
		   appendChild(am, (yyvsp[-1].node));
		 } else {
		   appendChild((yyval.node), (yyvsp[-1].node));
		   Setattr(Swig_extend_hash(),clsname,(yyval.node));
		 }
	       }
	       current_class = 0;
	       Delete(Classprefix);
	       Delete(clsname);
	       Classprefix = 0;
	       prev_symtab = 0;
	       (yyval.node) = 0;

	     }
#line 5208 "y.tab.c" /* yacc.c:1646  */
    break;

  case 43:
#line 1920 "parser.y" /* yacc.c:1646  */
    {
                    (yyval.node) = new_node("apply");
                    Setattr((yyval.node),"pattern",Getattr((yyvsp[-3].p),"pattern"));
		    appendChild((yyval.node),(yyvsp[-1].p));
               }
#line 5218 "y.tab.c" /* yacc.c:1646  */
    break;

  case 44:
#line 1930 "parser.y" /* yacc.c:1646  */
    {
		 (yyval.node) = new_node("clear");
		 appendChild((yyval.node),(yyvsp[-1].p));
               }
#line 5227 "y.tab.c" /* yacc.c:1646  */
    break;

  case 45:
#line 1941 "parser.y" /* yacc.c:1646  */
    {
		   if (((yyvsp[-1].dtype).type != T_ERROR) && ((yyvsp[-1].dtype).type != T_SYMBOL)) {
		     SwigType *type = NewSwigType((yyvsp[-1].dtype).type);
		     (yyval.node) = new_node("constant");
		     Setattr((yyval.node),"name",(yyvsp[-3].id));
		     Setattr((yyval.node),"type",type);
		     Setattr((yyval.node),"value",(yyvsp[-1].dtype).val);
		     if ((yyvsp[-1].dtype).rawval) Setattr((yyval.node),"rawval", (yyvsp[-1].dtype).rawval);
		     Setattr((yyval.node),"storage","%constant");
		     SetFlag((yyval.node),"feature:immutable");
		     add_symbols((yyval.node));
		     Delete(type);
		   } else {
		     if ((yyvsp[-1].dtype).type == T_ERROR) {
		       Swig_warning(WARN_PARSE_UNSUPPORTED_VALUE,cparse_file,cparse_line,"Unsupported constant value (ignored)\n");
		     }
		     (yyval.node) = 0;
		   }

	       }
#line 5252 "y.tab.c" /* yacc.c:1646  */
    break;

  case 46:
#line 1961 "parser.y" /* yacc.c:1646  */
    {
		 if (((yyvsp[-1].dtype).type != T_ERROR) && ((yyvsp[-1].dtype).type != T_SYMBOL)) {
		   SwigType_push((yyvsp[-3].type),(yyvsp[-2].decl).type);
		   /* Sneaky callback function trick */
		   if (SwigType_isfunction((yyvsp[-3].type))) {
		     SwigType_add_pointer((yyvsp[-3].type));
		   }
		   (yyval.node) = new_node("constant");
		   Setattr((yyval.node),"name",(yyvsp[-2].decl).id);
		   Setattr((yyval.node),"type",(yyvsp[-3].type));
		   Setattr((yyval.node),"value",(yyvsp[-1].dtype).val);
		   if ((yyvsp[-1].dtype).rawval) Setattr((yyval.node),"rawval", (yyvsp[-1].dtype).rawval);
		   Setattr((yyval.node),"storage","%constant");
		   SetFlag((yyval.node),"feature:immutable");
		   add_symbols((yyval.node));
		 } else {
		   if ((yyvsp[-1].dtype).type == T_ERROR) {
		     Swig_warning(WARN_PARSE_UNSUPPORTED_VALUE,cparse_file,cparse_line, "Unsupported constant value\n");
		   }
		   (yyval.node) = 0;
		 }
               }
#line 5279 "y.tab.c" /* yacc.c:1646  */
    break;

  case 47:
#line 1985 "parser.y" /* yacc.c:1646  */
    {
		 if (((yyvsp[-1].dtype).type != T_ERROR) && ((yyvsp[-1].dtype).type != T_SYMBOL)) {
		   SwigType_add_function((yyvsp[-7].type), (yyvsp[-4].pl));
		   SwigType_push((yyvsp[-7].type), (yyvsp[-2].dtype).qualifier);
		   SwigType_push((yyvsp[-7].type), (yyvsp[-6].decl).type);
		   /* Sneaky callback function trick */
		   if (SwigType_isfunction((yyvsp[-7].type))) {
		     SwigType_add_pointer((yyvsp[-7].type));
		   }
		   (yyval.node) = new_node("constant");
		   Setattr((yyval.node), "name", (yyvsp[-6].decl).id);
		   Setattr((yyval.node), "type", (yyvsp[-7].type));
		   Setattr((yyval.node), "value", (yyvsp[-1].dtype).val);
		   if ((yyvsp[-1].dtype).rawval) Setattr((yyval.node), "rawval", (yyvsp[-1].dtype).rawval);
		   Setattr((yyval.node), "storage", "%constant");
		   SetFlag((yyval.node), "feature:immutable");
		   add_symbols((yyval.node));
		 } else {
		   if ((yyvsp[-1].dtype).type == T_ERROR) {
		     Swig_warning(WARN_PARSE_UNSUPPORTED_VALUE,cparse_file,cparse_line, "Unsupported constant value\n");
		   }
		   (yyval.node) = 0;
		 }
	       }
#line 5308 "y.tab.c" /* yacc.c:1646  */
    break;

  case 48:
#line 2009 "parser.y" /* yacc.c:1646  */
    {
		 Swig_warning(WARN_PARSE_BAD_VALUE,cparse_file,cparse_line,"Bad constant value (ignored).\n");
		 (yyval.node) = 0;
	       }
#line 5317 "y.tab.c" /* yacc.c:1646  */
    break;

  case 49:
#line 2020 "parser.y" /* yacc.c:1646  */
    {
		 char temp[64];
		 Replace((yyvsp[0].str),"$file",cparse_file, DOH_REPLACE_ANY);
		 sprintf(temp,"%d", cparse_line);
		 Replace((yyvsp[0].str),"$line",temp,DOH_REPLACE_ANY);
		 Printf(stderr,"%s\n", (yyvsp[0].str));
		 Delete((yyvsp[0].str));
                 (yyval.node) = 0;
	       }
#line 5331 "y.tab.c" /* yacc.c:1646  */
    break;

  case 50:
#line 2029 "parser.y" /* yacc.c:1646  */
    {
		 char temp[64];
		 String *s = (yyvsp[0].str);
		 Replace(s,"$file",cparse_file, DOH_REPLACE_ANY);
		 sprintf(temp,"%d", cparse_line);
		 Replace(s,"$line",temp,DOH_REPLACE_ANY);
		 Printf(stderr,"%s\n", s);
		 Delete(s);
                 (yyval.node) = 0;
               }
#line 5346 "y.tab.c" /* yacc.c:1646  */
    break;

  case 51:
#line 2048 "parser.y" /* yacc.c:1646  */
    {
                    skip_balanced('{','}');
		    (yyval.node) = 0;
		    Swig_warning(WARN_DEPRECATED_EXCEPT,cparse_file, cparse_line, "%%except is deprecated.  Use %%exception instead.\n");
	       }
#line 5356 "y.tab.c" /* yacc.c:1646  */
    break;

  case 52:
#line 2054 "parser.y" /* yacc.c:1646  */
    {
                    skip_balanced('{','}');
		    (yyval.node) = 0;
		    Swig_warning(WARN_DEPRECATED_EXCEPT,cparse_file, cparse_line, "%%except is deprecated.  Use %%exception instead.\n");
               }
#line 5366 "y.tab.c" /* yacc.c:1646  */
    break;

  case 53:
#line 2060 "parser.y" /* yacc.c:1646  */
    {
		 (yyval.node) = 0;
		 Swig_warning(WARN_DEPRECATED_EXCEPT,cparse_file, cparse_line, "%%except is deprecated.  Use %%exception instead.\n");
               }
#line 5375 "y.tab.c" /* yacc.c:1646  */
    break;

  case 54:
#line 2065 "parser.y" /* yacc.c:1646  */
    {
		 (yyval.node) = 0;
		 Swig_warning(WARN_DEPRECATED_EXCEPT,cparse_file, cparse_line, "%%except is deprecated.  Use %%exception instead.\n");
	       }
#line 5384 "y.tab.c" /* yacc.c:1646  */
    break;

  case 55:
#line 2072 "parser.y" /* yacc.c:1646  */
    {		 
                 (yyval.node) = NewHash();
                 Setattr((yyval.node),"value",(yyvsp[-3].str));
		 Setattr((yyval.node),"type",Getattr((yyvsp[-1].p),"type"));
               }
#line 5394 "y.tab.c" /* yacc.c:1646  */
    break;

  case 56:
#line 2079 "parser.y" /* yacc.c:1646  */
    {
                 (yyval.node) = NewHash();
                 Setattr((yyval.node),"value",(yyvsp[0].str));
              }
#line 5403 "y.tab.c" /* yacc.c:1646  */
    break;

  case 57:
#line 2083 "parser.y" /* yacc.c:1646  */
    {
                (yyval.node) = (yyvsp[0].node);
              }
#line 5411 "y.tab.c" /* yacc.c:1646  */
    break;

  case 58:
#line 2096 "parser.y" /* yacc.c:1646  */
    {
                   Hash *p = (yyvsp[-2].node);
		   (yyval.node) = new_node("fragment");
		   Setattr((yyval.node),"value",Getattr((yyvsp[-4].node),"value"));
		   Setattr((yyval.node),"type",Getattr((yyvsp[-4].node),"type"));
		   Setattr((yyval.node),"section",Getattr(p,"name"));
		   Setattr((yyval.node),"kwargs",nextSibling(p));
		   Setattr((yyval.node),"code",(yyvsp[0].str));
                 }
#line 5425 "y.tab.c" /* yacc.c:1646  */
    break;

  case 59:
#line 2105 "parser.y" /* yacc.c:1646  */
    {
		   Hash *p = (yyvsp[-2].node);
		   String *code;
                   skip_balanced('{','}');
		   (yyval.node) = new_node("fragment");
		   Setattr((yyval.node),"value",Getattr((yyvsp[-4].node),"value"));
		   Setattr((yyval.node),"type",Getattr((yyvsp[-4].node),"type"));
		   Setattr((yyval.node),"section",Getattr(p,"name"));
		   Setattr((yyval.node),"kwargs",nextSibling(p));
		   Delitem(scanner_ccode,0);
		   Delitem(scanner_ccode,DOH_END);
		   code = Copy(scanner_ccode);
		   Setattr((yyval.node),"code",code);
		   Delete(code);
                 }
#line 5445 "y.tab.c" /* yacc.c:1646  */
    break;

  case 60:
#line 2120 "parser.y" /* yacc.c:1646  */
    {
		   (yyval.node) = new_node("fragment");
		   Setattr((yyval.node),"value",Getattr((yyvsp[-2].node),"value"));
		   Setattr((yyval.node),"type",Getattr((yyvsp[-2].node),"type"));
		   Setattr((yyval.node),"emitonly","1");
		 }
#line 5456 "y.tab.c" /* yacc.c:1646  */
    break;

  case 61:
#line 2133 "parser.y" /* yacc.c:1646  */
    {
                     (yyvsp[-3].loc).filename = Copy(cparse_file);
		     (yyvsp[-3].loc).line = cparse_line;
		     scanner_set_location((yyvsp[-1].str),1);
                     if ((yyvsp[-2].node)) { 
		       String *maininput = Getattr((yyvsp[-2].node), "maininput");
		       if (maininput)
		         scanner_set_main_input_file(NewString(maininput));
		     }
               }
#line 5471 "y.tab.c" /* yacc.c:1646  */
    break;

  case 62:
#line 2142 "parser.y" /* yacc.c:1646  */
    {
                     String *mname = 0;
                     (yyval.node) = (yyvsp[-1].node);
		     scanner_set_location((yyvsp[-6].loc).filename,(yyvsp[-6].loc).line+1);
		     if (strcmp((yyvsp[-6].loc).type,"include") == 0) set_nodeType((yyval.node),"include");
		     if (strcmp((yyvsp[-6].loc).type,"import") == 0) {
		       mname = (yyvsp[-5].node) ? Getattr((yyvsp[-5].node),"module") : 0;
		       set_nodeType((yyval.node),"import");
		       if (import_mode) --import_mode;
		     }
		     
		     Setattr((yyval.node),"name",(yyvsp[-4].str));
		     /* Search for the module (if any) */
		     {
			 Node *n = firstChild((yyval.node));
			 while (n) {
			     if (Strcmp(nodeType(n),"module") == 0) {
			         if (mname) {
				   Setattr(n,"name", mname);
				   mname = 0;
				 }
				 Setattr((yyval.node),"module",Getattr(n,"name"));
				 break;
			     }
			     n = nextSibling(n);
			 }
			 if (mname) {
			   /* There is no module node in the import
			      node, ie, you imported a .h file
			      directly.  We are forced then to create
			      a new import node with a module node.
			   */			      
			   Node *nint = new_node("import");
			   Node *mnode = new_node("module");
			   Setattr(mnode,"name", mname);
                           Setattr(mnode,"options",(yyvsp[-5].node));
			   appendChild(nint,mnode);
			   Delete(mnode);
			   appendChild(nint,firstChild((yyval.node)));
			   (yyval.node) = nint;
			   Setattr((yyval.node),"module",mname);
			 }
		     }
		     Setattr((yyval.node),"options",(yyvsp[-5].node));
               }
#line 5521 "y.tab.c" /* yacc.c:1646  */
    break;

  case 63:
#line 2189 "parser.y" /* yacc.c:1646  */
    { (yyval.loc).type = "include"; }
#line 5527 "y.tab.c" /* yacc.c:1646  */
    break;

  case 64:
#line 2190 "parser.y" /* yacc.c:1646  */
    { (yyval.loc).type = "import"; ++import_mode;}
#line 5533 "y.tab.c" /* yacc.c:1646  */
    break;

  case 65:
#line 2197 "parser.y" /* yacc.c:1646  */
    {
                 String *cpps;
		 if (Namespaceprefix) {
		   Swig_error(cparse_file, cparse_start_line, "%%inline directive inside a namespace is disallowed.\n");
		   (yyval.node) = 0;
		 } else {
		   (yyval.node) = new_node("insert");
		   Setattr((yyval.node),"code",(yyvsp[0].str));
		   /* Need to run through the preprocessor */
		   Seek((yyvsp[0].str),0,SEEK_SET);
		   Setline((yyvsp[0].str),cparse_start_line);
		   Setfile((yyvsp[0].str),cparse_file);
		   cpps = Preprocessor_parse((yyvsp[0].str));
		   start_inline(Char(cpps), cparse_start_line);
		   Delete((yyvsp[0].str));
		   Delete(cpps);
		 }
		 
	       }
#line 5557 "y.tab.c" /* yacc.c:1646  */
    break;

  case 66:
#line 2216 "parser.y" /* yacc.c:1646  */
    {
                 String *cpps;
		 int start_line = cparse_line;
		 skip_balanced('{','}');
		 if (Namespaceprefix) {
		   Swig_error(cparse_file, cparse_start_line, "%%inline directive inside a namespace is disallowed.\n");
		   
		   (yyval.node) = 0;
		 } else {
		   String *code;
                   (yyval.node) = new_node("insert");
		   Delitem(scanner_ccode,0);
		   Delitem(scanner_ccode,DOH_END);
		   code = Copy(scanner_ccode);
		   Setattr((yyval.node),"code", code);
		   Delete(code);		   
		   cpps=Copy(scanner_ccode);
		   start_inline(Char(cpps), start_line);
		   Delete(cpps);
		 }
               }
#line 5583 "y.tab.c" /* yacc.c:1646  */
    break;

  case 67:
#line 2247 "parser.y" /* yacc.c:1646  */
    {
                 (yyval.node) = new_node("insert");
		 Setattr((yyval.node),"code",(yyvsp[0].str));
	       }
#line 5592 "y.tab.c" /* yacc.c:1646  */
    break;

  case 68:
#line 2251 "parser.y" /* yacc.c:1646  */
    {
		 String *code = NewStringEmpty();
		 (yyval.node) = new_node("insert");
		 Setattr((yyval.node),"section",(yyvsp[-2].id));
		 Setattr((yyval.node),"code",code);
		 if (Swig_insert_file((yyvsp[0].str),code) < 0) {
		   Swig_error(cparse_file, cparse_line, "Couldn't find '%s'.\n", (yyvsp[0].str));
		   (yyval.node) = 0;
		 } 
               }
#line 5607 "y.tab.c" /* yacc.c:1646  */
    break;

  case 69:
#line 2261 "parser.y" /* yacc.c:1646  */
    {
		 (yyval.node) = new_node("insert");
		 Setattr((yyval.node),"section",(yyvsp[-2].id));
		 Setattr((yyval.node),"code",(yyvsp[0].str));
               }
#line 5617 "y.tab.c" /* yacc.c:1646  */
    break;

  case 70:
#line 2266 "parser.y" /* yacc.c:1646  */
    {
		 String *code;
                 skip_balanced('{','}');
		 (yyval.node) = new_node("insert");
		 Setattr((yyval.node),"section",(yyvsp[-2].id));
		 Delitem(scanner_ccode,0);
		 Delitem(scanner_ccode,DOH_END);
		 code = Copy(scanner_ccode);
		 Setattr((yyval.node),"code", code);
		 Delete(code);
	       }
#line 5633 "y.tab.c" /* yacc.c:1646  */
    break;

  case 71:
#line 2284 "parser.y" /* yacc.c:1646  */
    {
                 (yyval.node) = new_node("module");
		 if ((yyvsp[-1].node)) {
		   Setattr((yyval.node),"options",(yyvsp[-1].node));
		   if (Getattr((yyvsp[-1].node),"directors")) {
		     Wrapper_director_mode_set(1);
		     if (!cparse_cplusplus) {
		       Swig_error(cparse_file, cparse_line, "Directors are not supported for C code and require the -c++ option\n");
		     }
		   } 
		   if (Getattr((yyvsp[-1].node),"dirprot")) {
		     Wrapper_director_protected_mode_set(1);
		   } 
		   if (Getattr((yyvsp[-1].node),"allprotected")) {
		     Wrapper_all_protected_mode_set(1);
		   } 
		   if (Getattr((yyvsp[-1].node),"templatereduce")) {
		     template_reduce = 1;
		   }
		   if (Getattr((yyvsp[-1].node),"notemplatereduce")) {
		     template_reduce = 0;
		   }
		 }
		 if (!ModuleName) ModuleName = NewString((yyvsp[0].id));
		 if (!import_mode) {
		   /* first module included, we apply global
		      ModuleName, which can be modify by -module */
		   String *mname = Copy(ModuleName);
		   Setattr((yyval.node),"name",mname);
		   Delete(mname);
		 } else { 
		   /* import mode, we just pass the idstring */
		   Setattr((yyval.node),"name",(yyvsp[0].id));   
		 }		 
		 if (!module_node) module_node = (yyval.node);
	       }
#line 5674 "y.tab.c" /* yacc.c:1646  */
    break;

  case 72:
#line 2327 "parser.y" /* yacc.c:1646  */
    {
                 Swig_warning(WARN_DEPRECATED_NAME,cparse_file,cparse_line, "%%name is deprecated.  Use %%rename instead.\n");
		 Delete(yyrename);
                 yyrename = NewString((yyvsp[-1].id));
		 (yyval.node) = 0;
               }
#line 5685 "y.tab.c" /* yacc.c:1646  */
    break;

  case 73:
#line 2333 "parser.y" /* yacc.c:1646  */
    {
		 Swig_warning(WARN_DEPRECATED_NAME,cparse_file,cparse_line, "%%name is deprecated.  Use %%rename instead.\n");
		 (yyval.node) = 0;
		 Swig_error(cparse_file,cparse_line,"Missing argument to %%name directive.\n");
	       }
#line 5695 "y.tab.c" /* yacc.c:1646  */
    break;

  case 74:
#line 2346 "parser.y" /* yacc.c:1646  */
    {
                 (yyval.node) = new_node("native");
		 Setattr((yyval.node),"name",(yyvsp[-4].id));
		 Setattr((yyval.node),"wrap:name",(yyvsp[-1].id));
	         add_symbols((yyval.node));
	       }
#line 5706 "y.tab.c" /* yacc.c:1646  */
    break;

  case 75:
#line 2352 "parser.y" /* yacc.c:1646  */
    {
		 if (!SwigType_isfunction((yyvsp[-1].decl).type)) {
		   Swig_error(cparse_file,cparse_line,"%%native declaration '%s' is not a function.\n", (yyvsp[-1].decl).id);
		   (yyval.node) = 0;
		 } else {
		     Delete(SwigType_pop_function((yyvsp[-1].decl).type));
		     /* Need check for function here */
		     SwigType_push((yyvsp[-2].type),(yyvsp[-1].decl).type);
		     (yyval.node) = new_node("native");
	             Setattr((yyval.node),"name",(yyvsp[-5].id));
		     Setattr((yyval.node),"wrap:name",(yyvsp[-1].decl).id);
		     Setattr((yyval.node),"type",(yyvsp[-2].type));
		     Setattr((yyval.node),"parms",(yyvsp[-1].decl).parms);
		     Setattr((yyval.node),"decl",(yyvsp[-1].decl).type);
		 }
	         add_symbols((yyval.node));
	       }
#line 5728 "y.tab.c" /* yacc.c:1646  */
    break;

  case 76:
#line 2378 "parser.y" /* yacc.c:1646  */
    {
                 (yyval.node) = new_node("pragma");
		 Setattr((yyval.node),"lang",(yyvsp[-3].id));
		 Setattr((yyval.node),"name",(yyvsp[-2].id));
		 Setattr((yyval.node),"value",(yyvsp[0].str));
	       }
#line 5739 "y.tab.c" /* yacc.c:1646  */
    break;

  case 77:
#line 2384 "parser.y" /* yacc.c:1646  */
    {
		(yyval.node) = new_node("pragma");
		Setattr((yyval.node),"lang",(yyvsp[-1].id));
		Setattr((yyval.node),"name",(yyvsp[0].id));
	      }
#line 5749 "y.tab.c" /* yacc.c:1646  */
    break;

  case 78:
#line 2391 "parser.y" /* yacc.c:1646  */
    { (yyval.str) = (yyvsp[0].str); }
#line 5755 "y.tab.c" /* yacc.c:1646  */
    break;

  case 79:
#line 2392 "parser.y" /* yacc.c:1646  */
    { (yyval.str) = (yyvsp[0].str); }
#line 5761 "y.tab.c" /* yacc.c:1646  */
    break;

  case 80:
#line 2395 "parser.y" /* yacc.c:1646  */
    { (yyval.id) = (yyvsp[-1].id); }
#line 5767 "y.tab.c" /* yacc.c:1646  */
    break;

  case 81:
#line 2396 "parser.y" /* yacc.c:1646  */
    { (yyval.id) = (char *) "swig"; }
#line 5773 "y.tab.c" /* yacc.c:1646  */
    break;

  case 82:
#line 2403 "parser.y" /* yacc.c:1646  */
    {
                SwigType *t = (yyvsp[-2].decl).type;
		Hash *kws = NewHash();
		String *fixname;
		fixname = feature_identifier_fix((yyvsp[-2].decl).id);
		Setattr(kws,"name",(yyvsp[-1].id));
		if (!Len(t)) t = 0;
		/* Special declarator check */
		if (t) {
		  if (SwigType_isfunction(t)) {
		    SwigType *decl = SwigType_pop_function(t);
		    if (SwigType_ispointer(t)) {
		      String *nname = NewStringf("*%s",fixname);
		      if ((yyvsp[-3].intvalue)) {
			Swig_name_rename_add(Namespaceprefix, nname,decl,kws,(yyvsp[-2].decl).parms);
		      } else {
			Swig_name_namewarn_add(Namespaceprefix,nname,decl,kws);
		      }
		      Delete(nname);
		    } else {
		      if ((yyvsp[-3].intvalue)) {
			Swig_name_rename_add(Namespaceprefix,(fixname),decl,kws,(yyvsp[-2].decl).parms);
		      } else {
			Swig_name_namewarn_add(Namespaceprefix,(fixname),decl,kws);
		      }
		    }
		    Delete(decl);
		  } else if (SwigType_ispointer(t)) {
		    String *nname = NewStringf("*%s",fixname);
		    if ((yyvsp[-3].intvalue)) {
		      Swig_name_rename_add(Namespaceprefix,(nname),0,kws,(yyvsp[-2].decl).parms);
		    } else {
		      Swig_name_namewarn_add(Namespaceprefix,(nname),0,kws);
		    }
		    Delete(nname);
		  }
		} else {
		  if ((yyvsp[-3].intvalue)) {
		    Swig_name_rename_add(Namespaceprefix,(fixname),0,kws,(yyvsp[-2].decl).parms);
		  } else {
		    Swig_name_namewarn_add(Namespaceprefix,(fixname),0,kws);
		  }
		}
                (yyval.node) = 0;
		scanner_clear_rename();
              }
#line 5824 "y.tab.c" /* yacc.c:1646  */
    break;

  case 83:
#line 2449 "parser.y" /* yacc.c:1646  */
    {
		String *fixname;
		Hash *kws = (yyvsp[-4].node);
		SwigType *t = (yyvsp[-2].decl).type;
		fixname = feature_identifier_fix((yyvsp[-2].decl).id);
		if (!Len(t)) t = 0;
		/* Special declarator check */
		if (t) {
		  if ((yyvsp[-1].dtype).qualifier) SwigType_push(t,(yyvsp[-1].dtype).qualifier);
		  if (SwigType_isfunction(t)) {
		    SwigType *decl = SwigType_pop_function(t);
		    if (SwigType_ispointer(t)) {
		      String *nname = NewStringf("*%s",fixname);
		      if ((yyvsp[-6].intvalue)) {
			Swig_name_rename_add(Namespaceprefix, nname,decl,kws,(yyvsp[-2].decl).parms);
		      } else {
			Swig_name_namewarn_add(Namespaceprefix,nname,decl,kws);
		      }
		      Delete(nname);
		    } else {
		      if ((yyvsp[-6].intvalue)) {
			Swig_name_rename_add(Namespaceprefix,(fixname),decl,kws,(yyvsp[-2].decl).parms);
		      } else {
			Swig_name_namewarn_add(Namespaceprefix,(fixname),decl,kws);
		      }
		    }
		    Delete(decl);
		  } else if (SwigType_ispointer(t)) {
		    String *nname = NewStringf("*%s",fixname);
		    if ((yyvsp[-6].intvalue)) {
		      Swig_name_rename_add(Namespaceprefix,(nname),0,kws,(yyvsp[-2].decl).parms);
		    } else {
		      Swig_name_namewarn_add(Namespaceprefix,(nname),0,kws);
		    }
		    Delete(nname);
		  }
		} else {
		  if ((yyvsp[-6].intvalue)) {
		    Swig_name_rename_add(Namespaceprefix,(fixname),0,kws,(yyvsp[-2].decl).parms);
		  } else {
		    Swig_name_namewarn_add(Namespaceprefix,(fixname),0,kws);
		  }
		}
                (yyval.node) = 0;
		scanner_clear_rename();
              }
#line 5875 "y.tab.c" /* yacc.c:1646  */
    break;

  case 84:
#line 2495 "parser.y" /* yacc.c:1646  */
    {
		if ((yyvsp[-5].intvalue)) {
		  Swig_name_rename_add(Namespaceprefix,(yyvsp[-1].str),0,(yyvsp[-3].node),0);
		} else {
		  Swig_name_namewarn_add(Namespaceprefix,(yyvsp[-1].str),0,(yyvsp[-3].node));
		}
		(yyval.node) = 0;
		scanner_clear_rename();
              }
#line 5889 "y.tab.c" /* yacc.c:1646  */
    break;

  case 85:
#line 2506 "parser.y" /* yacc.c:1646  */
    {
		    (yyval.intvalue) = 1;
                }
#line 5897 "y.tab.c" /* yacc.c:1646  */
    break;

  case 86:
#line 2509 "parser.y" /* yacc.c:1646  */
    {
                    (yyval.intvalue) = 0;
                }
#line 5905 "y.tab.c" /* yacc.c:1646  */
    break;

  case 87:
#line 2536 "parser.y" /* yacc.c:1646  */
    {
                    String *val = (yyvsp[0].str) ? NewString((yyvsp[0].str)) : NewString("1");
                    new_feature((yyvsp[-4].id), val, 0, (yyvsp[-2].decl).id, (yyvsp[-2].decl).type, (yyvsp[-2].decl).parms, (yyvsp[-1].dtype).qualifier);
                    (yyval.node) = 0;
                    scanner_clear_rename();
                  }
#line 5916 "y.tab.c" /* yacc.c:1646  */
    break;

  case 88:
#line 2542 "parser.y" /* yacc.c:1646  */
    {
                    String *val = Len((yyvsp[-4].str)) ? (yyvsp[-4].str) : 0;
                    new_feature((yyvsp[-6].id), val, 0, (yyvsp[-2].decl).id, (yyvsp[-2].decl).type, (yyvsp[-2].decl).parms, (yyvsp[-1].dtype).qualifier);
                    (yyval.node) = 0;
                    scanner_clear_rename();
                  }
#line 5927 "y.tab.c" /* yacc.c:1646  */
    break;

  case 89:
#line 2548 "parser.y" /* yacc.c:1646  */
    {
                    String *val = (yyvsp[0].str) ? NewString((yyvsp[0].str)) : NewString("1");
                    new_feature((yyvsp[-5].id), val, (yyvsp[-4].node), (yyvsp[-2].decl).id, (yyvsp[-2].decl).type, (yyvsp[-2].decl).parms, (yyvsp[-1].dtype).qualifier);
                    (yyval.node) = 0;
                    scanner_clear_rename();
                  }
#line 5938 "y.tab.c" /* yacc.c:1646  */
    break;

  case 90:
#line 2554 "parser.y" /* yacc.c:1646  */
    {
                    String *val = Len((yyvsp[-5].str)) ? (yyvsp[-5].str) : 0;
                    new_feature((yyvsp[-7].id), val, (yyvsp[-4].node), (yyvsp[-2].decl).id, (yyvsp[-2].decl).type, (yyvsp[-2].decl).parms, (yyvsp[-1].dtype).qualifier);
                    (yyval.node) = 0;
                    scanner_clear_rename();
                  }
#line 5949 "y.tab.c" /* yacc.c:1646  */
    break;

  case 91:
#line 2562 "parser.y" /* yacc.c:1646  */
    {
                    String *val = (yyvsp[0].str) ? NewString((yyvsp[0].str)) : NewString("1");
                    new_feature((yyvsp[-2].id), val, 0, 0, 0, 0, 0);
                    (yyval.node) = 0;
                    scanner_clear_rename();
                  }
#line 5960 "y.tab.c" /* yacc.c:1646  */
    break;

  case 92:
#line 2568 "parser.y" /* yacc.c:1646  */
    {
                    String *val = Len((yyvsp[-2].str)) ? (yyvsp[-2].str) : 0;
                    new_feature((yyvsp[-4].id), val, 0, 0, 0, 0, 0);
                    (yyval.node) = 0;
                    scanner_clear_rename();
                  }
#line 5971 "y.tab.c" /* yacc.c:1646  */
    break;

  case 93:
#line 2574 "parser.y" /* yacc.c:1646  */
    {
                    String *val = (yyvsp[0].str) ? NewString((yyvsp[0].str)) : NewString("1");
                    new_feature((yyvsp[-3].id), val, (yyvsp[-2].node), 0, 0, 0, 0);
                    (yyval.node) = 0;
                    scanner_clear_rename();
                  }
#line 5982 "y.tab.c" /* yacc.c:1646  */
    break;

  case 94:
#line 2580 "parser.y" /* yacc.c:1646  */
    {
                    String *val = Len((yyvsp[-3].str)) ? (yyvsp[-3].str) : 0;
                    new_feature((yyvsp[-5].id), val, (yyvsp[-2].node), 0, 0, 0, 0);
                    (yyval.node) = 0;
                    scanner_clear_rename();
                  }
#line 5993 "y.tab.c" /* yacc.c:1646  */
    break;

  case 95:
#line 2588 "parser.y" /* yacc.c:1646  */
    { (yyval.str) = (yyvsp[0].str); }
#line 5999 "y.tab.c" /* yacc.c:1646  */
    break;

  case 96:
#line 2589 "parser.y" /* yacc.c:1646  */
    { (yyval.str) = 0; }
#line 6005 "y.tab.c" /* yacc.c:1646  */
    break;

  case 97:
#line 2590 "parser.y" /* yacc.c:1646  */
    { (yyval.str) = (yyvsp[-2].pl); }
#line 6011 "y.tab.c" /* yacc.c:1646  */
    break;

  case 98:
#line 2593 "parser.y" /* yacc.c:1646  */
    {
		  (yyval.node) = NewHash();
		  Setattr((yyval.node),"name",(yyvsp[-2].id));
		  Setattr((yyval.node),"value",(yyvsp[0].str));
                }
#line 6021 "y.tab.c" /* yacc.c:1646  */
    break;

  case 99:
#line 2598 "parser.y" /* yacc.c:1646  */
    {
		  (yyval.node) = NewHash();
		  Setattr((yyval.node),"name",(yyvsp[-3].id));
		  Setattr((yyval.node),"value",(yyvsp[-1].str));
                  set_nextSibling((yyval.node),(yyvsp[0].node));
                }
#line 6032 "y.tab.c" /* yacc.c:1646  */
    break;

  case 100:
#line 2608 "parser.y" /* yacc.c:1646  */
    {
                 Parm *val;
		 String *name;
		 SwigType *t;
		 if (Namespaceprefix) name = NewStringf("%s::%s", Namespaceprefix, (yyvsp[-2].decl).id);
		 else name = NewString((yyvsp[-2].decl).id);
		 val = (yyvsp[-4].pl);
		 if ((yyvsp[-2].decl).parms) {
		   Setmeta(val,"parms",(yyvsp[-2].decl).parms);
		 }
		 t = (yyvsp[-2].decl).type;
		 if (!Len(t)) t = 0;
		 if (t) {
		   if ((yyvsp[-1].dtype).qualifier) SwigType_push(t,(yyvsp[-1].dtype).qualifier);
		   if (SwigType_isfunction(t)) {
		     SwigType *decl = SwigType_pop_function(t);
		     if (SwigType_ispointer(t)) {
		       String *nname = NewStringf("*%s",name);
		       Swig_feature_set(Swig_cparse_features(), nname, decl, "feature:varargs", val, 0);
		       Delete(nname);
		     } else {
		       Swig_feature_set(Swig_cparse_features(), name, decl, "feature:varargs", val, 0);
		     }
		     Delete(decl);
		   } else if (SwigType_ispointer(t)) {
		     String *nname = NewStringf("*%s",name);
		     Swig_feature_set(Swig_cparse_features(),nname,0,"feature:varargs",val, 0);
		     Delete(nname);
		   }
		 } else {
		   Swig_feature_set(Swig_cparse_features(),name,0,"feature:varargs",val, 0);
		 }
		 Delete(name);
		 (yyval.node) = 0;
              }
#line 6072 "y.tab.c" /* yacc.c:1646  */
    break;

  case 101:
#line 2644 "parser.y" /* yacc.c:1646  */
    { (yyval.pl) = (yyvsp[0].pl); }
#line 6078 "y.tab.c" /* yacc.c:1646  */
    break;

  case 102:
#line 2645 "parser.y" /* yacc.c:1646  */
    { 
		  int i;
		  int n;
		  Parm *p;
		  n = atoi(Char((yyvsp[-2].dtype).val));
		  if (n <= 0) {
		    Swig_error(cparse_file, cparse_line,"Argument count in %%varargs must be positive.\n");
		    (yyval.pl) = 0;
		  } else {
		    String *name = Getattr((yyvsp[0].p), "name");
		    (yyval.pl) = Copy((yyvsp[0].p));
		    if (name)
		      Setattr((yyval.pl), "name", NewStringf("%s%d", name, n));
		    for (i = 1; i < n; i++) {
		      p = Copy((yyvsp[0].p));
		      name = Getattr(p, "name");
		      if (name)
		        Setattr(p, "name", NewStringf("%s%d", name, n-i));
		      set_nextSibling(p,(yyval.pl));
		      Delete((yyval.pl));
		      (yyval.pl) = p;
		    }
		  }
                }
#line 6107 "y.tab.c" /* yacc.c:1646  */
    break;

  case 103:
#line 2680 "parser.y" /* yacc.c:1646  */
    {
		   (yyval.node) = 0;
		   if ((yyvsp[-3].tmap).method) {
		     String *code = 0;
		     (yyval.node) = new_node("typemap");
		     Setattr((yyval.node),"method",(yyvsp[-3].tmap).method);
		     if ((yyvsp[-3].tmap).kwargs) {
		       ParmList *kw = (yyvsp[-3].tmap).kwargs;
                       code = remove_block(kw, (yyvsp[0].str));
		       Setattr((yyval.node),"kwargs", (yyvsp[-3].tmap).kwargs);
		     }
		     code = code ? code : NewString((yyvsp[0].str));
		     Setattr((yyval.node),"code", code);
		     Delete(code);
		     appendChild((yyval.node),(yyvsp[-1].p));
		   }
	       }
#line 6129 "y.tab.c" /* yacc.c:1646  */
    break;

  case 104:
#line 2697 "parser.y" /* yacc.c:1646  */
    {
		 (yyval.node) = 0;
		 if ((yyvsp[-3].tmap).method) {
		   (yyval.node) = new_node("typemap");
		   Setattr((yyval.node),"method",(yyvsp[-3].tmap).method);
		   appendChild((yyval.node),(yyvsp[-1].p));
		 }
	       }
#line 6142 "y.tab.c" /* yacc.c:1646  */
    break;

  case 105:
#line 2705 "parser.y" /* yacc.c:1646  */
    {
		   (yyval.node) = 0;
		   if ((yyvsp[-5].tmap).method) {
		     (yyval.node) = new_node("typemapcopy");
		     Setattr((yyval.node),"method",(yyvsp[-5].tmap).method);
		     Setattr((yyval.node),"pattern", Getattr((yyvsp[-1].p),"pattern"));
		     appendChild((yyval.node),(yyvsp[-3].p));
		   }
	       }
#line 6156 "y.tab.c" /* yacc.c:1646  */
    break;

  case 106:
#line 2718 "parser.y" /* yacc.c:1646  */
    {
		 Hash *p;
		 String *name;
		 p = nextSibling((yyvsp[0].node));
		 if (p && (!Getattr(p,"value"))) {
 		   /* this is the deprecated two argument typemap form */
 		   Swig_warning(WARN_DEPRECATED_TYPEMAP_LANG,cparse_file, cparse_line,
				"Specifying the language name in %%typemap is deprecated - use #ifdef SWIG<LANG> instead.\n");
		   /* two argument typemap form */
		   name = Getattr((yyvsp[0].node),"name");
		   if (!name || (Strcmp(name,typemap_lang))) {
		     (yyval.tmap).method = 0;
		     (yyval.tmap).kwargs = 0;
		   } else {
		     (yyval.tmap).method = Getattr(p,"name");
		     (yyval.tmap).kwargs = nextSibling(p);
		   }
		 } else {
		   /* one-argument typemap-form */
		   (yyval.tmap).method = Getattr((yyvsp[0].node),"name");
		   (yyval.tmap).kwargs = p;
		 }
                }
#line 6184 "y.tab.c" /* yacc.c:1646  */
    break;

  case 107:
#line 2743 "parser.y" /* yacc.c:1646  */
    {
                 (yyval.p) = (yyvsp[-1].p);
		 set_nextSibling((yyval.p),(yyvsp[0].p));
		}
#line 6193 "y.tab.c" /* yacc.c:1646  */
    break;

  case 108:
#line 2749 "parser.y" /* yacc.c:1646  */
    {
                 (yyval.p) = (yyvsp[-1].p);
		 set_nextSibling((yyval.p),(yyvsp[0].p));
                }
#line 6202 "y.tab.c" /* yacc.c:1646  */
    break;

  case 109:
#line 2753 "parser.y" /* yacc.c:1646  */
    { (yyval.p) = 0;}
#line 6208 "y.tab.c" /* yacc.c:1646  */
    break;

  case 110:
#line 2756 "parser.y" /* yacc.c:1646  */
    {
                  Parm *parm;
		  SwigType_push((yyvsp[-1].type),(yyvsp[0].decl).type);
		  (yyval.p) = new_node("typemapitem");
		  parm = NewParmWithoutFileLineInfo((yyvsp[-1].type),(yyvsp[0].decl).id);
		  Setattr((yyval.p),"pattern",parm);
		  Setattr((yyval.p),"parms", (yyvsp[0].decl).parms);
		  Delete(parm);
		  /*		  $$ = NewParmWithoutFileLineInfo($1,$2.id);
				  Setattr($$,"parms",$2.parms); */
                }
#line 6224 "y.tab.c" /* yacc.c:1646  */
    break;

  case 111:
#line 2767 "parser.y" /* yacc.c:1646  */
    {
                  (yyval.p) = new_node("typemapitem");
		  Setattr((yyval.p),"pattern",(yyvsp[-1].pl));
		  /*		  Setattr($$,"multitype",$2); */
               }
#line 6234 "y.tab.c" /* yacc.c:1646  */
    break;

  case 112:
#line 2772 "parser.y" /* yacc.c:1646  */
    {
		 (yyval.p) = new_node("typemapitem");
		 Setattr((yyval.p),"pattern", (yyvsp[-4].pl));
		 /*                 Setattr($$,"multitype",$2); */
		 Setattr((yyval.p),"parms",(yyvsp[-1].pl));
               }
#line 6245 "y.tab.c" /* yacc.c:1646  */
    break;

  case 113:
#line 2785 "parser.y" /* yacc.c:1646  */
    {
                   (yyval.node) = new_node("types");
		   Setattr((yyval.node),"parms",(yyvsp[-2].pl));
                   if ((yyvsp[0].str))
		     Setattr((yyval.node),"convcode",NewString((yyvsp[0].str)));
               }
#line 6256 "y.tab.c" /* yacc.c:1646  */
    break;

  case 114:
#line 2797 "parser.y" /* yacc.c:1646  */
    {
                  Parm *p, *tp;
		  Node *n;
		  Node *outer_class = currentOuterClass;
		  Symtab *tscope = 0;
		  int     specialized = 0;
		  int     variadic = 0;

		  (yyval.node) = 0;

		  tscope = Swig_symbol_current();          /* Get the current scope */

		  /* If the class name is qualified, we need to create or lookup namespace entries */
		  (yyvsp[-4].str) = resolve_create_node_scope((yyvsp[-4].str), 0);

		  if (nscope_inner && Strcmp(nodeType(nscope_inner), "class") == 0) {
		    outer_class	= nscope_inner;
		  }

		  /*
		    We use the new namespace entry 'nscope' only to
		    emit the template node. The template parameters are
		    resolved in the current 'tscope'.

		    This is closer to the C++ (typedef) behavior.
		  */
		  n = Swig_cparse_template_locate((yyvsp[-4].str),(yyvsp[-2].p),tscope);

		  /* Patch the argument types to respect namespaces */
		  p = (yyvsp[-2].p);
		  while (p) {
		    SwigType *value = Getattr(p,"value");
		    if (!value) {
		      SwigType *ty = Getattr(p,"type");
		      if (ty) {
			SwigType *rty = 0;
			int reduce = template_reduce;
			if (reduce || !SwigType_ispointer(ty)) {
			  rty = Swig_symbol_typedef_reduce(ty,tscope);
			  if (!reduce) reduce = SwigType_ispointer(rty);
			}
			ty = reduce ? Swig_symbol_type_qualify(rty,tscope) : Swig_symbol_type_qualify(ty,tscope);
			Setattr(p,"type",ty);
			Delete(ty);
			Delete(rty);
		      }
		    } else {
		      value = Swig_symbol_type_qualify(value,tscope);
		      Setattr(p,"value",value);
		      Delete(value);
		    }

		    p = nextSibling(p);
		  }

		  /* Look for the template */
		  {
                    Node *nn = n;
                    Node *linklistend = 0;
                    Node *linkliststart = 0;
                    while (nn) {
                      Node *templnode = 0;
                      if (Strcmp(nodeType(nn),"template") == 0) {
                        int nnisclass = (Strcmp(Getattr(nn,"templatetype"),"class") == 0); /* if not a templated class it is a templated function */
                        Parm *tparms = Getattr(nn,"templateparms");
                        if (!tparms) {
                          specialized = 1;
                        } else if (Getattr(tparms,"variadic") && strncmp(Char(Getattr(tparms,"variadic")), "1", 1)==0) {
                          variadic = 1;
                        }
                        if (nnisclass && !variadic && !specialized && (ParmList_len((yyvsp[-2].p)) > ParmList_len(tparms))) {
                          Swig_error(cparse_file, cparse_line, "Too many template parameters. Maximum of %d.\n", ParmList_len(tparms));
                        } else if (nnisclass && !specialized && ((ParmList_len((yyvsp[-2].p)) < (ParmList_numrequired(tparms) - (variadic?1:0))))) { /* Variadic parameter is optional */
                          Swig_error(cparse_file, cparse_line, "Not enough template parameters specified. %d required.\n", (ParmList_numrequired(tparms)-(variadic?1:0)) );
                        } else if (!nnisclass && ((ParmList_len((yyvsp[-2].p)) != ParmList_len(tparms)))) {
                          /* must be an overloaded templated method - ignore it as it is overloaded with a different number of template parameters */
                          nn = Getattr(nn,"sym:nextSibling"); /* repeat for overloaded templated functions */
                          continue;
                        } else {
			  String *tname = Copy((yyvsp[-4].str));
                          int def_supplied = 0;
                          /* Expand the template */
			  Node *templ = Swig_symbol_clookup((yyvsp[-4].str),0);
			  Parm *targs = templ ? Getattr(templ,"templateparms") : 0;

                          ParmList *temparms;
                          if (specialized) temparms = CopyParmList((yyvsp[-2].p));
                          else temparms = CopyParmList(tparms);

                          /* Create typedef's and arguments */
                          p = (yyvsp[-2].p);
                          tp = temparms;
                          if (!p && ParmList_len(p) != ParmList_len(temparms)) {
                            /* we have no template parameters supplied in %template for a template that has default args*/
                            p = tp;
                            def_supplied = 1;
                          }

                          while (p) {
                            String *value = Getattr(p,"value");
                            if (def_supplied) {
                              Setattr(p,"default","1");
                            }
                            if (value) {
                              Setattr(tp,"value",value);
                            } else {
                              SwigType *ty = Getattr(p,"type");
                              if (ty) {
                                Setattr(tp,"type",ty);
                              }
                              Delattr(tp,"value");
                            }
			    /* fix default arg values */
			    if (targs) {
			      Parm *pi = temparms;
			      Parm *ti = targs;
			      String *tv = Getattr(tp,"value");
			      if (!tv) tv = Getattr(tp,"type");
			      while(pi != tp && ti && pi) {
				String *name = Getattr(ti,"name");
				String *value = Getattr(pi,"value");
				if (!value) value = Getattr(pi,"type");
				Replaceid(tv, name, value);
				pi = nextSibling(pi);
				ti = nextSibling(ti);
			      }
			    }
                            p = nextSibling(p);
                            tp = nextSibling(tp);
                            if (!p && tp) {
                              p = tp;
                              def_supplied = 1;
                            } else if (p && !tp) { /* Variadic template - tp < p */
			      SWIG_WARN_NODE_BEGIN(nn);
                              Swig_warning(WARN_CPP11_VARIADIC_TEMPLATE,cparse_file, cparse_line,"Only the first variadic template argument is currently supported.\n");
			      SWIG_WARN_NODE_END(nn);
                              break;
                            }
                          }

                          templnode = copy_node(nn);
			  update_nested_classes(templnode); /* update classes nested within template */
                          /* We need to set the node name based on name used to instantiate */
                          Setattr(templnode,"name",tname);
			  Delete(tname);
                          if (!specialized) {
                            Delattr(templnode,"sym:typename");
                          } else {
                            Setattr(templnode,"sym:typename","1");
                          }
			  /* for now, nested %template is allowed only in the same scope as the template declaration */
                          if ((yyvsp[-6].id) && !(nnisclass && ((outer_class && (outer_class != Getattr(nn, "nested:outer")))
			    ||(extendmode && current_class && (current_class != Getattr(nn, "nested:outer")))))) {
			    /*
			       Comment this out for 1.3.28. We need to
			       re-enable it later but first we need to
			       move %ignore from using %rename to use
			       %feature(ignore).

			       String *symname = Swig_name_make(templnode,0,$3,0,0);
			    */
			    String *symname = NewString((yyvsp[-6].id));
                            Swig_cparse_template_expand(templnode,symname,temparms,tscope);
                            Setattr(templnode,"sym:name",symname);
                          } else {
                            static int cnt = 0;
                            String *nname = NewStringf("__dummy_%d__", cnt++);
                            Swig_cparse_template_expand(templnode,nname,temparms,tscope);
                            Setattr(templnode,"sym:name",nname);
			    Delete(nname);
                            Setattr(templnode,"feature:onlychildren", "typemap,typemapitem,typemapcopy,typedef,types,fragment,apply");
			    if ((yyvsp[-6].id)) {
			      Swig_warning(WARN_PARSE_NESTED_TEMPLATE, cparse_file, cparse_line, "Named nested template instantiations not supported. Processing as if no name was given to %%template().\n");
			    }
                          }
                          Delattr(templnode,"templatetype");
                          Setattr(templnode,"template",nn);
                          Setfile(templnode,cparse_file);
                          Setline(templnode,cparse_line);
                          Delete(temparms);
			  if (outer_class && nnisclass) {
			    SetFlag(templnode, "nested");
			    Setattr(templnode, "nested:outer", outer_class);
			  }
                          add_symbols_copy(templnode);

                          if (Strcmp(nodeType(templnode),"class") == 0) {

                            /* Identify pure abstract methods */
                            Setattr(templnode,"abstracts", pure_abstracts(firstChild(templnode)));

                            /* Set up inheritance in symbol table */
                            {
                              Symtab  *csyms;
                              List *baselist = Getattr(templnode,"baselist");
                              csyms = Swig_symbol_current();
                              Swig_symbol_setscope(Getattr(templnode,"symtab"));
                              if (baselist) {
                                List *bases = Swig_make_inherit_list(Getattr(templnode,"name"),baselist, Namespaceprefix);
                                if (bases) {
                                  Iterator s;
                                  for (s = First(bases); s.item; s = Next(s)) {
                                    Symtab *st = Getattr(s.item,"symtab");
                                    if (st) {
				      Setfile(st,Getfile(s.item));
				      Setline(st,Getline(s.item));
                                      Swig_symbol_inherit(st);
                                    }
                                  }
				  Delete(bases);
                                }
                              }
                              Swig_symbol_setscope(csyms);
                            }

                            /* Merge in %extend methods for this class.
			       This only merges methods within %extend for a template specialized class such as
			         template<typename T> class K {}; %extend K<int> { ... }
			       The copy_node() call above has already added in the generic %extend methods such as
			         template<typename T> class K {}; %extend K { ... } */

			    /* !!! This may be broken.  We may have to add the
			       %extend methods at the beginning of the class */
                            {
                              String *stmp = 0;
                              String *clsname;
                              Node *am;
                              if (Namespaceprefix) {
                                clsname = stmp = NewStringf("%s::%s", Namespaceprefix, Getattr(templnode,"name"));
                              } else {
                                clsname = Getattr(templnode,"name");
                              }
                              am = Getattr(Swig_extend_hash(),clsname);
                              if (am) {
                                Symtab *st = Swig_symbol_current();
                                Swig_symbol_setscope(Getattr(templnode,"symtab"));
                                /*			    Printf(stdout,"%s: %s %p %p\n", Getattr(templnode,"name"), clsname, Swig_symbol_current(), Getattr(templnode,"symtab")); */
                                Swig_extend_merge(templnode,am);
                                Swig_symbol_setscope(st);
				Swig_extend_append_previous(templnode,am);
                                Delattr(Swig_extend_hash(),clsname);
                              }
			      if (stmp) Delete(stmp);
                            }

                            /* Add to classes hash */
			    if (!classes)
			      classes = NewHash();

			    if (Namespaceprefix) {
			      String *temp = NewStringf("%s::%s", Namespaceprefix, Getattr(templnode,"name"));
			      Setattr(classes,temp,templnode);
			      Delete(temp);
			    } else {
			      String *qs = Swig_symbol_qualifiedscopename(templnode);
			      Setattr(classes, qs,templnode);
			      Delete(qs);
			    }
                          }
                        }

                        /* all the overloaded templated functions are added into a linked list */
                        if (!linkliststart)
                          linkliststart = templnode;
                        if (nscope_inner) {
                          /* non-global namespace */
                          if (templnode) {
                            appendChild(nscope_inner,templnode);
			    Delete(templnode);
                            if (nscope) (yyval.node) = nscope;
                          }
                        } else {
                          /* global namespace */
                          if (!linklistend) {
                            (yyval.node) = templnode;
                          } else {
                            set_nextSibling(linklistend,templnode);
			    Delete(templnode);
                          }
                          linklistend = templnode;
                        }
                      }
                      nn = Getattr(nn,"sym:nextSibling"); /* repeat for overloaded templated functions. If a templated class there will never be a sibling. */
                    }
                    update_defaultargs(linkliststart);
                    update_abstracts(linkliststart);
		  }
	          Swig_symbol_setscope(tscope);
		  Delete(Namespaceprefix);
		  Namespaceprefix = Swig_symbol_qualifiedscopename(0);
                }
#line 6552 "y.tab.c" /* yacc.c:1646  */
    break;

  case 115:
#line 3095 "parser.y" /* yacc.c:1646  */
    {
		  Swig_warning(0,cparse_file, cparse_line,"%s\n", (yyvsp[0].str));
		  (yyval.node) = 0;
               }
#line 6561 "y.tab.c" /* yacc.c:1646  */
    break;

  case 116:
#line 3105 "parser.y" /* yacc.c:1646  */
    {
                    (yyval.node) = (yyvsp[0].node); 
                    if ((yyval.node)) {
   		      add_symbols((yyval.node));
                      default_arguments((yyval.node));
   	            }
                }
#line 6573 "y.tab.c" /* yacc.c:1646  */
    break;

  case 117:
#line 3112 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = (yyvsp[0].node); }
#line 6579 "y.tab.c" /* yacc.c:1646  */
    break;

  case 118:
#line 3113 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = (yyvsp[0].node); }
#line 6585 "y.tab.c" /* yacc.c:1646  */
    break;

  case 119:
#line 3117 "parser.y" /* yacc.c:1646  */
    {
		  if (Strcmp((yyvsp[-1].str),"C") == 0) {
		    cparse_externc = 1;
		  }
		}
#line 6595 "y.tab.c" /* yacc.c:1646  */
    break;

  case 120:
#line 3121 "parser.y" /* yacc.c:1646  */
    {
		  cparse_externc = 0;
		  if (Strcmp((yyvsp[-4].str),"C") == 0) {
		    Node *n = firstChild((yyvsp[-1].node));
		    (yyval.node) = new_node("extern");
		    Setattr((yyval.node),"name",(yyvsp[-4].str));
		    appendChild((yyval.node),n);
		    while (n) {
		      SwigType *decl = Getattr(n,"decl");
		      if (SwigType_isfunction(decl) && !Equal(Getattr(n, "storage"), "typedef")) {
			Setattr(n,"storage","externc");
		      }
		      n = nextSibling(n);
		    }
		  } else {
		     Swig_warning(WARN_PARSE_UNDEFINED_EXTERN,cparse_file, cparse_line,"Unrecognized extern type \"%s\".\n", (yyvsp[-4].str));
		    (yyval.node) = new_node("extern");
		    Setattr((yyval.node),"name",(yyvsp[-4].str));
		    appendChild((yyval.node),firstChild((yyvsp[-1].node)));
		  }
                }
#line 6621 "y.tab.c" /* yacc.c:1646  */
    break;

  case 121:
#line 3142 "parser.y" /* yacc.c:1646  */
    {
		  (yyval.node) = (yyvsp[0].node);
		  SWIG_WARN_NODE_BEGIN((yyval.node));
		  Swig_warning(WARN_CPP11_LAMBDA, cparse_file, cparse_line, "Lambda expressions and closures are not fully supported yet.\n");
		  SWIG_WARN_NODE_END((yyval.node));
		}
#line 6632 "y.tab.c" /* yacc.c:1646  */
    break;

  case 122:
#line 3148 "parser.y" /* yacc.c:1646  */
    {
		  /* Convert using statement to a typedef statement */
		  (yyval.node) = new_node("cdecl");
		  Setattr((yyval.node),"type",(yyvsp[-2].type));
		  Setattr((yyval.node),"storage","typedef");
		  Setattr((yyval.node),"name",(yyvsp[-4].str));
		  Setattr((yyval.node),"decl",(yyvsp[-1].decl).type);
		  SetFlag((yyval.node),"typealias");
		  add_symbols((yyval.node));
		}
#line 6647 "y.tab.c" /* yacc.c:1646  */
    break;

  case 123:
#line 3158 "parser.y" /* yacc.c:1646  */
    {
		  /* Convert alias template to a "template" typedef statement */
		  (yyval.node) = new_node("template");
		  Setattr((yyval.node),"type",(yyvsp[-2].type));
		  Setattr((yyval.node),"storage","typedef");
		  Setattr((yyval.node),"name",(yyvsp[-4].str));
		  Setattr((yyval.node),"decl",(yyvsp[-1].decl).type);
		  Setattr((yyval.node),"templateparms",(yyvsp[-7].tparms));
		  Setattr((yyval.node),"templatetype","cdecl");
		  SetFlag((yyval.node),"aliastemplate");
		  add_symbols((yyval.node));
		}
#line 6664 "y.tab.c" /* yacc.c:1646  */
    break;

  case 124:
#line 3170 "parser.y" /* yacc.c:1646  */
    {
                   (yyval.node) = (yyvsp[0].node);
                }
#line 6672 "y.tab.c" /* yacc.c:1646  */
    break;

  case 125:
#line 3179 "parser.y" /* yacc.c:1646  */
    {
	      String *decl = (yyvsp[-3].decl).type;
              (yyval.node) = new_node("cdecl");
	      if ((yyvsp[-2].dtype).qualifier)
	        decl = add_qualifier_to_declarator((yyvsp[-3].decl).type, (yyvsp[-2].dtype).qualifier);
	      Setattr((yyval.node),"refqualifier",(yyvsp[-2].dtype).refqualifier);
	      Setattr((yyval.node),"type",(yyvsp[-4].type));
	      Setattr((yyval.node),"storage",(yyvsp[-5].id));
	      Setattr((yyval.node),"name",(yyvsp[-3].decl).id);
	      Setattr((yyval.node),"decl",decl);
	      Setattr((yyval.node),"parms",(yyvsp[-3].decl).parms);
	      Setattr((yyval.node),"value",(yyvsp[-1].dtype).val);
	      Setattr((yyval.node),"throws",(yyvsp[-2].dtype).throws);
	      Setattr((yyval.node),"throw",(yyvsp[-2].dtype).throwf);
	      Setattr((yyval.node),"noexcept",(yyvsp[-2].dtype).nexcept);
	      Setattr((yyval.node),"final",(yyvsp[-2].dtype).final);
	      if ((yyvsp[-1].dtype).val && (yyvsp[-1].dtype).type) {
		/* store initializer type as it might be different to the declared type */
		SwigType *valuetype = NewSwigType((yyvsp[-1].dtype).type);
		if (Len(valuetype) > 0)
		  Setattr((yyval.node),"valuetype",valuetype);
		else
		  Delete(valuetype);
	      }
	      if (!(yyvsp[0].node)) {
		if (Len(scanner_ccode)) {
		  String *code = Copy(scanner_ccode);
		  Setattr((yyval.node),"code",code);
		  Delete(code);
		}
	      } else {
		Node *n = (yyvsp[0].node);
		/* Inherit attributes */
		while (n) {
		  String *type = Copy((yyvsp[-4].type));
		  Setattr(n,"type",type);
		  Setattr(n,"storage",(yyvsp[-5].id));
		  n = nextSibling(n);
		  Delete(type);
		}
	      }
	      if ((yyvsp[-1].dtype).bitfield) {
		Setattr((yyval.node),"bitfield", (yyvsp[-1].dtype).bitfield);
	      }

	      if ((yyvsp[-3].decl).id) {
		/* Look for "::" declarations (ignored) */
		if (Strstr((yyvsp[-3].decl).id, "::")) {
		  /* This is a special case. If the scope name of the declaration exactly
		     matches that of the declaration, then we will allow it. Otherwise, delete. */
		  String *p = Swig_scopename_prefix((yyvsp[-3].decl).id);
		  if (p) {
		    if ((Namespaceprefix && Strcmp(p, Namespaceprefix) == 0) ||
			(Classprefix && Strcmp(p, Classprefix) == 0)) {
		      String *lstr = Swig_scopename_last((yyvsp[-3].decl).id);
		      Setattr((yyval.node), "name", lstr);
		      Delete(lstr);
		      set_nextSibling((yyval.node), (yyvsp[0].node));
		    } else {
		      Delete((yyval.node));
		      (yyval.node) = (yyvsp[0].node);
		    }
		    Delete(p);
		  } else {
		    Delete((yyval.node));
		    (yyval.node) = (yyvsp[0].node);
		  }
		} else {
		  set_nextSibling((yyval.node), (yyvsp[0].node));
		}
	      } else {
		Swig_error(cparse_file, cparse_line, "Missing symbol name for global declaration\n");
		(yyval.node) = 0;
	      }

	      if ((yyvsp[-2].dtype).qualifier && (yyvsp[-5].id) && Strstr((yyvsp[-5].id), "static"))
		Swig_error(cparse_file, cparse_line, "Static function %s cannot have a qualifier.\n", Swig_name_decl((yyval.node)));
           }
#line 6755 "y.tab.c" /* yacc.c:1646  */
    break;

  case 126:
#line 3259 "parser.y" /* yacc.c:1646  */
    {
              (yyval.node) = new_node("cdecl");
	      if ((yyvsp[-5].dtype).qualifier) SwigType_push((yyvsp[-6].decl).type, (yyvsp[-5].dtype).qualifier);
	      Setattr((yyval.node),"refqualifier",(yyvsp[-5].dtype).refqualifier);
	      Setattr((yyval.node),"type",(yyvsp[-3].node));
	      Setattr((yyval.node),"storage",(yyvsp[-8].id));
	      Setattr((yyval.node),"name",(yyvsp[-6].decl).id);
	      Setattr((yyval.node),"decl",(yyvsp[-6].decl).type);
	      Setattr((yyval.node),"parms",(yyvsp[-6].decl).parms);
	      Setattr((yyval.node),"value",(yyvsp[-5].dtype).val);
	      Setattr((yyval.node),"throws",(yyvsp[-5].dtype).throws);
	      Setattr((yyval.node),"throw",(yyvsp[-5].dtype).throwf);
	      Setattr((yyval.node),"noexcept",(yyvsp[-5].dtype).nexcept);
	      Setattr((yyval.node),"final",(yyvsp[-5].dtype).final);
	      if (!(yyvsp[0].node)) {
		if (Len(scanner_ccode)) {
		  String *code = Copy(scanner_ccode);
		  Setattr((yyval.node),"code",code);
		  Delete(code);
		}
	      } else {
		Node *n = (yyvsp[0].node);
		while (n) {
		  String *type = Copy((yyvsp[-3].node));
		  Setattr(n,"type",type);
		  Setattr(n,"storage",(yyvsp[-8].id));
		  n = nextSibling(n);
		  Delete(type);
		}
	      }
	      if ((yyvsp[-5].dtype).bitfield) {
		Setattr((yyval.node),"bitfield", (yyvsp[-5].dtype).bitfield);
	      }

	      if (Strstr((yyvsp[-6].decl).id,"::")) {
                String *p = Swig_scopename_prefix((yyvsp[-6].decl).id);
		if (p) {
		  if ((Namespaceprefix && Strcmp(p, Namespaceprefix) == 0) ||
		      (Classprefix && Strcmp(p, Classprefix) == 0)) {
		    String *lstr = Swig_scopename_last((yyvsp[-6].decl).id);
		    Setattr((yyval.node),"name",lstr);
		    Delete(lstr);
		    set_nextSibling((yyval.node), (yyvsp[0].node));
		  } else {
		    Delete((yyval.node));
		    (yyval.node) = (yyvsp[0].node);
		  }
		  Delete(p);
		} else {
		  Delete((yyval.node));
		  (yyval.node) = (yyvsp[0].node);
		}
	      } else {
		set_nextSibling((yyval.node), (yyvsp[0].node));
	      }

	      if ((yyvsp[-5].dtype).qualifier && (yyvsp[-8].id) && Strstr((yyvsp[-8].id), "static"))
		Swig_error(cparse_file, cparse_line, "Static function %s cannot have a qualifier.\n", Swig_name_decl((yyval.node)));
           }
#line 6819 "y.tab.c" /* yacc.c:1646  */
    break;

  case 127:
#line 3322 "parser.y" /* yacc.c:1646  */
    { 
                   (yyval.node) = 0;
                   Clear(scanner_ccode); 
               }
#line 6828 "y.tab.c" /* yacc.c:1646  */
    break;

  case 128:
#line 3326 "parser.y" /* yacc.c:1646  */
    {
		 (yyval.node) = new_node("cdecl");
		 if ((yyvsp[-2].dtype).qualifier) SwigType_push((yyvsp[-3].decl).type,(yyvsp[-2].dtype).qualifier);
		 Setattr((yyval.node),"refqualifier",(yyvsp[-2].dtype).refqualifier);
		 Setattr((yyval.node),"name",(yyvsp[-3].decl).id);
		 Setattr((yyval.node),"decl",(yyvsp[-3].decl).type);
		 Setattr((yyval.node),"parms",(yyvsp[-3].decl).parms);
		 Setattr((yyval.node),"value",(yyvsp[-1].dtype).val);
		 Setattr((yyval.node),"throws",(yyvsp[-2].dtype).throws);
		 Setattr((yyval.node),"throw",(yyvsp[-2].dtype).throwf);
		 Setattr((yyval.node),"noexcept",(yyvsp[-2].dtype).nexcept);
		 Setattr((yyval.node),"final",(yyvsp[-2].dtype).final);
		 if ((yyvsp[-1].dtype).bitfield) {
		   Setattr((yyval.node),"bitfield", (yyvsp[-1].dtype).bitfield);
		 }
		 if (!(yyvsp[0].node)) {
		   if (Len(scanner_ccode)) {
		     String *code = Copy(scanner_ccode);
		     Setattr((yyval.node),"code",code);
		     Delete(code);
		   }
		 } else {
		   set_nextSibling((yyval.node), (yyvsp[0].node));
		 }
	       }
#line 6858 "y.tab.c" /* yacc.c:1646  */
    break;

  case 129:
#line 3351 "parser.y" /* yacc.c:1646  */
    { 
                   skip_balanced('{','}');
                   (yyval.node) = 0;
               }
#line 6867 "y.tab.c" /* yacc.c:1646  */
    break;

  case 130:
#line 3355 "parser.y" /* yacc.c:1646  */
    {
		   (yyval.node) = 0;
		   if (yychar == RPAREN) {
		       Swig_error(cparse_file, cparse_line, "Unexpected ')'.\n");
		   } else {
		       Swig_error(cparse_file, cparse_line, "Syntax error - possibly a missing semicolon.\n");
		   }
		   SWIG_exit(EXIT_FAILURE);
               }
#line 6881 "y.tab.c" /* yacc.c:1646  */
    break;

  case 131:
#line 3366 "parser.y" /* yacc.c:1646  */
    {
                   (yyval.dtype) = (yyvsp[0].dtype);
              }
#line 6889 "y.tab.c" /* yacc.c:1646  */
    break;

  case 132:
#line 3371 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = (yyvsp[0].type); }
#line 6895 "y.tab.c" /* yacc.c:1646  */
    break;

  case 133:
#line 3372 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = (yyvsp[0].type); }
#line 6901 "y.tab.c" /* yacc.c:1646  */
    break;

  case 134:
#line 3373 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = (yyvsp[0].type); }
#line 6907 "y.tab.c" /* yacc.c:1646  */
    break;

  case 135:
#line 3377 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = (yyvsp[0].type); }
#line 6913 "y.tab.c" /* yacc.c:1646  */
    break;

  case 136:
#line 3378 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = (yyvsp[0].str); }
#line 6919 "y.tab.c" /* yacc.c:1646  */
    break;

  case 137:
#line 3379 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = (yyvsp[0].type); }
#line 6925 "y.tab.c" /* yacc.c:1646  */
    break;

  case 138:
#line 3390 "parser.y" /* yacc.c:1646  */
    {
		  (yyval.node) = new_node("lambda");
		  Setattr((yyval.node),"name",(yyvsp[-8].str));
		  add_symbols((yyval.node));
	        }
#line 6935 "y.tab.c" /* yacc.c:1646  */
    break;

  case 139:
#line 3395 "parser.y" /* yacc.c:1646  */
    {
		  (yyval.node) = new_node("lambda");
		  Setattr((yyval.node),"name",(yyvsp[-10].str));
		  add_symbols((yyval.node));
		}
#line 6945 "y.tab.c" /* yacc.c:1646  */
    break;

  case 140:
#line 3400 "parser.y" /* yacc.c:1646  */
    {
		  (yyval.node) = new_node("lambda");
		  Setattr((yyval.node),"name",(yyvsp[-4].str));
		  add_symbols((yyval.node));
		}
#line 6955 "y.tab.c" /* yacc.c:1646  */
    break;

  case 141:
#line 3407 "parser.y" /* yacc.c:1646  */
    {
		  skip_balanced('[',']');
		  (yyval.node) = 0;
	        }
#line 6964 "y.tab.c" /* yacc.c:1646  */
    break;

  case 142:
#line 3413 "parser.y" /* yacc.c:1646  */
    {
		  skip_balanced('{','}');
		  (yyval.node) = 0;
		}
#line 6973 "y.tab.c" /* yacc.c:1646  */
    break;

  case 143:
#line 3418 "parser.y" /* yacc.c:1646  */
    {
		  (yyval.pl) = 0;
		}
#line 6981 "y.tab.c" /* yacc.c:1646  */
    break;

  case 144:
#line 3421 "parser.y" /* yacc.c:1646  */
    {
		  skip_balanced('(',')');
		}
#line 6989 "y.tab.c" /* yacc.c:1646  */
    break;

  case 145:
#line 3423 "parser.y" /* yacc.c:1646  */
    {
		  (yyval.pl) = 0;
		}
#line 6997 "y.tab.c" /* yacc.c:1646  */
    break;

  case 146:
#line 3434 "parser.y" /* yacc.c:1646  */
    {
		   (yyval.node) = (char *)"enum";
	      }
#line 7005 "y.tab.c" /* yacc.c:1646  */
    break;

  case 147:
#line 3437 "parser.y" /* yacc.c:1646  */
    {
		   (yyval.node) = (char *)"enum class";
	      }
#line 7013 "y.tab.c" /* yacc.c:1646  */
    break;

  case 148:
#line 3440 "parser.y" /* yacc.c:1646  */
    {
		   (yyval.node) = (char *)"enum struct";
	      }
#line 7021 "y.tab.c" /* yacc.c:1646  */
    break;

  case 149:
#line 3449 "parser.y" /* yacc.c:1646  */
    {
                   (yyval.node) = (yyvsp[0].type);
              }
#line 7029 "y.tab.c" /* yacc.c:1646  */
    break;

  case 150:
#line 3452 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = 0; }
#line 7035 "y.tab.c" /* yacc.c:1646  */
    break;

  case 151:
#line 3459 "parser.y" /* yacc.c:1646  */
    {
		   SwigType *ty = 0;
		   int scopedenum = (yyvsp[-2].id) && !Equal((yyvsp[-3].node), "enum");
		   (yyval.node) = new_node("enumforward");
		   ty = NewStringf("enum %s", (yyvsp[-2].id));
		   Setattr((yyval.node),"enumkey",(yyvsp[-3].node));
		   if (scopedenum)
		     SetFlag((yyval.node), "scopedenum");
		   Setattr((yyval.node),"name",(yyvsp[-2].id));
		   Setattr((yyval.node),"inherit",(yyvsp[-1].node));
		   Setattr((yyval.node),"type",ty);
		   Setattr((yyval.node),"sym:weak", "1");
		   add_symbols((yyval.node));
	      }
#line 7054 "y.tab.c" /* yacc.c:1646  */
    break;

  case 152:
#line 3481 "parser.y" /* yacc.c:1646  */
    {
		  SwigType *ty = 0;
		  int scopedenum = (yyvsp[-5].id) && !Equal((yyvsp[-6].node), "enum");
                  (yyval.node) = new_node("enum");
		  ty = NewStringf("enum %s", (yyvsp[-5].id));
		  Setattr((yyval.node),"enumkey",(yyvsp[-6].node));
		  if (scopedenum)
		    SetFlag((yyval.node), "scopedenum");
		  Setattr((yyval.node),"name",(yyvsp[-5].id));
		  Setattr((yyval.node),"inherit",(yyvsp[-4].node));
		  Setattr((yyval.node),"type",ty);
		  appendChild((yyval.node),(yyvsp[-2].node));
		  add_symbols((yyval.node));      /* Add to tag space */

		  if (scopedenum) {
		    Swig_symbol_newscope();
		    Swig_symbol_setscopename((yyvsp[-5].id));
		    Delete(Namespaceprefix);
		    Namespaceprefix = Swig_symbol_qualifiedscopename(0);
		  }

		  add_symbols((yyvsp[-2].node));      /* Add enum values to appropriate enum or enum class scope */

		  if (scopedenum) {
		    Setattr((yyval.node),"symtab", Swig_symbol_popscope());
		    Delete(Namespaceprefix);
		    Namespaceprefix = Swig_symbol_qualifiedscopename(0);
		  }
               }
#line 7088 "y.tab.c" /* yacc.c:1646  */
    break;

  case 153:
#line 3510 "parser.y" /* yacc.c:1646  */
    {
		 Node *n;
		 SwigType *ty = 0;
		 String   *unnamed = 0;
		 int       unnamedinstance = 0;
		 int scopedenum = (yyvsp[-8].id) && !Equal((yyvsp[-9].node), "enum");

		 (yyval.node) = new_node("enum");
		 Setattr((yyval.node),"enumkey",(yyvsp[-9].node));
		 if (scopedenum)
		   SetFlag((yyval.node), "scopedenum");
		 Setattr((yyval.node),"inherit",(yyvsp[-7].node));
		 if ((yyvsp[-8].id)) {
		   Setattr((yyval.node),"name",(yyvsp[-8].id));
		   ty = NewStringf("enum %s", (yyvsp[-8].id));
		 } else if ((yyvsp[-3].decl).id) {
		   unnamed = make_unnamed();
		   ty = NewStringf("enum %s", unnamed);
		   Setattr((yyval.node),"unnamed",unnamed);
                   /* name is not set for unnamed enum instances, e.g. enum { foo } Instance; */
		   if ((yyvsp[-10].id) && Cmp((yyvsp[-10].id),"typedef") == 0) {
		     Setattr((yyval.node),"name",(yyvsp[-3].decl).id);
                   } else {
                     unnamedinstance = 1;
                   }
		   Setattr((yyval.node),"storage",(yyvsp[-10].id));
		 }
		 if ((yyvsp[-3].decl).id && Cmp((yyvsp[-10].id),"typedef") == 0) {
		   Setattr((yyval.node),"tdname",(yyvsp[-3].decl).id);
                   Setattr((yyval.node),"allows_typedef","1");
                 }
		 appendChild((yyval.node),(yyvsp[-5].node));
		 n = new_node("cdecl");
		 Setattr(n,"type",ty);
		 Setattr(n,"name",(yyvsp[-3].decl).id);
		 Setattr(n,"storage",(yyvsp[-10].id));
		 Setattr(n,"decl",(yyvsp[-3].decl).type);
		 Setattr(n,"parms",(yyvsp[-3].decl).parms);
		 Setattr(n,"unnamed",unnamed);

                 if (unnamedinstance) {
		   SwigType *cty = NewString("enum ");
		   Setattr((yyval.node),"type",cty);
		   SetFlag((yyval.node),"unnamedinstance");
		   SetFlag(n,"unnamedinstance");
		   Delete(cty);
                 }
		 if ((yyvsp[0].node)) {
		   Node *p = (yyvsp[0].node);
		   set_nextSibling(n,p);
		   while (p) {
		     SwigType *cty = Copy(ty);
		     Setattr(p,"type",cty);
		     Setattr(p,"unnamed",unnamed);
		     Setattr(p,"storage",(yyvsp[-10].id));
		     Delete(cty);
		     p = nextSibling(p);
		   }
		 } else {
		   if (Len(scanner_ccode)) {
		     String *code = Copy(scanner_ccode);
		     Setattr(n,"code",code);
		     Delete(code);
		   }
		 }

                 /* Ensure that typedef enum ABC {foo} XYZ; uses XYZ for sym:name, like structs.
                  * Note that class_rename/yyrename are bit of a mess so used this simple approach to change the name. */
                 if ((yyvsp[-3].decl).id && (yyvsp[-8].id) && Cmp((yyvsp[-10].id),"typedef") == 0) {
		   String *name = NewString((yyvsp[-3].decl).id);
                   Setattr((yyval.node), "parser:makename", name);
		   Delete(name);
                 }

		 add_symbols((yyval.node));       /* Add enum to tag space */
		 set_nextSibling((yyval.node),n);
		 Delete(n);

		 if (scopedenum) {
		   Swig_symbol_newscope();
		   Swig_symbol_setscopename((yyvsp[-8].id));
		   Delete(Namespaceprefix);
		   Namespaceprefix = Swig_symbol_qualifiedscopename(0);
		 }

		 add_symbols((yyvsp[-5].node));      /* Add enum values to appropriate enum or enum class scope */

		 if (scopedenum) {
		   Setattr((yyval.node),"symtab", Swig_symbol_popscope());
		   Delete(Namespaceprefix);
		   Namespaceprefix = Swig_symbol_qualifiedscopename(0);
		 }

	         add_symbols(n);
		 Delete(unnamed);
	       }
#line 7189 "y.tab.c" /* yacc.c:1646  */
    break;

  case 154:
#line 3608 "parser.y" /* yacc.c:1646  */
    {
                   /* This is a sick hack.  If the ctor_end has parameters,
                      and the parms parameter only has 1 parameter, this
                      could be a declaration of the form:

                         type (id)(parms)

			 Otherwise it's an error. */
                    int err = 0;
                    (yyval.node) = 0;

		    if ((ParmList_len((yyvsp[-2].pl)) == 1) && (!Swig_scopename_check((yyvsp[-4].type)))) {
		      SwigType *ty = Getattr((yyvsp[-2].pl),"type");
		      String *name = Getattr((yyvsp[-2].pl),"name");
		      err = 1;
		      if (!name) {
			(yyval.node) = new_node("cdecl");
			Setattr((yyval.node),"type",(yyvsp[-4].type));
			Setattr((yyval.node),"storage",(yyvsp[-5].id));
			Setattr((yyval.node),"name",ty);

			if ((yyvsp[0].decl).have_parms) {
			  SwigType *decl = NewStringEmpty();
			  SwigType_add_function(decl,(yyvsp[0].decl).parms);
			  Setattr((yyval.node),"decl",decl);
			  Setattr((yyval.node),"parms",(yyvsp[0].decl).parms);
			  if (Len(scanner_ccode)) {
			    String *code = Copy(scanner_ccode);
			    Setattr((yyval.node),"code",code);
			    Delete(code);
			  }
			}
			if ((yyvsp[0].decl).defarg) {
			  Setattr((yyval.node),"value",(yyvsp[0].decl).defarg);
			}
			Setattr((yyval.node),"throws",(yyvsp[0].decl).throws);
			Setattr((yyval.node),"throw",(yyvsp[0].decl).throwf);
			Setattr((yyval.node),"noexcept",(yyvsp[0].decl).nexcept);
			Setattr((yyval.node),"final",(yyvsp[0].decl).final);
			err = 0;
		      }
		    }
		    if (err) {
		      Swig_error(cparse_file,cparse_line,"Syntax error in input(2).\n");
		      SWIG_exit(EXIT_FAILURE);
		    }
                }
#line 7241 "y.tab.c" /* yacc.c:1646  */
    break;

  case 155:
#line 3661 "parser.y" /* yacc.c:1646  */
    {  (yyval.node) = (yyvsp[0].node); }
#line 7247 "y.tab.c" /* yacc.c:1646  */
    break;

  case 156:
#line 3662 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = (yyvsp[0].node); }
#line 7253 "y.tab.c" /* yacc.c:1646  */
    break;

  case 157:
#line 3663 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = (yyvsp[0].node); }
#line 7259 "y.tab.c" /* yacc.c:1646  */
    break;

  case 158:
#line 3664 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = (yyvsp[0].node); }
#line 7265 "y.tab.c" /* yacc.c:1646  */
    break;

  case 159:
#line 3665 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = (yyvsp[0].node); }
#line 7271 "y.tab.c" /* yacc.c:1646  */
    break;

  case 160:
#line 3666 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = 0; }
#line 7277 "y.tab.c" /* yacc.c:1646  */
    break;

  case 161:
#line 3671 "parser.y" /* yacc.c:1646  */
    {
                   String *prefix;
                   List *bases = 0;
		   Node *scope = 0;
		   String *code;
		   (yyval.node) = new_node("class");
		   Setline((yyval.node),cparse_start_line);
		   Setattr((yyval.node),"kind",(yyvsp[-3].id));
		   if ((yyvsp[-1].bases)) {
		     Setattr((yyval.node),"baselist", Getattr((yyvsp[-1].bases),"public"));
		     Setattr((yyval.node),"protectedbaselist", Getattr((yyvsp[-1].bases),"protected"));
		     Setattr((yyval.node),"privatebaselist", Getattr((yyvsp[-1].bases),"private"));
		   }
		   Setattr((yyval.node),"allows_typedef","1");

		   /* preserve the current scope */
		   Setattr((yyval.node),"prev_symtab",Swig_symbol_current());
		  
		   /* If the class name is qualified.  We need to create or lookup namespace/scope entries */
		   scope = resolve_create_node_scope((yyvsp[-2].str), 1);
		   /* save nscope_inner to the class - it may be overwritten in nested classes*/
		   Setattr((yyval.node), "nested:innerscope", nscope_inner);
		   Setattr((yyval.node), "nested:nscope", nscope);
		   Setfile(scope,cparse_file);
		   Setline(scope,cparse_line);
		   (yyvsp[-2].str) = scope;
		   Setattr((yyval.node),"name",(yyvsp[-2].str));

		   if (currentOuterClass) {
		     SetFlag((yyval.node), "nested");
		     Setattr((yyval.node), "nested:outer", currentOuterClass);
		     set_access_mode((yyval.node));
		   }
		   Swig_features_get(Swig_cparse_features(), Namespaceprefix, Getattr((yyval.node), "name"), 0, (yyval.node));
		   /* save yyrename to the class attribute, to be used later in add_symbols()*/
		   Setattr((yyval.node), "class_rename", make_name((yyval.node), (yyvsp[-2].str), 0));
		   Setattr((yyval.node), "Classprefix", (yyvsp[-2].str));
		   Classprefix = NewString((yyvsp[-2].str));
		   /* Deal with inheritance  */
		   if ((yyvsp[-1].bases))
		     bases = Swig_make_inherit_list((yyvsp[-2].str),Getattr((yyvsp[-1].bases),"public"),Namespaceprefix);
		   prefix = SwigType_istemplate_templateprefix((yyvsp[-2].str));
		   if (prefix) {
		     String *fbase, *tbase;
		     if (Namespaceprefix) {
		       fbase = NewStringf("%s::%s", Namespaceprefix,(yyvsp[-2].str));
		       tbase = NewStringf("%s::%s", Namespaceprefix, prefix);
		     } else {
		       fbase = Copy((yyvsp[-2].str));
		       tbase = Copy(prefix);
		     }
		     Swig_name_inherit(tbase,fbase);
		     Delete(fbase);
		     Delete(tbase);
		   }
                   if (strcmp((yyvsp[-3].id),"class") == 0) {
		     cplus_mode = CPLUS_PRIVATE;
		   } else {
		     cplus_mode = CPLUS_PUBLIC;
		   }
		   if (!cparse_cplusplus) {
		     set_scope_to_global();
		   }
		   Swig_symbol_newscope();
		   Swig_symbol_setscopename((yyvsp[-2].str));
		   Swig_inherit_base_symbols(bases);
		   Delete(Namespaceprefix);
		   Namespaceprefix = Swig_symbol_qualifiedscopename(0);
		   cparse_start_line = cparse_line;

		   /* If there are active template parameters, we need to make sure they are
                      placed in the class symbol table so we can catch shadows */

		   if (template_parameters) {
		     Parm *tp = template_parameters;
		     while(tp) {
		       String *tpname = Copy(Getattr(tp,"name"));
		       Node *tn = new_node("templateparm");
		       Setattr(tn,"name",tpname);
		       Swig_symbol_cadd(tpname,tn);
		       tp = nextSibling(tp);
		       Delete(tpname);
		     }
		   }
		   Delete(prefix);
		   inclass = 1;
		   currentOuterClass = (yyval.node);
		   if (cparse_cplusplusout) {
		     /* save the structure declaration to declare it in global scope for C++ to see */
		     code = get_raw_text_balanced('{', '}');
		     Setattr((yyval.node), "code", code);
		     Delete(code);
		   }
               }
#line 7376 "y.tab.c" /* yacc.c:1646  */
    break;

  case 162:
#line 3764 "parser.y" /* yacc.c:1646  */
    {
		   Node *p;
		   SwigType *ty;
		   Symtab *cscope;
		   Node *am = 0;
		   String *scpname = 0;
		   (void) (yyvsp[-3].node);
		   (yyval.node) = currentOuterClass;
		   currentOuterClass = Getattr((yyval.node), "nested:outer");
		   nscope_inner = Getattr((yyval.node), "nested:innerscope");
		   nscope = Getattr((yyval.node), "nested:nscope");
		   Delattr((yyval.node), "nested:innerscope");
		   Delattr((yyval.node), "nested:nscope");
		   if (nscope_inner && Strcmp(nodeType(nscope_inner), "class") == 0) { /* actual parent class for this class */
		     Node* forward_declaration = Swig_symbol_clookup_no_inherit(Getattr((yyval.node),"name"), Getattr(nscope_inner, "symtab"));
		     if (forward_declaration) {
		       Setattr((yyval.node), "access", Getattr(forward_declaration, "access"));
		     }
		     Setattr((yyval.node), "nested:outer", nscope_inner);
		     SetFlag((yyval.node), "nested");
                   }
		   if (!currentOuterClass)
		     inclass = 0;
		   cscope = Getattr((yyval.node), "prev_symtab");
		   Delattr((yyval.node), "prev_symtab");
		   
		   /* Check for pure-abstract class */
		   Setattr((yyval.node),"abstracts", pure_abstracts((yyvsp[-2].node)));
		   
		   /* This bit of code merges in a previously defined %extend directive (if any) */
		   {
		     String *clsname = Swig_symbol_qualifiedscopename(0);
		     am = Getattr(Swig_extend_hash(), clsname);
		     if (am) {
		       Swig_extend_merge((yyval.node), am);
		       Delattr(Swig_extend_hash(), clsname);
		     }
		     Delete(clsname);
		   }
		   if (!classes) classes = NewHash();
		   scpname = Swig_symbol_qualifiedscopename(0);
		   Setattr(classes, scpname, (yyval.node));

		   appendChild((yyval.node), (yyvsp[-2].node));
		   
		   if (am) 
		     Swig_extend_append_previous((yyval.node), am);

		   p = (yyvsp[0].node);
		   if (p && !nscope_inner) {
		     if (!cparse_cplusplus && currentOuterClass)
		       appendChild(currentOuterClass, p);
		     else
		      appendSibling((yyval.node), p);
		   }
		   
		   if (nscope_inner) {
		     ty = NewString(scpname); /* if the class is declared out of scope, let the declarator use fully qualified type*/
		   } else if (cparse_cplusplus && !cparse_externc) {
		     ty = NewString((yyvsp[-6].str));
		   } else {
		     ty = NewStringf("%s %s", (yyvsp[-7].id), (yyvsp[-6].str));
		   }
		   while (p) {
		     Setattr(p, "storage", (yyvsp[-8].id));
		     Setattr(p, "type" ,ty);
		     if (!cparse_cplusplus && currentOuterClass && (!Getattr(currentOuterClass, "name"))) {
		       SetFlag(p, "hasconsttype");
		       SetFlag(p, "feature:immutable");
		     }
		     p = nextSibling(p);
		   }
		   if ((yyvsp[0].node) && Cmp((yyvsp[-8].id),"typedef") == 0)
		     add_typedef_name((yyval.node), (yyvsp[0].node), (yyvsp[-6].str), cscope, scpname);
		   Delete(scpname);

		   if (cplus_mode != CPLUS_PUBLIC) {
		   /* we 'open' the class at the end, to allow %template
		      to add new members */
		     Node *pa = new_node("access");
		     Setattr(pa, "kind", "public");
		     cplus_mode = CPLUS_PUBLIC;
		     appendChild((yyval.node), pa);
		     Delete(pa);
		   }
		   if (currentOuterClass)
		     restore_access_mode((yyval.node));
		   Setattr((yyval.node), "symtab", Swig_symbol_popscope());
		   Classprefix = Getattr((yyval.node), "Classprefix");
		   Delattr((yyval.node), "Classprefix");
		   Delete(Namespaceprefix);
		   Namespaceprefix = Swig_symbol_qualifiedscopename(0);
		   if (cplus_mode == CPLUS_PRIVATE) {
		     (yyval.node) = 0; /* skip private nested classes */
		   } else if (cparse_cplusplus && currentOuterClass && ignore_nested_classes && !GetFlag((yyval.node), "feature:flatnested")) {
		     (yyval.node) = nested_forward_declaration((yyvsp[-8].id), (yyvsp[-7].id), (yyvsp[-6].str), Copy((yyvsp[-6].str)), (yyvsp[0].node));
		   } else if (nscope_inner) {
		     /* this is tricky */
		     /* we add the declaration in the original namespace */
		     if (Strcmp(nodeType(nscope_inner), "class") == 0 && cparse_cplusplus && ignore_nested_classes && !GetFlag((yyval.node), "feature:flatnested"))
		       (yyval.node) = nested_forward_declaration((yyvsp[-8].id), (yyvsp[-7].id), (yyvsp[-6].str), Copy((yyvsp[-6].str)), (yyvsp[0].node));
		     appendChild(nscope_inner, (yyval.node));
		     Swig_symbol_setscope(Getattr(nscope_inner, "symtab"));
		     Delete(Namespaceprefix);
		     Namespaceprefix = Swig_symbol_qualifiedscopename(0);
		     yyrename = Copy(Getattr((yyval.node), "class_rename"));
		     add_symbols((yyval.node));
		     Delattr((yyval.node), "class_rename");
		     /* but the variable definition in the current scope */
		     Swig_symbol_setscope(cscope);
		     Delete(Namespaceprefix);
		     Namespaceprefix = Swig_symbol_qualifiedscopename(0);
		     add_symbols((yyvsp[0].node));
		     if (nscope) {
		       (yyval.node) = nscope; /* here we return recreated namespace tower instead of the class itself */
		       if ((yyvsp[0].node)) {
			 appendSibling((yyval.node), (yyvsp[0].node));
		       }
		     } else if (!SwigType_istemplate(ty) && template_parameters == 0) { /* for template we need the class itself */
		       (yyval.node) = (yyvsp[0].node);
		     }
		   } else {
		     Delete(yyrename);
		     yyrename = 0;
		     if (!cparse_cplusplus && currentOuterClass) { /* nested C structs go into global scope*/
		       Node *outer = currentOuterClass;
		       while (Getattr(outer, "nested:outer"))
			 outer = Getattr(outer, "nested:outer");
		       appendSibling(outer, (yyval.node));
		       Swig_symbol_setscope(cscope); /* declaration goes in the parent scope */
		       add_symbols((yyvsp[0].node));
		       set_scope_to_global();
		       Delete(Namespaceprefix);
		       Namespaceprefix = Swig_symbol_qualifiedscopename(0);
		       yyrename = Copy(Getattr((yyval.node), "class_rename"));
		       add_symbols((yyval.node));
		       if (!cparse_cplusplusout)
			 Delattr((yyval.node), "nested:outer");
		       Delattr((yyval.node), "class_rename");
		       (yyval.node) = 0;
		     } else {
		       yyrename = Copy(Getattr((yyval.node), "class_rename"));
		       add_symbols((yyval.node));
		       add_symbols((yyvsp[0].node));
		       Delattr((yyval.node), "class_rename");
		     }
		   }
		   Delete(ty);
		   Swig_symbol_setscope(cscope);
		   Delete(Namespaceprefix);
		   Namespaceprefix = Swig_symbol_qualifiedscopename(0);
		   Classprefix = currentOuterClass ? Getattr(currentOuterClass, "Classprefix") : 0;
	       }
#line 7534 "y.tab.c" /* yacc.c:1646  */
    break;

  case 163:
#line 3920 "parser.y" /* yacc.c:1646  */
    {
	       String *unnamed;
	       String *code;
	       unnamed = make_unnamed();
	       (yyval.node) = new_node("class");
	       Setline((yyval.node),cparse_start_line);
	       Setattr((yyval.node),"kind",(yyvsp[-2].id));
	       if ((yyvsp[-1].bases)) {
		 Setattr((yyval.node),"baselist", Getattr((yyvsp[-1].bases),"public"));
		 Setattr((yyval.node),"protectedbaselist", Getattr((yyvsp[-1].bases),"protected"));
		 Setattr((yyval.node),"privatebaselist", Getattr((yyvsp[-1].bases),"private"));
	       }
	       Setattr((yyval.node),"storage",(yyvsp[-3].id));
	       Setattr((yyval.node),"unnamed",unnamed);
	       Setattr((yyval.node),"allows_typedef","1");
	       if (currentOuterClass) {
		 SetFlag((yyval.node), "nested");
		 Setattr((yyval.node), "nested:outer", currentOuterClass);
		 set_access_mode((yyval.node));
	       }
	       Swig_features_get(Swig_cparse_features(), Namespaceprefix, 0, 0, (yyval.node));
	       /* save yyrename to the class attribute, to be used later in add_symbols()*/
	       Setattr((yyval.node), "class_rename", make_name((yyval.node),0,0));
	       if (strcmp((yyvsp[-2].id),"class") == 0) {
		 cplus_mode = CPLUS_PRIVATE;
	       } else {
		 cplus_mode = CPLUS_PUBLIC;
	       }
	       Swig_symbol_newscope();
	       cparse_start_line = cparse_line;
	       currentOuterClass = (yyval.node);
	       inclass = 1;
	       Classprefix = 0;
	       Delete(Namespaceprefix);
	       Namespaceprefix = Swig_symbol_qualifiedscopename(0);
	       /* save the structure declaration to make a typedef for it later*/
	       code = get_raw_text_balanced('{', '}');
	       Setattr((yyval.node), "code", code);
	       Delete(code);
	     }
#line 7579 "y.tab.c" /* yacc.c:1646  */
    break;

  case 164:
#line 3959 "parser.y" /* yacc.c:1646  */
    {
	       String *unnamed;
               List *bases = 0;
	       String *name = 0;
	       Node *n;
	       Classprefix = 0;
	       (void)(yyvsp[-3].node);
	       (yyval.node) = currentOuterClass;
	       currentOuterClass = Getattr((yyval.node), "nested:outer");
	       if (!currentOuterClass)
		 inclass = 0;
	       else
		 restore_access_mode((yyval.node));
	       unnamed = Getattr((yyval.node),"unnamed");
               /* Check for pure-abstract class */
	       Setattr((yyval.node),"abstracts", pure_abstracts((yyvsp[-2].node)));
	       n = (yyvsp[0].node);
	       if (cparse_cplusplus && currentOuterClass && ignore_nested_classes && !GetFlag((yyval.node), "feature:flatnested")) {
		 String *name = n ? Copy(Getattr(n, "name")) : 0;
		 (yyval.node) = nested_forward_declaration((yyvsp[-7].id), (yyvsp[-6].id), 0, name, n);
		 Swig_symbol_popscope();
	         Delete(Namespaceprefix);
		 Namespaceprefix = Swig_symbol_qualifiedscopename(0);
	       } else if (n) {
	         appendSibling((yyval.node),n);
		 /* If a proper typedef name was given, we'll use it to set the scope name */
		 name = try_to_find_a_name_for_unnamed_structure((yyvsp[-7].id), n);
		 if (name) {
		   String *scpname = 0;
		   SwigType *ty;
		   Setattr((yyval.node),"tdname",name);
		   Setattr((yyval.node),"name",name);
		   Swig_symbol_setscopename(name);
		   if ((yyvsp[-5].bases))
		     bases = Swig_make_inherit_list(name,Getattr((yyvsp[-5].bases),"public"),Namespaceprefix);
		   Swig_inherit_base_symbols(bases);

		     /* If a proper name was given, we use that as the typedef, not unnamed */
		   Clear(unnamed);
		   Append(unnamed, name);
		   if (cparse_cplusplus && !cparse_externc) {
		     ty = NewString(name);
		   } else {
		     ty = NewStringf("%s %s", (yyvsp[-6].id),name);
		   }
		   while (n) {
		     Setattr(n,"storage",(yyvsp[-7].id));
		     Setattr(n, "type", ty);
		     if (!cparse_cplusplus && currentOuterClass && (!Getattr(currentOuterClass, "name"))) {
		       SetFlag(n,"hasconsttype");
		       SetFlag(n,"feature:immutable");
		     }
		     n = nextSibling(n);
		   }
		   n = (yyvsp[0].node);

		   /* Check for previous extensions */
		   {
		     String *clsname = Swig_symbol_qualifiedscopename(0);
		     Node *am = Getattr(Swig_extend_hash(),clsname);
		     if (am) {
		       /* Merge the extension into the symbol table */
		       Swig_extend_merge((yyval.node),am);
		       Swig_extend_append_previous((yyval.node),am);
		       Delattr(Swig_extend_hash(),clsname);
		     }
		     Delete(clsname);
		   }
		   if (!classes) classes = NewHash();
		   scpname = Swig_symbol_qualifiedscopename(0);
		   Setattr(classes,scpname,(yyval.node));
		   Delete(scpname);
		 } else { /* no suitable name was found for a struct */
		   Setattr((yyval.node), "nested:unnamed", Getattr(n, "name")); /* save the name of the first declarator for later use in name generation*/
		   while (n) { /* attach unnamed struct to the declarators, so that they would receive proper type later*/
		     Setattr(n, "nested:unnamedtype", (yyval.node));
		     Setattr(n, "storage", (yyvsp[-7].id));
		     n = nextSibling(n);
		   }
		   n = (yyvsp[0].node);
		   Swig_symbol_setscopename("<unnamed>");
		 }
		 appendChild((yyval.node),(yyvsp[-2].node));
		 /* Pop the scope */
		 Setattr((yyval.node),"symtab",Swig_symbol_popscope());
		 if (name) {
		   Delete(yyrename);
		   yyrename = Copy(Getattr((yyval.node), "class_rename"));
		   Delete(Namespaceprefix);
		   Namespaceprefix = Swig_symbol_qualifiedscopename(0);
		   add_symbols((yyval.node));
		   add_symbols(n);
		   Delattr((yyval.node), "class_rename");
		 }else if (cparse_cplusplus)
		   (yyval.node) = 0; /* ignore unnamed structs for C++ */
	         Delete(unnamed);
	       } else { /* unnamed struct w/o declarator*/
		 Swig_symbol_popscope();
	         Delete(Namespaceprefix);
		 Namespaceprefix = Swig_symbol_qualifiedscopename(0);
		 add_symbols((yyvsp[-2].node));
		 Delete((yyval.node));
		 (yyval.node) = (yyvsp[-2].node); /* pass member list to outer class/namespace (instead of self)*/
	       }
	       Classprefix = currentOuterClass ? Getattr(currentOuterClass, "Classprefix") : 0;
              }
#line 7690 "y.tab.c" /* yacc.c:1646  */
    break;

  case 165:
#line 4067 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = 0; }
#line 7696 "y.tab.c" /* yacc.c:1646  */
    break;

  case 166:
#line 4068 "parser.y" /* yacc.c:1646  */
    {
                        (yyval.node) = new_node("cdecl");
                        Setattr((yyval.node),"name",(yyvsp[-3].decl).id);
                        Setattr((yyval.node),"decl",(yyvsp[-3].decl).type);
                        Setattr((yyval.node),"parms",(yyvsp[-3].decl).parms);
			set_nextSibling((yyval.node), (yyvsp[0].node));
                    }
#line 7708 "y.tab.c" /* yacc.c:1646  */
    break;

  case 167:
#line 4080 "parser.y" /* yacc.c:1646  */
    {
              if ((yyvsp[-3].id) && (Strcmp((yyvsp[-3].id),"friend") == 0)) {
		/* Ignore */
                (yyval.node) = 0; 
	      } else {
		(yyval.node) = new_node("classforward");
		Setattr((yyval.node),"kind",(yyvsp[-2].id));
		Setattr((yyval.node),"name",(yyvsp[-1].str));
		Setattr((yyval.node),"sym:weak", "1");
		add_symbols((yyval.node));
	      }
             }
#line 7725 "y.tab.c" /* yacc.c:1646  */
    break;

  case 168:
#line 4098 "parser.y" /* yacc.c:1646  */
    { 
		    if (currentOuterClass)
		      Setattr(currentOuterClass, "template_parameters", template_parameters);
		    template_parameters = (yyvsp[-1].tparms); 
		    parsing_template_declaration = 1;
		  }
#line 7736 "y.tab.c" /* yacc.c:1646  */
    break;

  case 169:
#line 4103 "parser.y" /* yacc.c:1646  */
    {
			String *tname = 0;
			int     error = 0;

			/* check if we get a namespace node with a class declaration, and retrieve the class */
			Symtab *cscope = Swig_symbol_current();
			Symtab *sti = 0;
			Node *ntop = (yyvsp[0].node);
			Node *ni = ntop;
			SwigType *ntype = ni ? nodeType(ni) : 0;
			while (ni && Strcmp(ntype,"namespace") == 0) {
			  sti = Getattr(ni,"symtab");
			  ni = firstChild(ni);
			  ntype = nodeType(ni);
			}
			if (sti) {
			  Swig_symbol_setscope(sti);
			  Delete(Namespaceprefix);
			  Namespaceprefix = Swig_symbol_qualifiedscopename(0);
			  (yyvsp[0].node) = ni;
			}

			(yyval.node) = (yyvsp[0].node);
			if ((yyval.node)) tname = Getattr((yyval.node),"name");
			
			/* Check if the class is a template specialization */
			if (((yyval.node)) && (Strchr(tname,'<')) && (!is_operator(tname))) {
			  /* If a specialization.  Check if defined. */
			  Node *tempn = 0;
			  {
			    String *tbase = SwigType_templateprefix(tname);
			    tempn = Swig_symbol_clookup_local(tbase,0);
			    if (!tempn || (Strcmp(nodeType(tempn),"template") != 0)) {
			      SWIG_WARN_NODE_BEGIN(tempn);
			      Swig_warning(WARN_PARSE_TEMPLATE_SP_UNDEF, Getfile((yyval.node)),Getline((yyval.node)),"Specialization of non-template '%s'.\n", tbase);
			      SWIG_WARN_NODE_END(tempn);
			      tempn = 0;
			      error = 1;
			    }
			    Delete(tbase);
			  }
			  Setattr((yyval.node),"specialization","1");
			  Setattr((yyval.node),"templatetype",nodeType((yyval.node)));
			  set_nodeType((yyval.node),"template");
			  /* Template partial specialization */
			  if (tempn && ((yyvsp[-3].tparms)) && ((yyvsp[0].node))) {
			    List   *tlist;
			    String *targs = SwigType_templateargs(tname);
			    tlist = SwigType_parmlist(targs);
			    /*			  Printf(stdout,"targs = '%s' %s\n", targs, tlist); */
			    if (!Getattr((yyval.node),"sym:weak")) {
			      Setattr((yyval.node),"sym:typename","1");
			    }
			    
			    if (Len(tlist) != ParmList_len(Getattr(tempn,"templateparms"))) {
			      Swig_error(Getfile((yyval.node)),Getline((yyval.node)),"Inconsistent argument count in template partial specialization. %d %d\n", Len(tlist), ParmList_len(Getattr(tempn,"templateparms")));
			      
			    } else {

			    /* This code builds the argument list for the partial template
			       specialization.  This is a little hairy, but the idea is as
			       follows:

			       $3 contains a list of arguments supplied for the template.
			       For example template<class T>.

			       tlist is a list of the specialization arguments--which may be
			       different.  For example class<int,T>.

			       tp is a copy of the arguments in the original template definition.
       
			       The patching algorithm walks through the list of supplied
			       arguments ($3), finds the position in the specialization arguments
			       (tlist), and then patches the name in the argument list of the
			       original template.
			    */

			    {
			      String *pn;
			      Parm *p, *p1;
			      int i, nargs;
			      Parm *tp = CopyParmList(Getattr(tempn,"templateparms"));
			      nargs = Len(tlist);
			      p = (yyvsp[-3].tparms);
			      while (p) {
				for (i = 0; i < nargs; i++){
				  pn = Getattr(p,"name");
				  if (Strcmp(pn,SwigType_base(Getitem(tlist,i))) == 0) {
				    int j;
				    Parm *p1 = tp;
				    for (j = 0; j < i; j++) {
				      p1 = nextSibling(p1);
				    }
				    Setattr(p1,"name",pn);
				    Setattr(p1,"partialarg","1");
				  }
				}
				p = nextSibling(p);
			      }
			      p1 = tp;
			      i = 0;
			      while (p1) {
				if (!Getattr(p1,"partialarg")) {
				  Delattr(p1,"name");
				  Setattr(p1,"type", Getitem(tlist,i));
				} 
				i++;
				p1 = nextSibling(p1);
			      }
			      Setattr((yyval.node),"templateparms",tp);
			      Delete(tp);
			    }
  #if 0
			    /* Patch the parameter list */
			    if (tempn) {
			      Parm *p,*p1;
			      ParmList *tp = CopyParmList(Getattr(tempn,"templateparms"));
			      p = (yyvsp[-3].tparms);
			      p1 = tp;
			      while (p && p1) {
				String *pn = Getattr(p,"name");
				Printf(stdout,"pn = '%s'\n", pn);
				if (pn) Setattr(p1,"name",pn);
				else Delattr(p1,"name");
				pn = Getattr(p,"type");
				if (pn) Setattr(p1,"type",pn);
				p = nextSibling(p);
				p1 = nextSibling(p1);
			      }
			      Setattr((yyval.node),"templateparms",tp);
			      Delete(tp);
			    } else {
			      Setattr((yyval.node),"templateparms",(yyvsp[-3].tparms));
			    }
  #endif
			    Delattr((yyval.node),"specialization");
			    Setattr((yyval.node),"partialspecialization","1");
			    /* Create a specialized name for matching */
			    {
			      Parm *p = (yyvsp[-3].tparms);
			      String *fname = NewString(Getattr((yyval.node),"name"));
			      String *ffname = 0;
			      ParmList *partialparms = 0;

			      char   tmp[32];
			      int    i, ilen;
			      while (p) {
				String *n = Getattr(p,"name");
				if (!n) {
				  p = nextSibling(p);
				  continue;
				}
				ilen = Len(tlist);
				for (i = 0; i < ilen; i++) {
				  if (Strstr(Getitem(tlist,i),n)) {
				    sprintf(tmp,"$%d",i+1);
				    Replaceid(fname,n,tmp);
				  }
				}
				p = nextSibling(p);
			      }
			      /* Patch argument names with typedef */
			      {
				Iterator tt;
				Parm *parm_current = 0;
				List *tparms = SwigType_parmlist(fname);
				ffname = SwigType_templateprefix(fname);
				Append(ffname,"<(");
				for (tt = First(tparms); tt.item; ) {
				  SwigType *rtt = Swig_symbol_typedef_reduce(tt.item,0);
				  SwigType *ttr = Swig_symbol_type_qualify(rtt,0);

				  Parm *newp = NewParmWithoutFileLineInfo(ttr, 0);
				  if (partialparms)
				    set_nextSibling(parm_current, newp);
				  else
				    partialparms = newp;
				  parm_current = newp;

				  Append(ffname,ttr);
				  tt = Next(tt);
				  if (tt.item) Putc(',',ffname);
				  Delete(rtt);
				  Delete(ttr);
				}
				Delete(tparms);
				Append(ffname,")>");
			      }
			      {
				Node *new_partial = NewHash();
				String *partials = Getattr(tempn,"partials");
				if (!partials) {
				  partials = NewList();
				  Setattr(tempn,"partials",partials);
				  Delete(partials);
				}
				/*			      Printf(stdout,"partial: fname = '%s', '%s'\n", fname, Swig_symbol_typedef_reduce(fname,0)); */
				Setattr(new_partial, "partialparms", partialparms);
				Setattr(new_partial, "templcsymname", ffname);
				Append(partials, new_partial);
			      }
			      Setattr((yyval.node),"partialargs",ffname);
			      Swig_symbol_cadd(ffname,(yyval.node));
			    }
			    }
			    Delete(tlist);
			    Delete(targs);
			  } else {
			    /* An explicit template specialization */
			    /* add default args from primary (unspecialized) template */
			    String *ty = Swig_symbol_template_deftype(tname,0);
			    String *fname = Swig_symbol_type_qualify(ty,0);
			    Swig_symbol_cadd(fname,(yyval.node));
			    Delete(ty);
			    Delete(fname);
			  }
			}  else if ((yyval.node)) {
			  Setattr((yyval.node),"templatetype",nodeType((yyvsp[0].node)));
			  set_nodeType((yyval.node),"template");
			  Setattr((yyval.node),"templateparms", (yyvsp[-3].tparms));
			  if (!Getattr((yyval.node),"sym:weak")) {
			    Setattr((yyval.node),"sym:typename","1");
			  }
			  add_symbols((yyval.node));
			  default_arguments((yyval.node));
			  /* We also place a fully parameterized version in the symbol table */
			  {
			    Parm *p;
			    String *fname = NewStringf("%s<(", Getattr((yyval.node),"name"));
			    p = (yyvsp[-3].tparms);
			    while (p) {
			      String *n = Getattr(p,"name");
			      if (!n) n = Getattr(p,"type");
			      Append(fname,n);
			      p = nextSibling(p);
			      if (p) Putc(',',fname);
			    }
			    Append(fname,")>");
			    Swig_symbol_cadd(fname,(yyval.node));
			  }
			}
			(yyval.node) = ntop;
			Swig_symbol_setscope(cscope);
			Delete(Namespaceprefix);
			Namespaceprefix = Swig_symbol_qualifiedscopename(0);
			if (error || (nscope_inner && Strcmp(nodeType(nscope_inner), "class") == 0)) {
			  (yyval.node) = 0;
			}
			if (currentOuterClass)
			  template_parameters = Getattr(currentOuterClass, "template_parameters");
			else
			  template_parameters = 0;
			parsing_template_declaration = 0;
                }
#line 7995 "y.tab.c" /* yacc.c:1646  */
    break;

  case 170:
#line 4359 "parser.y" /* yacc.c:1646  */
    {
		  Swig_warning(WARN_PARSE_EXPLICIT_TEMPLATE, cparse_file, cparse_line, "Explicit template instantiation ignored.\n");
                  (yyval.node) = 0; 
		}
#line 8004 "y.tab.c" /* yacc.c:1646  */
    break;

  case 171:
#line 4365 "parser.y" /* yacc.c:1646  */
    {
		  Swig_warning(WARN_PARSE_EXPLICIT_TEMPLATE, cparse_file, cparse_line, "Explicit template instantiation ignored.\n");
                  (yyval.node) = 0; 
                }
#line 8013 "y.tab.c" /* yacc.c:1646  */
    break;

  case 172:
#line 4371 "parser.y" /* yacc.c:1646  */
    {
		  (yyval.node) = (yyvsp[0].node);
                }
#line 8021 "y.tab.c" /* yacc.c:1646  */
    break;

  case 173:
#line 4374 "parser.y" /* yacc.c:1646  */
    {
                   (yyval.node) = (yyvsp[0].node);
                }
#line 8029 "y.tab.c" /* yacc.c:1646  */
    break;

  case 174:
#line 4377 "parser.y" /* yacc.c:1646  */
    {
                   (yyval.node) = (yyvsp[0].node);
                }
#line 8037 "y.tab.c" /* yacc.c:1646  */
    break;

  case 175:
#line 4380 "parser.y" /* yacc.c:1646  */
    {
		  (yyval.node) = 0;
                }
#line 8045 "y.tab.c" /* yacc.c:1646  */
    break;

  case 176:
#line 4383 "parser.y" /* yacc.c:1646  */
    {
                  (yyval.node) = (yyvsp[0].node);
                }
#line 8053 "y.tab.c" /* yacc.c:1646  */
    break;

  case 177:
#line 4386 "parser.y" /* yacc.c:1646  */
    {
                  (yyval.node) = (yyvsp[0].node);
                }
#line 8061 "y.tab.c" /* yacc.c:1646  */
    break;

  case 178:
#line 4391 "parser.y" /* yacc.c:1646  */
    {
		   /* Rip out the parameter names */
		  Parm *p = (yyvsp[0].pl);
		  (yyval.tparms) = (yyvsp[0].pl);

		  while (p) {
		    String *name = Getattr(p,"name");
		    if (!name) {
		      /* Hmmm. Maybe it's a 'class T' parameter */
		      char *type = Char(Getattr(p,"type"));
		      /* Template template parameter */
		      if (strncmp(type,"template<class> ",16) == 0) {
			type += 16;
		      }
		      if ((strncmp(type,"class ",6) == 0) || (strncmp(type,"typename ", 9) == 0)) {
			char *t = strchr(type,' ');
			Setattr(p,"name", t+1);
		      } else 
                      /* Variadic template args */
		      if ((strncmp(type,"class... ",9) == 0) || (strncmp(type,"typename... ", 12) == 0)) {
			char *t = strchr(type,' ');
			Setattr(p,"name", t+1);
			Setattr(p,"variadic", "1");
		      } else {
			/*
			 Swig_error(cparse_file, cparse_line, "Missing template parameter name\n");
			 $$.rparms = 0;
			 $$.parms = 0;
			 break; */
		      }
		    }
		    p = nextSibling(p);
		  }
                 }
#line 8100 "y.tab.c" /* yacc.c:1646  */
    break;

  case 179:
#line 4427 "parser.y" /* yacc.c:1646  */
    {
                      set_nextSibling((yyvsp[-1].p),(yyvsp[0].pl));
                      (yyval.pl) = (yyvsp[-1].p);
                   }
#line 8109 "y.tab.c" /* yacc.c:1646  */
    break;

  case 180:
#line 4431 "parser.y" /* yacc.c:1646  */
    { (yyval.pl) = 0; }
#line 8115 "y.tab.c" /* yacc.c:1646  */
    break;

  case 181:
#line 4434 "parser.y" /* yacc.c:1646  */
    {
		    (yyval.p) = NewParmWithoutFileLineInfo(NewString((yyvsp[0].id)), 0);
                  }
#line 8123 "y.tab.c" /* yacc.c:1646  */
    break;

  case 182:
#line 4437 "parser.y" /* yacc.c:1646  */
    {
                    (yyval.p) = (yyvsp[0].p);
                  }
#line 8131 "y.tab.c" /* yacc.c:1646  */
    break;

  case 183:
#line 4442 "parser.y" /* yacc.c:1646  */
    {
                         set_nextSibling((yyvsp[-1].p),(yyvsp[0].pl));
                         (yyval.pl) = (yyvsp[-1].p);
                       }
#line 8140 "y.tab.c" /* yacc.c:1646  */
    break;

  case 184:
#line 4446 "parser.y" /* yacc.c:1646  */
    { (yyval.pl) = 0; }
#line 8146 "y.tab.c" /* yacc.c:1646  */
    break;

  case 185:
#line 4451 "parser.y" /* yacc.c:1646  */
    {
                  String *uname = Swig_symbol_type_qualify((yyvsp[-1].str),0);
		  String *name = Swig_scopename_last((yyvsp[-1].str));
                  (yyval.node) = new_node("using");
		  Setattr((yyval.node),"uname",uname);
		  Setattr((yyval.node),"name", name);
		  Delete(uname);
		  Delete(name);
		  add_symbols((yyval.node));
             }
#line 8161 "y.tab.c" /* yacc.c:1646  */
    break;

  case 186:
#line 4461 "parser.y" /* yacc.c:1646  */
    {
	       Node *n = Swig_symbol_clookup((yyvsp[-1].str),0);
	       if (!n) {
		 Swig_error(cparse_file, cparse_line, "Nothing known about namespace '%s'\n", (yyvsp[-1].str));
		 (yyval.node) = 0;
	       } else {

		 while (Strcmp(nodeType(n),"using") == 0) {
		   n = Getattr(n,"node");
		 }
		 if (n) {
		   if (Strcmp(nodeType(n),"namespace") == 0) {
		     Symtab *current = Swig_symbol_current();
		     Symtab *symtab = Getattr(n,"symtab");
		     (yyval.node) = new_node("using");
		     Setattr((yyval.node),"node",n);
		     Setattr((yyval.node),"namespace", (yyvsp[-1].str));
		     if (current != symtab) {
		       Swig_symbol_inherit(symtab);
		     }
		   } else {
		     Swig_error(cparse_file, cparse_line, "'%s' is not a namespace.\n", (yyvsp[-1].str));
		     (yyval.node) = 0;
		   }
		 } else {
		   (yyval.node) = 0;
		 }
	       }
             }
#line 8195 "y.tab.c" /* yacc.c:1646  */
    break;

  case 187:
#line 4492 "parser.y" /* yacc.c:1646  */
    { 
                Hash *h;
		Node *parent_ns = 0;
		List *scopes = Swig_scopename_tolist((yyvsp[-1].str));
		int ilen = Len(scopes);
		int i;

/*
Printf(stdout, "==== Namespace %s creation...\n", $2);
*/
		(yyval.node) = 0;
		for (i = 0; i < ilen; i++) {
		  Node *ns = new_node("namespace");
		  Symtab *current_symtab = Swig_symbol_current();
		  String *scopename = Getitem(scopes, i);
		  Setattr(ns, "name", scopename);
		  (yyval.node) = ns;
		  if (parent_ns)
		    appendChild(parent_ns, ns);
		  parent_ns = ns;
		  h = Swig_symbol_clookup(scopename, 0);
		  if (h && (current_symtab == Getattr(h, "sym:symtab")) && (Strcmp(nodeType(h), "namespace") == 0)) {
/*
Printf(stdout, "  Scope %s [found C++17 style]\n", scopename);
*/
		    if (Getattr(h, "alias")) {
		      h = Getattr(h, "namespace");
		      Swig_warning(WARN_PARSE_NAMESPACE_ALIAS, cparse_file, cparse_line, "Namespace alias '%s' not allowed here. Assuming '%s'\n",
				   scopename, Getattr(h, "name"));
		      scopename = Getattr(h, "name");
		    }
		    Swig_symbol_setscope(Getattr(h, "symtab"));
		  } else {
/*
Printf(stdout, "  Scope %s [creating single scope C++17 style]\n", scopename);
*/
		    h = Swig_symbol_newscope();
		    Swig_symbol_setscopename(scopename);
		  }
		  Delete(Namespaceprefix);
		  Namespaceprefix = Swig_symbol_qualifiedscopename(0);
		}
		Delete(scopes);
             }
#line 8244 "y.tab.c" /* yacc.c:1646  */
    break;

  case 188:
#line 4535 "parser.y" /* yacc.c:1646  */
    {
		Node *n = (yyvsp[-2].node);
		Node *top_ns = 0;
		do {
		  Setattr(n, "symtab", Swig_symbol_popscope());
		  Delete(Namespaceprefix);
		  Namespaceprefix = Swig_symbol_qualifiedscopename(0);
		  add_symbols(n);
		  top_ns = n;
		  n = parentNode(n);
		} while(n);
		appendChild((yyvsp[-2].node), firstChild((yyvsp[-1].node)));
		Delete((yyvsp[-1].node));
		(yyval.node) = top_ns;
             }
#line 8264 "y.tab.c" /* yacc.c:1646  */
    break;

  case 189:
#line 4550 "parser.y" /* yacc.c:1646  */
    {
	       Hash *h;
	       (yyvsp[-1].node) = Swig_symbol_current();
	       h = Swig_symbol_clookup("    ",0);
	       if (h && (Strcmp(nodeType(h),"namespace") == 0)) {
		 Swig_symbol_setscope(Getattr(h,"symtab"));
	       } else {
		 Swig_symbol_newscope();
		 /* we don't use "__unnamed__", but a long 'empty' name */
		 Swig_symbol_setscopename("    ");
	       }
	       Namespaceprefix = 0;
             }
#line 8282 "y.tab.c" /* yacc.c:1646  */
    break;

  case 190:
#line 4562 "parser.y" /* yacc.c:1646  */
    {
	       (yyval.node) = (yyvsp[-1].node);
	       set_nodeType((yyval.node),"namespace");
	       Setattr((yyval.node),"unnamed","1");
	       Setattr((yyval.node),"symtab", Swig_symbol_popscope());
	       Swig_symbol_setscope((yyvsp[-4].node));
	       Delete(Namespaceprefix);
	       Namespaceprefix = Swig_symbol_qualifiedscopename(0);
	       add_symbols((yyval.node));
             }
#line 8297 "y.tab.c" /* yacc.c:1646  */
    break;

  case 191:
#line 4572 "parser.y" /* yacc.c:1646  */
    {
	       /* Namespace alias */
	       Node *n;
	       (yyval.node) = new_node("namespace");
	       Setattr((yyval.node),"name",(yyvsp[-3].id));
	       Setattr((yyval.node),"alias",(yyvsp[-1].str));
	       n = Swig_symbol_clookup((yyvsp[-1].str),0);
	       if (!n) {
		 Swig_error(cparse_file, cparse_line, "Unknown namespace '%s'\n", (yyvsp[-1].str));
		 (yyval.node) = 0;
	       } else {
		 if (Strcmp(nodeType(n),"namespace") != 0) {
		   Swig_error(cparse_file, cparse_line, "'%s' is not a namespace\n",(yyvsp[-1].str));
		   (yyval.node) = 0;
		 } else {
		   while (Getattr(n,"alias")) {
		     n = Getattr(n,"namespace");
		   }
		   Setattr((yyval.node),"namespace",n);
		   add_symbols((yyval.node));
		   /* Set up a scope alias */
		   Swig_symbol_alias((yyvsp[-3].id),Getattr(n,"symtab"));
		 }
	       }
             }
#line 8327 "y.tab.c" /* yacc.c:1646  */
    break;

  case 192:
#line 4599 "parser.y" /* yacc.c:1646  */
    {
                   (yyval.node) = (yyvsp[-1].node);
                   /* Insert cpp_member (including any siblings) to the front of the cpp_members linked list */
		   if ((yyval.node)) {
		     Node *p = (yyval.node);
		     Node *pp =0;
		     while (p) {
		       pp = p;
		       p = nextSibling(p);
		     }
		     set_nextSibling(pp,(yyvsp[0].node));
		     if ((yyvsp[0].node))
		       set_previousSibling((yyvsp[0].node), pp);
		   } else {
		     (yyval.node) = (yyvsp[0].node);
		   }
             }
#line 8349 "y.tab.c" /* yacc.c:1646  */
    break;

  case 193:
#line 4616 "parser.y" /* yacc.c:1646  */
    { 
	       extendmode = 1;
	       if (cplus_mode != CPLUS_PUBLIC) {
		 Swig_error(cparse_file,cparse_line,"%%extend can only be used in a public section\n");
	       }
             }
#line 8360 "y.tab.c" /* yacc.c:1646  */
    break;

  case 194:
#line 4621 "parser.y" /* yacc.c:1646  */
    {
	       extendmode = 0;
	     }
#line 8368 "y.tab.c" /* yacc.c:1646  */
    break;

  case 195:
#line 4623 "parser.y" /* yacc.c:1646  */
    {
	       (yyval.node) = new_node("extend");
	       mark_nodes_as_extend((yyvsp[-3].node));
	       appendChild((yyval.node),(yyvsp[-3].node));
	       set_nextSibling((yyval.node),(yyvsp[0].node));
	     }
#line 8379 "y.tab.c" /* yacc.c:1646  */
    break;

  case 196:
#line 4629 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = (yyvsp[0].node); }
#line 8385 "y.tab.c" /* yacc.c:1646  */
    break;

  case 197:
#line 4630 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = 0;}
#line 8391 "y.tab.c" /* yacc.c:1646  */
    break;

  case 198:
#line 4631 "parser.y" /* yacc.c:1646  */
    {
	       int start_line = cparse_line;
	       skip_decl();
	       Swig_error(cparse_file,start_line,"Syntax error in input(3).\n");
	       SWIG_exit(EXIT_FAILURE);
	       }
#line 8402 "y.tab.c" /* yacc.c:1646  */
    break;

  case 199:
#line 4636 "parser.y" /* yacc.c:1646  */
    { 
		 (yyval.node) = (yyvsp[0].node);
   	     }
#line 8410 "y.tab.c" /* yacc.c:1646  */
    break;

  case 200:
#line 4647 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = (yyvsp[0].node); }
#line 8416 "y.tab.c" /* yacc.c:1646  */
    break;

  case 201:
#line 4648 "parser.y" /* yacc.c:1646  */
    { 
                 (yyval.node) = (yyvsp[0].node); 
		 if (extendmode && current_class) {
		   String *symname;
		   symname= make_name((yyval.node),Getattr((yyval.node),"name"), Getattr((yyval.node),"decl"));
		   if (Strcmp(symname,Getattr((yyval.node),"name")) == 0) {
		     /* No renaming operation.  Set name to class name */
		     Delete(yyrename);
		     yyrename = NewString(Getattr(current_class,"sym:name"));
		   } else {
		     Delete(yyrename);
		     yyrename = symname;
		   }
		 }
		 add_symbols((yyval.node));
                 default_arguments((yyval.node));
             }
#line 8438 "y.tab.c" /* yacc.c:1646  */
    break;

  case 202:
#line 4665 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = (yyvsp[0].node); }
#line 8444 "y.tab.c" /* yacc.c:1646  */
    break;

  case 203:
#line 4666 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = (yyvsp[0].node); }
#line 8450 "y.tab.c" /* yacc.c:1646  */
    break;

  case 204:
#line 4667 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = (yyvsp[0].node); }
#line 8456 "y.tab.c" /* yacc.c:1646  */
    break;

  case 205:
#line 4668 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = (yyvsp[0].node); }
#line 8462 "y.tab.c" /* yacc.c:1646  */
    break;

  case 206:
#line 4669 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = (yyvsp[0].node); }
#line 8468 "y.tab.c" /* yacc.c:1646  */
    break;

  case 207:
#line 4670 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = (yyvsp[0].node); }
#line 8474 "y.tab.c" /* yacc.c:1646  */
    break;

  case 208:
#line 4671 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = 0; }
#line 8480 "y.tab.c" /* yacc.c:1646  */
    break;

  case 209:
#line 4672 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = (yyvsp[0].node); }
#line 8486 "y.tab.c" /* yacc.c:1646  */
    break;

  case 210:
#line 4673 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = (yyvsp[0].node); }
#line 8492 "y.tab.c" /* yacc.c:1646  */
    break;

  case 211:
#line 4674 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = 0; }
#line 8498 "y.tab.c" /* yacc.c:1646  */
    break;

  case 212:
#line 4675 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = (yyvsp[0].node); }
#line 8504 "y.tab.c" /* yacc.c:1646  */
    break;

  case 213:
#line 4676 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = (yyvsp[0].node); }
#line 8510 "y.tab.c" /* yacc.c:1646  */
    break;

  case 214:
#line 4677 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = 0; }
#line 8516 "y.tab.c" /* yacc.c:1646  */
    break;

  case 215:
#line 4678 "parser.y" /* yacc.c:1646  */
    {(yyval.node) = (yyvsp[0].node); }
#line 8522 "y.tab.c" /* yacc.c:1646  */
    break;

  case 216:
#line 4679 "parser.y" /* yacc.c:1646  */
    {(yyval.node) = (yyvsp[0].node); }
#line 8528 "y.tab.c" /* yacc.c:1646  */
    break;

  case 217:
#line 4680 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = 0; }
#line 8534 "y.tab.c" /* yacc.c:1646  */
    break;

  case 218:
#line 4682 "parser.y" /* yacc.c:1646  */
    {
		(yyval.node) = (yyvsp[0].node);
	     }
#line 8542 "y.tab.c" /* yacc.c:1646  */
    break;

  case 219:
#line 4685 "parser.y" /* yacc.c:1646  */
    {
	         (yyval.node) = (yyvsp[0].node);
		 set_comment((yyvsp[0].node), (yyvsp[-1].str));
	     }
#line 8551 "y.tab.c" /* yacc.c:1646  */
    break;

  case 220:
#line 4689 "parser.y" /* yacc.c:1646  */
    {
	         (yyval.node) = (yyvsp[-1].node);
		 set_comment((yyvsp[-1].node), (yyvsp[0].str));
	     }
#line 8560 "y.tab.c" /* yacc.c:1646  */
    break;

  case 221:
#line 4701 "parser.y" /* yacc.c:1646  */
    {
              if (inclass || extendmode) {
		SwigType *decl = NewStringEmpty();
		(yyval.node) = new_node("constructor");
		Setattr((yyval.node),"storage",(yyvsp[-5].id));
		Setattr((yyval.node),"name",(yyvsp[-4].type));
		Setattr((yyval.node),"parms",(yyvsp[-2].pl));
		SwigType_add_function(decl,(yyvsp[-2].pl));
		Setattr((yyval.node),"decl",decl);
		Setattr((yyval.node),"throws",(yyvsp[0].decl).throws);
		Setattr((yyval.node),"throw",(yyvsp[0].decl).throwf);
		Setattr((yyval.node),"noexcept",(yyvsp[0].decl).nexcept);
		Setattr((yyval.node),"final",(yyvsp[0].decl).final);
		if (Len(scanner_ccode)) {
		  String *code = Copy(scanner_ccode);
		  Setattr((yyval.node),"code",code);
		  Delete(code);
		}
		SetFlag((yyval.node),"feature:new");
		if ((yyvsp[0].decl).defarg)
		  Setattr((yyval.node),"value",(yyvsp[0].decl).defarg);
	      } else {
		(yyval.node) = 0;
              }
              }
#line 8590 "y.tab.c" /* yacc.c:1646  */
    break;

  case 222:
#line 4730 "parser.y" /* yacc.c:1646  */
    {
               String *name = NewStringf("%s",(yyvsp[-4].str));
	       if (*(Char(name)) != '~') Insert(name,0,"~");
               (yyval.node) = new_node("destructor");
	       Setattr((yyval.node),"name",name);
	       Delete(name);
	       if (Len(scanner_ccode)) {
		 String *code = Copy(scanner_ccode);
		 Setattr((yyval.node),"code",code);
		 Delete(code);
	       }
	       {
		 String *decl = NewStringEmpty();
		 SwigType_add_function(decl,(yyvsp[-2].pl));
		 Setattr((yyval.node),"decl",decl);
		 Delete(decl);
	       }
	       Setattr((yyval.node),"throws",(yyvsp[0].dtype).throws);
	       Setattr((yyval.node),"throw",(yyvsp[0].dtype).throwf);
	       Setattr((yyval.node),"noexcept",(yyvsp[0].dtype).nexcept);
	       Setattr((yyval.node),"final",(yyvsp[0].dtype).final);
	       if ((yyvsp[0].dtype).val)
	         Setattr((yyval.node),"value",(yyvsp[0].dtype).val);
	       if ((yyvsp[0].dtype).qualifier)
		 Swig_error(cparse_file, cparse_line, "Destructor %s %s cannot have a qualifier.\n", Swig_name_decl((yyval.node)), SwigType_str((yyvsp[0].dtype).qualifier, 0));
	       add_symbols((yyval.node));
	      }
#line 8622 "y.tab.c" /* yacc.c:1646  */
    break;

  case 223:
#line 4760 "parser.y" /* yacc.c:1646  */
    {
		String *name;
		(yyval.node) = new_node("destructor");
		Setattr((yyval.node),"storage","virtual");
	        name = NewStringf("%s",(yyvsp[-4].str));
		if (*(Char(name)) != '~') Insert(name,0,"~");
		Setattr((yyval.node),"name",name);
		Delete(name);
		Setattr((yyval.node),"throws",(yyvsp[0].dtype).throws);
		Setattr((yyval.node),"throw",(yyvsp[0].dtype).throwf);
		Setattr((yyval.node),"noexcept",(yyvsp[0].dtype).nexcept);
		Setattr((yyval.node),"final",(yyvsp[0].dtype).final);
		if ((yyvsp[0].dtype).val)
		  Setattr((yyval.node),"value",(yyvsp[0].dtype).val);
		if (Len(scanner_ccode)) {
		  String *code = Copy(scanner_ccode);
		  Setattr((yyval.node),"code",code);
		  Delete(code);
		}
		{
		  String *decl = NewStringEmpty();
		  SwigType_add_function(decl,(yyvsp[-2].pl));
		  Setattr((yyval.node),"decl",decl);
		  Delete(decl);
		}
		if ((yyvsp[0].dtype).qualifier)
		  Swig_error(cparse_file, cparse_line, "Destructor %s %s cannot have a qualifier.\n", Swig_name_decl((yyval.node)), SwigType_str((yyvsp[0].dtype).qualifier, 0));
		add_symbols((yyval.node));
	      }
#line 8656 "y.tab.c" /* yacc.c:1646  */
    break;

  case 224:
#line 4793 "parser.y" /* yacc.c:1646  */
    {
                 (yyval.node) = new_node("cdecl");
                 Setattr((yyval.node),"type",(yyvsp[-5].type));
		 Setattr((yyval.node),"name",(yyvsp[-6].str));
		 Setattr((yyval.node),"storage",(yyvsp[-7].id));

		 SwigType_add_function((yyvsp[-4].type),(yyvsp[-2].pl));
		 if ((yyvsp[0].dtype).qualifier) {
		   SwigType_push((yyvsp[-4].type),(yyvsp[0].dtype).qualifier);
		 }
		 if ((yyvsp[0].dtype).val) {
		   Setattr((yyval.node),"value",(yyvsp[0].dtype).val);
		 }
		 Setattr((yyval.node),"refqualifier",(yyvsp[0].dtype).refqualifier);
		 Setattr((yyval.node),"decl",(yyvsp[-4].type));
		 Setattr((yyval.node),"parms",(yyvsp[-2].pl));
		 Setattr((yyval.node),"conversion_operator","1");
		 add_symbols((yyval.node));
              }
#line 8680 "y.tab.c" /* yacc.c:1646  */
    break;

  case 225:
#line 4812 "parser.y" /* yacc.c:1646  */
    {
		 SwigType *decl;
                 (yyval.node) = new_node("cdecl");
                 Setattr((yyval.node),"type",(yyvsp[-5].type));
		 Setattr((yyval.node),"name",(yyvsp[-6].str));
		 Setattr((yyval.node),"storage",(yyvsp[-7].id));
		 decl = NewStringEmpty();
		 SwigType_add_reference(decl);
		 SwigType_add_function(decl,(yyvsp[-2].pl));
		 if ((yyvsp[0].dtype).qualifier) {
		   SwigType_push(decl,(yyvsp[0].dtype).qualifier);
		 }
		 if ((yyvsp[0].dtype).val) {
		   Setattr((yyval.node),"value",(yyvsp[0].dtype).val);
		 }
		 Setattr((yyval.node),"refqualifier",(yyvsp[0].dtype).refqualifier);
		 Setattr((yyval.node),"decl",decl);
		 Setattr((yyval.node),"parms",(yyvsp[-2].pl));
		 Setattr((yyval.node),"conversion_operator","1");
		 add_symbols((yyval.node));
	       }
#line 8706 "y.tab.c" /* yacc.c:1646  */
    break;

  case 226:
#line 4833 "parser.y" /* yacc.c:1646  */
    {
		 SwigType *decl;
                 (yyval.node) = new_node("cdecl");
                 Setattr((yyval.node),"type",(yyvsp[-5].type));
		 Setattr((yyval.node),"name",(yyvsp[-6].str));
		 Setattr((yyval.node),"storage",(yyvsp[-7].id));
		 decl = NewStringEmpty();
		 SwigType_add_rvalue_reference(decl);
		 SwigType_add_function(decl,(yyvsp[-2].pl));
		 if ((yyvsp[0].dtype).qualifier) {
		   SwigType_push(decl,(yyvsp[0].dtype).qualifier);
		 }
		 if ((yyvsp[0].dtype).val) {
		   Setattr((yyval.node),"value",(yyvsp[0].dtype).val);
		 }
		 Setattr((yyval.node),"refqualifier",(yyvsp[0].dtype).refqualifier);
		 Setattr((yyval.node),"decl",decl);
		 Setattr((yyval.node),"parms",(yyvsp[-2].pl));
		 Setattr((yyval.node),"conversion_operator","1");
		 add_symbols((yyval.node));
	       }
#line 8732 "y.tab.c" /* yacc.c:1646  */
    break;

  case 227:
#line 4855 "parser.y" /* yacc.c:1646  */
    {
		 SwigType *decl;
                 (yyval.node) = new_node("cdecl");
                 Setattr((yyval.node),"type",(yyvsp[-6].type));
		 Setattr((yyval.node),"name",(yyvsp[-7].str));
		 Setattr((yyval.node),"storage",(yyvsp[-8].id));
		 decl = NewStringEmpty();
		 SwigType_add_pointer(decl);
		 SwigType_add_reference(decl);
		 SwigType_add_function(decl,(yyvsp[-2].pl));
		 if ((yyvsp[0].dtype).qualifier) {
		   SwigType_push(decl,(yyvsp[0].dtype).qualifier);
		 }
		 if ((yyvsp[0].dtype).val) {
		   Setattr((yyval.node),"value",(yyvsp[0].dtype).val);
		 }
		 Setattr((yyval.node),"refqualifier",(yyvsp[0].dtype).refqualifier);
		 Setattr((yyval.node),"decl",decl);
		 Setattr((yyval.node),"parms",(yyvsp[-2].pl));
		 Setattr((yyval.node),"conversion_operator","1");
		 add_symbols((yyval.node));
	       }
#line 8759 "y.tab.c" /* yacc.c:1646  */
    break;

  case 228:
#line 4878 "parser.y" /* yacc.c:1646  */
    {
		String *t = NewStringEmpty();
		(yyval.node) = new_node("cdecl");
		Setattr((yyval.node),"type",(yyvsp[-4].type));
		Setattr((yyval.node),"name",(yyvsp[-5].str));
		 Setattr((yyval.node),"storage",(yyvsp[-6].id));
		SwigType_add_function(t,(yyvsp[-2].pl));
		if ((yyvsp[0].dtype).qualifier) {
		  SwigType_push(t,(yyvsp[0].dtype).qualifier);
		}
		if ((yyvsp[0].dtype).val) {
		  Setattr((yyval.node),"value",(yyvsp[0].dtype).val);
		}
		Setattr((yyval.node),"refqualifier",(yyvsp[0].dtype).refqualifier);
		Setattr((yyval.node),"decl",t);
		Setattr((yyval.node),"parms",(yyvsp[-2].pl));
		Setattr((yyval.node),"conversion_operator","1");
		add_symbols((yyval.node));
              }
#line 8783 "y.tab.c" /* yacc.c:1646  */
    break;

  case 229:
#line 4901 "parser.y" /* yacc.c:1646  */
    {
                 skip_balanced('{','}');
                 (yyval.node) = 0;
               }
#line 8792 "y.tab.c" /* yacc.c:1646  */
    break;

  case 230:
#line 4909 "parser.y" /* yacc.c:1646  */
    {
                skip_balanced('(',')');
                (yyval.node) = 0;
              }
#line 8801 "y.tab.c" /* yacc.c:1646  */
    break;

  case 231:
#line 4916 "parser.y" /* yacc.c:1646  */
    { 
                (yyval.node) = new_node("access");
		Setattr((yyval.node),"kind","public");
                cplus_mode = CPLUS_PUBLIC;
              }
#line 8811 "y.tab.c" /* yacc.c:1646  */
    break;

  case 232:
#line 4923 "parser.y" /* yacc.c:1646  */
    { 
                (yyval.node) = new_node("access");
                Setattr((yyval.node),"kind","private");
		cplus_mode = CPLUS_PRIVATE;
	      }
#line 8821 "y.tab.c" /* yacc.c:1646  */
    break;

  case 233:
#line 4931 "parser.y" /* yacc.c:1646  */
    { 
		(yyval.node) = new_node("access");
		Setattr((yyval.node),"kind","protected");
		cplus_mode = CPLUS_PROTECTED;
	      }
#line 8831 "y.tab.c" /* yacc.c:1646  */
    break;

  case 234:
#line 4939 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = (yyvsp[0].node); }
#line 8837 "y.tab.c" /* yacc.c:1646  */
    break;

  case 235:
#line 4942 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = (yyvsp[0].node); }
#line 8843 "y.tab.c" /* yacc.c:1646  */
    break;

  case 236:
#line 4946 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = (yyvsp[0].node); }
#line 8849 "y.tab.c" /* yacc.c:1646  */
    break;

  case 237:
#line 4949 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = (yyvsp[0].node); }
#line 8855 "y.tab.c" /* yacc.c:1646  */
    break;

  case 238:
#line 4950 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = (yyvsp[0].node); }
#line 8861 "y.tab.c" /* yacc.c:1646  */
    break;

  case 239:
#line 4951 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = (yyvsp[0].node); }
#line 8867 "y.tab.c" /* yacc.c:1646  */
    break;

  case 240:
#line 4952 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = (yyvsp[0].node); }
#line 8873 "y.tab.c" /* yacc.c:1646  */
    break;

  case 241:
#line 4953 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = (yyvsp[0].node); }
#line 8879 "y.tab.c" /* yacc.c:1646  */
    break;

  case 242:
#line 4954 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = (yyvsp[0].node); }
#line 8885 "y.tab.c" /* yacc.c:1646  */
    break;

  case 243:
#line 4955 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = (yyvsp[0].node); }
#line 8891 "y.tab.c" /* yacc.c:1646  */
    break;

  case 244:
#line 4956 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = (yyvsp[0].node); }
#line 8897 "y.tab.c" /* yacc.c:1646  */
    break;

  case 245:
#line 4959 "parser.y" /* yacc.c:1646  */
    {
	            Clear(scanner_ccode);
		    (yyval.dtype).val = 0;
		    (yyval.dtype).qualifier = (yyvsp[-1].dtype).qualifier;
		    (yyval.dtype).refqualifier = (yyvsp[-1].dtype).refqualifier;
		    (yyval.dtype).bitfield = 0;
		    (yyval.dtype).throws = (yyvsp[-1].dtype).throws;
		    (yyval.dtype).throwf = (yyvsp[-1].dtype).throwf;
		    (yyval.dtype).nexcept = (yyvsp[-1].dtype).nexcept;
		    (yyval.dtype).final = (yyvsp[-1].dtype).final;
               }
#line 8913 "y.tab.c" /* yacc.c:1646  */
    break;

  case 246:
#line 4970 "parser.y" /* yacc.c:1646  */
    {
	            Clear(scanner_ccode);
		    (yyval.dtype).val = (yyvsp[-1].dtype).val;
		    (yyval.dtype).qualifier = (yyvsp[-3].dtype).qualifier;
		    (yyval.dtype).refqualifier = (yyvsp[-3].dtype).refqualifier;
		    (yyval.dtype).bitfield = 0;
		    (yyval.dtype).throws = (yyvsp[-3].dtype).throws;
		    (yyval.dtype).throwf = (yyvsp[-3].dtype).throwf;
		    (yyval.dtype).nexcept = (yyvsp[-3].dtype).nexcept;
		    (yyval.dtype).final = (yyvsp[-3].dtype).final;
               }
#line 8929 "y.tab.c" /* yacc.c:1646  */
    break;

  case 247:
#line 4981 "parser.y" /* yacc.c:1646  */
    { 
		    skip_balanced('{','}'); 
		    (yyval.dtype).val = 0;
		    (yyval.dtype).qualifier = (yyvsp[-1].dtype).qualifier;
		    (yyval.dtype).refqualifier = (yyvsp[-1].dtype).refqualifier;
		    (yyval.dtype).bitfield = 0;
		    (yyval.dtype).throws = (yyvsp[-1].dtype).throws;
		    (yyval.dtype).throwf = (yyvsp[-1].dtype).throwf;
		    (yyval.dtype).nexcept = (yyvsp[-1].dtype).nexcept;
		    (yyval.dtype).final = (yyvsp[-1].dtype).final;
	       }
#line 8945 "y.tab.c" /* yacc.c:1646  */
    break;

  case 248:
#line 4994 "parser.y" /* yacc.c:1646  */
    { 
                     Clear(scanner_ccode);
                     (yyval.dtype).val = 0;
                     (yyval.dtype).qualifier = (yyvsp[-1].dtype).qualifier;
                     (yyval.dtype).refqualifier = (yyvsp[-1].dtype).refqualifier;
                     (yyval.dtype).bitfield = 0;
                     (yyval.dtype).throws = (yyvsp[-1].dtype).throws;
                     (yyval.dtype).throwf = (yyvsp[-1].dtype).throwf;
                     (yyval.dtype).nexcept = (yyvsp[-1].dtype).nexcept;
                     (yyval.dtype).final = (yyvsp[-1].dtype).final;
                }
#line 8961 "y.tab.c" /* yacc.c:1646  */
    break;

  case 249:
#line 5005 "parser.y" /* yacc.c:1646  */
    { 
                     Clear(scanner_ccode);
                     (yyval.dtype).val = (yyvsp[-1].dtype).val;
                     (yyval.dtype).qualifier = (yyvsp[-3].dtype).qualifier;
                     (yyval.dtype).refqualifier = (yyvsp[-3].dtype).refqualifier;
                     (yyval.dtype).bitfield = 0;
                     (yyval.dtype).throws = (yyvsp[-3].dtype).throws; 
                     (yyval.dtype).throwf = (yyvsp[-3].dtype).throwf; 
                     (yyval.dtype).nexcept = (yyvsp[-3].dtype).nexcept;
                     (yyval.dtype).final = (yyvsp[-3].dtype).final;
               }
#line 8977 "y.tab.c" /* yacc.c:1646  */
    break;

  case 250:
#line 5016 "parser.y" /* yacc.c:1646  */
    { 
                     skip_balanced('{','}');
                     (yyval.dtype).val = 0;
                     (yyval.dtype).qualifier = (yyvsp[-1].dtype).qualifier;
                     (yyval.dtype).refqualifier = (yyvsp[-1].dtype).refqualifier;
                     (yyval.dtype).bitfield = 0;
                     (yyval.dtype).throws = (yyvsp[-1].dtype).throws; 
                     (yyval.dtype).throwf = (yyvsp[-1].dtype).throwf; 
                     (yyval.dtype).nexcept = (yyvsp[-1].dtype).nexcept;
                     (yyval.dtype).final = (yyvsp[-1].dtype).final;
               }
#line 8993 "y.tab.c" /* yacc.c:1646  */
    break;

  case 251:
#line 5030 "parser.y" /* yacc.c:1646  */
    { }
#line 8999 "y.tab.c" /* yacc.c:1646  */
    break;

  case 252:
#line 5033 "parser.y" /* yacc.c:1646  */
    { (yyval.type) = (yyvsp[0].type);
                  /* Printf(stdout,"primitive = '%s'\n", $$);*/
                }
#line 9007 "y.tab.c" /* yacc.c:1646  */
    break;

  case 253:
#line 5036 "parser.y" /* yacc.c:1646  */
    { (yyval.type) = (yyvsp[0].type); }
#line 9013 "y.tab.c" /* yacc.c:1646  */
    break;

  case 254:
#line 5037 "parser.y" /* yacc.c:1646  */
    { (yyval.type) = (yyvsp[0].type); }
#line 9019 "y.tab.c" /* yacc.c:1646  */
    break;

  case 255:
#line 5041 "parser.y" /* yacc.c:1646  */
    { (yyval.type) = (yyvsp[0].type); }
#line 9025 "y.tab.c" /* yacc.c:1646  */
    break;

  case 256:
#line 5043 "parser.y" /* yacc.c:1646  */
    {
		  (yyval.type) = (yyvsp[0].str);
               }
#line 9033 "y.tab.c" /* yacc.c:1646  */
    break;

  case 257:
#line 5051 "parser.y" /* yacc.c:1646  */
    {
                   if (Strcmp((yyvsp[0].str),"C") == 0) {
		     (yyval.id) = "externc";
                   } else if (Strcmp((yyvsp[0].str),"C++") == 0) {
		     (yyval.id) = "extern";
		   } else {
		     Swig_warning(WARN_PARSE_UNDEFINED_EXTERN,cparse_file, cparse_line,"Unrecognized extern type \"%s\".\n", (yyvsp[0].str));
		     (yyval.id) = 0;
		   }
               }
#line 9048 "y.tab.c" /* yacc.c:1646  */
    break;

  case 258:
#line 5063 "parser.y" /* yacc.c:1646  */
    { (yyval.id) = "extern"; }
#line 9054 "y.tab.c" /* yacc.c:1646  */
    break;

  case 259:
#line 5064 "parser.y" /* yacc.c:1646  */
    { (yyval.id) = (yyvsp[0].id); }
#line 9060 "y.tab.c" /* yacc.c:1646  */
    break;

  case 260:
#line 5065 "parser.y" /* yacc.c:1646  */
    { (yyval.id) = "thread_local"; }
#line 9066 "y.tab.c" /* yacc.c:1646  */
    break;

  case 261:
#line 5066 "parser.y" /* yacc.c:1646  */
    { (yyval.id) = "typedef"; }
#line 9072 "y.tab.c" /* yacc.c:1646  */
    break;

  case 262:
#line 5067 "parser.y" /* yacc.c:1646  */
    { (yyval.id) = "static"; }
#line 9078 "y.tab.c" /* yacc.c:1646  */
    break;

  case 263:
#line 5068 "parser.y" /* yacc.c:1646  */
    { (yyval.id) = "typedef"; }
#line 9084 "y.tab.c" /* yacc.c:1646  */
    break;

  case 264:
#line 5069 "parser.y" /* yacc.c:1646  */
    { (yyval.id) = "virtual"; }
#line 9090 "y.tab.c" /* yacc.c:1646  */
    break;

  case 265:
#line 5070 "parser.y" /* yacc.c:1646  */
    { (yyval.id) = "friend"; }
#line 9096 "y.tab.c" /* yacc.c:1646  */
    break;

  case 266:
#line 5071 "parser.y" /* yacc.c:1646  */
    { (yyval.id) = "explicit"; }
#line 9102 "y.tab.c" /* yacc.c:1646  */
    break;

  case 267:
#line 5072 "parser.y" /* yacc.c:1646  */
    { (yyval.id) = "constexpr"; }
#line 9108 "y.tab.c" /* yacc.c:1646  */
    break;

  case 268:
#line 5073 "parser.y" /* yacc.c:1646  */
    { (yyval.id) = "explicit constexpr"; }
#line 9114 "y.tab.c" /* yacc.c:1646  */
    break;

  case 269:
#line 5074 "parser.y" /* yacc.c:1646  */
    { (yyval.id) = "explicit constexpr"; }
#line 9120 "y.tab.c" /* yacc.c:1646  */
    break;

  case 270:
#line 5075 "parser.y" /* yacc.c:1646  */
    { (yyval.id) = "static constexpr"; }
#line 9126 "y.tab.c" /* yacc.c:1646  */
    break;

  case 271:
#line 5076 "parser.y" /* yacc.c:1646  */
    { (yyval.id) = "static constexpr"; }
#line 9132 "y.tab.c" /* yacc.c:1646  */
    break;

  case 272:
#line 5077 "parser.y" /* yacc.c:1646  */
    { (yyval.id) = "thread_local"; }
#line 9138 "y.tab.c" /* yacc.c:1646  */
    break;

  case 273:
#line 5078 "parser.y" /* yacc.c:1646  */
    { (yyval.id) = "static thread_local"; }
#line 9144 "y.tab.c" /* yacc.c:1646  */
    break;

  case 274:
#line 5079 "parser.y" /* yacc.c:1646  */
    { (yyval.id) = "static thread_local"; }
#line 9150 "y.tab.c" /* yacc.c:1646  */
    break;

  case 275:
#line 5080 "parser.y" /* yacc.c:1646  */
    { (yyval.id) = "extern thread_local"; }
#line 9156 "y.tab.c" /* yacc.c:1646  */
    break;

  case 276:
#line 5081 "parser.y" /* yacc.c:1646  */
    { (yyval.id) = "extern thread_local"; }
#line 9162 "y.tab.c" /* yacc.c:1646  */
    break;

  case 277:
#line 5082 "parser.y" /* yacc.c:1646  */
    { (yyval.id) = 0; }
#line 9168 "y.tab.c" /* yacc.c:1646  */
    break;

  case 278:
#line 5089 "parser.y" /* yacc.c:1646  */
    {
                 Parm *p;
		 (yyval.pl) = (yyvsp[0].pl);
		 p = (yyvsp[0].pl);
                 while (p) {
		   Replace(Getattr(p,"type"),"typename ", "", DOH_REPLACE_ANY);
		   p = nextSibling(p);
                 }
               }
#line 9182 "y.tab.c" /* yacc.c:1646  */
    break;

  case 279:
#line 5100 "parser.y" /* yacc.c:1646  */
    {
                  set_nextSibling((yyvsp[-1].p),(yyvsp[0].pl));
                  (yyval.pl) = (yyvsp[-1].p);
		}
#line 9191 "y.tab.c" /* yacc.c:1646  */
    break;

  case 280:
#line 5104 "parser.y" /* yacc.c:1646  */
    {
		  (yyval.pl) = 0;
		  previousNode = currentNode;
		  currentNode=0;
	       }
#line 9201 "y.tab.c" /* yacc.c:1646  */
    break;

  case 281:
#line 5111 "parser.y" /* yacc.c:1646  */
    {
                 set_nextSibling((yyvsp[-1].p),(yyvsp[0].pl));
		 (yyval.pl) = (yyvsp[-1].p);
                }
#line 9210 "y.tab.c" /* yacc.c:1646  */
    break;

  case 282:
#line 5115 "parser.y" /* yacc.c:1646  */
    {
		 set_comment(previousNode, (yyvsp[-2].str));
                 set_nextSibling((yyvsp[-1].p), (yyvsp[0].pl));
		 (yyval.pl) = (yyvsp[-1].p);
               }
#line 9220 "y.tab.c" /* yacc.c:1646  */
    break;

  case 283:
#line 5120 "parser.y" /* yacc.c:1646  */
    { (yyval.pl) = 0; }
#line 9226 "y.tab.c" /* yacc.c:1646  */
    break;

  case 284:
#line 5124 "parser.y" /* yacc.c:1646  */
    {
                   SwigType_push((yyvsp[-1].type),(yyvsp[0].decl).type);
		   (yyval.p) = NewParmWithoutFileLineInfo((yyvsp[-1].type),(yyvsp[0].decl).id);
		   previousNode = currentNode;
		   currentNode = (yyval.p);
		   Setfile((yyval.p),cparse_file);
		   Setline((yyval.p),cparse_line);
		   if ((yyvsp[0].decl).defarg) {
		     Setattr((yyval.p),"value",(yyvsp[0].decl).defarg);
		   }
		}
#line 9242 "y.tab.c" /* yacc.c:1646  */
    break;

  case 285:
#line 5136 "parser.y" /* yacc.c:1646  */
    {
                  (yyval.p) = NewParmWithoutFileLineInfo(NewStringf("template<class> %s %s", (yyvsp[-2].id),(yyvsp[-1].str)), 0);
		  previousNode = currentNode;
		  currentNode = (yyval.p);
		  Setfile((yyval.p),cparse_file);
		  Setline((yyval.p),cparse_line);
                  if ((yyvsp[0].dtype).val) {
                    Setattr((yyval.p),"value",(yyvsp[0].dtype).val);
                  }
                }
#line 9257 "y.tab.c" /* yacc.c:1646  */
    break;

  case 286:
#line 5146 "parser.y" /* yacc.c:1646  */
    {
		  SwigType *t = NewString("v(...)");
		  (yyval.p) = NewParmWithoutFileLineInfo(t, 0);
		  previousNode = currentNode;
		  currentNode = (yyval.p);
		  Setfile((yyval.p),cparse_file);
		  Setline((yyval.p),cparse_line);
		}
#line 9270 "y.tab.c" /* yacc.c:1646  */
    break;

  case 287:
#line 5156 "parser.y" /* yacc.c:1646  */
    {
		  (yyval.p) = (yyvsp[0].p);
		}
#line 9278 "y.tab.c" /* yacc.c:1646  */
    break;

  case 288:
#line 5159 "parser.y" /* yacc.c:1646  */
    {
		  (yyval.p) = (yyvsp[0].p);
		  set_comment((yyvsp[0].p), (yyvsp[-1].str));
		}
#line 9287 "y.tab.c" /* yacc.c:1646  */
    break;

  case 289:
#line 5163 "parser.y" /* yacc.c:1646  */
    {
		  (yyval.p) = (yyvsp[-1].p);
		  set_comment((yyvsp[-1].p), (yyvsp[0].str));
		}
#line 9296 "y.tab.c" /* yacc.c:1646  */
    break;

  case 290:
#line 5169 "parser.y" /* yacc.c:1646  */
    {
                 Parm *p;
		 (yyval.p) = (yyvsp[0].p);
		 p = (yyvsp[0].p);
                 while (p) {
		   if (Getattr(p,"type")) {
		     Replace(Getattr(p,"type"),"typename ", "", DOH_REPLACE_ANY);
		   }
		   p = nextSibling(p);
                 }
               }
#line 9312 "y.tab.c" /* yacc.c:1646  */
    break;

  case 291:
#line 5182 "parser.y" /* yacc.c:1646  */
    {
                  set_nextSibling((yyvsp[-1].p),(yyvsp[0].p));
                  (yyval.p) = (yyvsp[-1].p);
		}
#line 9321 "y.tab.c" /* yacc.c:1646  */
    break;

  case 292:
#line 5186 "parser.y" /* yacc.c:1646  */
    { (yyval.p) = 0; }
#line 9327 "y.tab.c" /* yacc.c:1646  */
    break;

  case 293:
#line 5189 "parser.y" /* yacc.c:1646  */
    {
                 set_nextSibling((yyvsp[-1].p),(yyvsp[0].p));
		 (yyval.p) = (yyvsp[-1].p);
                }
#line 9336 "y.tab.c" /* yacc.c:1646  */
    break;

  case 294:
#line 5193 "parser.y" /* yacc.c:1646  */
    { (yyval.p) = 0; }
#line 9342 "y.tab.c" /* yacc.c:1646  */
    break;

  case 295:
#line 5197 "parser.y" /* yacc.c:1646  */
    {
		  (yyval.p) = (yyvsp[0].p);
		  {
		    /* We need to make a possible adjustment for integer parameters. */
		    SwigType *type;
		    Node     *n = 0;

		    while (!n) {
		      type = Getattr((yyvsp[0].p),"type");
		      n = Swig_symbol_clookup(type,0);     /* See if we can find a node that matches the typename */
		      if ((n) && (Strcmp(nodeType(n),"cdecl") == 0)) {
			SwigType *decl = Getattr(n,"decl");
			if (!SwigType_isfunction(decl)) {
			  String *value = Getattr(n,"value");
			  if (value) {
			    String *v = Copy(value);
			    Setattr((yyvsp[0].p),"type",v);
			    Delete(v);
			    n = 0;
			  }
			}
		      } else {
			break;
		      }
		    }
		  }

               }
#line 9375 "y.tab.c" /* yacc.c:1646  */
    break;

  case 296:
#line 5225 "parser.y" /* yacc.c:1646  */
    {
                  (yyval.p) = NewParmWithoutFileLineInfo(0,0);
                  Setfile((yyval.p),cparse_file);
		  Setline((yyval.p),cparse_line);
		  Setattr((yyval.p),"value",(yyvsp[0].dtype).val);
               }
#line 9386 "y.tab.c" /* yacc.c:1646  */
    break;

  case 297:
#line 5233 "parser.y" /* yacc.c:1646  */
    { 
                  (yyval.dtype) = (yyvsp[0].dtype); 
		  if ((yyvsp[0].dtype).type == T_ERROR) {
		    Swig_warning(WARN_PARSE_BAD_DEFAULT,cparse_file, cparse_line, "Can't set default argument (ignored)\n");
		    (yyval.dtype).val = 0;
		    (yyval.dtype).rawval = 0;
		    (yyval.dtype).bitfield = 0;
		    (yyval.dtype).throws = 0;
		    (yyval.dtype).throwf = 0;
		    (yyval.dtype).nexcept = 0;
		    (yyval.dtype).final = 0;
		  }
               }
#line 9404 "y.tab.c" /* yacc.c:1646  */
    break;

  case 298:
#line 5246 "parser.y" /* yacc.c:1646  */
    { 
		  (yyval.dtype) = (yyvsp[-3].dtype);
		  if ((yyvsp[-3].dtype).type == T_ERROR) {
		    Swig_warning(WARN_PARSE_BAD_DEFAULT,cparse_file, cparse_line, "Can't set default argument (ignored)\n");
		    (yyval.dtype) = (yyvsp[-3].dtype);
		    (yyval.dtype).val = 0;
		    (yyval.dtype).rawval = 0;
		    (yyval.dtype).bitfield = 0;
		    (yyval.dtype).throws = 0;
		    (yyval.dtype).throwf = 0;
		    (yyval.dtype).nexcept = 0;
		    (yyval.dtype).final = 0;
		  } else {
		    (yyval.dtype).val = NewStringf("%s[%s]",(yyvsp[-3].dtype).val,(yyvsp[-1].dtype).val); 
		  }		  
               }
#line 9425 "y.tab.c" /* yacc.c:1646  */
    break;

  case 299:
#line 5262 "parser.y" /* yacc.c:1646  */
    {
		 skip_balanced('{','}');
		 (yyval.dtype).val = NewString(scanner_ccode);
		 (yyval.dtype).rawval = 0;
                 (yyval.dtype).type = T_INT;
		 (yyval.dtype).bitfield = 0;
		 (yyval.dtype).throws = 0;
		 (yyval.dtype).throwf = 0;
		 (yyval.dtype).nexcept = 0;
		 (yyval.dtype).final = 0;
	       }
#line 9441 "y.tab.c" /* yacc.c:1646  */
    break;

  case 300:
#line 5273 "parser.y" /* yacc.c:1646  */
    { 
		 (yyval.dtype).val = 0;
		 (yyval.dtype).rawval = 0;
		 (yyval.dtype).type = 0;
		 (yyval.dtype).bitfield = (yyvsp[0].dtype).val;
		 (yyval.dtype).throws = 0;
		 (yyval.dtype).throwf = 0;
		 (yyval.dtype).nexcept = 0;
		 (yyval.dtype).final = 0;
	       }
#line 9456 "y.tab.c" /* yacc.c:1646  */
    break;

  case 301:
#line 5283 "parser.y" /* yacc.c:1646  */
    {
                 (yyval.dtype).val = 0;
                 (yyval.dtype).rawval = 0;
                 (yyval.dtype).type = T_INT;
		 (yyval.dtype).bitfield = 0;
		 (yyval.dtype).throws = 0;
		 (yyval.dtype).throwf = 0;
		 (yyval.dtype).nexcept = 0;
		 (yyval.dtype).final = 0;
               }
#line 9471 "y.tab.c" /* yacc.c:1646  */
    break;

  case 302:
#line 5295 "parser.y" /* yacc.c:1646  */
    {
                 (yyval.decl) = (yyvsp[-1].decl);
		 (yyval.decl).defarg = (yyvsp[0].dtype).rawval ? (yyvsp[0].dtype).rawval : (yyvsp[0].dtype).val;
            }
#line 9480 "y.tab.c" /* yacc.c:1646  */
    break;

  case 303:
#line 5299 "parser.y" /* yacc.c:1646  */
    {
              (yyval.decl) = (yyvsp[-1].decl);
	      (yyval.decl).defarg = (yyvsp[0].dtype).rawval ? (yyvsp[0].dtype).rawval : (yyvsp[0].dtype).val;
            }
#line 9489 "y.tab.c" /* yacc.c:1646  */
    break;

  case 304:
#line 5303 "parser.y" /* yacc.c:1646  */
    {
   	      (yyval.decl).type = 0;
              (yyval.decl).id = 0;
	      (yyval.decl).defarg = (yyvsp[0].dtype).rawval ? (yyvsp[0].dtype).rawval : (yyvsp[0].dtype).val;
            }
#line 9499 "y.tab.c" /* yacc.c:1646  */
    break;

  case 305:
#line 5310 "parser.y" /* yacc.c:1646  */
    {
	      SwigType *t;
	      (yyval.decl) = (yyvsp[-4].decl);
	      t = NewStringEmpty();
	      SwigType_add_function(t,(yyvsp[-2].pl));
	      if ((yyvsp[0].dtype).qualifier)
	        SwigType_push(t, (yyvsp[0].dtype).qualifier);
	      if (!(yyval.decl).have_parms) {
		(yyval.decl).parms = (yyvsp[-2].pl);
		(yyval.decl).have_parms = 1;
	      }
	      if (!(yyval.decl).type) {
		(yyval.decl).type = t;
	      } else {
		SwigType_push(t, (yyval.decl).type);
		Delete((yyval.decl).type);
		(yyval.decl).type = t;
	      }
	      (yyval.decl).defarg = 0;
	    }
#line 9524 "y.tab.c" /* yacc.c:1646  */
    break;

  case 306:
#line 5332 "parser.y" /* yacc.c:1646  */
    {
                 (yyval.decl) = (yyvsp[0].decl);
		 if (SwigType_isfunction((yyvsp[0].decl).type)) {
		   Delete(SwigType_pop_function((yyvsp[0].decl).type));
		 } else if (SwigType_isarray((yyvsp[0].decl).type)) {
		   SwigType *ta = SwigType_pop_arrays((yyvsp[0].decl).type);
		   if (SwigType_isfunction((yyvsp[0].decl).type)) {
		     Delete(SwigType_pop_function((yyvsp[0].decl).type));
		   } else {
		     (yyval.decl).parms = 0;
		   }
		   SwigType_push((yyvsp[0].decl).type,ta);
		   Delete(ta);
		 } else {
		   (yyval.decl).parms = 0;
		 }
            }
#line 9546 "y.tab.c" /* yacc.c:1646  */
    break;

  case 307:
#line 5349 "parser.y" /* yacc.c:1646  */
    {
              (yyval.decl) = (yyvsp[0].decl);
	      if (SwigType_isfunction((yyvsp[0].decl).type)) {
		Delete(SwigType_pop_function((yyvsp[0].decl).type));
	      } else if (SwigType_isarray((yyvsp[0].decl).type)) {
		SwigType *ta = SwigType_pop_arrays((yyvsp[0].decl).type);
		if (SwigType_isfunction((yyvsp[0].decl).type)) {
		  Delete(SwigType_pop_function((yyvsp[0].decl).type));
		} else {
		  (yyval.decl).parms = 0;
		}
		SwigType_push((yyvsp[0].decl).type,ta);
		Delete(ta);
	      } else {
		(yyval.decl).parms = 0;
	      }
            }
#line 9568 "y.tab.c" /* yacc.c:1646  */
    break;

  case 308:
#line 5368 "parser.y" /* yacc.c:1646  */
    {
	      SwigType *t;
	      (yyval.decl) = (yyvsp[-4].decl);
	      t = NewStringEmpty();
	      SwigType_add_function(t, (yyvsp[-2].pl));
	      if ((yyvsp[0].dtype).qualifier)
	        SwigType_push(t, (yyvsp[0].dtype).qualifier);
	      if (!(yyval.decl).have_parms) {
		(yyval.decl).parms = (yyvsp[-2].pl);
		(yyval.decl).have_parms = 1;
	      }
	      if (!(yyval.decl).type) {
		(yyval.decl).type = t;
	      } else {
		SwigType_push(t, (yyval.decl).type);
		Delete((yyval.decl).type);
		(yyval.decl).type = t;
	      }
	    }
#line 9592 "y.tab.c" /* yacc.c:1646  */
    break;

  case 309:
#line 5387 "parser.y" /* yacc.c:1646  */
    {
   	      (yyval.decl).type = 0;
              (yyval.decl).id = 0;
	      (yyval.decl).parms = 0;
	      }
#line 9602 "y.tab.c" /* yacc.c:1646  */
    break;

  case 310:
#line 5394 "parser.y" /* yacc.c:1646  */
    {
              (yyval.decl) = (yyvsp[0].decl);
	      if ((yyval.decl).type) {
		SwigType_push((yyvsp[-1].type),(yyval.decl).type);
		Delete((yyval.decl).type);
	      }
	      (yyval.decl).type = (yyvsp[-1].type);
           }
#line 9615 "y.tab.c" /* yacc.c:1646  */
    break;

  case 311:
#line 5402 "parser.y" /* yacc.c:1646  */
    {
              (yyval.decl) = (yyvsp[0].decl);
	      SwigType_add_reference((yyvsp[-2].type));
              if ((yyval.decl).type) {
		SwigType_push((yyvsp[-2].type),(yyval.decl).type);
		Delete((yyval.decl).type);
	      }
	      (yyval.decl).type = (yyvsp[-2].type);
           }
#line 9629 "y.tab.c" /* yacc.c:1646  */
    break;

  case 312:
#line 5411 "parser.y" /* yacc.c:1646  */
    {
              (yyval.decl) = (yyvsp[0].decl);
	      SwigType_add_rvalue_reference((yyvsp[-2].type));
              if ((yyval.decl).type) {
		SwigType_push((yyvsp[-2].type),(yyval.decl).type);
		Delete((yyval.decl).type);
	      }
	      (yyval.decl).type = (yyvsp[-2].type);
           }
#line 9643 "y.tab.c" /* yacc.c:1646  */
    break;

  case 313:
#line 5420 "parser.y" /* yacc.c:1646  */
    {
              (yyval.decl) = (yyvsp[0].decl);
	      if (!(yyval.decl).type) (yyval.decl).type = NewStringEmpty();
           }
#line 9652 "y.tab.c" /* yacc.c:1646  */
    break;

  case 314:
#line 5424 "parser.y" /* yacc.c:1646  */
    {
	     (yyval.decl) = (yyvsp[0].decl);
	     (yyval.decl).type = NewStringEmpty();
	     SwigType_add_reference((yyval.decl).type);
	     if ((yyvsp[0].decl).type) {
	       SwigType_push((yyval.decl).type,(yyvsp[0].decl).type);
	       Delete((yyvsp[0].decl).type);
	     }
           }
#line 9666 "y.tab.c" /* yacc.c:1646  */
    break;

  case 315:
#line 5433 "parser.y" /* yacc.c:1646  */
    {
	     /* Introduced in C++11, move operator && */
             /* Adds one S/R conflict */
	     (yyval.decl) = (yyvsp[0].decl);
	     (yyval.decl).type = NewStringEmpty();
	     SwigType_add_rvalue_reference((yyval.decl).type);
	     if ((yyvsp[0].decl).type) {
	       SwigType_push((yyval.decl).type,(yyvsp[0].decl).type);
	       Delete((yyvsp[0].decl).type);
	     }
           }
#line 9682 "y.tab.c" /* yacc.c:1646  */
    break;

  case 316:
#line 5444 "parser.y" /* yacc.c:1646  */
    { 
	     SwigType *t = NewStringEmpty();

	     (yyval.decl) = (yyvsp[0].decl);
	     SwigType_add_memberpointer(t,(yyvsp[-2].str));
	     if ((yyval.decl).type) {
	       SwigType_push(t,(yyval.decl).type);
	       Delete((yyval.decl).type);
	     }
	     (yyval.decl).type = t;
	     }
#line 9698 "y.tab.c" /* yacc.c:1646  */
    break;

  case 317:
#line 5455 "parser.y" /* yacc.c:1646  */
    { 
	     SwigType *t = NewStringEmpty();
	     (yyval.decl) = (yyvsp[0].decl);
	     SwigType_add_memberpointer(t,(yyvsp[-2].str));
	     SwigType_push((yyvsp[-3].type),t);
	     if ((yyval.decl).type) {
	       SwigType_push((yyvsp[-3].type),(yyval.decl).type);
	       Delete((yyval.decl).type);
	     }
	     (yyval.decl).type = (yyvsp[-3].type);
	     Delete(t);
	   }
#line 9715 "y.tab.c" /* yacc.c:1646  */
    break;

  case 318:
#line 5467 "parser.y" /* yacc.c:1646  */
    { 
	     (yyval.decl) = (yyvsp[0].decl);
	     SwigType_add_memberpointer((yyvsp[-4].type),(yyvsp[-3].str));
	     SwigType_add_reference((yyvsp[-4].type));
	     if ((yyval.decl).type) {
	       SwigType_push((yyvsp[-4].type),(yyval.decl).type);
	       Delete((yyval.decl).type);
	     }
	     (yyval.decl).type = (yyvsp[-4].type);
	   }
#line 9730 "y.tab.c" /* yacc.c:1646  */
    break;

  case 319:
#line 5477 "parser.y" /* yacc.c:1646  */
    { 
	     SwigType *t = NewStringEmpty();
	     (yyval.decl) = (yyvsp[0].decl);
	     SwigType_add_memberpointer(t,(yyvsp[-3].str));
	     SwigType_add_reference(t);
	     if ((yyval.decl).type) {
	       SwigType_push(t,(yyval.decl).type);
	       Delete((yyval.decl).type);
	     } 
	     (yyval.decl).type = t;
	   }
#line 9746 "y.tab.c" /* yacc.c:1646  */
    break;

  case 320:
#line 5491 "parser.y" /* yacc.c:1646  */
    {
              (yyval.decl) = (yyvsp[0].decl);
	      if ((yyval.decl).type) {
		SwigType_push((yyvsp[-4].type),(yyval.decl).type);
		Delete((yyval.decl).type);
	      }
	      (yyval.decl).type = (yyvsp[-4].type);
           }
#line 9759 "y.tab.c" /* yacc.c:1646  */
    break;

  case 321:
#line 5499 "parser.y" /* yacc.c:1646  */
    {
              (yyval.decl) = (yyvsp[0].decl);
	      SwigType_add_reference((yyvsp[-5].type));
              if ((yyval.decl).type) {
		SwigType_push((yyvsp[-5].type),(yyval.decl).type);
		Delete((yyval.decl).type);
	      }
	      (yyval.decl).type = (yyvsp[-5].type);
           }
#line 9773 "y.tab.c" /* yacc.c:1646  */
    break;

  case 322:
#line 5508 "parser.y" /* yacc.c:1646  */
    {
              (yyval.decl) = (yyvsp[0].decl);
	      SwigType_add_rvalue_reference((yyvsp[-5].type));
              if ((yyval.decl).type) {
		SwigType_push((yyvsp[-5].type),(yyval.decl).type);
		Delete((yyval.decl).type);
	      }
	      (yyval.decl).type = (yyvsp[-5].type);
           }
#line 9787 "y.tab.c" /* yacc.c:1646  */
    break;

  case 323:
#line 5517 "parser.y" /* yacc.c:1646  */
    {
              (yyval.decl) = (yyvsp[0].decl);
	      if (!(yyval.decl).type) (yyval.decl).type = NewStringEmpty();
           }
#line 9796 "y.tab.c" /* yacc.c:1646  */
    break;

  case 324:
#line 5521 "parser.y" /* yacc.c:1646  */
    {
	     (yyval.decl) = (yyvsp[0].decl);
	     (yyval.decl).type = NewStringEmpty();
	     SwigType_add_reference((yyval.decl).type);
	     if ((yyvsp[0].decl).type) {
	       SwigType_push((yyval.decl).type,(yyvsp[0].decl).type);
	       Delete((yyvsp[0].decl).type);
	     }
           }
#line 9810 "y.tab.c" /* yacc.c:1646  */
    break;

  case 325:
#line 5530 "parser.y" /* yacc.c:1646  */
    {
	     /* Introduced in C++11, move operator && */
             /* Adds one S/R conflict */
	     (yyval.decl) = (yyvsp[0].decl);
	     (yyval.decl).type = NewStringEmpty();
	     SwigType_add_rvalue_reference((yyval.decl).type);
	     if ((yyvsp[0].decl).type) {
	       SwigType_push((yyval.decl).type,(yyvsp[0].decl).type);
	       Delete((yyvsp[0].decl).type);
	     }
           }
#line 9826 "y.tab.c" /* yacc.c:1646  */
    break;

  case 326:
#line 5541 "parser.y" /* yacc.c:1646  */
    { 
	     SwigType *t = NewStringEmpty();

	     (yyval.decl) = (yyvsp[0].decl);
	     SwigType_add_memberpointer(t,(yyvsp[-5].str));
	     if ((yyval.decl).type) {
	       SwigType_push(t,(yyval.decl).type);
	       Delete((yyval.decl).type);
	     }
	     (yyval.decl).type = t;
	     }
#line 9842 "y.tab.c" /* yacc.c:1646  */
    break;

  case 327:
#line 5552 "parser.y" /* yacc.c:1646  */
    { 
	     SwigType *t = NewStringEmpty();
	     (yyval.decl) = (yyvsp[0].decl);
	     SwigType_add_memberpointer(t,(yyvsp[-5].str));
	     SwigType_push((yyvsp[-6].type),t);
	     if ((yyval.decl).type) {
	       SwigType_push((yyvsp[-6].type),(yyval.decl).type);
	       Delete((yyval.decl).type);
	     }
	     (yyval.decl).type = (yyvsp[-6].type);
	     Delete(t);
	   }
#line 9859 "y.tab.c" /* yacc.c:1646  */
    break;

  case 328:
#line 5564 "parser.y" /* yacc.c:1646  */
    { 
	     (yyval.decl) = (yyvsp[0].decl);
	     SwigType_add_memberpointer((yyvsp[-7].type),(yyvsp[-6].str));
	     SwigType_add_reference((yyvsp[-7].type));
	     if ((yyval.decl).type) {
	       SwigType_push((yyvsp[-7].type),(yyval.decl).type);
	       Delete((yyval.decl).type);
	     }
	     (yyval.decl).type = (yyvsp[-7].type);
	   }
#line 9874 "y.tab.c" /* yacc.c:1646  */
    break;

  case 329:
#line 5574 "parser.y" /* yacc.c:1646  */
    { 
	     (yyval.decl) = (yyvsp[0].decl);
	     SwigType_add_memberpointer((yyvsp[-7].type),(yyvsp[-6].str));
	     SwigType_add_rvalue_reference((yyvsp[-7].type));
	     if ((yyval.decl).type) {
	       SwigType_push((yyvsp[-7].type),(yyval.decl).type);
	       Delete((yyval.decl).type);
	     }
	     (yyval.decl).type = (yyvsp[-7].type);
	   }
#line 9889 "y.tab.c" /* yacc.c:1646  */
    break;

  case 330:
#line 5584 "parser.y" /* yacc.c:1646  */
    { 
	     SwigType *t = NewStringEmpty();
	     (yyval.decl) = (yyvsp[0].decl);
	     SwigType_add_memberpointer(t,(yyvsp[-6].str));
	     SwigType_add_reference(t);
	     if ((yyval.decl).type) {
	       SwigType_push(t,(yyval.decl).type);
	       Delete((yyval.decl).type);
	     } 
	     (yyval.decl).type = t;
	   }
#line 9905 "y.tab.c" /* yacc.c:1646  */
    break;

  case 331:
#line 5595 "parser.y" /* yacc.c:1646  */
    { 
	     SwigType *t = NewStringEmpty();
	     (yyval.decl) = (yyvsp[0].decl);
	     SwigType_add_memberpointer(t,(yyvsp[-6].str));
	     SwigType_add_rvalue_reference(t);
	     if ((yyval.decl).type) {
	       SwigType_push(t,(yyval.decl).type);
	       Delete((yyval.decl).type);
	     } 
	     (yyval.decl).type = t;
	   }
#line 9921 "y.tab.c" /* yacc.c:1646  */
    break;

  case 332:
#line 5608 "parser.y" /* yacc.c:1646  */
    {
                /* Note: This is non-standard C.  Template declarator is allowed to follow an identifier */
                 (yyval.decl).id = Char((yyvsp[0].str));
		 (yyval.decl).type = 0;
		 (yyval.decl).parms = 0;
		 (yyval.decl).have_parms = 0;
                  }
#line 9933 "y.tab.c" /* yacc.c:1646  */
    break;

  case 333:
#line 5615 "parser.y" /* yacc.c:1646  */
    {
                  (yyval.decl).id = Char(NewStringf("~%s",(yyvsp[0].str)));
                  (yyval.decl).type = 0;
                  (yyval.decl).parms = 0;
                  (yyval.decl).have_parms = 0;
                  }
#line 9944 "y.tab.c" /* yacc.c:1646  */
    break;

  case 334:
#line 5623 "parser.y" /* yacc.c:1646  */
    {
                  (yyval.decl).id = Char((yyvsp[-1].str));
                  (yyval.decl).type = 0;
                  (yyval.decl).parms = 0;
                  (yyval.decl).have_parms = 0;
                  }
#line 9955 "y.tab.c" /* yacc.c:1646  */
    break;

  case 335:
#line 5639 "parser.y" /* yacc.c:1646  */
    {
		    (yyval.decl) = (yyvsp[-1].decl);
		    if ((yyval.decl).type) {
		      SwigType_push((yyvsp[-2].type),(yyval.decl).type);
		      Delete((yyval.decl).type);
		    }
		    (yyval.decl).type = (yyvsp[-2].type);
                  }
#line 9968 "y.tab.c" /* yacc.c:1646  */
    break;

  case 336:
#line 5647 "parser.y" /* yacc.c:1646  */
    {
		    SwigType *t;
		    (yyval.decl) = (yyvsp[-1].decl);
		    t = NewStringEmpty();
		    SwigType_add_memberpointer(t,(yyvsp[-3].str));
		    if ((yyval.decl).type) {
		      SwigType_push(t,(yyval.decl).type);
		      Delete((yyval.decl).type);
		    }
		    (yyval.decl).type = t;
		    }
#line 9984 "y.tab.c" /* yacc.c:1646  */
    break;

  case 337:
#line 5658 "parser.y" /* yacc.c:1646  */
    { 
		    SwigType *t;
		    (yyval.decl) = (yyvsp[-2].decl);
		    t = NewStringEmpty();
		    SwigType_add_array(t,"");
		    if ((yyval.decl).type) {
		      SwigType_push(t,(yyval.decl).type);
		      Delete((yyval.decl).type);
		    }
		    (yyval.decl).type = t;
                  }
#line 10000 "y.tab.c" /* yacc.c:1646  */
    break;

  case 338:
#line 5669 "parser.y" /* yacc.c:1646  */
    { 
		    SwigType *t;
		    (yyval.decl) = (yyvsp[-3].decl);
		    t = NewStringEmpty();
		    SwigType_add_array(t,(yyvsp[-1].dtype).val);
		    if ((yyval.decl).type) {
		      SwigType_push(t,(yyval.decl).type);
		      Delete((yyval.decl).type);
		    }
		    (yyval.decl).type = t;
                  }
#line 10016 "y.tab.c" /* yacc.c:1646  */
    break;

  case 339:
#line 5680 "parser.y" /* yacc.c:1646  */
    {
		    SwigType *t;
                    (yyval.decl) = (yyvsp[-3].decl);
		    t = NewStringEmpty();
		    SwigType_add_function(t,(yyvsp[-1].pl));
		    if (!(yyval.decl).have_parms) {
		      (yyval.decl).parms = (yyvsp[-1].pl);
		      (yyval.decl).have_parms = 1;
		    }
		    if (!(yyval.decl).type) {
		      (yyval.decl).type = t;
		    } else {
		      SwigType_push(t, (yyval.decl).type);
		      Delete((yyval.decl).type);
		      (yyval.decl).type = t;
		    }
		  }
#line 10038 "y.tab.c" /* yacc.c:1646  */
    break;

  case 340:
#line 5699 "parser.y" /* yacc.c:1646  */
    {
                /* Note: This is non-standard C.  Template declarator is allowed to follow an identifier */
                 (yyval.decl).id = Char((yyvsp[0].str));
		 (yyval.decl).type = 0;
		 (yyval.decl).parms = 0;
		 (yyval.decl).have_parms = 0;
                  }
#line 10050 "y.tab.c" /* yacc.c:1646  */
    break;

  case 341:
#line 5707 "parser.y" /* yacc.c:1646  */
    {
                  (yyval.decl).id = Char(NewStringf("~%s",(yyvsp[0].str)));
                  (yyval.decl).type = 0;
                  (yyval.decl).parms = 0;
                  (yyval.decl).have_parms = 0;
                  }
#line 10061 "y.tab.c" /* yacc.c:1646  */
    break;

  case 342:
#line 5724 "parser.y" /* yacc.c:1646  */
    {
		    (yyval.decl) = (yyvsp[-1].decl);
		    if ((yyval.decl).type) {
		      SwigType_push((yyvsp[-2].type),(yyval.decl).type);
		      Delete((yyval.decl).type);
		    }
		    (yyval.decl).type = (yyvsp[-2].type);
                  }
#line 10074 "y.tab.c" /* yacc.c:1646  */
    break;

  case 343:
#line 5732 "parser.y" /* yacc.c:1646  */
    {
                    (yyval.decl) = (yyvsp[-1].decl);
		    if (!(yyval.decl).type) {
		      (yyval.decl).type = NewStringEmpty();
		    }
		    SwigType_add_reference((yyval.decl).type);
                  }
#line 10086 "y.tab.c" /* yacc.c:1646  */
    break;

  case 344:
#line 5739 "parser.y" /* yacc.c:1646  */
    {
                    (yyval.decl) = (yyvsp[-1].decl);
		    if (!(yyval.decl).type) {
		      (yyval.decl).type = NewStringEmpty();
		    }
		    SwigType_add_rvalue_reference((yyval.decl).type);
                  }
#line 10098 "y.tab.c" /* yacc.c:1646  */
    break;

  case 345:
#line 5746 "parser.y" /* yacc.c:1646  */
    {
		    SwigType *t;
		    (yyval.decl) = (yyvsp[-1].decl);
		    t = NewStringEmpty();
		    SwigType_add_memberpointer(t,(yyvsp[-3].str));
		    if ((yyval.decl).type) {
		      SwigType_push(t,(yyval.decl).type);
		      Delete((yyval.decl).type);
		    }
		    (yyval.decl).type = t;
		  }
#line 10114 "y.tab.c" /* yacc.c:1646  */
    break;

  case 346:
#line 5757 "parser.y" /* yacc.c:1646  */
    {
		    SwigType *t;
		    (yyval.decl) = (yyvsp[-1].decl);
		    t = NewStringEmpty();
		    SwigType_add_memberpointer(t, (yyvsp[-4].str));
		    SwigType_push(t, (yyvsp[-2].str));
		    if ((yyval.decl).type) {
		      SwigType_push(t, (yyval.decl).type);
		      Delete((yyval.decl).type);
		    }
		    (yyval.decl).type = t;
		  }
#line 10131 "y.tab.c" /* yacc.c:1646  */
    break;

  case 347:
#line 5769 "parser.y" /* yacc.c:1646  */
    {
		    SwigType *t;
		    (yyval.decl) = (yyvsp[-1].decl);
		    t = NewStringEmpty();
		    SwigType_add_memberpointer(t, (yyvsp[-3].str));
		    if ((yyval.decl).type) {
		      SwigType_push(t, (yyval.decl).type);
		      Delete((yyval.decl).type);
		    }
		    (yyval.decl).type = t;
		  }
#line 10147 "y.tab.c" /* yacc.c:1646  */
    break;

  case 348:
#line 5780 "parser.y" /* yacc.c:1646  */
    {
		    SwigType *t;
		    (yyval.decl) = (yyvsp[-1].decl);
		    t = NewStringEmpty();
		    SwigType_add_memberpointer(t, (yyvsp[-4].str));
		    SwigType_push(t, (yyvsp[-2].str));
		    if ((yyval.decl).type) {
		      SwigType_push(t, (yyval.decl).type);
		      Delete((yyval.decl).type);
		    }
		    (yyval.decl).type = t;
		  }
#line 10164 "y.tab.c" /* yacc.c:1646  */
    break;

  case 349:
#line 5792 "parser.y" /* yacc.c:1646  */
    { 
		    SwigType *t;
		    (yyval.decl) = (yyvsp[-2].decl);
		    t = NewStringEmpty();
		    SwigType_add_array(t,"");
		    if ((yyval.decl).type) {
		      SwigType_push(t,(yyval.decl).type);
		      Delete((yyval.decl).type);
		    }
		    (yyval.decl).type = t;
                  }
#line 10180 "y.tab.c" /* yacc.c:1646  */
    break;

  case 350:
#line 5803 "parser.y" /* yacc.c:1646  */
    { 
		    SwigType *t;
		    (yyval.decl) = (yyvsp[-3].decl);
		    t = NewStringEmpty();
		    SwigType_add_array(t,(yyvsp[-1].dtype).val);
		    if ((yyval.decl).type) {
		      SwigType_push(t,(yyval.decl).type);
		      Delete((yyval.decl).type);
		    }
		    (yyval.decl).type = t;
                  }
#line 10196 "y.tab.c" /* yacc.c:1646  */
    break;

  case 351:
#line 5814 "parser.y" /* yacc.c:1646  */
    {
		    SwigType *t;
                    (yyval.decl) = (yyvsp[-3].decl);
		    t = NewStringEmpty();
		    SwigType_add_function(t,(yyvsp[-1].pl));
		    if (!(yyval.decl).have_parms) {
		      (yyval.decl).parms = (yyvsp[-1].pl);
		      (yyval.decl).have_parms = 1;
		    }
		    if (!(yyval.decl).type) {
		      (yyval.decl).type = t;
		    } else {
		      SwigType_push(t, (yyval.decl).type);
		      Delete((yyval.decl).type);
		      (yyval.decl).type = t;
		    }
		  }
#line 10218 "y.tab.c" /* yacc.c:1646  */
    break;

  case 352:
#line 5834 "parser.y" /* yacc.c:1646  */
    {
		    SwigType *t;
                    Append((yyvsp[-4].str), " "); /* intervening space is mandatory */
                    Append((yyvsp[-4].str), Char((yyvsp[-3].id)));
		    (yyval.decl).id = Char((yyvsp[-4].str));
		    t = NewStringEmpty();
		    SwigType_add_function(t,(yyvsp[-1].pl));
		    if (!(yyval.decl).have_parms) {
		      (yyval.decl).parms = (yyvsp[-1].pl);
		      (yyval.decl).have_parms = 1;
		    }
		    if (!(yyval.decl).type) {
		      (yyval.decl).type = t;
		    } else {
		      SwigType_push(t, (yyval.decl).type);
		      Delete((yyval.decl).type);
		      (yyval.decl).type = t;
		    }
		  }
#line 10242 "y.tab.c" /* yacc.c:1646  */
    break;

  case 353:
#line 5855 "parser.y" /* yacc.c:1646  */
    {
		    (yyval.decl).type = (yyvsp[0].type);
                    (yyval.decl).id = 0;
		    (yyval.decl).parms = 0;
		    (yyval.decl).have_parms = 0;
                  }
#line 10253 "y.tab.c" /* yacc.c:1646  */
    break;

  case 354:
#line 5861 "parser.y" /* yacc.c:1646  */
    { 
                     (yyval.decl) = (yyvsp[0].decl);
                     SwigType_push((yyvsp[-1].type),(yyvsp[0].decl).type);
		     (yyval.decl).type = (yyvsp[-1].type);
		     Delete((yyvsp[0].decl).type);
                  }
#line 10264 "y.tab.c" /* yacc.c:1646  */
    break;

  case 355:
#line 5867 "parser.y" /* yacc.c:1646  */
    {
		    (yyval.decl).type = (yyvsp[-1].type);
		    SwigType_add_reference((yyval.decl).type);
		    (yyval.decl).id = 0;
		    (yyval.decl).parms = 0;
		    (yyval.decl).have_parms = 0;
		  }
#line 10276 "y.tab.c" /* yacc.c:1646  */
    break;

  case 356:
#line 5874 "parser.y" /* yacc.c:1646  */
    {
		    (yyval.decl).type = (yyvsp[-1].type);
		    SwigType_add_rvalue_reference((yyval.decl).type);
		    (yyval.decl).id = 0;
		    (yyval.decl).parms = 0;
		    (yyval.decl).have_parms = 0;
		  }
#line 10288 "y.tab.c" /* yacc.c:1646  */
    break;

  case 357:
#line 5881 "parser.y" /* yacc.c:1646  */
    {
		    (yyval.decl) = (yyvsp[0].decl);
		    SwigType_add_reference((yyvsp[-2].type));
		    if ((yyval.decl).type) {
		      SwigType_push((yyvsp[-2].type),(yyval.decl).type);
		      Delete((yyval.decl).type);
		    }
		    (yyval.decl).type = (yyvsp[-2].type);
                  }
#line 10302 "y.tab.c" /* yacc.c:1646  */
    break;

  case 358:
#line 5890 "parser.y" /* yacc.c:1646  */
    {
		    (yyval.decl) = (yyvsp[0].decl);
		    SwigType_add_rvalue_reference((yyvsp[-2].type));
		    if ((yyval.decl).type) {
		      SwigType_push((yyvsp[-2].type),(yyval.decl).type);
		      Delete((yyval.decl).type);
		    }
		    (yyval.decl).type = (yyvsp[-2].type);
                  }
#line 10316 "y.tab.c" /* yacc.c:1646  */
    break;

  case 359:
#line 5899 "parser.y" /* yacc.c:1646  */
    {
		    (yyval.decl) = (yyvsp[0].decl);
                  }
#line 10324 "y.tab.c" /* yacc.c:1646  */
    break;

  case 360:
#line 5902 "parser.y" /* yacc.c:1646  */
    {
		    (yyval.decl) = (yyvsp[0].decl);
		    (yyval.decl).type = NewStringEmpty();
		    SwigType_add_reference((yyval.decl).type);
		    if ((yyvsp[0].decl).type) {
		      SwigType_push((yyval.decl).type,(yyvsp[0].decl).type);
		      Delete((yyvsp[0].decl).type);
		    }
                  }
#line 10338 "y.tab.c" /* yacc.c:1646  */
    break;

  case 361:
#line 5911 "parser.y" /* yacc.c:1646  */
    {
		    (yyval.decl) = (yyvsp[0].decl);
		    (yyval.decl).type = NewStringEmpty();
		    SwigType_add_rvalue_reference((yyval.decl).type);
		    if ((yyvsp[0].decl).type) {
		      SwigType_push((yyval.decl).type,(yyvsp[0].decl).type);
		      Delete((yyvsp[0].decl).type);
		    }
                  }
#line 10352 "y.tab.c" /* yacc.c:1646  */
    break;

  case 362:
#line 5920 "parser.y" /* yacc.c:1646  */
    {
                    (yyval.decl).id = 0;
                    (yyval.decl).parms = 0;
		    (yyval.decl).have_parms = 0;
                    (yyval.decl).type = NewStringEmpty();
		    SwigType_add_reference((yyval.decl).type);
                  }
#line 10364 "y.tab.c" /* yacc.c:1646  */
    break;

  case 363:
#line 5927 "parser.y" /* yacc.c:1646  */
    {
                    (yyval.decl).id = 0;
                    (yyval.decl).parms = 0;
		    (yyval.decl).have_parms = 0;
                    (yyval.decl).type = NewStringEmpty();
		    SwigType_add_rvalue_reference((yyval.decl).type);
                  }
#line 10376 "y.tab.c" /* yacc.c:1646  */
    break;

  case 364:
#line 5934 "parser.y" /* yacc.c:1646  */
    { 
		    (yyval.decl).type = NewStringEmpty();
                    SwigType_add_memberpointer((yyval.decl).type,(yyvsp[-1].str));
                    (yyval.decl).id = 0;
                    (yyval.decl).parms = 0;
		    (yyval.decl).have_parms = 0;
      	          }
#line 10388 "y.tab.c" /* yacc.c:1646  */
    break;

  case 365:
#line 5941 "parser.y" /* yacc.c:1646  */
    {
		    (yyval.decl).type = NewStringEmpty();
		    SwigType_add_memberpointer((yyval.decl).type, (yyvsp[-2].str));
		    SwigType_push((yyval.decl).type, (yyvsp[0].str));
		    (yyval.decl).id = 0;
		    (yyval.decl).parms = 0;
		    (yyval.decl).have_parms = 0;
		  }
#line 10401 "y.tab.c" /* yacc.c:1646  */
    break;

  case 366:
#line 5949 "parser.y" /* yacc.c:1646  */
    { 
		    SwigType *t = NewStringEmpty();
                    (yyval.decl).type = (yyvsp[-2].type);
		    (yyval.decl).id = 0;
		    (yyval.decl).parms = 0;
		    (yyval.decl).have_parms = 0;
		    SwigType_add_memberpointer(t,(yyvsp[-1].str));
		    SwigType_push((yyval.decl).type,t);
		    Delete(t);
                  }
#line 10416 "y.tab.c" /* yacc.c:1646  */
    break;

  case 367:
#line 5959 "parser.y" /* yacc.c:1646  */
    { 
		    (yyval.decl) = (yyvsp[0].decl);
		    SwigType_add_memberpointer((yyvsp[-3].type),(yyvsp[-2].str));
		    if ((yyval.decl).type) {
		      SwigType_push((yyvsp[-3].type),(yyval.decl).type);
		      Delete((yyval.decl).type);
		    }
		    (yyval.decl).type = (yyvsp[-3].type);
                  }
#line 10430 "y.tab.c" /* yacc.c:1646  */
    break;

  case 368:
#line 5970 "parser.y" /* yacc.c:1646  */
    { 
		    SwigType *t;
		    (yyval.decl) = (yyvsp[-2].decl);
		    t = NewStringEmpty();
		    SwigType_add_array(t,"");
		    if ((yyval.decl).type) {
		      SwigType_push(t,(yyval.decl).type);
		      Delete((yyval.decl).type);
		    }
		    (yyval.decl).type = t;
                  }
#line 10446 "y.tab.c" /* yacc.c:1646  */
    break;

  case 369:
#line 5981 "parser.y" /* yacc.c:1646  */
    { 
		    SwigType *t;
		    (yyval.decl) = (yyvsp[-3].decl);
		    t = NewStringEmpty();
		    SwigType_add_array(t,(yyvsp[-1].dtype).val);
		    if ((yyval.decl).type) {
		      SwigType_push(t,(yyval.decl).type);
		      Delete((yyval.decl).type);
		    }
		    (yyval.decl).type = t;
                  }
#line 10462 "y.tab.c" /* yacc.c:1646  */
    break;

  case 370:
#line 5992 "parser.y" /* yacc.c:1646  */
    { 
		    (yyval.decl).type = NewStringEmpty();
		    (yyval.decl).id = 0;
		    (yyval.decl).parms = 0;
		    (yyval.decl).have_parms = 0;
		    SwigType_add_array((yyval.decl).type,"");
                  }
#line 10474 "y.tab.c" /* yacc.c:1646  */
    break;

  case 371:
#line 5999 "parser.y" /* yacc.c:1646  */
    { 
		    (yyval.decl).type = NewStringEmpty();
		    (yyval.decl).id = 0;
		    (yyval.decl).parms = 0;
		    (yyval.decl).have_parms = 0;
		    SwigType_add_array((yyval.decl).type,(yyvsp[-1].dtype).val);
		  }
#line 10486 "y.tab.c" /* yacc.c:1646  */
    break;

  case 372:
#line 6006 "parser.y" /* yacc.c:1646  */
    {
                    (yyval.decl) = (yyvsp[-1].decl);
		  }
#line 10494 "y.tab.c" /* yacc.c:1646  */
    break;

  case 373:
#line 6009 "parser.y" /* yacc.c:1646  */
    {
		    SwigType *t;
                    (yyval.decl) = (yyvsp[-3].decl);
		    t = NewStringEmpty();
                    SwigType_add_function(t,(yyvsp[-1].pl));
		    if (!(yyval.decl).type) {
		      (yyval.decl).type = t;
		    } else {
		      SwigType_push(t,(yyval.decl).type);
		      Delete((yyval.decl).type);
		      (yyval.decl).type = t;
		    }
		    if (!(yyval.decl).have_parms) {
		      (yyval.decl).parms = (yyvsp[-1].pl);
		      (yyval.decl).have_parms = 1;
		    }
		  }
#line 10516 "y.tab.c" /* yacc.c:1646  */
    break;

  case 374:
#line 6026 "parser.y" /* yacc.c:1646  */
    {
		    SwigType *t;
                    (yyval.decl) = (yyvsp[-4].decl);
		    t = NewStringEmpty();
                    SwigType_add_function(t,(yyvsp[-2].pl));
		    SwigType_push(t, (yyvsp[0].dtype).qualifier);
		    if (!(yyval.decl).type) {
		      (yyval.decl).type = t;
		    } else {
		      SwigType_push(t,(yyval.decl).type);
		      Delete((yyval.decl).type);
		      (yyval.decl).type = t;
		    }
		    if (!(yyval.decl).have_parms) {
		      (yyval.decl).parms = (yyvsp[-2].pl);
		      (yyval.decl).have_parms = 1;
		    }
		  }
#line 10539 "y.tab.c" /* yacc.c:1646  */
    break;

  case 375:
#line 6044 "parser.y" /* yacc.c:1646  */
    {
                    (yyval.decl).type = NewStringEmpty();
                    SwigType_add_function((yyval.decl).type,(yyvsp[-1].pl));
		    (yyval.decl).parms = (yyvsp[-1].pl);
		    (yyval.decl).have_parms = 1;
		    (yyval.decl).id = 0;
                  }
#line 10551 "y.tab.c" /* yacc.c:1646  */
    break;

  case 376:
#line 6054 "parser.y" /* yacc.c:1646  */
    { 
             (yyval.type) = NewStringEmpty();
             SwigType_add_pointer((yyval.type));
	     SwigType_push((yyval.type),(yyvsp[-1].str));
	     SwigType_push((yyval.type),(yyvsp[0].type));
	     Delete((yyvsp[0].type));
           }
#line 10563 "y.tab.c" /* yacc.c:1646  */
    break;

  case 377:
#line 6061 "parser.y" /* yacc.c:1646  */
    {
	     (yyval.type) = NewStringEmpty();
	     SwigType_add_pointer((yyval.type));
	     SwigType_push((yyval.type),(yyvsp[0].type));
	     Delete((yyvsp[0].type));
	   }
#line 10574 "y.tab.c" /* yacc.c:1646  */
    break;

  case 378:
#line 6067 "parser.y" /* yacc.c:1646  */
    { 
	     (yyval.type) = NewStringEmpty();
	     SwigType_add_pointer((yyval.type));
	     SwigType_push((yyval.type),(yyvsp[0].str));
           }
#line 10584 "y.tab.c" /* yacc.c:1646  */
    break;

  case 379:
#line 6072 "parser.y" /* yacc.c:1646  */
    {
	     (yyval.type) = NewStringEmpty();
	     SwigType_add_pointer((yyval.type));
           }
#line 10593 "y.tab.c" /* yacc.c:1646  */
    break;

  case 380:
#line 6079 "parser.y" /* yacc.c:1646  */
    {
		  (yyval.dtype).qualifier = (yyvsp[0].str);
		  (yyval.dtype).refqualifier = 0;
	       }
#line 10602 "y.tab.c" /* yacc.c:1646  */
    break;

  case 381:
#line 6083 "parser.y" /* yacc.c:1646  */
    {
		  (yyval.dtype).qualifier = (yyvsp[-1].str);
		  (yyval.dtype).refqualifier = (yyvsp[0].str);
		  SwigType_push((yyval.dtype).qualifier, (yyvsp[0].str));
	       }
#line 10612 "y.tab.c" /* yacc.c:1646  */
    break;

  case 382:
#line 6088 "parser.y" /* yacc.c:1646  */
    {
		  (yyval.dtype).qualifier = NewStringEmpty();
		  (yyval.dtype).refqualifier = (yyvsp[0].str);
		  SwigType_push((yyval.dtype).qualifier, (yyvsp[0].str));
	       }
#line 10622 "y.tab.c" /* yacc.c:1646  */
    break;

  case 383:
#line 6095 "parser.y" /* yacc.c:1646  */
    {
	          (yyval.str) = NewStringEmpty();
	          SwigType_add_reference((yyval.str));
	       }
#line 10631 "y.tab.c" /* yacc.c:1646  */
    break;

  case 384:
#line 6099 "parser.y" /* yacc.c:1646  */
    {
	          (yyval.str) = NewStringEmpty();
	          SwigType_add_rvalue_reference((yyval.str));
	       }
#line 10640 "y.tab.c" /* yacc.c:1646  */
    break;

  case 385:
#line 6105 "parser.y" /* yacc.c:1646  */
    {
	          (yyval.str) = NewStringEmpty();
	          if ((yyvsp[0].id)) SwigType_add_qualifier((yyval.str),(yyvsp[0].id));
               }
#line 10649 "y.tab.c" /* yacc.c:1646  */
    break;

  case 386:
#line 6109 "parser.y" /* yacc.c:1646  */
    {
		  (yyval.str) = (yyvsp[0].str);
	          if ((yyvsp[-1].id)) SwigType_add_qualifier((yyval.str),(yyvsp[-1].id));
               }
#line 10658 "y.tab.c" /* yacc.c:1646  */
    break;

  case 387:
#line 6115 "parser.y" /* yacc.c:1646  */
    { (yyval.id) = "const"; }
#line 10664 "y.tab.c" /* yacc.c:1646  */
    break;

  case 388:
#line 6116 "parser.y" /* yacc.c:1646  */
    { (yyval.id) = "volatile"; }
#line 10670 "y.tab.c" /* yacc.c:1646  */
    break;

  case 389:
#line 6117 "parser.y" /* yacc.c:1646  */
    { (yyval.id) = 0; }
#line 10676 "y.tab.c" /* yacc.c:1646  */
    break;

  case 390:
#line 6123 "parser.y" /* yacc.c:1646  */
    {
                   (yyval.type) = (yyvsp[0].type);
                   Replace((yyval.type),"typename ","", DOH_REPLACE_ANY);
                }
#line 10685 "y.tab.c" /* yacc.c:1646  */
    break;

  case 391:
#line 6129 "parser.y" /* yacc.c:1646  */
    {
                   (yyval.type) = (yyvsp[0].type);
	           SwigType_push((yyval.type),(yyvsp[-1].str));
               }
#line 10694 "y.tab.c" /* yacc.c:1646  */
    break;

  case 392:
#line 6133 "parser.y" /* yacc.c:1646  */
    { (yyval.type) = (yyvsp[0].type); }
#line 10700 "y.tab.c" /* yacc.c:1646  */
    break;

  case 393:
#line 6134 "parser.y" /* yacc.c:1646  */
    {
		  (yyval.type) = (yyvsp[-1].type);
	          SwigType_push((yyval.type),(yyvsp[0].str));
	       }
#line 10709 "y.tab.c" /* yacc.c:1646  */
    break;

  case 394:
#line 6138 "parser.y" /* yacc.c:1646  */
    {
		  (yyval.type) = (yyvsp[-1].type);
	          SwigType_push((yyval.type),(yyvsp[0].str));
	          SwigType_push((yyval.type),(yyvsp[-2].str));
	       }
#line 10719 "y.tab.c" /* yacc.c:1646  */
    break;

  case 395:
#line 6145 "parser.y" /* yacc.c:1646  */
    { (yyval.type) = (yyvsp[0].type);
                  /* Printf(stdout,"primitive = '%s'\n", $$);*/
               }
#line 10727 "y.tab.c" /* yacc.c:1646  */
    break;

  case 396:
#line 6148 "parser.y" /* yacc.c:1646  */
    { (yyval.type) = (yyvsp[0].type); }
#line 10733 "y.tab.c" /* yacc.c:1646  */
    break;

  case 397:
#line 6149 "parser.y" /* yacc.c:1646  */
    { (yyval.type) = (yyvsp[0].type); }
#line 10739 "y.tab.c" /* yacc.c:1646  */
    break;

  case 398:
#line 6153 "parser.y" /* yacc.c:1646  */
    { (yyval.type) = NewStringf("enum %s", (yyvsp[0].str)); }
#line 10745 "y.tab.c" /* yacc.c:1646  */
    break;

  case 399:
#line 6154 "parser.y" /* yacc.c:1646  */
    { (yyval.type) = (yyvsp[0].type); }
#line 10751 "y.tab.c" /* yacc.c:1646  */
    break;

  case 400:
#line 6156 "parser.y" /* yacc.c:1646  */
    {
		  (yyval.type) = (yyvsp[0].str);
               }
#line 10759 "y.tab.c" /* yacc.c:1646  */
    break;

  case 401:
#line 6159 "parser.y" /* yacc.c:1646  */
    { 
		 (yyval.type) = NewStringf("%s %s", (yyvsp[-1].id), (yyvsp[0].str));
               }
#line 10767 "y.tab.c" /* yacc.c:1646  */
    break;

  case 402:
#line 6162 "parser.y" /* yacc.c:1646  */
    {
                 (yyval.type) = (yyvsp[0].type);
               }
#line 10775 "y.tab.c" /* yacc.c:1646  */
    break;

  case 403:
#line 6167 "parser.y" /* yacc.c:1646  */
    {
                 Node *n = Swig_symbol_clookup((yyvsp[-1].str),0);
                 if (!n) {
		   Swig_error(cparse_file, cparse_line, "Identifier %s not defined.\n", (yyvsp[-1].str));
                   (yyval.type) = (yyvsp[-1].str);
                 } else {
                   (yyval.type) = Getattr(n, "type");
                 }
               }
#line 10789 "y.tab.c" /* yacc.c:1646  */
    break;

  case 404:
#line 6178 "parser.y" /* yacc.c:1646  */
    {
		 if (!(yyvsp[0].ptype).type) (yyvsp[0].ptype).type = NewString("int");
		 if ((yyvsp[0].ptype).us) {
		   (yyval.type) = NewStringf("%s %s", (yyvsp[0].ptype).us, (yyvsp[0].ptype).type);
		   Delete((yyvsp[0].ptype).us);
                   Delete((yyvsp[0].ptype).type);
		 } else {
                   (yyval.type) = (yyvsp[0].ptype).type;
		 }
		 if (Cmp((yyval.type),"signed int") == 0) {
		   Delete((yyval.type));
		   (yyval.type) = NewString("int");
                 } else if (Cmp((yyval.type),"signed long") == 0) {
		   Delete((yyval.type));
                   (yyval.type) = NewString("long");
                 } else if (Cmp((yyval.type),"signed short") == 0) {
		   Delete((yyval.type));
		   (yyval.type) = NewString("short");
		 } else if (Cmp((yyval.type),"signed long long") == 0) {
		   Delete((yyval.type));
		   (yyval.type) = NewString("long long");
		 }
               }
#line 10817 "y.tab.c" /* yacc.c:1646  */
    break;

  case 405:
#line 6203 "parser.y" /* yacc.c:1646  */
    { 
                 (yyval.ptype) = (yyvsp[0].ptype);
               }
#line 10825 "y.tab.c" /* yacc.c:1646  */
    break;

  case 406:
#line 6206 "parser.y" /* yacc.c:1646  */
    {
                    if ((yyvsp[-1].ptype).us && (yyvsp[0].ptype).us) {
		      Swig_error(cparse_file, cparse_line, "Extra %s specifier.\n", (yyvsp[0].ptype).us);
		    }
                    (yyval.ptype) = (yyvsp[0].ptype);
                    if ((yyvsp[-1].ptype).us) (yyval.ptype).us = (yyvsp[-1].ptype).us;
		    if ((yyvsp[-1].ptype).type) {
		      if (!(yyvsp[0].ptype).type) (yyval.ptype).type = (yyvsp[-1].ptype).type;
		      else {
			int err = 0;
			if ((Cmp((yyvsp[-1].ptype).type,"long") == 0)) {
			  if ((Cmp((yyvsp[0].ptype).type,"long") == 0) || (Strncmp((yyvsp[0].ptype).type,"double",6) == 0)) {
			    (yyval.ptype).type = NewStringf("long %s", (yyvsp[0].ptype).type);
			  } else if (Cmp((yyvsp[0].ptype).type,"int") == 0) {
			    (yyval.ptype).type = (yyvsp[-1].ptype).type;
			  } else {
			    err = 1;
			  }
			} else if ((Cmp((yyvsp[-1].ptype).type,"short")) == 0) {
			  if (Cmp((yyvsp[0].ptype).type,"int") == 0) {
			    (yyval.ptype).type = (yyvsp[-1].ptype).type;
			  } else {
			    err = 1;
			  }
			} else if (Cmp((yyvsp[-1].ptype).type,"int") == 0) {
			  (yyval.ptype).type = (yyvsp[0].ptype).type;
			} else if (Cmp((yyvsp[-1].ptype).type,"double") == 0) {
			  if (Cmp((yyvsp[0].ptype).type,"long") == 0) {
			    (yyval.ptype).type = NewString("long double");
			  } else if (Cmp((yyvsp[0].ptype).type,"complex") == 0) {
			    (yyval.ptype).type = NewString("double complex");
			  } else {
			    err = 1;
			  }
			} else if (Cmp((yyvsp[-1].ptype).type,"float") == 0) {
			  if (Cmp((yyvsp[0].ptype).type,"complex") == 0) {
			    (yyval.ptype).type = NewString("float complex");
			  } else {
			    err = 1;
			  }
			} else if (Cmp((yyvsp[-1].ptype).type,"complex") == 0) {
			  (yyval.ptype).type = NewStringf("%s complex", (yyvsp[0].ptype).type);
			} else {
			  err = 1;
			}
			if (err) {
			  Swig_error(cparse_file, cparse_line, "Extra %s specifier.\n", (yyvsp[-1].ptype).type);
			}
		      }
		    }
               }
#line 10881 "y.tab.c" /* yacc.c:1646  */
    break;

  case 407:
#line 6260 "parser.y" /* yacc.c:1646  */
    { 
		    (yyval.ptype).type = NewString("int");
                    (yyval.ptype).us = 0;
               }
#line 10890 "y.tab.c" /* yacc.c:1646  */
    break;

  case 408:
#line 6264 "parser.y" /* yacc.c:1646  */
    { 
                    (yyval.ptype).type = NewString("short");
                    (yyval.ptype).us = 0;
                }
#line 10899 "y.tab.c" /* yacc.c:1646  */
    break;

  case 409:
#line 6268 "parser.y" /* yacc.c:1646  */
    { 
                    (yyval.ptype).type = NewString("long");
                    (yyval.ptype).us = 0;
                }
#line 10908 "y.tab.c" /* yacc.c:1646  */
    break;

  case 410:
#line 6272 "parser.y" /* yacc.c:1646  */
    { 
                    (yyval.ptype).type = NewString("char");
                    (yyval.ptype).us = 0;
                }
#line 10917 "y.tab.c" /* yacc.c:1646  */
    break;

  case 411:
#line 6276 "parser.y" /* yacc.c:1646  */
    { 
                    (yyval.ptype).type = NewString("wchar_t");
                    (yyval.ptype).us = 0;
                }
#line 10926 "y.tab.c" /* yacc.c:1646  */
    break;

  case 412:
#line 6280 "parser.y" /* yacc.c:1646  */
    { 
                    (yyval.ptype).type = NewString("float");
                    (yyval.ptype).us = 0;
                }
#line 10935 "y.tab.c" /* yacc.c:1646  */
    break;

  case 413:
#line 6284 "parser.y" /* yacc.c:1646  */
    { 
                    (yyval.ptype).type = NewString("double");
                    (yyval.ptype).us = 0;
                }
#line 10944 "y.tab.c" /* yacc.c:1646  */
    break;

  case 414:
#line 6288 "parser.y" /* yacc.c:1646  */
    { 
                    (yyval.ptype).us = NewString("signed");
                    (yyval.ptype).type = 0;
                }
#line 10953 "y.tab.c" /* yacc.c:1646  */
    break;

  case 415:
#line 6292 "parser.y" /* yacc.c:1646  */
    { 
                    (yyval.ptype).us = NewString("unsigned");
                    (yyval.ptype).type = 0;
                }
#line 10962 "y.tab.c" /* yacc.c:1646  */
    break;

  case 416:
#line 6296 "parser.y" /* yacc.c:1646  */
    { 
                    (yyval.ptype).type = NewString("complex");
                    (yyval.ptype).us = 0;
                }
#line 10971 "y.tab.c" /* yacc.c:1646  */
    break;

  case 417:
#line 6300 "parser.y" /* yacc.c:1646  */
    { 
                    (yyval.ptype).type = NewString("__int8");
                    (yyval.ptype).us = 0;
                }
#line 10980 "y.tab.c" /* yacc.c:1646  */
    break;

  case 418:
#line 6304 "parser.y" /* yacc.c:1646  */
    { 
                    (yyval.ptype).type = NewString("__int16");
                    (yyval.ptype).us = 0;
                }
#line 10989 "y.tab.c" /* yacc.c:1646  */
    break;

  case 419:
#line 6308 "parser.y" /* yacc.c:1646  */
    { 
                    (yyval.ptype).type = NewString("__int32");
                    (yyval.ptype).us = 0;
                }
#line 10998 "y.tab.c" /* yacc.c:1646  */
    break;

  case 420:
#line 6312 "parser.y" /* yacc.c:1646  */
    { 
                    (yyval.ptype).type = NewString("__int64");
                    (yyval.ptype).us = 0;
                }
#line 11007 "y.tab.c" /* yacc.c:1646  */
    break;

  case 421:
#line 6318 "parser.y" /* yacc.c:1646  */
    { /* scanner_check_typedef(); */ }
#line 11013 "y.tab.c" /* yacc.c:1646  */
    break;

  case 422:
#line 6318 "parser.y" /* yacc.c:1646  */
    {
                   (yyval.dtype) = (yyvsp[0].dtype);
		   if ((yyval.dtype).type == T_STRING) {
		     (yyval.dtype).rawval = NewStringf("\"%(escape)s\"",(yyval.dtype).val);
		   } else if ((yyval.dtype).type != T_CHAR && (yyval.dtype).type != T_WSTRING && (yyval.dtype).type != T_WCHAR) {
		     (yyval.dtype).rawval = NewStringf("%s", (yyval.dtype).val);
		   }
		   (yyval.dtype).qualifier = 0;
		   (yyval.dtype).refqualifier = 0;
		   (yyval.dtype).bitfield = 0;
		   (yyval.dtype).throws = 0;
		   (yyval.dtype).throwf = 0;
		   (yyval.dtype).nexcept = 0;
		   (yyval.dtype).final = 0;
		   scanner_ignore_typedef();
                }
#line 11034 "y.tab.c" /* yacc.c:1646  */
    break;

  case 423:
#line 6334 "parser.y" /* yacc.c:1646  */
    {
		  (yyval.dtype) = (yyvsp[0].dtype);
		}
#line 11042 "y.tab.c" /* yacc.c:1646  */
    break;

  case 424:
#line 6339 "parser.y" /* yacc.c:1646  */
    {
		  (yyval.dtype) = (yyvsp[0].dtype);
		}
#line 11050 "y.tab.c" /* yacc.c:1646  */
    break;

  case 425:
#line 6342 "parser.y" /* yacc.c:1646  */
    {
		  (yyval.dtype) = (yyvsp[0].dtype);
		}
#line 11058 "y.tab.c" /* yacc.c:1646  */
    break;

  case 426:
#line 6348 "parser.y" /* yacc.c:1646  */
    {
		  (yyval.dtype).val = NewString("delete");
		  (yyval.dtype).rawval = 0;
		  (yyval.dtype).type = T_STRING;
		  (yyval.dtype).qualifier = 0;
		  (yyval.dtype).refqualifier = 0;
		  (yyval.dtype).bitfield = 0;
		  (yyval.dtype).throws = 0;
		  (yyval.dtype).throwf = 0;
		  (yyval.dtype).nexcept = 0;
		  (yyval.dtype).final = 0;
		}
#line 11075 "y.tab.c" /* yacc.c:1646  */
    break;

  case 427:
#line 6363 "parser.y" /* yacc.c:1646  */
    {
		  (yyval.dtype).val = NewString("default");
		  (yyval.dtype).rawval = 0;
		  (yyval.dtype).type = T_STRING;
		  (yyval.dtype).qualifier = 0;
		  (yyval.dtype).refqualifier = 0;
		  (yyval.dtype).bitfield = 0;
		  (yyval.dtype).throws = 0;
		  (yyval.dtype).throwf = 0;
		  (yyval.dtype).nexcept = 0;
		  (yyval.dtype).final = 0;
		}
#line 11092 "y.tab.c" /* yacc.c:1646  */
    break;

  case 428:
#line 6379 "parser.y" /* yacc.c:1646  */
    { (yyval.id) = (yyvsp[0].id); }
#line 11098 "y.tab.c" /* yacc.c:1646  */
    break;

  case 429:
#line 6380 "parser.y" /* yacc.c:1646  */
    { (yyval.id) = (char *) 0;}
#line 11104 "y.tab.c" /* yacc.c:1646  */
    break;

  case 434:
#line 6399 "parser.y" /* yacc.c:1646  */
    {
		  Setattr((yyvsp[0].node),"_last",(yyvsp[0].node));
		  (yyval.node) = (yyvsp[0].node);
		}
#line 11113 "y.tab.c" /* yacc.c:1646  */
    break;

  case 435:
#line 6403 "parser.y" /* yacc.c:1646  */
    {
		  Setattr((yyvsp[-1].node),"_last",(yyvsp[-1].node));
		  set_comment((yyvsp[-1].node), (yyvsp[0].str));
		  (yyval.node) = (yyvsp[-1].node);
		}
#line 11123 "y.tab.c" /* yacc.c:1646  */
    break;

  case 436:
#line 6408 "parser.y" /* yacc.c:1646  */
    {
		  if ((yyvsp[0].node)) {
		    set_nextSibling((yyvsp[-2].node), (yyvsp[0].node));
		    Setattr((yyvsp[-2].node),"_last",Getattr((yyvsp[0].node),"_last"));
		    Setattr((yyvsp[0].node),"_last",NULL);
		  } else {
		    Setattr((yyvsp[-2].node),"_last",(yyvsp[-2].node));
		  }
		  (yyval.node) = (yyvsp[-2].node);
		}
#line 11138 "y.tab.c" /* yacc.c:1646  */
    break;

  case 437:
#line 6418 "parser.y" /* yacc.c:1646  */
    {
		  if ((yyvsp[0].node)) {
		    set_nextSibling((yyvsp[-3].node), (yyvsp[0].node));
		    Setattr((yyvsp[-3].node),"_last",Getattr((yyvsp[0].node),"_last"));
		    Setattr((yyvsp[0].node),"_last",NULL);
		  } else {
		    Setattr((yyvsp[-3].node),"_last",(yyvsp[-3].node));
		  }
		  set_comment((yyvsp[-3].node), (yyvsp[-1].str));
		  (yyval.node) = (yyvsp[-3].node);
		}
#line 11154 "y.tab.c" /* yacc.c:1646  */
    break;

  case 438:
#line 6429 "parser.y" /* yacc.c:1646  */
    {
		  (yyval.node) = 0;
		}
#line 11162 "y.tab.c" /* yacc.c:1646  */
    break;

  case 439:
#line 6434 "parser.y" /* yacc.c:1646  */
    {
		  (yyval.node) = (yyvsp[-1].node);
		}
#line 11170 "y.tab.c" /* yacc.c:1646  */
    break;

  case 440:
#line 6439 "parser.y" /* yacc.c:1646  */
    {
		  (yyval.node) = (yyvsp[0].node);
		}
#line 11178 "y.tab.c" /* yacc.c:1646  */
    break;

  case 441:
#line 6442 "parser.y" /* yacc.c:1646  */
    {
		  (yyval.node) = (yyvsp[0].node);
		  set_comment((yyvsp[0].node), (yyvsp[-1].str));
		}
#line 11187 "y.tab.c" /* yacc.c:1646  */
    break;

  case 442:
#line 6448 "parser.y" /* yacc.c:1646  */
    {
		   SwigType *type = NewSwigType(T_INT);
		   (yyval.node) = new_node("enumitem");
		   Setattr((yyval.node),"name",(yyvsp[0].id));
		   Setattr((yyval.node),"type",type);
		   SetFlag((yyval.node),"feature:immutable");
		   Delete(type);
		 }
#line 11200 "y.tab.c" /* yacc.c:1646  */
    break;

  case 443:
#line 6456 "parser.y" /* yacc.c:1646  */
    {
		   SwigType *type = NewSwigType((yyvsp[0].dtype).type == T_BOOL ? T_BOOL : ((yyvsp[0].dtype).type == T_CHAR ? T_CHAR : T_INT));
		   (yyval.node) = new_node("enumitem");
		   Setattr((yyval.node),"name",(yyvsp[-2].id));
		   Setattr((yyval.node),"type",type);
		   SetFlag((yyval.node),"feature:immutable");
		   Setattr((yyval.node),"enumvalue", (yyvsp[0].dtype).val);
		   Setattr((yyval.node),"value",(yyvsp[-2].id));
		   Delete(type);
                 }
#line 11215 "y.tab.c" /* yacc.c:1646  */
    break;

  case 444:
#line 6468 "parser.y" /* yacc.c:1646  */
    {
                   (yyval.dtype) = (yyvsp[0].dtype);
		   if (((yyval.dtype).type != T_INT) && ((yyval.dtype).type != T_UINT) &&
		       ((yyval.dtype).type != T_LONG) && ((yyval.dtype).type != T_ULONG) &&
		       ((yyval.dtype).type != T_LONGLONG) && ((yyval.dtype).type != T_ULONGLONG) &&
		       ((yyval.dtype).type != T_SHORT) && ((yyval.dtype).type != T_USHORT) &&
		       ((yyval.dtype).type != T_SCHAR) && ((yyval.dtype).type != T_UCHAR) &&
		       ((yyval.dtype).type != T_CHAR) && ((yyval.dtype).type != T_BOOL)) {
		     Swig_error(cparse_file,cparse_line,"Type error. Expecting an integral type\n");
		   }
                }
#line 11231 "y.tab.c" /* yacc.c:1646  */
    break;

  case 445:
#line 6483 "parser.y" /* yacc.c:1646  */
    { (yyval.dtype) = (yyvsp[0].dtype); }
#line 11237 "y.tab.c" /* yacc.c:1646  */
    break;

  case 446:
#line 6484 "parser.y" /* yacc.c:1646  */
    {
		 Node *n;
		 (yyval.dtype).val = (yyvsp[0].type);
		 (yyval.dtype).type = T_INT;
		 /* Check if value is in scope */
		 n = Swig_symbol_clookup((yyvsp[0].type),0);
		 if (n) {
                   /* A band-aid for enum values used in expressions. */
                   if (Strcmp(nodeType(n),"enumitem") == 0) {
                     String *q = Swig_symbol_qualified(n);
                     if (q) {
                       (yyval.dtype).val = NewStringf("%s::%s", q, Getattr(n,"name"));
                       Delete(q);
                     }
                   }
		 }
               }
#line 11259 "y.tab.c" /* yacc.c:1646  */
    break;

  case 447:
#line 6504 "parser.y" /* yacc.c:1646  */
    {
		 (yyval.dtype).val = NewStringf("%s->%s", (yyvsp[-2].id), (yyvsp[0].id));
		 (yyval.dtype).type = 0;
	       }
#line 11268 "y.tab.c" /* yacc.c:1646  */
    break;

  case 448:
#line 6508 "parser.y" /* yacc.c:1646  */
    {
		 (yyval.dtype) = (yyvsp[-2].dtype);
		 Printf((yyval.dtype).val, "->%s", (yyvsp[0].id));
	       }
#line 11277 "y.tab.c" /* yacc.c:1646  */
    break;

  case 449:
#line 6518 "parser.y" /* yacc.c:1646  */
    {
		 (yyval.dtype) = (yyvsp[-2].dtype);
		 Printf((yyval.dtype).val, ".%s", (yyvsp[0].id));
	       }
#line 11286 "y.tab.c" /* yacc.c:1646  */
    break;

  case 450:
#line 6524 "parser.y" /* yacc.c:1646  */
    {
		    (yyval.dtype) = (yyvsp[0].dtype);
               }
#line 11294 "y.tab.c" /* yacc.c:1646  */
    break;

  case 451:
#line 6527 "parser.y" /* yacc.c:1646  */
    {
		    (yyval.dtype) = (yyvsp[0].dtype);
               }
#line 11302 "y.tab.c" /* yacc.c:1646  */
    break;

  case 452:
#line 6530 "parser.y" /* yacc.c:1646  */
    {
		    (yyval.dtype).val = (yyvsp[0].str);
                    (yyval.dtype).type = T_STRING;
               }
#line 11311 "y.tab.c" /* yacc.c:1646  */
    break;

  case 453:
#line 6534 "parser.y" /* yacc.c:1646  */
    {
		  SwigType_push((yyvsp[-2].type),(yyvsp[-1].decl).type);
		  (yyval.dtype).val = NewStringf("sizeof(%s)",SwigType_str((yyvsp[-2].type),0));
		  (yyval.dtype).type = T_ULONG;
               }
#line 11321 "y.tab.c" /* yacc.c:1646  */
    break;

  case 454:
#line 6539 "parser.y" /* yacc.c:1646  */
    {
		  SwigType_push((yyvsp[-2].type),(yyvsp[-1].decl).type);
		  (yyval.dtype).val = NewStringf("sizeof...(%s)",SwigType_str((yyvsp[-2].type),0));
		  (yyval.dtype).type = T_ULONG;
               }
#line 11331 "y.tab.c" /* yacc.c:1646  */
    break;

  case 455:
#line 6544 "parser.y" /* yacc.c:1646  */
    { (yyval.dtype) = (yyvsp[0].dtype); }
#line 11337 "y.tab.c" /* yacc.c:1646  */
    break;

  case 456:
#line 6545 "parser.y" /* yacc.c:1646  */
    {
		    (yyval.dtype).val = (yyvsp[0].str);
		    (yyval.dtype).rawval = NewStringf("L\"%s\"", (yyval.dtype).val);
                    (yyval.dtype).type = T_WSTRING;
	       }
#line 11347 "y.tab.c" /* yacc.c:1646  */
    break;

  case 457:
#line 6550 "parser.y" /* yacc.c:1646  */
    {
		  (yyval.dtype).val = NewString((yyvsp[0].str));
		  if (Len((yyval.dtype).val)) {
		    (yyval.dtype).rawval = NewStringf("'%(escape)s'", (yyval.dtype).val);
		  } else {
		    (yyval.dtype).rawval = NewString("'\\0'");
		  }
		  (yyval.dtype).type = T_CHAR;
		  (yyval.dtype).bitfield = 0;
		  (yyval.dtype).throws = 0;
		  (yyval.dtype).throwf = 0;
		  (yyval.dtype).nexcept = 0;
		  (yyval.dtype).final = 0;
	       }
#line 11366 "y.tab.c" /* yacc.c:1646  */
    break;

  case 458:
#line 6564 "parser.y" /* yacc.c:1646  */
    {
		  (yyval.dtype).val = NewString((yyvsp[0].str));
		  if (Len((yyval.dtype).val)) {
		    (yyval.dtype).rawval = NewStringf("L\'%s\'", (yyval.dtype).val);
		  } else {
		    (yyval.dtype).rawval = NewString("L'\\0'");
		  }
		  (yyval.dtype).type = T_WCHAR;
		  (yyval.dtype).bitfield = 0;
		  (yyval.dtype).throws = 0;
		  (yyval.dtype).throwf = 0;
		  (yyval.dtype).nexcept = 0;
		  (yyval.dtype).final = 0;
	       }
#line 11385 "y.tab.c" /* yacc.c:1646  */
    break;

  case 459:
#line 6580 "parser.y" /* yacc.c:1646  */
    {
		    (yyval.dtype).val = NewStringf("(%s)",(yyvsp[-1].dtype).val);
		    if ((yyvsp[-1].dtype).rawval) {
		      (yyval.dtype).rawval = NewStringf("(%s)",(yyvsp[-1].dtype).rawval);
		    }
		    (yyval.dtype).type = (yyvsp[-1].dtype).type;
	       }
#line 11397 "y.tab.c" /* yacc.c:1646  */
    break;

  case 460:
#line 6590 "parser.y" /* yacc.c:1646  */
    {
                 (yyval.dtype) = (yyvsp[0].dtype);
		 if ((yyvsp[0].dtype).type != T_STRING) {
		   switch ((yyvsp[-2].dtype).type) {
		     case T_FLOAT:
		     case T_DOUBLE:
		     case T_LONGDOUBLE:
		     case T_FLTCPLX:
		     case T_DBLCPLX:
		       (yyval.dtype).val = NewStringf("(%s)%s", (yyvsp[-2].dtype).val, (yyvsp[0].dtype).val); /* SwigType_str and decimal points don't mix! */
		       break;
		     default:
		       (yyval.dtype).val = NewStringf("(%s) %s", SwigType_str((yyvsp[-2].dtype).val,0), (yyvsp[0].dtype).val);
		       break;
		   }
		 }
		 (yyval.dtype).type = promote((yyvsp[-2].dtype).type, (yyvsp[0].dtype).type);
 	       }
#line 11420 "y.tab.c" /* yacc.c:1646  */
    break;

  case 461:
#line 6608 "parser.y" /* yacc.c:1646  */
    {
                 (yyval.dtype) = (yyvsp[0].dtype);
		 if ((yyvsp[0].dtype).type != T_STRING) {
		   SwigType_push((yyvsp[-3].dtype).val,(yyvsp[-2].type));
		   (yyval.dtype).val = NewStringf("(%s) %s", SwigType_str((yyvsp[-3].dtype).val,0), (yyvsp[0].dtype).val);
		 }
 	       }
#line 11432 "y.tab.c" /* yacc.c:1646  */
    break;

  case 462:
#line 6615 "parser.y" /* yacc.c:1646  */
    {
                 (yyval.dtype) = (yyvsp[0].dtype);
		 if ((yyvsp[0].dtype).type != T_STRING) {
		   SwigType_add_reference((yyvsp[-3].dtype).val);
		   (yyval.dtype).val = NewStringf("(%s) %s", SwigType_str((yyvsp[-3].dtype).val,0), (yyvsp[0].dtype).val);
		 }
 	       }
#line 11444 "y.tab.c" /* yacc.c:1646  */
    break;

  case 463:
#line 6622 "parser.y" /* yacc.c:1646  */
    {
                 (yyval.dtype) = (yyvsp[0].dtype);
		 if ((yyvsp[0].dtype).type != T_STRING) {
		   SwigType_add_rvalue_reference((yyvsp[-3].dtype).val);
		   (yyval.dtype).val = NewStringf("(%s) %s", SwigType_str((yyvsp[-3].dtype).val,0), (yyvsp[0].dtype).val);
		 }
 	       }
#line 11456 "y.tab.c" /* yacc.c:1646  */
    break;

  case 464:
#line 6629 "parser.y" /* yacc.c:1646  */
    {
                 (yyval.dtype) = (yyvsp[0].dtype);
		 if ((yyvsp[0].dtype).type != T_STRING) {
		   SwigType_push((yyvsp[-4].dtype).val,(yyvsp[-3].type));
		   SwigType_add_reference((yyvsp[-4].dtype).val);
		   (yyval.dtype).val = NewStringf("(%s) %s", SwigType_str((yyvsp[-4].dtype).val,0), (yyvsp[0].dtype).val);
		 }
 	       }
#line 11469 "y.tab.c" /* yacc.c:1646  */
    break;

  case 465:
#line 6637 "parser.y" /* yacc.c:1646  */
    {
                 (yyval.dtype) = (yyvsp[0].dtype);
		 if ((yyvsp[0].dtype).type != T_STRING) {
		   SwigType_push((yyvsp[-4].dtype).val,(yyvsp[-3].type));
		   SwigType_add_rvalue_reference((yyvsp[-4].dtype).val);
		   (yyval.dtype).val = NewStringf("(%s) %s", SwigType_str((yyvsp[-4].dtype).val,0), (yyvsp[0].dtype).val);
		 }
 	       }
#line 11482 "y.tab.c" /* yacc.c:1646  */
    break;

  case 466:
#line 6645 "parser.y" /* yacc.c:1646  */
    {
		 (yyval.dtype) = (yyvsp[0].dtype);
                 (yyval.dtype).val = NewStringf("&%s",(yyvsp[0].dtype).val);
	       }
#line 11491 "y.tab.c" /* yacc.c:1646  */
    break;

  case 467:
#line 6649 "parser.y" /* yacc.c:1646  */
    {
		 (yyval.dtype) = (yyvsp[0].dtype);
                 (yyval.dtype).val = NewStringf("&&%s",(yyvsp[0].dtype).val);
	       }
#line 11500 "y.tab.c" /* yacc.c:1646  */
    break;

  case 468:
#line 6653 "parser.y" /* yacc.c:1646  */
    {
		 (yyval.dtype) = (yyvsp[0].dtype);
                 (yyval.dtype).val = NewStringf("*%s",(yyvsp[0].dtype).val);
	       }
#line 11509 "y.tab.c" /* yacc.c:1646  */
    break;

  case 469:
#line 6659 "parser.y" /* yacc.c:1646  */
    { (yyval.dtype) = (yyvsp[0].dtype); }
#line 11515 "y.tab.c" /* yacc.c:1646  */
    break;

  case 470:
#line 6660 "parser.y" /* yacc.c:1646  */
    { (yyval.dtype) = (yyvsp[0].dtype); }
#line 11521 "y.tab.c" /* yacc.c:1646  */
    break;

  case 471:
#line 6661 "parser.y" /* yacc.c:1646  */
    { (yyval.dtype) = (yyvsp[0].dtype); }
#line 11527 "y.tab.c" /* yacc.c:1646  */
    break;

  case 472:
#line 6662 "parser.y" /* yacc.c:1646  */
    { (yyval.dtype) = (yyvsp[0].dtype); }
#line 11533 "y.tab.c" /* yacc.c:1646  */
    break;

  case 473:
#line 6663 "parser.y" /* yacc.c:1646  */
    { (yyval.dtype) = (yyvsp[0].dtype); }
#line 11539 "y.tab.c" /* yacc.c:1646  */
    break;

  case 474:
#line 6664 "parser.y" /* yacc.c:1646  */
    { (yyval.dtype) = (yyvsp[0].dtype); }
#line 11545 "y.tab.c" /* yacc.c:1646  */
    break;

  case 475:
#line 6665 "parser.y" /* yacc.c:1646  */
    { (yyval.dtype) = (yyvsp[0].dtype); }
#line 11551 "y.tab.c" /* yacc.c:1646  */
    break;

  case 476:
#line 6666 "parser.y" /* yacc.c:1646  */
    { (yyval.dtype) = (yyvsp[0].dtype); }
#line 11557 "y.tab.c" /* yacc.c:1646  */
    break;

  case 477:
#line 6669 "parser.y" /* yacc.c:1646  */
    {
		 (yyval.dtype).val = NewStringf("%s+%s", COMPOUND_EXPR_VAL((yyvsp[-2].dtype)),COMPOUND_EXPR_VAL((yyvsp[0].dtype)));
		 (yyval.dtype).type = promote((yyvsp[-2].dtype).type,(yyvsp[0].dtype).type);
	       }
#line 11566 "y.tab.c" /* yacc.c:1646  */
    break;

  case 478:
#line 6673 "parser.y" /* yacc.c:1646  */
    {
		 (yyval.dtype).val = NewStringf("%s-%s",COMPOUND_EXPR_VAL((yyvsp[-2].dtype)),COMPOUND_EXPR_VAL((yyvsp[0].dtype)));
		 (yyval.dtype).type = promote((yyvsp[-2].dtype).type,(yyvsp[0].dtype).type);
	       }
#line 11575 "y.tab.c" /* yacc.c:1646  */
    break;

  case 479:
#line 6677 "parser.y" /* yacc.c:1646  */
    {
		 (yyval.dtype).val = NewStringf("%s*%s",COMPOUND_EXPR_VAL((yyvsp[-2].dtype)),COMPOUND_EXPR_VAL((yyvsp[0].dtype)));
		 (yyval.dtype).type = promote((yyvsp[-2].dtype).type,(yyvsp[0].dtype).type);
	       }
#line 11584 "y.tab.c" /* yacc.c:1646  */
    break;

  case 480:
#line 6681 "parser.y" /* yacc.c:1646  */
    {
		 (yyval.dtype).val = NewStringf("%s/%s",COMPOUND_EXPR_VAL((yyvsp[-2].dtype)),COMPOUND_EXPR_VAL((yyvsp[0].dtype)));
		 (yyval.dtype).type = promote((yyvsp[-2].dtype).type,(yyvsp[0].dtype).type);
	       }
#line 11593 "y.tab.c" /* yacc.c:1646  */
    break;

  case 481:
#line 6685 "parser.y" /* yacc.c:1646  */
    {
		 (yyval.dtype).val = NewStringf("%s%%%s",COMPOUND_EXPR_VAL((yyvsp[-2].dtype)),COMPOUND_EXPR_VAL((yyvsp[0].dtype)));
		 (yyval.dtype).type = promote((yyvsp[-2].dtype).type,(yyvsp[0].dtype).type);
	       }
#line 11602 "y.tab.c" /* yacc.c:1646  */
    break;

  case 482:
#line 6689 "parser.y" /* yacc.c:1646  */
    {
		 (yyval.dtype).val = NewStringf("%s&%s",COMPOUND_EXPR_VAL((yyvsp[-2].dtype)),COMPOUND_EXPR_VAL((yyvsp[0].dtype)));
		 (yyval.dtype).type = promote((yyvsp[-2].dtype).type,(yyvsp[0].dtype).type);
	       }
#line 11611 "y.tab.c" /* yacc.c:1646  */
    break;

  case 483:
#line 6693 "parser.y" /* yacc.c:1646  */
    {
		 (yyval.dtype).val = NewStringf("%s|%s",COMPOUND_EXPR_VAL((yyvsp[-2].dtype)),COMPOUND_EXPR_VAL((yyvsp[0].dtype)));
		 (yyval.dtype).type = promote((yyvsp[-2].dtype).type,(yyvsp[0].dtype).type);
	       }
#line 11620 "y.tab.c" /* yacc.c:1646  */
    break;

  case 484:
#line 6697 "parser.y" /* yacc.c:1646  */
    {
		 (yyval.dtype).val = NewStringf("%s^%s",COMPOUND_EXPR_VAL((yyvsp[-2].dtype)),COMPOUND_EXPR_VAL((yyvsp[0].dtype)));
		 (yyval.dtype).type = promote((yyvsp[-2].dtype).type,(yyvsp[0].dtype).type);
	       }
#line 11629 "y.tab.c" /* yacc.c:1646  */
    break;

  case 485:
#line 6701 "parser.y" /* yacc.c:1646  */
    {
		 (yyval.dtype).val = NewStringf("%s << %s",COMPOUND_EXPR_VAL((yyvsp[-2].dtype)),COMPOUND_EXPR_VAL((yyvsp[0].dtype)));
		 (yyval.dtype).type = promote_type((yyvsp[-2].dtype).type);
	       }
#line 11638 "y.tab.c" /* yacc.c:1646  */
    break;

  case 486:
#line 6705 "parser.y" /* yacc.c:1646  */
    {
		 (yyval.dtype).val = NewStringf("%s >> %s",COMPOUND_EXPR_VAL((yyvsp[-2].dtype)),COMPOUND_EXPR_VAL((yyvsp[0].dtype)));
		 (yyval.dtype).type = promote_type((yyvsp[-2].dtype).type);
	       }
#line 11647 "y.tab.c" /* yacc.c:1646  */
    break;

  case 487:
#line 6709 "parser.y" /* yacc.c:1646  */
    {
		 (yyval.dtype).val = NewStringf("%s&&%s",COMPOUND_EXPR_VAL((yyvsp[-2].dtype)),COMPOUND_EXPR_VAL((yyvsp[0].dtype)));
		 (yyval.dtype).type = cparse_cplusplus ? T_BOOL : T_INT;
	       }
#line 11656 "y.tab.c" /* yacc.c:1646  */
    break;

  case 488:
#line 6713 "parser.y" /* yacc.c:1646  */
    {
		 (yyval.dtype).val = NewStringf("%s||%s",COMPOUND_EXPR_VAL((yyvsp[-2].dtype)),COMPOUND_EXPR_VAL((yyvsp[0].dtype)));
		 (yyval.dtype).type = cparse_cplusplus ? T_BOOL : T_INT;
	       }
#line 11665 "y.tab.c" /* yacc.c:1646  */
    break;

  case 489:
#line 6717 "parser.y" /* yacc.c:1646  */
    {
		 (yyval.dtype).val = NewStringf("%s==%s",COMPOUND_EXPR_VAL((yyvsp[-2].dtype)),COMPOUND_EXPR_VAL((yyvsp[0].dtype)));
		 (yyval.dtype).type = cparse_cplusplus ? T_BOOL : T_INT;
	       }
#line 11674 "y.tab.c" /* yacc.c:1646  */
    break;

  case 490:
#line 6721 "parser.y" /* yacc.c:1646  */
    {
		 (yyval.dtype).val = NewStringf("%s!=%s",COMPOUND_EXPR_VAL((yyvsp[-2].dtype)),COMPOUND_EXPR_VAL((yyvsp[0].dtype)));
		 (yyval.dtype).type = cparse_cplusplus ? T_BOOL : T_INT;
	       }
#line 11683 "y.tab.c" /* yacc.c:1646  */
    break;

  case 491:
#line 6735 "parser.y" /* yacc.c:1646  */
    {
		 (yyval.dtype).val = NewStringf("%s >= %s", COMPOUND_EXPR_VAL((yyvsp[-2].dtype)), COMPOUND_EXPR_VAL((yyvsp[0].dtype)));
		 (yyval.dtype).type = cparse_cplusplus ? T_BOOL : T_INT;
	       }
#line 11692 "y.tab.c" /* yacc.c:1646  */
    break;

  case 492:
#line 6739 "parser.y" /* yacc.c:1646  */
    {
		 (yyval.dtype).val = NewStringf("%s <= %s", COMPOUND_EXPR_VAL((yyvsp[-2].dtype)), COMPOUND_EXPR_VAL((yyvsp[0].dtype)));
		 (yyval.dtype).type = cparse_cplusplus ? T_BOOL : T_INT;
	       }
#line 11701 "y.tab.c" /* yacc.c:1646  */
    break;

  case 493:
#line 6743 "parser.y" /* yacc.c:1646  */
    {
		 (yyval.dtype).val = NewStringf("%s?%s:%s", COMPOUND_EXPR_VAL((yyvsp[-4].dtype)), COMPOUND_EXPR_VAL((yyvsp[-2].dtype)), COMPOUND_EXPR_VAL((yyvsp[0].dtype)));
		 /* This may not be exactly right, but is probably good enough
		  * for the purposes of parsing constant expressions. */
		 (yyval.dtype).type = promote((yyvsp[-2].dtype).type, (yyvsp[0].dtype).type);
	       }
#line 11712 "y.tab.c" /* yacc.c:1646  */
    break;

  case 494:
#line 6749 "parser.y" /* yacc.c:1646  */
    {
		 (yyval.dtype).val = NewStringf("-%s",(yyvsp[0].dtype).val);
		 (yyval.dtype).type = (yyvsp[0].dtype).type;
	       }
#line 11721 "y.tab.c" /* yacc.c:1646  */
    break;

  case 495:
#line 6753 "parser.y" /* yacc.c:1646  */
    {
                 (yyval.dtype).val = NewStringf("+%s",(yyvsp[0].dtype).val);
		 (yyval.dtype).type = (yyvsp[0].dtype).type;
	       }
#line 11730 "y.tab.c" /* yacc.c:1646  */
    break;

  case 496:
#line 6757 "parser.y" /* yacc.c:1646  */
    {
		 (yyval.dtype).val = NewStringf("~%s",(yyvsp[0].dtype).val);
		 (yyval.dtype).type = (yyvsp[0].dtype).type;
	       }
#line 11739 "y.tab.c" /* yacc.c:1646  */
    break;

  case 497:
#line 6761 "parser.y" /* yacc.c:1646  */
    {
                 (yyval.dtype).val = NewStringf("!%s",COMPOUND_EXPR_VAL((yyvsp[0].dtype)));
		 (yyval.dtype).type = T_INT;
	       }
#line 11748 "y.tab.c" /* yacc.c:1646  */
    break;

  case 498:
#line 6765 "parser.y" /* yacc.c:1646  */
    {
		 String *qty;
                 skip_balanced('(',')');
		 qty = Swig_symbol_type_qualify((yyvsp[-1].type),0);
		 if (SwigType_istemplate(qty)) {
		   String *nstr = SwigType_namestr(qty);
		   Delete(qty);
		   qty = nstr;
		 }
		 (yyval.dtype).val = NewStringf("%s%s",qty,scanner_ccode);
		 Clear(scanner_ccode);
		 (yyval.dtype).type = T_INT;
		 Delete(qty);
               }
#line 11767 "y.tab.c" /* yacc.c:1646  */
    break;

  case 499:
#line 6781 "parser.y" /* yacc.c:1646  */
    {
	        (yyval.str) = NewString("...");
	      }
#line 11775 "y.tab.c" /* yacc.c:1646  */
    break;

  case 500:
#line 6786 "parser.y" /* yacc.c:1646  */
    {
	        (yyval.str) = (yyvsp[0].str);
	      }
#line 11783 "y.tab.c" /* yacc.c:1646  */
    break;

  case 501:
#line 6789 "parser.y" /* yacc.c:1646  */
    {
	        (yyval.str) = 0;
	      }
#line 11791 "y.tab.c" /* yacc.c:1646  */
    break;

  case 502:
#line 6794 "parser.y" /* yacc.c:1646  */
    {
		 (yyval.bases) = (yyvsp[0].bases);
               }
#line 11799 "y.tab.c" /* yacc.c:1646  */
    break;

  case 503:
#line 6799 "parser.y" /* yacc.c:1646  */
    { inherit_list = 1; }
#line 11805 "y.tab.c" /* yacc.c:1646  */
    break;

  case 504:
#line 6799 "parser.y" /* yacc.c:1646  */
    { (yyval.bases) = (yyvsp[0].bases); inherit_list = 0; }
#line 11811 "y.tab.c" /* yacc.c:1646  */
    break;

  case 505:
#line 6800 "parser.y" /* yacc.c:1646  */
    { (yyval.bases) = 0; }
#line 11817 "y.tab.c" /* yacc.c:1646  */
    break;

  case 506:
#line 6803 "parser.y" /* yacc.c:1646  */
    {
		   Hash *list = NewHash();
		   Node *base = (yyvsp[0].node);
		   Node *name = Getattr(base,"name");
		   List *lpublic = NewList();
		   List *lprotected = NewList();
		   List *lprivate = NewList();
		   Setattr(list,"public",lpublic);
		   Setattr(list,"protected",lprotected);
		   Setattr(list,"private",lprivate);
		   Delete(lpublic);
		   Delete(lprotected);
		   Delete(lprivate);
		   Append(Getattr(list,Getattr(base,"access")),name);
	           (yyval.bases) = list;
               }
#line 11838 "y.tab.c" /* yacc.c:1646  */
    break;

  case 507:
#line 6820 "parser.y" /* yacc.c:1646  */
    {
		   Hash *list = (yyvsp[-2].bases);
		   Node *base = (yyvsp[0].node);
		   Node *name = Getattr(base,"name");
		   Append(Getattr(list,Getattr(base,"access")),name);
                   (yyval.bases) = list;
               }
#line 11850 "y.tab.c" /* yacc.c:1646  */
    break;

  case 508:
#line 6829 "parser.y" /* yacc.c:1646  */
    {
		 (yyval.intvalue) = cparse_line;
	       }
#line 11858 "y.tab.c" /* yacc.c:1646  */
    break;

  case 509:
#line 6831 "parser.y" /* yacc.c:1646  */
    {
		 (yyval.node) = NewHash();
		 Setfile((yyval.node),cparse_file);
		 Setline((yyval.node),(yyvsp[-2].intvalue));
		 Setattr((yyval.node),"name",(yyvsp[-1].str));
		 Setfile((yyvsp[-1].str),cparse_file);
		 Setline((yyvsp[-1].str),(yyvsp[-2].intvalue));
                 if (last_cpptype && (Strcmp(last_cpptype,"struct") != 0)) {
		   Setattr((yyval.node),"access","private");
		   Swig_warning(WARN_PARSE_NO_ACCESS, Getfile((yyval.node)), Getline((yyval.node)), "No access specifier given for base class '%s' (ignored).\n", SwigType_namestr((yyvsp[-1].str)));
                 } else {
		   Setattr((yyval.node),"access","public");
		 }
		 if ((yyvsp[0].str))
		   SetFlag((yyval.node), "variadic");
               }
#line 11879 "y.tab.c" /* yacc.c:1646  */
    break;

  case 510:
#line 6847 "parser.y" /* yacc.c:1646  */
    {
		 (yyval.intvalue) = cparse_line;
	       }
#line 11887 "y.tab.c" /* yacc.c:1646  */
    break;

  case 511:
#line 6849 "parser.y" /* yacc.c:1646  */
    {
		 (yyval.node) = NewHash();
		 Setfile((yyval.node),cparse_file);
		 Setline((yyval.node),(yyvsp[-3].intvalue));
		 Setattr((yyval.node),"name",(yyvsp[-1].str));
		 Setfile((yyvsp[-1].str),cparse_file);
		 Setline((yyvsp[-1].str),(yyvsp[-3].intvalue));
		 Setattr((yyval.node),"access",(yyvsp[-4].id));
	         if (Strcmp((yyvsp[-4].id),"public") != 0) {
		   Swig_warning(WARN_PARSE_PRIVATE_INHERIT, Getfile((yyval.node)), Getline((yyval.node)), "%s inheritance from base '%s' (ignored).\n", (yyvsp[-4].id), SwigType_namestr((yyvsp[-1].str)));
		 }
		 if ((yyvsp[0].str))
		   SetFlag((yyval.node), "variadic");
               }
#line 11906 "y.tab.c" /* yacc.c:1646  */
    break;

  case 512:
#line 6865 "parser.y" /* yacc.c:1646  */
    { (yyval.id) = (char*)"public"; }
#line 11912 "y.tab.c" /* yacc.c:1646  */
    break;

  case 513:
#line 6866 "parser.y" /* yacc.c:1646  */
    { (yyval.id) = (char*)"private"; }
#line 11918 "y.tab.c" /* yacc.c:1646  */
    break;

  case 514:
#line 6867 "parser.y" /* yacc.c:1646  */
    { (yyval.id) = (char*)"protected"; }
#line 11924 "y.tab.c" /* yacc.c:1646  */
    break;

  case 515:
#line 6870 "parser.y" /* yacc.c:1646  */
    { 
                   (yyval.id) = (char*)"class"; 
		   if (!inherit_list) last_cpptype = (yyval.id);
               }
#line 11933 "y.tab.c" /* yacc.c:1646  */
    break;

  case 516:
#line 6874 "parser.y" /* yacc.c:1646  */
    { 
                   (yyval.id) = (char *)"typename"; 
		   if (!inherit_list) last_cpptype = (yyval.id);
               }
#line 11942 "y.tab.c" /* yacc.c:1646  */
    break;

  case 517:
#line 6878 "parser.y" /* yacc.c:1646  */
    { 
                   (yyval.id) = (char *)"class..."; 
		   if (!inherit_list) last_cpptype = (yyval.id);
               }
#line 11951 "y.tab.c" /* yacc.c:1646  */
    break;

  case 518:
#line 6882 "parser.y" /* yacc.c:1646  */
    { 
                   (yyval.id) = (char *)"typename..."; 
		   if (!inherit_list) last_cpptype = (yyval.id);
               }
#line 11960 "y.tab.c" /* yacc.c:1646  */
    break;

  case 519:
#line 6888 "parser.y" /* yacc.c:1646  */
    {
                 (yyval.id) = (yyvsp[0].id);
               }
#line 11968 "y.tab.c" /* yacc.c:1646  */
    break;

  case 520:
#line 6891 "parser.y" /* yacc.c:1646  */
    { 
                   (yyval.id) = (char*)"struct"; 
		   if (!inherit_list) last_cpptype = (yyval.id);
               }
#line 11977 "y.tab.c" /* yacc.c:1646  */
    break;

  case 521:
#line 6895 "parser.y" /* yacc.c:1646  */
    {
                   (yyval.id) = (char*)"union"; 
		   if (!inherit_list) last_cpptype = (yyval.id);
               }
#line 11986 "y.tab.c" /* yacc.c:1646  */
    break;

  case 522:
#line 6901 "parser.y" /* yacc.c:1646  */
    {
                   (yyval.id) = (char*)"class";
		   if (!inherit_list) last_cpptype = (yyval.id);
               }
#line 11995 "y.tab.c" /* yacc.c:1646  */
    break;

  case 523:
#line 6905 "parser.y" /* yacc.c:1646  */
    {
                   (yyval.id) = (char*)"struct";
		   if (!inherit_list) last_cpptype = (yyval.id);
               }
#line 12004 "y.tab.c" /* yacc.c:1646  */
    break;

  case 524:
#line 6909 "parser.y" /* yacc.c:1646  */
    {
                   (yyval.id) = (char*)"union";
		   if (!inherit_list) last_cpptype = (yyval.id);
               }
#line 12013 "y.tab.c" /* yacc.c:1646  */
    break;

  case 525:
#line 6915 "parser.y" /* yacc.c:1646  */
    {
		   (yyval.id) = (yyvsp[0].id);
               }
#line 12021 "y.tab.c" /* yacc.c:1646  */
    break;

  case 526:
#line 6918 "parser.y" /* yacc.c:1646  */
    {
		   (yyval.id) = 0;
               }
#line 12029 "y.tab.c" /* yacc.c:1646  */
    break;

  case 529:
#line 6927 "parser.y" /* yacc.c:1646  */
    {
                   (yyval.str) = 0;
	       }
#line 12037 "y.tab.c" /* yacc.c:1646  */
    break;

  case 530:
#line 6930 "parser.y" /* yacc.c:1646  */
    {
                   (yyval.str) = NewString("1");
	       }
#line 12045 "y.tab.c" /* yacc.c:1646  */
    break;

  case 531:
#line 6933 "parser.y" /* yacc.c:1646  */
    {
                   (yyval.str) = NewString("1");
	       }
#line 12053 "y.tab.c" /* yacc.c:1646  */
    break;

  case 532:
#line 6936 "parser.y" /* yacc.c:1646  */
    {
                   (yyval.str) = NewString("1");
	       }
#line 12061 "y.tab.c" /* yacc.c:1646  */
    break;

  case 533:
#line 6941 "parser.y" /* yacc.c:1646  */
    {
                   (yyval.str) = (yyvsp[0].str);
               }
#line 12069 "y.tab.c" /* yacc.c:1646  */
    break;

  case 534:
#line 6944 "parser.y" /* yacc.c:1646  */
    {
                   (yyval.str) = 0;
               }
#line 12077 "y.tab.c" /* yacc.c:1646  */
    break;

  case 535:
#line 6949 "parser.y" /* yacc.c:1646  */
    {
                    (yyval.dtype).throws = (yyvsp[-1].pl);
                    (yyval.dtype).throwf = NewString("1");
                    (yyval.dtype).nexcept = 0;
                    (yyval.dtype).final = 0;
	       }
#line 12088 "y.tab.c" /* yacc.c:1646  */
    break;

  case 536:
#line 6955 "parser.y" /* yacc.c:1646  */
    {
                    (yyval.dtype).throws = 0;
                    (yyval.dtype).throwf = 0;
                    (yyval.dtype).nexcept = NewString("true");
                    (yyval.dtype).final = 0;
	       }
#line 12099 "y.tab.c" /* yacc.c:1646  */
    break;

  case 537:
#line 6961 "parser.y" /* yacc.c:1646  */
    {
                    (yyval.dtype).throws = 0;
                    (yyval.dtype).throwf = 0;
                    (yyval.dtype).nexcept = 0;
                    (yyval.dtype).final = (yyvsp[0].str);
	       }
#line 12110 "y.tab.c" /* yacc.c:1646  */
    break;

  case 538:
#line 6967 "parser.y" /* yacc.c:1646  */
    {
                    (yyval.dtype).throws = (yyvsp[-2].pl);
                    (yyval.dtype).throwf = NewString("1");
                    (yyval.dtype).nexcept = 0;
                    (yyval.dtype).final = (yyvsp[0].str);
	       }
#line 12121 "y.tab.c" /* yacc.c:1646  */
    break;

  case 539:
#line 6973 "parser.y" /* yacc.c:1646  */
    {
                    (yyval.dtype).throws = 0;
                    (yyval.dtype).throwf = 0;
                    (yyval.dtype).nexcept = NewString("true");
                    (yyval.dtype).final = (yyvsp[0].str);
	       }
#line 12132 "y.tab.c" /* yacc.c:1646  */
    break;

  case 540:
#line 6979 "parser.y" /* yacc.c:1646  */
    {
                    (yyval.dtype).throws = 0;
                    (yyval.dtype).throwf = 0;
                    (yyval.dtype).nexcept = (yyvsp[-1].dtype).val;
                    (yyval.dtype).final = 0;
	       }
#line 12143 "y.tab.c" /* yacc.c:1646  */
    break;

  case 541:
#line 6987 "parser.y" /* yacc.c:1646  */
    {
                    (yyval.dtype).throws = 0;
                    (yyval.dtype).throwf = 0;
                    (yyval.dtype).nexcept = 0;
                    (yyval.dtype).final = 0;
                    (yyval.dtype).qualifier = (yyvsp[0].dtype).qualifier;
                    (yyval.dtype).refqualifier = (yyvsp[0].dtype).refqualifier;
               }
#line 12156 "y.tab.c" /* yacc.c:1646  */
    break;

  case 542:
#line 6995 "parser.y" /* yacc.c:1646  */
    {
		    (yyval.dtype) = (yyvsp[0].dtype);
                    (yyval.dtype).qualifier = 0;
                    (yyval.dtype).refqualifier = 0;
               }
#line 12166 "y.tab.c" /* yacc.c:1646  */
    break;

  case 543:
#line 7000 "parser.y" /* yacc.c:1646  */
    {
		    (yyval.dtype) = (yyvsp[0].dtype);
                    (yyval.dtype).qualifier = (yyvsp[-1].dtype).qualifier;
                    (yyval.dtype).refqualifier = (yyvsp[-1].dtype).refqualifier;
               }
#line 12176 "y.tab.c" /* yacc.c:1646  */
    break;

  case 544:
#line 7007 "parser.y" /* yacc.c:1646  */
    {
                    (yyval.dtype) = (yyvsp[0].dtype);
               }
#line 12184 "y.tab.c" /* yacc.c:1646  */
    break;

  case 545:
#line 7010 "parser.y" /* yacc.c:1646  */
    { 
                    (yyval.dtype).throws = 0;
                    (yyval.dtype).throwf = 0;
                    (yyval.dtype).nexcept = 0;
                    (yyval.dtype).final = 0;
                    (yyval.dtype).qualifier = 0;
                    (yyval.dtype).refqualifier = 0;
               }
#line 12197 "y.tab.c" /* yacc.c:1646  */
    break;

  case 546:
#line 7020 "parser.y" /* yacc.c:1646  */
    { 
                    Clear(scanner_ccode); 
                    (yyval.decl).have_parms = 0; 
                    (yyval.decl).defarg = 0; 
		    (yyval.decl).throws = (yyvsp[-2].dtype).throws;
		    (yyval.decl).throwf = (yyvsp[-2].dtype).throwf;
		    (yyval.decl).nexcept = (yyvsp[-2].dtype).nexcept;
		    (yyval.decl).final = (yyvsp[-2].dtype).final;
                    if ((yyvsp[-2].dtype).qualifier)
                      Swig_error(cparse_file, cparse_line, "Constructor cannot have a qualifier.\n");
               }
#line 12213 "y.tab.c" /* yacc.c:1646  */
    break;

  case 547:
#line 7031 "parser.y" /* yacc.c:1646  */
    { 
                    skip_balanced('{','}'); 
                    (yyval.decl).have_parms = 0; 
                    (yyval.decl).defarg = 0; 
                    (yyval.decl).throws = (yyvsp[-2].dtype).throws;
                    (yyval.decl).throwf = (yyvsp[-2].dtype).throwf;
                    (yyval.decl).nexcept = (yyvsp[-2].dtype).nexcept;
                    (yyval.decl).final = (yyvsp[-2].dtype).final;
                    if ((yyvsp[-2].dtype).qualifier)
                      Swig_error(cparse_file, cparse_line, "Constructor cannot have a qualifier.\n");
               }
#line 12229 "y.tab.c" /* yacc.c:1646  */
    break;

  case 548:
#line 7042 "parser.y" /* yacc.c:1646  */
    { 
                    Clear(scanner_ccode); 
                    (yyval.decl).parms = (yyvsp[-2].pl); 
                    (yyval.decl).have_parms = 1; 
                    (yyval.decl).defarg = 0; 
		    (yyval.decl).throws = 0;
		    (yyval.decl).throwf = 0;
		    (yyval.decl).nexcept = 0;
		    (yyval.decl).final = 0;
               }
#line 12244 "y.tab.c" /* yacc.c:1646  */
    break;

  case 549:
#line 7052 "parser.y" /* yacc.c:1646  */
    {
                    skip_balanced('{','}'); 
                    (yyval.decl).parms = (yyvsp[-2].pl); 
                    (yyval.decl).have_parms = 1; 
                    (yyval.decl).defarg = 0; 
                    (yyval.decl).throws = 0;
                    (yyval.decl).throwf = 0;
                    (yyval.decl).nexcept = 0;
                    (yyval.decl).final = 0;
               }
#line 12259 "y.tab.c" /* yacc.c:1646  */
    break;

  case 550:
#line 7062 "parser.y" /* yacc.c:1646  */
    { 
                    (yyval.decl).have_parms = 0; 
                    (yyval.decl).defarg = (yyvsp[-1].dtype).val; 
                    (yyval.decl).throws = 0;
                    (yyval.decl).throwf = 0;
                    (yyval.decl).nexcept = 0;
                    (yyval.decl).final = 0;
               }
#line 12272 "y.tab.c" /* yacc.c:1646  */
    break;

  case 551:
#line 7070 "parser.y" /* yacc.c:1646  */
    {
                    (yyval.decl).have_parms = 0;
                    (yyval.decl).defarg = (yyvsp[-1].dtype).val;
                    (yyval.decl).throws = (yyvsp[-3].dtype).throws;
                    (yyval.decl).throwf = (yyvsp[-3].dtype).throwf;
                    (yyval.decl).nexcept = (yyvsp[-3].dtype).nexcept;
                    (yyval.decl).final = (yyvsp[-3].dtype).final;
                    if ((yyvsp[-3].dtype).qualifier)
                      Swig_error(cparse_file, cparse_line, "Constructor cannot have a qualifier.\n");
               }
#line 12287 "y.tab.c" /* yacc.c:1646  */
    break;

  case 558:
#line 7092 "parser.y" /* yacc.c:1646  */
    {
		  skip_balanced('(',')');
		  Clear(scanner_ccode);
		}
#line 12296 "y.tab.c" /* yacc.c:1646  */
    break;

  case 559:
#line 7104 "parser.y" /* yacc.c:1646  */
    {
		  skip_balanced('{','}');
		  Clear(scanner_ccode);
		}
#line 12305 "y.tab.c" /* yacc.c:1646  */
    break;

  case 560:
#line 7110 "parser.y" /* yacc.c:1646  */
    {
                     String *s = NewStringEmpty();
                     SwigType_add_template(s,(yyvsp[-1].p));
                     (yyval.id) = Char(s);
		     scanner_last_id(1);
                }
#line 12316 "y.tab.c" /* yacc.c:1646  */
    break;

  case 561:
#line 7119 "parser.y" /* yacc.c:1646  */
    { (yyval.id) = (yyvsp[0].id); }
#line 12322 "y.tab.c" /* yacc.c:1646  */
    break;

  case 562:
#line 7120 "parser.y" /* yacc.c:1646  */
    { (yyval.id) = Swig_copy_string("override"); }
#line 12328 "y.tab.c" /* yacc.c:1646  */
    break;

  case 563:
#line 7121 "parser.y" /* yacc.c:1646  */
    { (yyval.id) = Swig_copy_string("final"); }
#line 12334 "y.tab.c" /* yacc.c:1646  */
    break;

  case 564:
#line 7124 "parser.y" /* yacc.c:1646  */
    { (yyval.id) = (yyvsp[0].id); }
#line 12340 "y.tab.c" /* yacc.c:1646  */
    break;

  case 565:
#line 7125 "parser.y" /* yacc.c:1646  */
    { (yyval.id) = Char((yyvsp[0].dtype).val); }
#line 12346 "y.tab.c" /* yacc.c:1646  */
    break;

  case 566:
#line 7126 "parser.y" /* yacc.c:1646  */
    { (yyval.id) = Char((yyvsp[0].str)); }
#line 12352 "y.tab.c" /* yacc.c:1646  */
    break;

  case 567:
#line 7129 "parser.y" /* yacc.c:1646  */
    { (yyval.id) = (yyvsp[0].id); }
#line 12358 "y.tab.c" /* yacc.c:1646  */
    break;

  case 568:
#line 7130 "parser.y" /* yacc.c:1646  */
    { (yyval.id) = 0; }
#line 12364 "y.tab.c" /* yacc.c:1646  */
    break;

  case 569:
#line 7133 "parser.y" /* yacc.c:1646  */
    { 
                  (yyval.str) = 0;
		  if (!(yyval.str)) (yyval.str) = NewStringf("%s%s", (yyvsp[-1].str),(yyvsp[0].str));
      	          Delete((yyvsp[0].str));
               }
#line 12374 "y.tab.c" /* yacc.c:1646  */
    break;

  case 570:
#line 7138 "parser.y" /* yacc.c:1646  */
    {
		 (yyval.str) = NewStringf("::%s%s",(yyvsp[-1].str),(yyvsp[0].str));
                 Delete((yyvsp[0].str));
               }
#line 12383 "y.tab.c" /* yacc.c:1646  */
    break;

  case 571:
#line 7142 "parser.y" /* yacc.c:1646  */
    {
		 (yyval.str) = NewString((yyvsp[0].str));
   	       }
#line 12391 "y.tab.c" /* yacc.c:1646  */
    break;

  case 572:
#line 7145 "parser.y" /* yacc.c:1646  */
    {
		 (yyval.str) = NewStringf("::%s",(yyvsp[0].str));
               }
#line 12399 "y.tab.c" /* yacc.c:1646  */
    break;

  case 573:
#line 7148 "parser.y" /* yacc.c:1646  */
    {
                 (yyval.str) = NewStringf("%s", (yyvsp[0].str));
	       }
#line 12407 "y.tab.c" /* yacc.c:1646  */
    break;

  case 574:
#line 7151 "parser.y" /* yacc.c:1646  */
    {
                 (yyval.str) = NewStringf("%s%s", (yyvsp[-1].str), (yyvsp[0].id));
	       }
#line 12415 "y.tab.c" /* yacc.c:1646  */
    break;

  case 575:
#line 7154 "parser.y" /* yacc.c:1646  */
    {
                 (yyval.str) = NewStringf("::%s",(yyvsp[0].str));
               }
#line 12423 "y.tab.c" /* yacc.c:1646  */
    break;

  case 576:
#line 7159 "parser.y" /* yacc.c:1646  */
    {
                   (yyval.str) = NewStringf("::%s%s",(yyvsp[-1].str),(yyvsp[0].str));
		   Delete((yyvsp[0].str));
               }
#line 12432 "y.tab.c" /* yacc.c:1646  */
    break;

  case 577:
#line 7163 "parser.y" /* yacc.c:1646  */
    {
                   (yyval.str) = NewStringf("::%s",(yyvsp[0].str));
               }
#line 12440 "y.tab.c" /* yacc.c:1646  */
    break;

  case 578:
#line 7166 "parser.y" /* yacc.c:1646  */
    {
                   (yyval.str) = NewStringf("::%s",(yyvsp[0].str));
               }
#line 12448 "y.tab.c" /* yacc.c:1646  */
    break;

  case 579:
#line 7173 "parser.y" /* yacc.c:1646  */
    {
		 (yyval.str) = NewStringf("::~%s",(yyvsp[0].str));
               }
#line 12456 "y.tab.c" /* yacc.c:1646  */
    break;

  case 580:
#line 7179 "parser.y" /* yacc.c:1646  */
    {
		(yyval.str) = NewStringf("%s", (yyvsp[0].id));
	      }
#line 12464 "y.tab.c" /* yacc.c:1646  */
    break;

  case 581:
#line 7182 "parser.y" /* yacc.c:1646  */
    {
		(yyval.str) = NewStringf("%s%s", (yyvsp[-1].id), (yyvsp[0].id));
	      }
#line 12472 "y.tab.c" /* yacc.c:1646  */
    break;

  case 582:
#line 7187 "parser.y" /* yacc.c:1646  */
    {
		(yyval.str) = (yyvsp[0].str);
	      }
#line 12480 "y.tab.c" /* yacc.c:1646  */
    break;

  case 583:
#line 7190 "parser.y" /* yacc.c:1646  */
    {
		(yyval.str) = NewStringf("%s%s", (yyvsp[-1].id), (yyvsp[0].id));
	      }
#line 12488 "y.tab.c" /* yacc.c:1646  */
    break;

  case 584:
#line 7196 "parser.y" /* yacc.c:1646  */
    {
                  (yyval.str) = 0;
		  if (!(yyval.str)) (yyval.str) = NewStringf("%s%s", (yyvsp[-1].id),(yyvsp[0].str));
      	          Delete((yyvsp[0].str));
               }
#line 12498 "y.tab.c" /* yacc.c:1646  */
    break;

  case 585:
#line 7201 "parser.y" /* yacc.c:1646  */
    {
		 (yyval.str) = NewStringf("::%s%s",(yyvsp[-1].id),(yyvsp[0].str));
                 Delete((yyvsp[0].str));
               }
#line 12507 "y.tab.c" /* yacc.c:1646  */
    break;

  case 586:
#line 7205 "parser.y" /* yacc.c:1646  */
    {
		 (yyval.str) = NewString((yyvsp[0].id));
   	       }
#line 12515 "y.tab.c" /* yacc.c:1646  */
    break;

  case 587:
#line 7208 "parser.y" /* yacc.c:1646  */
    {
		 (yyval.str) = NewStringf("::%s",(yyvsp[0].id));
               }
#line 12523 "y.tab.c" /* yacc.c:1646  */
    break;

  case 588:
#line 7211 "parser.y" /* yacc.c:1646  */
    {
                 (yyval.str) = NewString((yyvsp[0].str));
	       }
#line 12531 "y.tab.c" /* yacc.c:1646  */
    break;

  case 589:
#line 7214 "parser.y" /* yacc.c:1646  */
    {
                 (yyval.str) = NewStringf("::%s",(yyvsp[0].str));
               }
#line 12539 "y.tab.c" /* yacc.c:1646  */
    break;

  case 590:
#line 7219 "parser.y" /* yacc.c:1646  */
    {
                   (yyval.str) = NewStringf("::%s%s",(yyvsp[-1].id),(yyvsp[0].str));
		   Delete((yyvsp[0].str));
               }
#line 12548 "y.tab.c" /* yacc.c:1646  */
    break;

  case 591:
#line 7223 "parser.y" /* yacc.c:1646  */
    {
                   (yyval.str) = NewStringf("::%s",(yyvsp[0].id));
               }
#line 12556 "y.tab.c" /* yacc.c:1646  */
    break;

  case 592:
#line 7226 "parser.y" /* yacc.c:1646  */
    {
                   (yyval.str) = NewStringf("::%s",(yyvsp[0].str));
               }
#line 12564 "y.tab.c" /* yacc.c:1646  */
    break;

  case 593:
#line 7229 "parser.y" /* yacc.c:1646  */
    {
		 (yyval.str) = NewStringf("::~%s",(yyvsp[0].id));
               }
#line 12572 "y.tab.c" /* yacc.c:1646  */
    break;

  case 594:
#line 7235 "parser.y" /* yacc.c:1646  */
    { 
                   (yyval.str) = NewStringf("%s%s", (yyvsp[-1].str), (yyvsp[0].id));
               }
#line 12580 "y.tab.c" /* yacc.c:1646  */
    break;

  case 595:
#line 7238 "parser.y" /* yacc.c:1646  */
    { (yyval.str) = NewString((yyvsp[0].id));}
#line 12586 "y.tab.c" /* yacc.c:1646  */
    break;

  case 596:
#line 7241 "parser.y" /* yacc.c:1646  */
    {
                   (yyval.str) = NewStringf("%s%s", (yyvsp[-1].str), (yyvsp[0].id));
               }
#line 12594 "y.tab.c" /* yacc.c:1646  */
    break;

  case 597:
#line 7249 "parser.y" /* yacc.c:1646  */
    { (yyval.str) = NewString((yyvsp[0].id));}
#line 12600 "y.tab.c" /* yacc.c:1646  */
    break;

  case 598:
#line 7252 "parser.y" /* yacc.c:1646  */
    {
		 (yyval.str) = (yyvsp[0].str);
               }
#line 12608 "y.tab.c" /* yacc.c:1646  */
    break;

  case 599:
#line 7255 "parser.y" /* yacc.c:1646  */
    {
                  skip_balanced('{','}');
		  (yyval.str) = NewString(scanner_ccode);
               }
#line 12617 "y.tab.c" /* yacc.c:1646  */
    break;

  case 600:
#line 7259 "parser.y" /* yacc.c:1646  */
    {
		 (yyval.str) = (yyvsp[0].str);
              }
#line 12625 "y.tab.c" /* yacc.c:1646  */
    break;

  case 601:
#line 7264 "parser.y" /* yacc.c:1646  */
    {
                  Hash *n;
                  (yyval.node) = NewHash();
                  n = (yyvsp[-1].node);
                  while(n) {
                     String *name, *value;
                     name = Getattr(n,"name");
                     value = Getattr(n,"value");
		     if (!value) value = (String *) "1";
                     Setattr((yyval.node),name, value);
		     n = nextSibling(n);
		  }
               }
#line 12643 "y.tab.c" /* yacc.c:1646  */
    break;

  case 602:
#line 7277 "parser.y" /* yacc.c:1646  */
    { (yyval.node) = 0; }
#line 12649 "y.tab.c" /* yacc.c:1646  */
    break;

  case 603:
#line 7281 "parser.y" /* yacc.c:1646  */
    {
		 (yyval.node) = NewHash();
		 Setattr((yyval.node),"name",(yyvsp[-2].id));
		 Setattr((yyval.node),"value",(yyvsp[0].str));
               }
#line 12659 "y.tab.c" /* yacc.c:1646  */
    break;

  case 604:
#line 7286 "parser.y" /* yacc.c:1646  */
    {
		 (yyval.node) = NewHash();
		 Setattr((yyval.node),"name",(yyvsp[-4].id));
		 Setattr((yyval.node),"value",(yyvsp[-2].str));
		 set_nextSibling((yyval.node),(yyvsp[0].node));
               }
#line 12670 "y.tab.c" /* yacc.c:1646  */
    break;

  case 605:
#line 7292 "parser.y" /* yacc.c:1646  */
    {
                 (yyval.node) = NewHash();
                 Setattr((yyval.node),"name",(yyvsp[0].id));
	       }
#line 12679 "y.tab.c" /* yacc.c:1646  */
    break;

  case 606:
#line 7296 "parser.y" /* yacc.c:1646  */
    {
                 (yyval.node) = NewHash();
                 Setattr((yyval.node),"name",(yyvsp[-2].id));
                 set_nextSibling((yyval.node),(yyvsp[0].node));
               }
#line 12689 "y.tab.c" /* yacc.c:1646  */
    break;

  case 607:
#line 7301 "parser.y" /* yacc.c:1646  */
    {
                 (yyval.node) = (yyvsp[0].node);
		 Setattr((yyval.node),"name",(yyvsp[-2].id));
               }
#line 12698 "y.tab.c" /* yacc.c:1646  */
    break;

  case 608:
#line 7305 "parser.y" /* yacc.c:1646  */
    {
                 (yyval.node) = (yyvsp[-2].node);
		 Setattr((yyval.node),"name",(yyvsp[-4].id));
		 set_nextSibling((yyval.node),(yyvsp[0].node));
               }
#line 12708 "y.tab.c" /* yacc.c:1646  */
    break;

  case 609:
#line 7312 "parser.y" /* yacc.c:1646  */
    {
		 (yyval.str) = (yyvsp[0].str);
               }
#line 12716 "y.tab.c" /* yacc.c:1646  */
    break;

  case 610:
#line 7315 "parser.y" /* yacc.c:1646  */
    {
                 (yyval.str) = Char((yyvsp[0].dtype).val);
               }
#line 12724 "y.tab.c" /* yacc.c:1646  */
    break;


#line 12728 "y.tab.c" /* yacc.c:1646  */
      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYEMPTY : YYTRANSLATE (yychar);

  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (YY_("syntax error"));
#else
# define YYSYNTAX_ERROR yysyntax_error (&yymsg_alloc, &yymsg, \
                                        yyssp, yytoken)
      {
        char const *yymsgp = YY_("syntax error");
        int yysyntax_error_status;
        yysyntax_error_status = YYSYNTAX_ERROR;
        if (yysyntax_error_status == 0)
          yymsgp = yymsg;
        else if (yysyntax_error_status == 1)
          {
            if (yymsg != yymsgbuf)
              YYSTACK_FREE (yymsg);
            yymsg = (char *) YYSTACK_ALLOC (yymsg_alloc);
            if (!yymsg)
              {
                yymsg = yymsgbuf;
                yymsg_alloc = sizeof yymsgbuf;
                yysyntax_error_status = 2;
              }
            else
              {
                yysyntax_error_status = YYSYNTAX_ERROR;
                yymsgp = yymsg;
              }
          }
        yyerror (yymsgp);
        if (yysyntax_error_status == 2)
          goto yyexhaustedlab;
      }
# undef YYSYNTAX_ERROR
#endif
    }



  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
         error, discard it.  */

      if (yychar <= YYEOF)
        {
          /* Return failure if at end of input.  */
          if (yychar == YYEOF)
            YYABORT;
        }
      else
        {
          yydestruct ("Error: discarding",
                      yytoken, &yylval);
          yychar = YYEMPTY;
        }
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:

  /* Pacify compilers like GCC when the user code never invokes
     YYERROR and the label yyerrorlab therefore never appears in user
     code.  */
  if (/*CONSTCOND*/ 0)
     goto yyerrorlab;

  /* Do not reclaim the symbols of the rule whose action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYTERROR;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
            {
              yyn = yytable[yyn];
              if (0 < yyn)
                break;
            }
        }

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
        YYABORT;


      yydestruct ("Error: popping",
                  yystos[yystate], yyvsp);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;

#if !defined yyoverflow || YYERROR_VERBOSE
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  yystos[*yyssp], yyvsp);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
#if YYERROR_VERBOSE
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
#endif
  return yyresult;
}
#line 7322 "parser.y" /* yacc.c:1906  */


SwigType *Swig_cparse_type(String *s) {
   String *ns;
   ns = NewStringf("%s;",s);
   Seek(ns,0,SEEK_SET);
   scanner_file(ns);
   top = 0;
   scanner_next_token(PARSETYPE);
   yyparse();
   /*   Printf(stdout,"typeparse: '%s' ---> '%s'\n", s, top); */
   return top;
}


Parm *Swig_cparse_parm(String *s) {
   String *ns;
   ns = NewStringf("%s;",s);
   Seek(ns,0,SEEK_SET);
   scanner_file(ns);
   top = 0;
   scanner_next_token(PARSEPARM);
   yyparse();
   /*   Printf(stdout,"typeparse: '%s' ---> '%s'\n", s, top); */
   Delete(ns);
   return top;
}


ParmList *Swig_cparse_parms(String *s, Node *file_line_node) {
   String *ns;
   char *cs = Char(s);
   if (cs && cs[0] != '(') {
     ns = NewStringf("(%s);",s);
   } else {
     ns = NewStringf("%s;",s);
   }
   Setfile(ns, Getfile(file_line_node));
   Setline(ns, Getline(file_line_node));
   Seek(ns,0,SEEK_SET);
   scanner_file(ns);
   top = 0;
   scanner_next_token(PARSEPARMS);
   yyparse();
   /*   Printf(stdout,"typeparse: '%s' ---> '%s'\n", s, top); */
   return top;
}

