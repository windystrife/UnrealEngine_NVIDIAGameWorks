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
* \file ir_constant_expression.cpp
* Evaluate and process constant valued expressions
*
* In GLSL, constant valued expressions are used in several places.  These
* must be processed and evaluated very early in the compilation process.
*
*    * Sizes of arrays
*    * Initializers for uniforms
*    * Initializers for \c const variables
*/

#include "ShaderCompilerCommon.h"
#include "ir.h"
#include "ir_visitor.h"
#include "glsl_types.h"
#include "ir_optimization.h"
#include "ir_function_inlining.h"
#include "macros.h"

static float dot(ir_constant* op0, ir_constant* op1)
{
	check(op0 && op0->type->is_float() && op1 && op1->type->is_float());

	float result = 0;
	for (unsigned c = 0; c < op0->type->components(); c++)
	{
		result += op0->value.f[c] * op1->value.f[c];
	}

	return result;
}

ir_constant* ir_rvalue::constant_expression_value()
{
	check(this->type->is_error());
	return NULL;
}

ir_constant* ir_expression::constant_expression_value()
{
	if (this->type->is_error())
	{
		return NULL;
	}

	ir_constant* op[Elements(this->operands)] = { NULL, };
	ir_constant_data data;

	memset(&data, 0, sizeof(data));

	for (unsigned operand = 0; operand < this->get_num_operands(); operand++)
	{
		op[operand] = this->operands[operand]->constant_expression_value();
		if (!op[operand])
		{
			return NULL;
		}
	}

	check(op[0]);
	if (op[1] != NULL)
	{
		check(op[0]->type->base_type == op[1]->type->base_type ||
			this->operation == ir_binop_lshift ||
			this->operation == ir_binop_rshift);
	}

	bool op0_scalar = op[0]->type->is_scalar();
	bool op1_scalar = op[1] != NULL && op[1]->type->is_scalar();

	bool op2_scalar = op[2] ? op[2]->type->is_scalar() : false;
	unsigned c2_inc = op2_scalar ? 0 : 1;

	/* When iterating over a vector or matrix's components, we want to increase
	* the loop counter.  However, for scalars, we want to stay at 0.
	*/
	unsigned c0_inc = op0_scalar ? 0 : 1;
	unsigned c1_inc = op1_scalar ? 0 : 1;
	unsigned components;
	if (op1_scalar || !op[1])
	{
		components = op[0]->type->components();
	}
	else
	{
		components = op[1]->type->components();
	}

	void *ctx = ralloc_parent(this);

	/* Handle array operations here, rather than below. */
	if (op[0]->type->is_array())
	{
		check(op[1] != NULL && op[1]->type->is_array());
		switch (this->operation)
		{
		case ir_binop_all_equal:
			return new(ctx)ir_constant(op[0]->has_value(op[1]));
		case ir_binop_any_nequal:
			return new(ctx)ir_constant(!op[0]->has_value(op[1]));
		default:
			break;
		}
		return NULL;
	}

	switch (this->operation)
	{
	case ir_unop_bit_not:
		switch (op[0]->type->base_type)
		{
		case GLSL_TYPE_INT:
			for (unsigned c = 0; c < components; c++)
			{
				data.i[c] = ~op[0]->value.i[c];
			}
			break;
		case GLSL_TYPE_UINT:
			for (unsigned c = 0; c < components; c++)
			{
				data.u[c] = ~op[0]->value.u[c];
			}
			break;
		default:
			check(0);
		}
		break;

	case ir_unop_logic_not:
		check(op[0]->type->base_type == GLSL_TYPE_BOOL);
		for (unsigned c = 0; c < op[0]->type->components(); c++)
		{
			data.b[c] = !op[0]->value.b[c];
		}
		break;

	case ir_unop_h2i:
		check(op[0]->type->base_type == GLSL_TYPE_HALF);
		for (unsigned c = 0; c < op[0]->type->components(); c++)
		{
			data.i[c] = (int)op[0]->value.f[c];
		}
		break;
	case ir_unop_f2i:
		check(op[0]->type->base_type == GLSL_TYPE_FLOAT);
		for (unsigned c = 0; c < op[0]->type->components(); c++)
		{
			data.i[c] = (int)op[0]->value.f[c];
		}
		break;
	case ir_unop_i2f:
	case ir_unop_i2h:
		check(op[0]->type->base_type == GLSL_TYPE_INT);
		for (unsigned c = 0; c < op[0]->type->components(); c++)
		{
			data.f[c] = (float)op[0]->value.i[c];
		}
		break;
	case ir_unop_u2h:
	case ir_unop_u2f:
		check(op[0]->type->base_type == GLSL_TYPE_UINT);
		for (unsigned c = 0; c < op[0]->type->components(); c++)
		{
			data.f[c] = (float)op[0]->value.u[c];
		}
		break;
	case ir_unop_f2h:
		check(op[0]->type->base_type == GLSL_TYPE_FLOAT);
		for (unsigned c = 0; c < op[0]->type->components(); c++)
		{
			data.f[c] = op[0]->value.f[c];
		}
		break;
	case ir_unop_f2u:
		check(op[0]->type->base_type == GLSL_TYPE_FLOAT);
		for (unsigned c = 0; c < op[0]->type->components(); c++)
		{
			data.u[c] = (unsigned)op[0]->value.f[c];
		}
		break;
	case ir_unop_b2h:
	case ir_unop_b2f:
		check(op[0]->type->base_type == GLSL_TYPE_BOOL);
		for (unsigned c = 0; c < op[0]->type->components(); c++)
		{
			data.f[c] = op[0]->value.b[c] ? 1.0F : 0.0F;
		}
		break;
	case ir_unop_h2b:
		check(op[0]->type->base_type == GLSL_TYPE_HALF);
		for (unsigned c = 0; c < op[0]->type->components(); c++)
		{
			data.b[c] = op[0]->value.f[c] != 0.0F ? true : false;
		}
		break;
	case ir_unop_f2b:
		check(op[0]->type->base_type == GLSL_TYPE_FLOAT);
		for (unsigned c = 0; c < op[0]->type->components(); c++)
		{
			data.b[c] = op[0]->value.f[c] != 0.0F ? true : false;
		}
		break;
	case ir_unop_b2i:
		check(op[0]->type->base_type == GLSL_TYPE_BOOL);
		for (unsigned c = 0; c < op[0]->type->components(); c++)
		{
			data.u[c] = op[0]->value.b[c] ? 1 : 0;
		}
		break;
	case ir_unop_i2b:
		check(op[0]->type->is_integer());
		for (unsigned c = 0; c < op[0]->type->components(); c++)
		{
			data.b[c] = op[0]->value.u[c] ? true : false;
		}
		break;
	case ir_unop_u2i:
		check(op[0]->type->base_type == GLSL_TYPE_UINT);
		for (unsigned c = 0; c < op[0]->type->components(); c++)
		{
			data.i[c] = op[0]->value.u[c];
		}
		break;
	case ir_unop_i2u:
		check(op[0]->type->base_type == GLSL_TYPE_INT);
		for (unsigned c = 0; c < op[0]->type->components(); c++)
		{
			data.u[c] = op[0]->value.i[c];
		}
		break;
	case ir_unop_b2u:
		check(op[0]->type->base_type == GLSL_TYPE_BOOL);
		for (unsigned c = 0; c < op[0]->type->components(); c++)
		{
			data.u[c] = (unsigned)op[0]->value.b[c];
		}
		break;
	case ir_unop_u2b:
		check(op[0]->type->base_type == GLSL_TYPE_UINT);
		for (unsigned c = 0; c < op[0]->type->components(); c++)
		{
			data.b[c] = (bool)op[0]->value.u[c];
		}
		break;
	case ir_unop_any:
		check(op[0]->type->is_boolean());
		data.b[0] = false;
		for (unsigned c = 0; c < op[0]->type->components(); c++)
		{
			if (op[0]->value.b[c])
			{
				data.b[0] = true;
			}
		}
		break;

	case ir_unop_all:
		check(op[0]->type->is_boolean());
		data.b[0] = true;
		for (unsigned c = 0; c < op[0]->type->components(); c++)
		{
			if (op[0]->value.b[c] == false)
			{
				data.b[0] = false;
			}
		}
		break;

	case ir_unop_trunc:
		check(op[0]->type->is_float());
		for (unsigned c = 0; c < op[0]->type->components(); c++)
		{
			data.f[c] = truncf(op[0]->value.f[c]);
		}
		break;

	case ir_unop_round:
		check(op[0]->type->is_float());
		for (unsigned c = 0; c < op[0]->type->components(); c++)
		{
			data.f[c] = (float)(IROUND(op[0]->value.f[c]));
		}
		break;

	case ir_unop_ceil:
		check(op[0]->type->is_float());
		for (unsigned c = 0; c < op[0]->type->components(); c++)
		{
			data.f[c] = ceilf(op[0]->value.f[c]);
		}
		break;

	case ir_unop_floor:
		check(op[0]->type->is_float());
		for (unsigned c = 0; c < op[0]->type->components(); c++)
		{
			data.f[c] = floorf(op[0]->value.f[c]);
		}
		break;

	case ir_unop_fract:
		for (unsigned c = 0; c < op[0]->type->components(); c++)
		{
			switch (this->type->base_type)
			{
			case GLSL_TYPE_UINT:
				data.u[c] = 0;
				break;
			case GLSL_TYPE_INT:
				data.i[c] = 0;
				break;
			case GLSL_TYPE_HALF:
			case GLSL_TYPE_FLOAT:
				data.f[c] = op[0]->value.f[c] - floor(op[0]->value.f[c]);
				break;
			default:
				check(0);
			}
		}
		break;

	case ir_unop_sin:
		check(op[0]->type->is_float());
		for (unsigned c = 0; c < op[0]->type->components(); c++)
		{
			data.f[c] = sinf(op[0]->value.f[c]);
		}
		break;

	case ir_unop_cos:
		check(op[0]->type->is_float());
		for (unsigned c = 0; c < op[0]->type->components(); c++)
		{
			data.f[c] = cosf(op[0]->value.f[c]);
		}
		break;

	case ir_unop_tan:
		check(op[0]->type->is_float());
		for (unsigned c = 0; c < op[0]->type->components(); c++)
		{
			data.f[c] = tanf(op[0]->value.f[c]);
		}
		break;

	case ir_unop_asin:
		check(op[0]->type->is_float());
		for (unsigned c = 0; c < op[0]->type->components(); c++)
		{
			data.f[c] = asinf(op[0]->value.f[c]);
		}
		break;

	case ir_unop_acos:
		check(op[0]->type->is_float());
		for (unsigned c = 0; c < op[0]->type->components(); c++)
		{
			data.f[c] = acosf(op[0]->value.f[c]);
		}
		break;

	case ir_unop_atan:
		check(op[0]->type->is_float());
		for (unsigned c = 0; c < op[0]->type->components(); c++)
		{
			data.f[c] = atanf(op[0]->value.f[c]);
		}
		break;

	case ir_unop_sinh:
		check(op[0]->type->is_float());
		for (unsigned c = 0; c < op[0]->type->components(); c++)
		{
			data.f[c] = sinhf(op[0]->value.f[c]);
		}
		break;

	case ir_unop_cosh:
		check(op[0]->type->is_float());
		for (unsigned c = 0; c < op[0]->type->components(); c++)
		{
			data.f[c] = coshf(op[0]->value.f[c]);
		}
		break;

	case ir_unop_tanh:
		check(op[0]->type->is_float());
		for (unsigned c = 0; c < op[0]->type->components(); c++)
		{
			data.f[c] = tanhf(op[0]->value.f[c]);
		}
		break;

	case ir_unop_neg:
		for (unsigned c = 0; c < op[0]->type->components(); c++)
		{
			switch (this->type->base_type)
			{
			case GLSL_TYPE_UINT:
				data.u[c] = -((int)op[0]->value.u[c]);
				break;
			case GLSL_TYPE_INT:
				data.i[c] = -op[0]->value.i[c];
				break;
			case GLSL_TYPE_HALF:
			case GLSL_TYPE_FLOAT:
				data.f[c] = -op[0]->value.f[c];
				break;
			default:
				check(0);
			}
		}
		break;

	case ir_unop_abs:
		for (unsigned c = 0; c < op[0]->type->components(); c++)
		{
			switch (this->type->base_type)
			{
			case GLSL_TYPE_UINT:
				data.u[c] = op[0]->value.u[c];
				break;
			case GLSL_TYPE_INT:
				data.i[c] = op[0]->value.i[c];
				if (data.i[c] < 0)
				{
					data.i[c] = -data.i[c];
				}
				break;
			case GLSL_TYPE_HALF:
			case GLSL_TYPE_FLOAT:
				data.f[c] = fabs(op[0]->value.f[c]);
				break;
			default:
				check(0);
			}
		}
		break;

	case ir_unop_sign:
		for (unsigned c = 0; c < op[0]->type->components(); c++)
		{
			switch (this->type->base_type)
			{
			case GLSL_TYPE_UINT:
				data.u[c] = op[0]->value.i[c] > 0;
				break;
			case GLSL_TYPE_INT:
				data.i[c] = (op[0]->value.i[c] > 0) - (op[0]->value.i[c] < 0);
				break;
			case GLSL_TYPE_HALF:
			case GLSL_TYPE_FLOAT:
				data.f[c] = float((op[0]->value.f[c] > 0) - (op[0]->value.f[c] < 0));
				break;
			default:
				check(0);
			}
		}
		break;

	case ir_unop_rcp:
		check(op[0]->type->is_float());
		for (unsigned c = 0; c < op[0]->type->components(); c++)
		{
			switch (this->type->base_type)
			{
			case GLSL_TYPE_UINT:
				if (op[0]->value.u[c] != 0.0)
				{
					data.u[c] = 1 / op[0]->value.u[c];
				}
				break;
			case GLSL_TYPE_INT:
				if (op[0]->value.i[c] != 0.0)
				{
					data.i[c] = 1 / op[0]->value.i[c];
				}
				break;
			case GLSL_TYPE_HALF:
			case GLSL_TYPE_FLOAT:
				if (op[0]->value.f[c] != 0.0)
				{
					data.f[c] = 1.0F / op[0]->value.f[c];
				}
				break;
			default:
				check(0);
			}
		}
		break;

	case ir_unop_rsq:
		check(op[0]->type->is_float() || op[0]->type->is_integer());
		for (unsigned c = 0; c < op[0]->type->components(); c++)
		{
			data.f[c] = 1.0F / sqrtf(op[0]->type->is_integer() ? op[0]->value.u[c] : op[0]->value.f[c]);
		}
		break;

	case ir_unop_sqrt:
		check(op[0]->type->is_float() || op[0]->type->is_integer());
		for (unsigned c = 0; c < op[0]->type->components(); c++)
		{
			data.f[c] = sqrtf(op[0]->type->is_integer() ? op[0]->value.u[c] : op[0]->value.f[c]);
		}
		break;

	case ir_unop_exp:
		check(op[0]->type->is_float() || op[0]->type->is_integer());
		for (unsigned c = 0; c < op[0]->type->components(); c++)
		{
			data.f[c] = expf(op[0]->type->is_integer() ? op[0]->value.i[c] : op[0]->value.f[c]);
		}
		break;

	case ir_unop_exp2:
		check(op[0]->type->is_float() || op[0]->type->is_integer());
		for (unsigned c = 0; c < op[0]->type->components(); c++)
		{
			data.f[c] = exp2f(op[0]->type->is_integer() ? op[0]->value.i[c] : op[0]->value.f[c]);
		}
		break;

	case ir_unop_log:
		check(op[0]->type->is_float() || op[0]->type->is_integer());
		for (unsigned c = 0; c < op[0]->type->components(); c++)
		{
			data.f[c] = logf(op[0]->type->is_integer() ? op[0]->value.u[c] : op[0]->value.f[c]);
		}
		break;

	case ir_unop_log2:
		check(op[0]->type->is_float() || op[0]->type->is_integer());
		for (unsigned c = 0; c < op[0]->type->components(); c++)
		{
			data.f[c] = log2f(op[0]->type->is_integer() ? op[0]->value.u[c] : op[0]->value.f[c]);
		}
		break;

	case ir_unop_normalize:
		check(op[0]->type->is_float());
		{
			float mag = 0.0f;
			for (unsigned c = 0; c < op[0]->type->components(); c++)
			{
				mag += op[0]->value.f[c] * op[0]->value.f[c];
			}
			mag = sqrtf(mag);
			for (unsigned c = 0; c < op[0]->type->components(); c++)
			{
				data.f[c] = op[0]->value.f[c] / mag;
			}
		}
		break;

	case ir_unop_dFdx:
	case ir_unop_dFdy:
		check(op[0]->type->is_float());
		for (unsigned c = 0; c < op[0]->type->components(); c++)
		{
			data.f[c] = 0.0;
		}
		break;
		
	case ir_unop_saturate:
		check(op[0]->type->is_float());
		for (unsigned c = 0; c < op[0]->type->components(); c++)
		{
			data.f[c] = CLAMP(op[0]->value.f[c], 0.0f, 1.0f);
		}
		break;

	case ir_binop_pow:
		check(op[0]->type->is_float());
		for (unsigned c = 0; c < op[0]->type->components(); c++)
		{
			data.f[c] = powf(op[0]->value.f[c], op[1]->value.f[c]);
		}
		break;

	case ir_binop_atan2:
		check(op[0]->type->is_float());
		check(op[1] && op[1]->type->base_type == op[0]->type->base_type);
		for (unsigned c = 0; c < op[0]->type->components(); c++)
		{
			data.f[c] = atan2f(op[0]->value.f[c], op[1]->value.f[c]);
		}
		break;

	case ir_binop_cross:
		check(op[0]->type == glsl_type::vec3_type || op[0]->type == glsl_type::half3_type);
		check(op[1] && (op[1]->type == glsl_type::vec3_type || op[1]->type == glsl_type::half3_type));
		data.f[0] = (op[0]->value.f[1] * op[1]->value.f[2]) - (op[0]->value.f[2] * op[1]->value.f[1]);
		data.f[1] = (op[0]->value.f[2] * op[1]->value.f[0]) - (op[0]->value.f[0] * op[1]->value.f[2]);
		data.f[2] = (op[0]->value.f[0] * op[1]->value.f[1]) - (op[0]->value.f[1] * op[1]->value.f[0]);
		break;

	case ir_binop_dot:
		data.f[0] = dot(op[0], op[1]);
		break;

	case ir_binop_min:
		check(op[1]);
		check(op[0]->type == op[1]->type || op0_scalar || op1_scalar);
		for (unsigned c = 0, c0 = 0, c1 = 0;
			c < components;
			c0 += c0_inc, c1 += c1_inc, c++)
		{
			switch (op[0]->type->base_type)
			{
			case GLSL_TYPE_UINT:
				data.u[c] = MIN2(op[0]->value.u[c0], op[1]->value.u[c1]);
				break;
			case GLSL_TYPE_INT:
				data.i[c] = MIN2(op[0]->value.i[c0], op[1]->value.i[c1]);
				break;
			case GLSL_TYPE_HALF:
			case GLSL_TYPE_FLOAT:
				data.f[c] = MIN2(op[0]->value.f[c0], op[1]->value.f[c1]);
				break;
			default:
				check(0);
			}
		}

		break;
	case ir_binop_max:
		check(op[1]);
		check(op[0]->type == op[1]->type || op0_scalar || op1_scalar);
		for (unsigned c = 0, c0 = 0, c1 = 0;
			c < components;
			c0 += c0_inc, c1 += c1_inc, c++)
		{
			switch (op[0]->type->base_type)
			{
			case GLSL_TYPE_UINT:
				data.u[c] = MAX2(op[0]->value.u[c0], op[1]->value.u[c1]);
				break;
			case GLSL_TYPE_INT:
				data.i[c] = MAX2(op[0]->value.i[c0], op[1]->value.i[c1]);
				break;
			case GLSL_TYPE_HALF:
			case GLSL_TYPE_FLOAT:
				data.f[c] = MAX2(op[0]->value.f[c0], op[1]->value.f[c1]);
				break;
			default:
				check(0);
			}
		}
		break;

	case ir_binop_add:
		check(op[1]);
		check(op[0]->type == op[1]->type || op0_scalar || op1_scalar);
		for (unsigned c = 0, c0 = 0, c1 = 0;
			c < components;
			c0 += c0_inc, c1 += c1_inc, c++)
		{
			switch (op[0]->type->base_type)
			{
			case GLSL_TYPE_UINT:
				data.u[c] = op[0]->value.u[c0] + op[1]->value.u[c1];
				break;
			case GLSL_TYPE_INT:
				data.i[c] = op[0]->value.i[c0] + op[1]->value.i[c1];
				break;
			case GLSL_TYPE_HALF:
			case GLSL_TYPE_FLOAT:
				data.f[c] = op[0]->value.f[c0] + op[1]->value.f[c1];
				break;
			default:
				check(0);
			}
		}

		break;
	case ir_binop_sub:
		check(op[1]);
		check(op[0]->type == op[1]->type || op0_scalar || op1_scalar);
		for (unsigned c = 0, c0 = 0, c1 = 0;
			c < components;
			c0 += c0_inc, c1 += c1_inc, c++)
		{
			switch (op[0]->type->base_type)
			{
			case GLSL_TYPE_UINT:
				data.u[c] = op[0]->value.u[c0] - op[1]->value.u[c1];
				break;
			case GLSL_TYPE_INT:
				data.i[c] = op[0]->value.i[c0] - op[1]->value.i[c1];
				break;
			case GLSL_TYPE_HALF:
			case GLSL_TYPE_FLOAT:
				data.f[c] = op[0]->value.f[c0] - op[1]->value.f[c1];
				break;
			default:
				check(0);
			}
		}

		break;
	case ir_binop_mul:
		/* Check for equal types, or unequal types involving scalars */
		check(op[1]);
		if ((op[0]->type == op[1]->type) || op0_scalar || op1_scalar)
		{
			for (unsigned c = 0, c0 = 0, c1 = 0;
				c < components;
				c0 += c0_inc, c1 += c1_inc, c++)
			{
				switch (op[0]->type->base_type)
				{
				case GLSL_TYPE_UINT:
					data.u[c] = op[0]->value.u[c0] * op[1]->value.u[c1];
					break;
				case GLSL_TYPE_INT:
					data.i[c] = op[0]->value.i[c0] * op[1]->value.i[c1];
					break;
				case GLSL_TYPE_HALF:
				case GLSL_TYPE_FLOAT:
					data.f[c] = op[0]->value.f[c0] * op[1]->value.f[c1];
					break;
				default:
					check(0);
				}
			}
		}
		break;
	case ir_binop_div:
		/* FINISHME: Emit warning when division-by-zero is detected. */
		check(op[1]);
		check(op[0]->type == op[1]->type || op0_scalar || op1_scalar);
		for (unsigned c = 0, c0 = 0, c1 = 0;
			c < components;
			c0 += c0_inc, c1 += c1_inc, c++)
		{

			switch (op[0]->type->base_type)
			{
			case GLSL_TYPE_UINT:
				if (op[1]->value.u[c1] == 0)
				{
					data.u[c] = 0;
				}
				else
				{
					data.u[c] = op[0]->value.u[c0] / op[1]->value.u[c1];
				}
				break;
			case GLSL_TYPE_INT:
				if (op[1]->value.i[c1] == 0)
				{
					data.i[c] = 0;
				}
				else
				{
					data.i[c] = op[0]->value.i[c0] / op[1]->value.i[c1];
				}
				break;
			case GLSL_TYPE_HALF:
			case GLSL_TYPE_FLOAT:
				data.f[c] = op[0]->value.f[c0] / op[1]->value.f[c1];
				break;
			default:
				check(0);
			}
		}

		break;
	case ir_binop_mod:
		/* FINISHME: Emit warning when division-by-zero is detected. */
		check(op[1]);
		check(op[0]->type == op[1]->type || op0_scalar || op1_scalar);
		for (unsigned c = 0, c0 = 0, c1 = 0;
			c < components;
			c0 += c0_inc, c1 += c1_inc, c++)
		{
			switch (op[0]->type->base_type)
			{
			case GLSL_TYPE_UINT:
				if (op[1]->value.u[c1] == 0)
				{
					data.u[c] = 0;
				}
				else
				{
					data.u[c] = op[0]->value.u[c0] % op[1]->value.u[c1];
				}
				break;
			case GLSL_TYPE_INT:
				if (op[1]->value.i[c1] == 0)
				{
					data.i[c] = 0;
				}
				else
				{
					data.i[c] = op[0]->value.i[c0] % op[1]->value.i[c1];
				}
				break;
			case GLSL_TYPE_HALF:
			case GLSL_TYPE_FLOAT:
				/* We don't use fmod because it rounds toward zero; GLSL specifies
				* the use of floor.
				*/
				data.f[c] = op[0]->value.f[c0] - op[1]->value.f[c1]
					* floorf(op[0]->value.f[c0] / op[1]->value.f[c1]);
				break;
			default:
				check(0);
			}
		}

		break;

	case ir_binop_logic_and:
		check(op[1]);
		check(op[0]->type->base_type == GLSL_TYPE_BOOL);
		for (unsigned c = 0; c < op[0]->type->components(); c++)
			data.b[c] = op[0]->value.b[c] && op[1]->value.b[c];
		break;
	case ir_binop_logic_xor:
		check(op[0]->type->base_type == GLSL_TYPE_BOOL);
		for (unsigned c = 0; c < op[0]->type->components(); c++)
			data.b[c] = op[0]->value.b[c] ^ op[1]->value.b[c];
		break;
	case ir_binop_logic_or:
		check(op[0]->type->base_type == GLSL_TYPE_BOOL);
		for (unsigned c = 0; c < op[0]->type->components(); c++)
			data.b[c] = op[0]->value.b[c] || op[1]->value.b[c];
		break;

	case ir_binop_less:
		check(op[1]);
		check(op[0]->type == op[1]->type);
		for (unsigned c = 0; c < op[0]->type->components(); c++)
		{
			switch (op[0]->type->base_type)
			{
			case GLSL_TYPE_UINT:
				data.b[0] = op[0]->value.u[0] < op[1]->value.u[0];
				break;
			case GLSL_TYPE_INT:
				data.b[0] = op[0]->value.i[0] < op[1]->value.i[0];
				break;
			case GLSL_TYPE_HALF:
			case GLSL_TYPE_FLOAT:
				data.b[0] = op[0]->value.f[0] < op[1]->value.f[0];
				break;
			default:
				check(0);
			}
		}
		break;
	case ir_binop_greater:
		check(op[1]);
		check(op[0]->type == op[1]->type);
		for (unsigned c = 0; c < op[0]->type->components(); c++)
		{
			switch (op[0]->type->base_type)
			{
			case GLSL_TYPE_UINT:
				data.b[c] = op[0]->value.u[c] > op[1]->value.u[c];
				break;
			case GLSL_TYPE_INT:
				data.b[c] = op[0]->value.i[c] > op[1]->value.i[c];
				break;
			case GLSL_TYPE_HALF:
			case GLSL_TYPE_FLOAT:
				data.b[c] = op[0]->value.f[c] > op[1]->value.f[c];
				break;
			default:
				check(0);
			}
		}
		break;
	case ir_binop_lequal:
		check(op[1]);
		check(op[0]->type == op[1]->type);
		for (unsigned c = 0; c < op[0]->type->components(); c++)
		{
			switch (op[0]->type->base_type)
			{
			case GLSL_TYPE_UINT:
				data.b[0] = op[0]->value.u[0] <= op[1]->value.u[0];
				break;
			case GLSL_TYPE_INT:
				data.b[0] = op[0]->value.i[0] <= op[1]->value.i[0];
				break;
			case GLSL_TYPE_HALF:
			case GLSL_TYPE_FLOAT:
				data.b[0] = op[0]->value.f[0] <= op[1]->value.f[0];
				break;
			default:
				check(0);
			}
		}
		break;
	case ir_binop_gequal:
		check(op[1]);
		check(op[0]->type == op[1]->type);
		for (unsigned c = 0; c < op[0]->type->components(); c++)
		{
			switch (op[0]->type->base_type)
			{
			case GLSL_TYPE_UINT:
				data.b[0] = op[0]->value.u[0] >= op[1]->value.u[0];
				break;
			case GLSL_TYPE_INT:
				data.b[0] = op[0]->value.i[0] >= op[1]->value.i[0];
				break;
			case GLSL_TYPE_HALF:
			case GLSL_TYPE_FLOAT:
				data.b[0] = op[0]->value.f[0] >= op[1]->value.f[0];
				break;
			default:
				check(0);
			}
		}
		break;
	case ir_binop_equal:
		check(op[1]);
		check(op[0]->type == op[1]->type);
		for (unsigned c = 0; c < components; c++)
		{
			switch (op[0]->type->base_type)
			{
			case GLSL_TYPE_UINT:
				data.b[c] = op[0]->value.u[c] == op[1]->value.u[c];
				break;
			case GLSL_TYPE_INT:
				data.b[c] = op[0]->value.i[c] == op[1]->value.i[c];
				break;
			case GLSL_TYPE_HALF:
			case GLSL_TYPE_FLOAT:
				data.b[c] = op[0]->value.f[c] == op[1]->value.f[c];
				break;
			case GLSL_TYPE_BOOL:
				data.b[c] = op[0]->value.b[c] == op[1]->value.b[c];
				break;
			default:
				check(0);
			}
		}
		break;
	case ir_binop_nequal:
		check(op[1]);
		check(op[0]->type == op[1]->type);
		for (unsigned c = 0; c < components; c++)
		{
			switch (op[0]->type->base_type)
			{
			case GLSL_TYPE_UINT:
				data.b[c] = op[0]->value.u[c] != op[1]->value.u[c];
				break;
			case GLSL_TYPE_INT:
				data.b[c] = op[0]->value.i[c] != op[1]->value.i[c];
				break;
			case GLSL_TYPE_HALF:
			case GLSL_TYPE_FLOAT:
				data.b[c] = op[0]->value.f[c] != op[1]->value.f[c];
				break;
			case GLSL_TYPE_BOOL:
				data.b[c] = op[0]->value.b[c] != op[1]->value.b[c];
				break;
			default:
				check(0);
			}
		}
		break;
	case ir_binop_all_equal:
		check(op[1]);
		data.b[0] = op[0]->has_value(op[1]);
		break;
	case ir_binop_any_nequal:
		check(op[1]);
		data.b[0] = !op[0]->has_value(op[1]);
		break;

	case ir_binop_lshift:
		for (unsigned c = 0, c0 = 0, c1 = 0;
			c < components;
			c0 += c0_inc, c1 += c1_inc, c++)
		{

			check(op[1]);
			if (op[0]->type->base_type == GLSL_TYPE_INT &&
				op[1]->type->base_type == GLSL_TYPE_INT)
			{
				data.i[c] = op[0]->value.i[c0] << op[1]->value.i[c1];

			}
			else if (op[0]->type->base_type == GLSL_TYPE_INT &&
				op[1]->type->base_type == GLSL_TYPE_UINT)
			{
				data.i[c] = op[0]->value.i[c0] << op[1]->value.u[c1];

			}
			else if (op[0]->type->base_type == GLSL_TYPE_UINT &&
				op[1]->type->base_type == GLSL_TYPE_INT)
			{
				data.u[c] = op[0]->value.u[c0] << op[1]->value.i[c1];

			}
			else if (op[0]->type->base_type == GLSL_TYPE_UINT &&
				op[1]->type->base_type == GLSL_TYPE_UINT)
			{
				data.u[c] = op[0]->value.u[c0] << op[1]->value.u[c1];
			}
		}
		break;

	case ir_binop_rshift:
		for (unsigned c = 0, c0 = 0, c1 = 0;
			c < components;
			c0 += c0_inc, c1 += c1_inc, c++)
		{
			check(op[1]);
			if (op[0]->type->base_type == GLSL_TYPE_INT &&
				op[1]->type->base_type == GLSL_TYPE_INT)
			{
				data.i[c] = op[0]->value.i[c0] >> op[1]->value.i[c1];
			}
			else if (op[0]->type->base_type == GLSL_TYPE_INT &&
				op[1]->type->base_type == GLSL_TYPE_UINT)
			{
				data.i[c] = op[0]->value.i[c0] >> op[1]->value.u[c1];

			}
			else if (op[0]->type->base_type == GLSL_TYPE_UINT &&
				op[1]->type->base_type == GLSL_TYPE_INT)
			{
				data.u[c] = op[0]->value.u[c0] >> op[1]->value.i[c1];

			}
			else if (op[0]->type->base_type == GLSL_TYPE_UINT &&
				op[1]->type->base_type == GLSL_TYPE_UINT)
			{
				data.u[c] = op[0]->value.u[c0] >> op[1]->value.u[c1];
			}
		}
		break;

	case ir_binop_bit_and:
		for (unsigned c = 0, c0 = 0, c1 = 0;
			c < components;
			c0 += c0_inc, c1 += c1_inc, c++)
		{
			switch (op[0]->type->base_type)
			{
			case GLSL_TYPE_INT:
				data.i[c] = op[0]->value.i[c0] & op[1]->value.i[c1];
				break;
			case GLSL_TYPE_UINT:
				data.u[c] = op[0]->value.u[c0] & op[1]->value.u[c1];
				break;
			default:
				check(0);
			}
		}
		break;

	case ir_binop_bit_or:
		for (unsigned c = 0, c0 = 0, c1 = 0;
			c < components;
			c0 += c0_inc, c1 += c1_inc, c++)
		{
			switch (op[0]->type->base_type)
			{
			case GLSL_TYPE_INT:
				data.i[c] = op[0]->value.i[c0] | op[1]->value.i[c1];
				break;
			case GLSL_TYPE_UINT:
				data.u[c] = op[0]->value.u[c0] | op[1]->value.u[c1];
				break;
			default:
				check(0);
			}
		}
		break;

	case ir_binop_bit_xor:
		for (unsigned c = 0, c0 = 0, c1 = 0;
			c < components;
			c0 += c0_inc, c1 += c1_inc, c++)
		{

			switch (op[0]->type->base_type)
			{
			case GLSL_TYPE_INT:
				data.i[c] = op[0]->value.i[c0] ^ op[1]->value.i[c1];
				break;
			case GLSL_TYPE_UINT:
				data.u[c] = op[0]->value.u[c0] ^ op[1]->value.u[c1];
				break;
			default:
				check(0);
			}
		}
		break;

	case ir_unop_isnan:
	{
		for (unsigned c = 0, c0 = 0, c1 = 0;
			c < components;
			c0 += c0_inc, c1 += c1_inc, c++)
		{
			data.b[c] = (op[0]->value.u[c0] & 0x7FFFFFFF) > 0x7F800000;
		}
	}
		break;

	case ir_unop_isinf:
	{
		for (unsigned c = 0, c0 = 0, c1 = 0;
			c < components;
			c0 += c0_inc, c1 += c1_inc, c++)
		{
			data.b[c] = (op[0]->value.u[c0] & 0x7FFFFFFF) == 0x7F800000;
		}
	}
		break;

	case ir_unop_fasu:
	{
		for (unsigned c = 0, c0 = 0, c1 = 0;
			c < components;
			c0 += c0_inc, c1 += c1_inc, c++)
		{
			data.u[c] = op[0]->value.u[c0];
		}
	}
		break;

	case ir_unop_fasi:
	{
		for (unsigned c = 0, c0 = 0, c1 = 0;
			c < components;
			c0 += c0_inc, c1 += c1_inc, c++)
		{
			data.i[c] = op[0]->value.i[c0];
		}
	}
		break;

	case ir_unop_iasf:
	{
		for (unsigned c = 0, c0 = 0, c1 = 0;
			c < components;
			c0 += c0_inc, c1 += c1_inc, c++)
		{
			data.f[c] = op[0]->value.f[c0];
		}
	}
		break;

	case ir_unop_uasf:
	{
		for (unsigned c = 0, c0 = 0, c1 = 0;
			c < components;
			c0 += c0_inc, c1 += c1_inc, c++)
		{
			data.f[c] = op[0]->value.f[c0];
		}
	}
		break;

	case ir_unop_bitreverse:
	{
		const int bits = sizeof(int)* 8;
		for (unsigned c = 0, c0 = 0, c1 = 0;
			c < components;
			c0 += c0_inc, c1 += c1_inc, c++)
		{
			switch (op[0]->type->base_type)
			{
			case GLSL_TYPE_INT:
				data.i[c] = 0;
				for (int i = 0; i < bits; i++)
				{
					data.i[c] |= ((op[0]->value.i[c0] >> i) & 0x1) << (bits - 1 - i);
				}
				break;
			case GLSL_TYPE_UINT:
				data.u[c] = 0;
				for (int i = 0; i < bits; i++)
				{
					data.u[c] |= ((op[0]->value.u[c0] >> i) & 0x1) << (bits - 1 - i);
				}
				break;
			default:
				check(0);
			}
		}
	}
		break;

	case ir_unop_bitcount:
	{
		const int bits = sizeof(int)* 8;
		for (unsigned c = 0, c0 = 0, c1 = 0;
			c < components;
			c0 += c0_inc, c1 += c1_inc, c++)
		{
			switch (op[0]->type->base_type)
			{
			case GLSL_TYPE_INT:
				data.i[c] = 0;
				for (int i = 0; i < bits; i++)
				{
					data.i[c] += ((op[0]->value.i[c0] >> i) & 0x1);
				}
				break;
			case GLSL_TYPE_UINT:
				data.i[c] = 0;
				for (int i = 0; i < bits; i++)
				{
					data.i[c] += ((op[0]->value.u[c0] >> i) & 0x1);
				}
				break;
			default:
				check(0);
			}
		}
	}
		break;

	case ir_unop_msb:
	{
		const int bits = sizeof(int)* 8;
		for (unsigned c = 0, c0 = 0, c1 = 0;
			c < components;
			c0 += c0_inc, c1 += c1_inc, c++)
		{
			switch (op[0]->type->base_type)
			{
			case GLSL_TYPE_INT:
				data.i[c] = -1;
				for (int i = bits - 1; i >= 0; i--)
				{
					if (((op[0]->value.i[c0] >> i) & 0x1) != 0)
					{
						data.i[c] = i;
						break;
					}
				}
				break;
			case GLSL_TYPE_UINT:
				data.i[c] = -1;
				for (int i = bits - 1; i >= 0; i--)
				{
					if (((op[0]->value.u[c0] >> i) & 0x1) != 0)
					{
						data.i[c] = i;
						break;
					}
				}
				break;
			default:
				check(0);
			}
		}
	}
		break;

	case ir_unop_lsb:
	{
		const int bits = sizeof(int)* 8;
		for (unsigned c = 0, c0 = 0, c1 = 0;
			c < components;
			c0 += c0_inc, c1 += c1_inc, c++)
		{
			switch (op[0]->type->base_type)
			{
			case GLSL_TYPE_INT:
				data.i[c] = -1;
				for (int i = 0; i < bits; i++)
				{
					if (((op[0]->value.i[c0] >> i) & 0x1) != 0)
					{
						data.i[c] = i;
						break;
					}
				}
				break;
			case GLSL_TYPE_UINT:
				data.i[c] = -1;
				for (int i = 0; i < bits; i++)
				{
					if (((op[0]->value.u[c0] >> i) & 0x1) != 0)
					{
						data.i[c] = i;
						break;
					}
				}
				break;
			default:
				check(0);
			}
		}
	}
		break;

	case ir_binop_step:
	{
		for (unsigned c = 0, c0 = 0, c1 = 0;
			c < components;
			c0 += c0_inc, c1 += c1_inc, c++)
		{
			data.f[c] = op[1]->value.f[c1] < op[0]->value.f[c0] ? 0.0f : 1.0f;
		}
	}
		break;

	case ir_ternop_lerp:
	{
		check(type->is_float());
		check(op[0]->type->base_type == type->base_type);
		check(op[1] && op[1]->type->base_type == type->base_type);
		check(op[2] && op[2]->type->base_type == type->base_type);

		for (unsigned c = 0, c0 = 0, c1 = 0, c2 = 0;
			c < components;
			c0 += c0_inc, c1 += c1_inc, c2 += c2_inc, c++)
		{
			float x = op[0]->value.f[c0];
			float y = op[1]->value.f[c1];
			float s = op[2]->value.f[c2];
			data.f[c] = x * (1.0f - s) + y * s;
		}
	}
		break;

	case ir_ternop_smoothstep:
	{
		check(type->is_float());
		check(op[0]->type->base_type == type->base_type);
		check(op[1] && op[1]->type->base_type == type->base_type);
		check(op[2] && op[2]->type->base_type == type->base_type);

		for (unsigned c = 0, c0 = 0, c1 = 0, c2 = 0;
			c < components;
			c0 += c0_inc, c1 += c1_inc, c2 += c2_inc, c++)
		{
			float e0 = op[0]->value.f[c0];
			float e1 = op[1]->value.f[c1];
			float x = op[2]->value.f[c2];
			float t = (x - e0) / (e1 - e0);
			t = (t < 0.0f) ? 0.0f : t;
			t = (t > 1.0f) ? 1.0f : t;
			data.f[c] = t * t * (3.0f - 2.0f * t);
		}
	}
		break;

	case ir_ternop_clamp:
	{
		for (unsigned c = 0, c0 = 0, c1 = 0, c2 = 0;
			c < components;
			c0 += c0_inc, c1 += c1_inc, c2 += c2_inc, c++)
		{
			switch (op[0]->type->base_type)
			{
			case GLSL_TYPE_UINT:
				data.u[c] = MAX2(op[0]->value.u[c0], op[1]->value.u[c1]);
				data.u[c] = MIN2(data.u[c], op[2]->value.u[c2]);
				break;
			case GLSL_TYPE_INT:
				data.i[c] = MAX2(op[0]->value.i[c0], op[1]->value.i[c1]);
				data.i[c] = MIN2(data.i[c], op[2]->value.i[c2]);
				break;
			case GLSL_TYPE_HALF:
			case GLSL_TYPE_FLOAT:
				data.f[c] = MAX2(op[0]->value.f[c0], op[1]->value.f[c1]);
				data.f[c] = MIN2(data.f[c], op[2]->value.f[c2]);
				break;
			default:
				check(0);
			}
		}
	}
		break;
		
	case ir_ternop_fma:
	{
		check(op[2]);
		check(op[1]);
		if ((op[0]->type == op[1]->type) && (op[0]->type == op[2]->type))
		{
			for (unsigned c = 0, c0 = 0, c1 = 0, c2 = 0;
				 c < components;
				 c0 += c0_inc, c1 += c1_inc, c2 += c2_inc, c++)
			{
				switch (op[0]->type->base_type)
				{
					case GLSL_TYPE_UINT:
					data.u[c] = (op[0]->value.u[c0] * op[1]->value.u[c1]) + op[2]->value.u[c2];
					break;
					case GLSL_TYPE_INT:
					data.i[c] = (op[0]->value.i[c0] * op[1]->value.i[c1]) + op[2]->value.i[c2];
					break;
					case GLSL_TYPE_HALF:
					case GLSL_TYPE_FLOAT:
					data.f[c] = (op[0]->value.f[c0] * op[1]->value.f[c1]) + op[2]->value.f[c2];
					break;
					default:
					check(0);
				}
			}
		}
	}
		break;

	case ir_quadop_vector:
		for (unsigned c = 0; c < this->type->vector_elements; c++)
		{
			switch (this->type->base_type)
			{
			case GLSL_TYPE_INT:
				data.i[c] = op[c]->value.i[0];
				break;
			case GLSL_TYPE_UINT:
				data.u[c] = op[c]->value.u[0];
				break;
			case GLSL_TYPE_HALF:
			case GLSL_TYPE_FLOAT:
				data.f[c] = op[c]->value.f[0];
				break;
			default:
				check(0);
			}
		}
		break;

	default:
		/* FINISHME: Should handle all expression types. */
		return NULL;
	}

	ir_constant* Constant = new(ctx)ir_constant(this->type, &data);
	if (!Constant->is_finite())
	{
		// Debug point
		int i = 0;
		++i;
	}
	return Constant;
}


ir_constant* ir_texture::constant_expression_value()
{
	/* texture lookups aren't constant expressions */
	return NULL;
}


ir_constant* ir_swizzle::constant_expression_value()
{
	ir_constant* v = this->val->constant_expression_value();

	if (v != NULL)
	{
		ir_constant_data data = { { 0 } };

		const unsigned swiz_idx[4] =
		{
			this->mask.x, this->mask.y, this->mask.z, this->mask.w
		};

		for (unsigned i = 0; i < this->mask.num_components; i++)
		{
			switch (v->type->base_type)
			{
			case GLSL_TYPE_UINT:
			case GLSL_TYPE_INT:   data.u[i] = v->value.u[swiz_idx[i]]; break;
			case GLSL_TYPE_HALF:
			case GLSL_TYPE_FLOAT: data.f[i] = v->value.f[swiz_idx[i]]; break;
			case GLSL_TYPE_BOOL:  data.b[i] = v->value.b[swiz_idx[i]]; break;
			default:              check(!"Should not get here."); break;
			}
		}

		void *ctx = ralloc_parent(this);
		return new(ctx)ir_constant(this->type, &data);
	}
	return NULL;
}


ir_constant* ir_dereference_variable::constant_expression_value()
{
	/* This may occur during compile and var->type is glsl_type::error_type */
	if (!var)
	{
		return NULL;
	}

	/* The constant_value of a uniform variable is its initializer,
	* not the lifetime constant value of the uniform.
	*/
	if (var->mode == ir_var_uniform)
	{
		return NULL;
	}

	if (!var->constant_value)
	{
		return NULL;
	}

	return var->constant_value->clone(ralloc_parent(var), NULL);
}


ir_constant* ir_dereference_array::constant_expression_value()
{
	ir_constant* array = this->array->constant_expression_value();
	ir_constant* idx = this->array_index->constant_expression_value();

	if ((array != NULL) && (idx != NULL))
	{
		void *ctx = ralloc_parent(this);
		if (array->type->is_matrix())
		{
			/* Array access of a matrix results in a vector.
			*/
			const unsigned column = idx->value.u[0];

			const glsl_type *const column_type = array->type->column_type();

			/* Offset in the constant matrix to the first element of the column
			* to be extracted.
			*/
			const unsigned mat_idx = column * column_type->vector_elements;

			ir_constant_data data = { { 0 } };

			switch (column_type->base_type)
			{
			case GLSL_TYPE_UINT:
			case GLSL_TYPE_INT:
				for (unsigned i = 0; i < column_type->vector_elements; i++)
					data.u[i] = array->value.u[mat_idx + i];

				break;

			case GLSL_TYPE_HALF:
			case GLSL_TYPE_FLOAT:
				for (unsigned i = 0; i < column_type->vector_elements; i++)
				{
					data.f[i] = array->value.f[mat_idx + i];
				}
				break;

			default:
				check(!"Should not get here.");
				break;
			}

			return new(ctx)ir_constant(column_type, &data);
		}
		else if (array->type->is_vector())
		{
			const unsigned component = idx->value.u[0];

			return new(ctx)ir_constant(array, component);
		}
		else
		{
			const unsigned index = idx->value.u[0];
			return array->get_array_element(index)->clone(ctx, NULL);
		}
	}
	return NULL;
}


ir_constant* ir_dereference_image::constant_expression_value()
{
	// image data cannot be constant
	return NULL;
}

ir_constant* ir_dereference_record::constant_expression_value()
{
	ir_constant* v = this->record->constant_expression_value();

	return (v != NULL) ? v->get_record_field(this->field) : NULL;
}


ir_constant* ir_assignment::constant_expression_value()
{
	/* FINISHME: Handle CEs involving assignment (return RHS) */
	return NULL;
}


ir_constant* ir_constant::constant_expression_value()
{
	return this;
}


ir_constant* ir_call::constant_expression_value()
{
	return NULL;
}


ir_constant* ir_function_signature::constant_expression_value(exec_list *actual_parameters)
{
	return NULL;
}
