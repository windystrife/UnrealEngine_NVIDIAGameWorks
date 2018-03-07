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
* \file ir_variable_refcount.h
*
* Provides a visitor which produces a list of variables referenced,
* how many times they were referenced and assigned, and whether they
* were defined in the scope.
*/

#include "ir.h"
#include "ir_visitor.h"
#include "glsl_types.h"

#include <map>

class ir_variable_refcount_entry : public exec_node
{
public:
	ir_variable_refcount_entry(ir_variable *var);

	ir_variable *var; /* The key: the variable's pointer. */
	ir_assignment *assign; /* An assignment to the variable, if any */
	ir_assignment *last_assign; /* The last assignment to the variable. */
	ir_call *call; /* The function call that assigns to this variable, if any */

	/** Number of times the variable is referenced, including assignments. */
	unsigned referenced_count;

	/** Number of times the variable is assigned. */
	unsigned assigned_count;

	bool declaration; /* If the variable had a decl in the instruction stream */
};

class ir_variable_refcount_visitor : public ir_hierarchical_visitor
{
public:
	ir_variable_refcount_visitor(void)
	{
		this->mem_ctx = ralloc_context(NULL);
		this->control_flow_depth = 0;
	}

	~ir_variable_refcount_visitor(void)
	{
		ralloc_free(this->mem_ctx);
	}

	virtual ir_visitor_status visit(ir_variable *);
	virtual ir_visitor_status visit(ir_dereference_variable *);

	virtual ir_visitor_status visit_enter(ir_function_signature *);
	virtual ir_visitor_status visit_leave(ir_assignment *);
	virtual ir_visitor_status visit_leave(ir_call *);

	virtual ir_visitor_status visit_enter(ir_if *);
	virtual ir_visitor_status visit_leave(ir_if *);
	virtual ir_visitor_status visit_enter(ir_loop *);
	virtual ir_visitor_status visit_leave(ir_loop *);

	ir_variable_refcount_entry *get_variable_entry(ir_variable *var);

	/* List of ir_variable_refcount_entry */
	std::map<ir_variable*, ir_variable_refcount_entry*> Variables;
	unsigned control_flow_depth;
	void *mem_ctx;
};
