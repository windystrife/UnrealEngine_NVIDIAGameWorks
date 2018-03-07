// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

// This code is modified from that in the Mesa3D Graphics library available at
// http://mesa3d.org/
// The license for the original code follows:

/*
* Copyright © 2010 Intel Corporation
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

#pragma once
#ifndef GLSL_PARSER_EXTRAS_H
#define GLSL_PARSER_EXTRAS_H

/*
* Most of the definitions here only apply to C++
*/
#ifdef __cplusplus


#include <map>
#include <list>
#include <set>
#include <limits>

#include "glsl_symbol_table.h"

typedef std::set<ir_variable*> TIRVarSet;
typedef std::map<ir_variable*, TIRVarSet> TIRVarSetMap;
typedef std::set<std::string> TStringSet;
typedef std::map<std::string, TStringSet> TStringToSetMap;

struct glsl_uniform_block
{
	const char* name;
	unsigned num_vars;
	class ir_variable* vars[1];

	static glsl_uniform_block* alloc(void* mem_ctx, unsigned num)
	{
		size_t block_size = sizeof(glsl_uniform_block)+ (num - 1) * sizeof(ir_variable*);
		glsl_uniform_block *block = (glsl_uniform_block*)rzalloc_size(mem_ctx, block_size);
		block->num_vars = num;
		return block;
	}
};


struct SCBufferMember
{
	std::string Name;
	unsigned OffsetInFloats;
	unsigned SizeInFloats;
	unsigned NumColumns;
	unsigned NumRows;
	unsigned NumArrayElements;

	ir_variable* Var;
};

typedef std::list<SCBufferMember> TCBufferMembers;

struct SCBuffer
{
	std::string Name;
	TCBufferMembers Members;

	void AddMember(const struct glsl_type * field_type, ir_variable* var);

	static void CalculateMemberInfo(unsigned& SizeInFloats, unsigned& StartOffset, const struct glsl_type * field_type, unsigned* OutLastRowElement = NULL);

	TCBufferMembers::iterator Find(ir_variable* Var)
	{
		for (TCBufferMembers::iterator Iter = Members.begin(); Iter != Members.end(); ++Iter)
		{
			if (Iter->Var == Var)
			{
				return Iter;
			}
		}

		return Members.end();
	}

	bool Contains(ir_variable* Var)
	{
		return Find(Var) != Members.end();
	}
};

typedef std::list<SCBuffer> TCBuffers;

enum EArrayType
{
	EArrayType_FloatHighp = 'h',
	EArrayType_FloatMediump = 'm',
	EArrayType_FloatLowp = 'l',
	EArrayType_Int = 'i',
	EArrayType_UInt = 'u',
	EArrayType_Sampler = 's',
	EArrayType_Image = 'g',

	// Match with OGL_PACKED_TYPEINDEX_*!
	EArrayType_Index_FloatHighp = 0,
	EArrayType_Index_FloatMediump = 1,
	EArrayType_Index_FloatLowp = 2,
	EArrayType_Index_Int = 3,
	EArrayType_Index_UInt = 4,
	EArrayType_Index_Sampler = 5,
	EArrayType_Index_Image = 6,
};

static inline int ConvertArrayTypeToIndex(EArrayType Type)
{
	switch (Type)
	{
	case EArrayType_FloatHighp:
		return EArrayType_Index_FloatHighp;
	case EArrayType_FloatMediump:
		return EArrayType_Index_FloatMediump;
	case EArrayType_FloatLowp:
		return EArrayType_Index_FloatLowp;
	case EArrayType_Int:
		return EArrayType_Index_Int;
	case EArrayType_UInt:
		return EArrayType_Index_UInt;
	case EArrayType_Sampler:
		return EArrayType_Index_Sampler;
	case EArrayType_Image:
		return EArrayType_Index_Image;
	default:
		break;
	}

	return EArrayType_Index_FloatHighp;
}

static inline int GetArrayCharFromPrecisionType(glsl_base_type Type, bool bAssertIfNotFound)
{
	switch (Type)
	{
	case GLSL_TYPE_FLOAT:
		return EArrayType_FloatHighp;

	case GLSL_TYPE_HALF:
		return EArrayType_FloatMediump;

	case GLSL_TYPE_INT:
		return EArrayType_Int;

	case GLSL_TYPE_UINT:
	case GLSL_TYPE_BOOL:
		return EArrayType_UInt;

	case GLSL_TYPE_SAMPLER:
		return EArrayType_Sampler;

	case GLSL_TYPE_IMAGE:
		return EArrayType_Image;

	default:
		break;
	}

	if (bAssertIfNotFound)
	{
		//check(0);
	}

	return 0;
}

// Naming convention::
//	_vu_h: Global Vertex Highp Float
//	_vu_m: Global Vertex Mediump Float
//	_vu_l: Global Vertex Lowp Float
//	_vu_i: Global Vertex Int
//	_vu_u: Global Vertex Unit & Bool
//	_vs0: Global Sampler 0
//	_vs1: Global Sampler 1
struct glsl_packed_uniform
{
	std::string Name;
	unsigned offset;
	unsigned num_components;

	// Extra information for Uniforms coming from CBuffers
	std::string CB_PackedSampler;	// CB name or the name of the packed sampler
	unsigned OffsetIntoCBufferInFloats;
	unsigned SizeInFloats;

	glsl_packed_uniform() :
		Name("<INVALID>"), offset(0), num_components(0), CB_PackedSampler(""), OffsetIntoCBufferInFloats(0), SizeInFloats(0)
	{
	}
};

enum _mesa_glsl_parser_targets
{
	vertex_shader,
	geometry_shader,
	fragment_shader,
	tessellation_control_shader,
	tessellation_evaluation_shader,
	compute_shader
};

inline const char* glsl_variable_tag_from_parser_target(const _mesa_glsl_parser_targets target)
{
	switch (target)
	{
	case vertex_shader: return "v";
	case geometry_shader: return "g";
	case fragment_shader: return "p"; // the engine expects 'p' instead of 'f'
	case tessellation_control_shader: return "h";    // to avoid collision with 'c'ompute, using DX 'h'ull shader
	case tessellation_evaluation_shader: return "d"; // to avoid collision with 'c'ompute, using DX 'd'omain shader
	case compute_shader: return "c";
	default:/*check(false);*/ return "invalid_parser_target";
	}
}

struct gl_context;

struct glsl_switch_state
{
	/** Temporary variables needed for switch statement. */
	ir_variable *test_var;
	ir_variable *is_fallthru_var;
	ir_variable *is_break_var;
	class ast_switch_statement *switch_nesting_ast;

	/** Table of constant values already used in case labels */
	struct hash_table *labels_ht;
	class ast_case_label *previous_default;

	bool is_switch_innermost; // if switch stmt is closest to break, ...

	glsl_switch_state() :
		test_var(nullptr),
		is_fallthru_var(nullptr),
		is_break_var(nullptr),
		switch_nesting_ast(nullptr),
		labels_ht(nullptr),
		previous_default(nullptr),
		is_switch_innermost(false)
	{
	}
};


struct _mesa_glsl_parse_state
{
	_mesa_glsl_parse_state(
		void *mem_ctx,
		_mesa_glsl_parser_targets target,
		struct ILanguageSpec* InBackendLanguage,
		int glsl_version);

	/* Callers of this ralloc-based new need not call delete. It's
	* easier to just ralloc_free 'ctx' (or any of its ancestors). */
	static void* operator new(size_t size, void *ctx)
	{
		void *mem = rzalloc_size(ctx, size);
		//check(mem != NULL);

		return mem;
	}

	/* If the user *does* call delete, that's OK, we will just
	* ralloc_free in that case. */
	static void operator delete(void *mem)
	{
		ralloc_free(mem);
	}

	bool AddUserStruct(const glsl_type* Type);

	void *scanner;
	const char *base_source_file;
	const char *current_source_file;
	exec_list translation_unit;
	glsl_symbol_table *symbols;

	struct ILanguageSpec* LanguageSpec;
	bool bFlattenUniformBuffers;
	bool bGenerateES;		// Did we requested to compile FOR ES
	bool bSeparateShaderObjects;
	
	unsigned language_version;
	enum _mesa_glsl_parser_targets target;

	int maxunrollcount;

	/** Information for geometry shaders, used only if maxvertexcount>0 */
	unsigned maxvertexcount;
	unsigned geometryinput;
	unsigned outputstream_type;

	/** Information for explicitly  assigning locations for input and output variables, similar  to
	what FXC does for D3D, namely assigning "registers" in order of declaration
	*/

	bool bGenerateLayoutLocations;
	unsigned next_in_location_slot;
	unsigned next_out_location_slot;

	glsl_tessellation_info tessellation;

	/**
	* Should we adjust shaders so they invert gl_Position vertically and
	* extend depth of clip space to compensate for difference in coordinate
	* spaces between OpenGL and DX11?
	*/
	bool adjust_clip_space_dx11_to_opengl;

	/**
	* During AST to IR conversion, pointer to current IR function
	*
	* Will be \c NULL whenever the AST to IR conversion is not inside a
	* function definition.
	*/
	class ir_function_signature *current_function;

	/**
	* During AST to IR conversion, pointer to the toplevel IR
	* instruction list being generated.
	*/
	exec_list *toplevel_ir;

	/** Have we found a return statement in this function? */
	bool found_return;

	/** Was there an error during compilation? */
	bool error;

	/**
	* Are all shader inputs / outputs invariant?
	*
	* This is set when the 'STDGL invariant(all)' pragma is used.
	*/
	bool all_invariant;

	/** Loop or switch statement containing the current instructions. */
	class ast_iteration_statement *loop_nesting_ast;

	struct glsl_switch_state switch_state;

	/** List of structures defined in user code. */
	const glsl_type **user_structures;
	unsigned num_user_structures;

	/** List of uniform blocks defined in user code. */
	const glsl_uniform_block **uniform_blocks;
	unsigned num_uniform_blocks;

	/** Packed uniforms. */
	typedef std::list<glsl_packed_uniform> TUniformList;
	typedef std::map<char, TUniformList> TPackedArraysMap;				// char is the type ('h','m','l','i','u',etc)
	typedef std::map<std::string, TPackedArraysMap> TCBPackedArraysMap;	// Uniform Buffer name
	TPackedArraysMap GlobalPackedArraysMap;
	TCBPackedArraysMap CBPackedArraysMap;

	// Holds a mapping from a ir_variable to a uniform buffer member
	TCBuffers CBuffersOriginal;
	TCBuffers CBuffersStructuresFlattened;
	SCBuffer* FindCBufferByName(bool bFlattenStructure, const char* CBName);
	void FindOffsetIntoCBufferInFloats(bool bFlattenStructure, const char* CBName, const char* Member, unsigned& OffsetInCBInFloats, unsigned& SizeInFloats);
	const glsl_packed_uniform* FindPackedSamplerEntry(const char* Name);

	bool has_packed_uniforms;

	char *info_log;

	// Information for extracting sampler/texture names
	TStringToSetMap TextureToSamplerMap;
};

typedef struct YYLTYPE
{
	int first_line;
	int first_column;
	int last_line;
	int last_column;
	const char* source_file;
} YYLTYPE;
# define YYLTYPE_IS_DECLARED 1
# define YYLTYPE_IS_TRIVIAL 1

# define YYLLOC_DEFAULT(Current, Rhs, N)			\
do { \
if (N)							\
{								\
	(Current).first_line = YYRHSLOC(Rhs, 1).first_line;	\
	(Current).first_column = YYRHSLOC(Rhs, 1).first_column;	\
	(Current).last_line = YYRHSLOC(Rhs, N).last_line;	\
	(Current).last_column = YYRHSLOC(Rhs, N).last_column;	\
	(Current).source_file = YYRHSLOC(Rhs, N).source_file; \
}								\
else								\
{								\
	(Current).first_line = (Current).last_line = \
	YYRHSLOC(Rhs, 0).last_line;				\
	(Current).first_column = (Current).last_column = \
	YYRHSLOC(Rhs, 0).last_column;				\
	(Current).source_file = YYRHSLOC(Rhs, 0).source_file; \
}								\
} while (0)

extern void _mesa_glsl_error(YYLTYPE *locp, _mesa_glsl_parse_state *state,
const char *fmt, ...);
extern void _mesa_glsl_error(_mesa_glsl_parse_state *state,
	const char *fmt, ...);

/**
* Emit a warning to the shader log
*
* \sa _mesa_glsl_error
*/
extern void _mesa_glsl_warning(const YYLTYPE *locp,
	_mesa_glsl_parse_state *state,
	const char *fmt, ...);
extern void _mesa_glsl_warning(_mesa_glsl_parse_state *state,
	const char *fmt, ...);

extern void _mesa_glsl_lexer_ctor(struct _mesa_glsl_parse_state *state,
	const char *string);

extern void _mesa_glsl_lexer_dtor(struct _mesa_glsl_parse_state *state);

union YYSTYPE;
extern int _mesa_glsl_lex(union YYSTYPE *yylval, YYLTYPE *yylloc,
	void *scanner);

extern int _mesa_glsl_parse(struct _mesa_glsl_parse_state *);

extern void _mesa_hlsl_lexer_ctor(struct _mesa_glsl_parse_state *state,
	const char *string);

extern void _mesa_hlsl_lexer_dtor(struct _mesa_glsl_parse_state *state);

extern int _mesa_hlsl_lex(union YYSTYPE *yylval, YYLTYPE *yylloc,
	void *scanner);

extern int _mesa_hlsl_parse(struct _mesa_glsl_parse_state *);

/**
* Get the textual name of the specified shader target
*/
extern const char * _mesa_glsl_shader_target_name(enum _mesa_glsl_parser_targets target);


#endif /* __cplusplus */


/*
* These definitions apply to C and C++
*/
extern int preprocess(void *ctx, const char **shader, char **info_log);

#define FRAMEBUFFER_FETCH_ES2	"FramebufferFetchES2"
#define DEPTHBUFFER_FETCH_ES2	"DepthbufferFetchES2"
#define FRAMEBUFFER_FETCH_MRT	"FramebufferFetchMRT"
#define GET_HDR_32BPP_HDR_ENCODE_MODE_ES2 "intrinsic_GetHDR32bppEncodeModeES2"

#endif /* GLSL_PARSER_EXTRAS_H */
