// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#include "ShaderFormatVectorVM.h"
#include "CoreMinimal.h"
#include "hlslcc.h"
#include "hlslcc_private.h"
#include "VectorVMBackend.h"

PRAGMA_DISABLE_SHADOW_VARIABLE_WARNINGS
#include "glsl_parser_extras.h"
PRAGMA_POP

#include "hash_table.h"
#include "ir_rvalue_visitor.h"
#include "PackUniformBuffers.h"
#include "IRDump.h"
#include "OptValueNumbering.h"
#include "ir_optimization.h"
#include "ir_expression_flattening.h"
#include "ir.h"

#include "VectorVM.h"

/** Removes any assignments that don't actually map to a VM op but just move some data around. We look for refs and grab the source data direct. */
class ir_propagate_non_expressions_visitor final : public ir_rvalue_visitor
{
	_mesa_glsl_parse_state *parse_state;

	struct var_info
	{
		int32 latest_expr_assign[16];
		int32 latest_non_expr_assign[16];
		var_info()
		{
			FMemory::Memset(latest_expr_assign, 0xFF, sizeof(latest_expr_assign));
			FMemory::Memset(latest_non_expr_assign, 0xFF, sizeof(latest_non_expr_assign));
		}
	};
	TMap<ir_variable*, var_info> var_info_map;

	TArray<ir_assignment*> assignments;

	int num_expr;
	bool progress;
	TArray<ir_assignment*> non_expr_assignments;

public:
	ir_propagate_non_expressions_visitor(_mesa_glsl_parse_state *in_state)
	{
		parse_state = in_state;
		num_expr = 0;
		progress = false;
	}

	virtual ~ir_propagate_non_expressions_visitor()
	{
	}

	unsigned get_component_from_matrix_array_deref(ir_dereference_array* array_deref)
	{
		check(array_deref);
		check(array_deref->variable_referenced()->type->is_matrix());
		ir_constant* index = array_deref->array_index->as_constant();
		check((index->type == glsl_type::uint_type || index->type == glsl_type::int_type) && index->type->is_scalar());
		unsigned deref_idx = index->type == glsl_type::uint_type ? index->value.u[0] : index->value.i[0];
		return deref_idx * array_deref->variable_referenced()->type->vector_elements;
	}

	virtual void handle_rvalue(ir_rvalue **rvalue)
	{
		if (rvalue && *rvalue && !in_assignee)
		{
			ir_rvalue** to_replace = rvalue;
			ir_dereference* deref = (*rvalue)->as_dereference();
			ir_dereference_array* array_deref = (*rvalue)->as_dereference_array();
			ir_swizzle* swiz = (*rvalue)->as_swizzle();

			ir_variable* search_var = (*rvalue)->variable_referenced();
			unsigned search_comp = 0;
			if (swiz)
			{
				if (ir_dereference_array* swiz_array_deref = swiz->val->as_dereference_array())
				{
					search_comp = get_component_from_matrix_array_deref(swiz_array_deref);
				}
				search_comp += swiz->mask.x;
			}
			else if (array_deref)
			{
				//We can only handle matrix array derefs but these will have an outer swizzle that we'll work with. 
				check(array_deref->array->type->is_matrix());
				search_var = nullptr;
			}
			else if(!deref || !deref->type->is_scalar())
			{
				//If we're not a deref or we're not a straight scalar deref then we should leave this alone.
				search_var = nullptr;
			}

			//Search to see if this deref matches any of the non-expression assignments LHS. If so then clone the rhs in it's place.

			var_info* varinfo = var_info_map.Find(search_var);
			if (varinfo)
			{
				//Is there a previous non_expr assignment after any containing expressions?
				//If so, copy that in place of this rvalue.
				if (varinfo->latest_expr_assign[search_comp] < varinfo->latest_non_expr_assign[search_comp])
				{
					ir_assignment* assign = assignments[varinfo->latest_non_expr_assign[search_comp]];
					check(assign->rhs->as_expression() == nullptr);
					check(assign->rhs->as_swizzle() || assign->rhs->as_dereference_variable() || assign->rhs->as_constant());
					check(assign->rhs->type->is_scalar());//All assignments must be scalar at this point!
					ir_rvalue* new_rval = assign->rhs->clone(ralloc_parent(assign), nullptr);
					(*rvalue) = new_rval;
					progress = true;
				}
			}
		}
	}

	virtual ir_visitor_status visit_enter(ir_expression*)
	{
		num_expr++;
		return visit_continue;
	}

	virtual ir_visitor_status visit_enter(ir_assignment *assign)
	{
		if (assign->condition)
		{
			_mesa_glsl_error(parse_state, "conditional assignment in instruction stream");
			return visit_stop;
		}

		num_expr = 0;
		return visit_continue;
	}

	virtual ir_visitor_status visit_leave(ir_assignment *assign)
	{
		check(assign->next && assign->prev);
		ir_variable* lhs = assign->lhs->variable_referenced();
		var_info& varinfo = var_info_map.FindOrAdd(lhs);
		
		int32 assign_idx = assignments.Add(assign);

		//Add any new temp or auto assignments. These will be grabbed later to use in replacements in HandleRValue.

		unsigned assign_comp = 0;
		if (ir_dereference_array* array_deref = assign->lhs->as_dereference_array())
		{
			assign_comp += get_component_from_matrix_array_deref(array_deref);
		}

		unsigned write_mask = assign->write_mask;
		int32 components_written = 0;
		while (write_mask)
		{
			if (write_mask & 0x1)
			{
				++components_written;
				ir_variable_mode mode = lhs->mode;
				if (num_expr == 0)
				{
					varinfo.latest_non_expr_assign[assign_comp] = assign_idx;
					assign->remove();
				}
				else
				{
					check(mode == ir_var_temporary || mode == ir_var_auto);//We can only perform expressions on temp or auto variables.
					varinfo.latest_expr_assign[assign_comp] = assign_idx;
				}
			}
			++assign_comp;
			write_mask >>= 1;
		}
		check(components_written == 1);
		check(assign->rhs->type->is_scalar());
		return ir_rvalue_visitor::visit_leave(assign);
	}

	static void run(exec_list *ir, _mesa_glsl_parse_state *state)
	{
		bool progress = false;
		do
		{
			ir_propagate_non_expressions_visitor propagate_non_expressions_visitor(state);
			visit_list_elements(&propagate_non_expressions_visitor, ir);

			progress = propagate_non_expressions_visitor.progress;

			//vm_debug_print("== propagate non expressions - BEFORE DEADCODE ==\n");
			//vm_debug_dump(ir, state);

			progress = do_dead_code(ir, false) || progress;
			progress = do_dead_code_local(ir) || progress;
		} while (progress);
	}
};

void vm_propagate_non_expressions_visitor(exec_list* ir, _mesa_glsl_parse_state* state)
{
	ir_propagate_non_expressions_visitor::run(ir, state);
}

//////////////////////////////////////////////////////////////////////////
/** Replaces any array accesses to matrices with the equivalant swizzle. */
class ir_matrix_array_access_to_swizzles final : public ir_rvalue_visitor
{
public:

	bool progress;
	ir_matrix_array_access_to_swizzles()
	{
		progress = false;
	}

	virtual ~ir_matrix_array_access_to_swizzles()
	{
	}

	unsigned get_component_from_matrix_array_deref(ir_dereference_array* array_deref)
	{
		check(array_deref);
		check(array_deref->variable_referenced()->type->is_matrix());
		ir_constant* index = array_deref->array_index->as_constant();
		check((index->type == glsl_type::uint_type || index->type == glsl_type::int_type) && index->type->is_scalar());
		unsigned deref_idx = index->type == glsl_type::uint_type ? index->value.u[0] : index->value.i[0];
		return deref_idx * array_deref->variable_referenced()->type->vector_elements;
	}

	virtual void handle_rvalue(ir_rvalue **rvalue)
	{
		if (rvalue && *rvalue)
		{
			if (ir_swizzle* swiz = (*rvalue)->as_swizzle())
			{
				check(swiz->type->is_scalar());//Post scalarize so this should be all scalar.				
				if (ir_dereference_array* array_deref = swiz->val->as_dereference_array())
				{
					swiz->mask.x += get_component_from_matrix_array_deref(array_deref);
					swiz->val = array_deref->array->clone(ralloc_parent(swiz), nullptr);
				}
			}
		}
	}
	
	virtual ir_visitor_status visit_leave(ir_assignment *assign)
	{		
		//Remove matrix array access on the lhs too.
		if (ir_dereference_array* array_deref = assign->lhs->as_dereference_array())
		{			
			assign->write_mask <<= get_component_from_matrix_array_deref(array_deref);
			assign->lhs = array_deref->array->clone(ralloc_parent(assign), nullptr)->as_dereference();
		}
		return ir_rvalue_visitor::visit_leave(assign);
	}

	static void run(exec_list *ir)
	{
		bool progress = false;
		do
		{
			ir_matrix_array_access_to_swizzles v;
			visit_list_elements(&v, ir);

			progress = v.progress;

			//vm_debug_print("== propagate non expressions - BEFORE DEADCODE ==\n");
			//vm_debug_dump(ir, state);

			//progress = do_dead_code(ir, false) || progress;
			//progress = do_dead_code_local(ir) || progress;
		} while (progress);
	}
};

struct MatrixVectors
{
	ir_variable* v[4];
};

class ir_matrices_to_vectors : public ir_rvalue_visitor
{
	bool progress;

	TMap<class ir_variable*, MatrixVectors> MatrixVectorMap;
public:
	ir_matrices_to_vectors()
	{
		progress = false;
	}

	virtual ~ir_matrices_to_vectors()
	{
	}

	virtual void handle_rvalue(ir_rvalue **rvalue)
	{
		if (rvalue && *rvalue)
		{		
			check((*rvalue)->as_dereference_array() == nullptr);
			if (ir_swizzle* swiz = (*rvalue)->as_swizzle())
			{
				ir_variable* var = swiz->variable_referenced();
				if (var->type->is_matrix())
				{
					MatrixVectors& mv = MatrixVectorMap.FindChecked(var);
					unsigned v_idx = swiz->mask.x / 4;
					swiz->mask.x %= 4;					
					void* p = ralloc_parent(swiz);
					swiz->val = new(p) ir_dereference_variable(mv.v[v_idx]);
				}
			}
		}	
	}

	virtual ir_visitor_status visit_leave(ir_assignment *assign)
	{
		ir_variable* var = assign->lhs->variable_referenced();
		if (var->type->is_matrix())
		{
			MatrixVectors& mv = MatrixVectorMap.FindChecked(var);
			ir_variable* v = nullptr;
			unsigned v_idx = 0;
			while (assign->write_mask > 0)
			{
				if ((assign->write_mask & 15) != 0)
				{
					v = mv.v[v_idx];
					check(assign->write_mask >> 4 == 0);
					break;
				}
				++v_idx;
				assign->write_mask >>= 4;
			}
			check(v);
			void* p = ralloc_parent(assign);
			assign->lhs = new(p) ir_dereference_variable(v);
		}
		return ir_rvalue_visitor::visit_leave(assign);
	}

	virtual ir_visitor_status visit(ir_variable* var)
	{
		if (var->type->is_matrix())
		{
			MatrixVectors* mv = MatrixVectorMap.Find(var);
			if (!mv)
			{
				mv = &MatrixVectorMap.Add(var);
				void* p = ralloc_parent(var);

				check(var->next && var->prev);

				const glsl_type* vtype = var->type->column_type();
				const char *name = ralloc_asprintf(p, "%s_%s", var->name,"col0");				
				mv->v[0] = new(p) ir_variable(vtype, name, var->mode);
				var->insert_before(mv->v[0]);
				name = ralloc_asprintf(p, "%s_%s", var->name, "col1");
				mv->v[1] = new(p) ir_variable(vtype, name, var->mode);
				var->insert_before(mv->v[1]);
				name = ralloc_asprintf(p, "%s_%s", var->name, "col2");
				mv->v[2] = new(p) ir_variable(vtype, name, var->mode);
				var->insert_before(mv->v[2]);
				name = ralloc_asprintf(p, "%s_%s", var->name, "col3");
				mv->v[3] = new(p) ir_variable(vtype, name, var->mode);
				var->insert_before(mv->v[3]);
				var->remove();
			}
		}
		return visit_continue;
	}

	static void run(exec_list *ir, _mesa_glsl_parse_state* state)
	{
		bool progress = false;
		do
		{
			ir_matrices_to_vectors v;
			
			//Have to manually visit uniforms first.
			for (SCBuffer& buff : state->CBuffersOriginal)
			{
				for (SCBufferMember& member : buff.Members)
				{
					member.Var->accept(&v);
				}
			}

			visit_list_elements(&v, ir);

			progress = v.progress;

			//vm_debug_print("== propagate non expressions - BEFORE DEADCODE ==\n");
			//vm_debug_dump(ir, state);

			//progress = do_dead_code(ir, false) || progress;
			//progress = do_dead_code_local(ir) || progress;
		} while (progress);
	}
};

void vm_matrices_to_vectors(exec_list* ir, _mesa_glsl_parse_state* state)
{	
	ir_matrix_array_access_to_swizzles::run(ir);
	ir_matrices_to_vectors::run(ir, state);
}
