// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

// This code is largely based on that in ir_print_glsl_visitor.cpp from
// glsl-optimizer.
// https://github.com/aras-p/glsl-optimizer
// The license for glsl-optimizer is reproduced below:

/*
	GLSL Optimizer is licensed according to the terms of the MIT license:

	Copyright (C) 1999-2007  Brian Paul   All Rights Reserved.
	Copyright (C) 2010-2011  Unity Technologies All Rights Reserved.

	Permission is hereby granted, free of charge, to any person obtaining a
	copy of this software and associated documentation files (the "Software"),
	to deal in the Software without restriction, including without limitation
	the rights to use, copy, modify, merge, publish, distribute, sublicense,
	and/or sell copies of the Software, and to permit persons to whom the
	Software is furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included
	in all copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
	OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
	BRIAN PAUL BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
	AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
	CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include "ShaderCompilerCommon.h"
//@todo-rco: Remove STL!
#include <sstream>

#include "glsl_parser_extras.h"
#include "ir.h"
#include "ir_visitor.h"
#include "IRDump.h"
#include "hlslcc_private.h"

// Print to the debugger in visual studio?
#define DUMP_TO_VISUAL_STUDIO 0
#if DUMP_TO_VISUAL_STUDIO
	#define irdump_printf dprintf
#else
	#define irdump_printf printf
#endif

static inline void irdump_flush()
{
	fflush(stdout);
}

DebugPrintVisitor::DebugPrintVisitor(bool bSingleEntry) :
	Indentation(0),
	bIRVarEOL(!bSingleEntry),
	ID(0),
	bDumpBuiltInFunctions(false)
{
}

DebugPrintVisitor::~DebugPrintVisitor()
{
}

void DebugPrintVisitor::visit(ir_rvalue* ir)
{
	check(0);
}

void DebugPrintVisitor::visit(ir_variable* ir)
{
	PrintID(ir);
	switch (ir->mode)
	{
	case ir_var_auto:
		irdump_printf("/*A*/");
		break;
	case ir_var_uniform:
		irdump_printf("/*U*/");
		break;
	case ir_var_in:
		irdump_printf("/*I*/");
		break;
	case ir_var_out:
		irdump_printf("/*O*/");
		break;
	case ir_var_inout:
		irdump_printf("/*IO*/");
		break;
	case ir_var_const_in:
		irdump_printf("/*CI*/");
		break;
	case ir_var_temporary:
		irdump_printf("/*T*/");
		break;
	case ir_var_shared:
		irdump_printf("/*S*/");
		break;
	case ir_var_ref:
		irdump_printf("/*R*/");
		break;
	case ir_var_ref_image:
		irdump_printf("/*RI*/");
		break;
	}
	PrintType(ir->type);
	irdump_printf(" %s", GetVarName(ir).c_str());
	if (bIRVarEOL)
	{
		if (ir->semantic)
		{
			irdump_printf(" : %s", ir->semantic);
		}
		irdump_printf(";\n");
	}
}

void DebugPrintVisitor::visit(ir_function_signature* ir)
{
	PrintType(ir->return_type);
	irdump_printf(" %s(", ir->function_name());

	bool bFirst = true;
	foreach_iter(exec_list_iterator, iter, ir->parameters)
	{
		ir_variable *const inst = (ir_variable *) iter.get();
		if (bFirst)
		{
			bFirst = false;
		}
		else
		{
			irdump_printf(", ");
		}
		bool bPrevEOL = bIRVarEOL;
		bIRVarEOL = false;
		inst->accept(this);
		bIRVarEOL = bPrevEOL;
	}

	irdump_printf(")\n{\n");
	Indentation++;

	foreach_iter(exec_list_iterator, iter, ir->body)
	{
		ir_instruction *const inst = (ir_instruction *) iter.get();
		Indent();
		inst->accept(this);
	}
	Indentation--;
	irdump_printf("}\n");
}

void DebugPrintVisitor::visit(ir_function* ir)
{
	bool bFirst = true;
	foreach_iter(exec_list_iterator, iter, *ir)
	{
		ir_function_signature *const sig = (ir_function_signature*)iter.get();
		if (!sig->is_builtin || bDumpBuiltInFunctions)
		{
			if (bFirst)
			{
				PrintID(ir);
				bFirst = false;
			}

			Indent();
			sig->accept(this);
			irdump_printf("\n");
		}
	}
}

void DebugPrintVisitor::visit(ir_expression* ir)
{
	PrintID(ir);

	irdump_printf("(");
	irdump_printf("/*");
	PrintType(ir->type);
	irdump_printf("*/");

	if (ir->get_num_operands() == 1)
	{
		irdump_printf("%s ", ir->operator_string());
		ir->operands[0]->accept(this);
	}
	else
	{
		if (ir->get_num_operands() == 3)
		{
			irdump_printf("%s", ir->operator_string());
			for (int i = 0; i < ir->get_num_operands(); ++i)
			{
				irdump_printf("%s", (i == 0) ? "" : ", ");
				ir->operands[i]->accept(this);
			}
		}
		else
		{
			ir->operands[0]->accept(this);
			irdump_printf(" %s ", ir->operator_string());
			ir->operands[1]->accept(this);
		}
	}
/*
	irdump_printf("(%d expression ", ir->id);

	PrintType(ir->type);

	irdump_printf(" %s ", ir->operator_string());

	for (unsigned i = 0; i < ir->get_num_operands(); i++) {
		ir->operands[i]->accept(this);
	}

*/

	irdump_printf(")");
}

void DebugPrintVisitor::visit(ir_texture* ir)
{
	PrintID(ir);
	ir->sampler->accept(this);
	switch (ir->op)
	{
		case ir_tex:
			irdump_printf(".Sample(");
			if (ir->SamplerState)
			{
				ir->SamplerState->accept(this);
				irdump_printf(",");
			}
			ir->coordinate->accept(this);
			if (ir->shadow_comparitor)
			{
				irdump_printf(",");
				ir->shadow_comparitor->accept(this);
			}

			if (ir->offset)
			{
				irdump_printf(",");
				ir->offset->accept(this);
			}
			break;

		case ir_txm:
			irdump_printf(".get_num_mip_levels()");
			break;

		case ir_txb:
			irdump_printf(".SampleBias(");
			if (ir->SamplerState)
			{
				ir->SamplerState->accept(this);
				irdump_printf(",");
			}
			ir->coordinate->accept(this);
			ir->lod_info.bias->accept(this);
			if (ir->offset)
			{
				irdump_printf(",");
				ir->offset->accept(this);
			}
			break;

		case ir_txd:
			irdump_printf(".SampleGrad(");
			if (ir->SamplerState)
			{
				ir->SamplerState->accept(this);
				irdump_printf(",");
			}
			ir->coordinate->accept(this);
			irdump_printf(",");
			ir->lod_info.grad.dPdx->accept(this);
			irdump_printf(",");
			ir->lod_info.grad.dPdy->accept(this);
			if (ir->offset)
			{
				irdump_printf(",");
				ir->offset->accept(this);
			}
			break;

		case ir_txl:
			irdump_printf(".SampleLevel(");
			if (ir->SamplerState)
			{
				ir->SamplerState->accept(this);
				irdump_printf(",");
			}
			ir->coordinate->accept(this);
			if (ir->shadow_comparitor)
			{
				irdump_printf(",");
				ir->shadow_comparitor->accept(this);
			}
			irdump_printf(",");
			ir->lod_info.lod->accept(this);
			if (ir->offset)
			{
				irdump_printf(",");
				ir->offset->accept(this);
			}
			break;

		case ir_txf:
			irdump_printf(".Load(");
			ir->coordinate->accept(this);
			if (ir->lod_info.lod)
			{
				irdump_printf(",");
				ir->lod_info.lod->accept(this);
			}
			break;

		case ir_txs:
			irdump_printf(".GetDimensions(");
			ir->lod_info.lod->accept(this);
			irdump_printf(")");
			break;

		default:
			check(0);
	}
	irdump_printf(")");
}

void DebugPrintVisitor::visit(ir_swizzle* ir)
{
	PrintID(ir);

	ir->val->accept(this);
	irdump_printf(".");
	if (ir->val->type && ir->val->type->is_matrix())
	{
		for (unsigned i = 0; i < ir->mask.num_components; i++)
		{
			int32 Row = ir->mask.x /ir->val->type->matrix_columns;
			int32 Column = ir->mask.x % ir->val->type->matrix_columns;
			irdump_printf("_m%d%d", Row, Column);
		}
	}
	else
	{
		const unsigned swiz[4] =
		{
			ir->mask.x,
			ir->mask.y,
			ir->mask.z,
			ir->mask.w,
		};

		for (unsigned i = 0; i < ir->mask.num_components; i++)
		{
			irdump_printf("%c", "xyzw"[swiz[i]]);
		}
	}
}

void DebugPrintVisitor::visit(ir_dereference_variable* ir)
{
	PrintID(ir);
	ir_variable *var = ir->variable_referenced();
	irdump_printf("%s", GetVarName(var).c_str());
}

void DebugPrintVisitor::visit(ir_dereference_array* ir)
{
	PrintID(ir);
	ir->array->accept(this);
	irdump_printf("[");
	ir->array_index->accept(this);
	irdump_printf("]");
}

void DebugPrintVisitor::visit(ir_dereference_image* ir)
{
	PrintID(ir);
	ir->image->accept(this);
	irdump_printf("[");
	ir->image_index->accept(this);
	irdump_printf("]");
}

void DebugPrintVisitor::visit(ir_dereference_record* ir)
{
	PrintID(ir);
	ir->record->accept(this);
	irdump_printf(".%s", ir->field);
}

void DebugPrintVisitor::visit(ir_assignment* ir)
{
	PrintID(ir);

	if (ir->condition)
	{
		irdump_printf("if (");
		ir->condition->accept(this);
		irdump_printf(") ");
	}

	if (ir->lhs->type && ir->lhs->type->is_matrix())
	{
		char Mask[256];
		char* MaskPtr = Mask;
		uint32 BitIndex = 0;
		uint32 Bits = ir->write_mask;
		while (Bits)
		{
			if (Bits & 1)
			{
				*MaskPtr++ = '_';
				*MaskPtr++ = 'm';
				*MaskPtr++ = '0' + BitIndex / ir->lhs->type->matrix_columns;
				*MaskPtr++ = '0' + BitIndex % ir->lhs->type->matrix_columns;
			}
			++BitIndex;
			Bits >>= 1;
		}
		*MaskPtr = 0;
		ir->lhs->accept(this);
		if (strlen(Mask))
		{
			irdump_printf(".%s", Mask);
		}
	}
	else
	{
		char mask[5];
		unsigned j = 0;
		for (unsigned i = 0; i < 4; i++)
		{
			if ((ir->write_mask & (1 << i)) != 0)
			{
				mask[j] = "xyzw"[i];
				j++;
			}
		}
		mask[j] = '\0';

		ir->lhs->accept(this);

		if (strcmp(mask, "xyzw") && mask[0])
		{
			irdump_printf(".%s", mask);
		}
	}

	irdump_printf(" = ");

	ir->rhs->accept(this);

	irdump_printf(";\n");
}

void DebugPrintVisitor::visit(ir_constant* ir)
{
	PrintID(ir);
	irdump_printf("(");
	irdump_printf("/*");
	PrintType(ir->type);
	irdump_printf("*/");

	if (ir->type->is_array())
	{
		irdump_printf("{");
		for (unsigned i = 0; i < ir->type->length; i++)
		{
			if (i)
			{
				irdump_printf(", ");
			}
			ir->get_array_element(i)->accept(this);
		}
		irdump_printf("}");
	}
	else if (ir->type->is_record())
	{
		check(0);
/*
		ir_constant *value = (ir_constant *) ir->components.get_head();
		for (unsigned i = 0; i < ir->type->length; i++)
		{
			irdump_printf("(%s ", ir->type->fields.structure[i].name);
			value->accept(this);
			irdump_printf(")");

			value = (ir_constant *) value->next;
		}
*/
	}
	else
	{
		if (ir->type->components() > 1)
		{
			irdump_printf("(");
		}
		for (unsigned i = 0; i < ir->type->components(); i++)
		{
			if (i != 0)
			{
				irdump_printf(",");
			}
			switch (ir->type->base_type)
			{
				case GLSL_TYPE_UINT:  irdump_printf("%u", ir->value.u[i]); break;
				case GLSL_TYPE_INT:   irdump_printf("%d", ir->value.i[i]); break;
				case GLSL_TYPE_HALF:	irdump_printf("%f", ir->value.f[i]); break;
				case GLSL_TYPE_FLOAT: irdump_printf("%f", ir->value.f[i]); break;
				case GLSL_TYPE_BOOL:  irdump_printf("%d", ir->value.b[i]); break;
				default: check(0);
			}
		}
		if (ir->type->components() > 1)
		{
			irdump_printf(")");
		}
	}
	irdump_printf(")");
}

void DebugPrintVisitor::visit(ir_call* ir)
{
	PrintID(ir);
	if (ir->return_deref)
	{
		ir->return_deref->accept(this);
		irdump_printf(" = ");
	}

	irdump_printf("%s(", ir->callee_name());
	bool bFirst = true;
	foreach_iter(exec_list_iterator, iter, *ir)
	{
		if (bFirst)
		{
			bFirst = false;
		}
		else
		{
			irdump_printf(", ");
		}

		ir_instruction *const inst = (ir_instruction *) iter.get();
		inst->accept(this);
	}
	irdump_printf(");\n");
}

void DebugPrintVisitor::visit(ir_return* ir)
{
	PrintID(ir);
	irdump_printf("return");
	ir_rvalue* const value = ir->get_value();
	if (value)
	{
		irdump_printf(" ");
		value->accept(this);
	}
	irdump_printf(";\n");
}

void DebugPrintVisitor::visit(ir_discard* ir)
{
	PrintID(ir);
	irdump_printf("clip(");
	if (ir->condition)
	{
		ir->condition->accept(this);
	}
	irdump_printf(");\n");
}

void DebugPrintVisitor::visit(ir_if* ir)
{
	PrintID(ir);
	irdump_printf("if (");
	ir->condition->accept(this);
	irdump_printf(")\n");
	PrintBlockWithScope(ir->then_instructions);

	if (!ir->else_instructions.is_empty())
	{
		Indent();
		irdump_printf("else\n");
		PrintBlockWithScope(ir->else_instructions);
	}
}

void DebugPrintVisitor::visit(ir_loop* ir)
{
	PrintID(ir);
	irdump_printf("for (");
	if (ir->counter != NULL)
	{
		bool bPrevEOL = bIRVarEOL;
		bIRVarEOL = false;
		ir->counter->accept(this);
		bIRVarEOL = bPrevEOL;
	}
	if (ir->from != NULL)
	{
		irdump_printf(" = ");
		ir->from->accept(this);
	}
	irdump_printf(" : ");
	if (ir->to != NULL)
	{
		ir->to->accept(this);
	}
	irdump_printf(";");
	if (ir->increment != NULL)
	{
		ir->increment->accept(this);
	}
	irdump_printf(")\n");

	PrintBlockWithScope(ir->body_instructions);
}

void DebugPrintVisitor::visit(ir_loop_jump* ir)
{
	PrintID(ir);
	irdump_printf(ir->is_break() ? "break" : "continue");
}

void DebugPrintVisitor::visit(ir_atomic* ir)
{
	if (ir->lhs)
	{
		ir->lhs->accept(this);
		irdump_printf(" = ");
	}
	irdump_printf("%s(&", ir->operator_string());
	ir->memory_ref->accept(this);
	if (ir->operands[0])
	{
		irdump_printf(", ");
		ir->operands[0]->accept(this);
		if (ir->operands[1])
		{
			irdump_printf(", ");
			ir->operands[1]->accept(this);
		}
	}
	irdump_printf(")\n;");
}

void DebugPrintVisitor::PrintID(ir_instruction * ir)
{
	irdump_printf("/*%d*/", ir ? ir->id : -1);
}

void DebugPrintVisitor::Indent()
{
	for (int i =0 ; i < Indentation; ++i)
	{
		irdump_printf("\t");
	}
}

void DebugPrintVisitor::PrintType(const glsl_type* Type)
{
	irdump_printf("%s", Type->name);
	if (Type->inner_type)
	{
		irdump_printf("<");
		PrintType(Type->inner_type);
		irdump_printf(">");
	}
}

std::string DebugPrintVisitor::GetVarName(ir_variable* var)
{
	std::string s("");
	if (var->name)
	{
		if (var->mode == ir_var_uniform || var->mode == ir_var_in || var->mode == ir_var_out || var->mode == ir_var_inout || var->mode == ir_var_shared)
		{
			return var->name;
		}
		else if (var->mode == ir_var_temporary || var->mode == ir_var_auto)
		{
			TNameMap::iterator Found = NameMap.find(var);
			if (Found != NameMap.end())
			{
				s = Found->second;
			}
			else
			{
				TNameSet::iterator FoundName = UniqueNames.find(var->name);
				if (FoundName != UniqueNames.end())
				{
					std::basic_stringstream<char, std::char_traits<char>, std::allocator<char>> ss("");
					ss << *FoundName;
					ss << ID++;
					s = ss.str();
					NameMap[var] = s;
				}
				else
				{
					NameMap[var] = var->name;
					UniqueNames.insert(var->name);
					s = var->name;
				}
			}
		}
		else
		{
			check(0);
/*
			TVarSet::iterator FoundVar = UniqueVars.find(var);
			if (FoundVar != UniqueVars.end())
			{
				UniqueVars.insert(var);
				s = var->name;
			}
			else
			{
				std::basic_stringstream<char, std::char_traits<char>, std::allocator<char>> ss("");
				ss << var->name;
				ss << ID++;
				s = ss.str();
			}
*/
		}
	}
	else
	{
		check(var->mode == ir_var_temporary);
		TNameMap::iterator Found = NameMap.find(var);
		if (Found != NameMap.end())
		{
			s = Found->second;
		}
		else
		{
			std::basic_stringstream<char, std::char_traits<char>, std::allocator<char>> ss("");
			ss << "Param";
			ss << ID++;
			s = ss.str();
			NameMap[var] = s;
		}
	}

	return s;
}

void DebugPrintVisitor::Dump(exec_list* List, _mesa_glsl_parse_state* State)
{
	DebugPrintVisitor Visitor;

	if (State)
	{
		for (unsigned i = 0; i < State->num_user_structures; i++)
		{
			const glsl_type *const s = State->user_structures[i];
			irdump_printf("struct %s\n{\n", s->name);
			for (unsigned j = 0; j < s->length; j++)
			{
				irdump_printf("\t");
				Visitor.PrintType(s->fields.structure[j].type);
				irdump_printf(" %s", s->fields.structure[j].name);
				if (s->fields.structure[j].semantic)
				{
					irdump_printf(" : %s", s->fields.structure[j].semantic);
				}
				irdump_printf(";\n");
			}
			irdump_printf("};\n");
		}
	}

	visit_exec_list(List, &Visitor);
}

void DebugPrintVisitor::PrintBlockWithScope(exec_list& ir)
{
	Indent();
	irdump_printf("{\n");
	Indentation++;

	foreach_iter(exec_list_iterator, iter, ir)
	{
		ir_instruction *const inst = (ir_instruction *) iter.get();
		Indent();
		inst->accept(this);
		irdump_printf(";\n");
	}
	Indentation--;
	Indent();
	irdump_printf("}\n");
}


void IRDump(exec_list* ir, _mesa_glsl_parse_state* State, const char* S) 
{
	irdump_printf("###########################################################################\n");
	irdump_printf("## Begin IR dump: %s\n", S);
	DebugPrintVisitor::Dump(ir, State);
	irdump_printf("###########################################################################\n");
	irdump_flush();
}

void IRDump(ir_instruction* ir)
{
	DebugPrintVisitor Visitor(true);
	ir->accept(&Visitor);
	irdump_flush();
}

void IRDump(ir_instruction* IRFirst, ir_instruction* IRLast)
{
	DebugPrintVisitor Visitor;
	VisitRange(&Visitor, IRFirst, IRLast);
	irdump_flush();
}
