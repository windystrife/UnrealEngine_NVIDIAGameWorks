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

/**
 * \file ir_validate.cpp
 *
 * Attempts to verify that various invariants of the IR tree are true.
 *
 * In particular, at the moment it makes sure that no single
 * ir_instruction node except for ir_variable appears multiple times
 * in the ir tree.  ir_variable does appear multiple times: Once as a
 * declaration in an exec_list, and multiple times as the endpoint of
 * a dereference chain.
 */

#include "ShaderCompilerCommon.h"
#include "ir.h"
#include "ir_hierarchical_visitor.h"
#include "hash_table.h"
#include "glsl_types.h"
#include "glsl_parser_extras.h"
#include "LanguageSpec.h"

class ir_validate : public ir_hierarchical_visitor
{
public:

	ir_validate(_mesa_glsl_parse_state *in_state)
		: state(in_state)
	{
		this->ht = hash_table_ctor(0, hash_table_pointer_hash,
			hash_table_pointer_compare);

		this->current_function = NULL;

		this->callback = ir_validate::validate_ir;
		this->data = this;
	}

	~ir_validate()
	{
		hash_table_dtor(this->ht);
	}

	virtual ir_visitor_status visit(ir_variable *v);
	virtual ir_visitor_status visit(ir_dereference_variable *ir);
	virtual ir_visitor_status visit(ir_constant *ir);

	virtual ir_visitor_status visit_enter(ir_if *ir);

	virtual ir_visitor_status visit_leave(ir_loop *ir);
	virtual ir_visitor_status visit_enter(ir_function *ir);
	virtual ir_visitor_status visit_leave(ir_function *ir);
	virtual ir_visitor_status visit_enter(ir_function_signature *ir);

	virtual ir_visitor_status visit_leave(ir_expression *ir);
	virtual ir_visitor_status visit_leave(ir_swizzle *ir);

	virtual ir_visitor_status visit_enter(ir_assignment *ir);
	virtual ir_visitor_status visit_enter(ir_call *ir);

	static void validate_ir(ir_instruction *ir, void *data);

	ir_function *current_function;
	struct hash_table *ht;
	_mesa_glsl_parse_state *state;
};


ir_visitor_status ir_validate::visit(ir_dereference_variable *ir)
{
	if ((ir->var == NULL) || (ir->var->as_variable() == NULL))
	{
		_mesa_glsl_error(state,
			"internal compiler error: ir_dereference_variable @ %d %p does not "
			"specify a variable %p",
			ir->id,
			(void *) ir,
			(void *) ir->var
			);
	}

	if (hash_table_find(ht, ir->var) == NULL &&
		// uniform block variables are not explicitly declared -- that is ok.
		!(ir->var->mode == ir_var_uniform && ir->var->semantic != NULL)) 
	{
		_mesa_glsl_error(state,
			"internal compiler error: ir_dereference_variable @ %d %p specifies "
			"undeclared variable '%s' @ %d %p",
			ir->id,
			(void *) ir,
			ir->var->name,
			ir->var->id,
			(void *) ir->var
			);
	}

	this->validate_ir(ir, this->data);

	return visit_continue;
}


ir_visitor_status ir_validate::visit(ir_constant *ir)
{
/*
	if (!ir->is_finite())
	{
		_mesa_glsl_error(state, "internal compiler error: NaN/INF constant");
	}
*/
	return visit_continue;
}


ir_visitor_status ir_validate::visit_enter(ir_if *ir)
{
	if (ir->condition->type != glsl_type::bool_type)
	{
		_mesa_glsl_error(state,
			"internal compiler error: ir_if condition %s type instead of bool.",
			ir->condition->type->name
			);
	}

	return visit_continue;
}


ir_visitor_status ir_validate::visit_leave(ir_loop *ir)
{
	if (ir->counter != NULL)
	{
		if ((ir->from == NULL) || (ir->to == NULL) || (ir->increment == NULL))
		{
			_mesa_glsl_error(state,
				"internal compiler error: ir_loop has invalid loop controls:\n"
				"    counter:   %p\n"
				"    from:      %p\n"
				"    to:        %p\n"
				"    increment: %p",
				(void *) ir->counter,
				(void *) ir->from,
				(void *) ir->to,
				(void *) ir->increment
				);
		}

		if ((ir->cmp < ir_binop_less) || (ir->cmp > ir_binop_nequal))
		{
			_mesa_glsl_error(state,
				"internal compiler error: ir_loop has invalid comparitor %d",
				ir->cmp
				);
		}
	}
	else
	{
		if ((ir->from != NULL) || (ir->to != NULL) || (ir->increment != NULL))
		{
			_mesa_glsl_error(state,
				"internal compiler error: ir_loop has invalid loop controls:\n"
				"    counter:   %p\n"
				"    from:      %p\n"
				"    to:        %p\n"
				"    increment: %p",
				(void *) ir->counter,
				(void *) ir->from,
				(void *) ir->to,
				(void *) ir->increment
				);
		}
	}

	return visit_continue;
}


ir_visitor_status ir_validate::visit_enter(ir_function *ir)
{
	/* Function definitions cannot be nested.
	*/
	if (this->current_function != NULL)
	{
		_mesa_glsl_error(state,
			"internal compiler error: Function definition nested inside another "
			"function definition: %s %p inside %s %p",
			ir->name,
			(void *) ir,
			this->current_function->name,
			(void *) this->current_function
			);
	}

	/* Store the current function hierarchy being traversed.  This is used
	* by the function signature visitor to ensure that the signatures are
	* linked with the correct functions.
	*/
	this->current_function = ir;

	this->validate_ir(ir, this->data);

	/* Verify that all of the things stored in the list of signatures are,
	* in fact, function signatures.
	*/
	foreach_list(node, &ir->signatures)
	{
		ir_instruction *sig = (ir_instruction *) node;

		if (sig->ir_type != ir_type_function_signature)
		{
			_mesa_glsl_error(state,
				"internal compiler error: Non-signature in signature list of function '%s'",
				ir->name
				);
		}
	}

	return visit_continue;
}

ir_visitor_status ir_validate::visit_leave(ir_function *ir)
{
	check(ralloc_parent(ir->name) == ir);

	this->current_function = NULL;
	return visit_continue;
}

ir_visitor_status ir_validate::visit_enter(ir_function_signature *ir)
{
	if (this->current_function != ir->function())
	{
		_mesa_glsl_error(state,
			"internal compiler error: Function signature nested inside wrong "
			"function definition: %p inside %s %p instead of %s %p",
			(void *) ir,
			this->current_function->name,
			(void *) this->current_function,
			ir->function_name(),
			(void *) ir->function()
			);
	}

	if (ir->return_type == NULL)
	{
		_mesa_glsl_error(state,
			"internal compiler error: Function signature %p for function %s has NULL return type.",
			(void *) ir,
			ir->function_name()
			);
	}

	this->validate_ir(ir, this->data);

	return visit_continue;
}

static void validate_expr_error(
	_mesa_glsl_parse_state *state,
	ir_expression *expr,
	const char *validation_text
	)
{
	_mesa_glsl_error(state,
		"internal compiler error: '%s' @ %d: operation '%s' Types: Result %s, op[0] %s, op[1] %s, op[2] %s",
		validation_text,
		expr->id,
		expr->operator_string(),
		expr->type ? expr->type->name : "(no type)",
		expr->operands[0] ? (expr->operands[0]->type ? expr->operands[0]->type->name : "(no type)") : "(null)",
		expr->operands[1] ? (expr->operands[1]->type ? expr->operands[1]->type->name : "(no type)") : "(null)",
		expr->operands[2] ? (expr->operands[2]->type ? expr->operands[2]->type->name : "(no type)") : "(null)"
		);
}

ir_visitor_status ir_validate::visit_leave(ir_expression *ir)
{
	#define validate_expr(x) do { if (!(x)) validate_expr_error(state, ir, #x); } while (0)

	auto ValidateTypes = [&](const glsl_type* A, const glsl_type* B)
	{
		if (A->is_float() && B->is_float())
		{
			bool bAHalf = A->base_type == GLSL_TYPE_HALF;
			bool bBHalf = B->base_type == GLSL_TYPE_HALF;
			if ((int)bAHalf ^ (int)bBHalf)
			{
				if (state->LanguageSpec->CanConvertBetweenHalfAndFloat())
				{
					return;
				}
			}

			validate_expr(A == B);
		}
	};

	switch (ir->operation)
	{
	case ir_unop_bit_not:
		validate_expr(ir->operands[0]->type == ir->type);
		break;
	case ir_unop_logic_not:
		validate_expr(ir->type->base_type == GLSL_TYPE_BOOL);
		validate_expr(ir->operands[0]->type->base_type == GLSL_TYPE_BOOL);
		break;

	case ir_unop_neg:
	case ir_unop_abs:
	case ir_unop_sign:
	case ir_unop_rcp:
		ValidateTypes(ir->type, ir->operands[0]->type);
		break;

	case ir_unop_rsq:
	case ir_unop_sqrt:
	case ir_unop_exp:
	case ir_unop_log:
	case ir_unop_exp2:
	case ir_unop_log2:
		{
			validate_expr(ir->operands[0]->type->is_float() || ir->operands[0]->type->is_integer());
			if (ir->operands[0]->type->is_integer())
			{
				validate_expr(ir->type == glsl_type::get_instance(GLSL_TYPE_FLOAT, ir->operands[0]->type->vector_elements, ir->operands[0]->type->matrix_columns));
			}
			else
			{
				validate_expr(ir->type == ir->operands[0]->type);
			}
		}
		break;

	case ir_unop_f2i:
		validate_expr(ir->operands[0]->type->base_type == GLSL_TYPE_FLOAT);
		validate_expr(ir->type->base_type == GLSL_TYPE_INT);
		break;
	case ir_unop_i2f:
		validate_expr(ir->operands[0]->type->base_type == GLSL_TYPE_INT);
		validate_expr(ir->type->base_type == GLSL_TYPE_FLOAT);
		break;
	case ir_unop_f2b:
		validate_expr(ir->operands[0]->type->base_type == GLSL_TYPE_FLOAT);
		validate_expr(ir->type->base_type == GLSL_TYPE_BOOL);
		break;
	case ir_unop_b2f:
		validate_expr(ir->operands[0]->type->base_type == GLSL_TYPE_BOOL);
		validate_expr(ir->type->base_type == GLSL_TYPE_FLOAT);
		break;
	case ir_unop_i2b:
		validate_expr(ir->operands[0]->type->base_type == GLSL_TYPE_INT);
		validate_expr(ir->type->base_type == GLSL_TYPE_BOOL);
		break;
	case ir_unop_b2i:
		validate_expr(ir->operands[0]->type->base_type == GLSL_TYPE_BOOL);
		validate_expr(ir->type->base_type == GLSL_TYPE_INT);
		break;
	case ir_unop_u2f:
		validate_expr(ir->operands[0]->type->base_type == GLSL_TYPE_UINT);
		validate_expr(ir->type->base_type == GLSL_TYPE_FLOAT);
		break;
	case ir_unop_i2u:
		validate_expr(ir->operands[0]->type->base_type == GLSL_TYPE_INT);
		validate_expr(ir->type->base_type == GLSL_TYPE_UINT);
		break;
	case ir_unop_u2i:
		validate_expr(ir->operands[0]->type->base_type == GLSL_TYPE_UINT);
		validate_expr(ir->type->base_type == GLSL_TYPE_INT);
		break;
	case ir_unop_f2u:
		validate_expr(ir->operands[0]->type->base_type == GLSL_TYPE_FLOAT);
		validate_expr(ir->type->base_type == GLSL_TYPE_UINT);
		break;
	case ir_unop_b2u:
		validate_expr(ir->operands[0]->type->base_type == GLSL_TYPE_BOOL);
		validate_expr(ir->type->base_type == GLSL_TYPE_UINT);
		break;
	case ir_unop_u2b:
		validate_expr(ir->operands[0]->type->base_type == GLSL_TYPE_UINT);
		validate_expr(ir->type->base_type == GLSL_TYPE_BOOL);
		break;
	case ir_unop_h2i:
		validate_expr(ir->operands[0]->type->base_type == GLSL_TYPE_HALF);
		validate_expr(ir->type->base_type == GLSL_TYPE_INT);
		break;
	case ir_unop_i2h:
		validate_expr(ir->operands[0]->type->base_type == GLSL_TYPE_INT);
		validate_expr(ir->type->base_type == GLSL_TYPE_HALF);
		break;
	case ir_unop_h2f:
		validate_expr(ir->operands[0]->type->base_type == GLSL_TYPE_HALF);
		validate_expr(ir->type->base_type == GLSL_TYPE_FLOAT);
		break;
	case ir_unop_f2h:
		validate_expr(ir->operands[0]->type->base_type == GLSL_TYPE_FLOAT);
		validate_expr(ir->type->base_type == GLSL_TYPE_HALF);
		break;
	case ir_unop_h2b:
		validate_expr(ir->operands[0]->type->base_type == GLSL_TYPE_HALF);
		validate_expr(ir->type->base_type == GLSL_TYPE_BOOL);
		break;
	case ir_unop_b2h:
		validate_expr(ir->operands[0]->type->base_type == GLSL_TYPE_BOOL);
		validate_expr(ir->type->base_type == GLSL_TYPE_HALF);
		break;
	case ir_unop_h2u:
		validate_expr(ir->operands[0]->type->base_type == GLSL_TYPE_HALF);
		validate_expr(ir->type->base_type == GLSL_TYPE_UINT);
		break;
	case ir_unop_u2h:
		validate_expr(ir->operands[0]->type->base_type == GLSL_TYPE_UINT);
		validate_expr(ir->type->base_type == GLSL_TYPE_HALF);
		break;

	case ir_unop_transpose:
		validate_expr(ir->type->is_matrix());
		validate_expr(ir->operands[0]->type->is_matrix());
		validate_expr(ir->type->matrix_columns == ir->operands[0]->type->vector_elements);
		validate_expr(ir->type->vector_elements == ir->operands[0]->type->matrix_columns);
		break;

	case ir_unop_any:
	case ir_unop_all:
		validate_expr(ir->operands[0]->type->base_type == GLSL_TYPE_BOOL);
		validate_expr(ir->type == glsl_type::bool_type);
		break;

	case ir_unop_trunc:
	case ir_unop_round:
	case ir_unop_ceil:
	case ir_unop_floor:
	case ir_unop_fract:
	case ir_unop_sin:
	case ir_unop_cos:
	case ir_unop_tan:
	case ir_unop_asin:
	case ir_unop_acos:
	case ir_unop_atan:
	case ir_unop_sinh:
	case ir_unop_cosh:
	case ir_unop_tanh:
	case ir_unop_normalize:
	case ir_unop_dFdx:
	case ir_unop_dFdy:
	case ir_unop_saturate:
		validate_expr(ir->operands[0]->type->is_float());
		ValidateTypes(ir->operands[0]->type, ir->type);
		break;

	case ir_unop_noise:
		/* XXX what can we check here? */
		break;

	case ir_binop_div:
		if (ir->operands[1]->ir_type == ir_type_constant)
		{
			ir_constant* const_denom = ir->operands[1]->as_constant();
			if ( const_denom->type->base_type != GLSL_TYPE_FLOAT )
			{
				if (const_denom && const_denom->are_any_zero())
				{
					_mesa_glsl_warning(state, "internal compiler warning: integer division by zero");
				}
			}
			else
			{
				/* EHartNV - reduced static float div 0.0 to a warning
				 * It happens a lot with defaulted shader inputs, and 
				 * INF is a valid float that often gets generated by
				 * D3D shaders as well. Some GLSL compilers may fail
				 * so it is marked as a warning.
				 */
				if (const_denom && const_denom->are_any_zero())
				{
					_mesa_glsl_warning(state, "internal compiler warning: float division by zero");
				}
			}
			
		}
		if (ir->operands[0]->type->is_scalar())
		{
			ValidateTypes(ir->operands[1]->type, ir->type);
		}
		else if (ir->operands[1]->type->is_scalar())
		{
			ValidateTypes(ir->operands[0]->type, ir->type);
		}
		else if (
			ir->operands[0]->type->is_vector() &&
			ir->operands[1]->type->is_vector()
			)
		{
			ValidateTypes(ir->operands[0]->type, ir->operands[1]->type);
			ValidateTypes(ir->operands[0]->type, ir->type);
		}
		break;

	case ir_binop_mul:
	{
		bool const bNativeMatrixIntrinsics = state->LanguageSpec->SupportsMatrixIntrinsics();
		
		// Matrix-Vector multiplication not handled by ir_binop_mul.
		validate_expr(!ir->operands[0]->type->is_matrix() || !ir->operands[1]->type->is_vector());
		
		if (bNativeMatrixIntrinsics && ir->operands[1]->type->is_matrix() && ir->operands[0]->type->is_vector())
		{
			ValidateTypes(ir->operands[1]->type->column_type(), ir->type);
			ValidateTypes(ir->operands[1]->type->row_type(), ir->operands[0]->type);
			break;
		}
		else
		{
			// Vector-Matrix multiplication not handled by ir_binop_mul.
			validate_expr(!ir->operands[1]->type->is_matrix() || !ir->operands[0]->type->is_vector());
		}
		// intentional fallthrough
	}
	case ir_binop_add:
	case ir_binop_sub:
	case ir_binop_mod:
	case ir_binop_modf:
	case ir_binop_min:
	case ir_binop_max:
	case ir_binop_pow:
		if (ir->operands[0]->type->is_scalar())
		{
			ValidateTypes(ir->operands[1]->type, ir->type);
		}
		else if (ir->operands[1]->type->is_scalar())
		{
			ValidateTypes(ir->operands[0]->type, ir->type);
		}
		else if (
			ir->operands[0]->type->is_vector() &&
			ir->operands[1]->type->is_vector()
			)
		{
			ValidateTypes(ir->operands[0]->type, ir->operands[1]->type);
			ValidateTypes(ir->operands[0]->type, ir->type);
		}
		break;

	case ir_binop_less:
	case ir_binop_greater:
	case ir_binop_lequal:
	case ir_binop_gequal:
	case ir_binop_equal:
	case ir_binop_nequal:
		/* The semantics of the IR operators differ from the GLSL <, >, <=, >=,
		* ==, and != operators.  The IR operators perform a component-wise
		* comparison on scalar or vector types and return a boolean scalar or
		* vector type of the same size.
		*/
		validate_expr(ir->type->base_type == GLSL_TYPE_BOOL);
		validate_expr(ir->operands[0]->type == ir->operands[1]->type);
		validate_expr(ir->operands[0]->type->is_vector()
			|| ir->operands[0]->type->is_scalar());
		validate_expr(ir->operands[0]->type->vector_elements
			== ir->type->vector_elements);
		break;

	case ir_binop_all_equal:
	case ir_binop_any_nequal:
		/* GLSL == and != operate on scalars, vectors, matrices and arrays, and
		* return a scalar boolean.  The IR matches that.
		*/
		validate_expr(ir->type == glsl_type::bool_type);
		validate_expr(ir->operands[0]->type == ir->operands[1]->type);
		break;

	case ir_binop_lshift:
	case ir_binop_rshift:
		validate_expr(ir->operands[0]->type->is_integer() &&
			ir->operands[1]->type->is_integer());
		if (ir->operands[0]->type->is_scalar())
		{
			validate_expr(ir->operands[1]->type->is_scalar());
		}
		if (ir->operands[0]->type->is_vector() &&
			ir->operands[1]->type->is_vector())
		{
				validate_expr(ir->operands[0]->type->components() ==
					ir->operands[1]->type->components());
		}
		validate_expr(ir->type == ir->operands[0]->type);
		break;

	case ir_binop_bit_and:
	case ir_binop_bit_xor:
	case ir_binop_bit_or:
		validate_expr(ir->operands[0]->type->base_type ==
			ir->operands[1]->type->base_type);
		validate_expr(ir->type->is_integer());
		if (ir->operands[0]->type->is_vector() &&
			ir->operands[1]->type->is_vector())
		{
				validate_expr(ir->operands[0]->type->vector_elements ==
					ir->operands[1]->type->vector_elements);
		}
		break;

	case ir_binop_logic_and:
	case ir_binop_logic_xor:
	case ir_binop_logic_or:
		validate_expr(ir->type->is_boolean());
		validate_expr(ir->operands[0]->type == ir->type);
		validate_expr(ir->operands[1]->type == ir->type);
		break;

	case ir_binop_cross:
		validate_expr(ir->type == glsl_type::vec3_type || ir->type == glsl_type::half3_type);
		validate_expr(ir->type == ir->operands[0]->type);
		validate_expr(ir->type == ir->operands[1]->type);
		break;

	case ir_binop_dot:
		validate_expr(ir->type->is_float());
		validate_expr(ir->operands[0]->type->is_float());
		validate_expr(ir->operands[0]->type->is_vector() || ir->operands[0]->type->is_scalar());
		if (ir->operands[0]->type->is_float() && ir->operands[1]->type->is_float())
		{
			ValidateTypes(ir->operands[0]->type, ir->operands[1]->type);
		}
		else
		{
			validate_expr(ir->operands[0]->type == ir->operands[1]->type);
		}
		break;

	case ir_unop_isnan:
	case ir_unop_isinf:
		validate_expr(ir->type->base_type == GLSL_TYPE_BOOL);
		validate_expr(ir->operands[0]->type->is_float());
		validate_expr(ir->type->components() == ir->operands[0]->type->components());
		break;

	case ir_unop_fasu:
		validate_expr(ir->type->base_type == GLSL_TYPE_UINT);
		validate_expr(ir->operands[0]->type->base_type == GLSL_TYPE_FLOAT);
		validate_expr(ir->type->components() == ir->operands[0]->type->components());
		break;

	case ir_unop_fasi:
		validate_expr(ir->type->base_type == GLSL_TYPE_INT);
		validate_expr(ir->operands[0]->type->base_type == GLSL_TYPE_FLOAT);
		validate_expr(ir->type->components() == ir->operands[0]->type->components());
		break;

	case ir_unop_iasf:
		validate_expr(ir->type->base_type == GLSL_TYPE_FLOAT);
		validate_expr(ir->operands[0]->type->base_type == GLSL_TYPE_INT);
		validate_expr(ir->type->components() == ir->operands[0]->type->components());
		break;

	case ir_unop_uasf:
		validate_expr(ir->type->base_type == GLSL_TYPE_FLOAT);
		validate_expr(ir->operands[0]->type->base_type == GLSL_TYPE_UINT);
		validate_expr(ir->type->components() == ir->operands[0]->type->components());
		break;

	case ir_unop_bitreverse:
		validate_expr(ir->type->is_integer());
		validate_expr(ir->type == ir->operands[0]->type);
		break;

	case ir_unop_bitcount:
	case ir_unop_msb:
	case ir_unop_lsb:
		validate_expr(ir->type->base_type == GLSL_TYPE_INT);
		validate_expr(ir->operands[0]->type->is_integer());
		validate_expr(ir->type->components() == ir->operands[0]->type->components());
		break;

	case ir_binop_atan2:
		validate_expr(ir->operands[0]->type->is_float());
		validate_expr(ir->operands[1]->type->is_float());
		break;

	case ir_binop_step:
		validate_expr(ir->type->is_float());
		validate_expr(ir->operands[0]->type->is_float());
		validate_expr(ir->type == ir->operands[1]->type);
		validate_expr(ir->operands[0]->type->is_scalar() ||
			ir->operands[0]->type == ir->operands[1]->type);
		break;

	case ir_ternop_smoothstep:
		validate_expr(ir->type->is_float());
		validate_expr(ir->type == ir->operands[2]->type);
		validate_expr(ir->operands[0]->type->is_scalar() ||
			ir->operands[0]->type == ir->operands[2]->type);
		validate_expr(ir->operands[1]->type->is_scalar() ||
			ir->operands[1]->type == ir->operands[2]->type);
		break;

	case ir_ternop_lerp:
		validate_expr(ir->type->is_float());
		validate_expr(ir->operands[0]->type == ir->type);
		validate_expr(ir->operands[1]->type == ir->type);
		validate_expr(ir->operands[2]->type->base_type == ir->type->base_type);
		break;

	case ir_ternop_clamp:
		validate_expr(ir->operands[0]->type == ir->type);
		validate_expr(ir->operands[1]->type->base_type == ir->type->base_type);
		validate_expr(ir->operands[2]->type->base_type == ir->type->base_type);
		break;

	case ir_ternop_fma:
		validate_expr(ir->type->is_float());
		validate_expr(ir->operands[0]->type == ir->type);
		validate_expr(ir->operands[1]->type == ir->type);
		validate_expr(ir->operands[2]->type == ir->type);
		break;

	case ir_quadop_vector:
		/* The vector operator collects some number of scalars and generates a
		* vector from them.
		*
		*  - All of the operands must be scalar.
		*  - Number of operands must matche the size of the resulting vector.
		*  - Base type of the operands must match the base type of the result.
		*/
		validate_expr(ir->type->is_vector());
		switch (ir->type->vector_elements)
		{
		case 2:
			validate_expr(ir->operands[0]->type->is_scalar());
			validate_expr(ir->operands[0]->type->base_type == ir->type->base_type);
			validate_expr(ir->operands[1]->type->is_scalar());
			validate_expr(ir->operands[1]->type->base_type == ir->type->base_type);
			validate_expr(ir->operands[2] == NULL);
			validate_expr(ir->operands[3] == NULL);
			break;
		case 3:
			validate_expr(ir->operands[0]->type->is_scalar());
			validate_expr(ir->operands[0]->type->base_type == ir->type->base_type);
			validate_expr(ir->operands[1]->type->is_scalar());
			validate_expr(ir->operands[1]->type->base_type == ir->type->base_type);
			validate_expr(ir->operands[2]->type->is_scalar());
			validate_expr(ir->operands[2]->type->base_type == ir->type->base_type);
			validate_expr(ir->operands[3] == NULL);
			break;
		case 4:
			validate_expr(ir->operands[0]->type->is_scalar());
			validate_expr(ir->operands[0]->type->base_type == ir->type->base_type);
			validate_expr(ir->operands[1]->type->is_scalar());
			validate_expr(ir->operands[1]->type->base_type == ir->type->base_type);
			validate_expr(ir->operands[2]->type->is_scalar());
			validate_expr(ir->operands[2]->type->base_type == ir->type->base_type);
			validate_expr(ir->operands[3]->type->is_scalar());
			validate_expr(ir->operands[3]->type->base_type == ir->type->base_type);
			break;
		default:
			/* The is_vector assertion above should prevent execution from ever
			* getting here.
			*/
			validate_expr(!"Should not get here.");
			break;
		}
	case ir_invalid_opcode:
	case ir_opcode_count:
		// Added here to shut up Mac compiler
		validate_expr(!"Should not get here.");
		break;
	}

	#undef validate_expr

	return visit_continue;
}

ir_visitor_status ir_validate::visit_leave(ir_swizzle *ir)
{
	if (ir->val->type->is_matrix())
	{
		_mesa_glsl_error(state, "internal compiler error: ir_swizzle @ %p operates on a matrix.\n");
	}
	else
	{
		unsigned int chans[4] = {ir->mask.x, ir->mask.y, ir->mask.z, ir->mask.w};
		for (unsigned int i = 0; i < ir->type->vector_elements; i++)
		{
			if (chans[i] >= ir->val->type->vector_elements)
			{
				_mesa_glsl_error(state,
					"internal compiler error: ir_swizzle @ %p specifies channel "
					"'%c' not present in the value of type %s.",
					(void *) ir,
					"xyzw"[chans[i]],
					ir->val->type->name
					);
			}
		}
	}

	return visit_continue;
}

ir_visitor_status ir_validate::visit(ir_variable *ir)
{
	/* An ir_variable is the one thing that can (and will) appear multiple times
	* in an IR tree.  It is added to the hashtable so that it can be used
	* in the ir_dereference_variable handler to ensure that a variable is
	* declared before it is dereferenced.
	*/
	if (ir->name)
	{
		if (ralloc_parent(ir->name) != ir)
		{
			const char *old_name = ir->name;
			ir->name = ralloc_strdup(ir, old_name);
		}
		check(ralloc_parent(ir->name) == ir);
	}

	hash_table_insert(ht, ir, ir);

	/* If a variable is an array, verify that the maximum array index is in
	* bounds.  There was once an error in AST-to-HIR conversion that set this
	* to be out of bounds.
	*/
	if (ir->type->array_size() > 0)
	{
		if (ir->max_array_access >= ir->type->length)
		{
			_mesa_glsl_error(state,
				"internal compiler error: ir_variable '%s' has maximum access out of "
				"bounds (%d vs %d)",
				ir->name ? ir->name : "(unnamed)",
				ir->max_array_access,
				ir->type->length - 1
				);
		}
	}

	if (ir->constant_initializer != NULL && !ir->has_initializer)
	{
		_mesa_glsl_error(state,
			"internal compiler error: ir_variable '%s' didn't have an initializer, "
			"but has a constant initializer value.",
			ir->name ? ir->name : "(unnamed)"
			);
	}

	return visit_continue;
}

ir_visitor_status ir_validate::visit_enter(ir_assignment *ir)
{
	const ir_dereference *const lhs = ir->lhs;
	if (lhs->type->is_scalar() || lhs->type->is_vector())
	{
		if (ir->write_mask == 0)
		{
			_mesa_glsl_error(state,
				"internal compiler error: Assignment %d LHS is %s, but write mask "
				"is 0:",
				ir->id,
				lhs->type->is_scalar() ? "scalar" : "vector"
				);
		}

		uint32 lhs_components = 0;
		for (int i = 0; i < 4; i++)
		{
			if (ir->write_mask & (1 << i))
				lhs_components++;
		}

		if (lhs_components != ir->rhs->type->vector_elements)
		{
			_mesa_glsl_error(state,
				"internal compiler error: Assignment %d count of LHS write mask "
				"channels enabled not matching RHS vector size (%d LHS, %d RHS).",
				ir->id,
				lhs_components,
				ir->rhs->type->vector_elements
				);
		}
	}
	else if (lhs->variable_referenced()->type->is_matrix())
	{
		if (ir->write_mask != 0)
		{
			_mesa_glsl_error(state,
				"internal compiler error: assignment %d to matrix with a write mask "
				"not allowed.\n",
				ir->id
				);
		}
	}

	this->validate_ir(ir, this->data);

	return visit_continue;
}

ir_visitor_status ir_validate::visit_enter(ir_call *ir)
{
	ir_function_signature *const callee = ir->callee;

	if (callee->ir_type != ir_type_function_signature)
	{
		_mesa_glsl_error(state,
			"internal compiler error: IR called by ir_call is not "
			"ir_function_signature!\n"
			);
	}

	if (ir->return_deref)
	{
		if (ir->return_deref->type != callee->return_type)
		{
			_mesa_glsl_error(state,
				"internal compiler error: callee type %s does not match return storage type %s",
				callee->return_type->name,
				ir->return_deref->type->name
				);
		}
	}
	else if (callee->return_type != glsl_type::void_type)
	{
		_mesa_glsl_error(state,
			"internal compiler error: ir_call has non-void callee but no return storage\n"
			);
	}

	const exec_node *formal_param_node = callee->parameters.head;
	const exec_node *actual_param_node = ir->actual_parameters.head;
	while (true)
	{
		if (formal_param_node->is_tail_sentinel() != actual_param_node->is_tail_sentinel())
		{
				_mesa_glsl_error(state,
					"internal compiler error: call to '%s' has the wrong number of parameters:",
					callee->function_name()
					);
				return visit_stop;
		}
		if (formal_param_node->is_tail_sentinel())
		{
			break;
		}
		const ir_variable *formal_param = (const ir_variable *) formal_param_node;
		const ir_rvalue *actual_param = (const ir_rvalue *) actual_param_node;
		if (formal_param->type != actual_param->type)
		{
			_mesa_glsl_error(state,
				"internal compiler error: parameter type mismatch in call to '%s'",
				callee->function_name()
				);
			return visit_stop;
		}
		if (formal_param->mode == ir_var_out || formal_param->mode == ir_var_inout)
		{
				if (!actual_param->is_lvalue())
				{
					_mesa_glsl_error(state,
						"internal compiler error: out/inout parameters must be lvalues in call to '%s'",
						callee->function_name()
						);
					return visit_stop;
				}
		}
		formal_param_node = formal_param_node->next;
		actual_param_node = actual_param_node->next;
	}

	return visit_continue;
}

void ir_validate::validate_ir(ir_instruction *ir, void *data)
{
#ifndef NDEBUG
	ir_validate *v = (ir_validate*) data;

	if (hash_table_find(v->ht, ir))
	{
		_mesa_glsl_error(v->state, "internal compiler error: instruction node present twice in ir tree\n");
	}
	hash_table_insert(v->ht, ir, ir);
#endif
}

void check_node_type(ir_instruction *ir, void *data)
{
	ir_validate *v = (ir_validate*) data;

	if (ir->ir_type <= ir_type_unset || ir->ir_type >= ir_type_max)
	{
		_mesa_glsl_error(v->state, "internal compiler error: instruction node %d with unset type\n", ir->id);
	}
	else
	{
		ir_rvalue *value = ir->as_rvalue();
		if (value != NULL && value->type->is_error())
		{
			_mesa_glsl_error(v->state, "internal compiler error: rvalue %d with type 'error' present", value->id);
		}
	}
}

void validate_ir_tree(exec_list *instructions, _mesa_glsl_parse_state *state)
{
	ir_validate v(state);

	v.run(instructions);

	foreach_iter(exec_list_iterator, iter, *instructions)
	{
		ir_instruction *ir = (ir_instruction *)iter.get();
		visit_tree(ir, check_node_type, (void*)&v);
	}
}
