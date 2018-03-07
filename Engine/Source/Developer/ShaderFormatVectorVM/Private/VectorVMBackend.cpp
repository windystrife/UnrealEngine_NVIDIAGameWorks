// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "VectorVMBackend.h"
#include "ShaderFormatVectorVM.h"
#include "hlslcc.h"
#include "hlslcc_private.h"
#include "compiler.h"

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

#if !PLATFORM_WINDOWS
#define _strdup strdup
#endif

DEFINE_LOG_CATEGORY(LogVVMBackend);

bool FVectorVMCodeBackend::GenerateMain(EHlslShaderFrequency Frequency, const char* EntryPoint, exec_list* Instructions, _mesa_glsl_parse_state* ParseState)
{
	//vm_debug_dump(Instructions, ParseState);

	ir_function_signature* MainSig = NULL;
	int NumFunctions = 0;

	foreach_iter(exec_list_iterator, iter, *Instructions)
	{
		ir_instruction *ir = (ir_instruction *)iter.get();
		ir_function* Function = ir->as_function();
		if (Function)
		{
			if (strcmp(Function->name, "SimulateMain") == 0)
			{
				foreach_iter(exec_list_iterator, sigiter, *Function)
				{
					ir_function_signature* Sig = (ir_function_signature *)sigiter.get();
					Sig->is_main = true;
				}
			}
		}
	}

	//vm_debug_dump(Instructions, ParseState);
	return true;
}

char* FVectorVMCodeBackend::GenerateCode(exec_list* ir, _mesa_glsl_parse_state* state, EHlslShaderFrequency Frequency)
{
	vm_debug_print("========VECTOR VM BACKEND: Generate Code==============\n");
	vm_debug_dump(ir, state);

	if (state->error) return nullptr;

	//Inline all functions.
	vm_debug_print("== Initial misc ==\n");
	bool progress = false;
	do
	{
		//progress = do_function_inlining(ir);//Full optimization pass earlier will have already done this.
		progress = do_mat_op_to_vec(ir);
		progress = do_vec_op_to_scalar(ir) || progress;
		progress = do_vec_index_to_swizzle(ir) || progress;
		progress = do_copy_propagation(ir) || progress;
		progress = do_copy_propagation_elements(ir) || progress;
		progress = do_swizzle_swizzle(ir) || progress;
	} while (progress);
	//validate_ir_tree(ir, state);
	vm_debug_dump(ir, state);
	if (state->error) return nullptr;
	
	vm_debug_print("== Branches to selects ==\n");
	vm_flatten_branches_to_selects(ir, state);
	//validate_ir_tree(ir, state);
	vm_debug_dump(ir, state);

	vm_debug_print("== To Single Op ==\n");
	vm_to_single_op(ir, state);
	//validate_ir_tree(ir, state);
	vm_debug_dump(ir, state);
	if (state->error) return nullptr;

	vm_debug_print("== Scalarize ==\n");
	vm_scalarize_ops(ir, state);
	//validate_ir_tree(ir, state);
	vm_debug_dump(ir, state);
	//validate_ir_tree(ir, state);
	if (state->error) return nullptr;

	//99% complete code to remove all matrices from the code and replace them with just swizzled vectors. 
	//For now visitors below here can handle matrices ok but we may hit some edge cases in future requiring their removal.
	//vm_debug_print("== matrices to vectors ==\n");
	//vm_matrices_to_vectors(ir, state);
	//vm_debug_dump(ir, state);
	//if (state->error) return nullptr;

	vm_debug_print("== Merge Ops ==\n");
	vm_merge_ops(ir, state);
//	validate_ir_tree(ir, state);
	vm_debug_dump(ir, state);
	//validate_ir_tree(ir, state);
	if (state->error) return nullptr;

	vm_debug_print("== Propagate non-expressions ==\n");
	vm_propagate_non_expressions_visitor(ir, state);
	//validate_ir_tree(ir, state);
	vm_debug_dump(ir, state);
	if (state->error) return nullptr;

	vm_debug_print("== Cleanup ==\n");
	// Final cleanup
	progress = false;
	do
	{
		progress = do_dead_code(ir, false);
		progress = do_dead_code_local(ir) || progress;
		progress = do_swizzle_swizzle(ir) || progress;
		progress = do_noop_swizzle(ir) || progress;
		progress = do_copy_propagation(ir) || progress;
		progress = do_copy_propagation_elements(ir) || progress;
		progress = do_constant_propagation(ir) || progress;
	} while (progress);
	vm_debug_dump(ir, state);

	//validate_ir_tree(ir, state);

	if (state->error) return nullptr;

	vm_gen_bytecode(ir, state, CompilationOutput);
	return  nullptr;// Cheat and emit the bytecode into he CompilationOutput. The return here is treat as a string so the 0's it contains prove problematic.
}

void FVectorVMLanguageSpec::SetupLanguageIntrinsics(_mesa_glsl_parse_state* State, exec_list* ir)
{
// 	//TODO: Need to add a way of stopping these being stripped if they're not used in the code.
// 	//We fine if the func is unused entirely but we need to keep the scalar signatures for when we scalarize the call.
// 	//Maybe we can just keep the wrong sig but still replace the ret val and params?
// 	make_intrinsic_genType(ir, State, "mad", ir_invalid_opcode, IR_INTRINSIC_FLOAT, 3, 1, 4);
	make_intrinsic_genType(ir, State, "rand", ir_invalid_opcode, IR_INTRINSIC_FLOAT, 1, 1, 4);
	make_intrinsic_genType(ir, State, "rand", ir_invalid_opcode, IR_INTRINSIC_INT, 1, 1, 4);
	make_intrinsic_genType(ir, State, "Modulo", ir_invalid_opcode, IR_INTRINSIC_FLOAT, 1, 1, 4);

// Dont need all these as we're only using the basic scalar function which we provide the signature for in the usf.
// 	make_intrinsic_genType(ir, State, "InputDataFloat", ir_invalid_opcode, IR_INTRINSIC_FLOAT, 2, 1, 1);
// 	make_intrinsic_genType(ir, State, "InputDataInt", ir_invalid_opcode, IR_INTRINSIC_INT, 2, 1, 1);
// 	make_intrinsic_genType(ir, State, "OutputDataFloat", ir_invalid_opcode, 0, 3, 1, 1);
// 	make_intrinsic_genType(ir, State, "OutputDataInt", ir_invalid_opcode, 0, 3, 1, 1);
//	make_intrinsic_genType(ir, State, "AcquireIndex", ir_invalid_opcode, IR_INTRINSIC_INT, 2, 1, 1);
}
