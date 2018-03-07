// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

// This code is modified from that in the Mesa3D Graphics library available at
// http://mesa3d.org/
// The license for the original code follows:

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
//@todo-rco: Remove STL!
#include <stdarg.h>

#include "ralloc.h"
#include "ast.h"
#include "glsl_parser_extras.h"
#include "hlsl_parser.h"
#include "ir_optimization.h"
#include "loop_analysis.h"
#include "IRDump.h"
#include "LanguageSpec.h"

/* TODO: Move this in to _mesa_glsl_parse_state. */
static unsigned int g_anon_struct_count = 0;

_mesa_glsl_parse_state::_mesa_glsl_parse_state(void *mem_ctx, _mesa_glsl_parser_targets InTarget, ILanguageSpec* InLanguageSpec, int glsl_version) :
	scanner(nullptr),
	base_source_file(nullptr),
	current_source_file(nullptr),
	LanguageSpec(InLanguageSpec),
	bFlattenUniformBuffers(false),
	bGenerateES(false),
	bSeparateShaderObjects(false),
	language_version(glsl_version),
	target(InTarget),
	maxunrollcount(32),
	maxvertexcount(0),
	geometryinput(0),
	outputstream_type(0),
	bGenerateLayoutLocations(false),
	next_in_location_slot(0),
	next_out_location_slot(0),
	adjust_clip_space_dx11_to_opengl(false),
	current_function(nullptr),
	toplevel_ir(nullptr),
	found_return(false),
	error(false),
	all_invariant(false),
	loop_nesting_ast(nullptr),
	user_structures(nullptr),
	num_user_structures(0),
	uniform_blocks(nullptr),
	num_uniform_blocks(0),
	has_packed_uniforms(false)
{
	check(InLanguageSpec);

	this->translation_unit.make_empty();
	this->symbols = new(mem_ctx)glsl_symbol_table;
	this->info_log = ralloc_strdup(mem_ctx, "");

	/* Reset the anonymous struct count. */
	g_anon_struct_count = 0;
}

void _mesa_glsl_parse_state::FindOffsetIntoCBufferInFloats(bool bFlattenStructure, const char* CBName, const char* Member, unsigned& OffsetInCBInFloats, unsigned& SizeInFloats)
{
	check(Member && *Member);

	SCBuffer* CB = FindCBufferByName(bFlattenStructure, CBName);
	check(CB);
	for (TCBufferMembers::iterator Iter = CB->Members.begin(); Iter != CB->Members.end(); ++Iter)
	{
		SCBufferMember& CBMember = *Iter;
		if (CBMember.Name == Member)
		{
			OffsetInCBInFloats = CBMember.OffsetInFloats;
			SizeInFloats = CBMember.SizeInFloats;
			return;
		}
	}

	check(0);
}

SCBuffer* _mesa_glsl_parse_state::FindCBufferByName(bool bFlattenStructure, const char* CBName)
{
	check(CBName && *CBName);
	TCBuffers& CBuffers = bFlattenStructure ? CBuffersStructuresFlattened : CBuffersOriginal;
	for (TCBuffers::iterator IterCB = CBuffers.begin(); IterCB != CBuffers.end(); ++IterCB)
	{
		SCBuffer& CBuffer = *IterCB;
		if (CBuffer.Name == CBName)
		{
			return &CBuffer;
			break;
		}
	}

	return NULL;
}

const glsl_packed_uniform* _mesa_glsl_parse_state::FindPackedSamplerEntry(const char* Name)
{
	for (auto& Entry : GlobalPackedArraysMap[EArrayType_Sampler])
	{
		if (Entry.CB_PackedSampler == Name)
		{
			return &Entry;
		}
	}

	return nullptr;
}

bool _mesa_glsl_parse_state::AddUserStruct(const glsl_type* Type)
{
	if (!symbols->add_type(Type->name, Type))
	{
		return false;
		//YYLTYPE loc = { 0 };
		//_mesa_glsl_error(&loc, this, "struct '%s' previously defined", Type->name);
	}

	check(Type->is_record());
	const glsl_type **NewList = reralloc(this, user_structures, const glsl_type *, num_user_structures + 1);
	check(NewList);
	NewList[num_user_structures] = Type;
	user_structures = NewList;
	num_user_structures++;
	return true;
}

const char* _mesa_glsl_shader_target_name(enum _mesa_glsl_parser_targets target)
{
	switch (target)
	{
	case vertex_shader:   return "vertex";
	case fragment_shader: return "fragment";
	case geometry_shader: return "geometry";
	case compute_shader: return "compute";
	case tessellation_control_shader: return "tessellation control";
	case tessellation_evaluation_shader: return "tessellation evaluation";
	}

	check(!"Should not get here.");
	return "unknown";
}


void _mesa_glsl_error_v(YYLTYPE *locp, _mesa_glsl_parse_state *state, const char *fmt, va_list ap)
{
	YYLTYPE dummy_loc = {0};
	locp = locp ? locp : &dummy_loc;
	state->error = true;

	check(state->info_log != NULL);
	ralloc_asprintf_append(&state->info_log, "%s(%u): error: ",
		locp->source_file ? locp->source_file : state->base_source_file,
		locp->first_line);
	ralloc_vasprintf_append(&state->info_log, fmt, ap);
	ralloc_strcat(&state->info_log, "\n");
}


void _mesa_glsl_error(YYLTYPE *locp, _mesa_glsl_parse_state *state, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	_mesa_glsl_error_v(locp, state, fmt, ap);
	va_end(ap);
}


void _mesa_glsl_error(_mesa_glsl_parse_state *state, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	_mesa_glsl_error_v(NULL, state, fmt, ap);
	va_end(ap);
}


void _mesa_glsl_warning_v(const YYLTYPE *locp, _mesa_glsl_parse_state *state, const char *fmt, va_list ap)
{
	YYLTYPE dummy_loc = {0};
	locp = locp ? locp : &dummy_loc;

	check(state->info_log != NULL);
	ralloc_asprintf_append(&state->info_log, "%s(%u): warning: ",
		locp->source_file ? locp->source_file : state->base_source_file,
		locp->first_line);
	ralloc_vasprintf_append(&state->info_log, fmt, ap);
	ralloc_strcat(&state->info_log, "\n");
}


void _mesa_glsl_warning(const YYLTYPE *locp, _mesa_glsl_parse_state *state, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	_mesa_glsl_warning_v(locp, state, fmt, ap);
	va_end(ap);
}


void _mesa_glsl_warning(_mesa_glsl_parse_state *state, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	_mesa_glsl_warning_v(NULL, state, fmt, ap);
	va_end(ap);
}


/**
* Enum representing the possible behaviors that can be specified in
* an #extension directive.
*/
enum ext_behavior {
	extension_disable,
	extension_enable,
	extension_require,
	extension_warn
};

void
_mesa_ast_type_qualifier_print(const struct ast_type_qualifier *q)
{
	if (q->flags.q.constant)
	{
		printf("const ");
	}

	if (q->flags.q.is_static)
	{
		printf("static ");
	}

	if (q->flags.q.invariant)
	{
		printf("invariant ");
	}

	if (q->flags.q.attribute)
	{
		printf("attribute ");
	}

	if (q->flags.q.varying)
	{
		printf("varying ");
	}

	if (q->flags.q.in && q->flags.q.out)
	{
		printf("inout ");
	}
	else
	{
		if (q->flags.q.in)
		{
			printf("in ");
		}

		if (q->flags.q.out)
		{
			printf("out ");
		}
	}

	if (q->flags.q.centroid)
	{
		printf("centroid ");
	}
	if (q->flags.q.uniform)
	{
		printf("uniform ");
	}
	if (q->flags.q.smooth)
	{
		printf("smooth ");
	}
	if (q->flags.q.flat)
	{
		printf("flat ");
	}
	if (q->flags.q.noperspective)
	{
		printf("noperspective ");
	}
}


void
ast_node::print(void) const
{
	printf("unhandled node ");
}


ast_node::ast_node(void)
{
	this->location.source_file = 0;
	this->location.line = 0;
	this->location.column = 0;
}


static void ast_opt_array_size_print(bool is_array, const ast_expression *array_size)
{
	if (is_array && array_size == NULL)
	{
		printf("[]");
	}
	else
	{
		while (array_size)
		{
			printf("[");
			array_size->print();
			printf("]");
			array_size = (const ast_expression*)array_size->link.get_next();
		}
	}
}


void ast_compound_statement::print(void) const
{
	print_attributes();
	printf("{\n");

	foreach_list_const(n, &this->statements)
	{
		ast_node *ast = exec_node_data(ast_node, n, link);
		ast->print();
	}

	printf("}\n");
}


ast_compound_statement::ast_compound_statement(int new_scope, ast_node *statements)
{
	this->new_scope = new_scope;

	if (statements != NULL)
	{
		this->statements.push_degenerate_list_at_head(&statements->link);
	}
}


void ast_expression::print(void) const
{
	print_attributes();
	switch (oper)
	{
	case ast_assign:
	case ast_mul_assign:
	case ast_div_assign:
	case ast_mod_assign:
	case ast_add_assign:
	case ast_sub_assign:
	case ast_ls_assign:
	case ast_rs_assign:
	case ast_and_assign:
	case ast_xor_assign:
	case ast_or_assign:
		subexpressions[0]->print();
		printf("%s ", operator_string(oper));
		subexpressions[1]->print();
		break;

	case ast_field_selection:
		subexpressions[0]->print();
		printf(". %s ", primary_expression.identifier);
		break;

	case ast_plus:
	case ast_neg:
	case ast_bit_not:
	case ast_logic_not:
	case ast_pre_inc:
	case ast_pre_dec:
		printf("%s ", operator_string(oper));
		subexpressions[0]->print();
		break;

	case ast_post_inc:
	case ast_post_dec:
		subexpressions[0]->print();
		printf("%s ", operator_string(oper));
		break;

	case ast_conditional:
		subexpressions[0]->print();
		printf("? ");
		subexpressions[1]->print();
		printf(": ");
		subexpressions[2]->print();
		break;

	case ast_array_index:
		subexpressions[0]->print();
		printf("[ ");
		subexpressions[1]->print();
		printf("] ");
		break;

	case ast_function_call:
	{
		subexpressions[0]->print();
		printf("( ");

		foreach_list_const(n, &this->expressions)
		{
			if (n != this->expressions.get_head())
			{
				printf(", ");
			}

			ast_node *ast = exec_node_data(ast_node, n, link);
			ast->print();
		}

		printf(") ");
		break;
	}

	case ast_identifier:
		printf("%s ", primary_expression.identifier);
		break;

	case ast_int_constant:
		printf("%d ", primary_expression.int_constant);
		break;

	case ast_uint_constant:
		printf("%u ", primary_expression.uint_constant);
		break;

	case ast_float_constant:
		printf("%f ", primary_expression.float_constant);
		break;

	case ast_bool_constant:
		printf("%s ",
			primary_expression.bool_constant
			? "true" : "false");
		break;

	case ast_sequence:
	{
		printf("( ");
		foreach_list_const(n, &this->expressions)
		{
			if (n != this->expressions.get_head())
			{
				printf(", ");
			}

			ast_node *ast = exec_node_data(ast_node, n, link);
			ast->print();
		}
		printf(") ");
		break;
	}

	case ast_type_cast:
	{
		printf("(");
		this->primary_expression.type_specifier->print();
		printf(")");
		this->subexpressions[0]->print();
		break;
	}

	case ast_initializer_list:
	{
		printf("{");
		foreach_list_const(n, &this->expressions)
		{
			if (n != this->expressions.get_head())
			{
				printf(", ");
			}
			ast_node *ast = exec_node_data(ast_node, n, link);
			ast->print();
		}
		printf("}");
	}
		break;

	default:
		check(0);
		break;
	}
}

ast_expression::ast_expression(int oper,
	ast_expression *ex0,
	ast_expression *ex1,
	ast_expression *ex2)
{
	this->oper = ast_operators(oper);
	this->subexpressions[0] = ex0;
	this->subexpressions[1] = ex1;
	this->subexpressions[2] = ex2;
	this->non_lvalue_description = NULL;
}


void
ast_expression_statement::print(void) const
{
	print_attributes();
	if (expression)
	{
		expression->print();
	}

	printf(";\n");
}


ast_expression_statement::ast_expression_statement(ast_expression *ex) :
expression(ex)
{
	/* empty */
}


void ast_function::print(void) const
{
	print_attributes();
	printf("\n");
	return_type->print();
	printf(" %s (", identifier);

	foreach_list_const(n, &this->parameters)
	{
		ast_node *ast = exec_node_data(ast_node, n, link);
		ast->print();
		printf(", ");
	}

	printf(")\n");
}


ast_function::ast_function(void)
: is_definition(false), signature(NULL)
{
	/* empty */
	return_semantic = NULL;
}


void ast_fully_specified_type::print(void) const
{
	_mesa_ast_type_qualifier_print(&qualifier);
	specifier->print();
}


void ast_parameter_declarator::print(void) const
{
	type->print();
	if (identifier)
	{
		printf("%s ", identifier);
	}
	ast_opt_array_size_print(is_array, array_size);
}


void ast_function_definition::print(void) const
{
	print_attributes();
	prototype->print();
	body->print();
}


void ast_declaration::print(void) const
{
	print_attributes();
	printf("%s ", identifier);
	ast_opt_array_size_print(is_array, array_size);

	if (initializer)
	{
		printf("= ");
		initializer->print();
	}
}


ast_declaration::ast_declaration(const char *identifier, int is_array,
	ast_expression *array_size,
	ast_expression *initializer)
{
	this->identifier = identifier;
	this->semantic = NULL;
	this->is_array = is_array;
	this->is_unsized_array = 0;
	this->array_size = array_size;
	this->initializer = initializer;
}


void ast_declarator_list::print(void) const
{
	check(type || invariant);

	print_attributes();

	if (type)
	{
		type->print();
	}
	else
	{
		printf("invariant ");
	}

	foreach_list_const(ptr, &this->declarations)
	{
		if (ptr != this->declarations.get_head())
		{
			printf(", ");
		}

		ast_node *ast = exec_node_data(ast_node, ptr, link);
		ast->print();
	}

	printf(";\n");
}


ast_declarator_list::ast_declarator_list(ast_fully_specified_type *type)
{
	this->type = type;
	this->invariant = false;
}

void ast_jump_statement::print(void) const
{
	print_attributes();

	switch (mode)
	{
	case ast_continue:
		printf("continue;\n");
		break;
	case ast_break:
		printf("break;\n");
		break;
	case ast_return:
		printf("return ");
		if (opt_return_value)
		{
			opt_return_value->print();
		}

		printf(";\n");
		break;
	case ast_discard:
		printf("discard;\n");
		break;
	}
}


ast_jump_statement::ast_jump_statement(int mode, ast_expression *return_value)
{
	this->mode = ast_jump_modes(mode);

	if (mode == ast_return)
	{
		opt_return_value = return_value;
	}
}


void ast_selection_statement::print(void) const
{
	print_attributes();

	printf("if ( ");
	condition->print();
	printf(") ");

	then_statement->print();

	if (else_statement)
	{
		printf("else ");
		else_statement->print();
	}

}


ast_selection_statement::ast_selection_statement(ast_expression *condition,
	ast_node *then_statement,
	ast_node *else_statement)
{
	this->condition = condition;
	this->then_statement = then_statement;
	this->else_statement = else_statement;
}


void ast_switch_statement::print(void) const
{
	print_attributes();

	printf("switch ( ");
	test_expression->print();
	printf(") ");

	body->print();
}


ast_switch_statement::ast_switch_statement(ast_expression *test_expression,
	ast_node *body)
{
	this->test_expression = test_expression;
	this->body = body;
}


void ast_switch_body::print(void) const
{
	print_attributes();

	printf("{\n");
	if (stmts != NULL)
	{
		stmts->print();
	}
	printf("}\n");
}


ast_switch_body::ast_switch_body(ast_case_statement_list *stmts)
{
	this->stmts = stmts;
}


void ast_case_label::print(void) const
{
	print_attributes();

	if (test_value != NULL)
	{
		printf("case ");
		test_value->print();
		printf(": ");
	}
	else
	{
		printf("default: ");
	}
}


ast_case_label::ast_case_label(ast_expression *test_value)
{
	this->test_value = test_value;
}


void ast_case_label_list::print(void) const
{
	foreach_list_const(n, &this->labels)
	{
		ast_node *ast = exec_node_data(ast_node, n, link);
		ast->print();
	}
	printf("\n");
}


ast_case_label_list::ast_case_label_list(void)
{
}


void ast_case_statement::print(void) const
{
	print_attributes();

	labels->print();
	foreach_list_const(n, &this->stmts)
	{
		ast_node *ast = exec_node_data(ast_node, n, link);
		ast->print();
		printf("\n");
	}
}


ast_case_statement::ast_case_statement(ast_case_label_list *labels)
{
	this->labels = labels;
}


void ast_case_statement_list::print(void) const
{
	print_attributes();

	foreach_list_const(n, &this->cases)
	{
		ast_node *ast = exec_node_data(ast_node, n, link);
		ast->print();
	}
}


ast_case_statement_list::ast_case_statement_list(void)
{
}


void ast_iteration_statement::print(void) const
{
	print_attributes();

	switch (mode)
	{
	case ast_for:
		printf("for( ");
		if (init_statement)
		{
			init_statement->print();
		}
		printf("; ");

		if (condition)
		{
			condition->print();
		}
		printf("; ");

		if (rest_expression)
		{
			rest_expression->print();
		}
		printf(") ");

		body->print();
		break;

	case ast_while:
		printf("while ( ");
		if (condition)
		{
			condition->print();
		}
		printf(") ");
		body->print();
		break;

	case ast_do_while:
		printf("do ");
		body->print();
		printf("while ( ");
		if (condition)
		{
			condition->print();
		}
		printf("); ");
		break;
	}
}


ast_iteration_statement::ast_iteration_statement(int mode,
	ast_node *init,
	ast_node *condition,
	ast_expression *rest_expression,
	ast_node *body)
{
	this->mode = ast_iteration_modes(mode);
	this->init_statement = init;
	this->condition = condition;
	this->rest_expression = rest_expression;
	this->body = body;
}


void ast_struct_specifier::print(void) const
{
	print_attributes();

	if (parent_name != NULL)
	{
		printf("struct %s : %s\n{\n", name, parent_name);
	}
	else
	{
		printf("struct %s\n{\n", name);
	}

	foreach_list_const(n, &this->declarations)
	{
		ast_node *ast = exec_node_data(ast_node, n, link);
		ast->print();
	}
	printf("}\n");
}

ast_struct_specifier::ast_struct_specifier(const char *identifier,
	ast_node *declarator_list)
{
	if (identifier == NULL)
	{
		identifier = ralloc_asprintf(this, "anon_struct_%04x", g_anon_struct_count++);
	}
	name = identifier;

	parent_name = NULL;

	if (declarator_list)
	{
		this->declarations.push_degenerate_list_at_head(&declarator_list->link);
	}
}

ast_struct_specifier::ast_struct_specifier(const char *identifier,
	const char *parent,
	ast_node *declarator_list)
{
	if (identifier == NULL)
	{
		identifier = ralloc_asprintf(this, "anon_struct_%04x", g_anon_struct_count++);
	}
	name = identifier;

	parent_name = parent;

	foreach_list_const(n, &this->declarations)
	{
		ast_node *ast = exec_node_data(ast_node, n, link);
		ast->print();
	}

	if (declarator_list)
	{
		this->declarations.push_degenerate_list_at_head(&declarator_list->link);
	}
}

void ast_cbuffer_declaration::print(void) const
{
	print_attributes();

	printf("cbuffer %s\n{ ", name);
	foreach_list_const(n, &this->declarations)
	{
		ast_node *ast = exec_node_data(ast_node, n, link);
		ast->print();
	}
	printf("}\n");
}

ast_cbuffer_declaration::ast_cbuffer_declaration(const char *identifier,
	ast_node *declarator_list)
{
	name = identifier;
	this->declarations.push_degenerate_list_at_head(&declarator_list->link);
}

void ast_attribute_arg::print(void) const
{
	if (is_string)
	{
		printf("\"%s\"", argument.string_argument);
	}
	else
	{
		argument.exp_argument->print();
	}
}

void ast_attribute::print(void) const
{
	printf("[ %s ", attribute_name);
	if (!arguments.is_empty())
	{
		printf("(");
		foreach_list_const(n, &this->arguments)
		{
			ast_node *ast = exec_node_data(ast_node, n, link);
			ast->print();
			printf(",");
		}
		printf(")");
	}
	printf("] ");
}

void ast_node::print_attributes() const
{
	foreach_list_const(n, &this->attributes)
	{
		ast_node *ast = exec_node_data(ast_node, n, link);
		ast->print();
	}
}

bool do_optimization_pass(exec_list *ir, _mesa_glsl_parse_state * state, bool bPerformGlobalDeadCodeRemoval)
{
	bool progress = false;

	progress = lower_instructions(ir, SUB_TO_ADD_NEG) || progress;
	progress = do_function_inlining(ir) || progress;
	progress = do_dead_functions(ir) || progress;
	progress = do_structure_splitting(ir, state) || progress;
	progress = do_if_simplification(ir) || progress;
	progress = do_discard_simplification(ir) || progress;
	progress = do_copy_propagation(ir) || progress;
	progress = do_copy_propagation_elements(ir) || progress;
	if (bPerformGlobalDeadCodeRemoval)
	{
		progress = do_dead_code(ir, false) || progress;
	}
	progress = do_dead_code_local(ir) || progress;
	progress = do_tree_grafting(ir) || progress;
	progress = do_constant_propagation(ir) || progress;

	progress = do_constant_variable(ir) || progress;

	progress = do_constant_folding(ir) || progress;
	progress = do_algebraic(state, ir) || progress;
	if (state && state->LanguageSpec && state->LanguageSpec->SupportsFusedMultiplyAdd())
	{
		progress = lower_instructions(ir, ADD_MUL_TO_FMA) || progress;
	}
	progress = do_lower_jumps(ir) || progress;
	progress = do_vec_index_to_swizzle(ir) || progress;
	progress = do_swizzle_swizzle(ir) || progress;
	progress = do_noop_swizzle(ir) || progress;
	progress = optimize_split_arrays(ir, /*linked=*/ true) || progress;
	progress = optimize_redundant_jumps(ir) || progress;

	int const max_unroll_loop = state->maxunrollcount;
	if (max_unroll_loop > 0)
	{
		loop_state *ls = analyze_loop_variables(ir);
		if (ls->loop_found)
		{
			progress = set_loop_controls(ir, ls) || progress;
			progress = unroll_loops(ir, ls, max_unroll_loop, state) || progress;
		}
		delete ls;
	}

	return progress;
}

void _mesa_ast_print(struct _mesa_glsl_parse_state *state)
{
	foreach_list_typed(ast_node, ast, link, &state->translation_unit)
	{
		ast->print();
	}
}

void SCBuffer::AddMember(const struct glsl_type * field_type, ir_variable* var)
{
	unsigned StartOffset = 0;
	if (!Members.empty())
	{
		const SCBufferMember& Last = Members.back();
		StartOffset = Last.OffsetInFloats + Last.SizeInFloats;
	}

	unsigned SizeInFloats = 0;
	CalculateMemberInfo(SizeInFloats, StartOffset, field_type);

	SCBufferMember CBufferMember;
	CBufferMember.Name = var->name;
	CBufferMember.NumColumns = field_type->vector_elements;
	CBufferMember.NumRows = field_type->matrix_columns;
	CBufferMember.NumArrayElements = field_type->is_array() ? field_type->array_size() : 0;
	CBufferMember.Var = var;
	CBufferMember.SizeInFloats = SizeInFloats;

	CBufferMember.OffsetInFloats = StartOffset;
	Members.push_back(CBufferMember);
	//	printf("%s:\t%d\t%d\n", CBufferMember.Name.c_str(), 4 * StartOffset, 4 * SizeInFloats);
}

void SCBuffer::CalculateMemberInfo(unsigned& SizeInFloats, unsigned& StartOffset, const struct glsl_type * field_type, unsigned* OutLastRowElement)
{
	bool bNewRow = false;
	unsigned LastRowElements = 0;
	if (field_type->is_array())
	{
		unsigned ElementSizeInFloats = 0;
		unsigned Dummy = 0;
		CalculateMemberInfo(ElementSizeInFloats, Dummy, field_type->element_type(), &LastRowElements);
		check(ElementSizeInFloats > 0);
		SizeInFloats = (field_type->array_size() - 1) * ElementSizeInFloats;
		if (field_type->element_type()->is_matrix())
		{
			LastRowElements = 0;
			SizeInFloats += ElementSizeInFloats;
		}
		bNewRow = (StartOffset % 4) != 0;
	}
	else if (field_type->is_matrix())
	{
		SizeInFloats = (field_type->matrix_columns - 1) * 4;
		LastRowElements = field_type->column_type()->vector_elements;
		bNewRow = (StartOffset % 4) != 0;
	}
	else if (field_type->is_record())
	{
		unsigned OriginalStartOffset = StartOffset;
		for (int i = 0; i < field_type->length; ++i)
		{
			unsigned MemberSizeInFloats = 0;
			CalculateMemberInfo(MemberSizeInFloats, StartOffset, field_type->fields.structure[i].type);
			StartOffset += MemberSizeInFloats;
		}

		SizeInFloats = StartOffset - OriginalStartOffset;
	}
	else
	{
		if (field_type->vector_elements == 0)
		{
			return;
		}
		check(field_type->vector_elements > 0);
		LastRowElements = field_type->vector_elements;
	}

	SizeInFloats += LastRowElements;

	if (bNewRow || (StartOffset % 4) + LastRowElements > 4)
	{
		StartOffset = (StartOffset + 4) & ~3u;
	}

	if (OutLastRowElement)
	{
		*OutLastRowElement = LastRowElements;
	}
	//	printf("%d\t%d\n", 4 * StartOffset, 4 * SizeInFloats);
}
