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
#include "glsl_types.h"
#include "ir.h"

enum { INVALID_PARAMETER_LIST = -1 };

/**
* \brief Check if two parameter lists match.
*
* \param list_a Parameters of the function definition.
* \param list_b Actual parameters passed to the function.
* \see matching_signature()
*/
static uint64_t rank_parameter_lists(const exec_list *list_a, const exec_list *list_b)
{
	uint64_t const_promotions = 0;
	uint64_t const_demotions = 0;
	uint64_t int_conversions = 0;
	uint64_t i2f_conversions = 0;
	uint64_t i2h_conversions = 0;
	uint64_t f2i_conversions = 0;
	uint64_t f2h_conversions = 0;
	uint64_t h2f_conversions = 0;
	uint64_t scalar_promotions = 0;
	uint64_t truncations = 0;

	const exec_node *node_a = list_a->head;
	const exec_node *node_b = list_b->head;

	for (/* empty */
		; !node_a->is_tail_sentinel()
		; node_a = node_a->next, node_b = node_b->next)
	{
		/* If all of the parameters from the other parameter list have been
		* exhausted, the lists have different length and, by definition,
		* do not match.
		*/
		if (node_b->is_tail_sentinel())
		{
			return INVALID_PARAMETER_LIST;
		}

		const ir_variable *const param = (ir_variable *)node_a;	// Signature
		const ir_rvalue *const actual = (ir_rvalue *)node_b;

		if (param->type == actual->type)
		{
			continue;
		}

		/* Try to find an implicit conversion from actual to param. */
		switch ((enum ir_variable_mode)(param->mode))
		{
		case ir_var_auto:
		case ir_var_uniform:
		case ir_var_temporary:
			/* These are all error conditions.  It is invalid for a parameter to
			* a function to be declared as auto (not in, out, or inout) or
			* as uniform.
			*/
			check(0);
			return INVALID_PARAMETER_LIST;

		case ir_var_const_in:
		case ir_var_in:
			if (!actual->type->can_implicitly_convert_to(param->type))
			{
				return INVALID_PARAMETER_LIST;
			}
			break;

		case ir_var_out:
			if (!param->type->can_implicitly_convert_to(actual->type))
			{
				return INVALID_PARAMETER_LIST;
			}
			break;

		case ir_var_inout:
			if (!actual->type->can_implicitly_convert_to(param->type) ||
				!param->type->can_implicitly_convert_to(actual->type))
			{
				return INVALID_PARAMETER_LIST;
			}

		case ir_var_ref:
			/* ref parameters require an exact match */
			return INVALID_PARAMETER_LIST;

		default:
			check(false);
			return INVALID_PARAMETER_LIST;
		}

		if (actual->type->base_type == GLSL_TYPE_FLOAT && param->type->base_type == GLSL_TYPE_HALF)
		{
			if (actual->ir_type == ir_type_constant)
			{
				const_demotions++;
			}
			else
			{
				f2h_conversions++;
			}
		}
		else if (actual->type->base_type == GLSL_TYPE_HALF && param->type->base_type == GLSL_TYPE_FLOAT)
		{
			// Half to floats constants don't lose precision
			if (actual->ir_type == ir_type_constant)
			{
				const_promotions++;
			}
			else
			{
				h2f_conversions++;
			}
		}
		else if (actual->type->is_float() && !param->type->is_float())
		{
			f2i_conversions++;
		}
		else if (!actual->type->is_float() && param->type->is_float())
		{
			// Non-floats to floats constants don't lose precision
			if (actual->ir_type == ir_type_constant)
			{
				const_promotions++;
			}
			else
			{
				if (param->type->base_type == GLSL_TYPE_HALF)
				{
					i2h_conversions++;
				}
				else
				{
					i2f_conversions++;
				}
			}
		}
		else if (actual->type->base_type != param->type->base_type)
		{
			int_conversions++;
		}

		if (actual->type->components() > param->type->components())
		{
			truncations++;
		}
		else if (actual->type->is_scalar() && !param->type->is_scalar())
		{
			scalar_promotions++;
		}
	}

	/* If all of the parameters from the other parameter list have been
	* exhausted, the lists have different length and, by definition, do not
	* match.
	*/
	if (!node_b->is_tail_sentinel())
	{
		return INVALID_PARAMETER_LIST;
	}

	// Lower Match number number means better fit (0 means parameters match exactly), so do worse matches first
	uint64_t Match = 0;
	const uint64_t NumBits = 6;
	const uint64_t Mask = (1ULL << 6ULL) - 1;
	Match = (Match << NumBits) | (truncations & Mask);
	Match = (Match << NumBits) | (scalar_promotions & Mask);
	Match = (Match << NumBits) | (f2h_conversions & Mask);
	Match = (Match << NumBits) | (f2i_conversions & Mask);
	Match = (Match << NumBits) | (i2h_conversions & Mask);
	Match = (Match << NumBits) | (i2f_conversions & Mask);
	Match = (Match << NumBits) | (h2f_conversions & Mask);
	Match = (Match << NumBits) | (int_conversions & Mask);
	Match = (Match << NumBits) | (const_demotions & Mask);
	Match = (Match << NumBits) | (const_promotions & Mask);
	return Match;
}


ir_function_signature* ir_function::matching_signature(const exec_list *actual_parameters)
{
	bool is_exact;
	return matching_signature(actual_parameters, &is_exact);
}

ir_function_signature* ir_function::matching_signature(const exec_list *actual_parameters, bool *is_exact)
{
	ir_function_signature *match = NULL;
	uint64_t match_result = (uint64_t)INVALID_PARAMETER_LIST;
	bool is_ambiguous = false;

	foreach_iter(exec_list_iterator, iter, signatures)
	{
		ir_function_signature *const sig = (ir_function_signature *)iter.get();

		uint64_t this_match = rank_parameter_lists(&sig->parameters, actual_parameters);

		if (this_match == 0)
		{
			// Exact matches get priority.
			*is_exact = true;
			return sig;
		}
		else if (this_match < match_result)
		{
			match = sig;
			match_result = this_match;
			is_ambiguous = false;
		}
		else if (this_match == match_result)
		{
			is_ambiguous = true;
		}
	}

	/* There is no exact match (we would have returned it by now).  If there
	* are multiple inexact matches, the call is ambiguous, which is an error.
	*
	* FINISHME: Report a decent error.  Returning NULL will likely result in
	* FINISHME: a "no matching signature" error; it should report that the
	* FINISHME: call is ambiguous.  But reporting errors from here is hard.
	*/
	*is_exact = false;

	if (is_ambiguous)
	{
		return NULL;
	}

	return match;
}


static bool parameter_lists_match_exact(const exec_list *list_a, const exec_list *list_b)
{
	const exec_node *node_a = list_a->head;
	const exec_node *node_b = list_b->head;

	for (/* empty */
		; !node_a->is_tail_sentinel() && !node_b->is_tail_sentinel()
		; node_a = node_a->next, node_b = node_b->next)
	{
		ir_variable *a = (ir_variable *)node_a;
		ir_variable *b = (ir_variable *)node_b;

		/* If the types of the parameters do not match, the parameters lists
		* are different.
		*/
		if (a->type != b->type)
		{
			return false;
		}
	}

	/* Unless both lists are exhausted, they differ in length and, by
	* definition, do not match.
	*/
	return (node_a->is_tail_sentinel() == node_b->is_tail_sentinel());
}

ir_function_signature* ir_function::exact_matching_signature(const exec_list *actual_parameters)
{
	foreach_iter(exec_list_iterator, iter, signatures)
	{
		ir_function_signature *const sig =
			(ir_function_signature *)iter.get();

		if (parameter_lists_match_exact(&sig->parameters, actual_parameters))
		{
			return sig;
		}
	}
	return NULL;
}
