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
#include "glsl_parser_extras.h"
#include "LanguageSpec.h"
#include "IRDump.h"
#include "ast.h"

static ir_variable* make_var(void *ctx, const glsl_type* type, unsigned index, ir_variable_mode mode)
{
	return new(ctx)ir_variable(type, ralloc_asprintf(ctx, "arg%u", index), mode);
}

void make_intrinsic_matrix_wrappers(_mesa_glsl_parse_state* state, ir_function_signature* sig, unsigned num_args)
{
	void* ctx = state;
	ir_function* func = const_cast<ir_function*>(sig->function());
	const glsl_type* genType = sig->return_type;
	glsl_base_type base_type = genType->base_type;
	unsigned vec_size = genType->vector_elements;

	for (unsigned num_cols = 2; num_cols <= 4; ++num_cols)
	{
		const glsl_type* matrix_type = glsl_type::get_instance(base_type, vec_size, num_cols);
		ir_function_signature* matrix_sig = new(ctx)ir_function_signature(matrix_type);
		matrix_sig->is_builtin = true;
		matrix_sig->is_defined = true;

		ir_variable* temp_result = new(ctx)ir_variable(matrix_type, NULL, ir_var_temporary);
		matrix_sig->body.push_tail(temp_result);

		ir_variable* temp_matrices[3] = {0};
		for (unsigned a = 0; a < num_args; ++a)
		{
			temp_matrices[a] = make_var(ctx, matrix_type, a, ir_var_in);
			matrix_sig->parameters.push_tail(temp_matrices[a]);
		}

		for (unsigned c = 0; c < matrix_type->matrix_columns; ++c)
		{
			exec_list params;
			ir_variable* temp_vec = new(ctx)ir_variable(genType, NULL, ir_var_temporary);
			for (unsigned a = 0; a < num_args; ++a)
			{
				params.push_tail(new(ctx)ir_dereference_array(
					temp_matrices[a], new(ctx)ir_constant(c)));
			}
			matrix_sig->body.push_tail(temp_vec);
			matrix_sig->body.push_tail(new(ctx)ir_call(sig,
				new(ctx)ir_dereference_variable(temp_vec), &params));
			matrix_sig->body.push_tail(new(ctx)ir_assignment(
				new(ctx)ir_dereference_array(temp_result, new(ctx)ir_constant(c)),
				new(ctx)ir_dereference_variable(temp_vec)));
		}

		matrix_sig->body.push_tail(new(ctx)ir_return(
			new(ctx)ir_dereference_variable(temp_result)));
		func->add_signature(matrix_sig);
	}
}

void make_intrinsic_genType(
	exec_list *ir, _mesa_glsl_parse_state *state, const char *name, int op,
	unsigned flags, unsigned num_args,
	unsigned min_vec, unsigned max_vec)
{
	void* ctx = state;
	ir_variable* args[3] = {0};
	const bool is_scalar = flags & IR_INTRINSIC_SCALAR;
	const bool ret_bool_true = flags & IR_INTRINSIC_RETURNS_BOOL_TRUE;
	const bool ret_bool = ret_bool_true || (flags & IR_INTRINSIC_RETURNS_BOOL);
	const bool support_matrices = (flags & IR_INTRINSIC_MATRIX) && !is_scalar && !ret_bool;
	const bool bIsVoid = (flags & IR_INTRINSIC_RETURNS_VOID);
	const bool bPromoteIntsToFloat = (flags & IR_INTRINSIC_PROMOTE_ARG_FLOAT_RETURN_FLOAT) == IR_INTRINSIC_PROMOTE_ARG_FLOAT_RETURN_FLOAT;
	const bool bTakesInts = (flags & (IR_INTRINSIC_UINT | IR_INTRINSIC_INT)) != 0;
	// Can't both accept uint/int AND promote them
	if (bPromoteIntsToFloat)
	{
		check(!bTakesInts);
	}

	ir_function* func = new(ctx)ir_function(name);
	if ((flags & ~IR_INTRINSIC_RETURNS_VOID))
	{
		for (int base_type = GLSL_TYPE_UINT; base_type <= GLSL_TYPE_BOOL; ++base_type)
		{
			if (flags & (1 << base_type))
			{
				const bool do_passthru = flags & (0x10 << base_type) && num_args == 1;
				for (unsigned vec_size = min_vec; vec_size <= max_vec; ++vec_size)
				{
					const glsl_type* genType = glsl_type::get_instance(base_type, vec_size, 1);
					const glsl_type* retType = genType;

					if (is_scalar)
					{
						retType = glsl_type::get_instance(ret_bool ? GLSL_TYPE_BOOL : base_type, 1, 1);
					}
					else if (ret_bool)
					{
						retType = glsl_type::get_instance(GLSL_TYPE_BOOL, vec_size, 1);
					}
					else if (bIsVoid)
					{
						retType = glsl_type::get_instance(GLSL_TYPE_VOID, 0, 0);
					}

					ir_function_signature* sig = new(ctx)ir_function_signature(retType);
					sig->is_builtin = true;

					for (unsigned a = 0; a < num_args; ++a)
					{
						args[a] = make_var(ctx, genType, a, ir_var_in);
						sig->parameters.push_tail(args[a]);
					}

					if (do_passthru)
					{
						if (ret_bool_true)
						{
							ir_constant_data data;
							for (unsigned i = 0; i < 16; ++i) data.b[i] = true;
							sig->body.push_tail(new(ctx)ir_return(new(ctx)ir_constant(retType, &data)));
						}
						else if (ret_bool)
						{
							ir_constant_data data ={0};
							sig->body.push_tail(new(ctx)ir_return(new(ctx)ir_constant(retType, &data)));
						}
						else
						{
							sig->body.push_tail(new(ctx)ir_return(new(ctx)ir_dereference_variable(args[0])));
						}
						sig->is_defined = true;
					}
					else if (op != ir_invalid_opcode)
					{
						ir_expression* expr = new(ctx)ir_expression(op, retType, NULL, NULL, NULL, NULL);
						for (unsigned a = 0; a < num_args; ++a)
						{
							expr->operands[a] = new(ctx)ir_dereference_variable(args[a]);
						}
						sig->body.push_tail(new(ctx)ir_return(expr));
						sig->is_defined = true;
					}

					func->add_signature(sig);

					if (support_matrices && vec_size >= 2 && (base_type == GLSL_TYPE_FLOAT || base_type == GLSL_TYPE_HALF))
					{
						make_intrinsic_matrix_wrappers(state, sig, num_args);
					}
				}
			}
			else if (bPromoteIntsToFloat && (base_type == GLSL_TYPE_INT || base_type == GLSL_TYPE_UINT))
			{
				for (unsigned vec_size = min_vec; vec_size <= max_vec; ++vec_size)
				{
					const glsl_type* genType = glsl_type::get_instance(base_type, vec_size, 1);
					const glsl_type* retType = glsl_type::get_instance(GLSL_TYPE_FLOAT, vec_size, 1);
				
					ir_function_signature* sig = new(ctx)ir_function_signature(retType);
					sig->is_builtin = true;

					for (unsigned a = 0; a < num_args; ++a)
					{
						args[a] = make_var(ctx, genType, a, ir_var_in);
						sig->parameters.push_tail(args[a]);
					}

					ir_expression* expr = new(ctx)ir_expression(op, retType, NULL, NULL, NULL, NULL);
					for (unsigned a = 0; a < num_args; ++a)
					{
						expr->operands[a] = new(ctx)ir_dereference_variable(args[a]);
					}
					sig->body.push_tail(new(ctx)ir_return(expr));
					sig->is_defined = true;

					func->add_signature(sig);
				}
			}
		}
	}
	else
	{
		check(bIsVoid);
		const glsl_type* retType = glsl_type::get_instance(GLSL_TYPE_VOID, 0, 0);
		ir_function_signature* sig = new(ctx)ir_function_signature(retType);
		sig->is_builtin = true;
		func->add_signature(sig);
	}

	state->symbols->add_global_function(func);
	ir->push_tail(func);
}

void make_intrinsic_modf(exec_list *ir, _mesa_glsl_parse_state *state)
{
	void* ctx = state;
	ir_function* func = new(ctx)ir_function("modf");

	for (unsigned Type = GLSL_TYPE_HALF; Type <= GLSL_TYPE_FLOAT; ++Type)
	{
		for (unsigned c = 1; c <= 4; ++c)
		{
			const glsl_type* genType = glsl_type::get_instance(Type, c, 1);
			ir_function_signature* sig = new(ctx)ir_function_signature(genType);
			sig->is_builtin = true;
			sig->has_output_parameters = true;
			sig->parameters.push_tail(make_var(ctx, genType, 0, ir_var_in));
			sig->parameters.push_tail(make_var(ctx, genType, 1, ir_var_out));
			func->add_signature(sig);
		}
	}
	state->symbols->add_global_function(func);
	ir->push_tail(func);
}

// Trunc represented as {float(int(x))}
static ir_expression* MakeTruncExpression(_mesa_glsl_parse_state* State, ir_rvalue* Expr)
{
	void* ctx = State;
	ir_rvalue* IntExpr = Expr;
	check(Expr->type);
	check(Expr->type->is_float());
	if (Expr->type->base_type == GLSL_TYPE_HALF)
	{
		IntExpr = new(ctx)ir_expression(ir_unop_h2i, Expr);
		check(IntExpr->type->is_integer());
		return new(ctx)ir_expression(ir_unop_i2h, IntExpr);
	}
	check(Expr->type->base_type == GLSL_TYPE_FLOAT);
	IntExpr = new(ctx)ir_expression(ir_unop_f2i, Expr);
	check(IntExpr->type->is_integer());
	return new(ctx)ir_expression(ir_unop_i2f, IntExpr);
}

void MakeIntrinsicTrunc(exec_list *ir, _mesa_glsl_parse_state *state)
{
	void* ctx = state;
	ir_function* func = new(ctx)ir_function("trunc");

	for (unsigned Type = GLSL_TYPE_HALF; Type <= GLSL_TYPE_FLOAT; ++Type)
	{
		for (unsigned c = 1; c <= 4; ++c)
		{
			const glsl_type* genType = glsl_type::get_instance(Type, c, 1);
			ir_function_signature* sig = new(ctx)ir_function_signature(genType);
			sig->is_builtin = true;
			sig->is_defined = true;

			ir_variable* x = make_var(ctx, genType, 0, ir_var_in);
			sig->parameters.push_tail(x);

			ir_expression* TruncBody = MakeTruncExpression(state, new(ctx)ir_dereference_variable(x));
			sig->body.push_tail(new(ctx)ir_return(TruncBody));

			func->add_signature(sig);

			if (c >= 2)
			{
				make_intrinsic_matrix_wrappers(state, sig, 1);
			}
		}
	}

	state->symbols->add_global_function(func);
	ir->push_tail(func);
}

static void MakeIntrinsicTranspose(exec_list *ir, _mesa_glsl_parse_state *state)
{
	void* ctx = state;
	ir_function* func = new(ctx)ir_function("transpose");

	for (unsigned Type = GLSL_TYPE_HALF; Type <= GLSL_TYPE_FLOAT; ++Type)
	{
		for (unsigned c = 2; c <= 4; ++c)
		{
			const glsl_type* MatrixType = glsl_type::get_instance(Type, c, c);
			const glsl_type* RowType = glsl_type::get_instance(Type, c, 1);
			ir_function_signature* sig = new(ctx)ir_function_signature(MatrixType);
			sig->is_builtin = true;
			sig->is_defined = true;

			ir_variable* In = make_var(ctx, MatrixType, 0, ir_var_in);
			sig->parameters.push_tail(In);

			// Generates:
			// m0 = m[0];
			// m1 = m[1];
			// m2 = m[2];
			// m3 = m[3];
			// Out[0] = vec4(m0.x, m1.x, m2.x, m3.x);
			// Out[1] = vec4(m0.y, m1.y, m2.y, m3.y);
			// Out[2] = vec4(m0.z, m1.z, m2.z, m3.z);
			// Out[3] = vec4(m0.w, m1.w, m2.w, m3.w);

			std::map<unsigned, ir_variable*> RowVars;
			for (unsigned Row = 0; Row < c; ++Row)
			{
				ir_variable* RowVar = new(ctx)ir_variable(RowType, NULL, ir_var_temporary);
				ir_assignment* AssignRow = new(ctx)ir_assignment(
					new(ctx)ir_dereference_variable(RowVar),
					new(ctx)ir_dereference_array(In, new(ctx)ir_constant(Row)));
				RowVars[Row] = RowVar;

				sig->body.push_tail(RowVar);
				sig->body.push_tail(AssignRow);
			}

			ir_variable* Out = new(ctx)ir_variable(MatrixType, NULL, ir_var_temporary);
			sig->body.push_tail(Out);
			for (unsigned Row = 0; Row < c; ++Row)
			{
				ir_swizzle_mask SrcMask = {0};
				SrcMask.num_components = 1;
				SrcMask.x = Row;
				for (unsigned Col = 0; Col < c; ++Col)
				{
					ir_swizzle_mask DestMask = {0};
					DestMask.num_components = 1;
					DestMask.x = Col;
					ir_assignment* AssignRow = new(ctx)ir_assignment(
						new(ctx)ir_swizzle(new(ctx)ir_dereference_array(Out, new(ctx)ir_constant(Row)), DestMask),
						new(ctx)ir_swizzle(new(ctx)ir_dereference_variable(RowVars[Col]), SrcMask)
						);
					sig->body.push_tail(AssignRow);
				}
			}

			sig->body.push_tail(new(ctx)ir_return(new(ctx)ir_dereference_variable(Out)));
			func->add_signature(sig);
		}
	}

	state->symbols->add_global_function(func);
	ir->push_tail(func);
}

void make_intrinsic_fmod(exec_list *ir, _mesa_glsl_parse_state *state)
{
	void* ctx = state;
	ir_function* func = new(ctx)ir_function("fmod");

	for (unsigned Type = GLSL_TYPE_HALF; Type <= GLSL_TYPE_FLOAT; ++Type)
	{
		for (unsigned c = 1; c <= 4; ++c)
		{
			const glsl_type* genType = glsl_type::get_instance(Type, c, 1);
			ir_function_signature* sig = new(ctx)ir_function_signature(genType);
			sig->is_builtin = true;
			sig->is_defined = true;

			ir_variable* x = make_var(ctx, genType, 0, ir_var_in);
			ir_variable* y = make_var(ctx, genType, 1, ir_var_in);
			sig->parameters.push_tail(x);
			sig->parameters.push_tail(y);

			ir_expression* x_over_y = new(ctx)ir_expression(ir_binop_div, genType,
				new(ctx)ir_dereference_variable(x),
				new(ctx)ir_dereference_variable(y)
				);
			ir_expression* floor_xy = NULL;
			if (state->bGenerateES)
			{
				floor_xy = MakeTruncExpression(state, x_over_y);
			}
			else
			{
				floor_xy = new(ctx)ir_expression(ir_unop_trunc, genType, x_over_y);
			}
			ir_rvalue* y_floor_xy = new(ctx)ir_expression(ir_binop_mul, genType,
				new(ctx)ir_dereference_variable(y),
				floor_xy
				);
			ir_rvalue* x_sub_y_floor_xy = new(ctx)ir_expression(ir_binop_sub, genType,
				new(ctx)ir_dereference_variable(x),
				y_floor_xy
				);
			sig->body.push_tail(new(ctx)ir_return(x_sub_y_floor_xy));

			func->add_signature(sig);

			if (c >= 2)
			{
				make_intrinsic_matrix_wrappers(state, sig, 2);
			}
		}
	}
	state->symbols->add_global_function(func);
	ir->push_tail(func);
}

void make_intrinsic_sincos(exec_list *ir, _mesa_glsl_parse_state *state)
{
	void* ctx = state;
	ir_function* func = new(ctx)ir_function("sincos");

	for (unsigned Type = GLSL_TYPE_HALF; Type <= GLSL_TYPE_FLOAT; ++Type)
	{
		for (unsigned c = 1; c <= 4; ++c)
		{
			const glsl_type* genType = glsl_type::get_instance(Type, c, 1);
			ir_function_signature* sig = new(ctx)ir_function_signature(genType);
			sig->is_builtin = true;
			sig->is_defined = true;

			ir_variable* arg0 = make_var(ctx, genType, 0, ir_var_in);
			ir_variable* arg1 = make_var(ctx, genType, 1, ir_var_out);
			ir_variable* arg2 = make_var(ctx, genType, 2, ir_var_out);
			sig->parameters.push_tail(arg0);
			sig->parameters.push_tail(arg1);
			sig->parameters.push_tail(arg2);

			ir_expression* sin_expr = new(ctx)ir_expression(ir_unop_sin, genType,
				new(ctx)ir_dereference_variable(arg0));
			ir_expression* cos_expr = new(ctx)ir_expression(ir_unop_cos, genType,
				new(ctx)ir_dereference_variable(arg0));
			sig->body.push_tail(new(ctx)ir_assignment(new(ctx)ir_dereference_variable(arg1), sin_expr));
			sig->body.push_tail(new(ctx)ir_assignment(new(ctx)ir_dereference_variable(arg2), cos_expr));

			func->add_signature(sig);
		}
	}
	state->symbols->add_global_function(func);
	ir->push_tail(func);
}

void MakeIntrinsicSincos(exec_list *ir, _mesa_glsl_parse_state *state)
{
	void* ctx = state;
	ir_function* func = new(ctx)ir_function("sincos");
	
	for (unsigned Type = GLSL_TYPE_HALF; Type <= GLSL_TYPE_FLOAT; ++Type)
	{
		for (unsigned c = 1; c <= 4; ++c)
		{
			const glsl_type* genType = glsl_type::get_instance(Type, c, 1);
			ir_function_signature* sig = new(ctx)ir_function_signature(genType);
			{
				sig->is_builtin = true;
				
				ir_variable* arg0 = make_var(ctx, genType, 0, ir_var_in);
				ir_variable* arg1 = make_var(ctx, genType, 1, ir_var_out);
				sig->parameters.push_tail(arg0);
				sig->parameters.push_tail(arg1);
				func->add_signature(sig);
			}

			ir_function_signature* sig2 = new(ctx)ir_function_signature(glsl_type::void_type);
			{
				sig2->is_builtin = true;
				sig2->is_defined = true;
				
				ir_variable* arg0 = make_var(ctx, genType, 0, ir_var_in);
				ir_variable* arg1 = make_var(ctx, genType, 1, ir_var_out);
				ir_variable* arg2 = make_var(ctx, genType, 2, ir_var_out);
				sig2->parameters.push_tail(arg0);
				sig2->parameters.push_tail(arg1);
				sig2->parameters.push_tail(arg2);
				
				ir_dereference_variable* sin_val = new(ctx)ir_dereference_variable(arg1);
				exec_list actual_parameter;
				actual_parameter.push_tail(new(ctx)ir_dereference_variable(arg0));
				actual_parameter.push_tail(new(ctx)ir_dereference_variable(arg2));
				ir_call* sincos_call = new(ctx)ir_call(sig, sin_val, &actual_parameter);
				sig2->body.push_tail(sincos_call);
				
				func->add_signature(sig2);
			}
		}
	}
	state->symbols->add_global_function(func);
	ir->push_tail(func);
}

void make_intrinsic_radians(exec_list *ir, _mesa_glsl_parse_state *state)
{
	void* ctx = state;
	ir_function* func = new(ctx)ir_function("radians");

	for (unsigned Type = GLSL_TYPE_HALF; Type <= GLSL_TYPE_FLOAT; ++Type)
	{
		for (unsigned c = 1; c <= 4; ++c)
		{
			const glsl_type* genType = glsl_type::get_instance(Type, c, 1);
			ir_function_signature* sig = new(ctx)ir_function_signature(genType);
			sig->is_builtin = true;
			sig->is_defined = true;

			ir_variable* arg0 = make_var(ctx, genType, 0, ir_var_in);
			sig->parameters.push_tail(arg0);
			auto* Constant = new(ctx)ir_constant(3.1415926535897932f / 180.0f);
			Constant->type = genType;
			sig->body.push_tail(new(ctx)ir_return(new(ctx)ir_expression(ir_binop_mul, genType,
				new(ctx)ir_dereference_variable(arg0),
				Constant
				)));

			func->add_signature(sig);

			if (c >= 2)
			{
				make_intrinsic_matrix_wrappers(state, sig, 1);
			}
		}
	}
	state->symbols->add_global_function(func);
	ir->push_tail(func);
}

void make_intrinsic_ddy(exec_list *ir, _mesa_glsl_parse_state *state)
{
	void* ctx = state;
	ir_function* func = new(ctx)ir_function("ddy");

	for (unsigned Type = GLSL_TYPE_HALF; Type <= GLSL_TYPE_FLOAT; ++Type)
	{
		for (unsigned c = 1; c <= 4; ++c)
		{
			const glsl_type* genType = glsl_type::get_instance(Type, c, 1);
			ir_function_signature* sig = new(ctx)ir_function_signature(genType);
			sig->is_builtin = true;
			sig->is_defined = true;

			ir_variable* arg0 = make_var(ctx, genType, 0, ir_var_in);
			sig->parameters.push_tail(arg0);
			ir_rvalue* call = new(ctx)ir_expression(ir_unop_dFdy, genType,
				new(ctx)ir_dereference_variable(arg0),
				NULL, NULL, NULL);

			if (state->adjust_clip_space_dx11_to_opengl)
			{
				call = new(ctx)ir_expression(ir_unop_neg, call->type, call, NULL);
			}

			sig->body.push_tail(new(ctx)ir_return(call));

			func->add_signature(sig);

			if (c >= 2)
			{
				make_intrinsic_matrix_wrappers(state, sig, 1);
			}
		}
	}
	state->symbols->add_global_function(func);
	ir->push_tail(func);
}

void make_intrinsic_degrees(exec_list *ir, _mesa_glsl_parse_state *state)
{
	void* ctx = state;
	ir_function* func = new(ctx)ir_function("degrees");

	for (unsigned Type = GLSL_TYPE_HALF; Type <= GLSL_TYPE_FLOAT; ++Type)
	{
		for (unsigned c = 1; c <= 4; ++c)
		{
			const glsl_type* genType = glsl_type::get_instance(Type, c, 1);
			ir_function_signature* sig = new(ctx)ir_function_signature(genType);
			sig->is_builtin = true;
			sig->is_defined = true;

			ir_variable* arg0 = make_var(ctx, genType, 0, ir_var_in);
			sig->parameters.push_tail(arg0);
			auto* Constant = new(ctx)ir_constant(180.0f / 3.1415926535897932f);
			Constant->type = genType;
			sig->body.push_tail(
				new(ctx)ir_return(
				new(ctx)ir_expression(ir_binop_mul, genType,
				new(ctx)ir_dereference_variable(arg0),
				Constant
				)
				)
				);

			func->add_signature(sig);

			if (c >= 2)
			{
				make_intrinsic_matrix_wrappers(state, sig, 1);
			}
		}
	}
	state->symbols->add_global_function(func);
	ir->push_tail(func);
}

void MakeIntrinsicSaturate(exec_list *ir, _mesa_glsl_parse_state *state, int max_type)
{
	void* ctx = state;
	ir_function* func = nullptr;
	bool const bNativeIntrinsic = state->LanguageSpec->SupportsSaturateIntrinsic();
	if(bNativeIntrinsic)
	{
		max_type = GLSL_TYPE_INT;
		make_intrinsic_genType(ir, state, "saturate", ir_unop_saturate, IR_INTRINSIC_ALL_FLOATING | IR_INTRINSIC_MATRIX, 1, 1, 4);
		func = state->symbols->get_function("saturate");
		assert(func);
	}
	else
	{
		func = new(ctx)ir_function("saturate");
	}
	
	for (int base_type = GLSL_TYPE_UINT; base_type <= max_type; ++base_type)
	{
		ir_constant_data zero_data = {0};
		ir_constant_data one_data;
		
		if (base_type == GLSL_TYPE_FLOAT || base_type == GLSL_TYPE_HALF)
		{
			for (unsigned i = 0; i < 16; ++i)
			{
				one_data.f[i] = 1.0f;
			}
		}
		else
		{
			for (unsigned i = 0; i < 16; ++i)
			{
				one_data.u[i] = 1;
			}
		}
		
		for (unsigned vec_size = 1; vec_size <= 4; ++vec_size)
		{
			const glsl_type* genType = glsl_type::get_instance(base_type, vec_size, 1);
			ir_function_signature* sig = new(ctx)ir_function_signature(genType);
			sig->is_builtin = true;
			sig->is_defined = true;
			
			ir_variable* arg = make_var(ctx, genType, 0, ir_var_in);
			sig->parameters.push_tail(arg);
			
			ir_expression* expr = new(ctx)ir_expression(ir_ternop_clamp, genType,
														new(ctx)ir_dereference_variable(arg),
														new(ctx)ir_constant(genType, &zero_data),
														new(ctx)ir_constant(genType, &one_data),
														NULL);
			sig->body.push_tail(new(ctx)ir_return(expr));
			
			func->add_signature(sig);
			
			if (vec_size >= 2 && (base_type == GLSL_TYPE_FLOAT || base_type == GLSL_TYPE_HALF))
			{
				make_intrinsic_matrix_wrappers(state, sig, 1);
			}
		}
	}
	if (!bNativeIntrinsic)
	{
		state->symbols->add_global_function(func);
		ir->push_tail(func);
	}
}

void make_intrinsic_isfinite(exec_list *ir, _mesa_glsl_parse_state *state)
{
	void* ctx = state;

	// Generate a function that will return true for non-float types.
	make_intrinsic_genType(ir, state, "isfinite", ir_invalid_opcode,
		IR_INTRINSIC_INT_THRU | IR_INTRINSIC_UINT_THRU | IR_INTRINSIC_BOOL_THRU |
		IR_INTRINSIC_RETURNS_BOOL_TRUE, 1);
	ir_function* func = state->symbols->get_function("isfinite");
	check(func);

	for (unsigned Type = GLSL_TYPE_HALF; Type <= GLSL_TYPE_FLOAT; ++Type)
	{
		for (unsigned vec_size = 1; vec_size <= 4; ++vec_size)
		{
			const glsl_type* genType = glsl_type::get_instance(Type, vec_size, 1);
			const glsl_type* retType = glsl_type::get_instance(GLSL_TYPE_BOOL, vec_size, 1);
			ir_function_signature* sig = new(ctx)ir_function_signature(retType);
			sig->is_builtin = true;
			sig->is_defined = true;

			ir_variable* arg = make_var(ctx, genType, 0, ir_var_in);
			sig->parameters.push_tail(arg);

			ir_expression* expr = new(ctx)ir_expression(ir_unop_logic_not, retType,
				new(ctx)ir_expression(ir_unop_isinf, retType,
				new(ctx)ir_dereference_variable(arg)));
			sig->body.push_tail(new(ctx)ir_return(expr));

			func->add_signature(sig);
		}
	}
}

void make_intrinsic_refract(exec_list *ir, _mesa_glsl_parse_state *state)
{
	void* ctx = state;
	ir_function* func = new(ctx)ir_function("refract");

	for (unsigned Type = GLSL_TYPE_HALF; Type <= GLSL_TYPE_FLOAT; ++Type)
	{
		const glsl_type* ScalarType = glsl_type::get_instance(Type, 1, 1);
		for (unsigned c = 2; c <= 4; ++c)
		{
			const glsl_type* genType = glsl_type::get_instance(Type, c, 1);
			ir_function_signature* sig = new(ctx)ir_function_signature(genType);
			sig->is_builtin = true;
			sig->parameters.push_tail(make_var(ctx, genType, 0, ir_var_in));
			sig->parameters.push_tail(make_var(ctx, genType, 1, ir_var_in));
			sig->parameters.push_tail(make_var(ctx, ScalarType, 2, ir_var_in));
			func->add_signature(sig);
		}
	}
	state->symbols->add_global_function(func);
	ir->push_tail(func);
}

void make_intrinsic_clip(exec_list *ir, _mesa_glsl_parse_state *state)
{
	void* ctx = state;
	ir_function* func = new(ctx)ir_function("clip");
	ir_constant_data zero_data = {0};

	for (unsigned Type = GLSL_TYPE_INT; Type <= GLSL_TYPE_FLOAT; ++Type)
	{
		for (unsigned c = 1; c <= 4; ++c)
		{
			const glsl_type* genType = glsl_type::get_instance(Type, c, 1);
			ir_function_signature* sig = new(ctx)ir_function_signature(glsl_type::void_type);
			sig->is_builtin = true;
			sig->is_defined = true;

			ir_variable* arg = make_var(ctx, genType, 0, ir_var_in);
			sig->parameters.push_tail(arg);

			ir_rvalue* condition = new(ctx)ir_expression(ir_binop_less,
				new(ctx)ir_dereference_variable(arg),
				new(ctx)ir_constant(genType, &zero_data));
			if (c > 1)
			{
				condition = new(ctx)ir_expression(ir_unop_any, condition);
			}
			sig->body.push_tail(new(ctx)ir_discard(condition));

			func->add_signature(sig);
		}
	}
	state->symbols->add_global_function(func);
	ir->push_tail(func);
}

void make_intrinsic_determinant(exec_list *ir, _mesa_glsl_parse_state *state)
{
	void* ctx = state;
	ir_function* func = new(ctx)ir_function("determinant");

	for (unsigned Type = GLSL_TYPE_HALF; Type <= GLSL_TYPE_FLOAT; ++Type)
	{
		for (unsigned i = 2; i <= 4; ++i)
		{
			const glsl_type* matrix_type = glsl_type::get_instance(Type, i, i);
			ir_function_signature* sig = new(ctx)ir_function_signature(glsl_type::get_instance(Type, 1, 1));
			sig->is_builtin = true;
			sig->parameters.push_tail(make_var(ctx, matrix_type, 0, ir_var_in));
			func->add_signature(sig);
		}
	}
	state->symbols->add_global_function(func);
	ir->push_tail(func);
}

void make_intrinsic_transpose(exec_list *ir, _mesa_glsl_parse_state *state)
{
	void* ctx = state;
	ir_function* func = new(ctx)ir_function("transpose");

	for (unsigned Type = GLSL_TYPE_HALF; Type <= GLSL_TYPE_FLOAT; ++Type)
	{
		for (unsigned c = 2; c <= 4; ++c)
		{
			for (unsigned r = 2; r <= 4; ++r)
			{
				const glsl_type* matrix_type = glsl_type::get_instance(Type, r, c);
				const glsl_type* ret_type = glsl_type::get_instance(Type, c, r);
				ir_function_signature* sig = new(ctx)ir_function_signature(ret_type);
				sig->is_builtin = true;
				sig->is_defined = true;

				ir_variable* var = make_var(ctx, matrix_type, 0, ir_var_in);
				sig->parameters.push_tail(var);

				ir_expression* expr = new(ctx)ir_expression(ir_unop_transpose, ret_type,
					new(ctx)ir_dereference_variable(var));
				sig->body.push_tail(new(ctx)ir_return(expr));

				func->add_signature(sig);
			}
		}
	}

	state->symbols->add_global_function(func);
	ir->push_tail(func);
}

void make_intrinsic_emit_vertex(exec_list *ir, _mesa_glsl_parse_state *state)
{
	void* ctx = state;
	ir_function* func = new(ctx)ir_function("EmitVertex");
	ir_function_signature* sig = new(ctx)ir_function_signature(glsl_type::void_type);
	sig->is_builtin = true;
	func->add_signature(sig);
	state->symbols->add_global_function(func);
	ir->push_tail(func);
}

void make_intrinsic_end_primitive(exec_list *ir, _mesa_glsl_parse_state *state)
{
	void* ctx = state;
	ir_function* func = new(ctx)ir_function("EndPrimitive");
	ir_function_signature* sig = new(ctx)ir_function_signature(glsl_type::void_type);
	sig->is_builtin = true;
	func->add_signature(sig);
	state->symbols->add_global_function(func);
	ir->push_tail(func);
}

void make_intrinsic_pack_functions(exec_list *ir, _mesa_glsl_parse_state *state)
{
	void* ctx = state;

	ir_function_signature* packSig = NULL;
	ir_function_signature* unpackSig = NULL;

	/** Add hidden GLSL functions first */
	{
		ir_function* func = new(ctx)ir_function("packHalf2x16");
		ir_function_signature* sig = new(ctx)ir_function_signature(glsl_type::uint_type);
		sig->is_builtin = true;
		sig->parameters.push_tail(make_var(ctx, glsl_type::vec2_type, 0, ir_var_in));
		func->add_signature(sig);
		ir->push_tail(func);
		packSig = sig;
	}
	{
		ir_function* func = new(ctx)ir_function("unpackHalf2x16");
		ir_function_signature* sig = new(ctx)ir_function_signature(glsl_type::vec2_type);
		sig->is_builtin = true;
		sig->parameters.push_tail(make_var(ctx, glsl_type::uint_type, 0, ir_var_in));
		func->add_signature(sig);
		ir->push_tail(func);
		unpackSig = sig;
	}
	{
		ir_function* func = new(ctx)ir_function("f32tof16");

		/***********************************
		GLSL code for what this implements:

		uint f32tof16( float f)
		{
		return packHalf2x16( vec2( f, 0));
		}

		uint2 f32tof16( float2 f)
		{
		uint2 ret;
		ret.x = packHalf2x16( vec2( f.x, 0));
		ret.y = packHalf2x16( vec2( f.y, 0));
		return ret;
		}
		***********************************/

		/**********************************
		* f32tof16 scalar implementation:
		*  Create temp vec2
		* assign arg to temp.x
		* create constant 0
		* assign 0 to y
		* return result
		**********************************/
		ir_function_signature* scalar = new(ctx)ir_function_signature(glsl_type::uint_type);
		scalar->is_builtin = true;
		scalar->is_defined = true;

		/** Create parameter */
		ir_variable* arg = make_var(ctx, glsl_type::float_type, 0, ir_var_in);
		scalar->parameters.push_tail(arg);

		/** create temp vec2 */
		ir_variable *t_vec2 = new(ctx)ir_variable(glsl_type::vec2_type, "pack_temp", ir_var_temporary);
		scalar->body.push_tail(t_vec2);

		/** deref var */
		ir_dereference_variable *lhs = new(ctx)ir_dereference_variable(t_vec2);
		ir_dereference_variable *rhs = new(ctx)ir_dereference_variable(arg);

		/**assign arg */
		scalar->body.push_tail(new(ctx)ir_assignment(lhs, rhs, NULL, 0x1));

		/**assign zero*/
		lhs = new(ctx)ir_dereference_variable(t_vec2);
		scalar->body.push_tail(new(ctx)ir_assignment(lhs, new(ctx)ir_constant(0.0f), NULL, 0x2));

		/** temp to capture return value */
		ir_variable *t_ret = new(ctx)ir_variable(glsl_type::uint_type, "ret", ir_var_temporary);
		scalar->body.push_tail(t_ret);
		ir_dereference_variable *ret = new(ctx)ir_dereference_variable(t_ret);

		/** execute the call */
		exec_list actual_parameter;
		lhs = new(ctx)ir_dereference_variable(t_vec2);
		actual_parameter.push_tail(lhs);
		scalar->body.push_tail(
			new(ctx)ir_call(packSig, ret, &actual_parameter)
			);

		/** return value */
		ret = new(ctx)ir_dereference_variable(t_ret);
		scalar->body.push_tail(new(ctx)ir_return(ret));

		func->add_signature(scalar);

		/**********************************
		* f32tof16 vector implementation:
		*  Create temp vecN
		* For num_components
		*   create swizzle selecting component
		*   call scalar f32tof16
		*   assign masked temp
		* return temp
		**********************************/
		for (int vec_size = 2; vec_size <= 4; vec_size++)
		{
			ir_function_signature* sig = new(ctx)ir_function_signature(glsl_type::get_instance(GLSL_TYPE_UINT, vec_size, 1));
			sig->is_builtin = true;
			sig->is_defined = true;

			/** Create parameter */
			ir_variable* arg = make_var(ctx, glsl_type::get_instance(GLSL_TYPE_FLOAT, vec_size, 1), 0, ir_var_in);
			sig->parameters.push_tail(arg);

			/** create temp for return value */
			ir_variable *ret_var = new(ctx)ir_variable(glsl_type::get_instance(GLSL_TYPE_UINT, vec_size, 1), "ret", ir_var_temporary);
			sig->body.push_tail(ret_var);


			// iterate over components
			for (int i = 0; i < vec_size; i++)
			{
				exec_list actual_parameter;
				ir_variable *temp_var = new(ctx)ir_variable(glsl_type::uint_type, "temp", ir_var_temporary);
				sig->body.push_tail(temp_var);
				ir_dereference_variable *lhs = new(ctx)ir_dereference_variable(temp_var);
				ir_rvalue *rhs = new(ctx)ir_swizzle(
					new(ctx)ir_dereference_variable(arg), i, 0, 0, 0, 1
					);
				actual_parameter.push_tail(rhs);
				sig->body.push_tail(
					new(ctx)ir_call(scalar, lhs, &actual_parameter)
					);
				sig->body.push_tail(
					new(ctx)ir_assignment(
					new(ctx)ir_dereference_variable(ret_var),
					new(ctx)ir_dereference_variable(temp_var),
					NULL,
					1u << i
					)
					);
			}

			/** return value */
			sig->body.push_tail(new(ctx)ir_return(new(ctx)ir_dereference_variable(ret_var)));
			func->add_signature(sig);
		}

		state->symbols->add_global_function(func);
		ir->push_tail(func);
	}

	{
		ir_function* func = new(ctx)ir_function("f16tof32");

		/***********************************
		GLSL code for what this implements:

		float f16tof32( uint u)
		{
		return unpackHalf2x16( u).x;
		}

		float2 f32tof16( uint2 u)
		{
		float2 ret;
		ret.x = packHalf2x16( u.x).x;
		ret.y = packHalf2x16( u.y).x;
		return ret;
		}
		***********************************/

		/**********************************
		* f16tof32 scalar implementation:
		*  Create temp vec2
		* execute unpack on uint, to temp
		* return result.x
		**********************************/
		ir_function_signature* scalar = new(ctx)ir_function_signature(glsl_type::float_type);
		scalar->is_builtin = true;
		scalar->is_defined = true;

		/** Create parameter */
		ir_variable* arg = make_var(ctx, glsl_type::uint_type, 0, ir_var_in);
		scalar->parameters.push_tail(arg);

		/** create temp vec2 */
		ir_variable *t_vec2 = new(ctx)ir_variable(glsl_type::vec2_type, "unpack_temp", ir_var_temporary);
		scalar->body.push_tail(t_vec2);

		/** call unpack */
		exec_list actual_parameter;
		actual_parameter.push_tail(new(ctx)ir_dereference_variable(arg));
		scalar->body.push_tail(
			new(ctx)ir_call(unpackSig, new(ctx)ir_dereference_variable(t_vec2), &actual_parameter)
			);

		/** return value */
		scalar->body.push_tail
			(
			new(ctx)ir_return
			(
			new(ctx)ir_swizzle
			(
			new(ctx)ir_dereference_variable(t_vec2),
			0, 0, 0, 0, 1
			)
			)
			);

		func->add_signature(scalar);

		/**********************************
		* f16tof32 vector implementation:
		*  Create temp vecN
		* For num_components
		*   create swizzle selecting component
		*   call scalar f32tof16
		*   assign masked temp
		* return temp
		**********************************/
		for (int vec_size = 2; vec_size <= 4; vec_size++)
		{
			ir_function_signature* sig = new(ctx)ir_function_signature(glsl_type::get_instance(GLSL_TYPE_FLOAT, vec_size, 1));
			sig->is_builtin = true;
			sig->is_defined = true;

			/** Create parameter */
			ir_variable* arg = make_var(ctx, glsl_type::get_instance(GLSL_TYPE_UINT, vec_size, 1), 0, ir_var_in);
			sig->parameters.push_tail(arg);

			/** create temp for return value */
			ir_variable *ret_var = new(ctx)ir_variable(glsl_type::get_instance(GLSL_TYPE_FLOAT, vec_size, 1), "ret", ir_var_temporary);
			sig->body.push_tail(ret_var);


			// iterate over components
			for (int i = 0; i < vec_size; i++)
			{
				exec_list actual_parameter;
				ir_variable *temp_var = new(ctx)ir_variable(glsl_type::float_type, "temp", ir_var_temporary);
				sig->body.push_tail(temp_var);
				ir_dereference_variable *lhs = new(ctx)ir_dereference_variable(temp_var);
				ir_rvalue *rhs = new(ctx)ir_swizzle(
					new(ctx)ir_dereference_variable(arg), i, 0, 0, 0, 1
					);
				actual_parameter.push_tail(rhs);
				sig->body.push_tail(
					new(ctx)ir_call(scalar, lhs, &actual_parameter)
					);
				sig->body.push_tail(
					new(ctx)ir_assignment(
					new(ctx)ir_dereference_variable(ret_var),
					new(ctx)ir_dereference_variable(temp_var),
					NULL,
					1u << i
					)
					);
			}

			/** return value */
			sig->body.push_tail(new(ctx)ir_return(new(ctx)ir_dereference_variable(ret_var)));
			func->add_signature(sig);
		}

		state->symbols->add_global_function(func);
		ir->push_tail(func);
	}

	/** as* functions */
	{
		ir_function* asuint = new(ctx)ir_function("asuint");
		ir_function* asint = new(ctx)ir_function("asint");
		ir_function* asfloat = new(ctx)ir_function("asfloat");

		glsl_base_type inType[] =
		{
			GLSL_TYPE_UINT, GLSL_TYPE_INT, GLSL_TYPE_FLOAT, GLSL_TYPE_FLOAT
		};
		glsl_base_type outType[] =
		{
			GLSL_TYPE_FLOAT, GLSL_TYPE_FLOAT, GLSL_TYPE_UINT, GLSL_TYPE_INT
		};
		ir_function* func[] =
		{
			asfloat, asfloat, asuint, asint
		};
		ir_expression_operation op[] =
		{
			ir_unop_uasf, ir_unop_iasf, ir_unop_fasu, ir_unop_fasi
		};

		for (int vec_size = 1; vec_size <= 4; vec_size++)
		{
			for (size_t i = 0; i < sizeof(func) / sizeof(ir_function*); i++)
			{
				glsl_base_type inputTypes[2];
				inputTypes[0] = inType[i];
				inputTypes[1] = outType[i];
				
				for (int in_type = 0; in_type < sizeof(inputTypes) / sizeof(glsl_base_type); in_type++)
				{
					ir_function_signature* sig = new(ctx)ir_function_signature(glsl_type::get_instance(outType[i], vec_size, 1));
					sig->is_builtin = true;
					sig->is_defined = true;

					/** Create parameter */
					ir_variable* arg = make_var(ctx, glsl_type::get_instance(inputTypes[in_type], vec_size, 1), 0, ir_var_in);
					sig->parameters.push_tail(arg);

					/** Generate the expression and return it */
					ir_rvalue* expression = NULL;
					if (inputTypes[in_type] == inType[i])
					{
						expression = new(ctx)ir_expression(op[i], new(ctx)ir_dereference_variable(arg));
					}
					else
					{
						expression = convert_component(new(ctx)ir_dereference_variable(arg), glsl_type::get_instance(inputTypes[in_type], vec_size, 1));
					}
					sig->body.push_tail(new(ctx)ir_return(expression));

					func[i]->add_signature(sig);
				}
			}
		}
		state->symbols->add_global_function(asuint);
		ir->push_tail(asuint);
		state->symbols->add_global_function(asint);
		ir->push_tail(asint);
		state->symbols->add_global_function(asfloat);
		ir->push_tail(asfloat);
	}
}

void make_intrinsic_sm5_functions(exec_list *ir, _mesa_glsl_parse_state *state)
{
	/**
	* misc sm5 functions
	* frexp -> frexp
	* ldexp -> ldexp (GL lacks float exponent)
	* countbits -> bitCount
	* firstbithigh -> findMSB
	* firstbitlow -> findLSB
	* reversebits -> bitfieldReverse
	*/
	void* ctx = state;

	{
		ir_function* func = new(ctx)ir_function("frexp");
		for (unsigned Type = GLSL_TYPE_HALF; Type <= GLSL_TYPE_FLOAT; ++Type)
		{
			for (int vec_size = 1; vec_size <= 4; vec_size++)
			{
				const glsl_type *float_type = glsl_type::get_instance(Type, vec_size, 1);
				const glsl_type *int_type = glsl_type::get_instance(GLSL_TYPE_INT, vec_size, 1);
				ir_function_signature* sig = new(ctx)ir_function_signature(float_type);
				sig->is_builtin = true;
				sig->parameters.push_tail(make_var(ctx, float_type, 0, ir_var_in));
				sig->parameters.push_tail(make_var(ctx, int_type, 0, ir_var_out));
				func->add_signature(sig);
			}
		}
		ir->push_tail(func);
		state->symbols->add_global_function(func);
	}

	{
		/** can't use GLSL version due to float/int parameter mismatch */
		ir_function* func = new(ctx)ir_function("ldexp");
		for (unsigned Type = GLSL_TYPE_HALF; Type <= GLSL_TYPE_FLOAT; ++Type)
		{
			for (int vec_size = 1; vec_size <= 4; vec_size++)
			{
				const glsl_type *float_type = glsl_type::get_instance(Type, vec_size, 1);
				ir_function_signature* sig = new(ctx)ir_function_signature(float_type);
				sig->is_builtin = true;
				sig->is_defined = true;
				ir_variable *mantissa = make_var(ctx, float_type, 0, ir_var_in);
				ir_variable *exp = make_var(ctx, float_type, 0, ir_var_in);
				sig->parameters.push_tail(mantissa);
				sig->parameters.push_tail(exp);
				sig->body.push_tail
					(
					new(ctx)ir_return
					(
					new(ctx)ir_expression
					(
					ir_binop_mul,
					new(ctx)ir_dereference_variable(mantissa),
					new(ctx)ir_expression
					(
					ir_unop_exp2,
					new(ctx)ir_dereference_variable(exp)
					)
					)
					)
					);
				func->add_signature(sig);
			}
		}
		ir->push_tail(func);
		state->symbols->add_global_function(func);
	}

	{
		const char* funcName[] =
		{
			"countbits", "firstbithigh", "firstbitlow", "reversebits"
		};
		ir_expression_operation op[] =
		{
			ir_unop_bitcount, ir_unop_msb, ir_unop_lsb, ir_unop_bitreverse
		};
		bool use_base_type[] = {
			true, false, false, true
		};

		for (size_t i = 0; i < sizeof(funcName) / sizeof(char*); i++)
		{
			ir_function* func = new(ctx)ir_function(funcName[i]);

			for (int base_type = GLSL_TYPE_UINT; base_type <= GLSL_TYPE_INT; ++base_type)
			{
				for (int vec_size = 1; vec_size <= 4; vec_size++)
				{
					const glsl_type* type = use_base_type ? glsl_type::get_instance(base_type, vec_size, 1) : glsl_type::get_instance(GLSL_TYPE_INT, vec_size, 1);
					ir_function_signature* sig = new(ctx)ir_function_signature(type);
					sig->is_builtin = true;
					sig->is_defined = true;

					/** Create parameter */
					ir_variable* arg = make_var(ctx, glsl_type::get_instance(base_type, vec_size, 1), 0, ir_var_in);
					sig->parameters.push_tail(arg);

					/** Generate the expression and return it */
					ir_expression* expression = new(ctx)ir_expression(op[i], new(ctx)ir_dereference_variable(arg));
					sig->body.push_tail(new(ctx)ir_return(expression));

					func->add_signature(sig);
				}
			}
			state->symbols->add_global_function(func);
			ir->push_tail(func);
		}
	}
}

void make_intrinsic_atomics(exec_list *ir, _mesa_glsl_parse_state *state)
{
	void* ctx = state;
	{
		const char* funcName[] =
		{
			"InterlockedAdd", "InterlockedAnd", "InterlockedMax",
			"InterlockedMin", "InterlockedOr", "InterlockedXor",
			"InterlockedExchange"
		};
		ir_atomic_op op[] =
		{
			ir_atomic_add, ir_atomic_and, ir_atomic_max,
			ir_atomic_min, ir_atomic_or, ir_atomic_xor,
			ir_atomic_swap
		};
		int min_results[] =
		{
			0, 0, 0,
			0, 0, 0,
			1
		};

		for (size_t i = 0; i < sizeof(funcName) / sizeof(char*); i++)
		{
			ir_function* func = new(ctx)ir_function(funcName[i]);

			for (int base_type = GLSL_TYPE_UINT; base_type <= GLSL_TYPE_INT; ++base_type)
			{
				for (int results = min_results[i]; results < 2; results++)
				{
					ir_function_signature* sig = new(ctx)ir_function_signature(glsl_type::void_type);
					sig->is_builtin = true;
					sig->is_defined = true;

					/** Create parameters */
					ir_variable* mem = make_var(ctx, glsl_type::get_instance(base_type, 1, 1), 0, ir_var_ref);
					sig->parameters.push_tail(mem);
					ir_variable* arg = make_var(ctx, glsl_type::get_instance(base_type, 1, 1), 1, ir_var_in);
					sig->parameters.push_tail(arg);
					ir_variable* res = NULL;

					if (results == 1)
					{
						res = make_var(ctx, glsl_type::get_instance(base_type, 1, 1), 2, ir_var_out);
						sig->parameters.push_tail(res);
					}
					else
					{
						/** Generate temp to discard result*/
						res = make_var(ctx, glsl_type::get_instance(base_type, 1, 1), 3, ir_var_temporary);
						sig->body.push_tail(res);
					}

					/** Generate the atomic */
					sig->body.push_tail(
						new(ctx)ir_atomic(
						op[i],
						new(ctx)ir_dereference_variable(res),
						new(ctx)ir_dereference_variable(mem),
						new(ctx)ir_dereference_variable(arg),
						NULL
						)
						);

					func->add_signature(sig);
				}
			}
			state->symbols->add_global_function(func);
			ir->push_tail(func);
		}
	}

	{
		ir_function* func = new(ctx)ir_function("InterlockedCompareStore");

		for (int base_type = GLSL_TYPE_UINT; base_type <= GLSL_TYPE_INT; ++base_type)
		{
			ir_function_signature* sig = new(ctx)ir_function_signature(glsl_type::void_type);
			sig->is_builtin = true;
			sig->is_defined = true;

			/** Create parameters */
			ir_variable* mem = make_var(ctx, glsl_type::get_instance(base_type, 1, 1), 0, ir_var_ref);
			sig->parameters.push_tail(mem);
			ir_variable* arg0 = make_var(ctx, glsl_type::get_instance(base_type, 1, 1), 1, ir_var_in);
			sig->parameters.push_tail(arg0);
			ir_variable* arg1 = make_var(ctx, glsl_type::get_instance(base_type, 1, 1), 2, ir_var_in);
			sig->parameters.push_tail(arg1);

			/** Generate temp to discard result*/
			ir_variable* temp = make_var(ctx, glsl_type::get_instance(base_type, 1, 1), 3, ir_var_temporary);
			sig->body.push_tail(temp);

			/** Generate the atomic */
			sig->body.push_tail(
				new(ctx)ir_atomic(
				ir_atomic_cmp_swap,
				new(ctx)ir_dereference_variable(temp),
				new(ctx)ir_dereference_variable(mem),
				new(ctx)ir_dereference_variable(arg0),
				new(ctx)ir_dereference_variable(arg1)
				)
				);

			func->add_signature(sig);
		}
		state->symbols->add_global_function(func);
		ir->push_tail(func);
	}

	{
		ir_function* func = new(ctx)ir_function("InterlockedCompareExchange");

		for (int base_type = GLSL_TYPE_UINT; base_type <= GLSL_TYPE_INT; ++base_type)
		{
			ir_function_signature* sig = new(ctx)ir_function_signature(glsl_type::void_type);
			sig->is_builtin = true;
			sig->is_defined = true;

			/** Create parameters */
			ir_variable* mem = make_var(ctx, glsl_type::get_instance(base_type, 1, 1), 0, ir_var_ref);
			sig->parameters.push_tail(mem);
			ir_variable* arg0 = make_var(ctx, glsl_type::get_instance(base_type, 1, 1), 1, ir_var_in);
			sig->parameters.push_tail(arg0);
			ir_variable* arg1 = make_var(ctx, glsl_type::get_instance(base_type, 1, 1), 2, ir_var_in);
			sig->parameters.push_tail(arg1);
			ir_variable* res = make_var(ctx, glsl_type::get_instance(base_type, 1, 1), 3, ir_var_out);
			sig->parameters.push_tail(res);

			/** Generate the atomic */
			sig->body.push_tail(
				new(ctx)ir_atomic(
				ir_atomic_cmp_swap,
				new(ctx)ir_dereference_variable(res),
				new(ctx)ir_dereference_variable(mem),
				new(ctx)ir_dereference_variable(arg0),
				new(ctx)ir_dereference_variable(arg1)
				)
				);

			func->add_signature(sig);
		}
		state->symbols->add_global_function(func);
		ir->push_tail(func);
	}
}

void _mesa_glsl_initialize_functions(exec_list *ir, _mesa_glsl_parse_state *state)
{
	// 8.1 Angle and Trigonometry Functions.
	make_intrinsic_radians(ir, state);
	make_intrinsic_degrees(ir, state);
	make_intrinsic_ddy(ir, state);
	make_intrinsic_genType(ir, state, "sin", ir_unop_sin, IR_INTRINSIC_ALL_FLOATING | IR_INTRINSIC_MATRIX, 1);
	make_intrinsic_genType(ir, state, "cos", ir_unop_cos, IR_INTRINSIC_ALL_FLOATING | IR_INTRINSIC_MATRIX, 1);
	make_intrinsic_genType(ir, state, "tan", ir_unop_tan, IR_INTRINSIC_ALL_FLOATING | IR_INTRINSIC_MATRIX, 1);
	make_intrinsic_genType(ir, state, "asin", ir_unop_asin, IR_INTRINSIC_ALL_FLOATING | IR_INTRINSIC_MATRIX, 1);
	make_intrinsic_genType(ir, state, "acos", ir_unop_acos, IR_INTRINSIC_ALL_FLOATING | IR_INTRINSIC_MATRIX, 1);
	make_intrinsic_genType(ir, state, "atan", ir_unop_atan, IR_INTRINSIC_ALL_FLOATING | IR_INTRINSIC_MATRIX, 1);
	make_intrinsic_genType(ir, state, "sinh", ir_unop_sinh, IR_INTRINSIC_ALL_FLOATING | IR_INTRINSIC_MATRIX, 1);
	make_intrinsic_genType(ir, state, "cosh", ir_unop_cosh, IR_INTRINSIC_ALL_FLOATING | IR_INTRINSIC_MATRIX, 1);
	make_intrinsic_genType(ir, state, "tanh", ir_unop_tanh, IR_INTRINSIC_ALL_FLOATING | IR_INTRINSIC_MATRIX, 1);
	make_intrinsic_genType(ir, state, "atan2", ir_binop_atan2, IR_INTRINSIC_ALL_FLOATING | IR_INTRINSIC_MATRIX, 2);
	
	if (!state->LanguageSpec->SupportsSinCosIntrinsic())
	{
		make_intrinsic_sincos(ir, state);
	}
	else
	{
		MakeIntrinsicSincos(ir, state);
	}

	// 8.2 Exponential Functions.
	make_intrinsic_genType(ir, state, "pow", ir_binop_pow, IR_INTRINSIC_ALL_FLOATING | IR_INTRINSIC_MATRIX, 2);
	make_intrinsic_genType(ir, state, "exp", ir_unop_exp, IR_INTRINSIC_ALL_FLOATING | IR_INTRINSIC_MATRIX | IR_INTRINSIC_PROMOTE_ARG_FLOAT_RETURN_FLOAT, 1);
	make_intrinsic_genType(ir, state, "log", ir_unop_log, IR_INTRINSIC_ALL_FLOATING | IR_INTRINSIC_MATRIX | IR_INTRINSIC_PROMOTE_ARG_FLOAT_RETURN_FLOAT, 1);
	make_intrinsic_genType(ir, state, "exp2", ir_unop_exp2, IR_INTRINSIC_ALL_FLOATING | IR_INTRINSIC_MATRIX | IR_INTRINSIC_PROMOTE_ARG_FLOAT_RETURN_FLOAT, 1);
	make_intrinsic_genType(ir, state, "log2", ir_unop_log2, IR_INTRINSIC_ALL_FLOATING | IR_INTRINSIC_MATRIX | IR_INTRINSIC_PROMOTE_ARG_FLOAT_RETURN_FLOAT, 1);
	make_intrinsic_genType(ir, state, "sqrt", ir_unop_sqrt, IR_INTRINSIC_ALL_FLOATING | IR_INTRINSIC_MATRIX | IR_INTRINSIC_PROMOTE_ARG_FLOAT_RETURN_FLOAT, 1);
	make_intrinsic_genType(ir, state, "rsqrt", ir_unop_rsq, IR_INTRINSIC_ALL_FLOATING | IR_INTRINSIC_MATRIX | IR_INTRINSIC_PROMOTE_ARG_FLOAT_RETURN_FLOAT, 1);

	// 8.3 Common Functions.
	make_intrinsic_genType(ir, state, "abs", ir_unop_abs, IR_INTRINSIC_ALL_FLOATING | IR_INTRINSIC_INT | IR_INTRINSIC_UINT | IR_INTRINSIC_MATRIX, 1);
	make_intrinsic_genType(ir, state, "sign", ir_unop_sign, IR_INTRINSIC_ALL_FLOATING | IR_INTRINSIC_INT | IR_INTRINSIC_UINT | IR_INTRINSIC_MATRIX, 1);
	make_intrinsic_genType(ir, state, "floor", ir_unop_floor, IR_INTRINSIC_ALL_FLOATING | IR_INTRINSIC_MATRIX, 1);
	if (state->bGenerateES)
	{
		MakeIntrinsicTrunc(ir, state);
	}
	else
	{
		make_intrinsic_genType(ir, state, "trunc", ir_unop_trunc, IR_INTRINSIC_ALL_FLOATING | IR_INTRINSIC_MATRIX, 1);
	}
	make_intrinsic_genType(ir, state, "round", ir_unop_round, IR_INTRINSIC_ALL_FLOATING | IR_INTRINSIC_MATRIX, 1);
	make_intrinsic_genType(ir, state, "ceil", ir_unop_ceil, IR_INTRINSIC_ALL_FLOATING | IR_INTRINSIC_MATRIX, 1);
	make_intrinsic_genType(ir, state, "frac", ir_unop_fract, IR_INTRINSIC_ALL_FLOATING | IR_INTRINSIC_MATRIX, 1);
	make_intrinsic_fmod(ir, state);
	make_intrinsic_modf(ir, state);
	make_intrinsic_genType(ir, state, "min", ir_binop_min, IR_INTRINSIC_ALL_FLOATING | IR_INTRINSIC_INT | IR_INTRINSIC_UINT | IR_INTRINSIC_MATRIX, 2);
	make_intrinsic_genType(ir, state, "max", ir_binop_max, IR_INTRINSIC_ALL_FLOATING | IR_INTRINSIC_INT | IR_INTRINSIC_UINT | IR_INTRINSIC_MATRIX, 2);
	make_intrinsic_genType(ir, state, "clamp", ir_ternop_clamp, IR_INTRINSIC_ALL_FLOATING | IR_INTRINSIC_INT | IR_INTRINSIC_UINT | IR_INTRINSIC_MATRIX, 3);
	
	MakeIntrinsicSaturate(ir, state, GLSL_TYPE_FLOAT);
	
	make_intrinsic_genType(ir, state, "lerp", ir_ternop_lerp, IR_INTRINSIC_ALL_FLOATING | IR_INTRINSIC_MATRIX, 3);
	make_intrinsic_genType(ir, state, "step", ir_binop_step, IR_INTRINSIC_ALL_FLOATING | IR_INTRINSIC_MATRIX, 2);
	make_intrinsic_genType(ir, state, "smoothstep", ir_ternop_smoothstep, IR_INTRINSIC_ALL_FLOATING | IR_INTRINSIC_MATRIX, 3);
	make_intrinsic_genType(ir, state, "isnan", ir_unop_isnan, IR_INTRINSIC_ALL_FLOATING | IR_INTRINSIC_INT_THRU | IR_INTRINSIC_UINT_THRU | IR_INTRINSIC_BOOL_THRU | IR_INTRINSIC_MATRIX | IR_INTRINSIC_RETURNS_BOOL, 1);
	make_intrinsic_genType(ir, state, "isinf", ir_unop_isinf, IR_INTRINSIC_ALL_FLOATING | IR_INTRINSIC_INT_THRU | IR_INTRINSIC_UINT_THRU | IR_INTRINSIC_BOOL_THRU | IR_INTRINSIC_MATRIX | IR_INTRINSIC_RETURNS_BOOL, 1);
	make_intrinsic_isfinite(ir, state);

	// 8.4 Geometric Functions.
	make_intrinsic_genType(ir, state, "length", ir_invalid_opcode, IR_INTRINSIC_ALL_FLOATING | IR_INTRINSIC_SCALAR, 1, 2, 4);	// only float2..float4, length(float) is handled on ast_function_expression.hir
	make_intrinsic_genType(ir, state, "distance", ir_invalid_opcode, IR_INTRINSIC_ALL_FLOATING | IR_INTRINSIC_SCALAR, 2, 2, 4);
	make_intrinsic_genType(ir, state, "dot", ir_binop_dot, IR_INTRINSIC_ALL_FLOATING | IR_INTRINSIC_SCALAR, 2, 1, 4);
	make_intrinsic_genType(ir, state, "cross", ir_binop_cross, IR_INTRINSIC_ALL_FLOATING, 2, 3, 3);
	make_intrinsic_genType(ir, state, "normalize", ir_unop_normalize, IR_INTRINSIC_ALL_FLOATING, 1, 2, 4);
	make_intrinsic_genType(ir, state, "faceforward", ir_invalid_opcode, IR_INTRINSIC_ALL_FLOATING, 3, 2, 4);
	make_intrinsic_genType(ir, state, "reflect", ir_invalid_opcode, IR_INTRINSIC_ALL_FLOATING, 2, 2, 4);
	make_intrinsic_refract(ir, state);

	// 8.5 Matrix Functions.
	if (state->LanguageSpec->SupportsDeterminantIntrinsic())
	{
		make_intrinsic_determinant(ir, state);
	}

	if (state->LanguageSpec->SupportsTransposeIntrinsic())
	{
		make_intrinsic_transpose(ir, state);
	}
	else
	{
		MakeIntrinsicTranspose(ir, state);
	}

	state->LanguageSpec->SetupLanguageIntrinsics(state, ir);

	// NOTE: The mul intrinsic would generate an explosion of function signatures.
	// It's behavior is hardcoded. See process_mul in ast_function.cpp.

	// 8.8 Fragment Processing Functions.
	make_intrinsic_genType(ir, state, "ddx", ir_unop_dFdx, IR_INTRINSIC_ALL_FLOATING, 1);
	//   make_intrinsic_genType(ir, state, "ddy",    ir_unop_dFdy,      IR_INTRINSIC_ALL_FLOATING, 1);	// defined separately above
	make_intrinsic_genType(ir, state, "ddx_fine", ir_unop_dFdxFine, IR_INTRINSIC_ALL_FLOATING, 1);
	make_intrinsic_genType(ir, state, "ddy_fine", ir_unop_dFdyFine, IR_INTRINSIC_ALL_FLOATING, 1);
	make_intrinsic_genType(ir, state, "ddx_coarse", ir_unop_dFdxCoarse, IR_INTRINSIC_ALL_FLOATING, 1);
	make_intrinsic_genType(ir, state, "ddy_coarse", ir_unop_dFdyCoarse, IR_INTRINSIC_ALL_FLOATING, 1);
	make_intrinsic_genType(ir, state, "fwidth", ir_invalid_opcode, IR_INTRINSIC_ALL_FLOATING, 1);

	// Others.
	make_intrinsic_genType(ir, state, "all", ir_unop_all, IR_INTRINSIC_BOOL | IR_INTRINSIC_RETURNS_BOOL | IR_INTRINSIC_SCALAR, 1, 2);
	make_intrinsic_genType(ir, state, "any", ir_unop_any, IR_INTRINSIC_BOOL | IR_INTRINSIC_RETURNS_BOOL | IR_INTRINSIC_SCALAR, 1, 2);
	make_intrinsic_genType(ir, state, "rcp", ir_unop_rcp, IR_INTRINSIC_ALL_FLOATING | IR_INTRINSIC_MATRIX, 1);
	make_intrinsic_clip(ir, state);

	make_intrinsic_emit_vertex(ir, state);
	make_intrinsic_end_primitive(ir, state);

	make_intrinsic_pack_functions(ir, state);
	make_intrinsic_genType(ir, state, "bitreverse", ir_unop_bitreverse, IR_INTRINSIC_INT | IR_INTRINSIC_UINT, 1);
	make_intrinsic_sm5_functions(ir, state);
	make_intrinsic_atomics(ir, state);
}
