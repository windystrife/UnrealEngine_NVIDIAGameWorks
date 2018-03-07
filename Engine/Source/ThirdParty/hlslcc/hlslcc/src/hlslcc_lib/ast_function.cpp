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

#include "ShaderCompilerCommon.h"
#include "glsl_symbol_table.h"
#include "ast.h"
#include "glsl_types.h"
#include "ir.h"
#include "ir_optimization.h"
#include "ir_function_inlining.h"
#include "macros.h"
#include "LanguageSpec.h"

static ir_rvalue* process_mul(exec_list* instructions, _mesa_glsl_parse_state* state, exec_list* actual_parameters, YYLTYPE* loc)
{
	void* ctx = state;

	ir_rvalue* op[2] = { 0 };
	unsigned num_ops = 0;
	foreach_iter(exec_list_iterator, iter, *actual_parameters)
	{
		check(num_ops < 2);
		ir_instruction* ir = (ir_instruction*)iter.get();
		op[num_ops++] = ir ? ir->as_rvalue() : NULL;
	}

	if (op[0] == NULL || op[1] == NULL)
	{
		return NULL;
	}

	const glsl_type* type0 = op[0]->type;
	const glsl_type* type1 = op[1]->type;

	if (!type0->is_numeric() || !type1->is_numeric())
	{
		return NULL;
	}

	// Promote float * half as some languages can't deal with it; do not promote half * half
	bool bType0IsHalf = (type0->base_type == GLSL_TYPE_HALF);
	bool bType1IsHalf = (type1->base_type == GLSL_TYPE_HALF);
	bool bBothTypesAreHalf = (bType0IsHalf && bType1IsHalf);
	bool bPromoteHalf = state->LanguageSpec->CanConvertBetweenHalfAndFloat() ? false : !bBothTypesAreHalf;
	bool const bNativeMatrixIntrinsics = state->LanguageSpec->SupportsMatrixIntrinsics();
	
	if (!type0->is_float() || (bType0IsHalf && bPromoteHalf))
	{
		op[0] = convert_component(op[0],
			glsl_type::get_instance(GLSL_TYPE_FLOAT,
			type0->vector_elements, type0->matrix_columns));
		type0 = op[0]->type;
	}

	if (!type1->is_float() || (bType1IsHalf && bPromoteHalf))
	{
		op[1] = convert_component(op[1], glsl_type::get_instance(GLSL_TYPE_FLOAT,
			type1->vector_elements, type1->matrix_columns));
		type1 = op[1]->type;
	}

	if (type0->is_scalar() || type1->is_scalar())
	{
		// If either operand is scalar the result is op[0] * op[1].
		const glsl_type* result_type = arithmetic_result_type(op[0], op[1], instructions, state, loc, false);
		if (!result_type->is_error())
		{
			return new(ctx) ir_expression(ir_binop_mul, op[0], op[1]);
		}
	}
	else if (type0->is_vector() && type1->is_vector())
	{
		// If both operands are vectors, the result is just a dot product.
		const glsl_type* result_type = arithmetic_result_type(op[0], op[1], instructions, state, loc, false);
		if (!result_type->is_error())
		{
			return new(ctx) ir_expression(ir_binop_dot, op[0], op[1]);
		}
	}
	else if (type0->is_matrix() && type1->is_vector())
	{
		ir_variable* tmp_mat = new(ctx) ir_variable(type0, NULL, ir_var_temporary);
		instructions->push_tail(tmp_mat);
		instructions->push_tail(new(ctx) ir_assignment(
			new(ctx) ir_dereference_variable(tmp_mat), op[0]));

		// Matrix-vector multiplication treats the vector like a column vector,
		// but in HLSL the matrix is transposed relative to GLSL conventions.
		ir_variable* tmp_vec = new(ctx) ir_variable(type0->column_type(), NULL, ir_var_temporary);
		instructions->push_tail(tmp_vec);

		if (tmp_vec->type->vector_elements > type1->vector_elements)
		{
			// Oddly, HLSL zero-extends the vector in this case. It is the only
			// situation I know of where HLSL implicitly zero-extends instead
			// of truncating.
			ir_constant_data zero_data = { 0 };
			instructions->push_tail(new(ctx) ir_assignment(
				new(ctx) ir_dereference_variable(tmp_vec),
				new(ctx) ir_constant(tmp_vec->type, &zero_data)));
			instructions->push_tail(new(ctx) ir_assignment(
				new(ctx) ir_dereference_variable(tmp_vec),
				op[1], NULL, (1 << type0->vector_elements) - 1));
		}
		else
		{
			// The swizzle here is unnecessary if the # of elements match, but
			// it doesn't hurt and will be optimized out later anyway.
			instructions->push_tail(new(ctx) ir_assignment(
				new(ctx) ir_dereference_variable(tmp_vec),
				new(ctx) ir_swizzle(op[1], 0, 1, 2, 3, tmp_vec->type->vector_elements)));
		}

		ir_variable* tmp_result = new(ctx) ir_variable(type0->row_type(), NULL, ir_var_temporary);
		instructions->push_tail(tmp_result);

		unsigned write_mask = 1;
		for (unsigned c = 0; c < type0->matrix_columns; ++c)
		{
			ir_expression* expr = new(ctx) ir_expression(
				ir_binop_dot,
				new(ctx) ir_dereference_array(tmp_mat, new(ctx) ir_constant(c)),
				new(ctx) ir_dereference_variable(tmp_vec));
			instructions->push_tail(new(ctx) ir_assignment(
				new(ctx) ir_dereference_variable(tmp_result),
				expr, NULL, write_mask));
			write_mask <<= 1;
		}

		return new(ctx) ir_dereference_variable(tmp_result);
	}
	else if (type0->is_vector() && type1->is_matrix())
	{
		// Vector-matrix multiplication treats the vector like a row vector,
		// but in HLSL the matrix is transposed relative to GLSL conventions.
		ir_variable* tmp_vec = new(ctx) ir_variable(type1->row_type(), NULL, ir_var_temporary);
		instructions->push_tail(tmp_vec);
		
		if (tmp_vec->type->vector_elements > type0->vector_elements)
		{
			// Oddly, HLSL zero-extends the vector in this case. It is the only
			// situation I know of where HLSL implicitly zero-extends instead
			// of truncating.
			ir_constant_data zero_data = { 0 };
			instructions->push_tail(new(ctx) ir_assignment(
														   new(ctx) ir_dereference_variable(tmp_vec),
														   new(ctx) ir_constant(tmp_vec->type, &zero_data)));
			instructions->push_tail(new(ctx) ir_assignment(
														   new(ctx) ir_dereference_variable(tmp_vec),
														   op[0], NULL, (1 << type0->vector_elements) - 1));
		}
		else
		{
			// The swizzle here is unnecessary if the # of elements match, but
			// it doesn't hurt and will be optimized out later anyway.
			instructions->push_tail(new(ctx) ir_assignment(
														   new(ctx) ir_dereference_variable(tmp_vec),
														   new(ctx) ir_swizzle(op[0], 0, 1, 2, 3, tmp_vec->type->vector_elements)));
		}
		
		if (bNativeMatrixIntrinsics)
		{
			return new(ctx) ir_expression(ir_binop_mul, type1->column_type(), new(ctx) ir_dereference_variable(tmp_vec), op[1]);
		}
		else
		{
			ir_variable* tmp_mat = new(ctx) ir_variable(type1, NULL, ir_var_temporary);
			instructions->push_tail(tmp_mat);
			instructions->push_tail(new(ctx) ir_assignment(
				new(ctx) ir_dereference_variable(tmp_mat), op[1]));
			
			ir_variable* tmp_result = new(ctx) ir_variable(type1->column_type(), NULL, ir_var_temporary);
			instructions->push_tail(tmp_result);
			
			for (unsigned c = 0; c < type1->matrix_columns; ++c)
			{
				ir_expression* expr = new(ctx) ir_expression(
					ir_binop_mul,
					new(ctx) ir_dereference_array(tmp_mat, new(ctx) ir_constant(c)),
					new(ctx) ir_swizzle(new(ctx) ir_dereference_variable(tmp_vec), c, c, c, c, type1->vector_elements));
				if (c > 0)
				{
					expr = new(ctx) ir_expression(
						ir_binop_add,
						expr,
						new(ctx) ir_dereference_variable(tmp_result));
					
					tmp_result = new(ctx) ir_variable(tmp_result->type, NULL, ir_var_temporary);
					instructions->push_tail(tmp_result);
				}
				instructions->push_tail(new(ctx) ir_assignment(
					new(ctx) ir_dereference_variable(tmp_result),
					expr));
			}
			
			return new(ctx) ir_dereference_variable(tmp_result);
		}
	}
	else if (type0->is_matrix() && type1->is_matrix())
	{
		if (type0->is_float() && type1->is_float())
		{
			if (type0->base_type != type1->base_type)
			{
				// This means half/vector
				if (type0->base_type == GLSL_TYPE_HALF)
				{
					if (apply_type_conversion(glsl_type::get_instance(GLSL_TYPE_FLOAT, type0->matrix_columns, type0->matrix_columns), op[0], instructions, state, false, loc) == false)
					{
						return NULL;
					}
					type0 = op[0]->type;
				}
				else
				{
					check((type1->base_type == GLSL_TYPE_HALF));
					if (apply_type_conversion(glsl_type::get_instance(GLSL_TYPE_FLOAT, type1->matrix_columns, type1->matrix_columns), op[1], instructions, state, false, loc) == false)
					{
						return NULL;
					}
					type1 = op[1]->type;
				}
			}
		}

		if (type0->vector_elements > type1->matrix_columns)
		{
			if (apply_type_conversion(
				glsl_type::get_instance(type0->base_type, type1->matrix_columns, type0->matrix_columns),
				op[0], instructions, state, false, loc) == false)
			{
				return NULL;
			}
			type0 = op[0]->type;
		}
		else if (type0->vector_elements < type1->matrix_columns)
		{
			if (apply_type_conversion(
				glsl_type::get_instance(type0->base_type, type1->vector_elements, type0->vector_elements),
				op[1], instructions, state, false, loc) == false)
			{
				return NULL;
			}
			type1 = op[1]->type;
		}


		check(type0->vector_elements == type1->matrix_columns);

		ir_variable* tmp0 = new(ctx) ir_variable(type0, NULL, ir_var_temporary);
		instructions->push_tail(tmp0);
		instructions->push_tail(new(ctx) ir_assignment(new(ctx) ir_dereference_variable(tmp0), op[0]));

		ir_variable* tmp1 = new(ctx) ir_variable(type1, NULL, ir_var_temporary);
		instructions->push_tail(tmp1);
		instructions->push_tail(new(ctx) ir_assignment(new(ctx) ir_dereference_variable(tmp1), op[1]));

		ir_variable* tmp_result = new(ctx) ir_variable(
			glsl_type::get_instance(type0->base_type, type1->vector_elements, type0->matrix_columns),
			NULL, ir_var_temporary);
		instructions->push_tail(tmp_result);

		ir_variable* tmp_vec = new(ctx) ir_variable(tmp_result->type->column_type(), NULL, ir_var_temporary);
		instructions->push_tail(tmp_vec);

		for (unsigned c0 = 0; c0 < type0->matrix_columns; ++c0)
		{
			for (unsigned c1 = 0; c1 < type1->matrix_columns; ++c1)
			{
				ir_expression* expr = new(ctx) ir_expression(
					ir_binop_mul,
					new(ctx) ir_swizzle(new(ctx) ir_dereference_array(tmp0, new(ctx) ir_constant(c0)),
					c1, c1, c1, c1, type1->vector_elements),
					new(ctx) ir_dereference_array(tmp1, new(ctx) ir_constant(c1)));
				if (c1 > 0)
				{
					expr = new(ctx) ir_expression(
						ir_binop_add,
						expr,
						new(ctx) ir_dereference_variable(tmp_vec));
					tmp_vec = new(ctx) ir_variable(tmp_vec->type, NULL, ir_var_temporary);
					instructions->push_tail(tmp_vec);
				}
				instructions->push_tail(new(ctx) ir_assignment(
					new(ctx) ir_dereference_variable(tmp_vec),
					expr));
			}
			instructions->push_tail(new(ctx) ir_assignment(
				new(ctx) ir_dereference_array(tmp_result, new(ctx) ir_constant(c0)),
				new(ctx) ir_dereference_variable(tmp_vec)));
		}

		return new(ctx) ir_dereference_variable(tmp_result);
	}

	return NULL;
}

static unsigned
process_parameters(exec_list *instructions, exec_list *actual_parameters,
	const exec_list *parameters, struct _mesa_glsl_parse_state *state)
{
	unsigned count = 0;

	foreach_list(n, parameters)
	{
		ast_node *const ast = exec_node_data(ast_node, n, link);
		ir_rvalue *result = ast->hir(instructions, state);

		ir_constant *const constant = result->constant_expression_value();
		if (constant != NULL)
		{
			result = constant;
		}

		actual_parameters->push_tail(result);
		count++;
	}

	return count;
}


/**
* Generate a source prototype for a function signature
*
* \param return_type Return type of the function.  May be \c NULL.
* \param name        Name of the function.
* \param parameters  List of \c ir_instruction nodes representing the
*                    parameter list for the function.  This may be either a
*                    formal (\c ir_variable) or actual (\c ir_rvalue)
*                    parameter list.  Only the type is used.
*
* \return
* A ralloced string representing the prototype of the function.
*/
char *
prototype_string(const glsl_type *return_type, const char *name, exec_list *parameters)
{
	char *str = NULL;

	if (return_type != NULL)
	{
		str = ralloc_asprintf(NULL, "%s ", return_type->name);
	}

	ralloc_asprintf_append(&str, "%s(", name);

	const char *comma = "";
	foreach_list(node, parameters)
	{
		const ir_variable *const param = (ir_variable *)node;

		ralloc_asprintf_append(&str, "%s%s", comma, param->type->name);
		comma = ", ";
	}

	ralloc_strcat(&str, ")");
	return str;
}

/**
* Verify that 'out' and 'inout' actual parameters are lvalues.  Also, verify
* that 'const_in' formal parameters (an extension in our IR) correspond to
* ir_constant actual parameters.
*/
static bool verify_parameter_modes(_mesa_glsl_parse_state *state,
	ir_function_signature *sig, exec_list &actual_ir_parameters, exec_list &actual_ast_parameters)
{
	exec_node *actual_ir_node = actual_ir_parameters.head;
	exec_node *actual_ast_node = actual_ast_parameters.head;

	foreach_list(formal_node, &sig->parameters)
	{
		/* The lists must be the same length. */
		check(!actual_ir_node->is_tail_sentinel());
		check(!actual_ast_node->is_tail_sentinel());

		const ir_variable *const formal = (ir_variable *)formal_node;
		ir_rvalue *const actual = (ir_rvalue *)actual_ir_node;
		const ast_expression *const actual_ast =
			exec_node_data(ast_expression, actual_ast_node, link);

		/* FIXME: 'loc' is incorrect (as of 2011-01-21). It is always
		* FIXME: 0:0(0).
		*/
		YYLTYPE loc = actual_ast->get_location();

		/* Verify that 'const_in' parameters are ir_constants. */
		if (formal->mode == ir_var_const_in &&
			actual->ir_type != ir_type_constant)
		{
			_mesa_glsl_error(&loc, state,
				"parameter 'in %s' must be a constant expression",
				formal->name);
			return false;
		}

		/* Verify that 'out' and 'inout' actual parameters are lvalues. */
		if (formal->mode == ir_var_out || formal->mode == ir_var_inout)
		{
			const char *mode = NULL;
			switch (formal->mode)
			{
			case ir_var_out:   mode = "out";   break;
			case ir_var_inout: mode = "inout"; break;
			default:           check(false);  break;
			}

			/* This AST-based check catches errors like f(i++).  The IR-based
			* is_lvalue() is insufficient because the actual parameter at the
			* IR-level is just a temporary value, which is an l-value.
			*/
			if (actual_ast->non_lvalue_description != NULL)
			{
				_mesa_glsl_error(&loc, state,
					"function parameter '%s %s' references a %s",
					mode, formal->name,
					actual_ast->non_lvalue_description);
				return false;
			}

			if (actual->variable_referenced()
				&& actual->variable_referenced()->read_only)
			{
				_mesa_glsl_error(&loc, state,
					"function parameter '%s %s' references the "
					"read-only variable '%s'",
					mode, formal->name,
					actual->variable_referenced()->name);
				return false;
			}
			else if (!actual->is_lvalue())
			{
				_mesa_glsl_error(&loc, state,
					"function parameter '%s %s' is not an lvalue",
					mode, formal->name);
				return false;
			}
		}

		/* Verify that references connect to real memory */
		if (formal->mode == ir_var_ref)
		{
			if (actual_ast->non_lvalue_description != NULL)
			{
				_mesa_glsl_error(&loc, state,
					"function parameter 'ref %s' references a %s"
					" reference values must be an RW resource or groupshared",
					formal->name,
					actual_ast->non_lvalue_description);
				return false;
			}
			/* check that it is either an image deref or a shared var */
			bool fail = true;
			if (actual->as_dereference_image() == NULL)
			{
				ir_dereference * deref = actual->as_dereference();
				if (deref)
				{
					ir_variable * var = deref->variable_referenced();
					if (var != NULL && var->mode == ir_var_shared)
					{
						fail = false;
					}
				}
			}
			else
			{
				fail = false;
			}
			if (fail)
			{
				_mesa_glsl_error(&loc, state,
					"function parameter 'ref %s' must be an RW resource or groupshared",
					formal->name);
				return false;
			}

		}

		actual_ir_node = actual_ir_node->next;
		actual_ast_node = actual_ast_node->next;
	}
	return true;
}

/**
* If a function call is generated, \c call_ir will point to it on exit.
* Otherwise \c call_ir will be set to \c NULL.
*/
static ir_rvalue* generate_call(exec_list *instructions, ir_function_signature *sig,
	YYLTYPE *loc, exec_list *actual_parameters, ir_call **call_ir, _mesa_glsl_parse_state *state)
{
	void *ctx = state;
	exec_list call_instructions;
	exec_list post_call_conversions;

	*call_ir = NULL;

	/* Perform implicit conversion of arguments.  For out parameters, we need
	* to place them in a temporary variable and do the conversion after the
	* call takes place.  Since we haven't emitted the call yet, we'll place
	* the post-call conversions in a temporary exec_list, and emit them later.
	*/
	exec_list_iterator actual_iter = actual_parameters->iterator();
	exec_list_iterator formal_iter = sig->parameters.iterator();

	while (actual_iter.has_next())
	{
		ir_rvalue *actual = (ir_rvalue *)actual_iter.get();
		ir_variable *formal = (ir_variable *)formal_iter.get();

		check(actual != NULL);
		check(formal != NULL);

		if (formal->type->is_numeric() || formal->type->is_boolean())
		{
			switch (formal->mode)
			{
			case ir_var_const_in:
			case ir_var_in:
			{
				ir_rvalue *converted = actual;
				apply_type_conversion(formal->type, converted, &call_instructions, state, false, loc);
				actual->replace_with(converted);
				break;
			}
			case ir_var_out:
				if (actual->type != formal->type)
				{
					/* To convert an out parameter, we need to create a
					* temporary variable to hold the value before conversion,
					* and then perform the conversion after the function call
					* returns.
					*
					* This has the effect of transforming code like this:
					*
					*   void f(out int x);
					*   float value;
					*   f(value);
					*
					* Into IR that's equivalent to this:
					*
					*   void f(out int x);
					*   float value;
					*   int out_parameter_conversion;
					*   f(out_parameter_conversion);
					*   value = float(out_parameter_conversion);
					*/
					ir_variable *tmp =
						new(ctx) ir_variable(formal->type,
						"out_parameter_conversion",
						ir_var_temporary);
					call_instructions.push_tail(tmp);
					ir_dereference_variable *deref_tmp_1
						= new(ctx) ir_dereference_variable(tmp);
					ir_dereference_variable *deref_tmp_2
						= new(ctx) ir_dereference_variable(tmp);
					ir_rvalue *converted_tmp = deref_tmp_1;
					apply_type_conversion(actual->type, converted_tmp,
						&post_call_conversions, state, false, loc);
					ir_assignment *assignment
						= new(ctx) ir_assignment(actual, converted_tmp);
					post_call_conversions.push_tail(assignment);
					actual->replace_with(deref_tmp_2);
				}
				break;
			case ir_var_inout:
				/* Inout parameters should never require conversion, since that
				* would require an implicit conversion to exist both to and
				* from the formal parameter type, and there are no
				* bidirectional implicit conversions.
				*/
				check(actual->type == formal->type);
				break;
			case ir_var_ref:
				/* Ref parameters much match exactly, as they will get directly
				* inlined with no conversion as they represent memory references
				*/
				check(actual->type == formal->type);
				break;
			default:
				check(!"Illegal formal parameter mode");
				break;
			}
		}

		actual_iter.next();
		formal_iter.next();
	}

	/* If the function call is a constant expression, don't generate any
	* instructions; just generate an ir_constant.
	*
	* Function calls were first allowed to be constant expressions in GLSL 1.20.
	*/
	if (state->language_version >= 120)
	{
		ir_constant *value = sig->constant_expression_value(actual_parameters);
		if (value != NULL)
		{
			return value;
		}
	}

	bool bReturnVoid = false;
	ir_dereference_variable *deref = NULL;
	if (sig->return_type->is_void())
	{
		bReturnVoid = true;
	}
	else
	{
		/* Create a new temporary to hold the return value. */
		ir_variable *var;

		var = new(ctx) ir_variable(sig->return_type,
			ralloc_asprintf(ctx, "%s_retval",
			sig->function_name()),
			ir_var_temporary);
		call_instructions.push_tail(var);

		deref = new(ctx) ir_dereference_variable(var);
	}
	ir_call *call = new(ctx) ir_call(sig, deref, actual_parameters);
	call_instructions.push_tail(call);

	ir_constant* const_value = NULL;
	if (deref && deref->var && post_call_conversions.is_empty() &&
		sig->is_builtin && sig->is_defined && can_inline(call))
	{
		ir_variable* var = deref->var;
		var->mode = ir_var_out;

		bool progress = false;
		do
		{
			progress = do_optimization_pass(&call_instructions, state, true);
		}
		while (progress);

		ir_instruction* tail_ir = (ir_instruction*)call_instructions.get_tail();
		if (tail_ir && tail_ir->as_assignment())
		{
			ir_assignment* assign = (ir_assignment*)tail_ir;
			if (assign->lhs->variable_referenced() == var)
			{
				const_value = assign->rhs->constant_expression_value();
			}
		}

		if (const_value)
		{
			ralloc_free(call);
			ralloc_free(deref);
			ralloc_free(var);
		}
		else
		{
			var->mode = ir_var_temporary;
		}
	}

	if (const_value)
	{
		check(!bReturnVoid);
		return const_value;
	}

	instructions->append_list(&call_instructions);
	instructions->append_list(&post_call_conversions);

	if (bReturnVoid)
	{
		check(!deref);
		return ir_rvalue::void_value(ctx);
	}

	return deref ? deref->clone(ctx, NULL) : NULL;
}

/**
* Given a function name and parameter list, find the matching signature.
*/
static ir_function_signature* match_function_by_name(const char *name,
		exec_list *actual_parameters, struct _mesa_glsl_parse_state *state)
{
	void *ctx = state;
	ir_function *f = state->symbols->get_function(name);
	ir_function_signature *local_sig = NULL;
	ir_function_signature *sig = NULL;

	/* Is the function hidden by a record type constructor? */
	if (state->symbols->get_type(name))
	{
		goto done; /* no match */
	}

	/* Is the function hidden by a variable (impossible in 1.10)? */
	if (state->language_version != 110 && state->symbols->get_variable(name))
	{
		goto done; /* no match */
	}

	if (f != NULL)
	{
		/* Look for a match in the local shader.  If exact, we're done. */
		bool is_exact = false;
		sig = local_sig = f->matching_signature(actual_parameters, &is_exact);
		if (is_exact)
		{
			goto done;
		}

		if (f->has_user_signature())
		{
			/* In desktop GL, the presence of a user-defined signature hides any
			* built-in signatures, so we must ignore them.  In contrast, in ES2
			* user-defined signatures add new overloads, so we must proceed.
			*/
			goto done;
		}
	}

done:
	if (sig != NULL)
	{
		/* If the match is from a linked built-in shader, import the prototype. */
		if (sig != local_sig)
		{
			if (f == NULL)
			{
				f = new(ctx) ir_function(name);
				state->symbols->add_global_function(f);
				emit_function(state, f);
			}
			f->add_signature(sig->clone_prototype(f, NULL));
		}
	}
	return sig;
}

/**
* Raise a "no matching function" error, listing all possible overloads the
* compiler considered so developers can figure out what went wrong.
*/
static void no_matching_function_error(const char *name, YYLTYPE *loc,
	exec_list *actual_parameters, _mesa_glsl_parse_state *state)
{
	char *str = prototype_string(NULL, name, actual_parameters);
	_mesa_glsl_error(loc, state, "no matching function for call to '%s'", str);
	ralloc_free(str);

	const char *prefix = "candidates are: ";

	ir_function *f = state->symbols->get_function(name);
	if (f)
	{
		foreach_list(node, &f->signatures)
		{
			ir_function_signature *sig = (ir_function_signature *)node;
			str = prototype_string(sig->return_type, f->name, &sig->parameters);
			_mesa_glsl_error(loc, state, "%s%s", prefix, str);
			ralloc_free(str);
			prefix = "                ";
		}
	}
}

/**
* Perform automatic type conversion of constructor parameters
*
* This implements the rules in the "Conversion and Scalar Constructors"
* section (GLSL 1.10 section 5.4.1), not the "Implicit Conversions" rules.
*/
ir_rvalue* convert_component(ir_rvalue *src, const glsl_type *desired_type)
{
	void *ctx = ralloc_parent(src);
	auto a = desired_type->base_type;
	auto b = src->type->base_type;
	ir_expression *result = NULL;

	if (src->type->is_error())
	{
		return src;
	}

	if (src->type->is_sampler() || desired_type->is_sampler())
	{
		check(src->type->is_sampler());
		check(desired_type->is_sampler());
		return src;
	}

	check(a <= GLSL_TYPE_BOOL);
	check(b <= GLSL_TYPE_BOOL);
	check(src->type->vector_elements == desired_type->vector_elements);
	check(src->type->matrix_columns == desired_type->matrix_columns);

	if (a == b)
	{
		return src;
	}

	switch (a)
	{
	case GLSL_TYPE_UINT:
		switch (b)
		{
		case GLSL_TYPE_INT:
			result = new(ctx) ir_expression(ir_unop_i2u, src);
			break;
		case GLSL_TYPE_HALF:
			result = new(ctx) ir_expression(ir_unop_h2u, src);
			//result = new(ctx) ir_expression(ir_unop_i2u,
			//	  new(ctx) ir_expression(ir_unop_f2i, src));
			break;
		case GLSL_TYPE_FLOAT:
			result = new(ctx) ir_expression(ir_unop_f2u, src);
			//result = new(ctx) ir_expression(ir_unop_i2u,
			//	  new(ctx) ir_expression(ir_unop_f2i, src));
			break;
		case GLSL_TYPE_BOOL:
			result = new(ctx) ir_expression(ir_unop_b2u, src);
			//result = new(ctx) ir_expression(ir_unop_i2u,
			//new(ctx) ir_expression(ir_unop_b2i, src));
			break;
		default:
			break;
		}
		break;
	case GLSL_TYPE_INT:
		switch (b)
		{
		case GLSL_TYPE_UINT:
			result = new(ctx) ir_expression(ir_unop_u2i, src);
			break;
		case GLSL_TYPE_HALF:
			result = new(ctx) ir_expression(ir_unop_h2i, src);
			break;
		case GLSL_TYPE_FLOAT:
			result = new(ctx) ir_expression(ir_unop_f2i, src);
			break;
		case GLSL_TYPE_BOOL:
			result = new(ctx) ir_expression(ir_unop_b2i, src);
			break;
		default:
			break;
		}
		break;
	case GLSL_TYPE_HALF:
		switch (b)
		{
		case GLSL_TYPE_UINT:
			result = new(ctx) ir_expression(ir_unop_u2h, desired_type, src, NULL);
			break;
		case GLSL_TYPE_INT:
			result = new(ctx) ir_expression(ir_unop_i2h, desired_type, src, NULL);
			break;
		case GLSL_TYPE_FLOAT:
			result = new(ctx) ir_expression(ir_unop_f2h, desired_type, src, NULL);
			break;
		case GLSL_TYPE_BOOL:
			result = new(ctx) ir_expression(ir_unop_b2h, desired_type, src, NULL);
			break;
		default:
			break;
		}
		break;
	case GLSL_TYPE_FLOAT:
		switch (b)
		{
		case GLSL_TYPE_UINT:
			result = new(ctx) ir_expression(ir_unop_u2f, desired_type, src, NULL);
			break;
		case GLSL_TYPE_INT:
			result = new(ctx) ir_expression(ir_unop_i2f, desired_type, src, NULL);
			break;
		case GLSL_TYPE_HALF:
			result = new(ctx) ir_expression(ir_unop_h2f, desired_type, src, NULL);
			break;
		case GLSL_TYPE_BOOL:
			result = new(ctx) ir_expression(ir_unop_b2f, desired_type, src, NULL);
			break;
		default:
			break;
		}
		break;
	case GLSL_TYPE_BOOL:
		switch (b)
		{
		case GLSL_TYPE_UINT:
			result = new(ctx) ir_expression(ir_unop_u2b, src);
			//result = new(ctx) ir_expression(ir_unop_i2b,
			// new(ctx) ir_expression(ir_unop_u2i, src));
			break;
		case GLSL_TYPE_INT:
			result = new(ctx) ir_expression(ir_unop_i2b, desired_type, src, NULL);
			break;
		case GLSL_TYPE_HALF:
			result = new(ctx) ir_expression(ir_unop_h2b, desired_type, src, NULL);
			break;
		case GLSL_TYPE_FLOAT:
			result = new(ctx) ir_expression(ir_unop_f2b, desired_type, src, NULL);
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}

	check(result != NULL);
	check(result->type == desired_type);

	/* Try constant folding; it may fold in the conversion we just added. */
	ir_constant *const constant = result->constant_expression_value();
	return (constant != NULL) ? (ir_rvalue *)constant : (ir_rvalue *)result;
}

/**
* Dereference a specific component from a scalar, vector, or matrix
*/
static ir_rvalue* dereference_component(ir_rvalue *src, unsigned component)
{
	void *ctx = ralloc_parent(src);
	check(component < src->type->components());

	/* If the source is a constant, just create a new constant instead of a
	* dereference of the existing constant.
	*/
	ir_constant *constant = src->as_constant();
	if (constant)
	{
		return new(ctx) ir_constant(constant, component);
	}

	if (src->type->is_scalar())
	{
		return src;
	}
	else if (src->type->is_vector())
	{
		return new(ctx) ir_swizzle(src, component, 0, 0, 0, 1);
	}
	else
	{
		check(src->type->is_matrix());

		/* Dereference a row of the matrix, then call this function again to get
		* a specific element from that row.
		*/
		const int c = component / src->type->column_type()->vector_elements;
		const int r = component % src->type->column_type()->vector_elements;
		ir_constant *const col_index = new(ctx) ir_constant(c);
		ir_dereference *const col = new(ctx) ir_dereference_array(src, col_index);

		col->type = src->type->column_type();

		return dereference_component(col, r);
	}

	check(!"Should not get here.");
	return NULL;
}


static ir_rvalue* process_array_constructor(exec_list *instructions, const glsl_type *constructor_type,
						YYLTYPE *loc, exec_list *parameters, struct _mesa_glsl_parse_state *state)
{
	void *ctx = state;
	/* Array constructors come in two forms: sized and unsized.  Sized array
	* constructors look like 'vec4[2](a, b)', where 'a' and 'b' are vec4
	* variables.  In this case the number of parameters must exactly match the
	* specified size of the array.
	*
	* Unsized array constructors look like 'vec4[](a, b)', where 'a' and 'b'
	* are vec4 variables.  In this case the size of the array being constructed
	* is determined by the number of parameters.
	*
	* From page 52 (page 58 of the PDF) of the GLSL 1.50 spec:
	*
	*    "There must be exactly the same number of arguments as the size of
	*    the array being constructed. If no size is present in the
	*    constructor, then the array is explicitly sized to the number of
	*    arguments provided. The arguments are assigned in order, starting at
	*    element 0, to the elements of the constructed array. Each argument
	*    must be the same type as the element type of the array, or be a type
	*    that can be converted to the element type of the array according to
	*    Section 4.1.10 "Implicit Conversions.""
	*/
	exec_list actual_parameters;
	const unsigned parameter_count =
		process_parameters(instructions, &actual_parameters, parameters, state);

	if ((parameter_count == 0)
		|| ((constructor_type->length != 0)
		&& (constructor_type->length != parameter_count)))
	{
		const unsigned min_param = (constructor_type->length == 0)
			? 1 : constructor_type->length;

		_mesa_glsl_error(loc, state, "array constructor must have %s %u "
			"parameter%s",
			(constructor_type->length != 0) ? "at least" : "exactly",
			min_param, (min_param <= 1) ? "" : "s");
		return ir_rvalue::error_value(ctx);
	}

	if (constructor_type->length == 0)
	{
		constructor_type =
			glsl_type::get_array_instance(constructor_type->element_type(),
			parameter_count);
		check(constructor_type != NULL);
		check(constructor_type->length == parameter_count);
	}

	bool all_parameters_are_constant = true;

	/* Type cast each parameter and, if possible, fold constants. */
	foreach_list_safe(n, &actual_parameters)
	{
		ir_rvalue *ir = (ir_rvalue *)n;
		ir_rvalue *result = ir;

		/* Apply implicit conversions (not the scalar constructor rules!). See
		* the spec quote above. */
		if (constructor_type->element_type()->is_float())
		{
			const glsl_type *desired_type =
				glsl_type::get_instance(constructor_type->element_type()->base_type,
				ir->type->vector_elements,
				ir->type->matrix_columns);
			if (result->type->can_implicitly_convert_to(desired_type))
			{
				result = ir;
				apply_type_conversion(desired_type, result, instructions, state, false, loc);

			}
		}

		if (result->type != constructor_type->element_type())
		{
			_mesa_glsl_error(loc, state, "type error in array constructor: "
				"expected: %s, found %s",
				constructor_type->element_type()->name,
				result->type->name);
		}

		/* Attempt to convert the parameter to a constant valued expression.
		* After doing so, track whether or not all the parameters to the
		* constructor are trivially constant valued expressions.
		*/
		ir_rvalue *const constant = result->constant_expression_value();

		if (constant != NULL)
		{
			result = constant;
		}
		else
		{
			all_parameters_are_constant = false;
		}

		ir->replace_with(result);
	}

	if (all_parameters_are_constant)
	{
		return new(ctx) ir_constant(constructor_type, &actual_parameters);
	}

	ir_variable *var = new(ctx) ir_variable(constructor_type, "array_ctor",
		ir_var_temporary);
	instructions->push_tail(var);

	int i = 0;
	foreach_list(node, &actual_parameters)
	{
		ir_rvalue *rhs = (ir_rvalue *)node;
		ir_rvalue *lhs = new(ctx) ir_dereference_array(var,
			new(ctx) ir_constant(i));

		ir_instruction *assignment = new(ctx) ir_assignment(lhs, rhs, NULL);
		instructions->push_tail(assignment);

		i++;
	}

	return new(ctx) ir_dereference_variable(var);
}


/**
* Try to convert a record constructor to a constant expression
*/
static ir_constant * constant_record_constructor(const glsl_type *constructor_type, exec_list *parameters, void *mem_ctx)
{
	foreach_list(node, parameters)
	{
		ir_constant *constant = ((ir_instruction *)node)->as_constant();
		if (constant == NULL)
		{
			return NULL;
		}
		node->replace_with(constant);
	}

	return new(mem_ctx) ir_constant(constructor_type, parameters);
}


/**
* Determine if a list consists of a single scalar r-value
*/
bool single_scalar_parameter(exec_list *parameters)
{
	const ir_rvalue *const p = (ir_rvalue *)parameters->head;
	check(((ir_rvalue *)p)->as_rvalue() != NULL);

	return (p->type->is_scalar() && p->next->is_tail_sentinel());
}


/**
* Generate inline code for a vector constructor
*
* The generated constructor code will consist of a temporary variable
* declaration of the same type as the constructor.  A sequence of assignments
* from constructor parameters to the temporary will follow.
*
* \return
* An \c ir_dereference_variable of the temprorary generated in the constructor
* body.
*/
ir_rvalue * emit_inline_vector_constructor(const glsl_type *type, exec_list *instructions, exec_list *parameters, void *ctx)
{
	check(!parameters->is_empty());

	ir_variable *var = new(ctx) ir_variable(type, "vec_ctor", ir_var_temporary);
	instructions->push_tail(var);

	/* There are two kinds of vector constructors.
	*
	*  - Construct a vector from a single scalar by replicating that scalar to
	*    all components of the vector.
	*
	*  - Construct a vector from an arbirary combination of vectors and
	*    scalars.  The components of the constructor parameters are assigned
	*    to the vector in order until the vector is full.
	*/
	const unsigned lhs_components = type->components();
	if (single_scalar_parameter(parameters))
	{
		ir_rvalue *first_param = (ir_rvalue *)parameters->head;
		ir_rvalue *rhs = new(ctx) ir_swizzle(first_param, 0, 0, 0, 0,
			lhs_components);
		ir_dereference_variable *lhs = new(ctx) ir_dereference_variable(var);
		const unsigned mask = (1U << lhs_components) - 1;

		check(rhs->type == lhs->type);

		ir_instruction *inst = new(ctx) ir_assignment(lhs, rhs, NULL, mask);
		instructions->push_tail(inst);
	}
	else
	{
		unsigned base_component = 0;
		unsigned base_lhs_component = 0;
		ir_constant_data data;
		unsigned constant_mask = 0, constant_components = 0;

		memset(&data, 0, sizeof(data));

		foreach_list(node, parameters)
		{
			ir_rvalue *param = (ir_rvalue *)node;
			unsigned rhs_components = param->type->components();

			/* Do not try to assign more components to the vector than it has!
			*/
			if ((rhs_components + base_lhs_component) > lhs_components)
			{
				rhs_components = lhs_components - base_lhs_component;
			}

			const ir_constant *const c = param->as_constant();
			if (c != NULL)
			{
				for (unsigned i = 0; i < rhs_components; i++)
				{
					switch (c->type->base_type)
					{
					case GLSL_TYPE_UINT:
						data.u[i + base_component] = c->get_uint_component(i);
						break;
					case GLSL_TYPE_INT:
						data.i[i + base_component] = c->get_int_component(i);
						break;
					case GLSL_TYPE_HALF:
					case GLSL_TYPE_FLOAT:
						data.f[i + base_component] = c->get_float_component(i);
						break;
					case GLSL_TYPE_BOOL:
						data.b[i + base_component] = c->get_bool_component(i);
						break;
					default:
						check(!"Should not get here.");
						break;
					}
				}

				/* Mask of fields to be written in the assignment.
				*/
				constant_mask |= ((1U << rhs_components) - 1) << base_lhs_component;
				constant_components += rhs_components;

				base_component += rhs_components;
			}
			/* Advance the component index by the number of components
			* that were just assigned.
			*/
			base_lhs_component += rhs_components;
		}

		if (constant_mask != 0)
		{
			ir_dereference *lhs = new(ctx) ir_dereference_variable(var);
			const glsl_type *rhs_type = glsl_type::get_instance(var->type->base_type,
				constant_components,
				1);
			ir_rvalue *rhs = new(ctx) ir_constant(rhs_type, &data);

			ir_instruction *inst =
				new(ctx) ir_assignment(lhs, rhs, NULL, constant_mask);
			instructions->push_tail(inst);
		}

		base_component = 0;
		foreach_list(node, parameters)
		{
			ir_rvalue *param = (ir_rvalue *)node;
			unsigned rhs_components = param->type->components();

			/* Do not try to assign more components to the vector than it has!
			*/
			if ((rhs_components + base_component) > lhs_components)
			{
				rhs_components = lhs_components - base_component;
			}

			const ir_constant *const c = param->as_constant();
			if (c == NULL)
			{
				/* Mask of fields to be written in the assignment.
				*/
				const unsigned write_mask = ((1U << rhs_components) - 1)
					<< base_component;

				ir_dereference *lhs = new(ctx) ir_dereference_variable(var);

				/* Generate a swizzle so that LHS and RHS sizes match.
				*/
				ir_rvalue *rhs =
					new(ctx) ir_swizzle(param, 0, 1, 2, 3, rhs_components);

				ir_instruction *inst =
					new(ctx) ir_assignment(lhs, rhs, NULL, write_mask);
				instructions->push_tail(inst);
			}

			/* Advance the component index by the number of components that were
			* just assigned.
			*/
			base_component += rhs_components;
		}
	}
	return new(ctx) ir_dereference_variable(var);
}


/**
* Generate assignment of a portion of a vector to a portion of a matrix column
*
* \param src_base  First component of the source to be used in assignment
* \param column    Column of destination to be assiged
* \param row_base  First component of the destination column to be assigned
* \param count     Number of components to be assigned
*
* \note
* \c src_base + \c count must be less than or equal to the number of components
* in the source vector.
*/
ir_instruction* assign_to_matrix_column(ir_variable *var, unsigned column, unsigned row_base,
		ir_rvalue *src, unsigned src_base, unsigned count, void *mem_ctx)
{
	ir_constant *col_idx = new(mem_ctx) ir_constant(column);
	ir_dereference *column_ref = new(mem_ctx) ir_dereference_array(var, col_idx);

	check(column_ref->type->components() >= (row_base + count));
	check(src->type->components() >= (src_base + count));

	/* Generate a swizzle that extracts the number of components from the source
	* that are to be assigned to the column of the matrix.
	*/
	if (count < src->type->vector_elements)
	{
		src = new(mem_ctx) ir_swizzle(src,
			src_base + 0, src_base + 1,
			src_base + 2, src_base + 3,
			count);
	}

	/* Mask of fields to be written in the assignment.
	*/
	const unsigned write_mask = ((1U << count) - 1) << row_base;

	return new(mem_ctx) ir_assignment(column_ref, src, NULL, write_mask);
}


/**
* Generate inline code for a matrix constructor
*
* The generated constructor code will consist of a temporary variable
* declaration of the same type as the constructor.  A sequence of assignments
* from constructor parameters to the temporary will follow.
*
* \return
* An \c ir_dereference_variable of the temprorary generated in the constructor
* body.
*/
ir_rvalue* emit_inline_matrix_constructor(const glsl_type *type,
	exec_list *instructions, exec_list *parameters, void *ctx)
{
	check(!parameters->is_empty());

	ir_variable *var = new(ctx) ir_variable(type, "mat_ctor", ir_var_temporary);
	instructions->push_tail(var);

	/* There are three kinds of matrix constructors.
	*
	*  - Construct a matrix from a single scalar by replicating that scalar to
	*    along the diagonal of the matrix and setting all other components to
	*    zero.
	*
	*  - Construct a matrix from an arbirary combination of vectors and
	*    scalars.  The components of the constructor parameters are assigned
	*    to the matrix in colum-major order until the matrix is full.
	*
	*  - Construct a matrix from a single matrix.  The source matrix is copied
	*    to the upper left portion of the constructed matrix, and the remaining
	*    elements take values from the identity matrix.
	*/
	ir_rvalue *const first_param = (ir_rvalue *)parameters->head;
	if (single_scalar_parameter(parameters))
	{
		/* Assign the scalar to the X component of a vec4, and fill the remaining
		* components with zero.
		*/
		ir_variable *rhs_var =
			new(ctx) ir_variable(glsl_type::vec4_type, "mat_ctor_vec",
			ir_var_temporary);
		instructions->push_tail(rhs_var);

		ir_constant_data zero;
		zero.f[0] = 0.0;
		zero.f[1] = 0.0;
		zero.f[2] = 0.0;
		zero.f[3] = 0.0;

		ir_instruction *inst =
			new(ctx) ir_assignment(new(ctx) ir_dereference_variable(rhs_var),
			new(ctx) ir_constant(rhs_var->type, &zero),
			NULL);
		instructions->push_tail(inst);

		ir_dereference *const rhs_ref = new(ctx) ir_dereference_variable(rhs_var);

		inst = new(ctx) ir_assignment(rhs_ref, first_param, NULL, 0x01);
		instructions->push_tail(inst);

		/* Assign the temporary vector to each column of the destination matrix
		* with a swizzle that puts the X component on the diagonal of the
		* matrix.  In some cases this may mean that the X component does not
		* get assigned into the column at all (i.e., when the matrix has more
		* columns than rows).
		*/
		static const unsigned rhs_swiz[4][4] =
		{
			{ 0, 1, 1, 1 },
			{ 1, 0, 1, 1 },
			{ 1, 1, 0, 1 },
			{ 1, 1, 1, 0 }
		};

		const unsigned cols_to_init = MIN2(type->matrix_columns,
			type->vector_elements);
		for (unsigned i = 0; i < cols_to_init; i++)
		{
			ir_constant *const col_idx = new(ctx) ir_constant(i);
			ir_rvalue *const col_ref = new(ctx) ir_dereference_array(var, col_idx);

			ir_rvalue *const rhs_ref = new(ctx) ir_dereference_variable(rhs_var);
			ir_rvalue *const rhs = new(ctx) ir_swizzle(rhs_ref, rhs_swiz[i],
				type->vector_elements);

			inst = new(ctx) ir_assignment(col_ref, rhs, NULL);
			instructions->push_tail(inst);
		}

		for (unsigned i = cols_to_init; i < type->matrix_columns; i++)
		{
			ir_constant *const col_idx = new(ctx) ir_constant(i);
			ir_rvalue *const col_ref = new(ctx) ir_dereference_array(var, col_idx);

			ir_rvalue *const rhs_ref = new(ctx) ir_dereference_variable(rhs_var);
			ir_rvalue *const rhs = new(ctx) ir_swizzle(rhs_ref, 1, 1, 1, 1,
				type->vector_elements);

			inst = new(ctx) ir_assignment(col_ref, rhs, NULL);
			instructions->push_tail(inst);
		}
	}
	else if (first_param->type->is_matrix())
	{
		/* From page 50 (56 of the PDF) of the GLSL 1.50 spec:
		*
		*     "If a matrix is constructed from a matrix, then each component
		*     (column i, row j) in the result that has a corresponding
		*     component (column i, row j) in the argument will be initialized
		*     from there. All other components will be initialized to the
		*     identity matrix. If a matrix argument is given to a matrix
		*     constructor, it is an error to have any other arguments."
		*/
		check(first_param->next->is_tail_sentinel());
		ir_rvalue *const src_matrix = first_param;

		/* If the source matrix is smaller, pre-initialize the relavent parts of
		* the destination matrix to the identity matrix.
		*/
		if ((src_matrix->type->matrix_columns < var->type->matrix_columns)
			|| (src_matrix->type->vector_elements < var->type->vector_elements))
		{

			/* If the source matrix has fewer rows, every column of the destination
			* must be initialized.  Otherwise only the columns in the destination
			* that do not exist in the source must be initialized.
			*/
			unsigned col =
				(src_matrix->type->vector_elements < var->type->vector_elements)
				? 0 : src_matrix->type->matrix_columns;

			const glsl_type *const col_type = var->type->column_type();
			for (/* empty */; col < var->type->matrix_columns; col++)
			{
				ir_constant_data ident;

				ident.f[0] = 0.0;
				ident.f[1] = 0.0;
				ident.f[2] = 0.0;
				ident.f[3] = 0.0;

				ident.f[col] = 1.0;

				ir_rvalue *const rhs = new(ctx) ir_constant(col_type, &ident);

				ir_rvalue *const lhs =
					new(ctx) ir_dereference_array(var, new(ctx) ir_constant(col));

				ir_instruction *inst = new(ctx) ir_assignment(lhs, rhs, NULL);
				instructions->push_tail(inst);
			}
		}

		/* Assign columns from the source matrix to the destination matrix.
		*
		* Since the parameter will be used in the RHS of multiple assignments,
		* generate a temporary and copy the paramter there.
		*/
		ir_variable *const rhs_var =
			new(ctx) ir_variable(first_param->type, "mat_ctor_mat",
			ir_var_temporary);
		instructions->push_tail(rhs_var);

		ir_dereference *const rhs_var_ref =
			new(ctx) ir_dereference_variable(rhs_var);
		ir_instruction *const inst =
			new(ctx) ir_assignment(rhs_var_ref, first_param, NULL);
		instructions->push_tail(inst);

		const unsigned last_row = MIN2(src_matrix->type->vector_elements,
			var->type->vector_elements);
		const unsigned last_col = MIN2(src_matrix->type->matrix_columns,
			var->type->matrix_columns);

		unsigned swiz[4] = { 0, 0, 0, 0 };
		for (unsigned i = 1; i < last_row; i++)
		{
			swiz[i] = i;
		}

		const unsigned write_mask = (1U << last_row) - 1;

		for (unsigned i = 0; i < last_col; i++)
		{
			ir_dereference *const lhs =
				new(ctx) ir_dereference_array(var, new(ctx) ir_constant(i));
			ir_rvalue *const rhs_col =
				new(ctx) ir_dereference_array(rhs_var, new(ctx) ir_constant(i));

			/* If one matrix has columns that are smaller than the columns of the
			* other matrix, wrap the column access of the larger with a swizzle
			* so that the LHS and RHS of the assignment have the same size (and
			* therefore have the same type).
			*
			* It would be perfectly valid to unconditionally generate the
			* swizzles, this this will typically result in a more compact IR tree.
			*/
			ir_rvalue *rhs;
			if (lhs->type->vector_elements != rhs_col->type->vector_elements)
			{
				rhs = new(ctx) ir_swizzle(rhs_col, swiz, last_row);
			}
			else
			{
				rhs = rhs_col;
			}

			ir_instruction *inst =
				new(ctx) ir_assignment(lhs, rhs, NULL, write_mask);
			instructions->push_tail(inst);
		}
	}
	else
	{
		const unsigned cols = type->matrix_columns;
		const unsigned rows = type->vector_elements;
		unsigned col_idx = 0;
		unsigned row_idx = 0;

		foreach_list(node, parameters)
		{
			ir_rvalue *const rhs = (ir_rvalue *)node;
			const unsigned components_remaining_this_column = rows - row_idx;
			unsigned rhs_components = rhs->type->components();
			unsigned rhs_base = 0;

			/* Since the parameter might be used in the RHS of two assignments,
			* generate a temporary and copy the paramter there.
			*/
			ir_variable *rhs_var =
				new(ctx) ir_variable(rhs->type, "mat_ctor_vec", ir_var_temporary);
			instructions->push_tail(rhs_var);

			ir_dereference *rhs_var_ref =
				new(ctx) ir_dereference_variable(rhs_var);
			ir_instruction *inst = new(ctx) ir_assignment(rhs_var_ref, rhs, NULL);
			instructions->push_tail(inst);

			/* Assign the current parameter to as many components of the matrix
			* as it will fill.
			*
			* NOTE: A single vector parameter can span two matrix columns.  A
			* single vec4, for example, can completely fill a mat2.
			*/
			if (rhs_components >= components_remaining_this_column)
			{
				const unsigned count = MIN2(rhs_components,
					components_remaining_this_column);

				rhs_var_ref = new(ctx) ir_dereference_variable(rhs_var);

				ir_instruction *inst = assign_to_matrix_column(var, col_idx,
					row_idx,
					rhs_var_ref, 0,
					count, ctx);
				instructions->push_tail(inst);

				rhs_base = count;

				col_idx++;
				row_idx = 0;
			}

			/* If there is data left in the parameter and components left to be
			* set in the destination, emit another assignment.  It is possible
			* that the assignment could be of a vec4 to the last element of the
			* matrix.  In this case col_idx==cols, but there is still data
			* left in the source parameter.  Obviously, don't emit an assignment
			* to data outside the destination matrix.
			*/
			if ((col_idx < cols) && (rhs_base < rhs_components))
			{
				const unsigned count = rhs_components - rhs_base;

				rhs_var_ref = new(ctx) ir_dereference_variable(rhs_var);

				ir_instruction *inst = assign_to_matrix_column(var, col_idx,
					row_idx,
					rhs_var_ref,
					rhs_base,
					count, ctx);
				instructions->push_tail(inst);

				row_idx += count;
			}
		}
	}

	return new(ctx) ir_dereference_variable(var);
}

ir_rvalue * emit_inline_record_constructor(const glsl_type *type,
	exec_list *instructions, exec_list *parameters, void *mem_ctx)
{
	ir_variable *const var =
		new(mem_ctx) ir_variable(type, "record_ctor", ir_var_temporary);
	ir_dereference_variable *const d = new(mem_ctx) ir_dereference_variable(var);

	instructions->push_tail(var);

	exec_node *node = parameters->head;
	for (unsigned i = 0; i < type->length; i++)
	{
		check(!node->is_tail_sentinel());

		ir_dereference *const lhs =
			new(mem_ctx) ir_dereference_record(d->clone(mem_ctx, NULL),
			type->fields.structure[i].name);

		ir_rvalue *const rhs = ((ir_instruction *)node)->as_rvalue();
		check(rhs != NULL);

		ir_instruction *const assign = new(mem_ctx) ir_assignment(lhs, rhs, NULL);

		instructions->push_tail(assign);
		node = node->next;
	}

	return d;
}


ir_rvalue* ast_function_expression::hir(exec_list *instructions, struct _mesa_glsl_parse_state *state)
{
	void *ctx = state;
	/* There are three sorts of function calls.
	*
	* 1. constructors - The first subexpression is an ast_type_specifier.
	* 2. methods - Only the .length() method of array types.
	* 3. functions - Calls to regular old functions.
	*
	* Method calls are actually detected when the ast_field_selection
	* expression is handled.
	*/
	if (is_constructor())
	{
		const ast_type_specifier *type = (ast_type_specifier *)subexpressions[0];
		YYLTYPE loc = type->get_location();
		const char *name;

		const glsl_type *const constructor_type = type->glsl_type(&name, state);

		/* constructor_type can be NULL if a variable with the same name as the
		* structure has come into scope.
		*/
		if (constructor_type == NULL)
		{
			_mesa_glsl_error(&loc, state, "unknown type '%s' (structure name "
				"may be shadowed by a variable with the same name)",
				type->type_name);
			return ir_rvalue::error_value(ctx);
		}


		/* Constructors for samplers are illegal.
		*/
		if (constructor_type->is_sampler())
		{
			_mesa_glsl_error(&loc, state, "cannot construct sampler type '%s'",
				constructor_type->name);
			return ir_rvalue::error_value(ctx);
		}

		if (constructor_type->is_array())
		{
			if (state->language_version <= 110){
				_mesa_glsl_error(&loc, state,
					"array constructors forbidden in GLSL 1.10");
				return ir_rvalue::error_value(ctx);
			}

			return process_array_constructor(instructions, constructor_type,
				&loc, &this->expressions, state);
		}


		/* There are two kinds of constructor call.  Constructors for built-in
		* language types, such as mat4 and vec2, are free form.  The only
		* requirement is that the parameters must provide enough values of the
		* correct scalar type.  Constructors for arrays and structures must
		* have the exact number of parameters with matching types in the
		* correct order.  These constructors follow essentially the same type
		* matching rules as functions.
		*/
		if (constructor_type->is_record())
		{
			exec_list actual_parameters;

			process_parameters(instructions, &actual_parameters,
				&this->expressions, state);

			exec_node *node = actual_parameters.head;
			for (unsigned i = 0; i < constructor_type->length; i++)
			{
				ir_rvalue *ir = (ir_rvalue *)node;

				if (node->is_tail_sentinel())
				{
					_mesa_glsl_error(&loc, state,
						"insufficient parameters to constructor "
						"for '%s'",
						constructor_type->name);
					return ir_rvalue::error_value(ctx);
				}

				if (apply_type_conversion(constructor_type->fields.structure[i].type,
					ir, instructions, state, /*is_explicit=*/ false, &loc))
				{
					node->replace_with(ir);
				}
				else
				{
					_mesa_glsl_error(&loc, state,
						"parameter type mismatch in constructor "
						"for '%s.%s' (%s vs %s)",
						constructor_type->name,
						constructor_type->fields.structure[i].name,
						ir->type->name,
						constructor_type->fields.structure[i].type->name);
					return ir_rvalue::error_value(ctx);;
				}

				node = node->next;
			}

			if (!node->is_tail_sentinel())
			{
				_mesa_glsl_error(&loc, state, "too many parameters in constructor "
					"for '%s'", constructor_type->name);
				return ir_rvalue::error_value(ctx);
			}

			ir_rvalue *const constant =
				constant_record_constructor(constructor_type, &actual_parameters,
				state);

			return (constant != NULL)
				? constant
				: emit_inline_record_constructor(constructor_type, instructions,
				&actual_parameters, state);
		}

		if (!constructor_type->is_numeric() && !constructor_type->is_boolean())
		{
			return ir_rvalue::error_value(ctx);
		}

		/* Total number of components of the type being constructed. */
		const unsigned type_components = constructor_type->components();

		/* Number of components from parameters that have actually been
		* consumed.  This is used to perform several kinds of error checking.
		*/
		unsigned components_used = 0;

		unsigned matrix_parameters = 0;
		unsigned nonmatrix_parameters = 0;
		exec_list actual_parameters;

		foreach_list(n, &this->expressions)
		{
			ast_node *ast = exec_node_data(ast_node, n, link);
			ir_rvalue *result = ast->hir(instructions, state)->as_rvalue();

			/* From page 50 (page 56 of the PDF) of the GLSL 1.50 spec:
			*
			*    "It is an error to provide extra arguments beyond this
			*    last used argument."
			*/
			if (components_used >= type_components)
			{
				_mesa_glsl_error(&loc, state, "too many parameters to '%s' "
					"constructor",
					constructor_type->name);
				return ir_rvalue::error_value(ctx);
			}

			if (!result->type->is_numeric() && !result->type->is_boolean())
			{
				_mesa_glsl_error(&loc, state, "cannot construct '%s' from a "
					"non-numeric data type",
					constructor_type->name);
				return ir_rvalue::error_value(ctx);
			}

			/* Count the number of matrix and nonmatrix parameters.  This
			* is used below to enforce some of the constructor rules.
			*/
			if (result->type->is_matrix())
			{
				matrix_parameters++;
			}
			else
			{
				nonmatrix_parameters++;
			}

			actual_parameters.push_tail(result);
			components_used += result->type->components();
		}

		/* From page 28 (page 34 of the PDF) of the GLSL 1.10 spec:
		*
		*    "It is an error to construct matrices from other matrices. This
		*    is reserved for future use."
		*/
		if (state->language_version == 110 && matrix_parameters > 0
			&& constructor_type->is_matrix())
		{
			_mesa_glsl_error(&loc, state, "cannot construct '%s' from a "
				"matrix in GLSL 1.10",
				constructor_type->name);
			return ir_rvalue::error_value(ctx);
		}

		/* From page 50 (page 56 of the PDF) of the GLSL 1.50 spec:
		*
		*    "If a matrix argument is given to a matrix constructor, it is
		*    an error to have any other arguments."
		*/
		if ((matrix_parameters > 0)
			&& ((matrix_parameters + nonmatrix_parameters) > 1)
			&& constructor_type->is_matrix())
		{
			_mesa_glsl_error(&loc, state, "for matrix '%s' constructor, "
				"matrix must be only parameter",
				constructor_type->name);
			return ir_rvalue::error_value(ctx);
		}

		/* From page 28 (page 34 of the PDF) of the GLSL 1.10 spec:
		*
		*    "In these cases, there must be enough components provided in the
		*    arguments to provide an initializer for every component in the
		*    constructed value."
		*/
		if (components_used < type_components && components_used != 1
			&& matrix_parameters == 0)
		{
			_mesa_glsl_error(&loc, state, "too few components to construct "
				"'%s'",
				constructor_type->name);
			return ir_rvalue::error_value(ctx);
		}

		/* Later, we cast each parameter to the same base type as the
		* constructor.  Since there are no non-floating point matrices, we
		* need to break them up into a series of column vectors.
		*/
		if (!constructor_type->is_float())
		{
			foreach_list_safe(n, &actual_parameters)
			{
				ir_rvalue *matrix = (ir_rvalue *)n;

				if (!matrix->type->is_matrix())
				{
					continue;
				}

				/* Create a temporary containing the matrix. */
				ir_variable *var = new(ctx) ir_variable(matrix->type, "matrix_tmp",
					ir_var_temporary);
				instructions->push_tail(var);
				instructions->push_tail(new(ctx) ir_assignment(new(ctx)
					ir_dereference_variable(var), matrix, NULL));
				var->constant_value = matrix->constant_expression_value();

				/* Replace the matrix with dereferences of its columns. */
				for (int i = 0; i < matrix->type->matrix_columns; i++)
				{
					matrix->insert_before(new (ctx) ir_dereference_array(var,
						new(ctx) ir_constant(i)));
				}
				matrix->remove();
			}
		}

		bool all_parameters_are_constant = true;

		/* Type cast each parameter and, if possible, fold constants.*/
		foreach_list_safe(n, &actual_parameters)
		{
			ir_rvalue *ir = (ir_rvalue *)n;

			const glsl_type *desired_type =
				glsl_type::get_instance(constructor_type->base_type,
				ir->type->vector_elements,
				ir->type->matrix_columns);
			ir_rvalue *result = ir;
			apply_type_conversion(desired_type, result, instructions, state, false, &loc);

			/* Attempt to convert the parameter to a constant valued expression.
			* After doing so, track whether or not all the parameters to the
			* constructor are trivially constant valued expressions.
			*/
			ir_rvalue *const constant = result->constant_expression_value();

			if (constant != NULL)
			{
				result = constant;
			}
			else
			{
				all_parameters_are_constant = false;
			}

			if (result != ir)
			{
				ir->replace_with(result);
			}
		}

		/* If all of the parameters are trivially constant, create a
		* constant representing the complete collection of parameters.
		*/
		if (all_parameters_are_constant)
		{
			return new(ctx) ir_constant(constructor_type, &actual_parameters);
		}
		else if (constructor_type->is_scalar())
		{
			return dereference_component((ir_rvalue *)actual_parameters.head,
				0);
		}
		else if (constructor_type->is_vector())
		{
			return emit_inline_vector_constructor(constructor_type,
				instructions,
				&actual_parameters,
				ctx);
		}
		else
		{
			check(constructor_type->is_matrix());
			return emit_inline_matrix_constructor(constructor_type,
				instructions,
				&actual_parameters,
				ctx);
		}
	}
	else
	{
		const ast_expression *id = subexpressions[0];
		const char *func_name = id->primary_expression.identifier;
		YYLTYPE loc = this->get_location();
		exec_list actual_parameters;

		unsigned num_params = process_parameters(instructions, &actual_parameters,
			&this->expressions, state);

		ir_function_signature *sig = match_function_by_name(func_name, &actual_parameters, state);

		ir_call *call = NULL;
		ir_rvalue *value = NULL;

		if (sig == NULL)
		{
			if (num_params == 2 && strcmp(func_name, "mul") == 0)
			{
				value = process_mul(instructions, state, &actual_parameters, &loc);
			}
			else if (num_params == 1 && !strcmp(func_name, "length"))
			{
				// length(float X) => X
				auto* Instruction = (ir_instruction*)actual_parameters.iterator().get();
				auto* RValue = Instruction->as_rvalue();
				if (RValue && RValue->type->is_scalar())
				{
					value = RValue;
				}
			}
			if (value == NULL)
			{
				no_matching_function_error(func_name, &loc, &actual_parameters, state);
				value = ir_rvalue::error_value(ctx);
			}
		}
		else if (!verify_parameter_modes(state, sig, actual_parameters, this->expressions))
		{
			/* an error has already been emitted */
			value = ir_rvalue::error_value(ctx);
		}
		else
		{
			value = generate_call(instructions, sig, &loc, &actual_parameters,
				&call, state);
		}

		return value;
	}

	return ir_rvalue::error_value(ctx);
}

ir_texture_channel get_channel(const char * extension)
{
	ir_texture_channel channel = ir_channel_unknown;

	if (strcmp(extension, "Red") == 0)
	{
		channel = ir_channel_red;
	}
	else if (strcmp(extension, "Green") == 0)
	{
		channel = ir_channel_green;
	}
	else if (strcmp(extension, "Blue") == 0)
	{
		channel = ir_channel_blue;
	}
	else if (strcmp(extension, "Alpha") == 0)
	{
		channel = ir_channel_alpha;
	}
	else if (strlen(extension) == 0)
	{
		channel = ir_channel_none;
	}

	return channel;
}

ir_rvalue* gen_texture_op(
	const ast_expression *expr,
	ir_dereference *sampler,
	exec_list *instructions,
struct _mesa_glsl_parse_state *state)
{
	ir_rvalue* result = NULL;
	ast_expression *call = expr->subexpressions[1];
	void* ctx = state;
	YYLTYPE loc = expr->get_location();

	check(call->oper == ast_function_call);
	check(sampler);
	check(sampler->type->is_sampler());

	// Generate the function name and see if it already exists.
	const char *method = call->subexpressions[0]->primary_expression.identifier;

	// Process parameters. The sampler state is always the first parameter.
	ir_rvalue* parameters[8] = { 0 };
	exec_list param_list;
	int num_params = process_parameters(instructions, &param_list, &call->expressions, state);
	{
		int i = 0;
		foreach_iter(exec_list_iterator, iter, param_list)
		{
			parameters[i++] = ((ir_instruction*)iter.get())->as_rvalue();
		}
	}

	ir_rvalue* SamplerStateDeRef = NULL;

	// See if the first parameter was a sampler state type.
	ir_variable* sampler_var = sampler->variable_referenced();
	if (parameters[0] && parameters[0]->type->base_type == GLSL_TYPE_SAMPLER_STATE)
	{
		// Flag a texture once it has been sampled with a shadow sampler state.
		if (sampler_var->has_been_sampled == false
			&& sampler_var->type->sampler_shadow == false
			&& parameters[0]->type == glsl_type::sampler_cmp_state_type)
		{
			const glsl_type* shadow_sampler_type = sampler_var->type->get_shadow_sampler_type();
			if (shadow_sampler_type)
			{
				sampler_var->type = shadow_sampler_type;
				sampler->type = shadow_sampler_type;
			}
		}
		sampler_var->has_been_sampled = true;
		SamplerStateDeRef = parameters[0];

		if ((sampler->type->sampler_shadow == true && parameters[0]->type == glsl_type::sampler_state_type)
			|| (sampler->type->sampler_shadow == false && parameters[0]->type == glsl_type::sampler_cmp_state_type))
		{
			struct YYLTYPE location = expr->get_location();
			_mesa_glsl_error(&location, state, "Texture '%s' may "
				"not be sampled by both a SamplerState and in "
				"SamplerComparisonState the same shader.\n", sampler_var->name);
			return ir_rvalue::error_value(ctx);
		}
	}
	const bool is_shadow = sampler->type->sampler_shadow;
	const bool is_multisample = sampler->type->sampler_ms;
	bool gather = false;

	// Create a texture op.
	ir_texture* texop = NULL;
	SSourceLocation SourceLocation = expr->GetSourceLocation(state);
	if (!is_multisample && !is_shadow && num_params >= 2 && strcmp(method, "Sample") == 0)
	{
		texop = new(ctx) ir_texture(ir_tex, SourceLocation);
		texop->coordinate = parameters[1];
		texop->offset = parameters[2];
	}
	else if (!is_multisample && !is_shadow && num_params >= 3 && strcmp(method, "SampleBias") == 0)
	{
		texop = new(ctx) ir_texture(ir_txb, SourceLocation);
		texop->coordinate = parameters[1];
		texop->lod_info.bias = parameters[2];
		texop->offset = parameters[3];
	}
	else if (!is_multisample && !is_shadow && num_params >= 3 && strcmp(method, "SampleLevel") == 0)
	{
		texop = new(ctx) ir_texture(ir_txl, SourceLocation);
		texop->coordinate = parameters[1];
		texop->lod_info.lod = parameters[2];
		texop->offset = parameters[3];
	}
	else if (!is_multisample && !is_shadow && num_params >= 4 && strcmp(method, "SampleGrad") == 0)
	{
		texop = new(ctx) ir_texture(ir_txd, SourceLocation);
		texop->coordinate = parameters[1];
		texop->lod_info.grad.dPdx = parameters[2];
		texop->lod_info.grad.dPdy = parameters[3];
		texop->offset = parameters[4];
	}
	else if (!is_multisample && is_shadow && num_params >= 3 && strcmp(method, "SampleCmp") == 0)
	{
		texop = new(ctx) ir_texture(ir_tex, SourceLocation);
		texop->coordinate = parameters[1];
		texop->shadow_comparitor = parameters[2];
		texop->offset = parameters[3];
	}
	else if (!is_multisample && is_shadow && num_params >= 3 && strcmp(method, "SampleCmpLevelZero") == 0)
	{
		texop = new(ctx) ir_texture(ir_txl, SourceLocation);
		texop->coordinate = parameters[1];
		texop->shadow_comparitor = parameters[2];
		texop->offset = parameters[3];
		texop->lod_info.lod = new(ctx) ir_constant(0.0f);
	}
	else if (is_multisample && num_params >= 2 && strcmp(method, "Load") == 0)
	{
		texop = new(ctx) ir_texture(ir_txf, SourceLocation);
		texop->coordinate = new(ctx) ir_swizzle(parameters[0], 0, 1, 0, 0, 2); // .xy
		apply_type_conversion(glsl_type::int_type, parameters[1], instructions, state, false, &loc);
		texop->lod_info.sample_index = parameters[1];
	}
	else if (!is_multisample && num_params >= 1 && strcmp(method, "Load") == 0)
	{
		const int dimensions[2][6] =
		{
			{ 1, 2, 3, 3, 0, 1 }, { 2, 3, 4, 4, 0, 0 }
		};
		check(parameters[0]);
		ir_variable* param0 = new(ctx) ir_variable(parameters[0]->type, NULL, ir_var_temporary);
		instructions->push_tail(param0);
		instructions->push_tail(new(ctx) ir_assignment(
			new(ctx) ir_dereference_variable(param0), parameters[0]));

		texop = new(ctx) ir_texture(ir_txf, SourceLocation);
		const int dim = dimensions[sampler->type->sampler_array][sampler->type->sampler_dimensionality];
		texop->coordinate = new(ctx) ir_swizzle(new(ctx) ir_dereference_variable(param0), 0, 1, 2, 3, dim);
		if (!sampler->type->sampler_buffer)
		{
			texop->lod_info.lod = new(ctx) ir_swizzle(new(ctx) ir_dereference_variable(param0), dim, 0, 0, 0, 1);
			apply_type_conversion(glsl_type::int_type, texop->lod_info.lod, instructions, state, false, &loc);
			texop->offset = parameters[1];
		}
		else
		{
			texop->lod_info.lod = NULL;
			texop->offset = NULL;
		}
	}
	else if (!is_multisample && is_shadow && num_params >= 3 && strncmp(method, "GatherCmp", 9) == 0 && state->language_version >= 310)
	{
		const char* extension = method + 9;
		ir_texture_channel channel = get_channel(extension);
		gather = true;

		if (channel == ir_channel_unknown)
		{
			struct YYLTYPE location = expr->get_location();
			_mesa_glsl_error(&location, state, "Unsupported method '%s' called on '%s' of type '%s'.\n",
				method,
				sampler_var->name,
				sampler_var->type->name);
			result = ir_rvalue::error_value(ctx);
		}
		else if (channel != ir_channel_none)
		{
			struct YYLTYPE location = expr->get_location();
			_mesa_glsl_error(&location, state, "GatherCmp not supported with channel selection in OpenGL\n");
			result = ir_rvalue::error_value(ctx);
		}
		else
		{
			texop = new(ctx) ir_texture(ir_txg, SourceLocation);
			texop->coordinate = parameters[1];
			texop->shadow_comparitor = parameters[2];
			texop->offset = parameters[3];
			texop->channel = channel;
		}
	}
	else if (!is_multisample && !is_shadow && num_params >= 2 && strncmp(method, "Gather", 6) == 0 && state->language_version >= 310)
	{
		const char* extension = method + 6;
		ir_texture_channel channel = get_channel(extension);
		gather = true;

		if (channel == ir_channel_unknown)
		{
			struct YYLTYPE location = expr->get_location();
			_mesa_glsl_error(&location, state, "Unsupported method '%s' called on '%s' of type '%s'.\n",
				method,
				sampler_var->name,
				sampler_var->type->name);
			result = ir_rvalue::error_value(ctx);
		}
		else
		{
			texop = new(ctx) ir_texture(ir_txg, SourceLocation);
			texop->coordinate = parameters[1];
			texop->offset = parameters[2];
			texop->channel = channel;
		}
	}
	else if (strcmp(method, "GetDimensions") == 0)
	{
		struct YYLTYPE location = expr->get_location();
		int dimensions = 0;
		switch (sampler->type->sampler_dimensionality)
		{
		case GLSL_SAMPLER_DIM_1D:
			dimensions = 1;
			break;
		case GLSL_SAMPLER_DIM_2D:
		case GLSL_SAMPLER_DIM_CUBE:
			dimensions = 2;
			break;
		case GLSL_SAMPLER_DIM_3D:
			dimensions = 3;
			break;
		default:
			//unsupported sampler dimension
			_mesa_glsl_error(&location, state, "GetDimensions called on unsupported sampler type %s'.\n",
				sampler->type->name);
			return NULL;
		}

		dimensions += sampler->type->sampler_array;

		// GetDimensions must either have the number of parameters that matches the number of dimensions
		// or it must match the number of dimensions + 2 (one for mip level and one for mipcount)
		// GetDimensions further must exactly match parameter count if it is a multisampled texture
		if ((!sampler->type->sampler_ms && (num_params != dimensions && num_params != dimensions + 2))
			|| (sampler->type->sampler_ms && (num_params != dimensions + 1)))
		{
			// Incorrect number of parameters
			_mesa_glsl_error(&location, state, "GetDimensions called with incorrect number of parameters'.\n");
			return NULL;
		}

		int param_index = 0;
		texop = new(ctx) ir_texture(ir_txs, SourceLocation);
		if (sampler->type->sampler_ms || num_params == dimensions)
		{
			texop->lod_info.lod = new(ctx) ir_constant(0);
		}
		else
		{
			apply_type_conversion(glsl_type::int_type, parameters[param_index], instructions, state, false, &loc);
			texop->lod_info.lod = parameters[param_index++];
		}

		// ir_txs is special. Rather than returning the output directly, 
		// generate an intermediate value to hold the return and output to the
		// necessary derefs.
		texop->type = glsl_type::get_instance(GLSL_TYPE_INT, dimensions, 1);
		ir_variable* txs_return = new(ctx) ir_variable(texop->type, NULL, ir_var_temporary);
		ir_assignment* txs_return_assign = new(ctx) ir_assignment(new(ctx) ir_dereference_variable(txs_return), texop);
		instructions->push_tail(txs_return);
		instructions->push_tail(txs_return_assign);

		// Assign the outputs.
		int component_index = 0;
		while (param_index < num_params && component_index < texop->type->components())
		{
			ir_rvalue* lhs = parameters[param_index++];
			check(lhs);

			ir_rvalue* rhs = new (ctx) ir_swizzle(
				new(ctx) ir_dereference_variable(txs_return),
				component_index++, 0, 0, 0, 1);

			apply_type_conversion(lhs->type, rhs, instructions, state, false, &loc);

			instructions->push_tail(new(ctx) ir_assignment(lhs, rhs));
		}

		// For multisampled textures, return sample information defined in texture type
		if (sampler->type->sampler_ms)
		{
			ir_rvalue* lhs = parameters[param_index++];
			check(lhs);

			ir_rvalue* rhs = new(ctx) ir_constant(sampler->type->sample_count);
			apply_type_conversion(lhs->type, rhs, instructions, state, false, &loc);
			instructions->push_tail(new(ctx) ir_assignment(lhs, rhs));
		}

		if (state->language_version >= 310 && num_params == dimensions + 2)
		{
			// generate one extra txm instruction to query levels and assign
			ir_texture *query = new(ctx) ir_texture(ir_txm, SourceLocation);
			query->lod_info.lod = new(ctx) ir_constant(0);
			query->type = glsl_type::int_type;
			query->sampler = sampler->clone(ctx, 0); //clone the sampler, because it is a dereference

			ir_variable* txm_return = new(ctx) ir_variable(query->type, NULL, ir_var_temporary);
			ir_assignment* txm_return_assign = new(ctx) ir_assignment(new(ctx) ir_dereference_variable(txm_return), query);
			instructions->push_tail(txm_return);
			instructions->push_tail(txm_return_assign);

			ir_rvalue* lhs = parameters[param_index++];
			check(lhs);

			ir_rvalue* rhs = new(ctx) ir_dereference_variable(txm_return);

			apply_type_conversion(lhs->type, rhs, instructions, state, false, &loc);

			instructions->push_tail(new(ctx) ir_assignment(lhs, rhs));
		}

		// Assign remaining outputs to be constant 0.
		while (param_index < num_params)
		{
			ir_rvalue* lhs = parameters[param_index++];
			ir_rvalue* rhs = new(ctx) ir_constant((unsigned int)0);

			apply_type_conversion(lhs->type, rhs, instructions, state, false, &loc);

			instructions->push_tail(new(ctx) ir_assignment(lhs, rhs));
		}

		// GetDimensions doesn't return anything.
		result = NULL;
	}
	else
	{
		char* errmsg = ralloc_asprintf(ctx, "Unsupported method '%s(", method);
		for (int i = 0; i < num_params; ++i)
		{
			if (parameters[i])
			{
				ralloc_asprintf_append(&errmsg, "%s%s", (i == 0) ? "" : ",",
					parameters[i]->type->name);
			}
		}
		struct YYLTYPE location = expr->get_location();
		_mesa_glsl_error(&location, state, "%s)' called on '%s' of type '%s'.\n",
			errmsg,
			sampler_var->name,
			sampler_var->type->name);
		result = ir_rvalue::error_value(ctx);
	}

	if (texop)
	{
		texop->sampler = sampler;
		if (SamplerStateDeRef)
		{
			texop->SamplerState = SamplerStateDeRef->clone(ctx, NULL);
			auto* VarDeRef = SamplerStateDeRef->as_dereference_variable();
			if (VarDeRef && VarDeRef->var && VarDeRef->var->name)
			{
				texop->SamplerStateName = ralloc_strdup(ctx, VarDeRef->var->name);
			}
		}

		// txs sets its own return type. Which is void, so it doesn't need to be converted.
		if (texop->op != ir_txs)
		{
			// Set texop type to type which would be returned by appropriate GLSL function
			texop->type = glsl_type::get_instance(sampler->type->inner_type->base_type, is_shadow && !gather ? 1 : 4, 1);
			result = texop;

			// Make sure this type gets automatically converted, if HLSL's inner type was something else (for example Texture2DMS<float>).
			struct YYLTYPE location = expr->get_location();
			apply_type_conversion(sampler->type->inner_type, result, instructions, state, true, &location);
		}

		if (texop->op != ir_txf && texop->op != ir_txs)
		{
			const glsl_type* coord_type[2][4] =
			{
				{
					glsl_type::float_type,
					glsl_type::vec2_type,
					glsl_type::vec3_type,
					glsl_type::vec3_type
				},
				{
					glsl_type::vec2_type,
					glsl_type::vec3_type,
					glsl_type::vec4_type,
					glsl_type::vec4_type
				}
			};

			const glsl_type* dest_type = coord_type[sampler->type->sampler_array][sampler->type->sampler_dimensionality];
			if (texop->coordinate && texop->coordinate->type->base_type == GLSL_TYPE_HALF)
			{
				dest_type = glsl_type::get_instance(GLSL_TYPE_HALF, dest_type->vector_elements, 1);
			}

			struct YYLTYPE location = expr->get_location();
			apply_type_conversion(dest_type, texop->coordinate, instructions, state, false, &location);
		}
		else if (texop->op == ir_txf)
		{
			/** Texel fetch instructions must have an integer coordinate */
			const glsl_type* coord_type[2][4] =
			{
				{
					glsl_type::int_type,
					glsl_type::ivec2_type,
					glsl_type::ivec3_type,
					glsl_type::ivec3_type
				},
				{
					glsl_type::ivec2_type,
					glsl_type::ivec3_type,
					glsl_type::ivec4_type,
					glsl_type::ivec4_type
				}
			};

			struct YYLTYPE location = expr->get_location();
			apply_type_conversion(
				coord_type[sampler->type->sampler_array][sampler->type->sampler_dimensionality],
				texop->coordinate, instructions, state, false, &location);
		}

		if (texop->offset)
		{
			// Properly converts offset to dimension of the texture
			const glsl_type* OffsetType[GLSL_SAMPLER_DIM_EXTERNAL] =
			{
				glsl_type::int_type,
				glsl_type::ivec2_type,
				glsl_type::ivec3_type,
				nullptr,//GLSL_SAMPLER_DIM_CUBE,
				nullptr,//GLSL_SAMPLER_DIM_RECT,
				nullptr,//GLSL_SAMPLER_DIM_BUF,
			};

			if (OffsetType[sampler->type->sampler_dimensionality])
			{
				struct YYLTYPE location = expr->get_location();
				apply_type_conversion(OffsetType[sampler->type->sampler_dimensionality], texop->offset,
					instructions, state, false, &location);
			}
		}

		if (texop->op == ir_txb)
		{
			// bias parameters must always be scalar floats
			struct YYLTYPE location = expr->get_location();
			apply_type_conversion(glsl_type::float_type,texop->lod_info.bias, instructions, state,
				false, &location);
		}

		if (texop->op == ir_txl)
		{
			// lod parameters must always be scalar floats
			struct YYLTYPE location = expr->get_location();
			apply_type_conversion(glsl_type::float_type,texop->lod_info.lod, instructions, state,
				false, &location);
		}

		if (texop->shadow_comparitor)
		{
			//shadow comparitors must always be scalar floats
			struct YYLTYPE location = expr->get_location();
			apply_type_conversion(glsl_type::float_type,texop->shadow_comparitor, instructions, state,
				false, &location);
		}

		if (texop->op == ir_txd)
		{
			// Properly converts gradients to floating point values matching dimensionality
			const glsl_type* OffsetType[GLSL_SAMPLER_DIM_EXTERNAL] =
			{
				glsl_type::float_type,
				glsl_type::vec2_type,
				glsl_type::vec3_type,
				glsl_type::vec3_type,
				nullptr,//GLSL_SAMPLER_DIM_RECT,
				nullptr,//GLSL_SAMPLER_DIM_BUF,
			};

			if (OffsetType[sampler->type->sampler_dimensionality])
			{
				struct YYLTYPE location = expr->get_location();
				apply_type_conversion(OffsetType[sampler->type->sampler_dimensionality], texop->lod_info.grad.dPdx,
					instructions, state, false, &location);
				apply_type_conversion(OffsetType[sampler->type->sampler_dimensionality], texop->lod_info.grad.dPdy,
					instructions, state, false, &location);
			}
		}
	}

	return result;
}

ir_rvalue* gen_image_op(
	const ast_expression *expr,
	ir_dereference *image,
	exec_list *instructions,
	struct _mesa_glsl_parse_state *state)
{
	ir_rvalue* result = NULL;
	ast_expression *call = expr->subexpressions[1];
	void* ctx = state;
	YYLTYPE loc = expr->get_location();

	check(call->oper == ast_function_call);
	check(image);
	check(image->type->is_image());

	// Generate the function name and see if it already exists.
	const char *method = call->subexpressions[0]->primary_expression.identifier;

	// Process parameters. The texture is always the first parameter.
	ir_rvalue* parameters[8] = { 0 };
	exec_list param_list;
	int num_params = process_parameters(instructions, &param_list, &call->expressions, state);
	{
		int i = 0;
		foreach_iter(exec_list_iterator, iter, param_list)
		{
			check(i < 8);
			parameters[i++] = ((ir_instruction*)iter.get())->as_rvalue();
		}
	}

	if (strcmp(method, "GetDimensions") == 0)
	{
		ir_dereference_image *imageop = new(ctx) ir_dereference_image(image, new(ctx) ir_constant(0.0f), ir_image_dimensions);
		//@todo-rco: ?
		//image->variable_referenced()->type;

		// GetDimensions doesn't return anything, it will be overridden if there is an error
		result = NULL;

		int dimensions = 0;
		switch (image->type->sampler_dimensionality)
		{
		case GLSL_SAMPLER_DIM_1D:
			dimensions = 1;
			break;
		case GLSL_SAMPLER_DIM_2D:
			dimensions = 2;
			break;
		case GLSL_SAMPLER_DIM_3D:
			dimensions = 3;
			break;
		default:
			check(0); //bad sampler dimension
		}

		dimensions += image->type->sampler_array;

		if (num_params != dimensions)
		{
			struct YYLTYPE location = expr->get_location();
			_mesa_glsl_error(&location, state, "GetDimensions called with incorrect number of parameters. (expected %d)\n",
				dimensions);
			result = ir_rvalue::error_value(ctx);
		}
		else
		{
			const glsl_type *res_type = glsl_type::get_instance(GLSL_TYPE_INT, dimensions, 1);
			imageop->type = res_type;
			ir_variable* dim_return = new(ctx) ir_variable(res_type, NULL, ir_var_temporary);
			ir_assignment* dim_return_assign = new(ctx) ir_assignment(new(ctx) ir_dereference_variable(dim_return), imageop);
			instructions->push_tail(dim_return);
			instructions->push_tail(dim_return_assign);

			// Assign the outputs.
			int component_index = 0;
			int param_index = 0;
			while (param_index < num_params && component_index < res_type->components())
			{
				ir_rvalue* lhs = parameters[param_index++];
				check(lhs);

				if (!lhs->is_lvalue())
				{
					struct YYLTYPE location = expr->get_location();
					_mesa_glsl_error(&location, state, " Parameters to GetDimensions must be lvalues\n");
					result = ir_rvalue::error_value(ctx);
				}
				else
				{
					ir_rvalue* rhs = new (ctx) ir_swizzle(
						new(ctx) ir_dereference_variable(dim_return),
						component_index++, 0, 0, 0, 1);

					apply_type_conversion(lhs->type, rhs, instructions, state, false, &loc);

					instructions->push_tail(new(ctx) ir_assignment(lhs, rhs));
				}
			}
		}
	}
	else
	{
		char* errmsg = ralloc_asprintf(ctx, "Unsupported method '%s(", method);
		for (int i = 0; i < num_params; ++i)
		{
			if (parameters[i])
			{
				ralloc_asprintf_append(&errmsg, "%s%s", (i == 0) ? "" : ",",
					parameters[i]->type->name);
			}
		}
		struct YYLTYPE location = expr->get_location();
		ir_variable *var = image->variable_referenced();
		_mesa_glsl_error(&location, state, "%s)' called on '%s' of type '%s'.\n",
			errmsg,
			var->name,
			var->type->name);
		result = ir_rvalue::error_value(ctx);
	}


	return result;
}
