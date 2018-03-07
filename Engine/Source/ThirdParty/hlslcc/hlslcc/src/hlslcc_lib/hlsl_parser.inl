/* A Bison parser, made by GNU Bison 2.5.  */

/* Bison implementation for Yacc-like parsers in C
   
      Copyright (C) 1984, 1989-1990, 2000-2011 Free Software Foundation, Inc.
   
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
#define YYBISON_VERSION "2.5"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 1

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1

/* Using locations.  */
#define YYLSP_NEEDED 1

/* Substitute the variable and function names.  */
#define yyparse         _mesa_hlsl_parse
#define yylex           _mesa_hlsl_lex
#define yyerror         _mesa_hlsl_error
#define yylval          _mesa_hlsl_lval
#define yychar          _mesa_hlsl_char
#define yydebug         _mesa_hlsl_debug
#define yynerrs         _mesa_hlsl_nerrs
#define yylloc          _mesa_hlsl_lloc

/* Copy the first part of user declarations.  */

/* Line 268 of yacc.c  */
#line 1 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"

// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

// This code is modified from that in the Mesa3D Graphics library available at
// http://mesa3d.org/
// The license for the original code follows:
//#define YYDEBUG 1
/*
 * Copyright © 2008, 2009 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "ShaderCompilerCommon.h"
#include "ast.h"
#include "glsl_parser_extras.h"
#include "glsl_types.h"

#define YYLEX_PARAM state->scanner

#undef yyerror

static void yyerror(YYLTYPE *loc, _mesa_glsl_parse_state *st, const char *msg)
{
   _mesa_glsl_error(loc, st, "%s", msg);
}


/* Line 268 of yacc.c  */
#line 125 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.inl"

/* Enabling traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 1
#endif

/* Enabling the token table.  */
#ifndef YYTOKEN_TABLE
# define YYTOKEN_TABLE 0
#endif


/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     CONST_TOK = 258,
     BOOL_TOK = 259,
     FLOAT_TOK = 260,
     INT_TOK = 261,
     UINT_TOK = 262,
     BREAK = 263,
     CONTINUE = 264,
     DO = 265,
     ELSE = 266,
     FOR = 267,
     IF = 268,
     DISCARD = 269,
     RETURN = 270,
     SWITCH = 271,
     CASE = 272,
     DEFAULT = 273,
     BVEC2 = 274,
     BVEC3 = 275,
     BVEC4 = 276,
     IVEC2 = 277,
     IVEC3 = 278,
     IVEC4 = 279,
     UVEC2 = 280,
     UVEC3 = 281,
     UVEC4 = 282,
     VEC2 = 283,
     VEC3 = 284,
     VEC4 = 285,
     HALF_TOK = 286,
     HVEC2 = 287,
     HVEC3 = 288,
     HVEC4 = 289,
     IN_TOK = 290,
     OUT_TOK = 291,
     INOUT_TOK = 292,
     UNIFORM = 293,
     VARYING = 294,
     GLOBALLYCOHERENT = 295,
     SHARED = 296,
     CENTROID = 297,
     NOPERSPECTIVE = 298,
     NOINTERPOLATION = 299,
     LINEAR = 300,
     MAT2X2 = 301,
     MAT2X3 = 302,
     MAT2X4 = 303,
     MAT3X2 = 304,
     MAT3X3 = 305,
     MAT3X4 = 306,
     MAT4X2 = 307,
     MAT4X3 = 308,
     MAT4X4 = 309,
     HMAT2X2 = 310,
     HMAT2X3 = 311,
     HMAT2X4 = 312,
     HMAT3X2 = 313,
     HMAT3X3 = 314,
     HMAT3X4 = 315,
     HMAT4X2 = 316,
     HMAT4X3 = 317,
     HMAT4X4 = 318,
     FMAT2X2 = 319,
     FMAT2X3 = 320,
     FMAT2X4 = 321,
     FMAT3X2 = 322,
     FMAT3X3 = 323,
     FMAT3X4 = 324,
     FMAT4X2 = 325,
     FMAT4X3 = 326,
     FMAT4X4 = 327,
     SAMPLERSTATE = 328,
     SAMPLERSTATE_CMP = 329,
     TEXTURE1D = 330,
     TEXTURE1D_ARRAY = 331,
     TEXTURE2D = 332,
     TEXTURE2D_ARRAY = 333,
     TEXTURE2DMS = 334,
     TEXTURE_EXTERNAL = 335,
     BUFFER = 336,
     STRUCTUREDBUFFER = 337,
     BYTEADDRESSBUFFER = 338,
     TEXTURE2DMS_ARRAY = 339,
     TEXTURE3D = 340,
     TEXTURECUBE = 341,
     TEXTURECUBE_ARRAY = 342,
     RWBUFFER = 343,
     RWTEXTURE1D = 344,
     RWTEXTURE1D_ARRAY = 345,
     RWTEXTURE2D = 346,
     RWTEXTURE2D_ARRAY = 347,
     RWTEXTURE3D = 348,
     RWSTRUCTUREDBUFFER = 349,
     RWBYTEADDRESSBUFFER = 350,
     POINT_TOK = 351,
     LINE_TOK = 352,
     TRIANGLE_TOK = 353,
     LINEADJ_TOK = 354,
     TRIANGLEADJ_TOK = 355,
     POINTSTREAM = 356,
     LINESTREAM = 357,
     TRIANGLESTREAM = 358,
     INPUTPATCH = 359,
     OUTPUTPATCH = 360,
     STRUCT = 361,
     VOID_TOK = 362,
     WHILE = 363,
     CBUFFER = 364,
     IDENTIFIER = 365,
     TYPE_IDENTIFIER = 366,
     NEW_IDENTIFIER = 367,
     FLOATCONSTANT = 368,
     INTCONSTANT = 369,
     UINTCONSTANT = 370,
     BOOLCONSTANT = 371,
     STRINGCONSTANT = 372,
     FIELD_SELECTION = 373,
     LEFT_OP = 374,
     RIGHT_OP = 375,
     INC_OP = 376,
     DEC_OP = 377,
     LE_OP = 378,
     GE_OP = 379,
     EQ_OP = 380,
     NE_OP = 381,
     AND_OP = 382,
     OR_OP = 383,
     MUL_ASSIGN = 384,
     DIV_ASSIGN = 385,
     ADD_ASSIGN = 386,
     MOD_ASSIGN = 387,
     LEFT_ASSIGN = 388,
     RIGHT_ASSIGN = 389,
     AND_ASSIGN = 390,
     XOR_ASSIGN = 391,
     OR_ASSIGN = 392,
     SUB_ASSIGN = 393,
     INVARIANT = 394,
     VERSION_TOK = 395,
     EXTENSION = 396,
     LINE = 397,
     COLON = 398,
     EOL = 399,
     INTERFACE = 400,
     OUTPUT = 401,
     PRAGMA_DEBUG_ON = 402,
     PRAGMA_DEBUG_OFF = 403,
     PRAGMA_OPTIMIZE_ON = 404,
     PRAGMA_OPTIMIZE_OFF = 405,
     PRAGMA_INVARIANT_ALL = 406,
     ASM = 407,
     CLASS = 408,
     UNION = 409,
     ENUM = 410,
     TYPEDEF = 411,
     TEMPLATE = 412,
     THIS = 413,
     PACKED_TOK = 414,
     GOTO = 415,
     INLINE_TOK = 416,
     NOINLINE = 417,
     VOLATILE = 418,
     PUBLIC_TOK = 419,
     STATIC = 420,
     EXTERN = 421,
     EXTERNAL = 422,
     LONG_TOK = 423,
     SHORT_TOK = 424,
     DOUBLE_TOK = 425,
     HALF = 426,
     FIXED_TOK = 427,
     UNSIGNED = 428,
     DVEC2 = 429,
     DVEC3 = 430,
     DVEC4 = 431,
     FVEC2 = 432,
     FVEC3 = 433,
     FVEC4 = 434,
     SAMPLER2DRECT = 435,
     SAMPLER3DRECT = 436,
     SAMPLER2DRECTSHADOW = 437,
     SIZEOF = 438,
     CAST = 439,
     NAMESPACE = 440,
     USING = 441,
     ERROR_TOK = 442,
     COMMON = 443,
     PARTITION = 444,
     ACTIVE = 445,
     SAMPLERBUFFER = 446,
     FILTER = 447,
     IMAGE1D = 448,
     IMAGE2D = 449,
     IMAGE3D = 450,
     IMAGECUBE = 451,
     IMAGE1DARRAY = 452,
     IMAGE2DARRAY = 453,
     IIMAGE1D = 454,
     IIMAGE2D = 455,
     IIMAGE3D = 456,
     IIMAGECUBE = 457,
     IIMAGE1DARRAY = 458,
     IIMAGE2DARRAY = 459,
     UIMAGE1D = 460,
     UIMAGE2D = 461,
     UIMAGE3D = 462,
     UIMAGECUBE = 463,
     UIMAGE1DARRAY = 464,
     UIMAGE2DARRAY = 465,
     IMAGE1DSHADOW = 466,
     IMAGE2DSHADOW = 467,
     IMAGEBUFFER = 468,
     IIMAGEBUFFER = 469,
     UIMAGEBUFFER = 470,
     IMAGE1DARRAYSHADOW = 471,
     IMAGE2DARRAYSHADOW = 472,
     ROW_MAJOR = 473,
     COLUMN_MAJOR = 474
   };
#endif



#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
{

/* Line 293 of yacc.c  */
#line 61 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"

   int n;
   float real;
   const char *identifier;
   const char *string_literal;

   struct ast_type_qualifier type_qualifier;

   ast_node *node;
   ast_type_specifier *type_specifier;
   ast_fully_specified_type *fully_specified_type;
   ast_function *function;
   ast_parameter_declarator *parameter_declarator;
   ast_function_definition *function_definition;
   ast_compound_statement *compound_statement;
   ast_expression *expression;
   ast_declarator_list *declarator_list;
   ast_struct_specifier *struct_specifier;
   ast_declaration *declaration;
   ast_switch_body *switch_body;
   ast_case_label *case_label;
   ast_case_label_list *case_label_list;
   ast_case_statement *case_statement;
   ast_case_statement_list *case_statement_list;

   struct {
	  ast_node *cond;
	  ast_expression *rest;
   } for_rest_statement;

   struct {
	  ast_node *then_statement;
	  ast_node *else_statement;
   } selection_rest_statement;

   ast_attribute* attribute;
   ast_attribute_arg* attribute_arg;



/* Line 293 of yacc.c  */
#line 421 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.inl"
} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif

#if ! defined YYLTYPE && ! defined YYLTYPE_IS_DECLARED
typedef struct YYLTYPE
{
  int first_line;
  int first_column;
  int last_line;
  int last_column;
} YYLTYPE;
# define yyltype YYLTYPE /* obsolescent; will be withdrawn */
# define YYLTYPE_IS_DECLARED 1
# define YYLTYPE_IS_TRIVIAL 1
#endif


/* Copy the second part of user declarations.  */


/* Line 343 of yacc.c  */
#line 446 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.inl"

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
#elif (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
typedef signed char yytype_int8;
#else
typedef short int yytype_int8;
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
# elif ! defined YYSIZE_T && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
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
#   define YY_(msgid) dgettext ("bison-runtime", msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(msgid) msgid
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(e) ((void) (e))
#else
# define YYUSE(e) /* empty */
#endif

/* Identity function, used to suppress warnings about constant conditions.  */
#ifndef lint
# define YYID(n) (n)
#else
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static int
YYID (int yyi)
#else
static int
YYID (yyi)
    int yyi;
#endif
{
  return yyi;
}
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
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's `empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (YYID (0))
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
#   if ! defined malloc && ! defined EXIT_SUCCESS && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* ! defined yyoverflow || YYERROR_VERBOSE */


#if (! defined yyoverflow \
     && (! defined __cplusplus \
	 || (defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL \
	     && defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yytype_int16 yyss_alloc;
  YYSTYPE yyvs_alloc;
  YYLTYPE yyls_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE) + sizeof (YYLTYPE)) \
      + 2 * YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)				\
    do									\
      {									\
	YYSIZE_T yynewbytes;						\
	YYCOPY (&yyptr->Stack_alloc, Stack, yysize);			\
	Stack = &yyptr->Stack_alloc;					\
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (YYID (0))

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from FROM to TO.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(To, From, Count) \
      __builtin_memcpy (To, From, (Count) * sizeof (*(From)))
#  else
#   define YYCOPY(To, From, Count)		\
      do					\
	{					\
	  YYSIZE_T yyi;				\
	  for (yyi = 0; yyi < (Count); yyi++)	\
	    (To)[yyi] = (From)[yyi];		\
	}					\
      while (YYID (0))
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  3
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   6245

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  244
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  104
/* YYNRULES -- Number of rules.  */
#define YYNRULES  368
/* YYNRULES -- Number of states.  */
#define YYNSTATES  571

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   474

#define YYTRANSLATE(YYX)						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,   228,     2,     2,     2,   232,   235,     2,
     220,   221,   230,   226,   225,   227,   224,   231,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,   239,   241,
     233,   240,   234,   238,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,   222,     2,   223,   236,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,   242,   237,   243,   229,     2,     2,     2,
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
     135,   136,   137,   138,   139,   140,   141,   142,   143,   144,
     145,   146,   147,   148,   149,   150,   151,   152,   153,   154,
     155,   156,   157,   158,   159,   160,   161,   162,   163,   164,
     165,   166,   167,   168,   169,   170,   171,   172,   173,   174,
     175,   176,   177,   178,   179,   180,   181,   182,   183,   184,
     185,   186,   187,   188,   189,   190,   191,   192,   193,   194,
     195,   196,   197,   198,   199,   200,   201,   202,   203,   204,
     205,   206,   207,   208,   209,   210,   211,   212,   213,   214,
     215,   216,   217,   218,   219
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const yytype_uint16 yyprhs[] =
{
       0,     0,     3,     4,     7,     9,    11,    13,    15,    18,
      20,    22,    24,    26,    28,    30,    32,    36,    38,    43,
      45,    49,    52,    55,    57,    59,    61,    65,    68,    71,
      74,    76,    79,    83,    86,    88,    90,    92,    95,    98,
     101,   103,   106,   110,   113,   115,   118,   121,   124,   129,
     135,   141,   143,   145,   147,   149,   151,   155,   159,   163,
     165,   169,   173,   175,   179,   183,   185,   189,   193,   197,
     201,   203,   207,   211,   213,   217,   219,   223,   225,   229,
     231,   235,   237,   241,   243,   249,   251,   255,   257,   259,
     261,   263,   265,   267,   269,   271,   273,   275,   277,   279,
     283,   285,   288,   290,   293,   296,   301,   303,   305,   308,
     312,   316,   321,   324,   329,   334,   337,   345,   349,   353,
     358,   361,   364,   366,   370,   373,   375,   377,   379,   382,
     385,   387,   389,   391,   393,   395,   397,   399,   403,   409,
     413,   421,   427,   433,   435,   438,   443,   446,   453,   458,
     463,   466,   468,   471,   473,   475,   477,   479,   482,   485,
     487,   489,   491,   493,   495,   498,   501,   505,   507,   509,
     511,   514,   516,   518,   521,   524,   526,   528,   530,   532,
     535,   538,   540,   542,   544,   546,   548,   552,   556,   561,
     566,   568,   570,   577,   582,   587,   592,   597,   602,   607,
     612,   617,   624,   631,   633,   635,   637,   639,   641,   643,
     645,   647,   649,   651,   653,   655,   657,   659,   661,   663,
     665,   667,   669,   671,   673,   675,   677,   679,   681,   683,
     685,   687,   689,   691,   693,   695,   697,   699,   701,   703,
     705,   707,   709,   711,   713,   715,   717,   719,   721,   723,
     725,   727,   729,   731,   733,   735,   737,   739,   741,   743,
     745,   747,   749,   751,   753,   755,   757,   759,   761,   763,
     765,   771,   779,   784,   789,   796,   800,   806,   811,   813,
     816,   820,   822,   825,   827,   830,   833,   835,   837,   841,
     843,   845,   849,   856,   861,   866,   868,   872,   874,   878,
     881,   883,   885,   887,   890,   893,   895,   897,   899,   901,
     903,   905,   908,   909,   914,   916,   918,   921,   925,   927,
     930,   932,   935,   941,   945,   947,   949,   954,   960,   963,
     967,   971,   974,   976,   979,   982,   985,   987,   990,   996,
    1004,  1011,  1013,  1015,  1017,  1018,  1021,  1025,  1028,  1031,
    1034,  1038,  1041,  1043,  1045,  1047,  1049,  1051,  1055,  1059,
    1063,  1070,  1077,  1080,  1082,  1085,  1089,  1091,  1094
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int16 yyrhs[] =
{
     245,     0,    -1,    -1,   246,   248,    -1,   110,    -1,   111,
      -1,   112,    -1,   341,    -1,   248,   341,    -1,   110,    -1,
     112,    -1,   249,    -1,   114,    -1,   115,    -1,   113,    -1,
     116,    -1,   220,   279,   221,    -1,   250,    -1,   251,   222,
     252,   223,    -1,   253,    -1,   251,   224,   247,    -1,   251,
     121,    -1,   251,   122,    -1,   279,    -1,   254,    -1,   255,
      -1,   251,   224,   260,    -1,   257,   221,    -1,   256,   221,
      -1,   258,   107,    -1,   258,    -1,   258,   277,    -1,   257,
     225,   277,    -1,   259,   220,    -1,   298,    -1,   249,    -1,
     118,    -1,   262,   221,    -1,   261,   221,    -1,   263,   107,
      -1,   263,    -1,   263,   277,    -1,   262,   225,   277,    -1,
     249,   220,    -1,   251,    -1,   121,   264,    -1,   122,   264,
      -1,   265,   264,    -1,   220,   301,   221,   264,    -1,   220,
       3,   301,   221,   264,    -1,   220,   301,     3,   221,   264,
      -1,   226,    -1,   227,    -1,   228,    -1,   229,    -1,   264,
      -1,   266,   230,   264,    -1,   266,   231,   264,    -1,   266,
     232,   264,    -1,   266,    -1,   267,   226,   266,    -1,   267,
     227,   266,    -1,   267,    -1,   268,   119,   267,    -1,   268,
     120,   267,    -1,   268,    -1,   269,   233,   268,    -1,   269,
     234,   268,    -1,   269,   123,   268,    -1,   269,   124,   268,
      -1,   269,    -1,   270,   125,   269,    -1,   270,   126,   269,
      -1,   270,    -1,   271,   235,   270,    -1,   271,    -1,   272,
     236,   271,    -1,   272,    -1,   273,   237,   272,    -1,   273,
      -1,   274,   127,   273,    -1,   274,    -1,   275,   128,   274,
      -1,   275,    -1,   275,   238,   279,   239,   277,    -1,   276,
      -1,   264,   278,   277,    -1,   240,    -1,   129,    -1,   130,
      -1,   132,    -1,   131,    -1,   138,    -1,   133,    -1,   134,
      -1,   135,    -1,   136,    -1,   137,    -1,   277,    -1,   279,
     225,   277,    -1,   276,    -1,   283,   241,    -1,   281,    -1,
     291,   241,    -1,   284,   221,    -1,   284,   221,   239,   247,
      -1,   286,    -1,   285,    -1,   286,   288,    -1,   285,   225,
     288,    -1,   293,   249,   220,    -1,   161,   293,   249,   220,
      -1,   298,   247,    -1,   298,   247,   240,   280,    -1,   298,
     247,   239,   247,    -1,   298,   315,    -1,   298,   247,   222,
     280,   223,   239,   247,    -1,   295,   289,   287,    -1,   289,
     295,   287,    -1,   295,   289,   295,   287,    -1,   289,   287,
      -1,   295,   287,    -1,   287,    -1,   295,   289,   290,    -1,
     289,   290,    -1,    35,    -1,    36,    -1,    37,    -1,    35,
      36,    -1,    36,    35,    -1,    96,    -1,    97,    -1,    98,
      -1,    99,    -1,   100,    -1,   298,    -1,   292,    -1,   291,
     225,   247,    -1,   291,   225,   247,   222,   223,    -1,   291,
     225,   315,    -1,   291,   225,   247,   222,   223,   240,   316,
      -1,   291,   225,   315,   240,   316,    -1,   291,   225,   247,
     240,   316,    -1,   293,    -1,   293,   247,    -1,   293,   247,
     222,   223,    -1,   293,   315,    -1,   293,   247,   222,   223,
     240,   316,    -1,   293,   315,   240,   316,    -1,   293,   247,
     240,   316,    -1,   139,   249,    -1,   298,    -1,   296,   298,
      -1,    45,    -1,    44,    -1,    43,    -1,   294,    -1,    42,
     294,    -1,   294,    42,    -1,    42,    -1,     3,    -1,    38,
      -1,   297,    -1,   294,    -1,   294,   297,    -1,   139,   297,
      -1,   139,   294,   297,    -1,   139,    -1,     3,    -1,    39,
      -1,    42,    39,    -1,    35,    -1,    36,    -1,    42,    35,
      -1,    42,    36,    -1,    38,    -1,   218,    -1,   219,    -1,
     165,    -1,     3,   165,    -1,   165,     3,    -1,    40,    -1,
      41,    -1,   299,    -1,   301,    -1,   300,    -1,   301,   222,
     223,    -1,   300,   222,   223,    -1,   301,   222,   280,   223,
      -1,   300,   222,   280,   223,    -1,   302,    -1,   303,    -1,
     303,   233,   302,   225,   114,   234,    -1,   303,   233,   302,
     234,    -1,    82,   233,   302,   234,    -1,    82,   233,   307,
     234,    -1,    82,   233,   111,   234,    -1,    94,   233,   302,
     234,    -1,    94,   233,   307,   234,    -1,    94,   233,   111,
     234,    -1,   304,   233,   111,   234,    -1,   305,   233,   111,
     225,   114,   234,    -1,   306,   233,   111,   225,   114,   234,
      -1,   307,    -1,   111,    -1,   107,    -1,     5,    -1,    31,
      -1,     6,    -1,     7,    -1,     4,    -1,    28,    -1,    29,
      -1,    30,    -1,    32,    -1,    33,    -1,    34,    -1,    19,
      -1,    20,    -1,    21,    -1,    22,    -1,    23,    -1,    24,
      -1,    25,    -1,    26,    -1,    27,    -1,    46,    -1,    47,
      -1,    48,    -1,    49,    -1,    50,    -1,    51,    -1,    52,
      -1,    53,    -1,    54,    -1,    55,    -1,    56,    -1,    57,
      -1,    58,    -1,    59,    -1,    60,    -1,    61,    -1,    62,
      -1,    63,    -1,    73,    -1,    74,    -1,    81,    -1,    83,
      -1,    75,    -1,    76,    -1,    77,    -1,    78,    -1,    79,
      -1,    80,    -1,    84,    -1,    85,    -1,    86,    -1,    87,
      -1,    88,    -1,    95,    -1,    89,    -1,    90,    -1,    91,
      -1,    92,    -1,    93,    -1,   101,    -1,   102,    -1,   103,
      -1,   104,    -1,   105,    -1,   106,   247,   242,   309,   243,
      -1,   106,   247,   239,   111,   242,   309,   243,    -1,   106,
     242,   309,   243,    -1,   106,   247,   242,   243,    -1,   106,
     247,   239,   111,   242,   243,    -1,   106,   242,   243,    -1,
     109,   247,   242,   309,   243,    -1,   109,   247,   242,   243,
      -1,   310,    -1,   309,   310,    -1,   311,   313,   241,    -1,
     298,    -1,   312,   298,    -1,   294,    -1,    42,   294,    -1,
     294,    42,    -1,    42,    -1,   314,    -1,   313,   225,   314,
      -1,   247,    -1,   315,    -1,   247,   239,   247,    -1,   247,
     222,   280,   223,   239,   247,    -1,   247,   222,   280,   223,
      -1,   315,   222,   280,   223,    -1,   277,    -1,   242,   317,
     243,    -1,   316,    -1,   317,   225,   316,    -1,   317,   225,
      -1,   282,    -1,   321,    -1,   320,    -1,   345,   321,    -1,
     345,   320,    -1,   318,    -1,   326,    -1,   327,    -1,   330,
      -1,   336,    -1,   340,    -1,   242,   243,    -1,    -1,   242,
     322,   325,   243,    -1,   324,    -1,   320,    -1,   242,   243,
      -1,   242,   325,   243,    -1,   319,    -1,   325,   319,    -1,
     241,    -1,   279,   241,    -1,    13,   220,   279,   221,   328,
      -1,   319,    11,   319,    -1,   319,    -1,   279,    -1,   293,
     247,   240,   316,    -1,    16,   220,   279,   221,   331,    -1,
     242,   243,    -1,   242,   335,   243,    -1,    17,   279,   239,
      -1,    18,   239,    -1,   332,    -1,   333,   332,    -1,   333,
     319,    -1,   334,   319,    -1,   334,    -1,   335,   334,    -1,
     108,   220,   329,   221,   323,    -1,    10,   319,   108,   220,
     279,   221,   241,    -1,    12,   220,   337,   339,   221,   323,
      -1,   326,    -1,   318,    -1,   329,    -1,    -1,   338,   241,
      -1,   338,   241,   279,    -1,     9,   241,    -1,     8,   241,
      -1,    15,   241,    -1,    15,   279,   241,    -1,    14,   241,
      -1,   346,    -1,   347,    -1,   280,    -1,   117,    -1,   342,
      -1,   343,   225,   342,    -1,   222,   247,   223,    -1,   222,
     302,   223,    -1,   222,   247,   220,   343,   221,   223,    -1,
     222,   302,   220,   343,   221,   223,    -1,   345,   344,    -1,
     344,    -1,   283,   324,    -1,   345,   283,   324,    -1,   281,
      -1,   291,   241,    -1,   308,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   279,   279,   279,   291,   292,   293,   297,   305,   316,
     317,   321,   328,   335,   349,   356,   363,   370,   371,   377,
     381,   388,   394,   403,   407,   411,   412,   421,   422,   426,
     427,   431,   437,   449,   453,   459,   466,   476,   477,   481,
     482,   486,   492,   504,   515,   516,   522,   528,   534,   541,
     548,   559,   560,   561,   562,   566,   567,   573,   579,   588,
     589,   595,   604,   605,   611,   620,   621,   627,   633,   639,
     648,   649,   655,   664,   665,   674,   675,   684,   685,   694,
     695,   704,   705,   714,   715,   724,   725,   734,   735,   736,
     737,   738,   739,   740,   741,   742,   743,   744,   748,   752,
     768,   772,   780,   784,   791,   794,   802,   803,   807,   812,
     820,   831,   845,   855,   866,   877,   889,   905,   912,   919,
     927,   932,   937,   938,   949,   961,   966,   971,   977,   983,
     989,   994,   999,  1004,  1009,  1017,  1021,  1022,  1032,  1042,
    1051,  1061,  1071,  1085,  1092,  1101,  1110,  1119,  1128,  1138,
    1147,  1161,  1168,  1179,  1184,  1189,  1197,  1198,  1203,  1208,
    1213,  1218,  1226,  1227,  1228,  1233,  1238,  1244,  1252,  1257,
    1262,  1268,  1273,  1278,  1283,  1288,  1293,  1298,  1303,  1308,
    1314,  1320,  1325,  1333,  1340,  1341,  1345,  1351,  1356,  1362,
    1371,  1377,  1383,  1390,  1396,  1402,  1408,  1414,  1420,  1426,
    1432,  1438,  1445,  1452,  1458,  1467,  1468,  1469,  1470,  1471,
    1472,  1473,  1474,  1475,  1476,  1477,  1478,  1479,  1480,  1481,
    1482,  1483,  1484,  1485,  1486,  1487,  1488,  1489,  1490,  1491,
    1492,  1493,  1494,  1495,  1496,  1497,  1498,  1499,  1500,  1501,
    1502,  1503,  1504,  1505,  1506,  1507,  1542,  1543,  1544,  1545,
    1546,  1547,  1548,  1549,  1550,  1551,  1552,  1553,  1554,  1555,
    1556,  1557,  1558,  1559,  1560,  1564,  1565,  1566,  1570,  1574,
    1578,  1585,  1592,  1598,  1605,  1612,  1621,  1627,  1635,  1640,
    1648,  1658,  1665,  1676,  1677,  1682,  1687,  1695,  1700,  1708,
    1715,  1720,  1728,  1738,  1744,  1753,  1754,  1763,  1768,  1773,
    1780,  1786,  1787,  1788,  1793,  1801,  1802,  1803,  1804,  1805,
    1806,  1810,  1817,  1816,  1830,  1831,  1835,  1841,  1850,  1860,
    1872,  1878,  1887,  1896,  1901,  1909,  1913,  1931,  1939,  1944,
    1952,  1957,  1965,  1973,  1981,  1989,  1997,  2005,  2013,  2020,
    2027,  2037,  2038,  2042,  2044,  2050,  2055,  2064,  2070,  2076,
    2082,  2088,  2097,  2098,  2102,  2108,  2117,  2121,  2129,  2135,
    2141,  2148,  2158,  2163,  2170,  2180,  2194,  2198,  2206
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "CONST_TOK", "BOOL_TOK", "FLOAT_TOK",
  "INT_TOK", "UINT_TOK", "BREAK", "CONTINUE", "DO", "ELSE", "FOR", "IF",
  "DISCARD", "RETURN", "SWITCH", "CASE", "DEFAULT", "BVEC2", "BVEC3",
  "BVEC4", "IVEC2", "IVEC3", "IVEC4", "UVEC2", "UVEC3", "UVEC4", "VEC2",
  "VEC3", "VEC4", "HALF_TOK", "HVEC2", "HVEC3", "HVEC4", "IN_TOK",
  "OUT_TOK", "INOUT_TOK", "UNIFORM", "VARYING", "GLOBALLYCOHERENT",
  "SHARED", "CENTROID", "NOPERSPECTIVE", "NOINTERPOLATION", "LINEAR",
  "MAT2X2", "MAT2X3", "MAT2X4", "MAT3X2", "MAT3X3", "MAT3X4", "MAT4X2",
  "MAT4X3", "MAT4X4", "HMAT2X2", "HMAT2X3", "HMAT2X4", "HMAT3X2",
  "HMAT3X3", "HMAT3X4", "HMAT4X2", "HMAT4X3", "HMAT4X4", "FMAT2X2",
  "FMAT2X3", "FMAT2X4", "FMAT3X2", "FMAT3X3", "FMAT3X4", "FMAT4X2",
  "FMAT4X3", "FMAT4X4", "SAMPLERSTATE", "SAMPLERSTATE_CMP", "TEXTURE1D",
  "TEXTURE1D_ARRAY", "TEXTURE2D", "TEXTURE2D_ARRAY", "TEXTURE2DMS",
  "TEXTURE_EXTERNAL", "BUFFER", "STRUCTUREDBUFFER", "BYTEADDRESSBUFFER",
  "TEXTURE2DMS_ARRAY", "TEXTURE3D", "TEXTURECUBE", "TEXTURECUBE_ARRAY",
  "RWBUFFER", "RWTEXTURE1D", "RWTEXTURE1D_ARRAY", "RWTEXTURE2D",
  "RWTEXTURE2D_ARRAY", "RWTEXTURE3D", "RWSTRUCTUREDBUFFER",
  "RWBYTEADDRESSBUFFER", "POINT_TOK", "LINE_TOK", "TRIANGLE_TOK",
  "LINEADJ_TOK", "TRIANGLEADJ_TOK", "POINTSTREAM", "LINESTREAM",
  "TRIANGLESTREAM", "INPUTPATCH", "OUTPUTPATCH", "STRUCT", "VOID_TOK",
  "WHILE", "CBUFFER", "IDENTIFIER", "TYPE_IDENTIFIER", "NEW_IDENTIFIER",
  "FLOATCONSTANT", "INTCONSTANT", "UINTCONSTANT", "BOOLCONSTANT",
  "STRINGCONSTANT", "FIELD_SELECTION", "LEFT_OP", "RIGHT_OP", "INC_OP",
  "DEC_OP", "LE_OP", "GE_OP", "EQ_OP", "NE_OP", "AND_OP", "OR_OP",
  "MUL_ASSIGN", "DIV_ASSIGN", "ADD_ASSIGN", "MOD_ASSIGN", "LEFT_ASSIGN",
  "RIGHT_ASSIGN", "AND_ASSIGN", "XOR_ASSIGN", "OR_ASSIGN", "SUB_ASSIGN",
  "INVARIANT", "VERSION_TOK", "EXTENSION", "LINE", "COLON", "EOL",
  "INTERFACE", "OUTPUT", "PRAGMA_DEBUG_ON", "PRAGMA_DEBUG_OFF",
  "PRAGMA_OPTIMIZE_ON", "PRAGMA_OPTIMIZE_OFF", "PRAGMA_INVARIANT_ALL",
  "ASM", "CLASS", "UNION", "ENUM", "TYPEDEF", "TEMPLATE", "THIS",
  "PACKED_TOK", "GOTO", "INLINE_TOK", "NOINLINE", "VOLATILE", "PUBLIC_TOK",
  "STATIC", "EXTERN", "EXTERNAL", "LONG_TOK", "SHORT_TOK", "DOUBLE_TOK",
  "HALF", "FIXED_TOK", "UNSIGNED", "DVEC2", "DVEC3", "DVEC4", "FVEC2",
  "FVEC3", "FVEC4", "SAMPLER2DRECT", "SAMPLER3DRECT",
  "SAMPLER2DRECTSHADOW", "SIZEOF", "CAST", "NAMESPACE", "USING",
  "ERROR_TOK", "COMMON", "PARTITION", "ACTIVE", "SAMPLERBUFFER", "FILTER",
  "IMAGE1D", "IMAGE2D", "IMAGE3D", "IMAGECUBE", "IMAGE1DARRAY",
  "IMAGE2DARRAY", "IIMAGE1D", "IIMAGE2D", "IIMAGE3D", "IIMAGECUBE",
  "IIMAGE1DARRAY", "IIMAGE2DARRAY", "UIMAGE1D", "UIMAGE2D", "UIMAGE3D",
  "UIMAGECUBE", "UIMAGE1DARRAY", "UIMAGE2DARRAY", "IMAGE1DSHADOW",
  "IMAGE2DSHADOW", "IMAGEBUFFER", "IIMAGEBUFFER", "UIMAGEBUFFER",
  "IMAGE1DARRAYSHADOW", "IMAGE2DARRAYSHADOW", "ROW_MAJOR", "COLUMN_MAJOR",
  "'('", "')'", "'['", "']'", "'.'", "','", "'+'", "'-'", "'!'", "'~'",
  "'*'", "'/'", "'%'", "'<'", "'>'", "'&'", "'^'", "'|'", "'?'", "':'",
  "'='", "';'", "'{'", "'}'", "$accept", "translation_unit", "$@1",
  "any_identifier", "external_declaration_list", "variable_identifier",
  "primary_expression", "postfix_expression", "integer_expression",
  "function_call", "function_call_or_method", "function_call_generic",
  "function_call_header_no_parameters",
  "function_call_header_with_parameters", "function_call_header",
  "function_identifier", "method_call_generic",
  "method_call_header_no_parameters", "method_call_header_with_parameters",
  "method_call_header", "unary_expression", "unary_operator",
  "multiplicative_expression", "additive_expression", "shift_expression",
  "relational_expression", "equality_expression", "and_expression",
  "exclusive_or_expression", "inclusive_or_expression",
  "logical_and_expression", "logical_or_expression",
  "conditional_expression", "assignment_expression", "assignment_operator",
  "expression", "constant_expression", "function_decl", "declaration",
  "function_prototype", "function_declarator",
  "function_header_with_parameters", "function_header",
  "parameter_declarator", "parameter_declaration", "parameter_qualifier",
  "parameter_type_specifier", "init_declarator_list", "single_declaration",
  "fully_specified_type", "interpolation_qualifier",
  "parameter_type_qualifier", "type_qualifier", "storage_qualifier",
  "type_specifier", "type_specifier_no_prec", "type_specifier_array",
  "type_specifier_nonarray", "basic_type_specifier_nonarray",
  "texture_type_specifier_nonarray",
  "outputstream_type_specifier_nonarray",
  "inputpatch_type_specifier_nonarray",
  "outputpatch_type_specifier_nonarray", "struct_specifier",
  "cbuffer_declaration", "struct_declaration_list", "struct_declaration",
  "struct_type_specifier", "struct_type_qualifier",
  "struct_declarator_list", "struct_declarator", "array_identifier",
  "initializer", "initializer_list", "declaration_statement", "statement",
  "simple_statement", "compound_statement", "$@2",
  "statement_no_new_scope", "compound_statement_no_new_scope",
  "statement_list", "expression_statement", "selection_statement",
  "selection_rest_statement", "condition", "switch_statement",
  "switch_body", "case_label", "case_label_list", "case_statement",
  "case_statement_list", "iteration_statement", "for_init_statement",
  "conditionopt", "for_rest_statement", "jump_statement",
  "external_declaration", "attribute_arg", "attribute_arg_list",
  "attribute", "attribute_list", "function_definition",
  "global_declaration", 0
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[YYLEX-NUM] -- Internal token number corresponding to
   token YYLEX-NUM.  */
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
     395,   396,   397,   398,   399,   400,   401,   402,   403,   404,
     405,   406,   407,   408,   409,   410,   411,   412,   413,   414,
     415,   416,   417,   418,   419,   420,   421,   422,   423,   424,
     425,   426,   427,   428,   429,   430,   431,   432,   433,   434,
     435,   436,   437,   438,   439,   440,   441,   442,   443,   444,
     445,   446,   447,   448,   449,   450,   451,   452,   453,   454,
     455,   456,   457,   458,   459,   460,   461,   462,   463,   464,
     465,   466,   467,   468,   469,   470,   471,   472,   473,   474,
      40,    41,    91,    93,    46,    44,    43,    45,    33,   126,
      42,    47,    37,    60,    62,    38,    94,   124,    63,    58,
      61,    59,   123,   125
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint16 yyr1[] =
{
       0,   244,   246,   245,   247,   247,   247,   248,   248,   249,
     249,   250,   250,   250,   250,   250,   250,   251,   251,   251,
     251,   251,   251,   252,   253,   254,   254,   255,   255,   256,
     256,   257,   257,   258,   259,   259,   259,   260,   260,   261,
     261,   262,   262,   263,   264,   264,   264,   264,   264,   264,
     264,   265,   265,   265,   265,   266,   266,   266,   266,   267,
     267,   267,   268,   268,   268,   269,   269,   269,   269,   269,
     270,   270,   270,   271,   271,   272,   272,   273,   273,   274,
     274,   275,   275,   276,   276,   277,   277,   278,   278,   278,
     278,   278,   278,   278,   278,   278,   278,   278,   279,   279,
     280,   281,   282,   282,   283,   283,   284,   284,   285,   285,
     286,   286,   287,   287,   287,   287,   287,   288,   288,   288,
     288,   288,   288,   288,   288,   289,   289,   289,   289,   289,
     289,   289,   289,   289,   289,   290,   291,   291,   291,   291,
     291,   291,   291,   292,   292,   292,   292,   292,   292,   292,
     292,   293,   293,   294,   294,   294,   295,   295,   295,   295,
     295,   295,   296,   296,   296,   296,   296,   296,   297,   297,
     297,   297,   297,   297,   297,   297,   297,   297,   297,   297,
     297,   297,   297,   298,   299,   299,   300,   300,   300,   300,
     301,   301,   301,   301,   301,   301,   301,   301,   301,   301,
     301,   301,   301,   301,   301,   302,   302,   302,   302,   302,
     302,   302,   302,   302,   302,   302,   302,   302,   302,   302,
     302,   302,   302,   302,   302,   302,   302,   302,   302,   302,
     302,   302,   302,   302,   302,   302,   302,   302,   302,   302,
     302,   302,   302,   302,   302,   302,   303,   303,   303,   303,
     303,   303,   303,   303,   303,   303,   303,   303,   303,   303,
     303,   303,   303,   303,   303,   304,   304,   304,   305,   306,
     307,   307,   307,   307,   307,   307,   308,   308,   309,   309,
     310,   311,   311,   312,   312,   312,   312,   313,   313,   314,
     314,   314,   314,   315,   315,   316,   316,   317,   317,   317,
     318,   319,   319,   319,   319,   320,   320,   320,   320,   320,
     320,   321,   322,   321,   323,   323,   324,   324,   325,   325,
     326,   326,   327,   328,   328,   329,   329,   330,   331,   331,
     332,   332,   333,   333,   334,   334,   335,   335,   336,   336,
     336,   337,   337,   338,   338,   339,   339,   340,   340,   340,
     340,   340,   341,   341,   342,   342,   343,   343,   344,   344,
     344,   344,   345,   345,   346,   346,   347,   347,   347
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     0,     2,     1,     1,     1,     1,     2,     1,
       1,     1,     1,     1,     1,     1,     3,     1,     4,     1,
       3,     2,     2,     1,     1,     1,     3,     2,     2,     2,
       1,     2,     3,     2,     1,     1,     1,     2,     2,     2,
       1,     2,     3,     2,     1,     2,     2,     2,     4,     5,
       5,     1,     1,     1,     1,     1,     3,     3,     3,     1,
       3,     3,     1,     3,     3,     1,     3,     3,     3,     3,
       1,     3,     3,     1,     3,     1,     3,     1,     3,     1,
       3,     1,     3,     1,     5,     1,     3,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     3,
       1,     2,     1,     2,     2,     4,     1,     1,     2,     3,
       3,     4,     2,     4,     4,     2,     7,     3,     3,     4,
       2,     2,     1,     3,     2,     1,     1,     1,     2,     2,
       1,     1,     1,     1,     1,     1,     1,     3,     5,     3,
       7,     5,     5,     1,     2,     4,     2,     6,     4,     4,
       2,     1,     2,     1,     1,     1,     1,     2,     2,     1,
       1,     1,     1,     1,     2,     2,     3,     1,     1,     1,
       2,     1,     1,     2,     2,     1,     1,     1,     1,     2,
       2,     1,     1,     1,     1,     1,     3,     3,     4,     4,
       1,     1,     6,     4,     4,     4,     4,     4,     4,     4,
       4,     6,     6,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       5,     7,     4,     4,     6,     3,     5,     4,     1,     2,
       3,     1,     2,     1,     2,     2,     1,     1,     3,     1,
       1,     3,     6,     4,     4,     1,     3,     1,     3,     2,
       1,     1,     1,     2,     2,     1,     1,     1,     1,     1,
       1,     2,     0,     4,     1,     1,     2,     3,     1,     2,
       1,     2,     5,     3,     1,     1,     4,     5,     2,     3,
       3,     2,     1,     2,     2,     2,     1,     2,     5,     7,
       6,     1,     1,     1,     0,     2,     3,     2,     2,     2,
       3,     2,     1,     1,     1,     1,     1,     3,     3,     3,
       6,     6,     2,     1,     2,     3,     1,     2,     1
};

/* YYDEFACT[STATE-NAME] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint16 yydefact[] =
{
       2,     0,     0,     1,   168,   210,   206,   208,   209,   217,
     218,   219,   220,   221,   222,   223,   224,   225,   211,   212,
     213,   207,   214,   215,   216,   171,   172,   175,   169,   181,
     182,     0,   155,   154,   153,   226,   227,   228,   229,   230,
     231,   232,   233,   234,   235,   236,   237,   238,   239,   240,
     241,   242,   243,   244,   245,   248,   249,   250,   251,   252,
     253,   246,     0,   247,   254,   255,   256,   257,   258,   260,
     261,   262,   263,   264,     0,   259,   265,   266,   267,   268,
     269,     0,   205,     0,   204,   167,     0,   178,   176,   177,
       0,     3,   366,     0,     0,   107,   106,     0,   136,   143,
     163,     0,   162,   151,   183,   185,   184,   190,   191,     0,
       0,     0,   203,   368,     7,   363,     0,   352,   353,   179,
     173,   174,   170,     0,     0,     4,     5,     6,     0,     0,
       0,     9,    10,   150,     0,   165,   167,     0,   180,     0,
       0,     8,   101,     0,   364,   104,     0,   160,   125,   126,
     127,   161,   159,   130,   131,   132,   133,   134,   122,   108,
       0,   156,     0,     0,     0,   367,     4,     6,   144,     0,
     146,   164,   152,     0,     0,     0,     0,     0,     0,     0,
       0,   362,     0,     0,     0,     0,     0,     0,   286,   275,
     283,   281,     0,   278,     0,     0,     0,     0,     0,   166,
       0,     0,   358,     0,   359,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    14,    12,    13,    15,    36,     0,
       0,     0,    51,    52,    53,    54,   320,   312,   316,    11,
      17,    44,    19,    24,    25,     0,     0,    30,     0,    55,
       0,    59,    62,    65,    70,    73,    75,    77,    79,    81,
      83,    85,    98,     0,   102,   300,     0,     0,   151,   305,
     318,   302,   301,     0,   306,   307,   308,   309,   310,     0,
       0,   109,   128,   129,   157,   120,   124,     0,   135,   158,
     121,     0,   112,   115,   137,   139,     0,     0,   110,     0,
       0,   187,    55,   100,     0,    34,   186,     0,     0,     0,
       0,     0,   365,   196,   194,   195,   199,   197,   198,   284,
     285,   272,   279,   289,     0,   287,   290,   282,     0,   273,
       0,   277,     0,   111,   355,   354,   356,     0,     0,   348,
     347,     0,     0,     0,   351,   349,     0,     0,     0,    45,
      46,     0,     0,   184,   311,     0,    21,    22,     0,     0,
      28,    27,     0,   205,    31,    33,    88,    89,    91,    90,
      93,    94,    95,    96,    97,    92,    87,     0,    47,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   321,
     103,   317,   319,   304,   303,   105,   118,   117,   123,     0,
       0,     0,     0,     0,     0,     0,   145,     0,     0,   295,
     149,     0,   148,   189,   188,     0,   193,   200,     0,     0,
       0,     0,     0,   280,     0,   270,   276,     0,     0,     0,
       0,   342,   341,   344,     0,   350,     0,   325,     0,     0,
       0,    16,     0,     0,     0,     0,    23,    20,     0,    26,
       0,     0,    40,    32,    86,    56,    57,    58,    60,    61,
      63,    64,    68,    69,    66,    67,    71,    72,    74,    76,
      78,    80,    82,     0,    99,   119,     0,   114,   113,   138,
     142,   141,     0,   293,   297,     0,   294,     0,     0,     0,
       0,   291,   288,   274,     0,   360,   357,   361,     0,   343,
       0,     0,     0,     0,     0,     0,     0,     0,    48,   313,
      18,    43,    38,    37,     0,   205,    41,     0,   293,     0,
     147,   299,   296,   192,   201,   202,   293,   271,     0,   345,
       0,   324,   322,     0,   327,     0,   315,   338,   314,    49,
      50,    42,    84,     0,   140,   298,     0,     0,   346,   340,
       0,     0,     0,   328,   332,     0,   336,     0,   326,   116,
     292,   339,   323,     0,   331,   334,   333,   335,   329,   337,
     330
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     1,     2,   282,    91,   229,   230,   231,   445,   232,
     233,   234,   235,   236,   237,   238,   449,   450,   451,   452,
     239,   240,   241,   242,   243,   244,   245,   246,   247,   248,
     249,   250,   251,   252,   367,   253,   325,   254,   255,   256,
      94,    95,    96,   158,   159,   160,   276,   257,    98,    99,
     100,   162,   101,   102,   295,   104,   105,   106,   107,   108,
     109,   110,   111,   112,   113,   192,   193,   194,   195,   314,
     315,   283,   410,   485,   259,   260,   261,   262,   345,   537,
     538,   263,   264,   265,   532,   439,   266,   534,   554,   555,
     556,   557,   267,   433,   500,   501,   268,   114,   326,   327,
     115,   269,   117,   118
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -267
static const yytype_int16 yypact[] =
{
    -267,    46,  5377,  -267,   -53,  -267,  -267,  -267,  -267,  -267,
    -267,  -267,  -267,  -267,  -267,  -267,  -267,  -267,  -267,  -267,
    -267,  -267,  -267,  -267,  -267,  -267,  -267,  -267,  -267,  -267,
    -267,    74,  -267,  -267,  -267,  -267,  -267,  -267,  -267,  -267,
    -267,  -267,  -267,  -267,  -267,  -267,  -267,  -267,  -267,  -267,
    -267,  -267,  -267,  -267,  -267,  -267,  -267,  -267,  -267,  -267,
    -267,  -267,  -129,  -267,  -267,  -267,  -267,  -267,  -267,  -267,
    -267,  -267,  -267,  -267,  -106,  -267,  -267,  -267,  -267,  -267,
    -267,   -74,  -267,   112,  -267,   245,  5677,    82,  -267,  -267,
     920,  5377,  -267,   -22,   -92,   -85,  5824,  -186,  -267,   117,
     491,  6134,  -267,  -267,  -267,   -72,   -67,  -267,   -88,   -57,
     -49,   -47,  -267,  -267,  -267,  -267,  5527,  -267,  -267,  -267,
    -267,  -267,  -267,  1162,  1403,  -267,  -267,  -267,  1985,  -189,
     -34,  -267,  -267,  -267,   491,  -267,   271,     7,  -267,   -21,
     -20,  -267,  -267,   556,  -267,     1,  5824,  -267,   155,   163,
    -267,  -267,   187,  -267,  -267,  -267,  -267,  -267,  -267,  -267,
    5933,   175,  6026,   112,   112,  -267,    17,    34,  -174,    38,
    -173,  -267,  -267,  3878,  4062,   707,   149,   153,   157,    29,
       7,  -267,    44,    48,    57,    63,    68,    87,   187,  -267,
     266,  -267,  2078,  -267,   112,  6134,   211,  2187,  2280,  -267,
     103,  4246,  -267,  4246,  -267,    83,    86,  1519,   108,   109,
      90,  3249,   113,   118,  -267,  -267,  -267,  -267,  -267,  4798,
    4798,  3694,  -267,  -267,  -267,  -267,  -267,    94,  -267,   119,
    -267,   -61,  -267,  -267,  -267,   120,   -59,  4982,   122,   385,
    4798,    19,   -62,   -41,   -80,    89,   105,   123,   116,   227,
    -101,  -267,  -267,  -139,  -267,  -267,   115,  -138,   138,  -267,
    -267,  -267,  -267,   797,  -267,  -267,  -267,  -267,  -267,  1519,
     112,  -267,  -267,  -267,  -267,  -267,  -267,  6134,   112,  -267,
    -267,  5933,  -175,   139,  -170,  -168,  4430,  3023,  -267,  4798,
    3023,  -267,  -267,  -267,   141,  -267,  -267,   142,  -152,   132,
     143,   144,  -267,  -267,  -267,  -267,  -267,  -267,  -267,  -267,
    -267,  -267,  -267,  -146,  -125,  -267,   139,  -267,   125,  -267,
    2389,  -267,  2482,  -267,  -267,  -267,  -267,   -51,   -50,  -267,
    -267,   263,  2797,  4798,  -267,  -267,  -118,  4798,  3476,  -267,
    -267,  6134,   -48,    12,  -267,  1519,  -267,  -267,  4798,   117,
    -267,  -267,  4798,   151,  -267,  -267,  -267,  -267,  -267,  -267,
    -267,  -267,  -267,  -267,  -267,  -267,  -267,  4798,  -267,  4798,
    4798,  4798,  4798,  4798,  4798,  4798,  4798,  4798,  4798,  4798,
    4798,  4798,  4798,  4798,  4798,  4798,  4798,  4798,  4798,  -267,
    -267,  -267,  -267,  -267,  -267,  -267,  -267,  -267,  -267,  6134,
    4798,   112,  4798,  4614,  3023,  3023,   133,   170,  3023,  -267,
    -267,   171,  -267,  -267,  -267,   260,  -267,  -267,   281,   282,
    4798,   112,   112,  -267,  2591,  -267,  -267,   177,  4246,   178,
     179,  -267,  -267,  3476,   -43,  -267,   -40,   173,   112,   181,
     182,  -267,   183,  4798,  1038,   184,   173,  -267,   185,  -267,
     190,   -38,  5166,  -267,  -267,  -267,  -267,  -267,    19,    19,
     -62,   -62,   -41,   -41,   -41,   -41,   -80,   -80,    89,   105,
     123,   116,   227,  -148,  -267,  -267,   189,  -267,  -267,   174,
    -267,  -267,  3023,  -267,  -267,  -163,  -267,   186,   191,   193,
     192,  -267,  -267,  -267,  2684,  -267,  -267,  -267,  4798,  -267,
     172,   195,  1519,   188,   197,  1759,  4798,  4798,  -267,  -267,
    -267,  -267,  -267,  -267,  4798,   198,  -267,  4798,   194,  3023,
    -267,  3023,  -267,  -267,  -267,  -267,   196,  -267,   -24,  4798,
    1759,   407,  -267,    -8,  -267,  3023,  -267,  -267,  -267,  -267,
    -267,  -267,  -267,   112,  -267,  -267,   112,   180,   173,  -267,
    1519,  4798,   199,  -267,  -267,  1279,  1519,    14,  -267,  -267,
    -267,  -267,  -267,  -144,  -267,  -267,  -267,  -267,  -267,  1519,
    -267
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -267,  -267,  -267,   -76,  -267,   -79,  -267,  -267,  -267,  -267,
    -267,  -267,  -267,  -267,  -267,  -267,  -267,  -267,  -267,  -267,
       6,  -267,  -110,   -98,  -166,   -87,    41,    45,    55,    39,
      54,  -267,   -68,  -216,  -267,  -181,  -161,    31,  -267,    43,
    -267,  -267,  -267,  -134,   283,   270,   160,    33,  -267,   -82,
     -77,  -135,  -267,   -44,    -2,  -267,  -267,  -192,   -27,  -267,
    -267,  -267,  -267,   176,  -267,  -180,  -190,  -267,  -267,  -267,
      21,   -96,  -266,  -267,   114,  -206,  -258,   200,  -267,   -86,
     -71,   100,   121,  -267,  -267,    15,  -267,  -267,  -108,  -267,
    -107,  -267,  -267,  -267,  -267,  -267,  -267,   360,    24,   251,
    -100,    40,  -267,  -267
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -40
static const yytype_int16 yytable[] =
{
     103,   331,   312,   170,   137,   129,   133,   130,   134,   551,
     552,   393,   294,   297,   139,   442,   181,   320,   322,   161,
     169,   354,   144,   168,   412,   277,   275,   386,   280,   343,
     336,   551,   552,    92,   180,    97,   125,   126,   127,   164,
     342,   135,   116,   376,   377,    93,     3,   400,   286,   289,
     196,   190,   403,   197,   289,   165,   171,   392,   200,   134,
     346,   347,   521,   140,   401,   402,   287,   290,   285,   161,
     404,   409,   405,   415,   409,   274,   420,   388,   374,   375,
     522,   388,   416,   161,   103,   138,   388,   164,   284,   103,
     199,   517,   135,   421,   163,   570,   183,   186,   316,   172,
     422,   169,   389,   390,   123,   293,   293,   388,   302,   120,
     121,   309,   119,   122,   103,   190,   423,   131,   313,   132,
     190,   190,    92,   435,    97,   407,   191,   124,   411,   145,
     312,   116,   312,   293,    93,   293,   453,   387,   480,   481,
     146,   258,   484,   396,   163,   175,   399,   397,   298,   440,
     173,   454,   434,   378,   379,   174,   436,   437,   278,   179,
     163,   348,   351,   349,   372,   373,   352,   446,   128,   181,
     427,   429,   474,   441,   428,   428,   176,   388,   502,   292,
     292,   503,   388,   513,   177,   388,   178,   514,   409,   409,
     191,   272,   409,   317,   395,   191,   191,   547,   273,   201,
     203,   388,   202,   204,   161,   258,   473,   292,   198,   292,
     462,   463,   464,   465,   380,   381,   520,   279,   293,   142,
     143,   293,   125,   126,   127,   339,   340,   166,   126,   167,
      32,    33,    34,   443,   174,   553,   516,    -9,   392,   476,
     270,   478,   407,   190,   494,   190,   368,   536,     4,   369,
     370,   371,   437,   544,   -10,   545,   438,   568,   288,   490,
     299,   258,   458,   459,   300,   475,   409,   258,   301,   558,
     448,   143,   536,   447,     4,   163,   460,   461,   303,   278,
      25,    26,   304,    27,    28,    29,    30,    31,    32,    33,
      34,   305,   292,   466,   467,   292,   531,   306,   541,   184,
     187,   542,   307,   409,   312,   409,    25,    26,   310,    27,
      28,    29,    30,    31,    32,    33,    34,   528,   191,   409,
     191,   308,   318,   323,   329,   477,   316,   330,   332,   333,
     258,   334,   293,   337,   293,   293,   258,   344,   338,   -35,
     382,   350,   355,   258,   562,   491,   313,   190,   548,   565,
     567,   438,   293,   384,   385,   131,   142,   132,   -34,   383,
     293,   289,   504,   567,   413,   414,   417,   424,   418,   419,
     563,   430,   -29,   482,   487,   455,   456,   457,   292,   292,
     292,   292,   292,   292,   292,   292,   292,   292,   292,   292,
     292,   292,   292,   483,   486,   488,   489,   163,   388,   498,
     495,   497,   505,   506,   507,   511,   292,   510,   292,   292,
      87,   512,   518,   529,   519,   526,   530,   190,   550,   -39,
     523,   561,   191,   468,   471,   524,   292,   525,   469,   271,
     533,   258,   281,   543,   292,   546,    87,   535,   564,   470,
     472,   398,   258,   492,   549,   444,   431,   566,   499,   508,
     569,   141,   496,   432,   328,     0,     0,     0,     0,     0,
       0,     0,     0,    88,    89,     0,     0,   559,     0,   394,
     560,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    88,
      89,     0,   191,     0,     4,     0,     0,     0,     0,     0,
     258,     0,     0,   258,     0,     0,     0,     0,     0,     0,
       0,     0,   539,   540,   356,   357,   358,   359,   360,   361,
     362,   363,   364,   365,     0,     0,    25,    26,   258,    27,
      28,    29,    30,    31,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   258,     0,
       0,     0,     0,   258,   258,     0,     0,     0,     0,     4,
       5,     6,     7,     8,   205,   206,   207,   258,   208,   209,
     210,   211,   212,     0,     0,     9,    10,    11,    12,    13,
      14,    15,    16,    17,    18,    19,    20,    21,    22,    23,
      24,    25,    26,     0,    27,    28,    29,    30,    31,    32,
      33,    34,    35,    36,    37,    38,    39,    40,    41,    42,
      43,    44,    45,    46,    47,    48,    49,    50,    51,    52,
       0,     0,     0,     0,     0,   366,     0,     0,     0,    53,
      54,    55,    56,    57,    58,    59,    60,    61,    62,    63,
      64,    65,    66,    67,    68,    69,    70,    71,    72,    73,
      74,    75,     0,     0,     0,     0,    87,    76,    77,    78,
      79,    80,    81,    82,   213,     0,   131,    84,   132,   214,
     215,   216,   217,     0,   218,     0,     0,   219,   220,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    85,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    88,
      89,     5,     6,     7,     8,     0,     0,    86,     0,     0,
       0,    87,     0,     0,     0,     0,     9,    10,    11,    12,
      13,    14,    15,    16,    17,    18,    19,    20,    21,    22,
      23,    24,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    51,
      52,     0,     0,     0,    88,    89,   221,     0,    90,     0,
      53,    54,   222,   223,   224,   225,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   226,   227,   228,
       4,     5,     6,     7,     8,   205,   206,   207,     0,   208,
     209,   210,   211,   212,    82,     0,     9,    10,    11,    12,
      13,    14,    15,    16,    17,    18,    19,    20,    21,    22,
      23,    24,    25,    26,     0,    27,    28,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    51,
      52,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      53,    54,    55,    56,    57,    58,    59,    60,    61,    62,
      63,    64,    65,    66,    67,    68,    69,    70,    71,    72,
      73,    74,    75,     0,     0,     0,     0,     0,    76,    77,
      78,    79,    80,    81,    82,   213,     0,   131,    84,   132,
     214,   215,   216,   217,     0,   218,     0,     0,   219,   220,
       0,     0,     0,     0,     5,     6,     7,     8,     0,     0,
       0,     0,     0,     0,     0,     0,    85,     0,     0,     9,
      10,    11,    12,    13,    14,    15,    16,    17,    18,    19,
      20,    21,    22,    23,    24,     0,     0,     0,    86,     0,
       0,     0,    87,     0,     0,     0,    35,    36,    37,    38,
      39,    40,    41,    42,    43,    44,    45,    46,    47,    48,
      49,    50,    51,    52,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    53,    54,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    88,    89,   221,     0,    90,
       0,     0,     0,   222,   223,   224,   225,    82,     0,     0,
     125,   126,   127,     0,     0,     0,     0,     0,   226,   227,
     391,     4,     5,     6,     7,     8,   205,   206,   207,     0,
     208,   209,   210,   211,   212,     0,     0,     9,    10,    11,
      12,    13,    14,    15,    16,    17,    18,    19,    20,    21,
      22,    23,    24,    25,    26,     0,    27,    28,    29,    30,
      31,    32,    33,    34,    35,    36,    37,    38,    39,    40,
      41,    42,    43,    44,    45,    46,    47,    48,    49,    50,
      51,    52,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    53,    54,    55,    56,    57,    58,    59,    60,    61,
      62,    63,    64,    65,    66,    67,    68,    69,    70,    71,
      72,    73,    74,    75,     0,     0,     0,     0,     0,    76,
      77,    78,    79,    80,    81,    82,   213,     0,   131,    84,
     132,   214,   215,   216,   217,     0,   218,     0,     0,   219,
     220,     0,     0,     0,     0,     0,     5,     6,     7,     8,
       0,     0,     0,     0,     0,     0,     0,    85,     0,     0,
       0,     9,    10,    11,    12,    13,    14,    15,    16,    17,
      18,    19,    20,    21,    22,    23,    24,     0,     0,    86,
       0,     0,     0,    87,     0,     0,     0,     0,    35,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
      47,    48,    49,    50,    51,    52,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    53,    54,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    88,    89,   221,     0,
      90,     0,     0,     0,   222,   223,   224,   225,    81,    82,
       0,     0,     0,   182,     0,     0,     0,     0,     0,   226,
     227,   509,     4,     5,     6,     7,     8,   205,   206,   207,
       0,   208,   209,   210,   211,   212,   551,   552,     9,    10,
      11,    12,    13,    14,    15,    16,    17,    18,    19,    20,
      21,    22,    23,    24,    25,    26,     0,    27,    28,    29,
      30,    31,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      50,    51,    52,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    63,    64,    65,    66,    67,    68,    69,    70,
      71,    72,    73,    74,    75,     0,     0,     0,     0,     0,
      76,    77,    78,    79,    80,    81,    82,   213,     0,   131,
      84,   132,   214,   215,   216,   217,     0,   218,     0,     0,
     219,   220,     0,     0,     0,     0,     0,     5,     6,     7,
       8,     0,     0,     0,     0,     0,     0,     0,    85,     0,
       0,     0,     9,    10,    11,    12,    13,    14,    15,    16,
      17,    18,    19,    20,    21,    22,    23,    24,     0,     0,
      86,     0,     0,     0,    87,     0,     0,     0,     0,    35,
      36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
      46,    47,    48,    49,    50,    51,    52,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    53,    54,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    88,    89,   221,
       0,    90,     0,     0,     0,   222,   223,   224,   225,    81,
      82,     0,     0,     0,   185,     0,     0,     0,     0,     0,
     226,   227,     4,     5,     6,     7,     8,   205,   206,   207,
       0,   208,   209,   210,   211,   212,     0,     0,     9,    10,
      11,    12,    13,    14,    15,    16,    17,    18,    19,    20,
      21,    22,    23,    24,    25,    26,     0,    27,    28,    29,
      30,    31,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      50,    51,    52,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    63,    64,    65,    66,    67,    68,    69,    70,
      71,    72,    73,    74,    75,     0,     0,     0,     0,     0,
      76,    77,    78,    79,    80,    81,    82,   213,     0,   131,
      84,   132,   214,   215,   216,   217,     0,   218,     0,     0,
     219,   220,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    85,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      86,     0,     0,     0,    87,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    88,    89,   221,
       0,    90,     0,     0,     0,   222,   223,   224,   225,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     226,   227,     4,     5,     6,     7,     8,   205,   206,   207,
       0,   208,   209,   210,   211,   212,     0,     0,     9,    10,
      11,    12,    13,    14,    15,    16,    17,    18,    19,    20,
      21,    22,    23,    24,    25,    26,     0,    27,    28,    29,
      30,    31,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      50,    51,    52,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    63,    64,    65,    66,    67,    68,    69,    70,
      71,    72,    73,    74,    75,     0,     0,     0,     0,     0,
      76,    77,    78,    79,    80,    81,    82,   213,     0,   131,
      84,   132,   214,   215,   216,   217,     0,   218,     0,     0,
     219,   220,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    85,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      86,     0,     0,     0,    87,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    88,    89,   221,
       0,     0,     0,     0,     0,   222,   223,   224,   225,     5,
       6,     7,     8,     0,     0,     0,     0,     0,     0,     0,
     226,   143,     0,     0,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
       0,     0,     0,     0,     0,     0,     0,   188,    32,    33,
      34,    35,    36,    37,    38,    39,    40,    41,    42,    43,
      44,    45,    46,    47,    48,    49,    50,    51,    52,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,     0,     5,     6,     7,     8,    76,    77,    78,    79,
      80,    81,    82,     0,     0,     0,    84,     9,    10,    11,
      12,    13,    14,    15,    16,    17,    18,    19,    20,    21,
      22,    23,    24,     0,     0,     0,     0,     0,     0,     0,
     188,    32,    33,    34,    35,    36,    37,    38,    39,    40,
      41,    42,    43,    44,    45,    46,    47,    48,    49,    50,
      51,    52,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    53,    54,    55,    56,    57,    58,    59,    60,    61,
      62,    63,    64,    65,    66,    67,    68,    69,    70,    71,
      72,    73,    74,    75,     0,     0,     0,     0,     0,    76,
      77,    78,    79,    80,    81,    82,     0,     0,     0,    84,
       0,     5,     6,     7,     8,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     9,    10,    11,    12,
      13,    14,    15,    16,    17,    18,    19,    20,    21,    22,
      23,    24,     0,     0,     0,     0,     0,     0,   189,   188,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    51,
      52,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      53,    54,    55,    56,    57,    58,    59,    60,    61,    62,
      63,    64,    65,    66,    67,    68,    69,    70,    71,    72,
      73,    74,    75,     0,     5,     6,     7,     8,    76,    77,
      78,    79,    80,    81,    82,     0,     0,     0,    84,     9,
      10,    11,    12,    13,    14,    15,    16,    17,    18,    19,
      20,    21,    22,    23,    24,     0,     0,     0,     0,     0,
       0,   311,   188,    32,    33,    34,    35,    36,    37,    38,
      39,    40,    41,    42,    43,    44,    45,    46,    47,    48,
      49,    50,    51,    52,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    53,    54,    55,    56,    57,    58,    59,
      60,    61,    62,    63,    64,    65,    66,    67,    68,    69,
      70,    71,    72,    73,    74,    75,     0,     0,     0,     0,
       0,    76,    77,    78,    79,    80,    81,    82,     0,     0,
       0,    84,     0,     5,     6,     7,     8,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     9,    10,
      11,    12,    13,    14,    15,    16,    17,    18,    19,    20,
      21,    22,    23,    24,     0,     0,     0,     0,     0,     0,
     319,   188,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      50,    51,    52,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    63,    64,    65,    66,    67,    68,    69,    70,
      71,    72,    73,    74,    75,     0,     5,     6,     7,     8,
      76,    77,    78,    79,    80,    81,    82,     0,     0,     0,
      84,     9,    10,    11,    12,    13,    14,    15,    16,    17,
      18,    19,    20,    21,    22,    23,    24,     0,     0,     0,
       0,     0,     0,   321,   188,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
      47,    48,    49,    50,    51,    52,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    53,    54,    55,    56,    57,
      58,    59,    60,    61,    62,    63,    64,    65,    66,    67,
      68,    69,    70,    71,    72,    73,    74,    75,     0,     0,
       0,     0,     0,    76,    77,    78,    79,    80,    81,    82,
       0,     0,     0,    84,     0,     5,     6,     7,     8,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       9,    10,    11,    12,    13,    14,    15,    16,    17,    18,
      19,    20,    21,    22,    23,    24,     0,     0,     0,     0,
       0,     0,   425,   188,    32,    33,    34,    35,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    46,    47,
      48,    49,    50,    51,    52,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    53,    54,    55,    56,    57,    58,
      59,    60,    61,    62,    63,    64,    65,    66,    67,    68,
      69,    70,    71,    72,    73,    74,    75,     0,     5,     6,
       7,     8,    76,    77,    78,    79,    80,    81,    82,     0,
       0,     0,    84,     9,    10,    11,    12,    13,    14,    15,
      16,    17,    18,    19,    20,    21,    22,    23,    24,     0,
       0,     0,     0,     0,     0,   426,   188,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    53,    54,    55,
      56,    57,    58,    59,    60,    61,    62,    63,    64,    65,
      66,    67,    68,    69,    70,    71,    72,    73,    74,    75,
       0,     0,     0,     0,     0,    76,    77,    78,    79,    80,
      81,    82,     0,     0,     0,    84,     0,     0,     0,     0,
       4,     5,     6,     7,     8,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     9,    10,    11,    12,
      13,    14,    15,    16,    17,    18,    19,    20,    21,    22,
      23,    24,    25,    26,   493,    27,    28,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    51,
      52,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      53,    54,    55,    56,    57,    58,    59,    60,    61,    62,
      63,    64,    65,    66,    67,    68,    69,    70,    71,    72,
      73,    74,    75,     0,     0,     0,     0,     0,    76,    77,
      78,    79,    80,    81,    82,     0,     0,   131,    84,   132,
     214,   215,   216,   217,     0,   218,     0,     0,   219,   220,
       0,     0,     0,     0,     0,     0,     0,   527,     0,     0,
       0,     0,     0,     0,     0,     0,    85,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    86,     0,
       0,     0,    87,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    88,    89,   221,     0,     0,
       0,     0,     0,   222,   223,   224,   225,     5,     6,     7,
       8,     0,     0,     0,     0,     0,     0,     0,   226,     0,
       0,     0,     9,    10,    11,    12,    13,    14,    15,    16,
      17,    18,    19,    20,    21,    22,    23,    24,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    35,
      36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
      46,    47,    48,    49,    50,    51,    52,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    53,    54,    55,    56,
      57,    58,    59,    60,    61,    62,    63,    64,    65,    66,
      67,    68,    69,    70,    71,    72,    73,    74,    75,     0,
       0,     0,     0,     0,    76,    77,    78,    79,    80,    81,
      82,     0,     0,   131,    84,   132,   214,   215,   216,   217,
       0,   218,     0,     0,   219,   220,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   221,     0,     0,     0,     0,     0,   222,
     223,   224,   225,     5,     6,     7,     8,     0,     0,     0,
       0,     0,     0,     0,     0,   408,     0,     0,     9,    10,
      11,    12,    13,    14,    15,    16,    17,    18,    19,    20,
      21,    22,    23,    24,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    35,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      50,    51,    52,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    63,    64,    65,    66,    67,    68,    69,    70,
      71,    72,    73,    74,    75,     0,     0,     0,     0,     0,
      76,    77,    78,    79,    80,    81,    82,     0,     0,   131,
      84,   132,   214,   215,   216,   217,     0,   218,     0,     0,
     219,   220,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   221,
       0,     0,     0,     0,     0,   222,   223,   224,   225,     4,
       5,     6,     7,     8,     0,     0,     0,     0,     0,     0,
     335,     0,     0,     0,     0,     9,    10,    11,    12,    13,
      14,    15,    16,    17,    18,    19,    20,    21,    22,    23,
      24,    25,    26,     0,    27,    28,    29,    30,    31,    32,
      33,    34,    35,    36,    37,    38,    39,    40,    41,    42,
      43,    44,    45,    46,    47,    48,    49,    50,    51,    52,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    53,
      54,    55,    56,    57,    58,    59,    60,    61,    62,    63,
      64,    65,    66,    67,    68,    69,    70,    71,    72,    73,
      74,    75,     0,     0,     0,     0,     0,    76,    77,    78,
      79,    80,    81,    82,     0,     0,   131,    84,   132,   214,
     215,   216,   217,     0,   218,     0,     0,   219,   220,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   136,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    87,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    88,    89,   221,   341,     5,     6,
       7,     8,   222,   223,   224,   225,     0,     0,     0,     0,
       0,     0,     0,     9,    10,    11,    12,    13,    14,    15,
      16,    17,    18,    19,    20,    21,    22,    23,    24,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    53,    54,    55,
      56,    57,    58,    59,    60,    61,    62,    63,    64,    65,
      66,    67,    68,    69,    70,    71,    72,    73,    74,    75,
       0,     0,     0,     0,     0,    76,    77,    78,    79,    80,
      81,    82,     0,     0,   131,    84,   132,   214,   215,   216,
     217,     0,   218,     0,     0,   219,   220,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     5,     6,     7,     8,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     9,    10,    11,
      12,    13,    14,    15,    16,    17,    18,    19,    20,    21,
      22,    23,    24,     0,   221,     0,     0,     0,     0,     0,
     222,   223,   224,   225,    35,    36,    37,    38,    39,    40,
      41,    42,    43,    44,    45,    46,    47,    48,    49,    50,
      51,    52,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    53,    54,    55,    56,    57,    58,    59,    60,    61,
      62,    63,    64,    65,    66,    67,    68,    69,    70,    71,
      72,    73,    74,    75,     0,     0,     0,     0,     0,    76,
      77,    78,    79,    80,    81,    82,     0,     0,   131,    84,
     132,   214,   215,   216,   217,     0,   218,     0,     0,   219,
     220,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     5,     6,     7,     8,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     9,    10,    11,    12,    13,    14,    15,    16,    17,
      18,    19,    20,    21,    22,    23,    24,     0,   221,     0,
       0,   291,     0,     0,   222,   223,   224,   225,    35,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
      47,    48,    49,    50,    51,    52,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    53,    54,    55,    56,    57,
      58,    59,    60,    61,    62,    63,    64,    65,    66,    67,
      68,    69,    70,    71,    72,    73,    74,    75,     0,     0,
       0,     0,     0,    76,    77,    78,    79,    80,    81,    82,
       0,     0,   131,    84,   132,   214,   215,   216,   217,     0,
     218,     0,     0,   219,   220,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       5,     6,     7,     8,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     9,    10,    11,    12,    13,
      14,    15,    16,    17,    18,    19,    20,    21,    22,    23,
      24,     0,   221,     0,     0,   296,     0,     0,   222,   223,
     224,   225,    35,    36,    37,    38,    39,    40,    41,    42,
      43,    44,    45,    46,    47,    48,    49,    50,    51,    52,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    53,
      54,    55,    56,    57,    58,    59,    60,    61,    62,    63,
      64,    65,    66,    67,    68,    69,    70,    71,    72,    73,
      74,    75,     0,     0,     0,     0,     0,    76,    77,    78,
      79,    80,    81,    82,     0,     0,   131,    84,   132,   214,
     215,   216,   217,   324,   218,     0,     0,   219,   220,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     5,     6,     7,     8,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     9,
      10,    11,    12,    13,    14,    15,    16,    17,    18,    19,
      20,    21,    22,    23,    24,     0,   221,     0,     0,     0,
       0,     0,   222,   223,   224,   225,    35,    36,    37,    38,
      39,    40,    41,    42,    43,    44,    45,    46,    47,    48,
      49,    50,    51,    52,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    53,    54,    55,    56,    57,    58,    59,
      60,    61,    62,    63,    64,    65,    66,    67,    68,    69,
      70,    71,    72,    73,    74,    75,     0,     0,     0,     0,
       0,    76,    77,    78,    79,    80,    81,    82,     0,     0,
     131,    84,   132,   214,   215,   216,   217,     0,   218,     0,
       0,   219,   220,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     5,     6,
       7,     8,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     9,    10,    11,    12,    13,    14,    15,
      16,    17,    18,    19,    20,    21,    22,    23,    24,     0,
     221,     0,     0,   406,     0,     0,   222,   223,   224,   225,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    53,    54,    55,
      56,    57,    58,    59,    60,    61,    62,    63,    64,    65,
      66,    67,    68,    69,    70,    71,    72,    73,    74,    75,
       0,     0,     0,     0,     0,    76,    77,    78,    79,    80,
      81,    82,     0,     0,   131,    84,   132,   214,   215,   216,
     217,     0,   218,     0,     0,   219,   220,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     5,     6,     7,     8,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     9,    10,    11,
      12,    13,    14,    15,    16,    17,    18,    19,    20,    21,
      22,    23,    24,     0,   221,     0,     0,   479,     0,     0,
     222,   223,   224,   225,    35,    36,    37,    38,    39,    40,
      41,    42,    43,    44,    45,    46,    47,    48,    49,    50,
      51,    52,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    53,    54,    55,    56,    57,    58,    59,    60,    61,
      62,    63,    64,    65,    66,    67,    68,    69,    70,    71,
      72,    73,    74,    75,     0,     0,     0,     0,     0,    76,
      77,    78,    79,    80,    81,    82,     0,     0,   131,    84,
     132,   214,   215,   216,   217,     0,   218,     0,     0,   219,
     220,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     5,     6,     7,     8,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     9,    10,    11,    12,    13,    14,    15,    16,    17,
      18,    19,    20,    21,    22,    23,    24,     0,   221,     0,
       0,     0,     0,     0,   222,   223,   224,   225,    35,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
      47,    48,    49,    50,    51,    52,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    53,    54,    55,    56,    57,
      58,    59,    60,    61,    62,    63,    64,    65,    66,    67,
      68,    69,    70,    71,    72,    73,    74,    75,     0,     0,
       0,     0,     0,    76,    77,    78,    79,    80,    81,   353,
       0,     0,   131,    84,   132,   214,   215,   216,   217,     0,
     218,     0,     0,   219,   220,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       5,     6,     7,     8,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     9,    10,    11,    12,    13,
      14,    15,    16,    17,    18,    19,    20,    21,    22,    23,
      24,     0,   221,     0,     0,     0,     0,     0,   222,   223,
     224,   225,    35,    36,    37,    38,    39,    40,    41,    42,
      43,    44,    45,    46,    47,    48,    49,    50,    51,    52,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    53,
      54,    55,    56,    57,    58,    59,    60,    61,    62,    63,
      64,    65,    66,    67,    68,    69,    70,    71,    72,    73,
      74,    75,     0,     0,     0,     0,     0,    76,    77,    78,
      79,    80,    81,   515,     0,     0,   131,    84,   132,   214,
     215,   216,   217,     0,   218,     0,     0,   219,   220,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       4,     5,     6,     7,     8,     0,   221,     0,     0,     0,
       0,     0,   222,   223,   224,   225,     9,    10,    11,    12,
      13,    14,    15,    16,    17,    18,    19,    20,    21,    22,
      23,    24,    25,    26,     0,    27,    28,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    51,
      52,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      53,    54,    55,    56,    57,    58,    59,    60,    61,    62,
      63,    64,    65,    66,    67,    68,    69,    70,    71,    72,
      73,    74,    75,     0,     0,     0,     0,     0,    76,    77,
      78,    79,    80,    81,    82,     0,    83,     0,    84,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    85,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       4,     5,     6,     7,     8,     0,     0,     0,    86,     0,
       0,     0,    87,     0,     0,     0,     9,    10,    11,    12,
      13,    14,    15,    16,    17,    18,    19,    20,    21,    22,
      23,    24,    25,    26,     0,    27,    28,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    51,
      52,     0,     0,     0,     0,    88,    89,     0,     0,    90,
      53,    54,    55,    56,    57,    58,    59,    60,    61,    62,
      63,    64,    65,    66,    67,    68,    69,    70,    71,    72,
      73,    74,    75,     0,     0,     0,     0,     0,    76,    77,
      78,    79,    80,    81,    82,     0,     0,     0,    84,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   136,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       4,     5,     6,     7,     8,     0,     0,     0,    86,     0,
       0,     0,    87,     0,     0,     0,     9,    10,    11,    12,
      13,    14,    15,    16,    17,    18,    19,    20,    21,    22,
      23,    24,    25,    26,     0,    27,    28,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    51,
      52,     0,     0,     0,     0,    88,    89,     0,     0,    90,
      53,    54,    55,    56,    57,    58,    59,    60,    61,    62,
      63,    64,    65,    66,    67,    68,    69,    70,    71,    72,
      73,    74,    75,     0,     0,     0,     0,     0,    76,    77,
      78,    79,    80,    81,    82,     0,     0,     0,    84,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   136,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   147,     5,     6,
       7,     8,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    87,     9,    10,    11,    12,    13,    14,    15,
      16,    17,    18,    19,    20,    21,    22,    23,    24,   148,
     149,   150,   151,     0,     0,     0,   152,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,     0,     0,
       0,     0,     0,     0,     0,    88,    89,    53,    54,    55,
      56,    57,    58,    59,    60,    61,    62,    63,    64,    65,
      66,    67,    68,    69,    70,    71,    72,    73,    74,    75,
     153,   154,   155,   156,   157,    76,    77,    78,    79,    80,
      81,    82,     0,     0,     0,    84,   147,     5,     6,     7,
       8,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     9,    10,    11,    12,    13,    14,    15,    16,
      17,    18,    19,    20,    21,    22,    23,    24,     0,     0,
       0,   151,     0,     0,     0,   152,    32,    33,    34,    35,
      36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
      46,    47,    48,    49,    50,    51,    52,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    53,    54,    55,    56,
      57,    58,    59,    60,    61,    62,    63,    64,    65,    66,
      67,    68,    69,    70,    71,    72,    73,    74,    75,     0,
       5,     6,     7,     8,    76,    77,    78,    79,    80,    81,
      82,     0,     0,     0,    84,     9,    10,    11,    12,    13,
      14,    15,    16,    17,    18,    19,    20,    21,    22,    23,
      24,   148,   149,   150,     0,     0,     0,     0,     0,     0,
       0,     0,    35,    36,    37,    38,    39,    40,    41,    42,
      43,    44,    45,    46,    47,    48,    49,    50,    51,    52,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    53,
      54,    55,    56,    57,    58,    59,    60,    61,    62,    63,
      64,    65,    66,    67,    68,    69,    70,    71,    72,    73,
      74,    75,   153,   154,   155,   156,   157,    76,    77,    78,
      79,    80,    81,    82,     0,     0,     0,    84,     5,     6,
       7,     8,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     9,    10,    11,    12,    13,    14,    15,
      16,    17,    18,    19,    20,    21,    22,    23,    24,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    53,    54,    55,
      56,    57,    58,    59,    60,    61,    62,    63,    64,    65,
      66,    67,    68,    69,    70,    71,    72,    73,    74,    75,
       0,     0,     0,     0,     0,    76,    77,    78,    79,    80,
      81,    82,     0,     0,     0,    84
};

#define yypact_value_is_default(yystate) \
  ((yystate) == (-267))

#define yytable_value_is_error(yytable_value) \
  YYID (0)

static const yytype_int16 yycheck[] =
{
       2,   207,   192,    99,    86,    81,    85,    83,    85,    17,
      18,   269,   173,   174,    90,     3,   116,   197,   198,    96,
      99,   237,    93,    99,   290,   160,   160,   128,   162,   221,
     211,    17,    18,     2,   116,     2,   110,   111,   112,   225,
     221,    85,     2,   123,   124,     2,     0,   222,   222,   222,
     239,   128,   222,   242,   222,   241,   100,   263,   137,   136,
     121,   122,   225,    90,   239,   240,   240,   240,   164,   146,
     240,   287,   240,   225,   290,   152,   222,   225,   119,   120,
     243,   225,   234,   160,    86,     3,   225,   225,   164,    91,
     134,   239,   136,   239,    96,   239,   123,   124,   194,   101,
     225,   180,   241,   241,   233,   173,   174,   225,   179,    35,
      36,   188,   165,    39,   116,   192,   241,   110,   194,   112,
     197,   198,    91,   241,    91,   286,   128,   233,   289,   221,
     320,    91,   322,   201,    91,   203,   352,   238,   404,   405,
     225,   143,   408,   277,   146,   233,   281,   281,   175,   341,
     222,   367,   333,   233,   234,   222,   337,   338,   160,   116,
     162,   222,   221,   224,   226,   227,   225,   348,   242,   269,
     221,   221,   388,   221,   225,   225,   233,   225,   221,   173,
     174,   221,   225,   221,   233,   225,   233,   225,   404,   405,
     192,    36,   408,   195,   270,   197,   198,   221,    35,   220,
     220,   225,   223,   223,   281,   207,   387,   201,   242,   203,
     376,   377,   378,   379,   125,   126,   482,    42,   286,   241,
     242,   289,   110,   111,   112,   219,   220,   110,   111,   112,
      43,    44,    45,   221,   222,   243,   452,   220,   444,   400,
     239,   402,   403,   320,   424,   322,   240,   505,     3,   230,
     231,   232,   433,   519,   220,   521,   338,   243,   220,   420,
     111,   263,   372,   373,   111,   399,   482,   269,   111,   535,
     349,   242,   530,   349,     3,   277,   374,   375,   234,   281,
      35,    36,   234,    38,    39,    40,    41,    42,    43,    44,
      45,   234,   286,   380,   381,   289,   502,   234,   514,   123,
     124,   517,   234,   519,   494,   521,    35,    36,    42,    38,
      39,    40,    41,    42,    43,    44,    45,   498,   320,   535,
     322,   234,   111,   220,   241,   401,   422,   241,   220,   220,
     332,   241,   400,   220,   402,   403,   338,   243,   220,   220,
     235,   221,   220,   345,   550,   421,   422,   424,   529,   555,
     556,   433,   420,   237,   127,   110,   241,   112,   220,   236,
     428,   222,   438,   569,   223,   223,   234,   242,   225,   225,
     551,   108,   221,   240,   114,   369,   370,   371,   372,   373,
     374,   375,   376,   377,   378,   379,   380,   381,   382,   383,
     384,   385,   386,   223,   223,   114,   114,   399,   225,   220,
     223,   223,   221,   221,   221,   220,   400,   223,   402,   403,
     165,   221,   223,   241,   240,   223,   221,   494,    11,   221,
     234,   241,   424,   382,   385,   234,   420,   234,   383,   146,
     242,   433,   162,   239,   428,   239,   165,   240,   239,   384,
     386,   281,   444,   422,   530,   345,   332,   555,   433,   443,
     557,    91,   428,   332,   203,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   218,   219,    -1,    -1,   543,    -1,   269,
     546,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   218,
     219,    -1,   494,    -1,     3,    -1,    -1,    -1,    -1,    -1,
     502,    -1,    -1,   505,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   506,   507,   129,   130,   131,   132,   133,   134,
     135,   136,   137,   138,    -1,    -1,    35,    36,   530,    38,
      39,    40,    41,    42,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   550,    -1,
      -1,    -1,    -1,   555,   556,    -1,    -1,    -1,    -1,     3,
       4,     5,     6,     7,     8,     9,    10,   569,    12,    13,
      14,    15,    16,    -1,    -1,    19,    20,    21,    22,    23,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    35,    36,    -1,    38,    39,    40,    41,    42,    43,
      44,    45,    46,    47,    48,    49,    50,    51,    52,    53,
      54,    55,    56,    57,    58,    59,    60,    61,    62,    63,
      -1,    -1,    -1,    -1,    -1,   240,    -1,    -1,    -1,    73,
      74,    75,    76,    77,    78,    79,    80,    81,    82,    83,
      84,    85,    86,    87,    88,    89,    90,    91,    92,    93,
      94,    95,    -1,    -1,    -1,    -1,   165,   101,   102,   103,
     104,   105,   106,   107,   108,    -1,   110,   111,   112,   113,
     114,   115,   116,    -1,   118,    -1,    -1,   121,   122,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   139,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   218,
     219,     4,     5,     6,     7,    -1,    -1,   161,    -1,    -1,
      -1,   165,    -1,    -1,    -1,    -1,    19,    20,    21,    22,
      23,    24,    25,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    46,    47,    48,    49,    50,    51,    52,
      53,    54,    55,    56,    57,    58,    59,    60,    61,    62,
      63,    -1,    -1,    -1,   218,   219,   220,    -1,   222,    -1,
      73,    74,   226,   227,   228,   229,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   241,   242,   243,
       3,     4,     5,     6,     7,     8,     9,    10,    -1,    12,
      13,    14,    15,    16,   107,    -1,    19,    20,    21,    22,
      23,    24,    25,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    35,    36,    -1,    38,    39,    40,    41,    42,
      43,    44,    45,    46,    47,    48,    49,    50,    51,    52,
      53,    54,    55,    56,    57,    58,    59,    60,    61,    62,
      63,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      73,    74,    75,    76,    77,    78,    79,    80,    81,    82,
      83,    84,    85,    86,    87,    88,    89,    90,    91,    92,
      93,    94,    95,    -1,    -1,    -1,    -1,    -1,   101,   102,
     103,   104,   105,   106,   107,   108,    -1,   110,   111,   112,
     113,   114,   115,   116,    -1,   118,    -1,    -1,   121,   122,
      -1,    -1,    -1,    -1,     4,     5,     6,     7,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   139,    -1,    -1,    19,
      20,    21,    22,    23,    24,    25,    26,    27,    28,    29,
      30,    31,    32,    33,    34,    -1,    -1,    -1,   161,    -1,
      -1,    -1,   165,    -1,    -1,    -1,    46,    47,    48,    49,
      50,    51,    52,    53,    54,    55,    56,    57,    58,    59,
      60,    61,    62,    63,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    73,    74,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   218,   219,   220,    -1,   222,
      -1,    -1,    -1,   226,   227,   228,   229,   107,    -1,    -1,
     110,   111,   112,    -1,    -1,    -1,    -1,    -1,   241,   242,
     243,     3,     4,     5,     6,     7,     8,     9,    10,    -1,
      12,    13,    14,    15,    16,    -1,    -1,    19,    20,    21,
      22,    23,    24,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    35,    36,    -1,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    58,    59,    60,    61,
      62,    63,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    73,    74,    75,    76,    77,    78,    79,    80,    81,
      82,    83,    84,    85,    86,    87,    88,    89,    90,    91,
      92,    93,    94,    95,    -1,    -1,    -1,    -1,    -1,   101,
     102,   103,   104,   105,   106,   107,   108,    -1,   110,   111,
     112,   113,   114,   115,   116,    -1,   118,    -1,    -1,   121,
     122,    -1,    -1,    -1,    -1,    -1,     4,     5,     6,     7,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   139,    -1,    -1,
      -1,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    -1,    -1,   161,
      -1,    -1,    -1,   165,    -1,    -1,    -1,    -1,    46,    47,
      48,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      58,    59,    60,    61,    62,    63,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    73,    74,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   218,   219,   220,    -1,
     222,    -1,    -1,    -1,   226,   227,   228,   229,   106,   107,
      -1,    -1,    -1,   111,    -1,    -1,    -1,    -1,    -1,   241,
     242,   243,     3,     4,     5,     6,     7,     8,     9,    10,
      -1,    12,    13,    14,    15,    16,    17,    18,    19,    20,
      21,    22,    23,    24,    25,    26,    27,    28,    29,    30,
      31,    32,    33,    34,    35,    36,    -1,    38,    39,    40,
      41,    42,    43,    44,    45,    46,    47,    48,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    63,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    73,    74,    75,    76,    77,    78,    79,    80,
      81,    82,    83,    84,    85,    86,    87,    88,    89,    90,
      91,    92,    93,    94,    95,    -1,    -1,    -1,    -1,    -1,
     101,   102,   103,   104,   105,   106,   107,   108,    -1,   110,
     111,   112,   113,   114,   115,   116,    -1,   118,    -1,    -1,
     121,   122,    -1,    -1,    -1,    -1,    -1,     4,     5,     6,
       7,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   139,    -1,
      -1,    -1,    19,    20,    21,    22,    23,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    -1,    -1,
     161,    -1,    -1,    -1,   165,    -1,    -1,    -1,    -1,    46,
      47,    48,    49,    50,    51,    52,    53,    54,    55,    56,
      57,    58,    59,    60,    61,    62,    63,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    73,    74,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   218,   219,   220,
      -1,   222,    -1,    -1,    -1,   226,   227,   228,   229,   106,
     107,    -1,    -1,    -1,   111,    -1,    -1,    -1,    -1,    -1,
     241,   242,     3,     4,     5,     6,     7,     8,     9,    10,
      -1,    12,    13,    14,    15,    16,    -1,    -1,    19,    20,
      21,    22,    23,    24,    25,    26,    27,    28,    29,    30,
      31,    32,    33,    34,    35,    36,    -1,    38,    39,    40,
      41,    42,    43,    44,    45,    46,    47,    48,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    63,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    73,    74,    75,    76,    77,    78,    79,    80,
      81,    82,    83,    84,    85,    86,    87,    88,    89,    90,
      91,    92,    93,    94,    95,    -1,    -1,    -1,    -1,    -1,
     101,   102,   103,   104,   105,   106,   107,   108,    -1,   110,
     111,   112,   113,   114,   115,   116,    -1,   118,    -1,    -1,
     121,   122,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   139,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     161,    -1,    -1,    -1,   165,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   218,   219,   220,
      -1,   222,    -1,    -1,    -1,   226,   227,   228,   229,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     241,   242,     3,     4,     5,     6,     7,     8,     9,    10,
      -1,    12,    13,    14,    15,    16,    -1,    -1,    19,    20,
      21,    22,    23,    24,    25,    26,    27,    28,    29,    30,
      31,    32,    33,    34,    35,    36,    -1,    38,    39,    40,
      41,    42,    43,    44,    45,    46,    47,    48,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    63,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    73,    74,    75,    76,    77,    78,    79,    80,
      81,    82,    83,    84,    85,    86,    87,    88,    89,    90,
      91,    92,    93,    94,    95,    -1,    -1,    -1,    -1,    -1,
     101,   102,   103,   104,   105,   106,   107,   108,    -1,   110,
     111,   112,   113,   114,   115,   116,    -1,   118,    -1,    -1,
     121,   122,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   139,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     161,    -1,    -1,    -1,   165,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   218,   219,   220,
      -1,    -1,    -1,    -1,    -1,   226,   227,   228,   229,     4,
       5,     6,     7,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     241,   242,    -1,    -1,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    93,    94,
      95,    -1,     4,     5,     6,     7,   101,   102,   103,   104,
     105,   106,   107,    -1,    -1,    -1,   111,    19,    20,    21,
      22,    23,    24,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    58,    59,    60,    61,
      62,    63,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    73,    74,    75,    76,    77,    78,    79,    80,    81,
      82,    83,    84,    85,    86,    87,    88,    89,    90,    91,
      92,    93,    94,    95,    -1,    -1,    -1,    -1,    -1,   101,
     102,   103,   104,   105,   106,   107,    -1,    -1,    -1,   111,
      -1,     4,     5,     6,     7,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    19,    20,    21,    22,
      23,    24,    25,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    -1,    -1,    -1,    -1,    -1,    -1,   243,    42,
      43,    44,    45,    46,    47,    48,    49,    50,    51,    52,
      53,    54,    55,    56,    57,    58,    59,    60,    61,    62,
      63,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      73,    74,    75,    76,    77,    78,    79,    80,    81,    82,
      83,    84,    85,    86,    87,    88,    89,    90,    91,    92,
      93,    94,    95,    -1,     4,     5,     6,     7,   101,   102,
     103,   104,   105,   106,   107,    -1,    -1,    -1,   111,    19,
      20,    21,    22,    23,    24,    25,    26,    27,    28,    29,
      30,    31,    32,    33,    34,    -1,    -1,    -1,    -1,    -1,
      -1,   243,    42,    43,    44,    45,    46,    47,    48,    49,
      50,    51,    52,    53,    54,    55,    56,    57,    58,    59,
      60,    61,    62,    63,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    73,    74,    75,    76,    77,    78,    79,
      80,    81,    82,    83,    84,    85,    86,    87,    88,    89,
      90,    91,    92,    93,    94,    95,    -1,    -1,    -1,    -1,
      -1,   101,   102,   103,   104,   105,   106,   107,    -1,    -1,
      -1,   111,    -1,     4,     5,     6,     7,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    19,    20,
      21,    22,    23,    24,    25,    26,    27,    28,    29,    30,
      31,    32,    33,    34,    -1,    -1,    -1,    -1,    -1,    -1,
     243,    42,    43,    44,    45,    46,    47,    48,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    63,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    73,    74,    75,    76,    77,    78,    79,    80,
      81,    82,    83,    84,    85,    86,    87,    88,    89,    90,
      91,    92,    93,    94,    95,    -1,     4,     5,     6,     7,
     101,   102,   103,   104,   105,   106,   107,    -1,    -1,    -1,
     111,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    -1,    -1,    -1,
      -1,    -1,    -1,   243,    42,    43,    44,    45,    46,    47,
      48,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      58,    59,    60,    61,    62,    63,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    73,    74,    75,    76,    77,
      78,    79,    80,    81,    82,    83,    84,    85,    86,    87,
      88,    89,    90,    91,    92,    93,    94,    95,    -1,    -1,
      -1,    -1,    -1,   101,   102,   103,   104,   105,   106,   107,
      -1,    -1,    -1,   111,    -1,     4,     5,     6,     7,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      19,    20,    21,    22,    23,    24,    25,    26,    27,    28,
      29,    30,    31,    32,    33,    34,    -1,    -1,    -1,    -1,
      -1,    -1,   243,    42,    43,    44,    45,    46,    47,    48,
      49,    50,    51,    52,    53,    54,    55,    56,    57,    58,
      59,    60,    61,    62,    63,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    73,    74,    75,    76,    77,    78,
      79,    80,    81,    82,    83,    84,    85,    86,    87,    88,
      89,    90,    91,    92,    93,    94,    95,    -1,     4,     5,
       6,     7,   101,   102,   103,   104,   105,   106,   107,    -1,
      -1,    -1,   111,    19,    20,    21,    22,    23,    24,    25,
      26,    27,    28,    29,    30,    31,    32,    33,    34,    -1,
      -1,    -1,    -1,    -1,    -1,   243,    42,    43,    44,    45,
      46,    47,    48,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    58,    59,    60,    61,    62,    63,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    73,    74,    75,
      76,    77,    78,    79,    80,    81,    82,    83,    84,    85,
      86,    87,    88,    89,    90,    91,    92,    93,    94,    95,
      -1,    -1,    -1,    -1,    -1,   101,   102,   103,   104,   105,
     106,   107,    -1,    -1,    -1,   111,    -1,    -1,    -1,    -1,
       3,     4,     5,     6,     7,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    19,    20,    21,    22,
      23,    24,    25,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    35,    36,   243,    38,    39,    40,    41,    42,
      43,    44,    45,    46,    47,    48,    49,    50,    51,    52,
      53,    54,    55,    56,    57,    58,    59,    60,    61,    62,
      63,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      73,    74,    75,    76,    77,    78,    79,    80,    81,    82,
      83,    84,    85,    86,    87,    88,    89,    90,    91,    92,
      93,    94,    95,    -1,    -1,    -1,    -1,    -1,   101,   102,
     103,   104,   105,   106,   107,    -1,    -1,   110,   111,   112,
     113,   114,   115,   116,    -1,   118,    -1,    -1,   121,   122,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   243,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   139,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   161,    -1,
      -1,    -1,   165,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   218,   219,   220,    -1,    -1,
      -1,    -1,    -1,   226,   227,   228,   229,     4,     5,     6,
       7,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   241,    -1,
      -1,    -1,    19,    20,    21,    22,    23,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    46,
      47,    48,    49,    50,    51,    52,    53,    54,    55,    56,
      57,    58,    59,    60,    61,    62,    63,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    73,    74,    75,    76,
      77,    78,    79,    80,    81,    82,    83,    84,    85,    86,
      87,    88,    89,    90,    91,    92,    93,    94,    95,    -1,
      -1,    -1,    -1,    -1,   101,   102,   103,   104,   105,   106,
     107,    -1,    -1,   110,   111,   112,   113,   114,   115,   116,
      -1,   118,    -1,    -1,   121,   122,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   220,    -1,    -1,    -1,    -1,    -1,   226,
     227,   228,   229,     4,     5,     6,     7,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   242,    -1,    -1,    19,    20,
      21,    22,    23,    24,    25,    26,    27,    28,    29,    30,
      31,    32,    33,    34,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    46,    47,    48,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    63,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    73,    74,    75,    76,    77,    78,    79,    80,
      81,    82,    83,    84,    85,    86,    87,    88,    89,    90,
      91,    92,    93,    94,    95,    -1,    -1,    -1,    -1,    -1,
     101,   102,   103,   104,   105,   106,   107,    -1,    -1,   110,
     111,   112,   113,   114,   115,   116,    -1,   118,    -1,    -1,
     121,   122,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   220,
      -1,    -1,    -1,    -1,    -1,   226,   227,   228,   229,     3,
       4,     5,     6,     7,    -1,    -1,    -1,    -1,    -1,    -1,
     241,    -1,    -1,    -1,    -1,    19,    20,    21,    22,    23,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    35,    36,    -1,    38,    39,    40,    41,    42,    43,
      44,    45,    46,    47,    48,    49,    50,    51,    52,    53,
      54,    55,    56,    57,    58,    59,    60,    61,    62,    63,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    73,
      74,    75,    76,    77,    78,    79,    80,    81,    82,    83,
      84,    85,    86,    87,    88,    89,    90,    91,    92,    93,
      94,    95,    -1,    -1,    -1,    -1,    -1,   101,   102,   103,
     104,   105,   106,   107,    -1,    -1,   110,   111,   112,   113,
     114,   115,   116,    -1,   118,    -1,    -1,   121,   122,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   139,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   165,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   218,   219,   220,     3,     4,     5,
       6,     7,   226,   227,   228,   229,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    19,    20,    21,    22,    23,    24,    25,
      26,    27,    28,    29,    30,    31,    32,    33,    34,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      46,    47,    48,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    58,    59,    60,    61,    62,    63,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    73,    74,    75,
      76,    77,    78,    79,    80,    81,    82,    83,    84,    85,
      86,    87,    88,    89,    90,    91,    92,    93,    94,    95,
      -1,    -1,    -1,    -1,    -1,   101,   102,   103,   104,   105,
     106,   107,    -1,    -1,   110,   111,   112,   113,   114,   115,
     116,    -1,   118,    -1,    -1,   121,   122,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,     4,     5,     6,     7,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    19,    20,    21,
      22,    23,    24,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    -1,   220,    -1,    -1,    -1,    -1,    -1,
     226,   227,   228,   229,    46,    47,    48,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    58,    59,    60,    61,
      62,    63,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    73,    74,    75,    76,    77,    78,    79,    80,    81,
      82,    83,    84,    85,    86,    87,    88,    89,    90,    91,
      92,    93,    94,    95,    -1,    -1,    -1,    -1,    -1,   101,
     102,   103,   104,   105,   106,   107,    -1,    -1,   110,   111,
     112,   113,   114,   115,   116,    -1,   118,    -1,    -1,   121,
     122,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,     4,     5,     6,     7,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    -1,   220,    -1,
      -1,   223,    -1,    -1,   226,   227,   228,   229,    46,    47,
      48,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      58,    59,    60,    61,    62,    63,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    73,    74,    75,    76,    77,
      78,    79,    80,    81,    82,    83,    84,    85,    86,    87,
      88,    89,    90,    91,    92,    93,    94,    95,    -1,    -1,
      -1,    -1,    -1,   101,   102,   103,   104,   105,   106,   107,
      -1,    -1,   110,   111,   112,   113,   114,   115,   116,    -1,
     118,    -1,    -1,   121,   122,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
       4,     5,     6,     7,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    19,    20,    21,    22,    23,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    -1,   220,    -1,    -1,   223,    -1,    -1,   226,   227,
     228,   229,    46,    47,    48,    49,    50,    51,    52,    53,
      54,    55,    56,    57,    58,    59,    60,    61,    62,    63,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    73,
      74,    75,    76,    77,    78,    79,    80,    81,    82,    83,
      84,    85,    86,    87,    88,    89,    90,    91,    92,    93,
      94,    95,    -1,    -1,    -1,    -1,    -1,   101,   102,   103,
     104,   105,   106,   107,    -1,    -1,   110,   111,   112,   113,
     114,   115,   116,   117,   118,    -1,    -1,   121,   122,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,     4,     5,     6,     7,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    19,
      20,    21,    22,    23,    24,    25,    26,    27,    28,    29,
      30,    31,    32,    33,    34,    -1,   220,    -1,    -1,    -1,
      -1,    -1,   226,   227,   228,   229,    46,    47,    48,    49,
      50,    51,    52,    53,    54,    55,    56,    57,    58,    59,
      60,    61,    62,    63,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    73,    74,    75,    76,    77,    78,    79,
      80,    81,    82,    83,    84,    85,    86,    87,    88,    89,
      90,    91,    92,    93,    94,    95,    -1,    -1,    -1,    -1,
      -1,   101,   102,   103,   104,   105,   106,   107,    -1,    -1,
     110,   111,   112,   113,   114,   115,   116,    -1,   118,    -1,
      -1,   121,   122,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,     4,     5,
       6,     7,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    19,    20,    21,    22,    23,    24,    25,
      26,    27,    28,    29,    30,    31,    32,    33,    34,    -1,
     220,    -1,    -1,   223,    -1,    -1,   226,   227,   228,   229,
      46,    47,    48,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    58,    59,    60,    61,    62,    63,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    73,    74,    75,
      76,    77,    78,    79,    80,    81,    82,    83,    84,    85,
      86,    87,    88,    89,    90,    91,    92,    93,    94,    95,
      -1,    -1,    -1,    -1,    -1,   101,   102,   103,   104,   105,
     106,   107,    -1,    -1,   110,   111,   112,   113,   114,   115,
     116,    -1,   118,    -1,    -1,   121,   122,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,     4,     5,     6,     7,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    19,    20,    21,
      22,    23,    24,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    -1,   220,    -1,    -1,   223,    -1,    -1,
     226,   227,   228,   229,    46,    47,    48,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    58,    59,    60,    61,
      62,    63,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    73,    74,    75,    76,    77,    78,    79,    80,    81,
      82,    83,    84,    85,    86,    87,    88,    89,    90,    91,
      92,    93,    94,    95,    -1,    -1,    -1,    -1,    -1,   101,
     102,   103,   104,   105,   106,   107,    -1,    -1,   110,   111,
     112,   113,   114,   115,   116,    -1,   118,    -1,    -1,   121,
     122,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,     4,     5,     6,     7,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    -1,   220,    -1,
      -1,    -1,    -1,    -1,   226,   227,   228,   229,    46,    47,
      48,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      58,    59,    60,    61,    62,    63,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    73,    74,    75,    76,    77,
      78,    79,    80,    81,    82,    83,    84,    85,    86,    87,
      88,    89,    90,    91,    92,    93,    94,    95,    -1,    -1,
      -1,    -1,    -1,   101,   102,   103,   104,   105,   106,   107,
      -1,    -1,   110,   111,   112,   113,   114,   115,   116,    -1,
     118,    -1,    -1,   121,   122,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
       4,     5,     6,     7,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    19,    20,    21,    22,    23,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    -1,   220,    -1,    -1,    -1,    -1,    -1,   226,   227,
     228,   229,    46,    47,    48,    49,    50,    51,    52,    53,
      54,    55,    56,    57,    58,    59,    60,    61,    62,    63,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    73,
      74,    75,    76,    77,    78,    79,    80,    81,    82,    83,
      84,    85,    86,    87,    88,    89,    90,    91,    92,    93,
      94,    95,    -1,    -1,    -1,    -1,    -1,   101,   102,   103,
     104,   105,   106,   107,    -1,    -1,   110,   111,   112,   113,
     114,   115,   116,    -1,   118,    -1,    -1,   121,   122,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
       3,     4,     5,     6,     7,    -1,   220,    -1,    -1,    -1,
      -1,    -1,   226,   227,   228,   229,    19,    20,    21,    22,
      23,    24,    25,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    35,    36,    -1,    38,    39,    40,    41,    42,
      43,    44,    45,    46,    47,    48,    49,    50,    51,    52,
      53,    54,    55,    56,    57,    58,    59,    60,    61,    62,
      63,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      73,    74,    75,    76,    77,    78,    79,    80,    81,    82,
      83,    84,    85,    86,    87,    88,    89,    90,    91,    92,
      93,    94,    95,    -1,    -1,    -1,    -1,    -1,   101,   102,
     103,   104,   105,   106,   107,    -1,   109,    -1,   111,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   139,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
       3,     4,     5,     6,     7,    -1,    -1,    -1,   161,    -1,
      -1,    -1,   165,    -1,    -1,    -1,    19,    20,    21,    22,
      23,    24,    25,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    35,    36,    -1,    38,    39,    40,    41,    42,
      43,    44,    45,    46,    47,    48,    49,    50,    51,    52,
      53,    54,    55,    56,    57,    58,    59,    60,    61,    62,
      63,    -1,    -1,    -1,    -1,   218,   219,    -1,    -1,   222,
      73,    74,    75,    76,    77,    78,    79,    80,    81,    82,
      83,    84,    85,    86,    87,    88,    89,    90,    91,    92,
      93,    94,    95,    -1,    -1,    -1,    -1,    -1,   101,   102,
     103,   104,   105,   106,   107,    -1,    -1,    -1,   111,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   139,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
       3,     4,     5,     6,     7,    -1,    -1,    -1,   161,    -1,
      -1,    -1,   165,    -1,    -1,    -1,    19,    20,    21,    22,
      23,    24,    25,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    35,    36,    -1,    38,    39,    40,    41,    42,
      43,    44,    45,    46,    47,    48,    49,    50,    51,    52,
      53,    54,    55,    56,    57,    58,    59,    60,    61,    62,
      63,    -1,    -1,    -1,    -1,   218,   219,    -1,    -1,   222,
      73,    74,    75,    76,    77,    78,    79,    80,    81,    82,
      83,    84,    85,    86,    87,    88,    89,    90,    91,    92,
      93,    94,    95,    -1,    -1,    -1,    -1,    -1,   101,   102,
     103,   104,   105,   106,   107,    -1,    -1,    -1,   111,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   139,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,     3,     4,     5,
       6,     7,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   165,    19,    20,    21,    22,    23,    24,    25,
      26,    27,    28,    29,    30,    31,    32,    33,    34,    35,
      36,    37,    38,    -1,    -1,    -1,    42,    43,    44,    45,
      46,    47,    48,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    58,    59,    60,    61,    62,    63,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   218,   219,    73,    74,    75,
      76,    77,    78,    79,    80,    81,    82,    83,    84,    85,
      86,    87,    88,    89,    90,    91,    92,    93,    94,    95,
      96,    97,    98,    99,   100,   101,   102,   103,   104,   105,
     106,   107,    -1,    -1,    -1,   111,     3,     4,     5,     6,
       7,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    19,    20,    21,    22,    23,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    -1,    -1,
      -1,    38,    -1,    -1,    -1,    42,    43,    44,    45,    46,
      47,    48,    49,    50,    51,    52,    53,    54,    55,    56,
      57,    58,    59,    60,    61,    62,    63,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    73,    74,    75,    76,
      77,    78,    79,    80,    81,    82,    83,    84,    85,    86,
      87,    88,    89,    90,    91,    92,    93,    94,    95,    -1,
       4,     5,     6,     7,   101,   102,   103,   104,   105,   106,
     107,    -1,    -1,    -1,   111,    19,    20,    21,    22,    23,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    35,    36,    37,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    46,    47,    48,    49,    50,    51,    52,    53,
      54,    55,    56,    57,    58,    59,    60,    61,    62,    63,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    73,
      74,    75,    76,    77,    78,    79,    80,    81,    82,    83,
      84,    85,    86,    87,    88,    89,    90,    91,    92,    93,
      94,    95,    96,    97,    98,    99,   100,   101,   102,   103,
     104,   105,   106,   107,    -1,    -1,    -1,   111,     4,     5,
       6,     7,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    19,    20,    21,    22,    23,    24,    25,
      26,    27,    28,    29,    30,    31,    32,    33,    34,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      46,    47,    48,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    58,    59,    60,    61,    62,    63,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    73,    74,    75,
      76,    77,    78,    79,    80,    81,    82,    83,    84,    85,
      86,    87,    88,    89,    90,    91,    92,    93,    94,    95,
      -1,    -1,    -1,    -1,    -1,   101,   102,   103,   104,   105,
     106,   107,    -1,    -1,    -1,   111
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_uint16 yystos[] =
{
       0,   245,   246,     0,     3,     4,     5,     6,     7,    19,
      20,    21,    22,    23,    24,    25,    26,    27,    28,    29,
      30,    31,    32,    33,    34,    35,    36,    38,    39,    40,
      41,    42,    43,    44,    45,    46,    47,    48,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    63,    73,    74,    75,    76,    77,    78,    79,
      80,    81,    82,    83,    84,    85,    86,    87,    88,    89,
      90,    91,    92,    93,    94,    95,   101,   102,   103,   104,
     105,   106,   107,   109,   111,   139,   161,   165,   218,   219,
     222,   248,   281,   283,   284,   285,   286,   291,   292,   293,
     294,   296,   297,   298,   299,   300,   301,   302,   303,   304,
     305,   306,   307,   308,   341,   344,   345,   346,   347,   165,
      35,    36,    39,   233,   233,   110,   111,   112,   242,   247,
     247,   110,   112,   249,   294,   297,   139,   293,     3,   247,
     302,   341,   241,   242,   324,   221,   225,     3,    35,    36,
      37,    38,    42,    96,    97,    98,    99,   100,   287,   288,
     289,   294,   295,   298,   225,   241,   110,   112,   247,   249,
     315,   297,   298,   222,   222,   233,   233,   233,   233,   283,
     293,   344,   111,   302,   307,   111,   302,   307,    42,   243,
     294,   298,   309,   310,   311,   312,   239,   242,   242,   297,
     249,   220,   223,   220,   223,     8,     9,    10,    12,    13,
      14,    15,    16,   108,   113,   114,   115,   116,   118,   121,
     122,   220,   226,   227,   228,   229,   241,   242,   243,   249,
     250,   251,   253,   254,   255,   256,   257,   258,   259,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   279,   281,   282,   283,   291,   298,   318,
     319,   320,   321,   325,   326,   327,   330,   336,   340,   345,
     239,   288,    36,    35,   294,   287,   290,   295,   298,    42,
     287,   289,   247,   315,   247,   315,   222,   240,   220,   222,
     240,   223,   264,   276,   280,   298,   223,   280,   302,   111,
     111,   111,   324,   234,   234,   234,   234,   234,   234,   294,
      42,   243,   310,   247,   313,   314,   315,   298,   111,   243,
     309,   243,   309,   220,   117,   280,   342,   343,   343,   241,
     241,   319,   220,   220,   241,   241,   279,   220,   220,   264,
     264,     3,   279,   301,   243,   322,   121,   122,   222,   224,
     221,   221,   225,   107,   277,   220,   129,   130,   131,   132,
     133,   134,   135,   136,   137,   138,   240,   278,   264,   230,
     231,   232,   226,   227,   119,   120,   123,   124,   233,   234,
     125,   126,   235,   236,   237,   127,   128,   238,   225,   241,
     241,   243,   319,   320,   321,   247,   287,   287,   290,   295,
     222,   239,   240,   222,   240,   240,   223,   280,   242,   277,
     316,   280,   316,   223,   223,   225,   234,   234,   225,   225,
     222,   239,   225,   241,   242,   243,   243,   221,   225,   221,
     108,   318,   326,   337,   279,   241,   279,   279,   293,   329,
     301,   221,     3,   221,   325,   252,   279,   247,   249,   260,
     261,   262,   263,   277,   277,   264,   264,   264,   266,   266,
     267,   267,   268,   268,   268,   268,   269,   269,   270,   271,
     272,   273,   274,   279,   277,   287,   280,   247,   280,   223,
     316,   316,   240,   223,   316,   317,   223,   114,   114,   114,
     280,   247,   314,   243,   309,   223,   342,   223,   220,   329,
     338,   339,   221,   221,   247,   221,   221,   221,   264,   243,
     223,   220,   221,   221,   225,   107,   277,   239,   223,   240,
     316,   225,   243,   234,   234,   234,   223,   243,   279,   241,
     221,   319,   328,   242,   331,   240,   320,   323,   324,   264,
     264,   277,   277,   239,   316,   316,   239,   221,   279,   323,
      11,    17,    18,   243,   332,   333,   334,   335,   316,   247,
     247,   241,   319,   279,   239,   319,   332,   319,   243,   334,
     239
};

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		(-2)
#define YYEOF		0

#define YYACCEPT	goto yyacceptlab
#define YYABORT		goto yyabortlab
#define YYERROR		goto yyerrorlab


/* Like YYERROR except do call yyerror.  This remains here temporarily
   to ease the transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  However,
   YYFAIL appears to be in use.  Nevertheless, it is formally deprecated
   in Bison 2.4.2's NEWS entry, where a plan to phase it out is
   discussed.  */

#define YYFAIL		goto yyerrlab
#if defined YYFAIL
  /* This is here to suppress warnings from the GCC cpp's
     -Wunused-macros.  Normally we don't worry about that warning, but
     some users do, and we want to make it easy for users to remove
     YYFAIL uses, which will produce warnings from Bison 2.5.  */
#endif

#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)					\
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    {								\
      yychar = (Token);						\
      yylval = (Value);						\
      YYPOPSTACK (1);						\
      goto yybackup;						\
    }								\
  else								\
    {								\
      yyerror (&yylloc, state, YY_("syntax error: cannot back up")); \
      YYERROR;							\
    }								\
while (YYID (0))


#define YYTERROR	1
#define YYERRCODE	256


/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#define YYRHSLOC(Rhs, K) ((Rhs)[K])
#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)				\
    do									\
      if (YYID (N))                                                    \
	{								\
	  (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;	\
	  (Current).first_column = YYRHSLOC (Rhs, 1).first_column;	\
	  (Current).last_line    = YYRHSLOC (Rhs, N).last_line;		\
	  (Current).last_column  = YYRHSLOC (Rhs, N).last_column;	\
	}								\
      else								\
	{								\
	  (Current).first_line   = (Current).last_line   =		\
	    YYRHSLOC (Rhs, 0).last_line;				\
	  (Current).first_column = (Current).last_column =		\
	    YYRHSLOC (Rhs, 0).last_column;				\
	}								\
    while (YYID (0))
#endif


/* YY_LOCATION_PRINT -- Print the location on the stream.
   This macro was not mandated originally: define only if we know
   we won't break user code: when these are the locations we know.  */

#ifndef YY_LOCATION_PRINT
# if defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL
#  define YY_LOCATION_PRINT(File, Loc)			\
     fprintf (File, "%d.%d-%d.%d",			\
	      (Loc).first_line, (Loc).first_column,	\
	      (Loc).last_line,  (Loc).last_column)
# else
#  define YY_LOCATION_PRINT(File, Loc) ((void) 0)
# endif
#endif


/* YYLEX -- calling `yylex' with the right arguments.  */

#ifdef YYLEX_PARAM
# define YYLEX yylex (&yylval, &yylloc, YYLEX_PARAM)
#else
# define YYLEX yylex (&yylval, &yylloc, scanner)
#endif

/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)			\
do {						\
  if (yydebug)					\
    YYFPRINTF Args;				\
} while (YYID (0))

# define YY_SYMBOL_PRINT(Title, Type, Value, Location)			  \
do {									  \
  if (yydebug)								  \
    {									  \
      YYFPRINTF (stderr, "%s ", Title);					  \
      yy_symbol_print (stderr,						  \
		  Type, Value, Location, state); \
      YYFPRINTF (stderr, "\n");						  \
    }									  \
} while (YYID (0))


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp, struct _mesa_glsl_parse_state *state)
#else
static void
yy_symbol_value_print (yyoutput, yytype, yyvaluep, yylocationp, state)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
    YYLTYPE const * const yylocationp;
    struct _mesa_glsl_parse_state *state;
#endif
{
  if (!yyvaluep)
    return;
  YYUSE (yylocationp);
  YYUSE (state);
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# else
  YYUSE (yyoutput);
# endif
  switch (yytype)
    {
      default:
	break;
    }
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp, struct _mesa_glsl_parse_state *state)
#else
static void
yy_symbol_print (yyoutput, yytype, yyvaluep, yylocationp, state)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
    YYLTYPE const * const yylocationp;
    struct _mesa_glsl_parse_state *state;
#endif
{
  if (yytype < YYNTOKENS)
    YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);

  YY_LOCATION_PRINT (yyoutput, *yylocationp);
  YYFPRINTF (yyoutput, ": ");
  yy_symbol_value_print (yyoutput, yytype, yyvaluep, yylocationp, state);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_stack_print (yytype_int16 *yybottom, yytype_int16 *yytop)
#else
static void
yy_stack_print (yybottom, yytop)
    yytype_int16 *yybottom;
    yytype_int16 *yytop;
#endif
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)				\
do {								\
  if (yydebug)							\
    yy_stack_print ((Bottom), (Top));				\
} while (YYID (0))


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_reduce_print (YYSTYPE *yyvsp, YYLTYPE *yylsp, int yyrule, struct _mesa_glsl_parse_state *state)
#else
static void
yy_reduce_print (yyvsp, yylsp, yyrule, state)
    YYSTYPE *yyvsp;
    YYLTYPE *yylsp;
    int yyrule;
    struct _mesa_glsl_parse_state *state;
#endif
{
  int yynrhs = yyr2[yyrule];
  int yyi;
  unsigned long int yylno = yyrline[yyrule];
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
	     yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr, yyrhs[yyprhs[yyrule] + yyi],
		       &(yyvsp[(yyi + 1) - (yynrhs)])
		       , &(yylsp[(yyi + 1) - (yynrhs)])		       , state);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug)				\
    yy_reduce_print (yyvsp, yylsp, Rule, state); \
} while (YYID (0))

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
#ifndef	YYINITDEPTH
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
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static YYSIZE_T
yystrlen (const char *yystr)
#else
static YYSIZE_T
yystrlen (yystr)
    const char *yystr;
#endif
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
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static char *
yystpcpy (char *yydest, const char *yysrc)
#else
static char *
yystpcpy (yydest, yysrc)
    char *yydest;
    const char *yysrc;
#endif
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
  YYSIZE_T yysize0 = yytnamerr (0, yytname[yytoken]);
  YYSIZE_T yysize = yysize0;
  YYSIZE_T yysize1;
  enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
  /* Internationalized format string. */
  const char *yyformat = 0;
  /* Arguments of yyformat. */
  char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
  /* Number of reported tokens (one for the "unexpected", one per
     "expected"). */
  int yycount = 0;

  /* There are many possibilities here to consider:
     - Assume YYFAIL is not used.  It's too flawed to consider.  See
       <http://lists.gnu.org/archive/html/bison-patches/2009-12/msg00024.html>
       for details.  YYERROR is fine as it does not invoke this
       function.
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
                yysize1 = yysize + yytnamerr (0, yytname[yyx]);
                if (! (yysize <= yysize1
                       && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
                  return 2;
                yysize = yysize1;
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

  yysize1 = yysize + yystrlen (yyformat);
  if (! (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
    return 2;
  yysize = yysize1;

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

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep, YYLTYPE *yylocationp, struct _mesa_glsl_parse_state *state)
#else
static void
yydestruct (yymsg, yytype, yyvaluep, yylocationp, state)
    const char *yymsg;
    int yytype;
    YYSTYPE *yyvaluep;
    YYLTYPE *yylocationp;
    struct _mesa_glsl_parse_state *state;
#endif
{
  YYUSE (yyvaluep);
  YYUSE (yylocationp);
  YYUSE (state);

  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  switch (yytype)
    {

      default:
	break;
    }
}


/* Prevent warnings from -Wmissing-prototypes.  */
#ifdef YYPARSE_PARAM
#if defined __STDC__ || defined __cplusplus
int yyparse (void *YYPARSE_PARAM);
#else
int yyparse ();
#endif
#else /* ! YYPARSE_PARAM */
#if defined __STDC__ || defined __cplusplus
int yyparse (struct _mesa_glsl_parse_state *state);
#else
int yyparse ();
#endif
#endif /* ! YYPARSE_PARAM */


/*----------.
| yyparse.  |
`----------*/

#ifdef YYPARSE_PARAM
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void *YYPARSE_PARAM)
#else
int
yyparse (YYPARSE_PARAM)
    void *YYPARSE_PARAM;
#endif
#else /* ! YYPARSE_PARAM */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (struct _mesa_glsl_parse_state *state)
#else
int
yyparse (state)
    struct _mesa_glsl_parse_state *state;
#endif
#endif
{
/* The lookahead symbol.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;

/* Location data for the lookahead symbol.  */
YYLTYPE yylloc;

    /* Number of syntax errors so far.  */
    int yynerrs;

    int yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       `yyss': related to states.
       `yyvs': related to semantic values.
       `yyls': related to locations.

       Refer to the stacks thru separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* The state stack.  */
    yytype_int16 yyssa[YYINITDEPTH];
    yytype_int16 *yyss;
    yytype_int16 *yyssp;

    /* The semantic value stack.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs;
    YYSTYPE *yyvsp;

    /* The location stack.  */
    YYLTYPE yylsa[YYINITDEPTH];
    YYLTYPE *yyls;
    YYLTYPE *yylsp;

    /* The locations where the error started and ended.  */
    YYLTYPE yyerror_range[3];

    YYSIZE_T yystacksize;

  int yyn;
  int yyresult;
  /* Lookahead token as an internal (translated) token number.  */
  int yytoken;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;
  YYLTYPE yyloc;

#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N), yylsp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  yytoken = 0;
  yyss = yyssa;
  yyvs = yyvsa;
  yyls = yylsa;
  yystacksize = YYINITDEPTH;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY; /* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */
  yyssp = yyss;
  yyvsp = yyvs;
  yylsp = yyls;

#if defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL
  /* Initialize the default location before parsing starts.  */
  yylloc.first_line   = yylloc.last_line   = 1;
  yylloc.first_column = yylloc.last_column = 1;
#endif

/* User initialization code.  */

/* Line 1590 of yacc.c  */
#line 50 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
{
   yylloc.first_line = 1;
   yylloc.first_column = 1;
   yylloc.last_line = 1;
   yylloc.last_column = 1;
   yylloc.source_file = 0;
}

/* Line 1590 of yacc.c  */
#line 3355 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.inl"
  yylsp[0] = yylloc;

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
	YYLTYPE *yyls1 = yyls;

	/* Each stack pointer address is followed by the size of the
	   data in use in that stack, in bytes.  This used to be a
	   conditional around just the two extra args, but that might
	   be undefined if yyoverflow is a macro.  */
	yyoverflow (YY_("memory exhausted"),
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),
		    &yyls1, yysize * sizeof (*yylsp),
		    &yystacksize);

	yyls = yyls1;
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
	YYSTACK_RELOCATE (yyls_alloc, yyls);
#  undef YYSTACK_RELOCATE
	if (yyss1 != yyssa)
	  YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;
      yylsp = yyls + yysize - 1;

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
      yychar = YYLEX;
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
  *++yyvsp = yylval;
  *++yylsp = yylloc;
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
     `$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];

  /* Default location.  */
  YYLLOC_DEFAULT (yyloc, (yylsp - yylen), yylen);
  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 2:

/* Line 1806 of yacc.c  */
#line 279 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   _mesa_glsl_initialize_types(state);
	}
    break;

  case 3:

/* Line 1806 of yacc.c  */
#line 283 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   delete state->symbols;
	   state->symbols = new(ralloc_parent(state)) glsl_symbol_table;
	   _mesa_glsl_initialize_types(state);
	}
    break;

  case 7:

/* Line 1806 of yacc.c  */
#line 298 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   /* FINISHME: The NULL test is required because pragmas are set to
		* FINISHME: NULL. (See production rule for external_declaration.)
		*/
	   if ((yyvsp[(1) - (1)].node) != NULL)
		  state->translation_unit.push_tail(& (yyvsp[(1) - (1)].node)->link);
	}
    break;

  case 8:

/* Line 1806 of yacc.c  */
#line 306 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   /* FINISHME: The NULL test is required because pragmas are set to
		* FINISHME: NULL. (See production rule for external_declaration.)
		*/
	   if ((yyvsp[(2) - (2)].node) != NULL)
		  state->translation_unit.push_tail(& (yyvsp[(2) - (2)].node)->link);
	}
    break;

  case 11:

/* Line 1806 of yacc.c  */
#line 322 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression(ast_identifier, NULL, NULL, NULL);
	   (yyval.expression)->set_location(yylloc);
	   (yyval.expression)->primary_expression.identifier = (yyvsp[(1) - (1)].identifier);
	}
    break;

  case 12:

/* Line 1806 of yacc.c  */
#line 329 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression(ast_int_constant, NULL, NULL, NULL);
	   (yyval.expression)->set_location(yylloc);
	   (yyval.expression)->primary_expression.int_constant = (yyvsp[(1) - (1)].n);
	}
    break;

  case 13:

/* Line 1806 of yacc.c  */
#line 336 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression(state->bGenerateES ? ast_int_constant : ast_uint_constant, NULL, NULL, NULL);
	   (yyval.expression)->set_location(yylloc);
	   if (state->bGenerateES)
	   {
		   (yyval.expression)->primary_expression.int_constant = (yyvsp[(1) - (1)].n);
	   }
	   else
	   {
		   (yyval.expression)->primary_expression.uint_constant = (yyvsp[(1) - (1)].n);
	   }
	}
    break;

  case 14:

/* Line 1806 of yacc.c  */
#line 350 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression(ast_float_constant, NULL, NULL, NULL);
	   (yyval.expression)->set_location(yylloc);
	   (yyval.expression)->primary_expression.float_constant = (yyvsp[(1) - (1)].real);
	}
    break;

  case 15:

/* Line 1806 of yacc.c  */
#line 357 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression(ast_bool_constant, NULL, NULL, NULL);
	   (yyval.expression)->set_location(yylloc);
	   (yyval.expression)->primary_expression.bool_constant = (yyvsp[(1) - (1)].n);
	}
    break;

  case 16:

/* Line 1806 of yacc.c  */
#line 364 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.expression) = (yyvsp[(2) - (3)].expression);
	}
    break;

  case 18:

/* Line 1806 of yacc.c  */
#line 372 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression(ast_array_index, (yyvsp[(1) - (4)].expression), (yyvsp[(3) - (4)].expression), NULL);
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 19:

/* Line 1806 of yacc.c  */
#line 378 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.expression) = (yyvsp[(1) - (1)].expression);
	}
    break;

  case 20:

/* Line 1806 of yacc.c  */
#line 382 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression(ast_field_selection, (yyvsp[(1) - (3)].expression), NULL, NULL);
	   (yyval.expression)->set_location(yylloc);
	   (yyval.expression)->primary_expression.identifier = (yyvsp[(3) - (3)].identifier);
	}
    break;

  case 21:

/* Line 1806 of yacc.c  */
#line 389 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression(ast_post_inc, (yyvsp[(1) - (2)].expression), NULL, NULL);
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 22:

/* Line 1806 of yacc.c  */
#line 395 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression(ast_post_dec, (yyvsp[(1) - (2)].expression), NULL, NULL);
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 26:

/* Line 1806 of yacc.c  */
#line 413 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression(ast_field_selection, (yyvsp[(1) - (3)].expression), (yyvsp[(3) - (3)].expression), NULL);
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 31:

/* Line 1806 of yacc.c  */
#line 432 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.expression) = (yyvsp[(1) - (2)].expression);
	   (yyval.expression)->set_location(yylloc);
	   (yyval.expression)->expressions.push_tail(& (yyvsp[(2) - (2)].expression)->link);
	}
    break;

  case 32:

/* Line 1806 of yacc.c  */
#line 438 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.expression) = (yyvsp[(1) - (3)].expression);
	   (yyval.expression)->set_location(yylloc);
	   (yyval.expression)->expressions.push_tail(& (yyvsp[(3) - (3)].expression)->link);
	}
    break;

  case 34:

/* Line 1806 of yacc.c  */
#line 454 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_function_expression((yyvsp[(1) - (1)].type_specifier));
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 35:

/* Line 1806 of yacc.c  */
#line 460 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   ast_expression *callee = new(ctx) ast_expression((yyvsp[(1) - (1)].identifier));
	   (yyval.expression) = new(ctx) ast_function_expression(callee);
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 36:

/* Line 1806 of yacc.c  */
#line 467 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   ast_expression *callee = new(ctx) ast_expression((yyvsp[(1) - (1)].identifier));
	   (yyval.expression) = new(ctx) ast_function_expression(callee);
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 41:

/* Line 1806 of yacc.c  */
#line 487 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.expression) = (yyvsp[(1) - (2)].expression);
	   (yyval.expression)->set_location(yylloc);
	   (yyval.expression)->expressions.push_tail(& (yyvsp[(2) - (2)].expression)->link);
	}
    break;

  case 42:

/* Line 1806 of yacc.c  */
#line 493 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.expression) = (yyvsp[(1) - (3)].expression);
	   (yyval.expression)->set_location(yylloc);
	   (yyval.expression)->expressions.push_tail(& (yyvsp[(3) - (3)].expression)->link);
	}
    break;

  case 43:

/* Line 1806 of yacc.c  */
#line 505 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   ast_expression *callee = new(ctx) ast_expression((yyvsp[(1) - (2)].identifier));
	   (yyval.expression) = new(ctx) ast_function_expression(callee);
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 45:

/* Line 1806 of yacc.c  */
#line 517 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression(ast_pre_inc, (yyvsp[(2) - (2)].expression), NULL, NULL);
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 46:

/* Line 1806 of yacc.c  */
#line 523 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression(ast_pre_dec, (yyvsp[(2) - (2)].expression), NULL, NULL);
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 47:

/* Line 1806 of yacc.c  */
#line 529 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression((yyvsp[(1) - (2)].n), (yyvsp[(2) - (2)].expression), NULL, NULL);
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 48:

/* Line 1806 of yacc.c  */
#line 535 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.expression) = new(ctx) ast_expression(ast_type_cast, (yyvsp[(4) - (4)].expression), NULL, NULL);
		(yyval.expression)->primary_expression.type_specifier = (yyvsp[(2) - (4)].type_specifier);
		(yyval.expression)->set_location(yylloc);
	}
    break;

  case 49:

/* Line 1806 of yacc.c  */
#line 542 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.expression) = new(ctx) ast_expression(ast_type_cast, (yyvsp[(5) - (5)].expression), NULL, NULL);
		(yyval.expression)->primary_expression.type_specifier = (yyvsp[(3) - (5)].type_specifier);
		(yyval.expression)->set_location(yylloc);
	}
    break;

  case 50:

/* Line 1806 of yacc.c  */
#line 549 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.expression) = new(ctx) ast_expression(ast_type_cast, (yyvsp[(5) - (5)].expression), NULL, NULL);
		(yyval.expression)->primary_expression.type_specifier = (yyvsp[(2) - (5)].type_specifier);
		(yyval.expression)->set_location(yylloc);
	}
    break;

  case 51:

/* Line 1806 of yacc.c  */
#line 559 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.n) = ast_plus; }
    break;

  case 52:

/* Line 1806 of yacc.c  */
#line 560 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.n) = ast_neg; }
    break;

  case 53:

/* Line 1806 of yacc.c  */
#line 561 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.n) = ast_logic_not; }
    break;

  case 54:

/* Line 1806 of yacc.c  */
#line 562 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.n) = ast_bit_not; }
    break;

  case 56:

/* Line 1806 of yacc.c  */
#line 568 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression_bin(ast_mul, (yyvsp[(1) - (3)].expression), (yyvsp[(3) - (3)].expression));
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 57:

/* Line 1806 of yacc.c  */
#line 574 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression_bin(ast_div, (yyvsp[(1) - (3)].expression), (yyvsp[(3) - (3)].expression));
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 58:

/* Line 1806 of yacc.c  */
#line 580 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression_bin(ast_mod, (yyvsp[(1) - (3)].expression), (yyvsp[(3) - (3)].expression));
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 60:

/* Line 1806 of yacc.c  */
#line 590 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression_bin(ast_add, (yyvsp[(1) - (3)].expression), (yyvsp[(3) - (3)].expression));
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 61:

/* Line 1806 of yacc.c  */
#line 596 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression_bin(ast_sub, (yyvsp[(1) - (3)].expression), (yyvsp[(3) - (3)].expression));
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 63:

/* Line 1806 of yacc.c  */
#line 606 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression_bin(ast_lshift, (yyvsp[(1) - (3)].expression), (yyvsp[(3) - (3)].expression));
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 64:

/* Line 1806 of yacc.c  */
#line 612 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression_bin(ast_rshift, (yyvsp[(1) - (3)].expression), (yyvsp[(3) - (3)].expression));
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 66:

/* Line 1806 of yacc.c  */
#line 622 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression_bin(ast_less, (yyvsp[(1) - (3)].expression), (yyvsp[(3) - (3)].expression));
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 67:

/* Line 1806 of yacc.c  */
#line 628 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression_bin(ast_greater, (yyvsp[(1) - (3)].expression), (yyvsp[(3) - (3)].expression));
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 68:

/* Line 1806 of yacc.c  */
#line 634 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression_bin(ast_lequal, (yyvsp[(1) - (3)].expression), (yyvsp[(3) - (3)].expression));
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 69:

/* Line 1806 of yacc.c  */
#line 640 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression_bin(ast_gequal, (yyvsp[(1) - (3)].expression), (yyvsp[(3) - (3)].expression));
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 71:

/* Line 1806 of yacc.c  */
#line 650 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression_bin(ast_equal, (yyvsp[(1) - (3)].expression), (yyvsp[(3) - (3)].expression));
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 72:

/* Line 1806 of yacc.c  */
#line 656 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression_bin(ast_nequal, (yyvsp[(1) - (3)].expression), (yyvsp[(3) - (3)].expression));
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 74:

/* Line 1806 of yacc.c  */
#line 666 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression_bin(ast_bit_and, (yyvsp[(1) - (3)].expression), (yyvsp[(3) - (3)].expression));
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 76:

/* Line 1806 of yacc.c  */
#line 676 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression_bin(ast_bit_xor, (yyvsp[(1) - (3)].expression), (yyvsp[(3) - (3)].expression));
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 78:

/* Line 1806 of yacc.c  */
#line 686 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression_bin(ast_bit_or, (yyvsp[(1) - (3)].expression), (yyvsp[(3) - (3)].expression));
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 80:

/* Line 1806 of yacc.c  */
#line 696 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression_bin(ast_logic_and, (yyvsp[(1) - (3)].expression), (yyvsp[(3) - (3)].expression));
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 82:

/* Line 1806 of yacc.c  */
#line 706 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression_bin(ast_logic_or, (yyvsp[(1) - (3)].expression), (yyvsp[(3) - (3)].expression));
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 84:

/* Line 1806 of yacc.c  */
#line 716 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression(ast_conditional, (yyvsp[(1) - (5)].expression), (yyvsp[(3) - (5)].expression), (yyvsp[(5) - (5)].expression));
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 86:

/* Line 1806 of yacc.c  */
#line 726 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression((yyvsp[(2) - (3)].n), (yyvsp[(1) - (3)].expression), (yyvsp[(3) - (3)].expression), NULL);
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 87:

/* Line 1806 of yacc.c  */
#line 734 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.n) = ast_assign; }
    break;

  case 88:

/* Line 1806 of yacc.c  */
#line 735 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.n) = ast_mul_assign; }
    break;

  case 89:

/* Line 1806 of yacc.c  */
#line 736 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.n) = ast_div_assign; }
    break;

  case 90:

/* Line 1806 of yacc.c  */
#line 737 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.n) = ast_mod_assign; }
    break;

  case 91:

/* Line 1806 of yacc.c  */
#line 738 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.n) = ast_add_assign; }
    break;

  case 92:

/* Line 1806 of yacc.c  */
#line 739 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.n) = ast_sub_assign; }
    break;

  case 93:

/* Line 1806 of yacc.c  */
#line 740 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.n) = ast_ls_assign; }
    break;

  case 94:

/* Line 1806 of yacc.c  */
#line 741 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.n) = ast_rs_assign; }
    break;

  case 95:

/* Line 1806 of yacc.c  */
#line 742 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.n) = ast_and_assign; }
    break;

  case 96:

/* Line 1806 of yacc.c  */
#line 743 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.n) = ast_xor_assign; }
    break;

  case 97:

/* Line 1806 of yacc.c  */
#line 744 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.n) = ast_or_assign; }
    break;

  case 98:

/* Line 1806 of yacc.c  */
#line 749 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.expression) = (yyvsp[(1) - (1)].expression);
	}
    break;

  case 99:

/* Line 1806 of yacc.c  */
#line 753 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   if ((yyvsp[(1) - (3)].expression)->oper != ast_sequence) {
		  (yyval.expression) = new(ctx) ast_expression(ast_sequence, NULL, NULL, NULL);
		  (yyval.expression)->set_location(yylloc);
		  (yyval.expression)->expressions.push_tail(& (yyvsp[(1) - (3)].expression)->link);
	   } else {
		  (yyval.expression) = (yyvsp[(1) - (3)].expression);
	   }

	   (yyval.expression)->expressions.push_tail(& (yyvsp[(3) - (3)].expression)->link);
	}
    break;

  case 101:

/* Line 1806 of yacc.c  */
#line 773 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   state->symbols->pop_scope();
	   (yyval.function) = (yyvsp[(1) - (2)].function);
	}
    break;

  case 102:

/* Line 1806 of yacc.c  */
#line 781 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		(yyval.node) = (yyvsp[(1) - (1)].function);
	}
    break;

  case 103:

/* Line 1806 of yacc.c  */
#line 785 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.node) = (yyvsp[(1) - (2)].declarator_list);
	}
    break;

  case 104:

/* Line 1806 of yacc.c  */
#line 792 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	}
    break;

  case 105:

/* Line 1806 of yacc.c  */
#line 795 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		(yyval.function) = (yyvsp[(1) - (4)].function);
		(yyval.function)->return_semantic = (yyvsp[(4) - (4)].identifier);
	}
    break;

  case 108:

/* Line 1806 of yacc.c  */
#line 808 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.function) = (yyvsp[(1) - (2)].function);
	   (yyval.function)->parameters.push_tail(& (yyvsp[(2) - (2)].parameter_declarator)->link);
	}
    break;

  case 109:

/* Line 1806 of yacc.c  */
#line 813 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.function) = (yyvsp[(1) - (3)].function);
	   (yyval.function)->parameters.push_tail(& (yyvsp[(3) - (3)].parameter_declarator)->link);
	}
    break;

  case 110:

/* Line 1806 of yacc.c  */
#line 821 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.function) = new(ctx) ast_function();
	   (yyval.function)->set_location(yylloc);
	   (yyval.function)->return_type = (yyvsp[(1) - (3)].fully_specified_type);
	   (yyval.function)->identifier = (yyvsp[(2) - (3)].identifier);

	   state->symbols->add_function(new(state) ir_function((yyvsp[(2) - (3)].identifier)));
	   state->symbols->push_scope();
	}
    break;

  case 111:

/* Line 1806 of yacc.c  */
#line 832 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.function) = new(ctx) ast_function();
	   (yyval.function)->set_location(yylloc);
	   (yyval.function)->return_type = (yyvsp[(2) - (4)].fully_specified_type);
	   (yyval.function)->identifier = (yyvsp[(3) - (4)].identifier);

	   state->symbols->add_function(new(state) ir_function((yyvsp[(3) - (4)].identifier)));
	   state->symbols->push_scope();
	}
    break;

  case 112:

/* Line 1806 of yacc.c  */
#line 846 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.parameter_declarator) = new(ctx) ast_parameter_declarator();
	   (yyval.parameter_declarator)->set_location(yylloc);
	   (yyval.parameter_declarator)->type = new(ctx) ast_fully_specified_type();
	   (yyval.parameter_declarator)->type->set_location(yylloc);
	   (yyval.parameter_declarator)->type->specifier = (yyvsp[(1) - (2)].type_specifier);
	   (yyval.parameter_declarator)->identifier = (yyvsp[(2) - (2)].identifier);
	}
    break;

  case 113:

/* Line 1806 of yacc.c  */
#line 856 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.parameter_declarator) = new(ctx) ast_parameter_declarator();
		(yyval.parameter_declarator)->set_location(yylloc);
		(yyval.parameter_declarator)->type = new(ctx) ast_fully_specified_type();
		(yyval.parameter_declarator)->type->set_location(yylloc);
		(yyval.parameter_declarator)->type->specifier = (yyvsp[(1) - (4)].type_specifier);
		(yyval.parameter_declarator)->identifier = (yyvsp[(2) - (4)].identifier);
		(yyval.parameter_declarator)->default_value = (yyvsp[(4) - (4)].expression);
	}
    break;

  case 114:

/* Line 1806 of yacc.c  */
#line 867 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.parameter_declarator) = new(ctx) ast_parameter_declarator();
		(yyval.parameter_declarator)->set_location(yylloc);
		(yyval.parameter_declarator)->type = new(ctx) ast_fully_specified_type();
		(yyval.parameter_declarator)->type->set_location(yylloc);
		(yyval.parameter_declarator)->type->specifier = (yyvsp[(1) - (4)].type_specifier);
		(yyval.parameter_declarator)->identifier = (yyvsp[(2) - (4)].identifier);
		(yyval.parameter_declarator)->semantic = (yyvsp[(4) - (4)].identifier);
	}
    break;

  case 115:

/* Line 1806 of yacc.c  */
#line 878 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.parameter_declarator) = new(ctx) ast_parameter_declarator();
	   (yyval.parameter_declarator)->set_location(yylloc);
	   (yyval.parameter_declarator)->type = new(ctx) ast_fully_specified_type();
	   (yyval.parameter_declarator)->type->set_location(yylloc);
	   (yyval.parameter_declarator)->type->specifier = (yyvsp[(1) - (2)].type_specifier);
	   (yyval.parameter_declarator)->identifier = (yyvsp[(2) - (2)].declaration)->identifier;
	   (yyval.parameter_declarator)->is_array = true;
	   (yyval.parameter_declarator)->array_size = (yyvsp[(2) - (2)].declaration)->array_size;
	}
    break;

  case 116:

/* Line 1806 of yacc.c  */
#line 890 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.parameter_declarator) = new(ctx) ast_parameter_declarator();
	   (yyval.parameter_declarator)->set_location(yylloc);
	   (yyval.parameter_declarator)->type = new(ctx) ast_fully_specified_type();
	   (yyval.parameter_declarator)->type->set_location(yylloc);
	   (yyval.parameter_declarator)->type->specifier = (yyvsp[(1) - (7)].type_specifier);
	   (yyval.parameter_declarator)->identifier = (yyvsp[(2) - (7)].identifier);
	   (yyval.parameter_declarator)->is_array = true;
	   (yyval.parameter_declarator)->array_size = (yyvsp[(4) - (7)].expression);
	   (yyval.parameter_declarator)->semantic = (yyvsp[(7) - (7)].identifier);
	}
    break;

  case 117:

/* Line 1806 of yacc.c  */
#line 906 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyvsp[(1) - (3)].type_qualifier).flags.i |= (yyvsp[(2) - (3)].type_qualifier).flags.i;

	   (yyval.parameter_declarator) = (yyvsp[(3) - (3)].parameter_declarator);
	   (yyval.parameter_declarator)->type->qualifier = (yyvsp[(1) - (3)].type_qualifier);
	}
    break;

  case 118:

/* Line 1806 of yacc.c  */
#line 913 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyvsp[(1) - (3)].type_qualifier).flags.i |= (yyvsp[(2) - (3)].type_qualifier).flags.i;

	   (yyval.parameter_declarator) = (yyvsp[(3) - (3)].parameter_declarator);
	   (yyval.parameter_declarator)->type->qualifier = (yyvsp[(1) - (3)].type_qualifier);
	}
    break;

  case 119:

/* Line 1806 of yacc.c  */
#line 920 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyvsp[(1) - (4)].type_qualifier).flags.i |= (yyvsp[(2) - (4)].type_qualifier).flags.i;
	   (yyvsp[(1) - (4)].type_qualifier).flags.i |= (yyvsp[(3) - (4)].type_qualifier).flags.i;

	   (yyval.parameter_declarator) = (yyvsp[(4) - (4)].parameter_declarator);
	   (yyval.parameter_declarator)->type->qualifier = (yyvsp[(1) - (4)].type_qualifier);
	}
    break;

  case 120:

/* Line 1806 of yacc.c  */
#line 928 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.parameter_declarator) = (yyvsp[(2) - (2)].parameter_declarator);
	   (yyval.parameter_declarator)->type->qualifier = (yyvsp[(1) - (2)].type_qualifier);
	}
    break;

  case 121:

/* Line 1806 of yacc.c  */
#line 933 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.parameter_declarator) = (yyvsp[(2) - (2)].parameter_declarator);
	   (yyval.parameter_declarator)->type->qualifier = (yyvsp[(1) - (2)].type_qualifier);
	}
    break;

  case 123:

/* Line 1806 of yacc.c  */
#line 939 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyvsp[(1) - (3)].type_qualifier).flags.i |= (yyvsp[(2) - (3)].type_qualifier).flags.i;

	   (yyval.parameter_declarator) = new(ctx) ast_parameter_declarator();
	   (yyval.parameter_declarator)->set_location(yylloc);
	   (yyval.parameter_declarator)->type = new(ctx) ast_fully_specified_type();
	   (yyval.parameter_declarator)->type->qualifier = (yyvsp[(1) - (3)].type_qualifier);
	   (yyval.parameter_declarator)->type->specifier = (yyvsp[(3) - (3)].type_specifier);
	}
    break;

  case 124:

/* Line 1806 of yacc.c  */
#line 950 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.parameter_declarator) = new(ctx) ast_parameter_declarator();
	   (yyval.parameter_declarator)->set_location(yylloc);
	   (yyval.parameter_declarator)->type = new(ctx) ast_fully_specified_type();
	   (yyval.parameter_declarator)->type->qualifier = (yyvsp[(1) - (2)].type_qualifier);
	   (yyval.parameter_declarator)->type->specifier = (yyvsp[(2) - (2)].type_specifier);
	}
    break;

  case 125:

/* Line 1806 of yacc.c  */
#line 962 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
	   (yyval.type_qualifier).flags.q.in = 1;
	}
    break;

  case 126:

/* Line 1806 of yacc.c  */
#line 967 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
	   (yyval.type_qualifier).flags.q.out = 1;
	}
    break;

  case 127:

/* Line 1806 of yacc.c  */
#line 972 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
	   (yyval.type_qualifier).flags.q.in = 1;
	   (yyval.type_qualifier).flags.q.out = 1;
	}
    break;

  case 128:

/* Line 1806 of yacc.c  */
#line 978 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
		(yyval.type_qualifier).flags.q.in = 1;
		(yyval.type_qualifier).flags.q.out = 1;
	}
    break;

  case 129:

/* Line 1806 of yacc.c  */
#line 984 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
		(yyval.type_qualifier).flags.q.in = 1;
		(yyval.type_qualifier).flags.q.out = 1;
	}
    break;

  case 130:

/* Line 1806 of yacc.c  */
#line 990 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
		(yyval.type_qualifier).flags.q.gs_point = 1;
	}
    break;

  case 131:

/* Line 1806 of yacc.c  */
#line 995 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
		(yyval.type_qualifier).flags.q.gs_line = 1;
	}
    break;

  case 132:

/* Line 1806 of yacc.c  */
#line 1000 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
		(yyval.type_qualifier).flags.q.gs_triangle = 1;
	}
    break;

  case 133:

/* Line 1806 of yacc.c  */
#line 1005 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
		(yyval.type_qualifier).flags.q.gs_lineadj = 1;
	}
    break;

  case 134:

/* Line 1806 of yacc.c  */
#line 1010 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
		(yyval.type_qualifier).flags.q.gs_triangleadj = 1;
	}
    break;

  case 137:

/* Line 1806 of yacc.c  */
#line 1023 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   ast_declaration *decl = new(ctx) ast_declaration((yyvsp[(3) - (3)].identifier), false, NULL, NULL);
	   decl->set_location(yylloc);

	   (yyval.declarator_list) = (yyvsp[(1) - (3)].declarator_list);
	   (yyval.declarator_list)->declarations.push_tail(&decl->link);
	   state->symbols->add_variable(new(state) ir_variable(NULL, (yyvsp[(3) - (3)].identifier), ir_var_auto));
	}
    break;

  case 138:

/* Line 1806 of yacc.c  */
#line 1033 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   ast_declaration *decl = new(ctx) ast_declaration((yyvsp[(3) - (5)].identifier), true, NULL, NULL);
	   decl->set_location(yylloc);

	   (yyval.declarator_list) = (yyvsp[(1) - (5)].declarator_list);
	   (yyval.declarator_list)->declarations.push_tail(&decl->link);
	   state->symbols->add_variable(new(state) ir_variable(NULL, (yyvsp[(3) - (5)].identifier), ir_var_auto));
	}
    break;

  case 139:

/* Line 1806 of yacc.c  */
#line 1043 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   ast_declaration *decl = (yyvsp[(3) - (3)].declaration);

	   (yyval.declarator_list) = (yyvsp[(1) - (3)].declarator_list);
	   (yyval.declarator_list)->declarations.push_tail(&decl->link);
	   state->symbols->add_variable(new(state) ir_variable(NULL, (yyvsp[(3) - (3)].declaration)->identifier, ir_var_auto));
	}
    break;

  case 140:

/* Line 1806 of yacc.c  */
#line 1052 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   ast_declaration *decl = new(ctx) ast_declaration((yyvsp[(3) - (7)].identifier), true, NULL, (yyvsp[(7) - (7)].expression));
	   decl->set_location(yylloc);

	   (yyval.declarator_list) = (yyvsp[(1) - (7)].declarator_list);
	   (yyval.declarator_list)->declarations.push_tail(&decl->link);
	   state->symbols->add_variable(new(state) ir_variable(NULL, (yyvsp[(3) - (7)].identifier), ir_var_auto));
	}
    break;

  case 141:

/* Line 1806 of yacc.c  */
#line 1062 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   ast_declaration *decl = (yyvsp[(3) - (5)].declaration);
	   decl->set_location(yylloc);

	   (yyval.declarator_list) = (yyvsp[(1) - (5)].declarator_list);
	   (yyval.declarator_list)->declarations.push_tail(&decl->link);
	   state->symbols->add_variable(new(state) ir_variable(NULL, (yyvsp[(3) - (5)].declaration)->identifier, ir_var_auto));
	}
    break;

  case 142:

/* Line 1806 of yacc.c  */
#line 1072 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   ast_declaration *decl = new(ctx) ast_declaration((yyvsp[(3) - (5)].identifier), false, NULL, (yyvsp[(5) - (5)].expression));
	   decl->set_location(yylloc);

	   (yyval.declarator_list) = (yyvsp[(1) - (5)].declarator_list);
	   (yyval.declarator_list)->declarations.push_tail(&decl->link);
	   state->symbols->add_variable(new(state) ir_variable(NULL, (yyvsp[(3) - (5)].identifier), ir_var_auto));
	}
    break;

  case 143:

/* Line 1806 of yacc.c  */
#line 1086 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   /* Empty declaration list is valid. */
	   (yyval.declarator_list) = new(ctx) ast_declarator_list((yyvsp[(1) - (1)].fully_specified_type));
	   (yyval.declarator_list)->set_location(yylloc);
	}
    break;

  case 144:

/* Line 1806 of yacc.c  */
#line 1093 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   ast_declaration *decl = new(ctx) ast_declaration((yyvsp[(2) - (2)].identifier), false, NULL, NULL);

	   (yyval.declarator_list) = new(ctx) ast_declarator_list((yyvsp[(1) - (2)].fully_specified_type));
	   (yyval.declarator_list)->set_location(yylloc);
	   (yyval.declarator_list)->declarations.push_tail(&decl->link);
	}
    break;

  case 145:

/* Line 1806 of yacc.c  */
#line 1102 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   ast_declaration *decl = new(ctx) ast_declaration((yyvsp[(2) - (4)].identifier), true, NULL, NULL);

	   (yyval.declarator_list) = new(ctx) ast_declarator_list((yyvsp[(1) - (4)].fully_specified_type));
	   (yyval.declarator_list)->set_location(yylloc);
	   (yyval.declarator_list)->declarations.push_tail(&decl->link);
	}
    break;

  case 146:

/* Line 1806 of yacc.c  */
#line 1111 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   ast_declaration *decl = (yyvsp[(2) - (2)].declaration);

	   (yyval.declarator_list) = new(ctx) ast_declarator_list((yyvsp[(1) - (2)].fully_specified_type));
	   (yyval.declarator_list)->set_location(yylloc);
	   (yyval.declarator_list)->declarations.push_tail(&decl->link);
	}
    break;

  case 147:

/* Line 1806 of yacc.c  */
#line 1120 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   ast_declaration *decl = new(ctx) ast_declaration((yyvsp[(2) - (6)].identifier), true, NULL, (yyvsp[(6) - (6)].expression));

	   (yyval.declarator_list) = new(ctx) ast_declarator_list((yyvsp[(1) - (6)].fully_specified_type));
	   (yyval.declarator_list)->set_location(yylloc);
	   (yyval.declarator_list)->declarations.push_tail(&decl->link);
	}
    break;

  case 148:

/* Line 1806 of yacc.c  */
#line 1129 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   ast_declaration *decl = (yyvsp[(2) - (4)].declaration);
	   decl->initializer = (yyvsp[(4) - (4)].expression);

	   (yyval.declarator_list) = new(ctx) ast_declarator_list((yyvsp[(1) - (4)].fully_specified_type));
	   (yyval.declarator_list)->set_location(yylloc);
	   (yyval.declarator_list)->declarations.push_tail(&decl->link);
	}
    break;

  case 149:

/* Line 1806 of yacc.c  */
#line 1139 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   ast_declaration *decl = new(ctx) ast_declaration((yyvsp[(2) - (4)].identifier), false, NULL, (yyvsp[(4) - (4)].expression));

	   (yyval.declarator_list) = new(ctx) ast_declarator_list((yyvsp[(1) - (4)].fully_specified_type));
	   (yyval.declarator_list)->set_location(yylloc);
	   (yyval.declarator_list)->declarations.push_tail(&decl->link);
	}
    break;

  case 150:

/* Line 1806 of yacc.c  */
#line 1148 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   ast_declaration *decl = new(ctx) ast_declaration((yyvsp[(2) - (2)].identifier), false, NULL, NULL);

	   (yyval.declarator_list) = new(ctx) ast_declarator_list(NULL);
	   (yyval.declarator_list)->set_location(yylloc);
	   (yyval.declarator_list)->invariant = true;

	   (yyval.declarator_list)->declarations.push_tail(&decl->link);
	}
    break;

  case 151:

/* Line 1806 of yacc.c  */
#line 1162 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.fully_specified_type) = new(ctx) ast_fully_specified_type();
	   (yyval.fully_specified_type)->set_location(yylloc);
	   (yyval.fully_specified_type)->specifier = (yyvsp[(1) - (1)].type_specifier);
	}
    break;

  case 152:

/* Line 1806 of yacc.c  */
#line 1169 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.fully_specified_type) = new(ctx) ast_fully_specified_type();
	   (yyval.fully_specified_type)->set_location(yylloc);
	   (yyval.fully_specified_type)->qualifier = (yyvsp[(1) - (2)].type_qualifier);
	   (yyval.fully_specified_type)->specifier = (yyvsp[(2) - (2)].type_specifier);
	}
    break;

  case 153:

/* Line 1806 of yacc.c  */
#line 1180 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
	   (yyval.type_qualifier).flags.q.smooth = 1;
	}
    break;

  case 154:

/* Line 1806 of yacc.c  */
#line 1185 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
	   (yyval.type_qualifier).flags.q.flat = 1;
	}
    break;

  case 155:

/* Line 1806 of yacc.c  */
#line 1190 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
	   (yyval.type_qualifier).flags.q.noperspective = 1;
	}
    break;

  case 157:

/* Line 1806 of yacc.c  */
#line 1199 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		(yyval.type_qualifier) = (yyvsp[(2) - (2)].type_qualifier);
		(yyval.type_qualifier).flags.q.centroid = 1;
	}
    break;

  case 158:

/* Line 1806 of yacc.c  */
#line 1204 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		(yyval.type_qualifier) = (yyvsp[(1) - (2)].type_qualifier);
		(yyval.type_qualifier).flags.q.centroid = 1;
	}
    break;

  case 159:

/* Line 1806 of yacc.c  */
#line 1209 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
	   (yyval.type_qualifier).flags.q.centroid = 1;
	}
    break;

  case 160:

/* Line 1806 of yacc.c  */
#line 1214 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
	   (yyval.type_qualifier).flags.q.constant = 1;
	}
    break;

  case 161:

/* Line 1806 of yacc.c  */
#line 1219 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
	   (yyval.type_qualifier).flags.q.uniform = 1;
	}
    break;

  case 164:

/* Line 1806 of yacc.c  */
#line 1229 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.type_qualifier) = (yyvsp[(1) - (2)].type_qualifier);
	   (yyval.type_qualifier).flags.i |= (yyvsp[(2) - (2)].type_qualifier).flags.i;
	}
    break;

  case 165:

/* Line 1806 of yacc.c  */
#line 1234 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.type_qualifier) = (yyvsp[(2) - (2)].type_qualifier);
	   (yyval.type_qualifier).flags.q.invariant = 1;
	}
    break;

  case 166:

/* Line 1806 of yacc.c  */
#line 1239 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.type_qualifier) = (yyvsp[(2) - (3)].type_qualifier);
	   (yyval.type_qualifier).flags.i |= (yyvsp[(3) - (3)].type_qualifier).flags.i;
	   (yyval.type_qualifier).flags.q.invariant = 1;
	}
    break;

  case 167:

/* Line 1806 of yacc.c  */
#line 1245 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
	   (yyval.type_qualifier).flags.q.invariant = 1;
	}
    break;

  case 168:

/* Line 1806 of yacc.c  */
#line 1253 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
	   (yyval.type_qualifier).flags.q.constant = 1;
	}
    break;

  case 169:

/* Line 1806 of yacc.c  */
#line 1258 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
	   (yyval.type_qualifier).flags.q.varying = 1;
	}
    break;

  case 170:

/* Line 1806 of yacc.c  */
#line 1263 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
	   (yyval.type_qualifier).flags.q.centroid = 1;
	   (yyval.type_qualifier).flags.q.varying = 1;
	}
    break;

  case 171:

/* Line 1806 of yacc.c  */
#line 1269 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
	   (yyval.type_qualifier).flags.q.in = 1;
	}
    break;

  case 172:

/* Line 1806 of yacc.c  */
#line 1274 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
	   (yyval.type_qualifier).flags.q.out = 1;
	}
    break;

  case 173:

/* Line 1806 of yacc.c  */
#line 1279 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
	   (yyval.type_qualifier).flags.q.centroid = 1; (yyval.type_qualifier).flags.q.in = 1;
	}
    break;

  case 174:

/* Line 1806 of yacc.c  */
#line 1284 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
	   (yyval.type_qualifier).flags.q.centroid = 1; (yyval.type_qualifier).flags.q.out = 1;
	}
    break;

  case 175:

/* Line 1806 of yacc.c  */
#line 1289 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
	   (yyval.type_qualifier).flags.q.uniform = 1;
	}
    break;

  case 176:

/* Line 1806 of yacc.c  */
#line 1294 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
		(yyval.type_qualifier).flags.q.row_major = 1;
	}
    break;

  case 177:

/* Line 1806 of yacc.c  */
#line 1299 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
		(yyval.type_qualifier).flags.q.column_major = 1;
	}
    break;

  case 178:

/* Line 1806 of yacc.c  */
#line 1304 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
		(yyval.type_qualifier).flags.q.is_static = 1;
	}
    break;

  case 179:

/* Line 1806 of yacc.c  */
#line 1309 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
		(yyval.type_qualifier).flags.q.constant = 1;
		(yyval.type_qualifier).flags.q.is_static = 1;
	}
    break;

  case 180:

/* Line 1806 of yacc.c  */
#line 1315 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
		(yyval.type_qualifier).flags.q.constant = 1;
		(yyval.type_qualifier).flags.q.is_static = 1;
	}
    break;

  case 181:

/* Line 1806 of yacc.c  */
#line 1321 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
		(yyval.type_qualifier).flags.q.coherent = 1;
	}
    break;

  case 182:

/* Line 1806 of yacc.c  */
#line 1326 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
		(yyval.type_qualifier).flags.q.shared = 1;
	}
    break;

  case 183:

/* Line 1806 of yacc.c  */
#line 1334 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.type_specifier) = (yyvsp[(1) - (1)].type_specifier);
	}
    break;

  case 186:

/* Line 1806 of yacc.c  */
#line 1346 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.type_specifier) = (yyvsp[(1) - (3)].type_specifier);
	   (yyval.type_specifier)->is_array = true;
	   (yyval.type_specifier)->is_unsized_array++;
	}
    break;

  case 187:

/* Line 1806 of yacc.c  */
#line 1352 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.type_specifier) = (yyvsp[(1) - (3)].type_specifier);
	   (yyval.type_specifier)->is_unsized_array++;
	}
    break;

  case 188:

/* Line 1806 of yacc.c  */
#line 1357 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.type_specifier) = (yyvsp[(1) - (4)].type_specifier);
	   (yyval.type_specifier)->is_array = true;
	   (yyval.type_specifier)->array_size = (yyvsp[(3) - (4)].expression);
	}
    break;

  case 189:

/* Line 1806 of yacc.c  */
#line 1363 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.type_specifier) = (yyvsp[(1) - (4)].type_specifier);
	   (yyvsp[(3) - (4)].expression)->link.next = &((yyval.type_specifier)->array_size->link);
	   (yyval.type_specifier)->array_size = (yyvsp[(3) - (4)].expression);
	}
    break;

  case 190:

/* Line 1806 of yacc.c  */
#line 1372 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.type_specifier) = new(ctx) ast_type_specifier((yyvsp[(1) - (1)].identifier));
	   (yyval.type_specifier)->set_location(yylloc);
	}
    break;

  case 191:

/* Line 1806 of yacc.c  */
#line 1378 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.type_specifier) = new(ctx) ast_type_specifier((yyvsp[(1) - (1)].identifier),"vec4");
		(yyval.type_specifier)->set_location(yylloc);
	}
    break;

  case 192:

/* Line 1806 of yacc.c  */
#line 1384 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.type_specifier) = new(ctx) ast_type_specifier((yyvsp[(1) - (6)].identifier),(yyvsp[(3) - (6)].identifier));
		(yyval.type_specifier)->texture_ms_num_samples = (yyvsp[(5) - (6)].n);
		(yyval.type_specifier)->set_location(yylloc);
	}
    break;

  case 193:

/* Line 1806 of yacc.c  */
#line 1391 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.type_specifier) = new(ctx) ast_type_specifier((yyvsp[(1) - (4)].identifier),(yyvsp[(3) - (4)].identifier));
		(yyval.type_specifier)->set_location(yylloc);
	}
    break;

  case 194:

/* Line 1806 of yacc.c  */
#line 1397 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.type_specifier) = new(ctx) ast_type_specifier("StructuredBuffer",(yyvsp[(3) - (4)].identifier));
		(yyval.type_specifier)->set_location(yylloc);
	}
    break;

  case 195:

/* Line 1806 of yacc.c  */
#line 1403 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.type_specifier) = new(ctx) ast_type_specifier("StructuredBuffer",(yyvsp[(3) - (4)].struct_specifier));
		(yyval.type_specifier)->set_location(yylloc);
	}
    break;

  case 196:

/* Line 1806 of yacc.c  */
#line 1409 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.type_specifier) = new(ctx) ast_type_specifier("StructuredBuffer",(yyvsp[(3) - (4)].identifier));
		(yyval.type_specifier)->set_location(yylloc);
	}
    break;

  case 197:

/* Line 1806 of yacc.c  */
#line 1415 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.type_specifier) = new(ctx) ast_type_specifier("RWStructuredBuffer",(yyvsp[(3) - (4)].identifier));
		(yyval.type_specifier)->set_location(yylloc);
	}
    break;

  case 198:

/* Line 1806 of yacc.c  */
#line 1421 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.type_specifier) = new(ctx) ast_type_specifier("RWStructuredBuffer",(yyvsp[(3) - (4)].struct_specifier));
		(yyval.type_specifier)->set_location(yylloc);
	}
    break;

  case 199:

/* Line 1806 of yacc.c  */
#line 1427 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.type_specifier) = new(ctx) ast_type_specifier("RWStructuredBuffer",(yyvsp[(3) - (4)].identifier));
		(yyval.type_specifier)->set_location(yylloc);
	}
    break;

  case 200:

/* Line 1806 of yacc.c  */
#line 1433 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.type_specifier) = new(ctx) ast_type_specifier((yyvsp[(1) - (4)].identifier),(yyvsp[(3) - (4)].identifier));
		(yyval.type_specifier)->set_location(yylloc);
	}
    break;

  case 201:

/* Line 1806 of yacc.c  */
#line 1439 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.type_specifier) = new(ctx) ast_type_specifier((yyvsp[(1) - (6)].identifier),(yyvsp[(3) - (6)].identifier));
		(yyval.type_specifier)->set_location(yylloc);
		(yyval.type_specifier)->patch_size = (yyvsp[(5) - (6)].n);
	}
    break;

  case 202:

/* Line 1806 of yacc.c  */
#line 1446 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.type_specifier) = new(ctx) ast_type_specifier((yyvsp[(1) - (6)].identifier),(yyvsp[(3) - (6)].identifier));
		(yyval.type_specifier)->set_location(yylloc);
		(yyval.type_specifier)->patch_size = (yyvsp[(5) - (6)].n);
	}
    break;

  case 203:

/* Line 1806 of yacc.c  */
#line 1453 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.type_specifier) = new(ctx) ast_type_specifier((yyvsp[(1) - (1)].struct_specifier));
	   (yyval.type_specifier)->set_location(yylloc);
	}
    break;

  case 204:

/* Line 1806 of yacc.c  */
#line 1459 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.type_specifier) = new(ctx) ast_type_specifier((yyvsp[(1) - (1)].identifier));
	   (yyval.type_specifier)->set_location(yylloc);
	}
    break;

  case 205:

/* Line 1806 of yacc.c  */
#line 1467 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "void"; }
    break;

  case 206:

/* Line 1806 of yacc.c  */
#line 1468 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "float"; }
    break;

  case 207:

/* Line 1806 of yacc.c  */
#line 1469 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "half"; }
    break;

  case 208:

/* Line 1806 of yacc.c  */
#line 1470 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "int"; }
    break;

  case 209:

/* Line 1806 of yacc.c  */
#line 1471 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "uint"; }
    break;

  case 210:

/* Line 1806 of yacc.c  */
#line 1472 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "bool"; }
    break;

  case 211:

/* Line 1806 of yacc.c  */
#line 1473 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "vec2"; }
    break;

  case 212:

/* Line 1806 of yacc.c  */
#line 1474 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "vec3"; }
    break;

  case 213:

/* Line 1806 of yacc.c  */
#line 1475 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "vec4"; }
    break;

  case 214:

/* Line 1806 of yacc.c  */
#line 1476 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "half2"; }
    break;

  case 215:

/* Line 1806 of yacc.c  */
#line 1477 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "half3"; }
    break;

  case 216:

/* Line 1806 of yacc.c  */
#line 1478 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "half4"; }
    break;

  case 217:

/* Line 1806 of yacc.c  */
#line 1479 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "bvec2"; }
    break;

  case 218:

/* Line 1806 of yacc.c  */
#line 1480 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "bvec3"; }
    break;

  case 219:

/* Line 1806 of yacc.c  */
#line 1481 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "bvec4"; }
    break;

  case 220:

/* Line 1806 of yacc.c  */
#line 1482 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "ivec2"; }
    break;

  case 221:

/* Line 1806 of yacc.c  */
#line 1483 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "ivec3"; }
    break;

  case 222:

/* Line 1806 of yacc.c  */
#line 1484 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "ivec4"; }
    break;

  case 223:

/* Line 1806 of yacc.c  */
#line 1485 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "uvec2"; }
    break;

  case 224:

/* Line 1806 of yacc.c  */
#line 1486 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "uvec3"; }
    break;

  case 225:

/* Line 1806 of yacc.c  */
#line 1487 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "uvec4"; }
    break;

  case 226:

/* Line 1806 of yacc.c  */
#line 1488 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "mat2"; }
    break;

  case 227:

/* Line 1806 of yacc.c  */
#line 1489 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "mat2x3"; }
    break;

  case 228:

/* Line 1806 of yacc.c  */
#line 1490 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "mat2x4"; }
    break;

  case 229:

/* Line 1806 of yacc.c  */
#line 1491 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "mat3x2"; }
    break;

  case 230:

/* Line 1806 of yacc.c  */
#line 1492 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "mat3"; }
    break;

  case 231:

/* Line 1806 of yacc.c  */
#line 1493 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "mat3x4"; }
    break;

  case 232:

/* Line 1806 of yacc.c  */
#line 1494 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "mat4x2"; }
    break;

  case 233:

/* Line 1806 of yacc.c  */
#line 1495 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "mat4x3"; }
    break;

  case 234:

/* Line 1806 of yacc.c  */
#line 1496 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "mat4"; }
    break;

  case 235:

/* Line 1806 of yacc.c  */
#line 1497 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "half2x2"; }
    break;

  case 236:

/* Line 1806 of yacc.c  */
#line 1498 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "half2x3"; }
    break;

  case 237:

/* Line 1806 of yacc.c  */
#line 1499 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "half2x4"; }
    break;

  case 238:

/* Line 1806 of yacc.c  */
#line 1500 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "half3x2"; }
    break;

  case 239:

/* Line 1806 of yacc.c  */
#line 1501 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "half3x3"; }
    break;

  case 240:

/* Line 1806 of yacc.c  */
#line 1502 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "half3x4"; }
    break;

  case 241:

/* Line 1806 of yacc.c  */
#line 1503 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "half4x2"; }
    break;

  case 242:

/* Line 1806 of yacc.c  */
#line 1504 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "half4x3"; }
    break;

  case 243:

/* Line 1806 of yacc.c  */
#line 1505 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "half4x4"; }
    break;

  case 244:

/* Line 1806 of yacc.c  */
#line 1506 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "samplerState"; }
    break;

  case 245:

/* Line 1806 of yacc.c  */
#line 1507 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "samplerComparisonState"; }
    break;

  case 246:

/* Line 1806 of yacc.c  */
#line 1542 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "Buffer"; }
    break;

  case 247:

/* Line 1806 of yacc.c  */
#line 1543 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "ByteAddressBuffer"; }
    break;

  case 248:

/* Line 1806 of yacc.c  */
#line 1544 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "Texture1D"; }
    break;

  case 249:

/* Line 1806 of yacc.c  */
#line 1545 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "Texture1DArray"; }
    break;

  case 250:

/* Line 1806 of yacc.c  */
#line 1546 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "Texture2D"; }
    break;

  case 251:

/* Line 1806 of yacc.c  */
#line 1547 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "Texture2DArray"; }
    break;

  case 252:

/* Line 1806 of yacc.c  */
#line 1548 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "Texture2DMS"; }
    break;

  case 253:

/* Line 1806 of yacc.c  */
#line 1549 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "TextureExternal"; }
    break;

  case 254:

/* Line 1806 of yacc.c  */
#line 1550 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "Texture2DMSArray"; }
    break;

  case 255:

/* Line 1806 of yacc.c  */
#line 1551 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "Texture3D"; }
    break;

  case 256:

/* Line 1806 of yacc.c  */
#line 1552 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "TextureCube"; }
    break;

  case 257:

/* Line 1806 of yacc.c  */
#line 1553 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "TextureCubeArray"; }
    break;

  case 258:

/* Line 1806 of yacc.c  */
#line 1554 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "RWBuffer"; }
    break;

  case 259:

/* Line 1806 of yacc.c  */
#line 1555 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "RWByteAddressBuffer"; }
    break;

  case 260:

/* Line 1806 of yacc.c  */
#line 1556 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "RWTexture1D"; }
    break;

  case 261:

/* Line 1806 of yacc.c  */
#line 1557 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "RWTexture1DArray"; }
    break;

  case 262:

/* Line 1806 of yacc.c  */
#line 1558 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "RWTexture2D"; }
    break;

  case 263:

/* Line 1806 of yacc.c  */
#line 1559 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "RWTexture2DArray"; }
    break;

  case 264:

/* Line 1806 of yacc.c  */
#line 1560 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "RWTexture3D"; }
    break;

  case 265:

/* Line 1806 of yacc.c  */
#line 1564 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "PointStream"; }
    break;

  case 266:

/* Line 1806 of yacc.c  */
#line 1565 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "LineStream"; }
    break;

  case 267:

/* Line 1806 of yacc.c  */
#line 1566 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "TriangleStream"; }
    break;

  case 268:

/* Line 1806 of yacc.c  */
#line 1570 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "InputPatch"; }
    break;

  case 269:

/* Line 1806 of yacc.c  */
#line 1574 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "OutputPatch"; }
    break;

  case 270:

/* Line 1806 of yacc.c  */
#line 1579 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.struct_specifier) = new(ctx) ast_struct_specifier((yyvsp[(2) - (5)].identifier), (yyvsp[(4) - (5)].node));
	   (yyval.struct_specifier)->set_location(yylloc);
	   state->symbols->add_type((yyvsp[(2) - (5)].identifier), glsl_type::void_type);
	}
    break;

  case 271:

/* Line 1806 of yacc.c  */
#line 1586 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.struct_specifier) = new(ctx) ast_struct_specifier((yyvsp[(2) - (7)].identifier), (yyvsp[(4) - (7)].identifier), (yyvsp[(6) - (7)].node));
	   (yyval.struct_specifier)->set_location(yylloc);
	   state->symbols->add_type((yyvsp[(2) - (7)].identifier), glsl_type::void_type);
	}
    break;

  case 272:

/* Line 1806 of yacc.c  */
#line 1593 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.struct_specifier) = new(ctx) ast_struct_specifier(NULL, (yyvsp[(3) - (4)].node));
	   (yyval.struct_specifier)->set_location(yylloc);
	}
    break;

  case 273:

/* Line 1806 of yacc.c  */
#line 1599 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.struct_specifier) = new(ctx) ast_struct_specifier((yyvsp[(2) - (4)].identifier),NULL);
		(yyval.struct_specifier)->set_location(yylloc);
		state->symbols->add_type((yyvsp[(2) - (4)].identifier), glsl_type::void_type);
	}
    break;

  case 274:

/* Line 1806 of yacc.c  */
#line 1606 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.struct_specifier) = new(ctx) ast_struct_specifier((yyvsp[(2) - (6)].identifier), (yyvsp[(4) - (6)].identifier), NULL);
		(yyval.struct_specifier)->set_location(yylloc);
		state->symbols->add_type((yyvsp[(2) - (6)].identifier), glsl_type::void_type);
	}
    break;

  case 275:

/* Line 1806 of yacc.c  */
#line 1613 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.struct_specifier) = new(ctx) ast_struct_specifier(NULL,NULL);
		(yyval.struct_specifier)->set_location(yylloc);
	}
    break;

  case 276:

/* Line 1806 of yacc.c  */
#line 1622 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.node) = new(ctx) ast_cbuffer_declaration((yyvsp[(2) - (5)].identifier), (yyvsp[(4) - (5)].node));
	   (yyval.node)->set_location(yylloc);
	}
    break;

  case 277:

/* Line 1806 of yacc.c  */
#line 1628 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		/* Do nothing! */
		(yyval.node) = NULL;
	}
    break;

  case 278:

/* Line 1806 of yacc.c  */
#line 1636 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.node) = (ast_node *) (yyvsp[(1) - (1)].declarator_list);
	   (yyvsp[(1) - (1)].declarator_list)->link.self_link();
	}
    break;

  case 279:

/* Line 1806 of yacc.c  */
#line 1641 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.node) = (ast_node *) (yyvsp[(1) - (2)].node);
	   (yyval.node)->link.insert_before(& (yyvsp[(2) - (2)].declarator_list)->link);
	}
    break;

  case 280:

/* Line 1806 of yacc.c  */
#line 1649 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.declarator_list) = new(ctx) ast_declarator_list((yyvsp[(1) - (3)].fully_specified_type));
	   (yyval.declarator_list)->set_location(yylloc);
	   (yyval.declarator_list)->declarations.push_degenerate_list_at_head(& (yyvsp[(2) - (3)].declaration)->link);
	}
    break;

  case 281:

/* Line 1806 of yacc.c  */
#line 1659 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.fully_specified_type) = new(ctx) ast_fully_specified_type();
		(yyval.fully_specified_type)->set_location(yylloc);
		(yyval.fully_specified_type)->specifier = (yyvsp[(1) - (1)].type_specifier);
	}
    break;

  case 282:

/* Line 1806 of yacc.c  */
#line 1666 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.fully_specified_type) = new(ctx) ast_fully_specified_type();
		(yyval.fully_specified_type)->set_location(yylloc);
		(yyval.fully_specified_type)->specifier = (yyvsp[(2) - (2)].type_specifier);
		(yyval.fully_specified_type)->qualifier = (yyvsp[(1) - (2)].type_qualifier);
	}
    break;

  case 284:

/* Line 1806 of yacc.c  */
#line 1678 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		(yyval.type_qualifier) = (yyvsp[(2) - (2)].type_qualifier);
		(yyval.type_qualifier).flags.q.centroid = 1;
	}
    break;

  case 285:

/* Line 1806 of yacc.c  */
#line 1683 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		(yyval.type_qualifier) = (yyvsp[(1) - (2)].type_qualifier);
		(yyval.type_qualifier).flags.q.centroid = 1;
	}
    break;

  case 286:

/* Line 1806 of yacc.c  */
#line 1688 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
		(yyval.type_qualifier).flags.q.centroid = 1;
	}
    break;

  case 287:

/* Line 1806 of yacc.c  */
#line 1696 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.declaration) = (yyvsp[(1) - (1)].declaration);
	   (yyvsp[(1) - (1)].declaration)->link.self_link();
	}
    break;

  case 288:

/* Line 1806 of yacc.c  */
#line 1701 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.declaration) = (yyvsp[(1) - (3)].declaration);
	   (yyval.declaration)->link.insert_before(& (yyvsp[(3) - (3)].declaration)->link);
	}
    break;

  case 289:

/* Line 1806 of yacc.c  */
#line 1709 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.declaration) = new(ctx) ast_declaration((yyvsp[(1) - (1)].identifier), false, NULL, NULL);
	   (yyval.declaration)->set_location(yylloc);
	   state->symbols->add_variable(new(state) ir_variable(NULL, (yyvsp[(1) - (1)].identifier), ir_var_auto));
	}
    break;

  case 290:

/* Line 1806 of yacc.c  */
#line 1716 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.declaration) = (yyvsp[(1) - (1)].declaration);
	}
    break;

  case 291:

/* Line 1806 of yacc.c  */
#line 1721 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.declaration) = new(ctx) ast_declaration((yyvsp[(1) - (3)].identifier), false, NULL, NULL);
		(yyval.declaration)->set_location(yylloc);
		(yyval.declaration)->semantic = (yyvsp[(3) - (3)].identifier);
		state->symbols->add_variable(new(state) ir_variable(NULL, (yyvsp[(1) - (3)].identifier), ir_var_auto));
	}
    break;

  case 292:

/* Line 1806 of yacc.c  */
#line 1729 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.declaration) = new(ctx) ast_declaration((yyvsp[(1) - (6)].identifier), true, (yyvsp[(3) - (6)].expression), NULL);
	   (yyval.declaration)->set_location(yylloc);
	   (yyval.declaration)->semantic = (yyvsp[(6) - (6)].identifier);
	}
    break;

  case 293:

/* Line 1806 of yacc.c  */
#line 1739 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.declaration) = new(ctx) ast_declaration((yyvsp[(1) - (4)].identifier), true, (yyvsp[(3) - (4)].expression), NULL);
	   (yyval.declaration)->set_location(yylloc);
	}
    break;

  case 294:

/* Line 1806 of yacc.c  */
#line 1745 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.declaration) = (yyvsp[(1) - (4)].declaration);
	   (yyvsp[(3) - (4)].expression)->link.next = &((yyval.declaration)->array_size->link);
	   (yyval.declaration)->array_size = (yyvsp[(3) - (4)].expression);
	}
    break;

  case 296:

/* Line 1806 of yacc.c  */
#line 1755 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.expression) = new(ctx) ast_initializer_list_expression();
		(yyval.expression)->expressions.push_degenerate_list_at_head(& (yyvsp[(2) - (3)].expression)->link);
	}
    break;

  case 297:

/* Line 1806 of yacc.c  */
#line 1764 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		(yyval.expression) = (yyvsp[(1) - (1)].expression);
		(yyval.expression)->link.self_link();
	}
    break;

  case 298:

/* Line 1806 of yacc.c  */
#line 1769 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		(yyval.expression) = (yyvsp[(1) - (3)].expression);
		(yyval.expression)->link.insert_before(& (yyvsp[(3) - (3)].expression)->link);
	}
    break;

  case 299:

/* Line 1806 of yacc.c  */
#line 1774 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		(yyval.expression) = (yyvsp[(1) - (2)].expression);
	}
    break;

  case 301:

/* Line 1806 of yacc.c  */
#line 1786 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.node) = (ast_node *) (yyvsp[(1) - (1)].compound_statement); }
    break;

  case 303:

/* Line 1806 of yacc.c  */
#line 1789 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		(yyval.node) = (ast_node *) (yyvsp[(2) - (2)].compound_statement);
		(yyval.node)->attributes.push_degenerate_list_at_head( & (yyvsp[(1) - (2)].attribute)->link);
	}
    break;

  case 304:

/* Line 1806 of yacc.c  */
#line 1794 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		(yyval.node) = (yyvsp[(2) - (2)].node);
		(yyval.node)->attributes.push_degenerate_list_at_head( & (yyvsp[(1) - (2)].attribute)->link);
	}
    break;

  case 311:

/* Line 1806 of yacc.c  */
#line 1811 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.compound_statement) = new(ctx) ast_compound_statement(true, NULL);
	   (yyval.compound_statement)->set_location(yylloc);
	}
    break;

  case 312:

/* Line 1806 of yacc.c  */
#line 1817 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   state->symbols->push_scope();
	}
    break;

  case 313:

/* Line 1806 of yacc.c  */
#line 1821 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.compound_statement) = new(ctx) ast_compound_statement(true, (yyvsp[(3) - (4)].node));
	   (yyval.compound_statement)->set_location(yylloc);
	   state->symbols->pop_scope();
	}
    break;

  case 314:

/* Line 1806 of yacc.c  */
#line 1830 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.node) = (ast_node *) (yyvsp[(1) - (1)].compound_statement); }
    break;

  case 316:

/* Line 1806 of yacc.c  */
#line 1836 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.compound_statement) = new(ctx) ast_compound_statement(false, NULL);
	   (yyval.compound_statement)->set_location(yylloc);
	}
    break;

  case 317:

/* Line 1806 of yacc.c  */
#line 1842 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.compound_statement) = new(ctx) ast_compound_statement(false, (yyvsp[(2) - (3)].node));
	   (yyval.compound_statement)->set_location(yylloc);
	}
    break;

  case 318:

/* Line 1806 of yacc.c  */
#line 1851 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   if ((yyvsp[(1) - (1)].node) == NULL) {
		  _mesa_glsl_error(& (yylsp[(1) - (1)]), state, "<nil> statement\n");
		  check((yyvsp[(1) - (1)].node) != NULL);
	   }

	   (yyval.node) = (yyvsp[(1) - (1)].node);
	   (yyval.node)->link.self_link();
	}
    break;

  case 319:

/* Line 1806 of yacc.c  */
#line 1861 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   if ((yyvsp[(2) - (2)].node) == NULL) {
		  _mesa_glsl_error(& (yylsp[(2) - (2)]), state, "<nil> statement\n");
		  check((yyvsp[(2) - (2)].node) != NULL);
	   }
	   (yyval.node) = (yyvsp[(1) - (2)].node);
	   (yyval.node)->link.insert_before(& (yyvsp[(2) - (2)].node)->link);
	}
    break;

  case 320:

/* Line 1806 of yacc.c  */
#line 1873 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.node) = new(ctx) ast_expression_statement(NULL);
	   (yyval.node)->set_location(yylloc);
	}
    break;

  case 321:

/* Line 1806 of yacc.c  */
#line 1879 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.node) = new(ctx) ast_expression_statement((yyvsp[(1) - (2)].expression));
	   (yyval.node)->set_location(yylloc);
	}
    break;

  case 322:

/* Line 1806 of yacc.c  */
#line 1888 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.node) = new(state) ast_selection_statement((yyvsp[(3) - (5)].expression), (yyvsp[(5) - (5)].selection_rest_statement).then_statement,
						   (yyvsp[(5) - (5)].selection_rest_statement).else_statement);
	   (yyval.node)->set_location(yylloc);
	}
    break;

  case 323:

/* Line 1806 of yacc.c  */
#line 1897 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.selection_rest_statement).then_statement = (yyvsp[(1) - (3)].node);
	   (yyval.selection_rest_statement).else_statement = (yyvsp[(3) - (3)].node);
	}
    break;

  case 324:

/* Line 1806 of yacc.c  */
#line 1902 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.selection_rest_statement).then_statement = (yyvsp[(1) - (1)].node);
	   (yyval.selection_rest_statement).else_statement = NULL;
	}
    break;

  case 325:

/* Line 1806 of yacc.c  */
#line 1910 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.node) = (ast_node *) (yyvsp[(1) - (1)].expression);
	}
    break;

  case 326:

/* Line 1806 of yacc.c  */
#line 1914 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   ast_declaration *decl = new(ctx) ast_declaration((yyvsp[(2) - (4)].identifier), false, NULL, (yyvsp[(4) - (4)].expression));
	   ast_declarator_list *declarator = new(ctx) ast_declarator_list((yyvsp[(1) - (4)].fully_specified_type));
	   decl->set_location(yylloc);
	   declarator->set_location(yylloc);

	   declarator->declarations.push_tail(&decl->link);
	   (yyval.node) = declarator;
	}
    break;

  case 327:

/* Line 1806 of yacc.c  */
#line 1932 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.node) = new(state) ast_switch_statement((yyvsp[(3) - (5)].expression), (yyvsp[(5) - (5)].switch_body));
	   (yyval.node)->set_location(yylloc);
	}
    break;

  case 328:

/* Line 1806 of yacc.c  */
#line 1940 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.switch_body) = new(state) ast_switch_body(NULL);
	   (yyval.switch_body)->set_location(yylloc);
	}
    break;

  case 329:

/* Line 1806 of yacc.c  */
#line 1945 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.switch_body) = new(state) ast_switch_body((yyvsp[(2) - (3)].case_statement_list));
	   (yyval.switch_body)->set_location(yylloc);
	}
    break;

  case 330:

/* Line 1806 of yacc.c  */
#line 1953 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.case_label) = new(state) ast_case_label((yyvsp[(2) - (3)].expression));
	   (yyval.case_label)->set_location(yylloc);
	}
    break;

  case 331:

/* Line 1806 of yacc.c  */
#line 1958 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.case_label) = new(state) ast_case_label(NULL);
	   (yyval.case_label)->set_location(yylloc);
	}
    break;

  case 332:

/* Line 1806 of yacc.c  */
#line 1966 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   ast_case_label_list *labels = new(state) ast_case_label_list();

	   labels->labels.push_tail(& (yyvsp[(1) - (1)].case_label)->link);
	   (yyval.case_label_list) = labels;
	   (yyval.case_label_list)->set_location(yylloc);
	}
    break;

  case 333:

/* Line 1806 of yacc.c  */
#line 1974 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.case_label_list) = (yyvsp[(1) - (2)].case_label_list);
	   (yyval.case_label_list)->labels.push_tail(& (yyvsp[(2) - (2)].case_label)->link);
	}
    break;

  case 334:

/* Line 1806 of yacc.c  */
#line 1982 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   ast_case_statement *stmts = new(state) ast_case_statement((yyvsp[(1) - (2)].case_label_list));
	   stmts->set_location(yylloc);

	   stmts->stmts.push_tail(& (yyvsp[(2) - (2)].node)->link);
	   (yyval.case_statement) = stmts;
	}
    break;

  case 335:

/* Line 1806 of yacc.c  */
#line 1990 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.case_statement) = (yyvsp[(1) - (2)].case_statement);
	   (yyval.case_statement)->stmts.push_tail(& (yyvsp[(2) - (2)].node)->link);
	}
    break;

  case 336:

/* Line 1806 of yacc.c  */
#line 1998 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   ast_case_statement_list *cases= new(state) ast_case_statement_list();
	   cases->set_location(yylloc);

	   cases->cases.push_tail(& (yyvsp[(1) - (1)].case_statement)->link);
	   (yyval.case_statement_list) = cases;
	}
    break;

  case 337:

/* Line 1806 of yacc.c  */
#line 2006 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.case_statement_list) = (yyvsp[(1) - (2)].case_statement_list);
	   (yyval.case_statement_list)->cases.push_tail(& (yyvsp[(2) - (2)].case_statement)->link);
	}
    break;

  case 338:

/* Line 1806 of yacc.c  */
#line 2014 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.node) = new(ctx) ast_iteration_statement(ast_iteration_statement::ast_while,
							NULL, (yyvsp[(3) - (5)].node), NULL, (yyvsp[(5) - (5)].node));
	   (yyval.node)->set_location(yylloc);
	}
    break;

  case 339:

/* Line 1806 of yacc.c  */
#line 2021 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.node) = new(ctx) ast_iteration_statement(ast_iteration_statement::ast_do_while,
							NULL, (yyvsp[(5) - (7)].expression), NULL, (yyvsp[(2) - (7)].node));
	   (yyval.node)->set_location(yylloc);
	}
    break;

  case 340:

/* Line 1806 of yacc.c  */
#line 2028 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.node) = new(ctx) ast_iteration_statement(ast_iteration_statement::ast_for,
							(yyvsp[(3) - (6)].node), (yyvsp[(4) - (6)].for_rest_statement).cond, (yyvsp[(4) - (6)].for_rest_statement).rest, (yyvsp[(6) - (6)].node));
	   (yyval.node)->set_location(yylloc);
	}
    break;

  case 344:

/* Line 1806 of yacc.c  */
#line 2044 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.node) = NULL;
	}
    break;

  case 345:

/* Line 1806 of yacc.c  */
#line 2051 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.for_rest_statement).cond = (yyvsp[(1) - (2)].node);
	   (yyval.for_rest_statement).rest = NULL;
	}
    break;

  case 346:

/* Line 1806 of yacc.c  */
#line 2056 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.for_rest_statement).cond = (yyvsp[(1) - (3)].node);
	   (yyval.for_rest_statement).rest = (yyvsp[(3) - (3)].expression);
	}
    break;

  case 347:

/* Line 1806 of yacc.c  */
#line 2065 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.node) = new(ctx) ast_jump_statement(ast_jump_statement::ast_continue, NULL);
	   (yyval.node)->set_location(yylloc);
	}
    break;

  case 348:

/* Line 1806 of yacc.c  */
#line 2071 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.node) = new(ctx) ast_jump_statement(ast_jump_statement::ast_break, NULL);
	   (yyval.node)->set_location(yylloc);
	}
    break;

  case 349:

/* Line 1806 of yacc.c  */
#line 2077 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.node) = new(ctx) ast_jump_statement(ast_jump_statement::ast_return, NULL);
	   (yyval.node)->set_location(yylloc);
	}
    break;

  case 350:

/* Line 1806 of yacc.c  */
#line 2083 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.node) = new(ctx) ast_jump_statement(ast_jump_statement::ast_return, (yyvsp[(2) - (3)].expression));
	   (yyval.node)->set_location(yylloc);
	}
    break;

  case 351:

/* Line 1806 of yacc.c  */
#line 2089 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.node) = new(ctx) ast_jump_statement(ast_jump_statement::ast_discard, NULL);
	   (yyval.node)->set_location(yylloc);
	}
    break;

  case 352:

/* Line 1806 of yacc.c  */
#line 2097 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.node) = (yyvsp[(1) - (1)].function_definition); }
    break;

  case 353:

/* Line 1806 of yacc.c  */
#line 2098 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.node) = (yyvsp[(1) - (1)].node); }
    break;

  case 354:

/* Line 1806 of yacc.c  */
#line 2103 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.attribute_arg) = new(ctx) ast_attribute_arg( (yyvsp[(1) - (1)].expression) );
		(yyval.attribute_arg)->link.self_link();
	}
    break;

  case 355:

/* Line 1806 of yacc.c  */
#line 2109 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.attribute_arg) = new(ctx) ast_attribute_arg( (yyvsp[(1) - (1)].string_literal) );
		(yyval.attribute_arg)->link.self_link();
	}
    break;

  case 356:

/* Line 1806 of yacc.c  */
#line 2118 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		(yyval.attribute_arg) = (yyvsp[(1) - (1)].attribute_arg);
	}
    break;

  case 357:

/* Line 1806 of yacc.c  */
#line 2122 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		(yyval.attribute_arg) = (yyvsp[(1) - (3)].attribute_arg);
		(yyval.attribute_arg)->link.insert_before( & (yyvsp[(3) - (3)].attribute_arg)->link);
	}
    break;

  case 358:

/* Line 1806 of yacc.c  */
#line 2130 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.attribute) = new(ctx) ast_attribute( (yyvsp[(2) - (3)].identifier) );
		(yyval.attribute)->link.self_link();
	}
    break;

  case 359:

/* Line 1806 of yacc.c  */
#line 2136 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.attribute) = new(ctx) ast_attribute( (yyvsp[(2) - (3)].identifier) );
		(yyval.attribute)->link.self_link();
	}
    break;

  case 360:

/* Line 1806 of yacc.c  */
#line 2142 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.attribute) = new(ctx) ast_attribute( (yyvsp[(2) - (6)].identifier) );
		(yyval.attribute)->link.self_link();
		(yyval.attribute)->arguments.push_degenerate_list_at_head( & (yyvsp[(4) - (6)].attribute_arg)->link );
	}
    break;

  case 361:

/* Line 1806 of yacc.c  */
#line 2149 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.attribute) = new(ctx) ast_attribute( (yyvsp[(2) - (6)].identifier) );
		(yyval.attribute)->link.self_link();
		(yyval.attribute)->arguments.push_degenerate_list_at_head( & (yyvsp[(4) - (6)].attribute_arg)->link );
	}
    break;

  case 362:

/* Line 1806 of yacc.c  */
#line 2159 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		(yyval.attribute) = (yyvsp[(1) - (2)].attribute);
		(yyval.attribute)->link.insert_before( & (yyvsp[(2) - (2)].attribute)->link);
	}
    break;

  case 363:

/* Line 1806 of yacc.c  */
#line 2164 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		(yyval.attribute) = (yyvsp[(1) - (1)].attribute);
	}
    break;

  case 364:

/* Line 1806 of yacc.c  */
#line 2171 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.function_definition) = new(ctx) ast_function_definition();
	   (yyval.function_definition)->set_location(yylloc);
	   (yyval.function_definition)->prototype = (yyvsp[(1) - (2)].function);
	   (yyval.function_definition)->body = (yyvsp[(2) - (2)].compound_statement);

	   state->symbols->pop_scope();
	}
    break;

  case 365:

/* Line 1806 of yacc.c  */
#line 2181 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.function_definition) = new(ctx) ast_function_definition();
	   (yyval.function_definition)->set_location(yylloc);
	   (yyval.function_definition)->prototype = (yyvsp[(2) - (3)].function);
	   (yyval.function_definition)->body = (yyvsp[(3) - (3)].compound_statement);
	   (yyval.function_definition)->attributes.push_degenerate_list_at_head( & (yyvsp[(1) - (3)].attribute)->link);

	   state->symbols->pop_scope();
	}
    break;

  case 366:

/* Line 1806 of yacc.c  */
#line 2195 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		(yyval.node) = (yyvsp[(1) - (1)].function);
	}
    break;

  case 367:

/* Line 1806 of yacc.c  */
#line 2199 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		if ((yyvsp[(1) - (2)].declarator_list)->type->qualifier.flags.q.is_static == 0 && (yyvsp[(1) - (2)].declarator_list)->type->qualifier.flags.q.shared == 0)
		{
			(yyvsp[(1) - (2)].declarator_list)->type->qualifier.flags.q.uniform = 1;
		}
		(yyval.node) = (yyvsp[(1) - (2)].declarator_list);
	}
    break;

  case 368:

/* Line 1806 of yacc.c  */
#line 2207 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.node) = (yyvsp[(1) - (1)].node);
	}
    break;



/* Line 1806 of yacc.c  */
#line 6712 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.inl"
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
  *++yylsp = yyloc;

  /* Now `shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*------------------------------------.
| yyerrlab -- here on detecting error |
`------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYEMPTY : YYTRANSLATE (yychar);

  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (&yylloc, state, YY_("syntax error"));
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
        yyerror (&yylloc, state, yymsgp);
        if (yysyntax_error_status == 2)
          goto yyexhaustedlab;
      }
# undef YYSYNTAX_ERROR
#endif
    }

  yyerror_range[1] = yylloc;

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
		      yytoken, &yylval, &yylloc, state);
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

  yyerror_range[1] = yylsp[1-yylen];
  /* Do not reclaim the symbols of the rule which action triggered
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
  yyerrstatus = 3;	/* Each real token shifted decrements this.  */

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

      yyerror_range[1] = *yylsp;
      yydestruct ("Error: popping",
		  yystos[yystate], yyvsp, yylsp, state);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  *++yyvsp = yylval;

  yyerror_range[2] = yylloc;
  /* Using YYLLOC is tempting, but would change the location of
     the lookahead.  YYLOC is available though.  */
  YYLLOC_DEFAULT (yyloc, yyerror_range, 2);
  *++yylsp = yyloc;

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

#if !defined(yyoverflow) || YYERROR_VERBOSE
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (&yylloc, state, YY_("memory exhausted"));
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
                  yytoken, &yylval, &yylloc, state);
    }
  /* Do not reclaim the symbols of the rule which action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
		  yystos[*yyssp], yyvsp, yylsp, state);
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
  /* Make sure YYID is used.  */
  return YYID (yyresult);
}



