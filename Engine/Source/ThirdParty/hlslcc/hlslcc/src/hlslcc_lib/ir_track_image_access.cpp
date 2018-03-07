// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

// ir_track_image_access.cpp: Utility functions

#include "ShaderCompilerCommon.h"
#include "hlslcc.h"
#include "compiler.h"
#include "glsl_parser_extras.h"
#include "hash_table.h"
#include "LanguageSpec.h"


/**
 * IR visitor used to track how image* variables are accessed.
 * when complete, all image vars are labeled with read/write 
 */
class ir_track_image_access_visitor : public ir_hierarchical_visitor
{
public:

	ir_track_image_access_visitor(_mesa_glsl_parse_state* InParseState) :
		ParseState(InParseState)
	{

	}

	/**
	 * Only need to hook image dereference, as it is
	 * the only thing with relevant data
	 */
	virtual ir_visitor_status visit_enter(class ir_dereference_image *image)
	{
		ir_variable* var = image->variable_referenced(); 

		if (var)
		{
			if (this->in_assignee)
			{
				var->image_write = 1;
			}
			else
			{
				const glsl_type* t = var->type->inner_type;
				if (!ParseState->LanguageSpec->AllowsImageLoadsForNonScalar() && !(t->is_scalar() && t->is_numeric() && t->base_type != GLSL_TYPE_HALF))
				{
					_mesa_glsl_error(ParseState, "loads from image/UAV '%s' are only allowed for 32-bit scalar components", var->name ? var->name: "");
				}
				var->image_read = 1;
			}
		}
		return visit_continue;
	}


	_mesa_glsl_parse_state* ParseState;
};

void TrackImageAccess(struct exec_list* ir, _mesa_glsl_parse_state* InParseState)
{
	ir_track_image_access_visitor v(InParseState);

	v.run(ir);
}
