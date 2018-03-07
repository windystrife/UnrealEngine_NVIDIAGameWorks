// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ShaderCompilerCommon.h"
#include "glsl_parser_extras.h"
#include "ir.h"
#include "ir_visitor.h"
#include "ir_rvalue_visitor.h"
#include "PackUniformBuffers.h"
#include "IRDump.h"
#include "ast.h"
#include "LanguageSpec.h"
//@todo-rco: Remove STL!
#include <algorithm>
#include <sstream>
#include <stack>
#include <vector>
typedef TArray<ir_variable*> TIRVarVector;


template <typename T>
static inline T MIN2(T a, T b)
{
	return a < b ? a : b;
}

template <typename T>
static inline T MAX2(T a, T b)
{
	return a > b ? a : b;
}

static std::string GetUniformArrayName(_mesa_glsl_parser_targets Target, glsl_base_type Type, int CBIndex)
{
	std::basic_stringstream<char, std::char_traits<char>, std::allocator<char>> Name("");

	Name << glsl_variable_tag_from_parser_target(Target);

	if (CBIndex == -1)
	{
		Name << "u_";
	}
	else
	{
		Name << "c" << CBIndex << "_";
	}

	Name << (char)GetArrayCharFromPrecisionType(Type, false);
	Name.flush();
	return Name.str();
}

struct SFixSimpleArrayDereferencesVisitor : ir_rvalue_visitor
{
	_mesa_glsl_parse_state* ParseState;
	exec_list* FunctionBody;
	TVarVarMap& UniformMap;

	SFixSimpleArrayDereferencesVisitor(_mesa_glsl_parse_state* InParseState, exec_list* InFunctionBody, TVarVarMap& InUniformMap) :
		ParseState(InParseState),
		FunctionBody(InFunctionBody),
		UniformMap(InUniformMap)
	{
	}

	virtual void handle_rvalue(ir_rvalue** RValuePointer) override
	{
		static int TempID = 0;

		if (RValuePointer && *RValuePointer)
		{
			ir_rvalue* RValue = *RValuePointer;
			if (RValue && RValue->as_dereference_array())
			{
				ir_dereference_array* DerefArray = RValue->as_dereference_array();
				ir_variable* ArrayVar = RValue->variable_referenced();
				const glsl_type* ArrayElementType = ArrayVar->type->element_type();
				if (ArrayVar->read_only && ArrayElementType && !ArrayElementType->is_matrix())
				{
					if (ArrayVar->mode == ir_var_auto)
					{
						TVarVarMap::iterator itFound = UniformMap.find(ArrayVar);
						if (itFound != UniformMap.end())
						{
							ir_variable* NewLocal = new(ParseState) ir_variable(ArrayElementType, ralloc_asprintf(ParseState, "ar%d", TempID++), ir_var_auto);
							*RValuePointer = new(ParseState) ir_dereference_variable(NewLocal);

							SUniformVarEntry& Entry = itFound->second;

							ir_constant* ArrayBaseOffset = (DerefArray->array_index->type->base_type == GLSL_TYPE_UINT) ?
								new(ParseState) ir_constant((unsigned)Entry.Vec4Start) :
								new(ParseState) ir_constant(Entry.Vec4Start);
							ir_expression* NewArrayIndex = new(ParseState) ir_expression(ir_binop_add, ArrayBaseOffset, DerefArray->array_index);
							ir_dereference_array* NewDerefArray = new(ParseState) ir_dereference_array(new(ParseState) ir_dereference_variable(Entry.UniformArrayVar), NewArrayIndex);

							ir_swizzle* NewSwizzle = new(ParseState) ir_swizzle(
								NewDerefArray,
								MIN2(Entry.Components + 0, 3),
								MIN2(Entry.Components + 1, 3),
								MIN2(Entry.Components + 2, 3),
								MIN2(Entry.Components + 3, 3),
								ArrayElementType->vector_elements
								);

							ir_assignment* NewLocalInitializer = new(ParseState) ir_assignment(new(ParseState) ir_dereference_variable(NewLocal), NewSwizzle);
							base_ir->insert_before(NewLocalInitializer);
							NewLocalInitializer->insert_before(NewLocal);
						}
					}
				}
				else if (ArrayVar->read_only && ArrayElementType && ArrayElementType->is_matrix())
				{
					//matrix path
					if (ArrayVar->mode == ir_var_auto)
					{
						TVarVarMap::iterator itFound = UniformMap.find(ArrayVar);
						if (itFound != UniformMap.end())
						{
							ir_variable* NewLocal = new(ParseState) ir_variable(ArrayElementType, ralloc_asprintf(ParseState, "ar%d", TempID++), ir_var_auto);
							*RValuePointer = new(ParseState) ir_dereference_variable(NewLocal);

							SUniformVarEntry& Entry = itFound->second;

							exec_list instructions;
							instructions.push_tail(NewLocal);

							// matrix construction goes column by column performing an assignment
							for (int i = 0; i < ArrayElementType->matrix_columns; i++)
							{
								// Offset baking in matrix column
								ir_constant* ArrayBaseOffset = (DerefArray->array_index->type->base_type == GLSL_TYPE_UINT) ?
									new(ParseState) ir_constant((unsigned)(Entry.Vec4Start + i)) :
									new(ParseState) ir_constant((int)(Entry.Vec4Start + i));
								// Scale index by matrix columns
								ir_constant* ArrayScale = (DerefArray->array_index->type->base_type == GLSL_TYPE_UINT) ?
									new(ParseState) ir_constant((unsigned)(ArrayElementType->matrix_columns)) :
									new(ParseState) ir_constant((int)(ArrayElementType->matrix_columns));
								ir_rvalue* BaseIndex = DerefArray->array_index->clone(ParseState, NULL);
								ir_expression* NewArrayScale = new(ParseState) ir_expression(ir_binop_mul, BaseIndex, ArrayScale);
								// Compute final matrix address
								ir_expression* NewArrayIndex = new(ParseState) ir_expression(ir_binop_add, ArrayBaseOffset, NewArrayScale);
								ir_dereference_array* NewDerefArray = new(ParseState) ir_dereference_array(new(ParseState) ir_dereference_variable(Entry.UniformArrayVar), NewArrayIndex);

								ir_swizzle* NewSwizzle = new(ParseState) ir_swizzle(
									NewDerefArray,
									MIN2(Entry.Components + 0, 3),
									MIN2(Entry.Components + 1, 3),
									MIN2(Entry.Components + 2, 3),
									MIN2(Entry.Components + 3, 3),
									ArrayElementType->vector_elements
									);

								ir_assignment* NewLocalInitializer = new(ParseState) ir_assignment(new(ParseState) ir_dereference_array(NewLocal, new(ParseState) ir_constant(i)), NewSwizzle);
								instructions.push_tail(NewLocalInitializer);
							}
							base_ir->insert_before(&instructions);
						}
					}
				}
			}
		}
	}
};


struct SFindStructMembersVisitor : public ir_rvalue_visitor
{
	TIRVarSet& FoundRecordVars;

	SFindStructMembersVisitor(TIRVarSet& InFoundRecordVars) :
		FoundRecordVars(InFoundRecordVars)
	{
	}

	virtual void handle_rvalue(ir_rvalue** RValuePointer) override
	{
		if (RValuePointer && *RValuePointer)
		{
			ir_rvalue* RValue = *RValuePointer;
			if (RValue && RValue->as_dereference_record())
			{
				ir_variable* RecordVar = RValue->variable_referenced();
				if (RecordVar->mode == ir_var_uniform)
				{
					check(RecordVar->type->is_record());
					check(RecordVar->semantic && *RecordVar->semantic);
					FoundRecordVars.insert(RecordVar);
				}
			}
		}
	}
};



struct SConvertStructMemberToUniform : ir_rvalue_visitor
{
	_mesa_glsl_parse_state* ParseState;
	TStringStringIRVarMap& UniformMap;

	SConvertStructMemberToUniform(_mesa_glsl_parse_state* InParseState, TStringStringIRVarMap& InUniformMap) :
		ParseState(InParseState),
		UniformMap(InUniformMap)
	{
	}

	virtual void handle_rvalue(ir_rvalue** RValuePointer) override
	{
		if (RValuePointer && *RValuePointer)
		{
			ir_rvalue* RValue = *RValuePointer;
			if (RValue && RValue->as_dereference_record())
			{
				ir_dereference_record* DerefStruct = RValue->as_dereference_record();
				ir_variable* StructVar = RValue->variable_referenced();
				check(StructVar);
				if (StructVar->name)
				{
					// Name can be NULL when working on inputs to geometry shader structures
					TStringStringIRVarMap::iterator FoundStructIter = UniformMap.find(StructVar->name);
					if (FoundStructIter != UniformMap.end())
					{
						TStringIRVarMap::iterator FoundMember = FoundStructIter->second.find(DerefStruct->field);
						check(FoundMember != FoundStructIter->second.end());
						*RValuePointer = new(ParseState) ir_dereference_variable(FoundMember->second);
					}
				}
			}
		}
	}
};


// Flattens structures inside a uniform buffer into uniform variables; from:
//		cbuffer CB
//		{
//			float4 Value0;
//			struct
//			{
//				float4 Member0;
//				float3 Member1;
//			} S;
//			float4 Value1;
//		};
//	to:
//		cbuffer CB
//		{
//			float4 Value;
//			float4 S_Member0;
//			float3 S_Member1;
//			float4 Value1;
//		};
void FlattenUniformBufferStructures(exec_list* Instructions, _mesa_glsl_parse_state* ParseState)
{
	//IRDump(Instructions, ParseState, "Before FlattenUniformBufferStructures()");

	// Populate
	TIRVarSet StructVars;
	foreach_iter(exec_list_iterator, Iter, *Instructions)
	{
		ir_instruction* Instruction = (ir_instruction*)Iter.get();
		ir_function* Function = Instruction->as_function();
		if (Function)
		{
			foreach_iter(exec_list_iterator, SigIter, *Function)
			{
				ir_function_signature* Sig = (ir_function_signature *)SigIter.get();
				if (!Sig->is_builtin && Sig->is_defined)
				{
					SFindStructMembersVisitor FindMembersVisitor(StructVars);
					FindMembersVisitor.run(&Sig->body);
				}
			}
		}
		else if (Instruction->ir_type == ir_type_variable)
		{
			ir_variable* Var = (ir_variable*)Instruction;
			if (Var->mode == ir_var_uniform && Var->type->is_record())
			{
				check(Var->semantic && *Var->semantic);
				StructVars.insert(Var);
			}
		}
	}

	if (StructVars.empty())
	{
		// Nothing to do if no structs found; just copy the original state
		ParseState->CBuffersStructuresFlattened = ParseState->CBuffersOriginal;
		return;
	}

	// Find all CBs that need to be flattened
	unsigned UsedCBsMask = 0;
	for (TIRVarSet::iterator VarIter = StructVars.begin(); VarIter != StructVars.end(); ++VarIter)
	{
		ir_variable* var = *VarIter;
		for (unsigned i = 0; i < ParseState->num_uniform_blocks; ++i)
		{
			if (!strcmp(ParseState->uniform_blocks[i]->name, var->semantic))
			{
				UsedCBsMask |= 1 << i;
				break;
			}
		}
	}

	// Add the unchanged ones first
	for (unsigned i = 0; i < ParseState->num_uniform_blocks; ++i)
	{
		if ((UsedCBsMask & (1 << i)) == 0)
		{
			SCBuffer* CBuffer = ParseState->FindCBufferByName(false, ParseState->uniform_blocks[i]->name);
			check(CBuffer);
			ParseState->CBuffersStructuresFlattened.push_back(*CBuffer);
		}
	}

	// Now Flatten and store member info
	TStringStringIRVarMap StructMemberMap;
	for (TIRVarSet::iterator VarIter = StructVars.begin(); VarIter != StructVars.end(); ++VarIter)
	{
		ir_variable* var = *VarIter;

		// Find UB index
		int UniformBufferIndex = -1;
		for (unsigned i = 0; i < ParseState->num_uniform_blocks; ++i)
		{
			if (!strcmp(ParseState->uniform_blocks[i]->name, var->semantic))
			{
				UniformBufferIndex = (int)i;
				break;
			}
		}
		check(UniformBufferIndex != -1);

		bool bNeedToAddUB = (UsedCBsMask & (1 << UniformBufferIndex)) != 0;
		const glsl_uniform_block* OriginalUB = ParseState->uniform_blocks[UniformBufferIndex];

		// Copy the cbuffer list with room for the expanded values
		glsl_uniform_block* NewUniformBlock = NULL;

		if (bNeedToAddUB)
		{
			NewUniformBlock = glsl_uniform_block::alloc(ParseState, OriginalUB->num_vars - 1 + var->type->length);
			NewUniformBlock->name = OriginalUB->name;
		}
		else
		{
			UsedCBsMask |= 1 << UniformBufferIndex;
		}

		SCBuffer CBuffer;
		CBuffer.Name = OriginalUB->name;

		// Now find this struct member in the cbuffer and flatten it
		ir_variable* UniformBufferMemberVar = NULL;
		unsigned NewMemberIndex = 0;
		for (unsigned MemberIndex = 0; MemberIndex < OriginalUB->num_vars; ++MemberIndex)
		{
			if (!strcmp(OriginalUB->vars[MemberIndex]->name, var->name))
			{
				check(!UniformBufferMemberVar);
				UniformBufferMemberVar = OriginalUB->vars[MemberIndex];

				// Go through each member and add a new entry on the uniform buffer
				for (unsigned StructMemberIndex = 0; StructMemberIndex < var->type->length; ++StructMemberIndex)
				{
					ir_variable* NewLocal = new (ParseState) ir_variable(var->type->fields.structure[StructMemberIndex].type, ralloc_asprintf(ParseState, "%s_%s", var->name, var->type->fields.structure[StructMemberIndex].name), ir_var_uniform);
					NewLocal->semantic = var->semantic; // alias semantic to specify the uniform block.
					NewLocal->read_only = true;

					StructMemberMap[var->name][var->type->fields.structure[StructMemberIndex].name] = NewLocal;
					if (bNeedToAddUB)
					{
						NewUniformBlock->vars[NewMemberIndex++] = NewLocal;
						CBuffer.AddMember(NewLocal->type, NewLocal);
					}

					Instructions->push_head(NewLocal);
				}
			}
			else
			{
				if (bNeedToAddUB)
				{
					NewUniformBlock->vars[NewMemberIndex++] = OriginalUB->vars[MemberIndex];
					CBuffer.AddMember(OriginalUB->vars[MemberIndex]->type, OriginalUB->vars[MemberIndex]);
				}
			}
		}

		if (bNeedToAddUB)
		{
			check(NewMemberIndex == NewUniformBlock->num_vars);

			// Replace the original UB with this new one
			ParseState->uniform_blocks[UniformBufferIndex] = NewUniformBlock;
			ParseState->CBuffersStructuresFlattened.push_back(CBuffer);
		}

		// Downgrade the structure variable to a local
		var->mode = ir_var_temporary;
	}

	// Finally replace the struct member accesses into regular member access
	foreach_iter(exec_list_iterator, Iter, *Instructions)
	{
		ir_instruction* Instruction = (ir_instruction*)Iter.get();
		ir_function* Function = Instruction->as_function();
		if (Function)
		{
			foreach_iter(exec_list_iterator, SigIter, *Function)
			{
				ir_function_signature* Sig = (ir_function_signature *)SigIter.get();
				if (!Sig->is_builtin && Sig->is_defined)
				{
					SConvertStructMemberToUniform Visitor(ParseState, StructMemberMap);
					Visitor.run(&Sig->body);
				}
			}
		}
	}
	//	IRDump(Instructions, ParseState, "After FlattenUniformBufferStructures()");
}


void RemovePackedUniformBufferReferences(exec_list* Instructions, _mesa_glsl_parse_state* ParseState, TVarVarMap& UniformMap)
{
	foreach_iter(exec_list_iterator, Iter, *Instructions)
	{
		ir_instruction* Instruction = (ir_instruction*)Iter.get();
		ir_function* Function = Instruction->as_function();
		if (Function)
		{
			foreach_iter(exec_list_iterator, SigIter, *Function)
			{
				ir_function_signature* Sig = (ir_function_signature *)SigIter.get();
				if (!Sig->is_builtin && Sig->is_defined)
				{
					SFixSimpleArrayDereferencesVisitor Visitor(ParseState, &Sig->body, UniformMap);
					Visitor.run(&Sig->body);
				}
			}
		}
	}
}

/**
* Compare two uniform variables for the purpose of packing them into arrays.
*/
struct SSortUniformsPredicate
{
	bool operator()(ir_variable* v1, ir_variable* v2)
	{
		const glsl_type* Type1 = v1->type;
		const glsl_type* Type2 = v2->type;

		const bool bType1Array = Type1->is_array();
		const bool bType2Array = Type2->is_array();

		// Sort by base type.
		const glsl_base_type BaseType1 = bType1Array ? Type1->fields.array->base_type : Type1->base_type;
		const glsl_base_type BaseType2 = bType2Array ? Type2->fields.array->base_type : Type2->base_type;
		if (BaseType1 != BaseType2)
		{
			static const unsigned BaseTypeOrder[GLSL_TYPE_MAX] =
			{
				0, // GLSL_TYPE_UINT,
				2, // GLSL_TYPE_INT,
				3, // GLSL_TYPE_HALF,
				4, // GLSL_TYPE_FLOAT,
				1, // GLSL_TYPE_BOOL,
				5, // GLSL_TYPE_SAMPLER,
				6, // GLSL_TYPE_STRUCT,
				7, // GLSL_TYPE_ARRAY,
				8, // GLSL_TYPE_VOID,
				9, // GLSL_TYPE_ERROR,
				10, // GLSL_TYPE_SAMPLER_STATE,
				11, // GLSL_TYPE_OUTPUTSTREAM,
				12, // GLSL_TYPE_IMAGE,
				13, // GLSL_TYPE_INPUTPATCH,
				14, // GLSL_TYPE_OUTPUTPATCH,
			};

			return BaseTypeOrder[BaseType1] < BaseTypeOrder[BaseType2];
		}

		//sort by array first
		// arrays must be aligned on a vec4 boundary, placing them first ensures this
		if (bType1Array != bType2Array)
		{
			return int(bType1Array) > int(bType2Array);
		}

		// Then number of vector elements.
		if (Type1->vector_elements != Type2->vector_elements)
		{
			return Type1->vector_elements > Type2->vector_elements;
		}

		// Then matrix columns.
		if (Type1->matrix_columns != Type2->matrix_columns)
		{
			return Type1->matrix_columns > Type2->matrix_columns;
		}

		// If the types match, sort on the uniform name.
		return strcmp(v1->name, v2->name) < 0;
	}
};


struct SPackedUniformsInfo
{
	struct SInfoPerArray
	{
		int NumUniforms;
		int SizeInFloats;

		TIRVarList Variables;

		SInfoPerArray() : NumUniforms(0), SizeInFloats(0) {}
	};

	typedef std::map<char, SInfoPerArray> TInfoPerArray;
	TInfoPerArray UniformArrays;

	void AddVar(ir_variable* Var, _mesa_glsl_parse_state* ParseState)
	{
		const glsl_type* type = Var->type->is_array() ? Var->type->fields.array : Var->type;
		char ArrayType = GetArrayCharFromPrecisionType(type->base_type, false);
		SInfoPerArray& Info = UniformArrays[ArrayType];

		++Info.NumUniforms;

		int Stride = (type->vector_elements > 2 || Var->type->is_array()) ? 4 : MAX2(type->vector_elements, 1u);
		int NumRows = Var->type->is_array() ? Var->type->length : 1;
		NumRows = NumRows * MAX2(type->matrix_columns, 1u);
		Info.SizeInFloats += (Stride * NumRows);
		Info.Variables.push_back(Var);
	}
};

static void FindMainAndCalculateUniformArraySizes(exec_list* Instructions, _mesa_glsl_parse_state* ParseState, ir_function_signature*& OutMain, TIRVarVector& OutUniformVariables, SPackedUniformsInfo& OutInfo)
{
	foreach_iter(exec_list_iterator, iter, *Instructions)
	{
		ir_instruction* ir = (ir_instruction*)iter.get();
		if (ir->ir_type == ir_type_variable)
		{
			ir_variable* var = (ir_variable*)ir;
			if (var->mode == ir_var_uniform)
			{
				const glsl_type* type = var->type->is_array() ? var->type->fields.array : var->type;
				if (type->IsSamplerState())
				{
					// Ignore HLSL sampler states
					continue;
				}

				if (type->is_array())
				{
					_mesa_glsl_error(ParseState, "'%s' uniform variables "
						"cannot be multi-dimensional arrays", var->name);
					goto done;
				}

				OutUniformVariables.Add(var);
				OutInfo.AddVar(var, ParseState);
			}
		}
		else if (ir->ir_type == ir_type_function && OutMain == NULL)
		{
			ir_function* func = (ir_function*)ir;
			foreach_iter(exec_list_iterator, iter, func->signatures)
			{
				ir_function_signature* sig = (ir_function_signature*)iter.get();
				if (sig->is_main)
				{
					OutMain = sig;
					break;
				}
			}
		}
	}
done:
	return;
}

static int ProcessPackedUniformArrays(exec_list* Instructions, void* ctx, _mesa_glsl_parse_state* ParseState, const TIRVarVector& UniformVariables, SPackedUniformsInfo& PUInfo, bool bFlattenStructure, bool bGroupFlattenedUBs, bool bPackGlobalArraysIntoUniformBuffers, TVarVarMap& OutUniformMap)
{
	// First organize all uniforms by location (CB or Global) and Precision
	int UniformIndex = 0;
	std::map<std::string, std::map<char, TIRVarVector> > OrganizedVars;
	for (int NumUniforms = UniformVariables.Num(); UniformIndex < NumUniforms; ++UniformIndex)
	{
		ir_variable* var = UniformVariables[UniformIndex];
		const glsl_type* type = var->type->is_array() ? var->type->fields.array : var->type;
		const glsl_base_type array_base_type = (type->base_type == GLSL_TYPE_BOOL) ? GLSL_TYPE_UINT : type->base_type;
		if (type->is_sampler() || type->is_image())
		{
			break;
		}

		char ArrayType = GetArrayCharFromPrecisionType(array_base_type, true);
		if (!ArrayType)
		{
			_mesa_glsl_error(ParseState, "uniform '%s' has invalid type '%s'", var->name, var->type->name);
			return -1;
		}

		OrganizedVars[var->semantic ? var->semantic : ""][ArrayType].Add(var);
	}

	// Now create the list of used cb's to get their index
	std::map<std::string, int> CBIndices;
	int CBIndex = 0;
	CBIndices[""] = -1;
	for (auto& Current : ParseState->CBuffersOriginal)
	{
		auto IterFound = OrganizedVars.find(Current.Name);
		if (IterFound != OrganizedVars.end())
		{
			CBIndices[Current.Name] = CBIndex;
			++CBIndex;
		}
	}

	// Make sure any CB's with big matrices get at the end
	std::vector<std::string> CBOrder;
	{
		std::vector<std::string> EndOrganizedVars;
		for (auto& Pair : OrganizedVars)
		{
			bool bNonArrayFound = false;
			for (auto& PrecListPair : Pair.second)
			{
				for (auto* Var : PrecListPair.second)
				{
					if (!Var->type->is_array())
					{
						bNonArrayFound = true;
						break;
					}
				}

				if (bNonArrayFound)
				{
					break;
				}
			}

			if (bNonArrayFound)
			{
				CBOrder.push_back(Pair.first);
			}
			else
			{
				EndOrganizedVars.push_back(Pair.first);
			}
		}

		CBOrder.insert(CBOrder.end(), EndOrganizedVars.begin(), EndOrganizedVars.end());
	}

	// Now actually create the packed variables
	TStringIRVarMap UniformArrayVarMap;
	std::map<std::string, std::map<char, int> > NumElementsMap;
	for (auto& SourceCB : CBOrder)
	{
		std::string DestCB = bGroupFlattenedUBs ? SourceCB : "";
		check(OrganizedVars.find(SourceCB) != OrganizedVars.end());
		for (auto& VarSetPair : OrganizedVars[SourceCB])
		{
			ir_variable* UniformArrayVar = nullptr;
			char ArrayType = VarSetPair.first;
			auto& Vars = VarSetPair.second;
			for (auto* var : Vars)
			{
				const glsl_type* type = var->type->is_array() ? var->type->fields.array : var->type;
				const glsl_base_type array_base_type = (type->base_type == GLSL_TYPE_BOOL) ? GLSL_TYPE_UINT : type->base_type;
				if (!UniformArrayVar)
				{
					std::string UniformArrayName = GetUniformArrayName(ParseState->target, type->base_type, CBIndices[DestCB]);
					auto IterFound = UniformArrayVarMap.find(UniformArrayName);
					if (IterFound == UniformArrayVarMap.end())
					{
						const glsl_type* ArrayElementType = glsl_type::get_instance(array_base_type, 4, 1);
						int NumElementsAligned = (PUInfo.UniformArrays[ArrayType].SizeInFloats + 3) / 4;
						UniformArrayVar = new(ctx) ir_variable(
							glsl_type::get_array_instance(ArrayElementType, NumElementsAligned),
							ralloc_asprintf(ParseState, "%s", UniformArrayName.c_str()),
							ir_var_uniform
							);
						UniformArrayVar->semantic = ralloc_asprintf(ParseState, "%c", ArrayType);

						Instructions->push_head(UniformArrayVar);
						if (NumElementsMap.find(DestCB) == NumElementsMap.end() || NumElementsMap[DestCB].find(ArrayType) == NumElementsMap[DestCB].end())
						{
							NumElementsMap[DestCB][ArrayType] = 0;
						}

						UniformArrayVarMap[UniformArrayName] = UniformArrayVar;
					}
					else
					{
						UniformArrayVar = IterFound->second;
					}
				}

				int& NumElements = NumElementsMap[DestCB][ArrayType];
				int Stride = (type->vector_elements > 2 || var->type->is_array()) ? 4 : MAX2(type->vector_elements, 1u);
				int NumRows = var->type->is_array() ? var->type->length : 1;
				NumRows = NumRows * MAX2(type->matrix_columns, 1u);

				glsl_packed_uniform PackedUniform;
				check(var->name);
				PackedUniform.Name = var->name;
				PackedUniform.offset = NumElements;
				PackedUniform.num_components = Stride * NumRows;
				if (!SourceCB.empty())
				{
					PackedUniform.CB_PackedSampler = SourceCB;
					ParseState->FindOffsetIntoCBufferInFloats(bFlattenStructure, var->semantic, var->name, PackedUniform.OffsetIntoCBufferInFloats, PackedUniform.SizeInFloats);
					ParseState->CBPackedArraysMap[PackedUniform.CB_PackedSampler][ArrayType].push_back(PackedUniform);
				}
				else
				{
					ParseState->GlobalPackedArraysMap[ArrayType].push_back(PackedUniform);
				}

				SUniformVarEntry Entry = { UniformArrayVar, NumElements / 4, NumElements % 4, NumRows };
				OutUniformMap[var] = Entry;

				for (int RowIndex = 0; RowIndex < NumRows; ++RowIndex)
				{
					int SrcIndex = NumElements / 4;
					int SrcComponents = NumElements % 4;
					ir_rvalue* Src = new(ctx) ir_dereference_array(
						new(ctx) ir_dereference_variable(UniformArrayVar),
						new(ctx) ir_constant(SrcIndex)
						);
					if (type->is_numeric() || type->is_boolean())
					{
						Src = new(ctx) ir_swizzle(
							Src,
							MIN2(SrcComponents + 0, 3),
							MIN2(SrcComponents + 1, 3),
							MIN2(SrcComponents + 2, 3),
							MIN2(SrcComponents + 3, 3),
							type->vector_elements
							);
					}
					if (type->is_boolean())
					{
						Src = new(ctx) ir_expression(ir_unop_u2b, Src);
					}
					ir_dereference* Dest = new(ctx) ir_dereference_variable(var);
					if (NumRows > 1 || var->type->is_array())
					{
						if (var->type->is_array() && var->type->fields.array->matrix_columns > 1)
						{
							int MatrixNum = RowIndex / var->type->fields.array->matrix_columns;
							int MatrixRow = RowIndex - (var->type->fields.array->matrix_columns * MatrixNum);
							Dest = new(ctx) ir_dereference_array(Dest, new(ctx) ir_constant(MatrixNum));
							Dest = new(ctx) ir_dereference_array(Dest, new(ctx) ir_constant(MatrixRow));
						}
						else
						{
							Dest = new(ctx) ir_dereference_array(Dest, new(ctx) ir_constant(RowIndex));
						}
					}
					var->insert_after(new(ctx) ir_assignment(Dest, Src));
					NumElements += Stride;
				}
				var->mode = ir_var_auto;

				// Update Uniform Array size to match actual usage
				NumElements = (NumElements + 3) & ~3;
				UniformArrayVar->type = glsl_type::get_array_instance(UniformArrayVar->type->fields.array, NumElements / 4);
			}
		}
	}

	if (bPackGlobalArraysIntoUniformBuffers)
	{
		for (auto& Pair : UniformArrayVarMap)
		{
			auto* Var = Pair.second;

			glsl_uniform_block* block = glsl_uniform_block::alloc(ParseState, 1);
			block->name = ralloc_asprintf(ParseState, "HLSLCC_CB%c", Var->name[3]);
			block->vars[0] = Var;

			SCBuffer CBuffer;
			CBuffer.Name = block->name;
			CBuffer.AddMember(Var->type, Var);

			const glsl_uniform_block** blocks = reralloc(ParseState, ParseState->uniform_blocks,
				const glsl_uniform_block *,
				ParseState->num_uniform_blocks + 1);
			if (blocks != NULL)
			{
				blocks[ParseState->num_uniform_blocks] = block;
				ParseState->uniform_blocks = blocks;
				ParseState->num_uniform_blocks++;
			}
			Var->remove();
			Var->semantic = ralloc_strdup(ParseState, CBuffer.Name.c_str());
			ParseState->CBuffersOriginal.push_back(CBuffer);
		}
	}

	return UniformIndex;
}

static int ProcessPackedSamplers(int UniformIndex, _mesa_glsl_parse_state* ParseState, bool bKeepNames, const TIRVarVector& UniformVariables)
{
	int NumElements = 0;
	check(ParseState->GlobalPackedArraysMap[EArrayType_Sampler].empty());
	for (int NumUniforms = UniformVariables.Num(); UniformIndex < NumUniforms; ++UniformIndex)
	{
		ir_variable* var = UniformVariables[UniformIndex];
		const glsl_type* type = var->type->is_array() ? var->type->fields.array : var->type;

		if (!type->is_sampler() && !type->is_image())
		{
			_mesa_glsl_error(ParseState, "unexpected uniform '%s' "
				"of type '%s' when packing uniforms", var->name, var->type->name);
			return -1;
		}

		if (type->is_image())
		{
			break;
		}

		glsl_packed_uniform PackedSampler;
		check(var->name);
		PackedSampler.Name = var->name;
		PackedSampler.offset = NumElements;
		PackedSampler.num_components = var->type->is_array() ? var->type->length : 1;
		if (!bKeepNames)
		{
			var->name = ralloc_asprintf(var, "%ss%d",
				glsl_variable_tag_from_parser_target(ParseState->target),
				NumElements);
		}
		PackedSampler.CB_PackedSampler = var->name;
		ParseState->GlobalPackedArraysMap[EArrayType_Sampler].push_back(PackedSampler);

		NumElements += PackedSampler.num_components;
	}

	return UniformIndex;
}

static int ProcessPackedImages(int UniformIndex, _mesa_glsl_parse_state* ParseState, bool bKeepNames, const TIRVarVector& UniformVariables)
{
	int NumElements = 0;
	check(ParseState->GlobalPackedArraysMap[EArrayType_Image].empty());
	for (int NumUniforms = UniformVariables.Num(); UniformIndex < NumUniforms; ++UniformIndex)
	{
		ir_variable* var = UniformVariables[UniformIndex];
		const glsl_type* type = var->type->is_array() ? var->type->fields.array : var->type;

		if (!type->is_sampler() && !type->is_image())
		{
			_mesa_glsl_error(ParseState, "unexpected uniform '%s' "
				"of type '%s' when packing uniforms", var->name, var->type->name);
			return -1;
		}

		if (type->is_sampler())
		{
			break;
		}

		glsl_packed_uniform PackedImage;
		check(var->name);
		PackedImage.Name = var->name;
		PackedImage.offset = NumElements;
		PackedImage.num_components = var->type->is_array() ? var->type->length : 1;
		ParseState->GlobalPackedArraysMap[EArrayType_Image].push_back(PackedImage);
		if (!bKeepNames)
		{
			var->name = ralloc_asprintf(var, "%si%d",
				glsl_variable_tag_from_parser_target(ParseState->target),
				NumElements);
		}
		
		if (ParseState->bGenerateLayoutLocations)
		{
			if (ParseState->target != compute_shader)
			{
				// easy for compute shaders, since all the bindings start at 0, harder for a set of graphics shaders
				_mesa_glsl_warning(ParseState, "assigning explicit locations to UAVs/images is currently only fully tested for compute shaders");
			}
			var->explicit_location = true;
			var->location = NumElements;
		}

		NumElements += PackedImage.num_components;
	}

	return UniformIndex;
}

namespace DebugPackUniforms
{
	struct SDMARange
	{
		unsigned SourceCB;
		unsigned SourceOffset;
		unsigned Size;
		unsigned DestCBIndex;
		unsigned DestCBPrecision;
		unsigned DestOffset;

		bool operator <(SDMARange const & Other) const
		{
			if (SourceCB == Other.SourceCB)
			{
				return SourceOffset < Other.SourceOffset;
			}

			return SourceCB < Other.SourceCB;
		}
	};
	typedef std::list<SDMARange> TDMARangeList;
	typedef std::map<unsigned, TDMARangeList> TCBDMARangeMap;


	static void InsertRange(TCBDMARangeMap& CBAllRanges, unsigned SourceCB, unsigned SourceOffset, unsigned Size, unsigned DestCBIndex, unsigned DestCBPrecision, unsigned DestOffset)
	{
		check(SourceCB < (1 << 12));
		check(DestCBIndex < (1 << 12));
		check(DestCBPrecision < (1 << 8));
		unsigned SourceDestCBKey = (SourceCB << 20) | (DestCBIndex << 8) | DestCBPrecision;
		SDMARange Range ={SourceCB, SourceOffset, Size, DestCBIndex, DestCBPrecision, DestOffset};

		TDMARangeList& CBRanges = CBAllRanges[SourceDestCBKey];
		//printf("* InsertRange: %08x\t%d:%d - %d:%c:%d:%d\n", SourceDestCBKey, SourceCB, SourceOffset, DestCBIndex, DestCBPrecision, DestOffset, Size);
		if (CBRanges.empty())
		{
			CBRanges.push_back(Range);
		}
		else
		{
			TDMARangeList::iterator Prev = CBRanges.end();
			bool bAdded = false;
			for (auto Iter = CBRanges.begin(); Iter != CBRanges.end(); ++Iter)
			{
				if (SourceOffset + Size <= Iter->SourceOffset)
				{
					if (Prev == CBRanges.end())
					{
						CBRanges.push_front(Range);
					}
					else
					{
						CBRanges.insert(Iter, Range);
					}

					bAdded = true;
					break;
				}

				Prev = Iter;
			}

			if (!bAdded)
			{
				CBRanges.push_back(Range);
			}

			if (CBRanges.size() > 1)
			{
				// Try to merge ranges
				bool bDirty = false;
				do
				{
					bDirty = false;
					TDMARangeList NewCBRanges;
					for (auto Iter = CBRanges.begin(); Iter != CBRanges.end(); ++Iter)
					{
						if (Iter == CBRanges.begin())
						{
							Prev = CBRanges.begin();
						}
						else
						{
							if (Prev->SourceOffset + Prev->Size == Iter->SourceOffset && Prev->DestOffset + Prev->Size == Iter->DestOffset)
							{
								SDMARange Merged = *Prev;
								Merged.Size = Prev->Size + Iter->Size;
								NewCBRanges.pop_back();
								NewCBRanges.push_back(Merged);
								++Iter;
								NewCBRanges.insert(NewCBRanges.end(), Iter, CBRanges.end());
								bDirty = true;
								break;
							}
						}

						NewCBRanges.push_back(*Iter);
						Prev = Iter;
					}

					CBRanges.swap(NewCBRanges);
				}
				while (bDirty);
			}
		}
	}

	static TDMARangeList SortRanges(TCBDMARangeMap& CBRanges)
	{
		TDMARangeList Sorted;
		for (auto& Pair : CBRanges)
		{
			Sorted.insert(Sorted.end(), Pair.second.begin(), Pair.second.end());
		}

		Sorted.sort();

		return Sorted;
	}

	void DebugPrintPackedUniformBuffers(_mesa_glsl_parse_state* ParseState, bool bGroupFlattenedUBs)
	{
		// @PackedUB: UniformBuffer0(SourceIndex0): Member0(SourceOffset,SizeInFloats),Member1(SourceOffset,SizeInFloats), ...
		// @PackedUB: UniformBuffer1(SourceIndex1): Member0(SourceOffset,SizeInFloats),Member1(SourceOffset,SizeInFloats), ...
		// ...

		// First find all used CBs (since we lost that info during flattening)
		TStringSet UsedCBs;
		for (auto IterCB = ParseState->CBPackedArraysMap.begin(); IterCB != ParseState->CBPackedArraysMap.end(); ++IterCB)
		{
			for (auto Iter = IterCB->second.begin(); Iter != IterCB->second.end(); ++Iter)
			{
				_mesa_glsl_parse_state::TUniformList& Uniforms = Iter->second;
				for (auto IterU = Uniforms.begin(); IterU != Uniforms.end(); ++IterU)
				{
					if (!IterU->CB_PackedSampler.empty())
					{
						check(IterCB->first == IterU->CB_PackedSampler);
						UsedCBs.insert(IterU->CB_PackedSampler);
					}
				}
			}
		}

		check(UsedCBs.size() == ParseState->CBPackedArraysMap.size());

		// Now get the CB index based off source declaration order, and print an info line for each, while creating the mem copy list
		unsigned CBIndex = 0;
		TCBDMARangeMap CBRanges;
		for (unsigned i = 0; i < ParseState->num_uniform_blocks; i++)
		{
			const glsl_uniform_block* block = ParseState->uniform_blocks[i];
			if (UsedCBs.find(block->name) != UsedCBs.end())
			{
				bool bNeedsHeader = true;

				// Now the members for this CB
				bool bNeedsComma = false;
				auto IterPackedArrays = ParseState->CBPackedArraysMap.find(block->name);
				check(IterPackedArrays != ParseState->CBPackedArraysMap.end());
				for (auto Iter = IterPackedArrays->second.begin(); Iter != IterPackedArrays->second.end(); ++Iter)
				{
					char ArrayType = Iter->first;
					check(ArrayType != EArrayType_Image && ArrayType != EArrayType_Sampler);

					_mesa_glsl_parse_state::TUniformList& Uniforms = Iter->second;
					for (auto IterU = Uniforms.begin(); IterU != Uniforms.end(); ++IterU)
					{
						glsl_packed_uniform& Uniform = *IterU;
						if (Uniform.CB_PackedSampler == block->name)
						{
							if (bNeedsHeader)
							{
								printf("// @PackedUB: %s(%d): ",
									block->name,
									CBIndex);
								bNeedsHeader = false;
							}

							printf("%s%s(%u,%u)",
								bNeedsComma ? "," : "",
								Uniform.Name.c_str(),
								Uniform.OffsetIntoCBufferInFloats,
								Uniform.SizeInFloats);

							bNeedsComma = true;
							unsigned SourceOffset = Uniform.OffsetIntoCBufferInFloats;
							unsigned DestOffset = Uniform.offset;
							unsigned Size = Uniform.SizeInFloats;
							unsigned DestCBIndex = bGroupFlattenedUBs ? std::distance(UsedCBs.begin(), UsedCBs.find(block->name)) : 0;
							unsigned DestCBPrecision = ArrayType;
							InsertRange(CBRanges, CBIndex, SourceOffset, Size, DestCBIndex, DestCBPrecision, DestOffset);
						}
					}
				}

				if (!bNeedsHeader)
				{
					printf("\n");
				}

				CBIndex++;
			}
		}

		//DumpSortedRanges(SortRanges(CBRanges));

		// @PackedUBCopies: SourceArray:SourceOffset-DestArray:DestOffset,SizeInFloats;SourceArray:SourceOffset-DestArray:DestOffset,SizeInFloats,...
		bool bFirst = true;
		for (auto& Pair : CBRanges)
		{
			TDMARangeList& List = Pair.second;
			for (auto IterList = List.begin(); IterList != List.end(); ++IterList)
			{
				if (bFirst)
				{
					printf(bGroupFlattenedUBs ? "// @PackedUBCopies: " : "// @PackedUBGlobalCopies: ");
					bFirst = false;
				}
				else
				{
					printf(",");
				}

				if (bGroupFlattenedUBs)
				{
					printf("%d:%d-%d:%c:%d:%d", IterList->SourceCB, IterList->SourceOffset, IterList->DestCBIndex, IterList->DestCBPrecision, IterList->DestOffset, IterList->Size);
				}
				else
				{
					check(IterList->DestCBIndex == 0);
					printf("%d:%d-%c:%d:%d", IterList->SourceCB, IterList->SourceOffset, IterList->DestCBPrecision, IterList->DestOffset, IterList->Size);
				}
			}
		}

		if (!bFirst)
		{
			printf("\n");
		}
	}

	void DebugPrintPackedGlobals(_mesa_glsl_parse_state* State)
	{
		//	@PackedGlobals: Global0(DestArrayType, DestOffset, SizeInFloats), Global1(DestArrayType, DestOffset, SizeInFloats), ...
		bool bNeedsHeader = true;
		bool bNeedsComma = false;
		for (auto& Pair : State->GlobalPackedArraysMap)
		{
			char ArrayType = Pair.first;
			if (ArrayType != EArrayType_Image && ArrayType != EArrayType_Sampler)
			{
				_mesa_glsl_parse_state::TUniformList& Uniforms = Pair.second;
				check(!Uniforms.empty());

				for (auto Iter = Uniforms.begin(); Iter != Uniforms.end(); ++Iter)
				{
					glsl_packed_uniform& Uniform = *Iter;
					if (!State->bFlattenUniformBuffers || Uniform.CB_PackedSampler.empty())
					{
						if (bNeedsHeader)
						{
							printf("// @PackedGlobals: ");
							bNeedsHeader = false;
						}

						printf(
							"%s%s(%c:%u,%u)",
							bNeedsComma ? "," : "",
							Uniform.Name.c_str(),
							ArrayType,
							Uniform.offset,
							Uniform.num_components
							);
						bNeedsComma = true;
					}
				}
			}
		}

		if (!bNeedsHeader)
		{
			printf("\n");
		}
	}
	void DebugPrintPackedUniforms(_mesa_glsl_parse_state* ParseState, bool bGroupFlattenedUBs)
	{
		DebugPrintPackedGlobals(ParseState);

		if (ParseState->bFlattenUniformBuffers && !ParseState->CBuffersOriginal.empty())
		{
			DebugPrintPackedUniformBuffers(ParseState, bGroupFlattenedUBs);
		}
	}

}

/**
* Pack uniforms in to typed arrays.
* @param Instructions - The IR for which to pack uniforms.
* @param ParseState - Parse state.
*/
void PackUniforms(exec_list* Instructions, _mesa_glsl_parse_state* ParseState, bool bFlattenStructure, bool bGroupFlattenedUBs, bool bPackGlobalArraysIntoUniformBuffers, bool bKeepNames, TVarVarMap& OutUniformMap)
{
	//IRDump(Instructions);
	void* ctx = ParseState;
	void* tmp_ctx = ralloc_context(NULL);
	ir_function_signature* MainSig = NULL;
	TIRVarVector UniformVariables;

	SPackedUniformsInfo PUInfo;
	FindMainAndCalculateUniformArraySizes(Instructions, ParseState, MainSig, UniformVariables, PUInfo);

	if (MainSig && UniformVariables.Num())
	{
		std::sort(UniformVariables.begin(), UniformVariables.end(), SSortUniformsPredicate());
		int UniformIndex = ProcessPackedUniformArrays(Instructions, ctx, ParseState, UniformVariables, PUInfo, bFlattenStructure, bGroupFlattenedUBs, bPackGlobalArraysIntoUniformBuffers, OutUniformMap);
		if (UniformIndex == -1)
		{
			goto done;
		}
		UniformIndex = ProcessPackedSamplers(UniformIndex, ParseState, bKeepNames, UniformVariables);
		if (UniformIndex == -1)
		{
			goto done;
		}

		UniformIndex = ProcessPackedImages(UniformIndex, ParseState, bKeepNames, UniformVariables);
		if (UniformIndex == -1)
		{
			goto done;
		}
	}

	ParseState->has_packed_uniforms = true;

done:
	static bool Debug = false;
	if (Debug)
	{
		DebugPackUniforms::DebugPrintPackedUniforms(ParseState, true);
	}

	ralloc_free(tmp_ctx);
}

struct SExpandArrayAssignment : public ir_hierarchical_visitor
{
	bool bModified;
	_mesa_glsl_parse_state* ParseState;

	std::map<const glsl_type*, std::map<std::string, int>> MemberIsArrayMap;

	SExpandArrayAssignment(_mesa_glsl_parse_state* InState) :
		ParseState(InState),
		bModified(false)
	{
	}

	ir_visitor_status DoExpandAssignment(ir_assignment* ir)
	{
		if (ir->condition)
		{
			return visit_continue;
		}

		auto* DerefVar = ir->lhs->as_dereference_variable();
		auto* DerefStruct = ir->lhs->as_dereference_record();
		if (DerefVar)
		{
			ir_variable* Var = DerefVar->variable_referenced();
			if (!Var || Var->type->array_size() <= 0)
			{
				return visit_continue;
			}

			for (int i = 0; i < Var->type->array_size(); ++i)
			{
				ir_dereference_array* NewLHS = new(ParseState) ir_dereference_array(ir->lhs->clone(ParseState, NULL), new(ParseState) ir_constant(i));
				NewLHS->type = Var->type->element_type();
				ir_dereference_array* NewRHS = new(ParseState) ir_dereference_array(ir->rhs->clone(ParseState, NULL), new(ParseState) ir_constant(i));
				NewRHS->type = Var->type->element_type();
				ir_assignment* NewCopy = new(ParseState) ir_assignment(NewLHS, NewRHS);
				ir->insert_before(NewCopy);
			}

			ir->remove();
			delete ir;
			return visit_stop;
		}
		else if (DerefStruct)
		{
			auto FoundStruct = MemberIsArrayMap.find(DerefStruct->record->type);
			if (FoundStruct == MemberIsArrayMap.end())
			{
				//glsl_struct_field* Member = nullptr;
				for (int i = 0; i < DerefStruct->record->type->length; ++i)
				{
					if (DerefStruct->record->type->fields.structure[i].type->is_array())
					{
						MemberIsArrayMap[DerefStruct->record->type][DerefStruct->record->type->fields.structure[i].name] = i;
					}
				}

				if (MemberIsArrayMap[DerefStruct->record->type].empty())
				{
					// Avoid re-caching
					MemberIsArrayMap[DerefStruct->record->type][""] = -1;
				}
				return DoExpandAssignment(ir);
			}

			auto& Members = MemberIsArrayMap[DerefStruct->record->type];
			auto FoundMember = Members.find(DerefStruct->field);
			if (FoundMember != Members.end() && FoundMember->second >= 0)
			{
				auto& Member = DerefStruct->record->type->fields.structure[FoundMember->second];
				for (int i = 0; i < Member.type->length; ++i)
				{
					ir_dereference_array* NewLHS = new(ParseState) ir_dereference_array(DerefStruct->clone(ParseState, NULL), new(ParseState) ir_constant(i));
					NewLHS->type = DerefStruct->type->element_type();
					ir_dereference_array* NewRHS = new(ParseState) ir_dereference_array(ir->rhs->clone(ParseState, NULL), new(ParseState) ir_constant(i));
					NewRHS->type = ir->rhs->type->element_type();
					ir_assignment* NewCopy = new(ParseState) ir_assignment(NewLHS, NewRHS);
					ir->insert_before(NewCopy);
				}

				ir->remove();
				delete ir;
				return visit_stop;
			}
		}

		return visit_continue;
	}

	virtual ir_visitor_status visit_leave(ir_assignment* ir) override
	{
		auto Result = DoExpandAssignment(ir);
		if (Result != visit_continue)
		{
			bModified = true;
		}

		return Result;
	}
};

// Expand any full assignments (a = b) to per element (a[0] = b[0]; a[1] = b[1]; etc) so the array can be split
bool ExpandArrayAssignments(exec_list* ir, _mesa_glsl_parse_state* State)
{
	SExpandArrayAssignment Visitor(State);
	Visitor.run(ir);

	return Visitor.bModified;
}


struct FSamplerNameVisitor : public ir_rvalue_visitor
{
	_mesa_glsl_parse_state* ParseState;
	TStringToSetMap SamplerToTextureMap;
	TStringToSetMap& TextureToSamplerMap;

	FSamplerNameVisitor(_mesa_glsl_parse_state* InParseState, TStringToSetMap& InTextureToSamplerMap)
		: ParseState(InParseState)
		, TextureToSamplerMap(InTextureToSamplerMap)
	{
	}

	virtual void handle_rvalue(ir_rvalue **RValuePointer) override
	{
		ir_rvalue* RValue = *RValuePointer;
		ir_texture* TextureIR = RValue ? RValue->as_texture() : NULL;
		if (TextureIR)
		{
			if (TextureIR->SamplerState)
			{
				ir_variable* SamplerVar = TextureIR->sampler->variable_referenced();
				ir_variable* SamplerStateVar = TextureIR->SamplerState->variable_referenced();
				if (SamplerVar->mode == ir_var_uniform && SamplerStateVar->mode == ir_var_uniform)
				{
					SamplerToTextureMap[SamplerStateVar->name].insert(SamplerVar->name);
					TextureToSamplerMap[SamplerVar->name].insert(SamplerStateVar->name);

					check(SamplerStateVar->name);
					TextureIR->SamplerStateName = ralloc_strdup(ParseState, SamplerStateVar->name);

					// Remove the reference to the hlsl sampler
					ralloc_free(TextureIR->SamplerState);
					TextureIR->SamplerState = NULL;
				}
				else
				{
					int i = 0;
					++i;
				}
			}
		}
	}
};

bool ExtractSamplerStatesNameInformation(exec_list* Instructions, _mesa_glsl_parse_state* ParseState)
{
	//IRDump(Instructions);
	FSamplerNameVisitor SamplerNameVisitor(ParseState, ParseState->TextureToSamplerMap);
	SamplerNameVisitor.run(Instructions);

	bool bFail = false;
	if (!ParseState->LanguageSpec->AllowsSharingSamplers())
	{
		for (auto& Pair : SamplerNameVisitor.SamplerToTextureMap)
		{
			const std::string& SamplerName = Pair.first;
			const TStringSet& Textures = Pair.second;
			if (Textures.size() > 1)
			{
				_mesa_glsl_error(ParseState, "Sampler '%s' can't be used with more than one texture.\n", SamplerName.c_str());
				bFail = true;
			}
		}
	}

	if (bFail)
	{
		return false;
	}

	return true;
}

// Removes redundant casts (A->B->A), except for the case of a truncation (float->int->float)
struct FFixRedundantCastsVisitor : public ir_rvalue_visitor
{
	FFixRedundantCastsVisitor() {}

	virtual ir_visitor_status visit_enter(ir_expression* ir) override
	{
		return ir_rvalue_visitor::visit_enter(ir);
	}

	virtual ir_visitor_status visit_leave(ir_expression* ir) override
	{
		auto Result = ir_rvalue_visitor::visit_leave(ir);
		return Result;
	}

	virtual void handle_rvalue(ir_rvalue** RValuePtr) override
	{
		if (!RValuePtr || !*RValuePtr)
		{
			return;
		}
		auto* Expression = (*RValuePtr)->as_expression();
		if (Expression && Expression->operation >= ir_unop_first_conversion && Expression->operation <= ir_unop_last_conversion)
		{
			auto* OperandRValue = Expression->operands[0];
			auto* OperandExpr = OperandRValue->as_expression();
			if (OperandExpr && (OperandExpr->operation >= ir_unop_first_conversion && OperandExpr->operation <= ir_unop_last_conversion))
			{
				if (Expression->type == OperandExpr->operands[0]->type)
				{
					if (Expression->type->is_float() && OperandExpr->type->is_integer())
					{
						// Skip
					}
					else
					{
						// Remove the conversion
						*RValuePtr = OperandExpr->operands[0];
					}
				}
			}
		}
	}
};

void FixRedundantCasts(exec_list* ir)
{
	FFixRedundantCastsVisitor FixRedundantCastsVisitor;
	FixRedundantCastsVisitor.run(ir);
}

// Converts matrices to arrays in order to remove non-square matrices
namespace ArraysToMatrices
{
	typedef std::map<ir_variable*, int> TArrayReplacedMap;

	// Convert matrix types to array types
	struct SConvertTypes : public ir_hierarchical_visitor
	{
		TArrayReplacedMap& NeedToFixVars;

		SConvertTypes(TArrayReplacedMap& InNeedToFixVars) : NeedToFixVars(InNeedToFixVars) {}

		virtual ir_visitor_status visit(ir_variable* IR) override
		{
			IR->type = ConvertMatrix(IR->type, IR);
			return visit_continue;
		}

		const glsl_type* ConvertMatrix(const glsl_type* Type, ir_variable* Var)
		{
			if (Type->is_array())
			{
				const auto* OriginalElementType = Type->fields.array;
				if (OriginalElementType->is_matrix())
				{
					// Arrays of matrices have to be converted into a single array of vectors
					int OriginalRows = OriginalElementType->matrix_columns;

					Type = glsl_type::get_array_instance(OriginalElementType->column_type(), OriginalRows * Type->length);

					// Need to array dereferences later
					NeedToFixVars[Var] = OriginalRows;
				}
				else
				{
					const auto* NewElementType = ConvertMatrix(OriginalElementType, Var);
					Type = glsl_type::get_array_instance(NewElementType, Type->length);
				}
			}
			//else if (Type->is_record())
			//{
			//	check(0);
			//	/*
			//	for (int i = 0; i < Type->length; ++i)
			//	{
			//	const auto* OriginalRecordType = Type->fields.structure[i].type;
			//	Type->fields.structure[i].type = ConvertMatrix(OriginalRecordType, Var);
			//	}
			//	*/
			//}
			else if (Type->is_matrix())
			{
				const auto* ColumnType = Type->column_type();
				check(Type->matrix_columns > 0);
				Type = glsl_type::get_array_instance(ColumnType, Type->matrix_columns);
			}

			return Type;
		}
	};

	// Fixes the case where matNxM A[L] is accessed by row since that requires an extra offset/multiply: A[i][r] => A[i * N + r]
	struct SFixArrays : public ir_hierarchical_visitor
	{
		TArrayReplacedMap& Entries;

		_mesa_glsl_parse_state* ParseState;
		SFixArrays(_mesa_glsl_parse_state* InParseState, TArrayReplacedMap& InEntries) : ParseState(InParseState), Entries(InEntries) {}

		virtual ir_visitor_status visit_enter(ir_dereference_array* DerefArray)
		{
			auto FoundIter = Entries.find(DerefArray->variable_referenced());
			if (FoundIter == Entries.end())
			{
				return visit_continue;
			}

			auto* ArraySubIndex = DerefArray->array->as_dereference_array();
			if (ArraySubIndex)
			{
				auto* ArrayIndexMultiplier = new(ParseState) ir_constant(FoundIter->second);
				auto* ArrayIndexMulExpression = new(ParseState)ir_expression(ir_binop_mul,ArraySubIndex->array_index,convert_component(ArrayIndexMultiplier,ArraySubIndex->array_index->type));
				DerefArray->array_index = new(ParseState) ir_expression(ir_binop_add, convert_component(ArrayIndexMulExpression, DerefArray->array_index->type), DerefArray->array_index);
				DerefArray->array = ArraySubIndex->array;
			}

			return visit_continue;
		}
	};

	// Converts a complex matrix expression into simpler ones
	// matNxM A, B, C; C = A * B + C - D * E;
	//	to:
	// T0[0] = A[0] * B[0]; (0..N-1); T1[0] = T0[0] + C[0], etc
	struct SSimplifyMatrixExpressions : public ir_rvalue_visitor
	{
		_mesa_glsl_parse_state* ParseState;

		SSimplifyMatrixExpressions(_mesa_glsl_parse_state* InParseState) :
			ParseState(InParseState)
		{
		}

		virtual void handle_rvalue(ir_rvalue** RValue) override
		{
			if (!RValue || !*RValue)
			{
				return;
			}

			ir_expression* Expression = (*RValue)->as_expression();
			if (!Expression)
			{
				return;
			}

			if (!Expression->type || !Expression->type->is_matrix())
			{
				bool bExpand = false;
				for (int i = 0; i < Expression->get_num_operands(); ++i)
				{
					bExpand |= (Expression->operands[i]->type && Expression->operands[i]->type->is_matrix());
				}

				if (!bExpand)
				{
					return;
				}
			}

			auto* NewTemporary = new(ParseState) ir_variable(Expression->type, NULL, ir_var_temporary);
			base_ir->insert_before(NewTemporary);

			for (int i = 0; i < Expression->type->matrix_columns; ++i)
			{
				auto* NewLHS = new(ParseState) ir_dereference_array(NewTemporary, new(ParseState) ir_constant(i));
				auto* NewRHS = Expression->clone(ParseState, NULL);
				for (int j = 0; j < Expression->get_num_operands(); ++j)
				{
					NewRHS->operands[j] = new(ParseState) ir_dereference_array(NewRHS->operands[j], new(ParseState) ir_constant(i));
				}
				NewRHS->type = Expression->type->column_type();
				auto* NewAssign = new(ParseState) ir_assignment(NewLHS, NewRHS);
				base_ir->insert_before(NewAssign);
			}

			*RValue = new(ParseState) ir_dereference_variable(NewTemporary);
		}
	};
}

bool ExpandMatricesIntoArrays(exec_list* Instructions, _mesa_glsl_parse_state* ParseState)
{
	ArraysToMatrices::SSimplifyMatrixExpressions ExpressionToFuncVisitor(ParseState);
	ExpressionToFuncVisitor.run(Instructions);

	ArraysToMatrices::TArrayReplacedMap NeedToFixVars;
	ArraysToMatrices::SConvertTypes ConvertVisitor(NeedToFixVars);
	ConvertVisitor.run(Instructions);
	ExpandArrayAssignments(Instructions, ParseState);
	ArraysToMatrices::SFixArrays FixDereferencesVisitor(ParseState, NeedToFixVars);
	FixDereferencesVisitor.run(Instructions);

	return true;
}


struct FFindAtomicVariables : public ir_hierarchical_visitor
{
	TIRVarSet& AtomicVariables;
	FFindAtomicVariables(TIRVarSet& InAtomicVariables) :
		AtomicVariables(InAtomicVariables)
	{
	}

	virtual ir_visitor_status visit_enter(ir_atomic* ir) override
	{
		auto* Var = ir->memory_ref->variable_referenced();
		check(Var);
		AtomicVariables.insert(Var);
		return visit_continue_with_parent;
	}
};

void FindAtomicVariables(exec_list* ir, TIRVarSet& OutAtomicVariables)
{
	FFindAtomicVariables FindVisitor(OutAtomicVariables);
	FindVisitor.run(ir);
}

struct FFixAtomicVariables : public ir_rvalue_visitor
{
	_mesa_glsl_parse_state* State;
	TIRVarSet& AtomicVariables;
	FFixAtomicVariables(_mesa_glsl_parse_state* InState, TIRVarSet& InAtomicVariables) :
		State(InState),
		AtomicVariables(InAtomicVariables)
	{
	}

	virtual void handle_rvalue(ir_rvalue** RValuePtr) override
	{
		if (!RValuePtr || !*RValuePtr)
		{
			return;
		}

		if ((*RValuePtr)->as_atomic())
		{
			return;
		}

		auto* DeRefVar = (*RValuePtr)->as_dereference_variable();
		auto* DeRefArray = (*RValuePtr)->as_dereference_array();
		if (DeRefVar)
		{
			auto* Var = DeRefVar->var;
			if ((Var->mode == ir_var_shared || Var->mode == ir_var_uniform) && AtomicVariables.find(Var) != AtomicVariables.end())
			{
				check(!in_assignee);
				if (State->LanguageSpec->NeedsAtomicLoadStore())
				{
					auto* NewVar = new(State)ir_variable(Var->type, nullptr, ir_var_temporary);
					auto* NewAtomic = new(State)ir_atomic(ir_atomic_load, new(State) ir_dereference_variable(NewVar), DeRefVar, nullptr, nullptr);
					base_ir->insert_before(NewVar);
					base_ir->insert_before(NewAtomic);
					*RValuePtr = new(State)ir_dereference_variable(NewVar);
				}
				else
				{
					//#todo-rco: This code path is broken!
					auto* DummyVar = new(State)ir_variable(Var->type, nullptr, ir_var_temporary);
					auto* NewVar = new(State)ir_variable(Var->type, nullptr, ir_var_temporary);
					auto* NewAtomic = new(State)ir_atomic(ir_atomic_swap, new(State)ir_dereference_variable(DummyVar), DeRefVar, new(State)ir_dereference_variable(NewVar), nullptr);
					base_ir->insert_before(DummyVar);
					base_ir->insert_before(NewVar);
					base_ir->insert_before(NewAtomic);
					*RValuePtr = new(State)ir_dereference_variable(NewVar);
				}
			}
		}
		else if (DeRefArray)
		{
			auto* Var = DeRefArray->array->variable_referenced();
			if ((Var->mode == ir_var_shared || Var->mode == ir_var_uniform) && AtomicVariables.find(Var) != AtomicVariables.end())
			{
				check(!in_assignee);
				if (State->LanguageSpec->NeedsAtomicLoadStore())
				{
					auto* NewVar = new(State) ir_variable(DeRefArray->type, nullptr, ir_var_temporary);
					auto* NewAtomic = new(State)ir_atomic(ir_atomic_load, new(State)ir_dereference_variable(NewVar), DeRefArray, nullptr, nullptr);
					base_ir->insert_before(NewVar);
					base_ir->insert_before(NewAtomic);
					*RValuePtr = new(State)ir_dereference_variable(NewVar);
				}
				else
				{
					//#todo-rco: This code path is broken!
					auto* DummyVar = new(State)ir_variable(DeRefArray->type, nullptr, ir_var_temporary);
					auto* NewVar = new(State)ir_variable(DeRefArray->type, nullptr, ir_var_temporary);
					auto* NewAtomic = new(State)ir_atomic(ir_atomic_swap, new(State)ir_dereference_variable(DummyVar), DeRefArray, new(State)ir_dereference_variable(NewVar), nullptr);
					base_ir->insert_before(DummyVar);
					base_ir->insert_before(NewVar);
					base_ir->insert_before(NewAtomic);
					*RValuePtr = new(State)ir_dereference_variable(NewVar);
				}
			}
		}
	}

	ir_visitor_status visit_leave(ir_dereference_array* ir) override
	{
		/* The array index is not the target of the assignment, so clear the
		* 'in_assignee' flag.  Restore it after returning from the array index.
		*/
		const bool was_in_assignee = this->in_assignee;
		this->in_assignee = false;
		handle_rvalue(&ir->array_index);
		this->in_assignee = was_in_assignee;

		auto* Var = ir->array->variable_referenced();
		if ((Var->mode == ir_var_shared || Var->mode == ir_var_uniform) && AtomicVariables.find(Var) != AtomicVariables.end())
		{
			return visit_continue;
		}

		handle_rvalue(&ir->array);
		return visit_continue;
	}
/*
	virtual ir_visitor_status visit_enter(ir_atomic* ir) override
	{
		auto* Var = ir->lhs->variable_referenced();
		if (Var && Var->type && Var->type->sampler_buffer)
		{
			__debugbreak();
		}
		return visit_continue_with_parent;
	}
*/
	virtual ir_visitor_status visit_enter(ir_assignment* ir) override
	{
		//if (ir->id == 50456)
		//{
		//	__debugbreak();
		//}
		auto* LHSVar = ir->lhs->variable_referenced();
		if ((LHSVar->mode == ir_var_shared || LHSVar->mode == ir_var_uniform) && AtomicVariables.find(LHSVar) != AtomicVariables.end())
		{
			auto* DeRefVar = ir->lhs->as_dereference_variable();
			auto* DeRefArray = ir->lhs->as_dereference_array();
			auto* DeRefImage = ir->lhs->as_dereference_image();
			//#todo-rco: Atomic Store instead of swap
			if (DeRefImage)
			{
				check(ir == base_ir);
				auto* DummyVar = new(State) ir_variable(LHSVar->type->inner_type, nullptr, ir_var_temporary);
				auto* NewAtomic = new(State) ir_atomic(ir_atomic_swap, new(State) ir_dereference_variable(DummyVar), DeRefImage, ir->rhs, nullptr);
				base_ir->insert_before(DummyVar);
				base_ir->insert_before(NewAtomic);
				ir->remove();
			}
			else if (DeRefArray)
			{
				check(ir == base_ir);
				if (State->LanguageSpec->NeedsAtomicLoadStore())
				{
					auto* NewAtomic = new(State)ir_atomic(ir_atomic_store, nullptr, DeRefArray, ir->rhs, nullptr);
					base_ir->insert_before(NewAtomic);
				}
				else
				{
					auto* DummyVar = new(State) ir_variable(LHSVar->type->element_type(), nullptr, ir_var_temporary);
					auto* NewAtomic = new(State) ir_atomic(ir_atomic_swap, new(State) ir_dereference_variable(DummyVar), DeRefArray, ir->rhs, nullptr);
					base_ir->insert_before(DummyVar);
					base_ir->insert_before(NewAtomic);
				}
				ir->remove();
			}
			else if (DeRefVar)
			{
				check(ir == base_ir);
				if (State->LanguageSpec->NeedsAtomicLoadStore())
				{
					auto* NewAtomic = new(State) ir_atomic(ir_atomic_store, nullptr, DeRefVar, ir->rhs, nullptr);
					base_ir->insert_before(NewAtomic);
				}
				else
				{
					//#todo-rco: This code path is probably broken!
					auto* DummyVar = new(State)ir_variable(LHSVar->type, nullptr, ir_var_temporary);
					auto* NewAtomic = new(State)ir_atomic(ir_atomic_swap, new(State)ir_dereference_variable(DummyVar), DeRefVar, ir->rhs, nullptr);
					base_ir->insert_before(DummyVar);
					base_ir->insert_before(NewAtomic);
				}
				ir->remove();
			}
		}
		else
		{
			auto* RHSVar = ir->rhs->variable_referenced();
			if (RHSVar && (RHSVar->mode == ir_var_shared || RHSVar->mode == ir_var_uniform) && AtomicVariables.find(RHSVar) != AtomicVariables.end())
			{
				auto* DeRefVar = ir->rhs->as_dereference_variable();
				if (DeRefVar)
				{
					check(ir == base_ir);
					if (State->LanguageSpec->NeedsAtomicLoadStore())
					{
						auto* ResultVar = new(State)ir_variable(RHSVar->type, nullptr, ir_var_temporary);
						auto* NewAtomic = new(State) ir_atomic(ir_atomic_load, new(State) ir_dereference_variable(ResultVar), new(State) ir_dereference_variable(RHSVar), nullptr, nullptr);
						base_ir->insert_before(ResultVar);
						base_ir->insert_before(NewAtomic);
						ir->rhs = new(State) ir_dereference_variable(ResultVar);
					}
					else
					{
						//#todo-rco: This code path is probably broken!
						auto* DummyVar = new(State) ir_variable(RHSVar->type, nullptr, ir_var_temporary);
						auto* ResultVar = new(State)ir_variable(RHSVar->type, nullptr, ir_var_temporary);
						auto* NewAtomic = new(State) ir_atomic(ir_atomic_swap, new(State) ir_dereference_variable(DummyVar), DeRefVar, new(State) ir_dereference_variable(ResultVar), nullptr);
						base_ir->insert_before(ResultVar);
						base_ir->insert_before(DummyVar);
						base_ir->insert_before(NewAtomic);
						ir->rhs = new(State) ir_dereference_variable(ResultVar);
					}
					//#todo-rco: Won't handle the case of two atomic rvalues!
					return visit_continue_with_parent;
				}
			}
		}

		ir->rhs->accept(this);

		return visit_continue_with_parent;
	}

	virtual ir_visitor_status visit_enter(ir_atomic* ir) override
	{
		return visit_continue_with_parent;
	}
};

void FixAtomicReferences(exec_list* ir, _mesa_glsl_parse_state* State, TIRVarSet& AtomicVariables)
{
	if (AtomicVariables.empty())
	{
		return;
	}

	FFixAtomicVariables FixVisitor(State, AtomicVariables);
	FixVisitor.run(ir);
}
