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
#include "ir.h"
#include "symbol_table.h"
#include "glsl_parser_extras.h"
#include "ast.h"
#include "glsl_types.h"

ir_rvalue * _mesa_ast_field_selection_to_hir(const ast_expression *expr,
	exec_list *instructions, struct _mesa_glsl_parse_state *state)
{
	void *ctx = state;
	ir_rvalue *result = NULL;
	ir_rvalue *op;

	op = expr->subexpressions[0]->hir(instructions, state);

	/* There are two kinds of field selection.  There is the selection of a
	* specific field from a structure, and there is the selection of a
	* swizzle / mask from a vector.  Which is which is determined entirely
	* by the base type of the thing to which the field selection operator is
	* being applied.
	*/
	YYLTYPE loc = expr->get_location();
	if (op->type->is_error())
	{
		/* silently propagate the error */
	}
	else if (op->type->is_vector() || op->type->is_scalar())
	{
		ir_swizzle *swiz = ir_swizzle::create(op,
			expr->primary_expression.identifier,
			op->type->vector_elements);
		if (swiz != NULL)
		{
			result = swiz;
		}
		else
		{
			/* FINISHME: Logging of error messages should be moved into
			* FINISHME: ir_swizzle::create.  This allows the generation of more
			* FINISHME: specific error messages.
			*/
			_mesa_glsl_error(& loc, state, "Invalid swizzle / mask '%s'",
				expr->primary_expression.identifier);
		}
	}
	else if (op->type->is_matrix() && expr->primary_expression.identifier)
	{
		int src_components = op->type->components();
		int components[4] = {0};
		uint32 num_components = 0;
		ir_swizzle_mask mask = {0};
		const char* mask_str = expr->primary_expression.identifier;
		if (mask_str[0] == '_' && mask_str[1] == 'm')
		{
			do
			{
				mask_str += 2;
				int col = (*mask_str) ? (*mask_str++) - '0' : -1;
				int row = (*mask_str) ? (*mask_str++) - '0' : -1;

				if (col >= 0 && col <= op->type->matrix_columns &&
					row >= 0 && row <= op->type->vector_elements)
				{
					components[num_components++] = col * op->type->vector_elements + row;
				}
				else
				{
					components[num_components++] = -1;
				}
			} while (*mask_str != 0 && num_components < 4);
		}
		else if (mask_str[0] == '_' && mask_str[1] >= '1' && mask_str[2] <= '4')
		{
			do
			{
				mask_str += 1;
				int col = (*mask_str) ? (*mask_str++) - '1' : -1;
				int row = (*mask_str) ? (*mask_str++) - '1' : -1;

				if (col >= 0 && col <= op->type->matrix_columns &&
					row >= 0 && row <= op->type->vector_elements)
				{
					components[num_components++] = col * op->type->vector_elements + row;
				}
				else
				{
					components[num_components++] = -1;
				}
			} while (*mask_str != 0 && num_components < 4);
		}

		if (*mask_str == 0)
		{
			if (num_components > 0 && components[0] >= 0 && components[0] <= src_components)
			{
				mask.x = (unsigned)components[0];
				mask.num_components++;
			}
			if (num_components > 1 && components[1] >= 0 && components[1] <= src_components)
			{
				mask.y = (unsigned)components[1];
				mask.has_duplicates = (mask.y == mask.x);
				mask.num_components++;
			}
			if (num_components > 2 && components[2] >= 0 && components[2] <= src_components)
			{
				mask.z = (unsigned)components[2];
				mask.has_duplicates = mask.has_duplicates || (mask.z == mask.y) || (mask.z == mask.x);
				mask.num_components++;
			}
			if (num_components > 3 && components[3] >= 0 && components[3] <= src_components)
			{
				mask.w = (unsigned)components[3];
				mask.has_duplicates = mask.has_duplicates || (mask.w == mask.z) ||
					(mask.w == mask.y) || (mask.w == mask.x);
				mask.num_components++;
			}
		}

		if (mask.num_components == num_components)
		{
			result = new(ctx)ir_swizzle(op, mask);
		}

		if (result == NULL)
		{
			_mesa_glsl_error(&loc, state, "invalid matrix swizzle '%s'",
				expr->primary_expression.identifier);
		}
	}
	else if (op->type->base_type == GLSL_TYPE_STRUCT)
	{
		result = new(ctx)ir_dereference_record(op,
			expr->primary_expression.identifier);

		if (result->type->is_error())
		{
			_mesa_glsl_error(& loc, state, "Cannot access field '%s' of "
				"structure",
				expr->primary_expression.identifier);
		}
	}
	else if (expr->subexpressions[1] != NULL)
	{
		/* Handle "method calls" in GLSL 1.20+ */
		if (state->language_version < 120)
			_mesa_glsl_error(&loc, state, "Methods not supported in GLSL 1.10.");

		ast_expression *call = expr->subexpressions[1];
		check(call->oper == ast_function_call);

		const char *method;
		method = call->subexpressions[0]->primary_expression.identifier;

		if (op->type->is_array() && strcmp(method, "length") == 0)
		{
			if (!call->expressions.is_empty())
				_mesa_glsl_error(&loc, state, "length method takes no arguments.");

			if (op->type->array_size() == 0)
				_mesa_glsl_error(&loc, state, "length called on unsized array.");

			result = new(ctx)ir_constant(op->type->array_size());
		}
		else if (op->type->is_sampler() && op->as_dereference() != NULL)
		{
			return gen_texture_op(expr, op->as_dereference(), instructions, state);
		}
		else if (op->type->is_image() && op->as_dereference() != NULL)
		{
			return gen_image_op(expr, op->as_dereference(), instructions, state);
		}
		else if (op->type->is_outputstream() && strcmp(method, "Append") == 0)
		{
			check(op->variable_referenced()->type->inner_type->is_record());
			check(op->variable_referenced()->type->inner_type->name);

			const char* function_name = "OutputStream_Append";

			ir_function *func = state->symbols->get_function(function_name);
			if (!func)
			{
				// Prepare the function, add it to global symbols. It will be added to declarations at GenerateGlslMain().
				func = new(ctx)ir_function(function_name);
				state->symbols->add_global_function(func);

				//				_mesa_glsl_warning(state, "Append function generation for type '%s'", op->variable_referenced()->type->inner_type->name );
				//				{
				//					const glsl_type* output_type = op->variable_referenced()->type->inner_type;
				//					for (int i = 0; i < output_type->length; i++)
				//					{
				//						_mesa_glsl_warning(state, "   name '%s' : semantic '%s'", output_type->fields.structure[i].name, output_type->fields.structure[i].semantic );
				//					}
				//				}
			}

			exec_list comparison_parameter;
			ir_variable* var = new(ctx)ir_variable(op->variable_referenced()->type->inner_type, ralloc_asprintf(ctx, "arg0"), ir_var_in);
			comparison_parameter.push_tail(var);

			bool is_exact = false;
			ir_function_signature *sig = func->matching_signature(&comparison_parameter, &is_exact);
			if (!sig || !is_exact)
			{
				sig = new(ctx)ir_function_signature(glsl_type::void_type);
				sig->parameters.push_tail(var);
				sig->is_builtin = false;
				sig->is_defined = true;
				func->add_signature(sig);
			}

			if (call->expressions.is_empty() || (call->expressions.get_head() != call->expressions.get_tail()))
			{
				_mesa_glsl_error(&loc, state, "Append method takes one argument.");
			}
			else
			{
				exec_list actual_parameter;
				ast_node *const ast = exec_node_data(ast_node, call->expressions.get_head(), link);
				ir_rvalue *result = ast->hir(instructions, state);
				actual_parameter.push_tail(result);

				instructions->push_tail(new(ctx)ir_call(sig, NULL, &actual_parameter));
			}

			return NULL;
		}
		else if (op->type->is_outputstream() && strcmp(method, "RestartStrip") == 0)
		{
			exec_list actual_parameters;	// empty, as no parameters

			ir_function *func = state->symbols->get_function("EndPrimitive");
			check(func);

			bool is_exact = false;
			ir_function_signature *sig = func->matching_signature(&actual_parameters, &is_exact);
			check(sig && is_exact);

			instructions->push_tail(new(ctx)ir_call(sig, NULL, &actual_parameters));

			return NULL;
		}
		else
		{
			_mesa_glsl_error(&loc, state, "Unknown method: '%s'.", method);
		}
	}
	else
	{
		_mesa_glsl_error(& loc, state, "Cannot access field '%s' of "
			"non-structure / non-vector.",
			expr->primary_expression.identifier);
	}

	return result ? result : ir_rvalue::error_value(ctx);
}
