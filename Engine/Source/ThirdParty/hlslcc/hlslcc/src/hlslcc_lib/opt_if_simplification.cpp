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
* \file opt_if_simplification.cpp
*
* Moves constant branches of if statements out to the surrounding
* instruction stream.
*/

#include "ShaderCompilerCommon.h"
#include "ir.h"
#include "IRDump.h"

class ir_if_simplification_visitor : public ir_hierarchical_visitor
{
public:
	ir_if_simplification_visitor()
	{
		this->made_progress = false;
	}

	ir_visitor_status visit_leave(ir_if *);
	ir_visitor_status visit_enter(ir_assignment *);

	bool made_progress;
};

/* We only care about the top level "if" instructions, so don't
* descend into expressions.
*/
ir_visitor_status
ir_if_simplification_visitor::visit_enter(ir_assignment *ir)
{
	(void)ir;
	return visit_continue_with_parent;
}

bool do_if_simplification(exec_list *instructions)
{
	ir_if_simplification_visitor v;

	v.run(instructions);
	return v.made_progress;
}


ir_visitor_status ir_if_simplification_visitor::visit_leave(ir_if *ir)
{
	// Remove if statements that do nothing.
	if (ir->then_instructions.is_empty() && ir->else_instructions.is_empty())
	{
		// It's possible that the condition has side-effects. So declare a
		// temporary, and assign the result of the condition to it in place of
		// the if statement. If the condition statement has no side-effects
		// further optimization passes will remove it.
		void* ctx = ralloc_parent(ir);
		ir_variable* tmp = new(ctx)ir_variable(ir->condition->type, NULL, ir_var_temporary);
		ir_assignment* assign = new(ctx)ir_assignment(
			new(ctx)ir_dereference_variable(tmp), ir->condition);
		ir->insert_before(tmp);
		ir->insert_before(assign);
		ir->remove();
		this->made_progress = true;
	}
	else
	{
		if (ir->then_instructions.is_empty())
		{
			/* Turn:
			*     if (cond) {
			*     } else {
			*         do_work();
			*     }
			* into :
			*     if (!cond)
			*         do_work();
			* which avoids control flow for "else" (which is usually more
			* expensive than normal operations), and the "not" can usually be
			* folded into the generation of "cond" anyway.
			*/
			ir->condition = new(ralloc_parent(ir->condition)) ir_expression(ir_unop_logic_not, ir->condition);
			ir->else_instructions.move_nodes_to(&ir->then_instructions);
			this->made_progress = true;
		}

		/* FINISHME: Ideally there would be a way to note that the condition results
		* FINISHME: in a constant before processing both of the other subtrees.
		* FINISHME: This can probably be done with some flags, but it would take
		* FINISHME: some work to get right.
		*/
		ir_constant *condition_constant = ir->condition->constant_expression_value();
		if (condition_constant)
		{
			/* Move the contents of the one branch of the conditional
			* that matters out.
			*/
			if (condition_constant->value.b[0])
			{
				foreach_iter(exec_list_iterator, then_iter, ir->then_instructions)
				{
					ir_instruction *then_ir = (ir_instruction *)then_iter.get();
					ir->insert_before(then_ir);
				}
			}
			else
			{
				foreach_iter(exec_list_iterator, else_iter, ir->else_instructions)
				{
					ir_instruction *else_ir = (ir_instruction *)else_iter.get();
					ir->insert_before(else_ir);
				}
			}
			ir->remove();
			this->made_progress = true;
		}
	}

	return visit_continue;
}
