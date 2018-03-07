// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MetalUtils.h"
#include "MetalShaderFormat.h"
#include "MetalBackend.h"

#include "hlslcc.h"
#include "hlslcc_private.h"
#include "compiler.h"
#include "MetalUtils.h"

PRAGMA_DISABLE_SHADOW_VARIABLE_WARNINGS
#include "glsl_parser_extras.h"
PRAGMA_ENABLE_SHADOW_VARIABLE_WARNINGS

#include "hash_table.h"
#include "ir_rvalue_visitor.h"
#include "ast.h"
#include "PackUniformBuffers.h"
#include "IRDump.h"
#include "OptValueNumbering.h"
#include "ir_optimization.h"

const bool bExpandVSInputsToFloat4 = false;
const bool bGenerateVSInputDummies = false;

static int GetIndexSuffix(const char* Prefix, int PrefixLength, const char* Semantic)
{
	check(Semantic);

	if (!strncmp(Semantic, Prefix, PrefixLength))
	{
		Semantic += PrefixLength;
		int Index = 0;
		if (isdigit((unsigned char)*Semantic))
		{
			Index = (*Semantic) - '0';

			++Semantic;
			if (*Semantic == 0)
			{
				return Index;
			}
			else if (isdigit((unsigned char)*Semantic))
			{
				Index = Index * 10 + (*Semantic) - '0';
				++Semantic;
				if (*Semantic == 0)
				{
					return Index;
				}
			}
		}
	}

	return -1;
}

static int GetAttributeIndex(const char* Semantic)
{
	return GetIndexSuffix("ATTRIBUTE", 9, Semantic);
}

static int GetInAttributeIndex(const char* Semantic)
{
	return GetIndexSuffix("IN_ATTRIBUTE", 12, Semantic);
}

const glsl_type* PromoteHalfToFloatType(_mesa_glsl_parse_state* state, const glsl_type* type)
{
	if (type->base_type == GLSL_TYPE_HALF)
	{
		return glsl_type::get_instance(GLSL_TYPE_FLOAT, type->vector_elements, type->matrix_columns);
	}
	else if (type->is_array())
	{
		auto* ElementType = type->element_type();
		auto* NewElementType = PromoteHalfToFloatType(state, ElementType);
		if (NewElementType != ElementType)
		{
			return glsl_type::get_array_instance(NewElementType, type->length);
		}
	}
	else if (type->is_record())
	{
		auto* Fields = ralloc_array(state, glsl_struct_field, type->length);
		bool bNeedNewType = false;
		for (unsigned i = 0; i < type->length; ++i)
		{
			auto* NewMemberType = PromoteHalfToFloatType(state, type->fields.structure[i].type);
			Fields[i] = type->fields.structure[i];
			if (NewMemberType != type->fields.structure[i].type)
			{
				bNeedNewType = true;
				Fields[i].type = NewMemberType;
			}
		}

		if (bNeedNewType)
		{
			auto* NewType = glsl_type::get_record_instance(Fields, type->length, ralloc_asprintf(state, "%s_F", type->name));
			// Hack: This way we tell this is a uniform buffer and we need to emit 'packed_'
			((glsl_type*)NewType)->HlslName = "__PACKED__";
			state->AddUserStruct(NewType);
			return NewType;
		}
	}

	return type;
}

void CreateNewAssignmentsFloat2Half(_mesa_glsl_parse_state* State, exec_list& NewAssignments, ir_variable* NewVar, ir_rvalue* RValue)
{
	if (NewVar->type->is_matrix())
	{
		for (uint32 i = 0; i < NewVar->type->matrix_columns; ++i)
		{
			auto* NewF2H = new(State)ir_expression(ir_unop_f2h, new(State)ir_dereference_array(RValue, new(State)ir_constant(i)));
			auto* NewAssignment = new(State)ir_assignment(new(State)ir_dereference_array(NewVar, new(State)ir_constant(i)), NewF2H);
			NewAssignments.push_tail(NewAssignment);
		}
	}
	else
	{
		auto* NewF2H = new(State)ir_expression(ir_unop_f2h, RValue);
		auto* NewAssignment = new(State)ir_assignment(new(State)ir_dereference_variable(NewVar), NewF2H);

		NewAssignments.push_tail(NewAssignment);
	}
}

static void CreateNewAssignmentsHalf2Float(_mesa_glsl_parse_state* State, exec_list& NewAssignments, ir_variable* NewVar, ir_rvalue* RValue)
{
	if (NewVar->type->is_matrix())
	{
		for (uint32 i = 0; i < NewVar->type->matrix_columns; ++i)
		{
			auto* NewF2H = new(State)ir_expression(ir_unop_h2f, new(State)ir_dereference_array(RValue, new(State)ir_constant(i)));
			auto* NewAssignment = new(State)ir_assignment(new(State)ir_dereference_array(NewVar, new(State)ir_constant(i)), NewF2H);
			NewAssignments.push_tail(NewAssignment);
		}
	}
	else
	{
		auto* NewF2H = new(State)ir_expression(ir_unop_h2f, RValue);
		auto* NewAssignment = new(State)ir_assignment(new(State)ir_dereference_variable(NewVar), NewF2H);

		NewAssignments.push_tail(NewAssignment);
	}
}

const glsl_type* GetFragColorTypeFromMetalOutputStruct(const glsl_type* OutputType)
{
	const glsl_type* FragColorType = glsl_type::error_type;
	if (OutputType && OutputType->base_type == GLSL_TYPE_STRUCT)
	{
		for (unsigned j = 0; j < OutputType->length; j++)
		{
			if (OutputType->fields.structure[j].semantic)
			{
				//@todo-rco: MRTs?
				if (!strncmp(OutputType->fields.structure[j].semantic, "[[ color(", 9))
				{
					FragColorType = OutputType->fields.structure[j].type;
					break;
				}
			}
		}
	}
	return FragColorType;
}

namespace MetalUtils
{
	/** Information on system values. */
	struct FSystemValue
	{
		const char* HlslSemantic;
		const glsl_type* Type;
		const char* MetalName;
		ir_variable_mode Mode;
		const char* MetalSemantic;
	};

	/** Vertex shader system values. */
	static FSystemValue VertexSystemValueTable[] =
	{
		{"SV_VertexID", glsl_type::uint_type, "IN_VertexID", ir_var_in, "[[ vertex_id ]]"},
		{"SV_InstanceID", glsl_type::uint_type, "IN_InstanceID", ir_var_in, "[[ instance_id ]]"},
		{"SV_Position", glsl_type::vec4_type, "Position", ir_var_out, "[[ position ]]"},
		{"SV_RenderTargetArrayIndex", glsl_type::uint_type, "OUT_Layer", ir_var_out, "[[ render_target_array_index ]]"},
		{"SV_ViewPortArrayIndex", glsl_type::uint_type, "OUT_Viewport", ir_var_out, "[[ viewport_array_index ]]"},
		{NULL, NULL, NULL, ir_var_auto, nullptr}
	};

	/** Pixel shader system values. */
	static FSystemValue MobilePixelSystemValueTable[] =
	{
		{"SV_Depth", glsl_type::float_type, "FragDepth", ir_var_out, "[[ depth(any) ]]"},
		{"SV_DepthLessEqual", glsl_type::float_type, "FragDepth", ir_var_out, "[[ depth(less) ]]"},
		{"SV_Position", glsl_type::vec4_type, "IN_FragCoord", ir_var_in, "[[ position ]]"},
		{"SV_IsFrontFace", glsl_type::bool_type, "IN_FrontFacing", ir_var_in, "[[ front_facing ]]"},
		//{"SV_PrimitiveID", glsl_type::int_type, "IN_PrimitiveID", ir_var_in, "[[  ]]"},
		//{"SV_RenderTargetArrayIndex", glsl_type::uint_type, "IN_Layer", ir_var_in, "[[ render_target_array_index ]]"},
		//{"SV_ViewPortArrayIndex", glsl_type::uint_type, "IN_Viewport", ir_var_in, "[[ viewport_array_index ]]"},
		{"SV_Target0", glsl_type::half4_type, "FragColor0", ir_var_out, "[[ color(0) ]]"},
		{"SV_Target1", glsl_type::half4_type, "FragColor1", ir_var_out, "[[ color(1) ]]"},
		{"SV_Target2", glsl_type::half4_type, "FragColor2", ir_var_out, "[[ color(2) ]]"},
		{"SV_Target3", glsl_type::half4_type, "FragColor3", ir_var_out, "[[ color(3) ]]"},
		{"SV_Target4", glsl_type::half4_type, "FragColor4", ir_var_out, "[[ color(4) ]]"},
		{"SV_Target5", glsl_type::half4_type, "FragColor5", ir_var_out, "[[ color(5) ]]"},
		{"SV_Target6", glsl_type::half4_type, "FragColor6", ir_var_out, "[[ color(6) ]]"},
		{"SV_Target7", glsl_type::half4_type, "FragColor7", ir_var_out, "[[ color(7) ]]"},
		{"SV_Target0", glsl_type::half3_type, "FragColor0", ir_var_out, "[[ color(0) ]]"},
		{"SV_Target1", glsl_type::half3_type, "FragColor1", ir_var_out, "[[ color(1) ]]"},
		{"SV_Target2", glsl_type::half3_type, "FragColor2", ir_var_out, "[[ color(2) ]]"},
		{"SV_Target3", glsl_type::half3_type, "FragColor3", ir_var_out, "[[ color(3) ]]"},
		{"SV_Target4", glsl_type::half3_type, "FragColor4", ir_var_out, "[[ color(4) ]]"},
		{"SV_Target5", glsl_type::half3_type, "FragColor5", ir_var_out, "[[ color(5) ]]"},
		{"SV_Target6", glsl_type::half3_type, "FragColor6", ir_var_out, "[[ color(6) ]]"},
		{"SV_Target7", glsl_type::half3_type, "FragColor7", ir_var_out, "[[ color(7) ]]"},
		{"SV_Target0", glsl_type::half2_type, "FragColor0", ir_var_out, "[[ color(0) ]]"},
		{"SV_Target1", glsl_type::half2_type, "FragColor1", ir_var_out, "[[ color(1) ]]"},
		{"SV_Target2", glsl_type::half2_type, "FragColor2", ir_var_out, "[[ color(2) ]]"},
		{"SV_Target3", glsl_type::half2_type, "FragColor3", ir_var_out, "[[ color(3) ]]"},
		{"SV_Target4", glsl_type::half2_type, "FragColor4", ir_var_out, "[[ color(4) ]]"},
		{"SV_Target5", glsl_type::half2_type, "FragColor5", ir_var_out, "[[ color(5) ]]"},
		{"SV_Target6", glsl_type::half2_type, "FragColor6", ir_var_out, "[[ color(6) ]]"},
		{"SV_Target7", glsl_type::half2_type, "FragColor7", ir_var_out, "[[ color(7) ]]"},
		{"SV_Target0", glsl_type::float_type, "FragColor0", ir_var_out, "[[ color(0) ]]"},
		{"SV_Target1", glsl_type::float_type, "FragColor1", ir_var_out, "[[ color(1) ]]"},
		{"SV_Target2", glsl_type::float_type, "FragColor2", ir_var_out, "[[ color(2) ]]"},
		{"SV_Target3", glsl_type::float_type, "FragColor3", ir_var_out, "[[ color(3) ]]"},
		{"SV_Target4", glsl_type::float_type, "FragColor4", ir_var_out, "[[ color(4) ]]"},
		{"SV_Target5", glsl_type::float_type, "FragColor5", ir_var_out, "[[ color(5) ]]"},
		{"SV_Target6", glsl_type::float_type, "FragColor6", ir_var_out, "[[ color(6) ]]"},
		{"SV_Target7", glsl_type::float_type, "FragColor7", ir_var_out, "[[ color(7) ]]"},
		{NULL, NULL, NULL, ir_var_auto, nullptr}
	};

	/** Pixel shader system values. */
	static FSystemValue DesktopPixelSystemValueTable[] =
	{
		{"SV_Depth", glsl_type::float_type, "FragDepth", ir_var_out, "[[ depth(any) ]]"},
		{"SV_DepthLessEqual", glsl_type::float_type, "FragDepth", ir_var_out, "[[ depth(less) ]]"},
		{"SV_Position", glsl_type::vec4_type, "IN_FragCoord", ir_var_in, "[[ position ]]"},
		{"SV_IsFrontFace", glsl_type::bool_type, "IN_FrontFacing", ir_var_in, "[[ front_facing ]]"},
		{"SV_Coverage", glsl_type::uint_type, "IN_Coverage", ir_var_in, "[[ sample_mask ]]"},
		{"SV_Coverage", glsl_type::uint_type, "OUT_Coverage", ir_var_out, "[[ sample_mask ]]"},
		//{"SV_PrimitiveID", glsl_type::int_type, "IN_PrimitiveID", ir_var_in, "[[  ]]"},
		//{"SV_RenderTargetArrayIndex", glsl_type::uint_type, "IN_Layer", ir_var_in, "[[ render_target_array_index ]]"},
		//{"SV_ViewPortArrayIndex", glsl_type::uint_type, "IN_Viewport", ir_var_in, "[[ viewport_array_index ]]"},
		{"SV_Target0", glsl_type::vec4_type, "FragColor0", ir_var_out, "[[ color(0) ]]"},
		{"SV_Target1", glsl_type::vec4_type, "FragColor1", ir_var_out, "[[ color(1) ]]"},
		{"SV_Target2", glsl_type::vec4_type, "FragColor2", ir_var_out, "[[ color(2) ]]"},
		{"SV_Target3", glsl_type::vec4_type, "FragColor3", ir_var_out, "[[ color(3) ]]"},
		{"SV_Target4", glsl_type::vec4_type, "FragColor4", ir_var_out, "[[ color(4) ]]"},
		{"SV_Target5", glsl_type::vec4_type, "FragColor5", ir_var_out, "[[ color(5) ]]"},
		{"SV_Target6", glsl_type::vec4_type, "FragColor6", ir_var_out, "[[ color(6) ]]"},
		{"SV_Target7", glsl_type::vec4_type, "FragColor7", ir_var_out, "[[ color(7) ]]"},
		{"SV_Target0", glsl_type::vec3_type, "FragColor0", ir_var_out, "[[ color(0) ]]"},
		{"SV_Target1", glsl_type::vec3_type, "FragColor1", ir_var_out, "[[ color(1) ]]"},
		{"SV_Target2", glsl_type::vec3_type, "FragColor2", ir_var_out, "[[ color(2) ]]"},
		{"SV_Target3", glsl_type::vec3_type, "FragColor3", ir_var_out, "[[ color(3) ]]"},
		{"SV_Target4", glsl_type::vec3_type, "FragColor4", ir_var_out, "[[ color(4) ]]"},
		{"SV_Target5", glsl_type::vec3_type, "FragColor5", ir_var_out, "[[ color(5) ]]"},
		{"SV_Target6", glsl_type::vec3_type, "FragColor6", ir_var_out, "[[ color(6) ]]"},
		{"SV_Target7", glsl_type::vec3_type, "FragColor7", ir_var_out, "[[ color(7) ]]"},
		{"SV_Target0", glsl_type::vec2_type, "FragColor0", ir_var_out, "[[ color(0) ]]"},
		{"SV_Target1", glsl_type::vec2_type, "FragColor1", ir_var_out, "[[ color(1) ]]"},
		{"SV_Target2", glsl_type::vec2_type, "FragColor2", ir_var_out, "[[ color(2) ]]"},
		{"SV_Target3", glsl_type::vec2_type, "FragColor3", ir_var_out, "[[ color(3) ]]"},
		{"SV_Target4", glsl_type::vec2_type, "FragColor4", ir_var_out, "[[ color(4) ]]"},
		{"SV_Target5", glsl_type::vec2_type, "FragColor5", ir_var_out, "[[ color(5) ]]"},
		{"SV_Target6", glsl_type::vec2_type, "FragColor6", ir_var_out, "[[ color(6) ]]"},
		{"SV_Target7", glsl_type::vec2_type, "FragColor7", ir_var_out, "[[ color(7) ]]"},
		{"SV_Target0", glsl_type::float_type, "FragColor0", ir_var_out, "[[ color(0) ]]"},
		{"SV_Target1", glsl_type::float_type, "FragColor1", ir_var_out, "[[ color(1) ]]"},
		{"SV_Target2", glsl_type::float_type, "FragColor2", ir_var_out, "[[ color(2) ]]"},
		{"SV_Target3", glsl_type::float_type, "FragColor3", ir_var_out, "[[ color(3) ]]"},
		{"SV_Target4", glsl_type::float_type, "FragColor4", ir_var_out, "[[ color(4) ]]"},
		{"SV_Target5", glsl_type::float_type, "FragColor5", ir_var_out, "[[ color(5) ]]"},
		{"SV_Target6", glsl_type::float_type, "FragColor6", ir_var_out, "[[ color(6) ]]"},
		{"SV_Target7", glsl_type::float_type, "FragColor7", ir_var_out, "[[ color(7) ]]"},
		{"SV_Target0", glsl_type::uvec4_type, "FragColor0", ir_var_out, "[[ color(0) ]]"},
		{"SV_Target1", glsl_type::uvec4_type, "FragColor1", ir_var_out, "[[ color(1) ]]"},
		{"SV_Target2", glsl_type::uvec4_type, "FragColor2", ir_var_out, "[[ color(2) ]]"},
		{"SV_Target3", glsl_type::uvec4_type, "FragColor3", ir_var_out, "[[ color(3) ]]"},
		{"SV_Target4", glsl_type::uvec4_type, "FragColor4", ir_var_out, "[[ color(4) ]]"},
		{"SV_Target5", glsl_type::uvec4_type, "FragColor5", ir_var_out, "[[ color(5) ]]"},
		{"SV_Target6", glsl_type::uvec4_type, "FragColor6", ir_var_out, "[[ color(6) ]]"},
		{"SV_Target7", glsl_type::uvec4_type, "FragColor7", ir_var_out, "[[ color(7) ]]"},
		{"SV_Target0", glsl_type::uvec3_type, "FragColor0", ir_var_out, "[[ color(0) ]]"},
		{"SV_Target1", glsl_type::uvec3_type, "FragColor1", ir_var_out, "[[ color(1) ]]"},
		{"SV_Target2", glsl_type::uvec3_type, "FragColor2", ir_var_out, "[[ color(2) ]]"},
		{"SV_Target3", glsl_type::uvec3_type, "FragColor3", ir_var_out, "[[ color(3) ]]"},
		{"SV_Target4", glsl_type::uvec3_type, "FragColor4", ir_var_out, "[[ color(4) ]]"},
		{"SV_Target5", glsl_type::uvec3_type, "FragColor5", ir_var_out, "[[ color(5) ]]"},
		{"SV_Target6", glsl_type::uvec3_type, "FragColor6", ir_var_out, "[[ color(6) ]]"},
		{"SV_Target7", glsl_type::uvec3_type, "FragColor7", ir_var_out, "[[ color(7) ]]"},
		{"SV_Target0", glsl_type::uvec2_type, "FragColor0", ir_var_out, "[[ color(0) ]]"},
		{"SV_Target1", glsl_type::uvec2_type, "FragColor1", ir_var_out, "[[ color(1) ]]"},
		{"SV_Target2", glsl_type::uvec2_type, "FragColor2", ir_var_out, "[[ color(2) ]]"},
		{"SV_Target3", glsl_type::uvec2_type, "FragColor3", ir_var_out, "[[ color(3) ]]"},
		{"SV_Target4", glsl_type::uvec2_type, "FragColor4", ir_var_out, "[[ color(4) ]]"},
		{"SV_Target5", glsl_type::uvec2_type, "FragColor5", ir_var_out, "[[ color(5) ]]"},
		{"SV_Target6", glsl_type::uvec2_type, "FragColor6", ir_var_out, "[[ color(6) ]]"},
		{"SV_Target7", glsl_type::uvec2_type, "FragColor7", ir_var_out, "[[ color(7) ]]"},
		{"SV_Target0", glsl_type::uint_type, "FragColor0", ir_var_out, "[[ color(0) ]]"},
		{"SV_Target1", glsl_type::uint_type, "FragColor1", ir_var_out, "[[ color(1) ]]"},
		{"SV_Target2", glsl_type::uint_type, "FragColor2", ir_var_out, "[[ color(2) ]]"},
		{"SV_Target3", glsl_type::uint_type, "FragColor3", ir_var_out, "[[ color(3) ]]"},
		{"SV_Target4", glsl_type::uint_type, "FragColor4", ir_var_out, "[[ color(4) ]]"},
		{"SV_Target5", glsl_type::uint_type, "FragColor5", ir_var_out, "[[ color(5) ]]"},
		{"SV_Target6", glsl_type::uint_type, "FragColor6", ir_var_out, "[[ color(6) ]]"},
		{"SV_Target7", glsl_type::uint_type, "FragColor7", ir_var_out, "[[ color(7) ]]"},
		{NULL, NULL, NULL, ir_var_auto, nullptr}
	};

	/** Geometry shader system values. */
	static FSystemValue GeometrySystemValueTable[] =
	{
/*
		{"SV_VertexID", glsl_type::int_type, "IN_VertexID", ir_var_in, false, false, false, false},
		{"SV_InstanceID", glsl_type::int_type, "IN_InstanceID", ir_var_in, false, false, false, false},
		{"SV_Position", glsl_type::vec4_type, "IN_Position", ir_var_in, false, true, true, false},
		{"SV_Position", glsl_type::vec4_type, "OUT_Position", ir_var_out, false, false, true, false},
		{"SV_RenderTargetArrayIndex", glsl_type::uint_type, "OUT_Layer", ir_var_out, false, false, false, false},
		{"SV_PrimitiveID", glsl_type::int_type, "OUT_PrimitiveID", ir_var_out, false, false, false, false},
		{"SV_PrimitiveID", glsl_type::int_type, "IN_PrimitiveIDIn", ir_var_in, false, false, false, false},
*/
		{NULL, NULL, NULL, ir_var_auto, nullptr}
	};


	/** Hull shader system values. */
	static FSystemValue HullSystemValueTable[] =
	{
		{"SV_VertexID", glsl_type::uint_type, "IN_VertexID", ir_var_in, "[[ vertex_id ]]"},
		{"SV_InstanceID", glsl_type::uint_type, "IN_InstanceID", ir_var_in, "[[ instance_id ]]"},
		/*
		{"SV_OutputControlPointID", glsl_type::int_type, "gl_InvocationID", ir_var_in, false, false, false, false},
*/
//		{"SV_OutputControlPointID", glsl_type::uint_type, "LocalInvocationID", ir_var_in, "[[ thread_position_in_threadgroup ]]"},
		{NULL, NULL, NULL, ir_var_auto, nullptr}
	};

	/** Domain shader system values. */
	static FSystemValue DomainSystemValueTable[] =
	{
		{"SV_Position", glsl_type::vec4_type, "IN_Position", ir_var_in, "[[ TODO ]]"},
		{"SV_Position", glsl_type::vec4_type, "Position", ir_var_out, "[[ position ]]"},
		{"SV_DomainLocation", glsl_type::vec2_type, "PositionInPatch", ir_var_in, "[[ position_in_patch ]]"}, // @todo maybe add a NULL/void_type/error_type and set it using the passed in type for GenerateInputFromSemantic -- use it to simplify SV_Target as well
		{"SV_DomainLocation", glsl_type::vec3_type, "PositionInPatch", ir_var_in, "[[ position_in_patch ]]"},
		{"SV_RenderTargetArrayIndex", glsl_type::uint_type, "OUT_Layer", ir_var_out, "[[ render_target_array_index ]]"},
		{"SV_ViewPortArrayIndex", glsl_type::uint_type, "OUT_Viewport", ir_var_out, "[[ viewport_array_index ]]"},
		{NULL, NULL, NULL, ir_var_auto, nullptr}
	};

	/** Compute shader system values. */
	static FSystemValue ComputeSystemValueTable[] =
	{
		// D3D,					type,					GL,									param,		Metal
		{"SV_DispatchThreadID",	glsl_type::uvec3_type,	"thread_position_in_grid",			ir_var_in, "[[ thread_position_in_grid ]]"},
		{"SV_GroupID",			glsl_type::uvec3_type,	"threadgroup_position_in_grid",		ir_var_in, "[[ threadgroup_position_in_grid ]]"},
		{"SV_GroupIndex",		glsl_type::uint_type,	"thread_index_in_threadgroup",		ir_var_in, "[[ thread_index_in_threadgroup ]]"},
		{"SV_GroupThreadID",	glsl_type::uvec3_type,	"thread_position_in_threadgroup",	ir_var_in, "[[ thread_position_in_threadgroup ]]"},
		{NULL, NULL, NULL, ir_var_auto, nullptr}
	};

	FSystemValue* MobileSystemValueTable[HSF_FrequencyCount] =
	{
		VertexSystemValueTable,
		MobilePixelSystemValueTable,
		GeometrySystemValueTable,
		HullSystemValueTable,
		DomainSystemValueTable,
		ComputeSystemValueTable
	};

	FSystemValue* DesktopSystemValueTable[HSF_FrequencyCount] =
	{
		VertexSystemValueTable,
		DesktopPixelSystemValueTable,
		GeometrySystemValueTable,
		HullSystemValueTable,
		DomainSystemValueTable,
		ComputeSystemValueTable
	};

	static ir_rvalue* GenerateInputFromSemantic(EHlslShaderFrequency Frequency, EMetalGPUSemantics bIsDesktop, _mesa_glsl_parse_state* ParseState,
		const char* Semantic, const glsl_type* Type, exec_list* DeclInstructions, exec_list* PreCallInstructions)
	{
		if (!Semantic)
		{
			//todo-rco: More info!
			_mesa_glsl_error(ParseState, "Missing input semantic!", Semantic);
			return nullptr;
		}

		if (!FCStringAnsi::Stricmp(Semantic, "SV_OutputControlPointID"))
		{
			//check(bIsTessellationVSHS);
			auto Variable = ParseState->symbols->get_variable("SV_OutputControlPointID");
			check(Variable);
			return new (ParseState)ir_dereference_variable(Variable);
		}
		else
		if (FCStringAnsi::Strnicmp(Semantic, "SV_", 3) == 0)
		{
			FSystemValue* SystemValues = (bIsDesktop == EMetalGPUSemanticsMobile) ? MobileSystemValueTable[Frequency] : DesktopSystemValueTable[Frequency];
			for (int i = 0; SystemValues[i].HlslSemantic != NULL; ++i)
			{
				if (SystemValues[i].Mode == ir_var_in && FCStringAnsi::Stricmp(SystemValues[i].HlslSemantic, Semantic) == 0)
				{
					if (!FCStringAnsi::Stricmp(Semantic, "SV_DomainLocation") && Frequency == HSF_DomainShader)
					{
						// SV_DomainLocation is either float2 or float3 -- find the proper type
						if (SystemValues[i].Type != Type)
						{
							continue;
						}
					}
					
					ir_variable* Variable = ParseState->symbols->get_variable(SystemValues[i].MetalName);
					if (!Variable)
					{
						Variable = new(ParseState) ir_variable(SystemValues[i].Type, SystemValues[i].MetalName, ir_var_in);
						Variable->semantic = SystemValues[i].MetalSemantic;
						Variable->read_only = true;
						Variable->origin_upper_left = false;
						DeclInstructions->push_tail(Variable);
						ParseState->symbols->add_variable(Variable);
					}
					ir_dereference_variable* VariableDeref = new(ParseState) ir_dereference_variable(Variable);
					if (!FCStringAnsi::Stricmp(Semantic, "SV_Position") && Frequency == HSF_PixelShader)
					{
						// UE4 requires w instead of 1/w in SVPosition
						auto* TempVariable = new(ParseState) ir_variable(Variable->type, nullptr, ir_var_temporary);
						DeclInstructions->push_tail(TempVariable);

						// Assign input to this variable
						auto* TempVariableDeref = new(ParseState) ir_dereference_variable(TempVariable);
						DeclInstructions->push_tail(
							new(ParseState) ir_assignment(TempVariableDeref, VariableDeref)
							);

						// TempVariable.w = ( 1.0f / TempVariable.w );
						DeclInstructions->push_tail(
							new(ParseState) ir_assignment(
								new(ParseState) ir_swizzle(TempVariableDeref->clone(ParseState, nullptr), 3, 0, 0, 0, 1),
								new(ParseState) ir_expression(
									ir_binop_div,
									new(ParseState) ir_constant(1.0f),
									new(ParseState) ir_swizzle(TempVariableDeref->clone(ParseState, nullptr), 3, 0, 0, 0, 1)
									)
								)
							);

						VariableDeref = TempVariableDeref->clone(ParseState, nullptr);
					}
					return VariableDeref;
				}
			}
		}

		// If we're here, no built-in variables matched.
		bool bUseSlice = false;
		bool bUseViewport = false;
		bool bUseSampleID = false;
		if (FCStringAnsi::Strnicmp(Semantic, "SV_", 3) == 0)
		{
			if (FCStringAnsi::Strnicmp(Semantic, "SV_RenderTargetArrayIndex", 25) == 0)
			{
				bUseSlice = true;
			}
			else if (FCStringAnsi::Strnicmp(Semantic, "SV_ViewPortArrayIndex", 21) == 0)
			{
				bUseViewport = true;
			}
			else if (FCStringAnsi::Strnicmp(Semantic, "SV_SampleIndex", 14) == 0)
			{
				bUseSampleID = true;
			}
			else
			{
				_mesa_glsl_warning(ParseState, "unrecognized system value input '%s'", Semantic);
			}
		}
		
		ir_variable* Variable = new(ParseState)ir_variable(
														   Type,
														   ralloc_asprintf(ParseState, "IN_%s", Semantic),
														   ir_var_in);
		if (Frequency == HSF_VertexShader)
		{
			if (!FCStringAnsi::Strnicmp(Semantic, "ATTRIBUTE", 9))
			{
				Variable->semantic = ralloc_asprintf(ParseState, "[[ attribute(%s) ]]", Semantic);
			}
			else if (FCStringAnsi::Strnicmp(Semantic, "[[", 2))
			{
				_mesa_glsl_warning(ParseState, "Unrecognized input attribute '%s'", Semantic);
			}
		}
		else if (bUseSlice)
		{
			check(Frequency == HSF_PixelShader);
			Variable->semantic = ralloc_asprintf(ParseState, "[[ render_target_array_index ]]");
		}
		else if (bUseViewport)
		{
			check(Frequency == HSF_PixelShader);
			Variable->semantic = ralloc_asprintf(ParseState, "[[ viewport_array_index ]]");
		}
		else if (bUseSampleID)
		{
			check(Frequency == HSF_PixelShader);
			Variable->semantic = ralloc_asprintf(ParseState, "[[ sample_id ]]");
		}

		if (Variable->type->is_patch())
		{
			// do not add any semantics for patch-types
		}
		else
		if (!Variable->semantic)
		{
			Variable->semantic = ralloc_asprintf(ParseState, "[[ user(%s) ]]", Semantic);
		}
		Variable->read_only = true;
		DeclInstructions->push_tail(Variable);
		ParseState->symbols->add_variable(Variable);
		ir_dereference_variable* VariableDeref = new(ParseState)ir_dereference_variable(Variable);
		return VariableDeref;
	}

	static void GenerateInputForVariable(EHlslShaderFrequency Frequency, EMetalGPUSemantics bIsDesktop, _mesa_glsl_parse_state* ParseState,
		const char* InputSemantic, ir_dereference* InputVariableDeref, exec_list* DeclInstructions, exec_list* PreCallInstructions)
	{
		const glsl_type* InputType = InputVariableDeref->type;
		if (InputType->is_record())
		{
			for (uint32 i = 0; i < InputType->length; ++i)
			{
				const char* Semantic = nullptr;
				const char* FieldSemantic = InputType->fields.structure[i].semantic;
				if (InputSemantic && FieldSemantic)
				{
					_mesa_glsl_warning(ParseState, "semantic '%s' of field '%s' will be overridden by enclosing types' semantic '%s'",
						InputType->fields.structure[i].semantic,
						InputType->fields.structure[i].name,
						InputSemantic);
					FieldSemantic = nullptr;
				}
				else if (InputSemantic && !FieldSemantic)
				{
					Semantic = ralloc_asprintf(ParseState, "%s%d", InputSemantic, i);
					_mesa_glsl_warning(ParseState, "  creating semantic '%s' for struct field '%s'", Semantic, InputType->fields.structure[i].name);
				}
				else if (!InputSemantic && FieldSemantic)
				{
					Semantic = FieldSemantic;
				}
				else
				{
					Semantic = nullptr;
				}

				if (InputType->fields.structure[i].type->is_record() || Semantic)
				{
					ir_dereference_record* FieldDeref = new(ParseState)ir_dereference_record(
						InputVariableDeref->clone(ParseState, NULL),
						InputType->fields.structure[i].name);
					GenerateInputForVariable(Frequency, bIsDesktop, ParseState, Semantic, FieldDeref, DeclInstructions, PreCallInstructions);
				}
				else
				{
					_mesa_glsl_error(
						ParseState,
						"field '%s' in input structure '%s' does not specify a semantic",
						InputType->fields.structure[i].name,
						InputType->name
						);
				}
			}
		}
		else
		{
			if (InputType->is_array())
			{
				int BaseIndex = 0;
				const char* Semantic = 0;
				check(InputSemantic);
				ParseSemanticAndIndex(ParseState, InputSemantic, &Semantic, &BaseIndex);
				check(BaseIndex >= 0);
				for (unsigned i = 0; i < InputType->length; ++i)
				{
					ir_dereference_array* ArrayDeref = new(ParseState)ir_dereference_array(
						InputVariableDeref->clone(ParseState, NULL),
						new(ParseState)ir_constant((unsigned)i)
						);
					GenerateInputForVariable(
						Frequency,
						bIsDesktop,
						ParseState,
						ralloc_asprintf(ParseState, "%s%d", Semantic, BaseIndex + i),
						ArrayDeref,
						DeclInstructions,
						PreCallInstructions);
				}
			}
			else
			{
				ir_rvalue* SrcValue = GenerateInputFromSemantic(Frequency, bIsDesktop, ParseState, InputSemantic, InputType, DeclInstructions, PreCallInstructions);
				if (SrcValue)
				{
					YYLTYPE loc;
					apply_type_conversion(InputType, SrcValue, PreCallInstructions, ParseState, true, &loc);
					PreCallInstructions->push_tail(
						new(ParseState) ir_assignment(InputVariableDeref->clone(ParseState, NULL),SrcValue));
				}
			}
		}
	}

	ir_dereference_variable* GenerateInput(EHlslShaderFrequency Frequency, uint32 bIsDesktop, _mesa_glsl_parse_state* ParseState, const char* InputSemantic, const glsl_type* InputType, exec_list* DeclInstructions, exec_list* PreCallInstructions)
	{
		if((InputType->is_inputpatch()))
		{
			return GenerateInputFromSemantic(Frequency, (EMetalGPUSemantics)bIsDesktop, ParseState, InputSemantic, InputType, DeclInstructions, PreCallInstructions)->as_dereference_variable();
		}
		ir_variable* TempVariable = new(ParseState)ir_variable(InputType, nullptr, ir_var_temporary);
		ir_dereference_variable* TempVariableDeref = new(ParseState)ir_dereference_variable(TempVariable);
		PreCallInstructions->push_tail(TempVariable);
		GenerateInputForVariable(Frequency, (EMetalGPUSemantics)bIsDesktop, ParseState, InputSemantic, TempVariableDeref, DeclInstructions, PreCallInstructions);
		return TempVariableDeref;
	}

	static ir_rvalue* GenerateOutputFromSemantic(EHlslShaderFrequency Frequency, uint32 bIsDesktop, _mesa_glsl_parse_state* ParseState,
		const char* Semantic, FSemanticQualifier Qualifier, const glsl_type* Type, exec_list* DeclInstructions, const glsl_type** DestVariableType)
	{
		ir_variable* Variable = NULL;

		if (FCStringAnsi::Strnicmp(Semantic, "SV_", 3) == 0)
		{
			FSystemValue* SystemValues = (bIsDesktop == EMetalGPUSemanticsMobile) ? MobileSystemValueTable[Frequency] : DesktopSystemValueTable[Frequency];
			
			for (int i = 0; SystemValues[i].HlslSemantic != nullptr; ++i)
			{
				if (SystemValues[i].Mode == ir_var_out && FCStringAnsi::Stricmp(SystemValues[i].HlslSemantic, Semantic) == 0 && SystemValues[i].Type == Type)
				{
					Variable = new(ParseState) ir_variable(SystemValues[i].Type, SystemValues[i].MetalName, ir_var_out);
					Variable->semantic = SystemValues[i].MetalSemantic;
					break;
				}
			}
			
			if (!Variable)
			{
				for (int i = 0; SystemValues[i].HlslSemantic != nullptr; ++i)
				{
					if (SystemValues[i].Mode == ir_var_out && FCStringAnsi::Stricmp(SystemValues[i].HlslSemantic, Semantic) == 0 && SystemValues[i].Type->vector_elements == Type->vector_elements)
					{
						Variable = new(ParseState) ir_variable(SystemValues[i].Type, SystemValues[i].MetalName, ir_var_out);
						Variable->semantic = SystemValues[i].MetalSemantic;
						break;
					}
				}
			}
		}

		// Need to generate a single clip-distance for broken desktop drivers - done by simply dropping the higher clip-distances
		// As it happens we already order them by importance (0: Global > 1: VR-instanced fallback > 2: vertex-shader-layer)
		uint32 const ClipPrefixLen = 15;
		if ((bIsDesktop == EMetalGPUSemanticsImmediateDesktop) && Semantic && FCStringAnsi::Strnicmp(Semantic, "SV_ClipDistance", ClipPrefixLen) == 0 && !Variable)
		{
			FMetalLanguageSpec* Spec = (FMetalLanguageSpec*)ParseState->LanguageSpec;
			uint32 const Count = Spec->GetClipDistanceCount();
			uint32 const Used = Spec->ClipDistancesUsed;
			check(Count > 0);
			
			uint32 Index = 0;
			if (Semantic[ClipPrefixLen] >= '1' && Semantic[ClipPrefixLen] <= '7')
			{
				Index = Semantic[ClipPrefixLen] - '0';
			}
			
			bool bUsed = false;
			for (uint32 i = 0; !bUsed && i < Index; i++)
			{
				bUsed = (Used & (1 << i)) != 0;
			}
			
			if (!bUsed)
			{
				ir_variable* compacted_clip = ParseState->symbols->get_variable("clip_distance_array");
				if (!compacted_clip)
				{
					compacted_clip = new(ParseState)ir_variable(glsl_type::float_type, "clip_distance_array", ir_var_out);
					compacted_clip->semantic = ralloc_asprintf(ParseState, "[[ clip_distance ]]");
					DeclInstructions->push_tail(compacted_clip);
					ParseState->symbols->add_variable(compacted_clip);
				}
				*DestVariableType = glsl_type::float_type;
				ir_rvalue* VariableDeref = new(ParseState)ir_dereference_variable(compacted_clip);
				return VariableDeref;
			}
		}
		
		// But for iOS/tvOS and future, non-broken desktop we can just remap the variable to the actual clip-distance-array
		if ((bIsDesktop != EMetalGPUSemanticsImmediateDesktop) && Semantic && FCStringAnsi::Strnicmp(Semantic, "SV_ClipDistance", ClipPrefixLen) == 0 && !Variable)
		{
			Variable = ParseState->symbols->get_variable("clip_distance_array");
			
			FMetalLanguageSpec* Spec = (FMetalLanguageSpec*)ParseState->LanguageSpec;
			uint32 const Count = Spec->GetClipDistanceCount();
			check(Count > 0);
			
			*DestVariableType = (Count > 1) ? glsl_type::get_array_instance(glsl_type::float_type, Count) : glsl_type::float_type;
			if (!Variable)
			{
				Variable = new(ParseState)ir_variable(*DestVariableType, "clip_distance_array", ir_var_out);
				Variable->semantic = ralloc_asprintf(ParseState, "[[ clip_distance ]]");
				DeclInstructions->push_tail(Variable);
				ParseState->symbols->add_variable(Variable);
			}

			ir_rvalue* VariableDeref = new(ParseState)ir_dereference_variable(Variable);
			if (Count > 0)
			{
				uint32 Index = 0;
				if (Semantic[ClipPrefixLen] >= '1' && Semantic[ClipPrefixLen] <= '7')
				{
					Index = Semantic[ClipPrefixLen] - '0';
				}
				ir_variable* IndexVar = nullptr;
				for (uint32 i = 0; i < 8; i++)
				{
					check(i < Count);
					char* IndexName = ralloc_asprintf(ParseState, "ClipDistanceIndex%u", i);
					IndexVar = ParseState->symbols->get_variable(IndexName);
					if (!IndexVar)
					{
						IndexVar = new(ParseState)ir_variable(*DestVariableType, IndexName, ir_var_const_in);
						IndexVar->constant_value = new(ParseState) ir_constant((unsigned)i);
						IndexVar->constant_initializer = new(ParseState) ir_constant((unsigned)i);
						ParseState->symbols->add_variable(IndexVar);
						break;
					}
				}
				check(IndexVar);
				ir_dereference_array* ArrayDeref = new(ParseState)ir_dereference_array(
																					   VariableDeref,
																					   IndexVar->constant_value->clone(ParseState, NULL)
																					   );
				return ArrayDeref;
			}
			else
			{
				return VariableDeref;
			}
		}
		
		if (Semantic && FCStringAnsi::Strnicmp(Semantic, "SV_", 3) == 0 && !Variable)
		{
			_mesa_glsl_warning(ParseState, "unrecognized system value output '%s'", Semantic);
		}

		if (!Variable)
		{
			Variable = new(ParseState)ir_variable(Type, ralloc_asprintf(ParseState, "OUT_%s", Semantic), ir_var_out);
			Variable->semantic = ralloc_asprintf(ParseState, "[[ user(%s) ]]", Semantic);
			if (Qualifier.Fields.bIsPatchConstant)
			{
				// Propagate the semantic straight through for things like SV_TessFactor and SV_InsideTessFactor as they aren't treated as
				// system variables yet.
				Variable->semantic = Semantic;
			}
		}

		*DestVariableType = Variable->type;
		DeclInstructions->push_tail(Variable);
		ParseState->symbols->add_variable(Variable);
		ir_rvalue* VariableDeref = new(ParseState)ir_dereference_variable(Variable);
		return VariableDeref;
	}

	static void GenerateOutputForVariable(EHlslShaderFrequency Frequency, EMetalGPUSemantics bIsDesktop, _mesa_glsl_parse_state* ParseState,
		const char* OutputSemantic, FSemanticQualifier Qualifier, ir_dereference* OutputVariableDeref, exec_list* DeclInstructions, exec_list* PostCallInstructions
		/*int SemanticArraySize,int SemanticArrayIndex*/)
	{
		const glsl_type* OutputType = OutputVariableDeref->type;
		if (OutputType->is_record())
		{
			for (uint32 i = 0; i < OutputType->length; ++i)
			{
				const char* FieldSemantic = OutputType->fields.structure[i].semantic;
				const char* Semantic = nullptr;
				if (OutputSemantic && FieldSemantic)
				{
					_mesa_glsl_warning(ParseState, "semantic '%s' of field '%s' will be overridden by enclosing types' semantic '%s'",
						OutputType->fields.structure[i].semantic,
						OutputType->fields.structure[i].name,
						OutputSemantic);
					FieldSemantic = nullptr;
				}
				else if (OutputSemantic && !FieldSemantic)
				{
					Semantic = ralloc_asprintf(ParseState, "%s%d", OutputSemantic, i);
					_mesa_glsl_warning(ParseState, "  creating semantic '%s' for struct field '%s'", Semantic, OutputType->fields.structure[i].name);
				}
				else if (!OutputSemantic && FieldSemantic)
				{
					Semantic = FieldSemantic;
				}
				else
				{
					Semantic = nullptr;
				}

				if (OutputType->fields.structure[i].type->is_record() || Semantic)
				{
					// Dereference the field and generate shader outputs for the field.
					ir_dereference* FieldDeref = new(ParseState)ir_dereference_record(
						OutputVariableDeref->clone(ParseState, NULL),
						OutputType->fields.structure[i].name);
					GenerateOutputForVariable(Frequency, bIsDesktop, ParseState, Semantic, Qualifier, FieldDeref, DeclInstructions, PostCallInstructions);
				}
				else
				{
					_mesa_glsl_error(
						ParseState,
						"field '%s' in output structure '%s' does not specify a semantic",
						OutputType->fields.structure[i].name,
						OutputType->name
						);
				}
			}
		}
		else
		{
			if (!OutputSemantic)
			{
				_mesa_glsl_error(ParseState, "Entry point does not specify a semantic for its return value");
			}
			else
			{
				if (OutputType->is_array())
				{
					int BaseIndex = 0;
					const char* Semantic = 0;

					ParseSemanticAndIndex(ParseState, OutputSemantic, &Semantic, &BaseIndex);

					for (unsigned i = 0; i < OutputType->length; ++i)
					{
						ir_dereference_array* ArrayDeref = new(ParseState)ir_dereference_array(
							OutputVariableDeref->clone(ParseState, NULL),
							new(ParseState) ir_constant((unsigned)i)
							);
						GenerateOutputForVariable(Frequency, bIsDesktop, ParseState,
							ralloc_asprintf(ParseState, "%s%d", Semantic, BaseIndex + i),
							Qualifier,
							ArrayDeref, DeclInstructions, PostCallInstructions);
					}
				}
				else
				{
					YYLTYPE loc;
					ir_rvalue* Src = OutputVariableDeref->clone(ParseState, NULL);
					const glsl_type* DestVariableType = NULL;
					ir_rvalue* DestVariableDeref = GenerateOutputFromSemantic(Frequency, bIsDesktop, ParseState, OutputSemantic,
						Qualifier,
						OutputType, DeclInstructions, &DestVariableType);

					apply_type_conversion(DestVariableType, Src, PostCallInstructions, ParseState, true, &loc);
					PostCallInstructions->push_tail(new(ParseState)ir_assignment(DestVariableDeref, Src));
				}
			}
		}
	}

	ir_dereference_variable* GenerateOutput(EHlslShaderFrequency Frequency, uint32 bIsDesktop, _mesa_glsl_parse_state* ParseState,
		const char* OutputSemantic, FSemanticQualifier Qualifier, const glsl_type* OutputType, exec_list* DeclInstructions, exec_list* PreCallInstructions, exec_list* PostCallInstructions)
	{
		// Generate a local variable to hold the output.
		ir_variable* TempVariable = new(ParseState) ir_variable(OutputType, nullptr, ir_var_temporary);
		ir_dereference_variable* TempVariableDeref = new(ParseState) ir_dereference_variable(TempVariable);
		PreCallInstructions->push_tail(TempVariable);

		GenerateOutputForVariable(Frequency, (EMetalGPUSemantics)bIsDesktop, ParseState, OutputSemantic, Qualifier, TempVariableDeref, DeclInstructions, PostCallInstructions);

		return TempVariableDeref;
	}
}

struct FFixIntrinsicsVisitor : public ir_rvalue_visitor
{
	_mesa_glsl_parse_state* State;
	bool bUsesFramebufferFetchES2;
	int MRTFetchMask;
	ir_variable* DestColorVar;
	const glsl_type* DestColorType;
	ir_variable* DestMRTColorVar[MAX_SIMULTANEOUS_RENDER_TARGETS];

	FFixIntrinsicsVisitor(_mesa_glsl_parse_state* InState, ir_function_signature* InMainSig) :
		State(InState),
		bUsesFramebufferFetchES2(false),
		MRTFetchMask(0),
		DestColorVar(nullptr),
		DestColorType(glsl_type::error_type)
	{
		DestColorType = GetFragColorTypeFromMetalOutputStruct(InMainSig->return_type);
		memset(DestMRTColorVar, 0, sizeof(DestMRTColorVar));
	}

	//ir_visitor_status visit_leave(ir_expression* expr) override
	virtual void handle_rvalue(ir_rvalue** RValue)
	{
		if (!RValue || !*RValue)
		{
			return;
		}


		// Fix .x swizzle of scalars...
		auto* Swizzle = (*RValue)->as_swizzle();
		if (Swizzle)
		{
			auto* Texture = Swizzle->val->as_texture();
			if (Texture && Texture->op == ir_txf && 
				Texture->sampler && Texture->sampler->type->sampler_buffer && 
				Texture->sampler->type->inner_type && Texture->sampler->type->inner_type->is_scalar() && 
				Swizzle->mask.x == 0 &&
				Swizzle->mask.y == 0 &&
				Swizzle->mask.z == 0 &&
				Swizzle->mask.w == 0 &&
				Swizzle->mask.num_components == 1 &&
				Swizzle->mask.has_duplicates == 0)
			{
				*RValue = Texture;
			}
		}

		auto* expr = (*RValue)->as_expression();
		if (!expr)
		{
			return;
		}

		ir_expression_operation op = expr->operation;

		if (op == ir_binop_mul && expr->type->is_matrix()
			&& expr->operands[0]->type->is_matrix()
			&& expr->operands[1]->type->is_matrix())
		{
			// Convert matrixCompMult to memberwise multiply
			check(expr->operands[0]->type == expr->operands[1]->type);
			auto* NewTemp = new(State)ir_variable(expr->operands[0]->type, nullptr, ir_var_temporary);
			base_ir->insert_before(NewTemp);
			for (uint32 Index = 0; Index < expr->operands[0]->type->matrix_columns; ++Index)
			{
				auto* NewMul = new(State)ir_expression(ir_binop_mul,
					new(State)ir_dereference_array(expr->operands[0], new(State)ir_constant(Index)),
					new(State)ir_dereference_array(expr->operands[1], new(State)ir_constant(Index)));
				auto* NewAssign = new(State)ir_assignment(
					new(State)ir_dereference_array(NewTemp, new(State)ir_constant(Index)),
					NewMul);
				base_ir->insert_before(NewAssign);
			}

			*RValue = new(State)ir_dereference_variable(NewTemp);
		}
	}

	virtual ir_visitor_status visit_leave(ir_call* IR) override
	{
		if (IR->use_builtin)
		{
			const char* CalleeName = IR->callee_name();
			static auto ES2Len = strlen(FRAMEBUFFER_FETCH_ES2);
			static auto MRTLen = strlen(FRAMEBUFFER_FETCH_MRT);
			if (!strncmp(CalleeName, FRAMEBUFFER_FETCH_ES2, ES2Len))
			{
				// 'Upgrade' framebuffer fetch
				check(IR->actual_parameters.is_empty());
				bUsesFramebufferFetchES2 = true;
				if (!DestColorVar)
				{
					// Generate new input variable for Metal semantics
					DestColorVar = new(State)ir_variable(glsl_type::get_instance(DestColorType->base_type, 4, 1), "gl_LastFragData", ir_var_in);
					DestColorVar->semantic = "[[ color(0) ]]";
				}

				ir_rvalue* DestColor = new(State)ir_dereference_variable(DestColorVar);
				if (IR->return_deref->type->base_type != DestColor->type->base_type)
				{
					DestColor = convert_component(DestColor, IR->return_deref->type);
				}
				auto* Assignment = new (State)ir_assignment(IR->return_deref, DestColor);
				IR->insert_before(Assignment);
				IR->remove();
			}
			else if (!strncmp(CalleeName, FRAMEBUFFER_FETCH_MRT, MRTLen))
			{
				int Index = atoi(CalleeName + MRTLen);
				if (!DestMRTColorVar[Index])
				{
					DestMRTColorVar[Index] = new(State)ir_variable(glsl_type::get_instance(DestColorType->base_type, 4, 1), CalleeName, ir_var_in);
					DestMRTColorVar[Index]->semantic = ralloc_asprintf(State, "[[ color(%d) ]]", Index);
				}

				ir_rvalue* DestColor = new(State) ir_dereference_variable(DestMRTColorVar[Index]);
				if (IR->return_deref->type->base_type != DestColor->type->base_type)
				{
					DestColor = convert_component(DestColor, IR->return_deref->type);
				}
				auto* Assignment = new (State) ir_assignment(IR->return_deref, DestColor);
				IR->insert_before(Assignment);
				IR->remove();
			}
		}

		return visit_continue;
	}
};

void FMetalCodeBackend::FixIntrinsics(exec_list* ir, _mesa_glsl_parse_state* state)
{
	ir_function_signature* MainSig = GetMainFunction(ir);
	check(MainSig);

	FFixIntrinsicsVisitor Visitor(state,MainSig);
	Visitor.run(&MainSig->body);

	if (Visitor.bUsesFramebufferFetchES2)
	{
		check(Visitor.DestColorVar);
		MainSig->parameters.push_tail(Visitor.DestColorVar);
	}

	for (int i = 0; i < MAX_SIMULTANEOUS_RENDER_TARGETS; ++i)
	{
		if (Visitor.DestMRTColorVar[i])
		{
			MainSig->parameters.push_tail(Visitor.DestMRTColorVar[i]);
		}
	}
}

struct FConvertUBVisitor : public ir_rvalue_visitor
{
	_mesa_glsl_parse_state* State;
	TStringIRVarMap& Map;
	FConvertUBVisitor(_mesa_glsl_parse_state* InState, TStringIRVarMap& InMap) :
		State(InState),
		Map(InMap)
	{
	}

	virtual void handle_rvalue(ir_rvalue** RValuePtr) override
	{
		if (!RValuePtr || !*RValuePtr)
		{
			return;
		}
		auto* ReferencedVar = (*RValuePtr)->variable_referenced();
		if (ReferencedVar && ReferencedVar->mode == ir_var_uniform && ReferencedVar->semantic)
		{
			auto FoundIter = Map.find(ReferencedVar->semantic);
			if (FoundIter != Map.end())
			{
				auto* StructVar = FoundIter->second;
				StructVar->used = 1;

				// Actually replace the variable
				auto* DeRefVar = (*RValuePtr)->as_dereference_variable();
				if (DeRefVar)
				{
					*RValuePtr = new(State)ir_dereference_record(StructVar, ReferencedVar->name);
				}
				else
				{
					check(0);
				}
			}
		}
	}
};

void FMetalCodeBackend::MovePackedUniformsToMain(exec_list* ir, _mesa_glsl_parse_state* state, FBuffers& OutBuffers)
{
	//IRDump(ir);
	TStringIRVarMap CBVarMap;

	// Now make a new struct type and global variable per uniform buffer
	for (uint32 i = 0; i < state->num_uniform_blocks; ++i)
		//	for (auto& CB : state->CBuffersOriginal)
	{
		auto* CBP = state->FindCBufferByName(false, state->uniform_blocks[i]->name);
		check(CBP);
		auto& CB = *CBP;
		if (!CB.Members.empty())
		{
			glsl_struct_field* Fields = ralloc_array(state, glsl_struct_field, (unsigned)CB.Members.size());
			uint32 Index = 0;
			for (auto& Member : CB.Members)
			{
				check(Member.Var);
				Fields[Index++] = glsl_struct_field(Member.Var->type, ralloc_strdup(state, Member.Var->name));
			}

			auto* Type = glsl_type::get_record_instance(Fields, (unsigned)CB.Members.size(), ralloc_asprintf(state, "CB_%s", CB.Name.c_str()));
			// Hack: This way we tell this is a uniform buffer and we need to emit 'packed_'
			((glsl_type*)Type)->HlslName = "__PACKED__";
			state->AddUserStruct(Type);

			auto* Var = new(state)ir_variable(Type, ralloc_asprintf(state, "%s", CB.Name.c_str()), ir_var_uniform);
			CBVarMap[CB.Name] = Var;
		}
	}

	FConvertUBVisitor ConvertVisitor(state, CBVarMap);
	ConvertVisitor.run(ir);

	std::set<const glsl_type*> PendingTypes;
	std::set<const glsl_type*> ProcessedTypes;

	// Actually only save the used variables
	for (auto& Pair : CBVarMap)
	{
		auto* Var = Pair.second;
		if (Var->used)
		{
			// Go through each struct type and mark it as packed
			ir->push_head(Var);
			if (Var->type->is_record())
			{
				PendingTypes.insert(Var->type);
			}
		}
	}

	// Mark all structures as packed
	while (!PendingTypes.empty())
	{
		auto* Type = *PendingTypes.begin();
		PendingTypes.erase(PendingTypes.begin());
		if (ProcessedTypes.find(Type) == ProcessedTypes.end())
		{
			ProcessedTypes.insert(Type);
			((glsl_type*)Type)->HlslName = "__PACKED__";

			for (uint32 i = 0; i < Type->length; ++i)
			{
				if (Type->fields.structure[i].type->is_record())
				{
					PendingTypes.insert(Type->fields.structure[i].type);
				}
			}
		}
	}

	ir_function_signature* MainSig = GetMainFunction(ir);
	check(MainSig);

	// Gather all globals still lying outside Main
	foreach_iter(exec_list_iterator, iter, *ir)
	{
		auto* Instruction = ((ir_instruction*)iter.get());
		auto* Var = Instruction->as_variable();
		if (Var)
		{
			bool bIsBuffer = false;
			bool bIsStructuredBuffer = Var->type->sampler_buffer && (Var->type->inner_type->is_record() || !strncmp(Var->type->name, "RWStructuredBuffer<", 19) || !strncmp(Var->type->name, "StructuredBuffer<", 17));
			bool bIsByteAddressBuffer = Var->type->sampler_buffer && (!strncmp(Var->type->name, "RWByteAddressBuffer<", 20) || !strncmp(Var->type->name, "ByteAddressBuffer<", 18));
							
			switch(TypedMode)
			{
				case EMetalTypeBufferModeNone:
				{
					bIsBuffer = (!Var->type->is_sampler() && !Var->type->is_image()) || Var->type->sampler_buffer;
					break;
				}
				case EMetalTypeBufferModeSRV:
				{
					bIsBuffer = (!Var->type->is_sampler() && !Var->type->is_image()) || (Var->type->sampler_buffer && (Var->type->is_image() || bIsStructuredBuffer || bIsByteAddressBuffer || OutBuffers.AtomicVariables.find(Var) != OutBuffers.AtomicVariables.end()));
					break;
				}
				case EMetalTypeBufferModeUAV:
				{
					bIsBuffer = (!Var->type->is_sampler() && !Var->type->is_image()) || (Var->type->sampler_buffer && (OutBuffers.AtomicVariables.find(Var) != OutBuffers.AtomicVariables.end() || bIsStructuredBuffer || bIsByteAddressBuffer));
					break;
				}
				default:
					check(false);
					break;
			}
			if (bIsBuffer)
			{
				OutBuffers.AddBuffer(Var);
			}
			else
			{
				OutBuffers.AddTexture(Var);
			}
		}
	}
    
	OutBuffers.SortBuffers(state);

	// And move them to main
	for (auto Iter : OutBuffers.Buffers)
	{
		auto* Var = (ir_variable*)Iter;
		if (Var)
		{
			Var->remove();
			MainSig->parameters.push_tail(Var);
		}
    }
    
    // And move them to main
    for (auto Iter : OutBuffers.Textures)
    {
        auto* Var = (ir_variable*)Iter;
        if (Var)
        {
            Var->remove();
            MainSig->parameters.push_tail(Var);
        }
    }
	//IRDump(ir, state);
}


void FMetalCodeBackend::PromoteInputsAndOutputsGlobalHalfToFloat(exec_list* Instructions, _mesa_glsl_parse_state* State, EHlslShaderFrequency Frequency)
{
	//IRDump(Instructions);
	ir_function_signature* EntryPointSig = GetMainFunction(Instructions);
	check(EntryPointSig);
	foreach_iter(exec_list_iterator, Iter, *Instructions)
	{
		ir_instruction* IR = (ir_instruction*)Iter.get();
		auto* Variable = IR->as_variable();
		if (Variable)
		{
			auto* NewType = PromoteHalfToFloatType(State, Variable->type);
			if (Variable->type == NewType)
			{
				continue;
			}

			switch (Variable->mode)
			{
			case ir_var_in:
			{
				auto* NewVar = new(State)ir_variable(NewType, Variable->name, ir_var_in);
				NewVar->semantic = Variable->semantic;
				Variable->insert_before(NewVar);
				Variable->name = nullptr;
				Variable->semantic = nullptr;
				Variable->mode = ir_var_temporary;
				Variable->remove();
				exec_list Assignments;
				Assignments.push_head(Variable);
				CreateNewAssignmentsFloat2Half(State, Assignments, Variable, new(State)ir_dereference_variable(NewVar));
				EntryPointSig->body.get_head()->insert_before(&Assignments);
			}
				break;

			case ir_var_out:
			{
				if(bIsTessellationVSHS)
				{
					// do nothing
				}
				else
				if (Frequency != HSF_PixelShader)
				{
				   auto* NewVar = new(State)ir_variable(NewType, Variable->name, ir_var_out);
				   NewVar->semantic = Variable->semantic;
				   Variable->insert_before(NewVar);
				   Variable->name = nullptr;
				   Variable->semantic = nullptr;
				   Variable->mode = ir_var_temporary;
				   Variable->remove();
				   exec_list Assignments;
				   CreateNewAssignmentsHalf2Float(State, Assignments, NewVar, new(State)ir_dereference_variable(Variable));
				   EntryPointSig->body.push_head(Variable);
				   EntryPointSig->body.append_list(&Assignments);
				}
			}
			   break;
			}
		}
	}
}

static bool ProcessStageInVariables(_mesa_glsl_parse_state* ParseState, EMetalGPUSemantics bIsDesktop, EHlslShaderFrequency Frequency, ir_variable* Variable, TArray<glsl_struct_field>& OutStageInMembers, std::set<ir_variable*>& OutStageInVariables, unsigned int* OutVertexAttributesMask, TIRVarList& OutFunctionArguments)
{
	// Don't move variables that are system values into the input structures
	const auto* SystemValues = (bIsDesktop == EMetalGPUSemanticsMobile) ? MetalUtils::MobileSystemValueTable[Frequency] : MetalUtils::DesktopSystemValueTable[Frequency];
	for(int i = 0; SystemValues[i].MetalSemantic != nullptr; ++i)
	{
		if (Variable->semantic && !FCStringAnsi::Stricmp(Variable->semantic, "SV_DomainLocation"))
		{
			check(Frequency == HSF_DomainShader);
		}
		else if (Variable->semantic && SystemValues[i].Mode == ir_var_in && FCStringAnsi::Stricmp(SystemValues[i].MetalSemantic, Variable->semantic) == 0)
		{
			return true;
		}
	}


	if (Frequency == HSF_VertexShader)
	{
		// Generate an uber struct
		if (Variable->type->is_record())
		{
			check(0);
		}
		else
		{
			int AttributeIndex = GetInAttributeIndex(Variable->name);
			if (AttributeIndex >= 0)
			{
				if (Variable->type->is_array())
				{
					check(Variable->type->element_type()->is_vector());
					for (uint32 i = 0; i < Variable->type->length; ++i, ++AttributeIndex)
					{
						glsl_struct_field OutMember;
						OutMember.type = Variable->type->element_type();
						OutMember.semantic = ralloc_asprintf(ParseState, "ATTRIBUTE%d", AttributeIndex);
						OutMember.name = ralloc_asprintf(ParseState, "ATTRIBUTE%d", AttributeIndex);

						if (OutVertexAttributesMask)
						{
							*OutVertexAttributesMask |= (1 << AttributeIndex);
						}

						OutStageInMembers.Add(OutMember);
					}
				}
				else
				{
					glsl_struct_field OutMember;
					OutMember.type = Variable->type;
					OutMember.semantic = ralloc_asprintf(ParseState, "ATTRIBUTE%d", AttributeIndex);
					OutMember.name = ralloc_asprintf(ParseState, "IN_ATTRIBUTE%d", AttributeIndex);

					if (OutVertexAttributesMask)
					{
						*OutVertexAttributesMask |= (1 << AttributeIndex);
					}

					OutStageInMembers.Add(OutMember);
				}
			}
			else if (!strcmp(Variable->name, "gl_VertexID") || !strcmp(Variable->name, "gl_InstanceID"))
			{
				OutFunctionArguments.push_back(Variable);
				return true;
			}
			else
			{
				_mesa_glsl_error(ParseState, "Unknown semantic for input attribute %s!\n", Variable->semantic ? Variable->semantic : "", Variable->name);
				check(0);
				return false;
			}
		}

		OutStageInVariables.insert(Variable);
		return true;
	}
	else if(Frequency != HSF_HullShader && Frequency != HSF_DomainShader)
	{
		check(Frequency == HSF_PixelShader);
		if (!strcmp(Variable->name, "gl_FrontFacing"))
		{
			// Make sure we add a semantic
			Variable->semantic = "gl_FrontFacing";
			return true;
		}
	}

	glsl_struct_field Member;
	Member.type = Variable->type;
	Member.name = ralloc_strdup(ParseState, Variable->name);
	Member.semantic = ralloc_strdup(ParseState, Variable->semantic ? Variable->semantic : Variable->name);
	OutStageInMembers.Add(Member);
	OutStageInVariables.insert(Variable);

	return true;
}


/** Information on system values. */
struct FSystemValue
{
	const char* Semantic;
	const glsl_type* Type;
	const char* GlslName;
	ir_variable_mode Mode;
	bool bOriginUpperLeft;
	bool bArrayVariable;
};

/** Vertex shader system values. */
static FSystemValue VertexSystemValueTable[] =
{
	{"SV_VertexID", glsl_type::uint_type, "gl_VertexID", ir_var_in, false, false},
	{"SV_InstanceID", glsl_type::uint_type, "gl_InstanceID", ir_var_in, false, false},
	{"SV_RenderTargetArrayIndex", glsl_type::uint_type, "OUT_Layer", ir_var_out, false, false},
	{"SV_ViewPortArrayIndex", glsl_type::uint_type, "OUT_Viewport", ir_var_out, false, false},
    //{ "SV_Position", glsl_type::vec4_type, "gl_Position", ir_var_out, false, true },
	{NULL, NULL, NULL, ir_var_auto, false, false}
};

/** Pixel shader system values. */
static FSystemValue PixelSystemValueTable[] =
{
	{"SV_Depth", glsl_type::float_type, "gl_FragDepth", ir_var_out, false, false},
	{"SV_Position", glsl_type::vec4_type, "gl_FragCoord", ir_var_in, true, false},
	{"SV_Coverage", glsl_type::uint_type, "IN_Coverage", ir_var_in, false, false},
	{"SV_Coverage", glsl_type::uint_type, "OUT_Coverage", ir_var_out, false, false},
	//	{ "SV_IsFrontFace", glsl_type::bool_type, "gl_FrontFacing", ir_var_in, false, true },
    {"SV_PrimitiveID", glsl_type::int_type, "gl_PrimitiveID", ir_var_in, false, false},
	{"SV_RenderTargetArrayIndex", glsl_type::uint_type, "IN_Layer", ir_var_in, false, false},
	{"SV_ViewPortArrayIndex", glsl_type::uint_type, "IN_Viewport", ir_var_in, false, false},
	//	{ "SV_RenderTargetArrayIndex", glsl_type::uint_type, "gl_Layer", ir_var_in, false, false },
	//	{ "SV_Target0", glsl_type::vec4_type, "gl_FragColor", ir_var_out, false, true },
	{ "SV_SampleIndex", glsl_type::uint_type, "IN_SampleID", ir_var_in, false, false },
	{NULL, NULL, NULL, ir_var_auto, false, false}
};

static FSystemValue* SystemValueTable[] =
{
	VertexSystemValueTable,
	PixelSystemValueTable,
	nullptr,
	nullptr,
	nullptr,
	nullptr
};

/**
* Generate a shader input.
* @param Frequency - The shader frequency.
* @param ParseState - Parse state.
* @param InputSemantic - The semantic name to generate.
* @param InputQualifier - Qualifiers applied to the semantic.
* @param InputType - Value type.
* @param DeclInstructions - IR to which declarations may be added.
* @param PreCallInstructions - IR to which instructions may be added before the
*                              entry point is called.
* @returns the IR variable deref for the semantic.
*/
static ir_dereference_variable* GenerateShaderInput(
	EHlslShaderFrequency Frequency, EMetalGPUSemantics bIsDesktop,
	_mesa_glsl_parse_state* ParseState,
	const char* InputSemantic,
	const glsl_type* InputType,
	exec_list* DeclInstructions,
	exec_list* PreCallInstructions)
{
	ir_variable* TempVariable = new(ParseState)ir_variable(
		InputType,
		NULL,
		ir_var_temporary);
	ir_dereference_variable* TempVariableDeref = new(ParseState) ir_dereference_variable(TempVariable);
	PreCallInstructions->push_tail(TempVariable);

	check(!InputType->is_inputpatch() && !InputType->is_outputpatch());
	ir_rvalue* SrcValue = MetalUtils::GenerateInputFromSemantic(Frequency, bIsDesktop, ParseState, InputSemantic, InputType, DeclInstructions, PreCallInstructions);
	if(SrcValue)
	{
		YYLTYPE loc ={0};
		apply_type_conversion(InputType,SrcValue,PreCallInstructions,ParseState,true,&loc);
		PreCallInstructions->push_tail(
			new(ParseState)ir_assignment(
			TempVariableDeref->clone(ParseState,NULL),
			SrcValue
			)
			);
	}

	return TempVariableDeref;
}


/**
* Generate an output semantic.
* @param Frequency - The shader frequency.
* @param ParseState - Parse state.
* @param Semantic - The semantic name to generate.
* @param Type - Value type.
* @param DeclInstructions - IR to which declarations may be added.
* @returns the IR variable for the semantic.
*/
static ir_rvalue* GenShaderOutputSemantic(
	EHlslShaderFrequency Frequency,
	_mesa_glsl_parse_state* ParseState,
	const char* Semantic,
	const glsl_type* Type,
	exec_list* DeclInstructions,
	const glsl_type** DestVariableType)
{
	check(Semantic);

	FSystemValue* SystemValues = SystemValueTable[Frequency];
	ir_variable* Variable = NULL;

	if (FCStringAnsi::Strnicmp(Semantic, "SV_", 3) == 0)
	{
		for (int i = 0; SystemValues[i].Semantic != NULL; ++i)
		{
			if (SystemValues[i].Mode == ir_var_out
				&& FCStringAnsi::Stricmp(SystemValues[i].Semantic, Semantic) == 0)
			{
				check(0);
			}
		}
	}

	if (Variable == NULL && Frequency == HSF_VertexShader)
	{
		const int PrefixLength = 15;
		if (FCStringAnsi::Strnicmp(Semantic, "SV_ClipDistance", PrefixLength) == 0
			&& Semantic[PrefixLength] >= '0'
			&& Semantic[PrefixLength] <= '9')
		{
			check(0);
		}
	}

	if (Variable == NULL && Frequency == HSF_PixelShader)
	{
		const int PrefixLength = 9;
		if (FCStringAnsi::Strnicmp(Semantic, "SV_Target", PrefixLength) == 0
			&& Semantic[PrefixLength] >= '0'
			&& Semantic[PrefixLength] <= '7')
		{
			int OutputIndex = Semantic[PrefixLength] - '0';
			Variable = new(ParseState)ir_variable(
				Type,
				ralloc_asprintf(ParseState, "out_Target%d", OutputIndex),
				ir_var_out
				);
		}
	}

	// @todo Dead function?
	check(0);
	if (Variable == NULL && Frequency == HSF_HullShader)
	{
		const int PrefixLength = 13;
		if (FCStringAnsi::Strnicmp(Semantic, "SV_TessFactor", PrefixLength) == 0
			&& Semantic[PrefixLength] >= '0'
			&& Semantic[PrefixLength] <= '3')
		{
			int OutputIndex = Semantic[PrefixLength] - '0';
			Variable = new(ParseState)ir_variable(
				Type,
				ralloc_asprintf(ParseState, "gl_TessLevelOuter[%d]", OutputIndex),
				ir_var_out
				);
		}
	}

	if (Variable == NULL && Frequency == HSF_HullShader)
	{
		const int PrefixLength = 19;
		if (FCStringAnsi::Strnicmp(Semantic, "SV_InsideTessFactor", PrefixLength) == 0
			&& Semantic[PrefixLength] >= '0'
			&& Semantic[PrefixLength] <= '1')
		{
			int OutputIndex = Semantic[PrefixLength] - '0';
			Variable = new(ParseState)ir_variable(
				Type,
				ralloc_asprintf(ParseState, "gl_TessLevelInner[%d]", OutputIndex),
				ir_var_out
				);
		}
		else if (FCStringAnsi::Stricmp(Semantic, "SV_InsideTessFactor") == 0)
		{
			Variable = new(ParseState)ir_variable(
				Type,
				ralloc_asprintf(ParseState, "gl_TessLevelInner[0]"),
				ir_var_out
				);
		}
	}

	if (Variable == NULL && ParseState->bGenerateES)
	{
		check(0);
		// Create a variable so that a struct will not get added
		Variable = new(ParseState)ir_variable(Type, ralloc_asprintf(ParseState, "var_%s", Semantic), ir_var_out);
	}

	if (Variable)
	{
		// Up to this point, variables aren't contained in structs
		*DestVariableType = Variable->type;
		DeclInstructions->push_tail(Variable);
		ParseState->symbols->add_variable(Variable);
		Variable->centroid = false;//OutputQualifier.Fields.bCentroid;
		Variable->interpolation = false;//OutputQualifier.Fields.InterpolationMode;
		Variable->is_patch_constant = false;// OutputQualifier.Fields.bIsPatchConstant;
		ir_rvalue* VariableDeref = new(ParseState)ir_dereference_variable(Variable);
		return VariableDeref;
	}

	if (Semantic && FCStringAnsi::Strnicmp(Semantic, "SV_", 3) == 0)
	{
		_mesa_glsl_warning(ParseState, "unrecognized system value output '%s'",
			Semantic);
	}

	*DestVariableType = Type;

	// Create variable
	glsl_struct_field *StructField = ralloc_array(ParseState, glsl_struct_field, 1);

	memset(StructField, 0, sizeof(glsl_struct_field));
	StructField[0].type = Type;
	StructField[0].name = ralloc_strdup(ParseState, "Data");

	const glsl_type* VariableType = glsl_type::get_record_instance(StructField, 1, ralloc_strdup(ParseState, Semantic));

	Variable = new(ParseState)ir_variable(VariableType, ralloc_asprintf(ParseState, "out_%s", Semantic), ir_var_out);
	Variable->centroid = false;//OutputQualifier.Fields.bCentroid;
	Variable->interpolation = false;//OutputQualifier.Fields.InterpolationMode;
	Variable->is_interface_block = true;
	Variable->is_patch_constant = false;//OutputQualifier.Fields.bIsPatchConstant;

	DeclInstructions->push_tail(Variable);
	ParseState->symbols->add_variable(Variable);

	ir_rvalue* VariableDeref = new(ParseState)ir_dereference_variable(Variable);

	if (Frequency == HSF_HullShader /*&& !OutputQualifier.Fields.bIsPatchConstant*/)
	{
		check(0); // @todo Still a dead function?
		_mesa_glsl_warning(ParseState, "Dead function called: %s:%d\n", __FILE__, __LINE__);
		//VariableDeref = new(ParseState)ir_dereference_array(VariableDeref, new(ParseState)ir_dereference_variable(ParseState->symbols->get_variable("SV_OutputControlPointID")));
	}

	VariableDeref = new(ParseState)ir_dereference_record(VariableDeref, ralloc_strdup(ParseState, "Data"));

	return VariableDeref;
}


/**
* Generate an output semantic.
* @param Frequency - The shader frequency.
* @param ParseState - Parse state.
* @param OutputSemantic - The semantic name to generate.
* @param OutputQualifier - Qualifiers applied to the semantic.
* @param OutputVariableDeref - Deref for the argument variable.
* @param DeclInstructions - IR to which declarations may be added.
* @param PostCallInstructions - IR to which instructions may be added after the
*                               entry point returns.
*/
void GenShaderOutputForVariable(
	EHlslShaderFrequency Frequency,
	_mesa_glsl_parse_state* ParseState,
	const char* OutputSemantic,
	ir_dereference* OutputVariableDeref,
	exec_list* DeclInstructions,
	exec_list* PostCallInstructions,
	int SemanticArraySize,
	int SemanticArrayIndex
	)
{
	const glsl_type* OutputType = OutputVariableDeref->type;
	if (OutputType->is_record())
	{
		check(0);
	}
	else if (OutputType->is_array())
	{
		check(0);
	}
	else
	{
		if (OutputSemantic)
		{
			YYLTYPE loc = {0};
			ir_rvalue* Src = OutputVariableDeref->clone(ParseState, NULL);
			const glsl_type* DestVariableType = NULL;
			ir_rvalue* DestVariableDeref = GenShaderOutputSemantic(Frequency, ParseState, OutputSemantic,
				OutputType, DeclInstructions, &DestVariableType);

			apply_type_conversion(DestVariableType, Src, PostCallInstructions, ParseState, true, &loc);
			PostCallInstructions->push_tail(new(ParseState)ir_assignment(DestVariableDeref, Src));
		}
		else
		{
			_mesa_glsl_error(ParseState, "entry point does not specify a semantic for its return value");
		}
	}
}


/**
* Generate an output semantic.
* @param Frequency - The shader frequency.
* @param ParseState - Parse state.
* @param OutputSemantic - The semantic name to generate.
* @param OutputQualifier - Qualifiers applied to the semantic.
* @param OutputType - Value type.
* @param DeclInstructions - IR to which declarations may be added.
* @param PreCallInstructions - IR to which isntructions may be added before the
entry point is called.
* @param PostCallInstructions - IR to which instructions may be added after the
*                               entry point returns.
* @returns the IR variable deref for the semantic.
*/
static ir_dereference_variable* GenerateShaderOutput(
	EHlslShaderFrequency Frequency,
	_mesa_glsl_parse_state* ParseState,
	const char* OutputSemantic,
	const glsl_type* OutputType,
	exec_list* DeclInstructions,
	exec_list* PreCallInstructions,
	exec_list* PostCallInstructions
	)
{
	// Generate a local variable to hold the output.
	ir_variable* TempVariable = new(ParseState)ir_variable(
		OutputType,
		NULL,
		ir_var_temporary);
	ir_dereference_variable* TempVariableDeref = new(ParseState)ir_dereference_variable(TempVariable);
	PreCallInstructions->push_tail(TempVariable);
	GenShaderOutputForVariable(
		Frequency,
		ParseState,
		OutputSemantic,
		TempVariableDeref,
		DeclInstructions,
		PostCallInstructions,
		0,
		0
		);
	return TempVariableDeref;
}

#define USE_DS_ATTRIBUTES 0 // @todo Unicorn: consider using this as it would be more elegant, but currently isn't complete.

void FMetalCodeBackend::PackInputsAndOutputs(exec_list* Instructions, _mesa_glsl_parse_state* ParseState, EHlslShaderFrequency Frequency, exec_list& InputVars)
{
	ir_function_signature* EntryPointSig = GetMainFunction(Instructions);
	check(EntryPointSig);

	exec_list DeclInstructions;
	exec_list PreCallInstructions;
	exec_list ArgInstructions;
	exec_list PostCallInstructions;
	ParseState->symbols->push_scope();

	// Set of variables packed into a struct
	std::set<ir_variable*> VSStageInVariables;
	std::set<ir_variable*> PSStageInVariables;
	std::set<ir_variable*> VSOutVariables;
	std::set<ir_variable*> PSOutVariables;

	// Return var/struct
	ir_variable* VSOut = nullptr;
	ir_variable* PSOut = nullptr;

	// Input stage variables
	ir_variable* VSStageIn = nullptr;
	std::map<std::string, glsl_struct_field> OriginalVSStageInMembers;
	ir_variable* PSStageIn = nullptr;

	// Extra arguments needed for input (VertexID, etc)
	TIRVarList VSInputArguments;
	TIRVarList PSInputArguments;
	TIRVarList CSInputArguments;

	std::set<ir_variable*> DSStageInVariables;
	std::set<ir_variable*> DSOutVariables;
	ir_variable* DSOut = nullptr;
	ir_variable* DSStageIn = nullptr;
	TIRVarList DSInputArguments;

#if USE_DS_ATTRIBUTES
#else // USE_DS_ATTRIBUTES
	std::set<ir_variable*> DSPatchVariables;
	ir_variable* DSPatch = nullptr;
#endif // USE_DS_ATTRIBUTES

	ir_variable* internalPatchIDVar = nullptr;

	if (Frequency == HSF_DomainShader)
	{
		// NOTE: possibly unused
		// create and call GET_INTERNAL_PATCH_ID
		{
			ir_function *Function_GET_INTERNAL_PATCH_ID = NULL;
			// create GET_INTERNAL_PATCH_ID
			{
				const glsl_type* retType = glsl_type::get_instance(GLSL_TYPE_UINT, 1, 1);
				ir_function_signature* sig = new(ParseState)ir_function_signature(retType);
				sig->is_builtin = true;
				Function_GET_INTERNAL_PATCH_ID = new(ParseState)ir_function("GET_INTERNAL_PATCH_ID");
				Function_GET_INTERNAL_PATCH_ID->add_signature(sig);
			}
			check(Function_GET_INTERNAL_PATCH_ID);

			exec_list VoidParameter;
			ir_function_signature * GetInternalPatchIDFunctionSig = Function_GET_INTERNAL_PATCH_ID->matching_signature(&VoidParameter);

			internalPatchIDVar = new(ParseState) ir_variable(glsl_type::get_instance(GLSL_TYPE_UINT, 1, 1), nullptr, ir_var_temporary);
			ir_dereference_variable* TempVariableDeref = new(ParseState) ir_dereference_variable(internalPatchIDVar);
			PreCallInstructions.push_tail(internalPatchIDVar);

			auto* Call = new(ParseState)ir_call(GetInternalPatchIDFunctionSig, TempVariableDeref, &VoidParameter);
			PreCallInstructions.push_tail(Call);
		}
	}

	if(bIsTessellationVSHS)
	{
		foreach_iter(exec_list_iterator, Iter, *Instructions)
		{
			ir_instruction* IR = (ir_instruction*)Iter.get();
			auto* Variable = IR->as_variable();
			if (Variable)
			{
				switch (Variable->mode)
				{
				case ir_var_out:
					{
						// do nothing here MovePackedUniformsToMain will move all these output arguments to the function signature
					}
					break;

				case ir_var_in:
#if USE_VS_HS_ATTRIBUTES
					{
						// do nothing here MovePackedUniformsToMain will move all these input arguments to the function signature
					}
#else // USE_VS_HS_ATTRIBUTES
					// there should be no input attributes
					check(0);
#endif // USE_VS_HS_ATTRIBUTES
					break;
				}
			}
		}

		// Fill up InputVars so "// @Inputs" will have stuff
		for (unsigned i = 0; i < ParseState->num_user_structures; i++)
		{
			const glsl_type *const s = ParseState->user_structures[i];

			if (strcmp(s->name, "InputVertexType") == 0)
			{
				for (unsigned j = 0; j < s->length; j++)
				{
					InputVars.push_tail(new(ParseState)extern_var(new(ParseState)ir_variable(s->fields.structure[j].type, ralloc_strdup(ParseState, s->fields.structure[j].name), ir_var_in)));
				}
			}
		}
	}
	else if (Frequency == HSF_VertexShader)
	{
		// Vertex Fetch to Vertex connector
		TArray<glsl_struct_field> VSStageInMembers;

		// Vertex Output connector. Gather position semantic & other outputs into a struct
		TArray<glsl_struct_field> VSOutMembers;

		foreach_iter(exec_list_iterator, Iter, *Instructions)
		{
			ir_instruction* IR = (ir_instruction*)Iter.get();
			auto* Variable = IR->as_variable();
			if (Variable)
			{
				switch (Variable->mode)
				{
				case ir_var_out:
					{
						glsl_struct_field Member;
						Member.type = Variable->type;
						Member.name = ralloc_strdup(ParseState, Variable->name);
						Member.semantic = ralloc_strdup(ParseState, Variable->semantic ? Variable->semantic : Variable->name);
						VSOutMembers.Add(Member);
						VSOutVariables.insert(Variable);
					}
					break;

				case ir_var_in:
					if (!ProcessStageInVariables(ParseState, bIsDesktop, Frequency, Variable, VSStageInMembers, VSStageInVariables, nullptr, VSInputArguments))
					{
						return;
					}
					break;
				}
			}
		}

		if (VSStageInMembers.Num())
		{
			check(Frequency == HSF_VertexShader);

			//@todo-rco: Make me nice...
			int AttributesUsedMask = 0;
			for (auto& Member : VSStageInMembers)
			{
				int Index = GetAttributeIndex(Member.semantic);
				if (Index >= 0 && Index < 16)
				{
					AttributesUsedMask |= (1 << Index);
				}
				InputVars.push_tail(new(ParseState)extern_var(new(ParseState)ir_variable(Member.type, ralloc_strdup(ParseState, Member.name), ir_var_in)));
			}

			if (bGenerateVSInputDummies)
			{
				int Bit = 0;
				for (int i = 0; i < 16; ++i)
				{
					if ((AttributesUsedMask & (1 << i)) == 0)
					{
						glsl_struct_field NewMember;
						NewMember.name = ralloc_asprintf(ParseState, "__dummy%d", i);
						NewMember.semantic = ralloc_asprintf(ParseState, "ATTRIBUTE%d", i);
						NewMember.type = glsl_type::get_instance(GLSL_TYPE_FLOAT, 4, 1);
						VSStageInMembers.Add(NewMember);
					}
				}
			}

			VSStageInMembers.Sort(
				[](glsl_struct_field const& A, glsl_struct_field const& B)
				{
					return GetAttributeIndex(A.semantic) < GetAttributeIndex(B.semantic);
				});

			// Convert all members to float4
			if (bExpandVSInputsToFloat4)
			{
				for (auto& Member : VSStageInMembers)
				{
					OriginalVSStageInMembers[Member.name] = Member;
					check(Member.type->matrix_columns == 1);
					Member.type = glsl_type::get_instance(Member.type->base_type, 4, 1);
				}
			}

			auto* Type = glsl_type::get_record_instance(&VSStageInMembers[0], (unsigned int)VSStageInMembers.Num(), "FVSStageIn");
			VSStageIn = new(ParseState)ir_variable(Type, "__VSStageIn", ir_var_in);
			// Hack: This way we tell we need to convert types from half to float
			((glsl_type*)Type)->HlslName = "__STAGE_IN__";
			ParseState->symbols->add_variable(VSStageIn);

			if (!ParseState->AddUserStruct(Type))
			{
				YYLTYPE loc = {0};
				_mesa_glsl_error(&loc, ParseState, "struct '%s' previously defined", Type->name);
			}
		}

		if (VSOutMembers.Num() && bIsTessellationVSHS)
		{
			check(VSOutMembers.Num() == 1);
			check(VSOutVariables.size() == 1);
			VSOut = *VSOutVariables.begin();
			VSOut->remove();
			VSOut->mode = ir_var_temporary;
			DeclInstructions.push_tail(VSOut);
		}
		else
		if (VSOutMembers.Num())
		{
			auto* Type = glsl_type::get_record_instance(&VSOutMembers[0], (unsigned int)VSOutMembers.Num(), "FVSOut");
			VSOut = new(ParseState)ir_variable(Type, "__VSOut", ir_var_temporary);
			PostCallInstructions.push_tail(VSOut);
			ParseState->symbols->add_variable(VSOut);

			if (!ParseState->AddUserStruct(Type))
			{
				YYLTYPE loc = {0};
				_mesa_glsl_error(&loc, ParseState, "struct '%s' previously defined", Type->name);
			}
		}
	}
	else if (Frequency == HSF_PixelShader)
	{
		// Vertex to Pixel connector
		TArray<glsl_struct_field> PSStageInMembers;

		// Pixel Output connector. Gather color & depth outputs into a struct
		TArray<glsl_struct_field> PSOutMembers;

		// Gather all inputs and generate the StageIn VS->PS connector
		foreach_iter(exec_list_iterator, Iter, *Instructions)
		{
			ir_instruction* IR = (ir_instruction*)Iter.get();
			auto* Variable = IR->as_variable();
			if (Variable)
			{
				switch (Variable->mode)
				{
				case ir_var_out:
					{
						glsl_struct_field Member;
						Member.type = Variable->type;
						Member.name = ralloc_strdup(ParseState, Variable->name);
						Member.semantic = ralloc_strdup(ParseState, Variable->semantic ? Variable->semantic : Variable->name);
						PSOutMembers.Add(Member);
						PSOutVariables.insert(Variable);
					}
					break;

				case ir_var_in:
					if (!ProcessStageInVariables(ParseState, bIsDesktop, Frequency, Variable, PSStageInMembers, PSStageInVariables, nullptr, PSInputArguments))
					{
						return;
					}
					break;

				default:
					break;
				}
			}
		}

		if (PSStageInMembers.Num())
		{
			auto* Type = glsl_type::get_record_instance(&PSStageInMembers[0], (unsigned int)PSStageInMembers.Num(), "FPSStageIn");
			// Hack: This way we tell we need to convert types from half to float
			((glsl_type*)Type)->HlslName = "__STAGE_IN__";
			PSStageIn = new(ParseState)ir_variable(Type, "__PSStageIn", ir_var_in);
			ParseState->symbols->add_variable(PSStageIn);

			if (!ParseState->AddUserStruct(Type))
			{
				YYLTYPE loc = {0};
				_mesa_glsl_error(&loc, ParseState, "struct '%s' previously defined", Type->name);
			}
		}

		if (PSOutMembers.Num())
		{
			auto* Type = glsl_type::get_record_instance(&PSOutMembers[0], (unsigned int)PSOutMembers.Num(), "FPSOut");
			PSOut = new(ParseState)ir_variable(Type, "__PSOut", ir_var_temporary);
			PostCallInstructions.push_tail(PSOut);
			ParseState->symbols->add_variable(PSOut);

			if (!ParseState->AddUserStruct(Type))
			{
				YYLTYPE loc = {0};
				_mesa_glsl_error(&loc, ParseState, "struct '%s' previously defined", Type->name);
			}
		}
	}
	else if (Frequency == HSF_ComputeShader)
	{
		YYLTYPE loc = {0};

		foreach_iter(exec_list_iterator, Iter, *Instructions)
		{
			ir_instruction* IR = (ir_instruction*)Iter.get();
			auto* Variable = IR->as_variable();
			if (Variable)
			{
				switch (Variable->mode)
				{
				case ir_var_out:
					{
						_mesa_glsl_error(&loc, ParseState, "Compute/Kernel shaders do not support out variables ('%s')!", Variable->name);
						return;
					}
					break;

				case ir_var_in:
					{
						TArray<glsl_struct_field> CSStageInMembers;
						TIRVarSet CSStageInVariables;
						if (!ProcessStageInVariables(ParseState, bIsDesktop, Frequency, Variable, CSStageInMembers, CSStageInVariables, nullptr, CSInputArguments))
						{
							return;
						}

						if (CSStageInMembers.Num() != 0 || CSStageInVariables.size() != 0)
						{
							_mesa_glsl_error(&loc, ParseState, "Compute/Kernel shaders do not support out stage_in variables or vertex attributes ('%s')!", Variable->name);
							return;
						}
					}
					break;

				case ir_var_shared:
					{
						// groupshared
						Variable->remove();
						DeclInstructions.push_head(Variable);
					}
					break;

				default:
					break;
				}
			}
		}
	}
	else if (Frequency == HSF_DomainShader)
	{
		// Vertex Fetch to Vertex connector
		TArray<glsl_struct_field> DSStageInMembers;
#if USE_DS_ATTRIBUTES
#else // USE_DS_ATTRIBUTES
		TArray<glsl_struct_field> DSPatchMembers;
#endif // USE_DS_ATTRIBUTES

		// Vertex Output connector. Gather position semantic & other outputs into a struct
		TArray<glsl_struct_field> DSOutMembers;

		foreach_iter(exec_list_iterator, Iter, *Instructions)
		{
			ir_instruction* IR = (ir_instruction*)Iter.get();
			auto* Variable = IR->as_variable();
			if (Variable)
			{
				switch (Variable->mode)
				{
				case ir_var_out:
					{
						glsl_struct_field Member;
						Member.type = Variable->type;
						Member.name = ralloc_strdup(ParseState, Variable->name);
						Member.semantic = ralloc_strdup(ParseState, Variable->semantic ? Variable->semantic : Variable->name);
						DSOutMembers.Add(Member);
						DSOutVariables.insert(Variable);
					}
					break;

				case ir_var_in:
#if USE_DS_ATTRIBUTES
#else // USE_DS_ATTRIBUTES
					if(Variable->type->is_patch())
					{
						glsl_struct_field Member;
						Member.type = Variable->type;
						Member.name = ralloc_strdup(ParseState, Variable->name);
						Member.semantic = ralloc_strdup(ParseState, Variable->semantic ? Variable->semantic : Variable->name);
						DSPatchMembers.Add(Member);
						DSPatchVariables.insert(Variable);
					}
					else
#endif // USE_DS_ATTRIBUTES
					if (!ProcessStageInVariables(ParseState, bIsDesktop, Frequency, Variable, DSStageInMembers, DSStageInVariables, nullptr, DSInputArguments))
					{
						return;
					}
					break;
				}
			}
		}

#if USE_DS_ATTRIBUTES
#error add attribute(0..N) semantics...
		if (DSStageInMembers.Num())
		{
			check(Frequency == HSF_DomainShader);

			//@todo-rco: Make me nice...
			for (auto& Member : DSStageInMembers)
			{
				InputVars.push_tail(new(ParseState)extern_var(new(ParseState)ir_variable(Member.type, ralloc_strdup(ParseState, Member.name), ir_var_in)));
			}

			DSStageInMembers.Sort(
				[](glsl_struct_field const& A, glsl_struct_field const& B)
				{
					return GetAttributeIndex(A.semantic) < GetAttributeIndex(B.semantic);
				});

			auto* Type = glsl_type::get_record_instance(&DSStageInMembers[0], (unsigned int)DSStageInMembers.Num(), "FDSStageIn");
			DSStageIn = new(ParseState)ir_variable(Type, "__DSStageIn", ir_var_in);
			// Hack: This way we tell we need to convert types from half to float
			((glsl_type*)Type)->HlslName = "__STAGE_IN__";
			ParseState->symbols->add_variable(DSStageIn);

			if (!ParseState->AddUserStruct(Type))
			{
				YYLTYPE loc = {0};
				_mesa_glsl_error(&loc, ParseState, "struct '%s' previously defined", Type->name);
			}
		}
#else // USE_DS_ATTRIBUTES

		// track attribute#s
		int onAttribute = 0;

		if (DSStageInMembers.Num())
		{
			check(Frequency == HSF_DomainShader);

			for (auto& Member : DSStageInMembers)
			{
				// NOTE: DS structs do not have to match...
				for(auto &Variable : DSStageInVariables)
				{
					if(strcmp(Member.name, Variable->name) == 0)
					{
						Variable->name = ralloc_asprintf(ParseState, "OUT_ATTRIBUTE%d_%s", onAttribute, Variable->name);
						break;
					}
				}
				Member.name = ralloc_asprintf(ParseState, "OUT_ATTRIBUTE%d_%s", onAttribute, Member.name);
				Member.semantic = ralloc_asprintf(ParseState, "[[ attribute(%d) ]]", onAttribute);
				onAttribute++;
			}

			auto Type = glsl_type::get_record_instance(&DSStageInMembers[0], (unsigned int)DSStageInMembers.Num(), "FDSStageIn");
			auto InType = glsl_type::get_array_instance(Type, 1000); // the size is meaningless
			DSStageIn = new(ParseState)ir_variable(InType, "__DSStageIn", ir_var_in);
			DSStageIn->semantic = ralloc_asprintf(ParseState, ""); // empty attribute for a buffer pointer means that it will be automatically choosen
			ParseState->symbols->add_variable(DSStageIn);
			ParseState->AddUserStruct(Type);
			Instructions->push_head(DSStageIn);
			//PatchConstOutSize = glslTypeSize(Type);

			// copy from DSStageIn
			for(auto &Variable : DSStageInVariables)
			{
				Variable->remove();
				Variable->mode = ir_var_temporary;
				DeclInstructions.push_tail(Variable);
				check(Variable->name);
				ir_dereference* DeRefArray = new(ParseState)ir_dereference_array(DSStageIn, new(ParseState)ir_constant((unsigned)0));
				ir_dereference* DeRefMember = new(ParseState)ir_dereference_record(DeRefArray, Variable->name);
				auto* Assign = new(ParseState)ir_assignment(new(ParseState)ir_dereference_variable(Variable), DeRefMember);
				PreCallInstructions.push_tail(Assign);
			}
		}

		if(DSPatchMembers.Num())
		{
			check(DSPatchMembers.Num() == 1);
			check(DSPatchMembers[0].type->is_patch());

			// generate...
			// MainDomainArg[0] = __DSPatch[0]
			// MainDomainArg[1] = __DSPatch[1]
			// MainDomainArg[2] = __DSPatch[2]
			{
				check(DSPatchVariables.size() == 1);
				auto Variable = *DSPatchVariables.begin();
				Variable->remove();
				Variable->mode = ir_var_temporary;
				DeclInstructions.push_tail(Variable);
				check(Variable->type->is_outputpatch());
				check(ParseState->tessellation.outputcontrolpoints == Variable->type->patch_length);
				const glsl_type* Type = NULL;
				const glsl_type* InType = NULL;
				int origOnAttribute = onAttribute; // should not have to do this...
				for (int OutputVertex = 0; OutputVertex < ParseState->tessellation.outputcontrolpoints; ++OutputVertex)
				{
					int InnerAttribute = origOnAttribute;
					exec_list MainDomainDeclInstructions;
					exec_list PreMainDomainTempDeclInstructions;
					exec_list PreMainDomainCallInstructions;

					FSemanticQualifier Qualifier;

					// @todo there has to be a better way to handle this vs looping over GenerateInput...
					auto deref = MetalUtils::GenerateInput(Frequency, bIsDesktop, ParseState, Variable->semantic, Variable->type->inner_type, &MainDomainDeclInstructions, &PreMainDomainCallInstructions);

					// make a flat perControlPoint struct
					ir_dereference_variable* OutputControlPointDeref = NULL;
					{
						std::set<ir_variable*> DSInVariables;

						TArray<glsl_struct_field> DSInMembers;

						foreach_iter(exec_list_iterator, Iter, MainDomainDeclInstructions)
						{
							ir_instruction* IR = (ir_instruction*)Iter.get();
							auto* InnerVariable = IR->as_variable();
							if (InnerVariable)
							{
								switch (InnerVariable->mode)
								{
								case ir_var_in:
									{
										check(!InnerVariable->type->is_array());
										glsl_struct_field Member;
										Member.type = InnerVariable->type;
										InnerVariable->name = ralloc_asprintf(ParseState, "OUT_ATTRIBUTE%d_%s", InnerAttribute, InnerVariable->name);
										Member.name = ralloc_strdup(ParseState, InnerVariable->name);
										Member.semantic = ralloc_asprintf(ParseState, "[[ attribute(%d) ]]", InnerAttribute);
										InnerAttribute++;
										DSInMembers.Add(Member);
										DSInVariables.insert(InnerVariable);
									}
									break;
								default:
									check(0);
								}
							}
						}

						if (DSInMembers.Num())
						{
							if(OutputVertex == 0)
							{
								Type = glsl_type::get_record_instance(&DSInMembers[0], (unsigned int)DSInMembers.Num(), "PatchControlPointOut");
								ParseState->AddUserStruct(Type);
								InType = glsl_type::get_array_instance(Type, 1000); // the size is meaningless
							}

							auto OutputControlPointVar = new(ParseState)ir_variable(Type, nullptr, ir_var_temporary);
							PreMainDomainTempDeclInstructions.push_tail(OutputControlPointVar);
							OutputControlPointDeref = new(ParseState)ir_dereference_variable(OutputControlPointVar);

							// copy to DSIn
							for(auto &InnerVariable : DSInVariables)
							{
								InnerVariable->remove();
								InnerVariable->mode = ir_var_temporary;
								PreMainDomainTempDeclInstructions.push_tail(InnerVariable);
								check(InnerVariable->name);
								ir_dereference* DeRefMember = new(ParseState)ir_dereference_record(OutputControlPointVar, InnerVariable->name);
								auto* Assign = new(ParseState)ir_assignment(new(ParseState)ir_dereference_variable(InnerVariable), DeRefMember);
								PreMainDomainCallInstructions.push_head(Assign);
							}
						}
					}

					if(OutputVertex == 0)
					{
						DSPatch = new(ParseState)ir_variable(InType, "__DSPatch", ir_var_in);
						DSPatch->semantic = ralloc_asprintf(ParseState, ""); // empty attribute for a buffer pointer means that it will be automatically choosen
						ParseState->symbols->add_variable(DSPatch);
						Instructions->push_head(DSPatch);
					}

					ir_dereference_array* DSPatchDeref = new(ParseState)ir_dereference_array(
						DSPatch,
						new(ParseState)ir_constant((unsigned)OutputVertex)
						);

					DeclInstructions.append_list(&MainDomainDeclInstructions); // should be empty
					PreCallInstructions.append_list(&PreMainDomainTempDeclInstructions);
					PreCallInstructions.push_tail(
						new (ParseState)ir_assignment(
							OutputControlPointDeref,
							DSPatchDeref
						)
						);
					PreCallInstructions.append_list(&PreMainDomainCallInstructions);
					PreCallInstructions.push_tail(
						new (ParseState)ir_assignment(
							new(ParseState)ir_dereference_array(
								Variable,
								new(ParseState)ir_constant((unsigned)OutputVertex)
							),
							deref
						)
						);
				}
			}
		}
#endif // USE_DS_ATTRIBUTES

		if (DSOutMembers.Num())
		{
			auto* DSOutType = glsl_type::get_record_instance(&DSOutMembers[0], (unsigned int)DSOutMembers.Num(), "FDSOut");
			DSOut = new(ParseState)ir_variable(DSOutType, "__DSOut", ir_var_temporary);
			PostCallInstructions.push_tail(DSOut);
			ParseState->symbols->add_variable(DSOut);

			if (!ParseState->AddUserStruct(DSOutType))
			{
				YYLTYPE loc = {0};
				_mesa_glsl_error(&loc, ParseState, "struct '%s' previously defined", DSOutType->name);
			}
		}
	}
	else
	{
		check(0);
	}

	TIRVarList VarsToMoveToBody;
	foreach_iter(exec_list_iterator, Iter, *Instructions)
	{
		ir_instruction* IR = (ir_instruction*)Iter.get();
		auto* Variable = IR->as_variable();
		if (Variable)
		{
			ir_dereference_variable* ArgVarDeref = NULL;
			switch (Variable->mode)
			{
			case ir_var_in:
				if (PSStageInVariables.find(Variable) != PSStageInVariables.end())
				{
					ir_dereference* DeRefMember = new(ParseState)ir_dereference_record(PSStageIn, Variable->name);
					auto* Assign = new(ParseState)ir_assignment(new(ParseState)ir_dereference_variable(Variable), DeRefMember);
					PreCallInstructions.push_tail(Assign);
					VarsToMoveToBody.push_back(Variable);
				}
				else if (VSStageInVariables.find(Variable) != VSStageInVariables.end())
				{
					ir_rvalue* DeRefMember = new(ParseState)ir_dereference_record(VSStageIn, Variable->name);
					unsigned int Mask = 0;
					if (bExpandVSInputsToFloat4)
					{
						Mask = (1 << 4) - 1;
						auto FoundMember = OriginalVSStageInMembers.find(Variable->name);
						check(FoundMember != OriginalVSStageInMembers.end());
						if (FoundMember->second.type)
						{
							check(FoundMember->second.type->vector_elements != 0);
							Mask = (1 << FoundMember->second.type->vector_elements) - 1;
							if (Mask != 15)
							{
								DeRefMember = new(ParseState)ir_swizzle(DeRefMember, 0, 1, 2, 3, FoundMember->second.type->vector_elements);
							}
						}
					}
					else
					{
						Mask = (1 << Variable->type->vector_elements) - 1;
					}
					auto* Assign = new(ParseState)ir_assignment(new(ParseState) ir_dereference_variable(Variable), DeRefMember, nullptr, Mask);
					PreCallInstructions.push_tail(Assign);
					VarsToMoveToBody.push_back(Variable);
				}
				else if(bIsTessellationVSHS)
				{
#if USE_VS_HS_ATTRIBUTES
					// see above...
					// do nothing here MovePackedUniformsToMain will move all these output arguments to the function signature
#else // USE_VS_HS_ATTRIBUTES
					// there should be no input attributes
					check(0);
#endif // USE_VS_HS_ATTRIBUTES
				}
#if USE_DS_ATTRIBUTES
				else if (DSStageInVariables.find(Variable) != DSStageInVariables.end())
				{
					//if(Variable->type->is_outputpatch()) // not exactly what I want...
					//{
					//	break; // skip for the outputpatch
					//}
					ir_dereference* DeRefMember = new(ParseState)ir_dereference_record(DSStageIn, Variable->name);
					if(DeRefMember->type->is_outputpatch())
					{
						const unsigned ElementCount = DeRefMember->type->patch_length;
						check(Variable->type->is_outputpatch());
						check(ElementCount == Variable->type->patch_length);
						for (unsigned i = 0; i < ElementCount; ++i)
						{
							ir_dereference_array* ArrayVariable = new(ParseState)ir_dereference_array(
								new(ParseState) ir_dereference_variable(Variable),
								new(ParseState) ir_constant((unsigned)i)
								);
							ir_dereference_array* ArrayDerefMember = new(ParseState)ir_dereference_array(
								DeRefMember->clone(ParseState, NULL),
								new(ParseState) ir_constant((unsigned)i)
								);
							auto* Assign = new(ParseState)ir_assignment(ArrayVariable, ArrayDerefMember);
							PreCallInstructions.push_tail(Assign);
						}
					}
					else
					{
						auto* Assign = new(ParseState)ir_assignment(new(ParseState)ir_dereference_variable(Variable), DeRefMember);
						PreCallInstructions.push_tail(Assign);
					}
					VarsToMoveToBody.push_back(Variable);
				}
#else // USE_DS_ATTRIBUTES
				else if (DSStageInVariables.find(Variable) != DSStageInVariables.end())
				{
					// TOOD could merge the code above down here...
				}
				else if (DSPatchVariables.find(Variable) != DSPatchVariables.end())
				{
					// TOOD could merge the code above down here...
				}
#endif // USE_DS_ATTRIBUTES
				else
				{
					// At this point this should be a built-in system value
					check(Variable->semantic);
					ArgVarDeref = GenerateShaderInput(
						Frequency, bIsDesktop,
						ParseState,
						Variable->semantic,
						Variable->type,
						&DeclInstructions,
						&PreCallInstructions
						);
				}
				break;

			case ir_var_out:
				if (VSOutVariables.find(Variable) != VSOutVariables.end())
				{
					VarsToMoveToBody.push_back(Variable);
					ir_dereference* DeRefMember = new(ParseState) ir_dereference_record(VSOut, Variable->name);
					auto* Assign = new(ParseState) ir_assignment(DeRefMember, new(ParseState)ir_dereference_variable(Variable));
					PostCallInstructions.push_tail(Assign);
				}
				else if (PSOutVariables.find(Variable) != PSOutVariables.end())
				{
					VarsToMoveToBody.push_back(Variable);
					ir_dereference* DeRefMember = new(ParseState) ir_dereference_record(PSOut, Variable->name);
					ir_rvalue* DeRefVar = new(ParseState) ir_dereference_variable(Variable);
					auto* Assign = new(ParseState) ir_assignment(DeRefMember, DeRefVar);
					PostCallInstructions.push_tail(Assign);
				}
				else if(bIsTessellationVSHS)
				{
					// see above...
					// do nothing here MovePackedUniformsToMain will move all these output arguments to the function signature
				}
				else if (DSOutVariables.find(Variable) != DSOutVariables.end())
				{
					VarsToMoveToBody.push_back(Variable);
					ir_dereference* DeRefMember = new(ParseState) ir_dereference_record(DSOut, Variable->name);
					auto* Assign = new(ParseState) ir_assignment(DeRefMember, new(ParseState)ir_dereference_variable(Variable));
					PostCallInstructions.push_tail(Assign);
				}
				else
				{
					ArgVarDeref = GenerateShaderOutput(
						Frequency,
						ParseState,
						Variable->semantic,
						Variable->type,
						&DeclInstructions,
						&PreCallInstructions,
						&PostCallInstructions
						);
				}
				break;

			default:
				break;
				/*
				_mesa_glsl_error(
				ParseState,
				"entry point parameter '%s' must be an input or output",
				Variable->name
				);
				*/
			}
			if (ArgVarDeref)
			{
				ArgInstructions.push_tail(ArgVarDeref);
			}
		}
		/*
		else
		{
		_mesa_glsl_error(ParseState, "entry point parameter "
		"'%s' does not specify a semantic", Variable->name);
		}
		*/
	}

	// The function's return value should have an output semantic if it's not void.
	ir_dereference_variable* EntryPointReturn = NULL;
	if (EntryPointSig->return_type->is_void() == false)
	{
		if (Frequency == HSF_PixelShader)
		{
			check(!PSOut);
			PSOut = new(ParseState)ir_variable(EntryPointSig->return_type, "__PSOut", ir_var_temporary);
			PreCallInstructions.push_tail(PSOut);
		}
		else if (Frequency == HSF_VertexShader)
		{
			check(!VSOut);
			VSOut = new(ParseState)ir_variable(EntryPointSig->return_type, "__VSOut", ir_var_temporary);
			PreCallInstructions.push_tail(VSOut);
		}
		else if(bIsTessellationVSHS)
		{
			check(0); // cannot get a return type here
		}
		else if (Frequency == HSF_DomainShader)
		{
			check(!DSOut);
			DSOut = new(ParseState)ir_variable(EntryPointSig->return_type, "__DSOut", ir_var_temporary);
			PreCallInstructions.push_tail(DSOut);
		}
		else
		{
			check(0);
		}
	}

	ParseState->symbols->pop_scope();

	// Build the void main() function for GLSL.
	auto* ReturnType = glsl_type::void_type;
	if (VSOut)
	{
		ReturnType = VSOut->type;
		check(!EntryPointReturn);
		EntryPointReturn = new(ParseState)ir_dereference_variable(VSOut);
		PostCallInstructions.push_tail(new(ParseState)ir_return(new(ParseState)ir_dereference_variable(VSOut)));
		EntryPointSig->return_type = ReturnType;
	}
	else if (PSOut)
	{
		ReturnType = PSOut->type;
		check(!EntryPointReturn);
		EntryPointReturn = new(ParseState)ir_dereference_variable(PSOut);
		PostCallInstructions.push_tail(new(ParseState)ir_return(new(ParseState)ir_dereference_variable(PSOut)));
		EntryPointSig->return_type = ReturnType;
	}
	else if (DSOut)
	{
		ReturnType = DSOut->type;
		check(!EntryPointReturn);
		EntryPointReturn = new(ParseState)ir_dereference_variable(DSOut);
		PostCallInstructions.push_tail(new(ParseState)ir_return(new(ParseState)ir_dereference_variable(DSOut)));
		EntryPointSig->return_type = ReturnType;
	}

	for (auto* Var : VarsToMoveToBody)
	{
		Var->remove();
		if (Var->mode == ir_var_in || Var->mode == ir_var_out)
		{
			Var->mode = ir_var_temporary;
		}
		DeclInstructions.push_head(Var);
	}

	DeclInstructions.append_list(&PreCallInstructions);
	DeclInstructions.append_list(&EntryPointSig->body);
	DeclInstructions.append_list(&PostCallInstructions);

	EntryPointSig->body.append_list(&DeclInstructions);
#if 0
	Instructions->push_tail(MainFunction);
#endif // 0

	// Now that we have a proper main(), move global setup to main().
	if (VSStageIn)
	{
		EntryPointSig->parameters.push_tail(VSStageIn);
	}
	else if (PSStageIn)
	{
		EntryPointSig->parameters.push_tail(PSStageIn);
	}

	/*
	MoveGlobalInstructionsToMain(Instructions);
	*/
}

struct FConvertHalfToFloatUniformAndSamples final : public ir_rvalue_visitor
{
	struct FPair
	{
		ir_rvalue** RValuePtr;
		ir_instruction* InsertPoint;
	};
	typedef std::map<ir_rvalue*, std::list<FPair>> TReplacedVarMap;
	TReplacedVarMap ReplacedVars;

	std::list<TReplacedVarMap> PendingReplacements;
	TIRVarSet ReferencedUniforms;

	_mesa_glsl_parse_state* State;

	bool bIsMaster;
	bool bConvertUniforms;
	bool bConvertSamples;

	FConvertHalfToFloatUniformAndSamples(_mesa_glsl_parse_state* InState, bool bInConvertUniforms, bool bInConvertSamples) :
		State(InState),
		bIsMaster(true),
		bConvertUniforms(bInConvertUniforms),
		bConvertSamples(bInConvertSamples)
	{
	}

	virtual ~FConvertHalfToFloatUniformAndSamples()
	{
	}

	void DoConvertOneMap(TReplacedVarMap& Map)
	{
		for (auto& TopIter : Map)
		{
			auto* RValue = TopIter.first;
			auto& List = TopIter.second;

			// Coerce this var into float
			auto* OriginalVar = RValue->variable_referenced();
			const auto* OriginalVarType = OriginalVar->type;
			const auto* PromotedVarType = PromoteHalfToFloatType(State, OriginalVarType);
			OriginalVar->type = PromotedVarType;

			// Temp var and assignment
			auto* NewVar = new(State)ir_variable(RValue->type, nullptr, ir_var_temporary);
			RValue->type = PromoteHalfToFloatType(State, RValue->type);
			exec_list NewAssignments;
			CreateNewAssignmentsFloat2Half(State, NewAssignments, NewVar, RValue);
			auto* BaseIR = List.front().InsertPoint;

			// Store new instructions so we add a nice block in the asm
			BaseIR->insert_before(NewVar);
			BaseIR->insert_before(&NewAssignments);

			for (auto& Iter : List)
			{
				*(Iter.RValuePtr) = new(State)ir_dereference_variable(NewVar);
			}
		}

		// Go through all remaining parameters
		for (auto& Var : ReferencedUniforms)
		{
			Var->type = PromoteHalfToFloatType(State, Var->type);
		}
	}

	void DoConvert(exec_list* ir)
	{
		run(ir);
		DoConvertOneMap(ReplacedVars);

		if (bIsMaster)
		{
			for (auto& Map : PendingReplacements)
			{
				DoConvertOneMap(Map);
			}
		}
	}

	void ConvertBlock(exec_list* instructions)
	{
		FConvertHalfToFloatUniformAndSamples Visitor(State, bConvertUniforms, bConvertSamples);
		Visitor.bIsMaster = false;
		Visitor.run(instructions);
		PendingReplacements.push_back(TReplacedVarMap());
		PendingReplacements.back().swap(Visitor.ReplacedVars);
		for (auto& Var : Visitor.ReferencedUniforms)
		{
			ReferencedUniforms.insert(Var);
		}
	}

	virtual ir_visitor_status visit_enter(ir_if* ir) override
	{
		ir->condition->accept(this);
		handle_rvalue(&ir->condition);

		ConvertBlock(&ir->then_instructions);
		ConvertBlock(&ir->else_instructions);

		/* already descended into the children. */
		return visit_continue_with_parent;
	}

	ir_visitor_status visit_enter(ir_loop* ir)
	{
		ConvertBlock(&ir->body_instructions);

		/* already descended into the children. */
		return visit_continue_with_parent;
	}

	virtual void handle_rvalue(ir_rvalue** RValuePtr) override
	{
		if (!RValuePtr || !*RValuePtr)
		{
			return;
		}

		auto* RValue = *RValuePtr;
		if (bConvertSamples && RValue->as_texture())
		{
			auto* Texture = RValue->as_texture();
			if (Texture->coordinate && Texture->coordinate->type->base_type == GLSL_TYPE_HALF)
			{
				// Promote to float
				Texture->coordinate = new(State)ir_expression(ir_unop_h2f, Texture->coordinate);
			}
			else if (Texture->coordinate && Texture->coordinate->type->base_type == GLSL_TYPE_INT)
			{
				// convert int to uint32
				Texture->coordinate = new(State)ir_expression(ir_unop_i2u, Texture->coordinate);
			}
		}
		// Skip swizzles, textures, etc
		else if (bConvertUniforms && RValue->as_dereference())
		{
			auto* Var = RValue->variable_referenced();
			if (Var && Var->mode == ir_var_uniform)
			{
				ReferencedUniforms.insert(Var);
				if (RValue->type->base_type == GLSL_TYPE_HALF)
				{
					// Save this RValue and prep for later change
					FPair Pair = {RValuePtr, base_ir};
					for (auto& Iter : ReplacedVars)
					{
						auto* TestRValue = Iter.first;
						if (RValue->ir_type == TestRValue->ir_type && AreEquivalent(RValue, TestRValue))
						{
							Iter.second.push_back(Pair);
							return;
						}
					}

					ReplacedVars[RValue].push_back(Pair);
				}
			}
		}
	}
};


void FMetalCodeBackend::ConvertHalfToFloatUniformsAndSamples(exec_list* ir, _mesa_glsl_parse_state* State, bool bConvertUniforms, bool bConvertSamples)
{
	if (bConvertUniforms || bConvertSamples)
	{
		FConvertHalfToFloatUniformAndSamples ConvertHalfToFloatUniforms(State, bConvertUniforms, bConvertSamples);
		ConvertHalfToFloatUniforms.DoConvert(ir);
	}
}

struct FMetalBreakPrecisionChangesVisitor final : public ir_rvalue_visitor
{
	_mesa_glsl_parse_state* State;

	std::map<ir_rvalue*, ir_variable*> ReplacedVars;

	FMetalBreakPrecisionChangesVisitor(_mesa_glsl_parse_state* InState) : State(InState) {}

	virtual ~FMetalBreakPrecisionChangesVisitor() { }

	void ConvertBlock(exec_list* instructions)
	{
		FMetalBreakPrecisionChangesVisitor Visitor(State);
		Visitor.run(instructions);
	}

	virtual ir_visitor_status visit_enter(ir_if* ir) override
	{
		ir->condition->accept(this);
		handle_rvalue(&ir->condition);

		ConvertBlock(&ir->then_instructions);
		ConvertBlock(&ir->else_instructions);

		/* already descended into the children. */
		return visit_continue_with_parent;
	}

	ir_visitor_status visit_enter(ir_loop* ir)
	{
		ConvertBlock(&ir->body_instructions);

		/* already descended into the children. */
		return visit_continue_with_parent;
	}

	virtual void handle_rvalue(ir_rvalue** RValuePtr) override
	{
		if (!RValuePtr || !*RValuePtr)
		{
			return;
		}
		bool bGenerateNewVar = false;
		auto* RValue = *RValuePtr;
		auto* Expression = RValue->as_expression();
		auto* Constant = RValue->as_constant();
		auto* DeRef = RValue->as_dereference();
		auto* DeRefImage = RValue->as_dereference_image();
		if (Expression)
		{
			switch (Expression->operation)
			{
			case ir_unop_h2f:
			case ir_unop_f2h:
				if (!Expression->operands[0]->as_texture())
				{
					bGenerateNewVar = true;
				}
				break;
			}
		}
		else if (Constant)
		{
			if (Constant->type->base_type == GLSL_TYPE_HALF)
			{
				bGenerateNewVar = true;
			}
		}
		else if (DeRefImage)
		{
			auto* Var = DeRef->variable_referenced();
			if (!in_assignee && Var && Var->type && Var->type->is_image())
			{
				// RW indices have to be unsigned
				if (DeRefImage->image_index->type && DeRefImage->image_index->type->base_type == GLSL_TYPE_INT)
				{
					auto* NewType = glsl_type::get_instance(GLSL_TYPE_UINT, DeRefImage->image_index->type->vector_elements, DeRefImage->image_index->type->matrix_columns);
					auto* NewExpression = new(State) ir_expression(ir_unop_i2u, NewType, DeRefImage->image_index);
					DeRefImage->image_index = NewExpression;
				}
			}
		}

		if (bGenerateNewVar)
		{
			for (auto& Iter : ReplacedVars)
			{
				auto* TestRValue = Iter.first;
				if (AreEquivalent(TestRValue, RValue))
				{
					*RValuePtr = new(State)ir_dereference_variable(Iter.second);
					return;
				}
			}

			auto* NewVar = new(State)ir_variable(RValue->type, nullptr, ir_var_temporary);
			auto* NewAssignment = new(State)ir_assignment(new(State)ir_dereference_variable(NewVar), RValue);
			*RValuePtr = new(State)ir_dereference_variable(NewVar);
			ReplacedVars[RValue] = NewVar;
			base_ir->insert_before(NewVar);
			base_ir->insert_before(NewAssignment);
		}
	}
};


void FMetalCodeBackend::BreakPrecisionChangesVisitor(exec_list* ir, _mesa_glsl_parse_state* State)
{
	FMetalBreakPrecisionChangesVisitor Visitor(State);
	Visitor.run(ir);
}

struct FDeReferencePackedVarsVisitor final : public ir_rvalue_visitor
{
	_mesa_glsl_parse_state* State;
	int ExpressionDepth;

	FDeReferencePackedVarsVisitor(_mesa_glsl_parse_state* InState)
		: State(InState)
		, ExpressionDepth(0)
	{
	}

	virtual ~FDeReferencePackedVarsVisitor()
	{
	}

	virtual ir_visitor_status visit_enter(ir_expression* ir) override
	{
		++ExpressionDepth;
		return ir_rvalue_visitor::visit_enter(ir);
	}

	virtual ir_visitor_status visit_leave(ir_expression* ir) override
	{
		for (uint32 i = 0; i < ir->get_num_operands(); ++i)
		{
			auto* Operand = ir->operands[i];
			auto* DerefRecord = Operand->as_dereference_record();
			if (DerefRecord)
			{
				handle_rvalue(&ir->operands[i]);
			}
		}

		--ExpressionDepth;
		return ir_rvalue_visitor::visit_leave(ir);
	}

	virtual void handle_rvalue(ir_rvalue** RValuePtr) override
	{
		if (!RValuePtr || !*RValuePtr)
		{
			return;
		}

		auto* DeRefRecord = (*RValuePtr)->as_dereference_record();
		auto* Swizzle = (*RValuePtr)->as_swizzle();
		auto* SwizzleValDeRefRecord = Swizzle ? Swizzle->val->as_dereference_record() : nullptr;
		if (SwizzleValDeRefRecord)
		{
			auto* StructVar = Swizzle->variable_referenced();
			if (StructVar->type->HlslName && !strcmp(StructVar->type->HlslName, "__PACKED__"))
			{
				if (SwizzleValDeRefRecord->type->vector_elements > 1 && SwizzleValDeRefRecord->type->vector_elements < 4)
				{
					auto* Var = GetVar(SwizzleValDeRefRecord);
					Swizzle->val = new(State)ir_dereference_variable(Var);
				}
			}
		}
		else if (DeRefRecord && ExpressionDepth > 0)
		{
			auto* StructVar = DeRefRecord->variable_referenced();
			if (StructVar->type->HlslName && !strcmp(StructVar->type->HlslName, "__PACKED__"))
			{
				if (DeRefRecord->type->vector_elements > 1 && DeRefRecord->type->vector_elements < 4)
				{
					auto* Var = GetVar(DeRefRecord);
					*RValuePtr = new(State)ir_dereference_variable(Var);
				}
			}
		}
	}

	std::map<ir_dereference_record*, ir_variable*> Replaced;

	ir_variable* GetVar(ir_dereference_record* ir)
	{
		ir_variable* Var = nullptr;
		for (auto& Pair : Replaced)
		{
			if (Pair.first->IsEquivalent(ir))
			{
				Var = Pair.second;
				break;
			}
		}

		if (!Var)
		{
			Var = new(State)ir_variable(ir->type, nullptr, ir_var_temporary);
			Replaced[ir] = Var;
		}
		return Var;
	}
};

void FMetalCodeBackend::RemovePackedVarReferences(exec_list* ir, _mesa_glsl_parse_state* State)
{
	FDeReferencePackedVarsVisitor Visitor(State);
	Visitor.run(ir);

	if (Visitor.Replaced.empty())
	{
		return;
	}

	ir_function_signature* Main = GetMainFunction(ir);
	for (auto& OuterPair : Visitor.Replaced)
	{
		auto* NewVar = OuterPair.second;
		auto* DeRefRecord = OuterPair.first;
		auto* NewAssignment = new(State)ir_assignment(new(State)ir_dereference_variable(NewVar), DeRefRecord);
		Main->body.push_head(NewAssignment);
		Main->body.push_head(NewVar);
	}
}
