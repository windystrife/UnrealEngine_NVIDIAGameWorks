// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

// This code is modified from that in the Mesa3D Graphics library available at
// http://mesa3d.org/
// The license for the original code follows:

/*
* Copyright © 2009 Intel Corporation
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
#include "glsl_symbol_table.h"
#include "glsl_parser_extras.h"
#include "glsl_types.h"
#include "builtin_types.h"
#include "hash_table.h"

static const char * const sampler_type_prefix[] = { "u", "i", "", "" };

hash_table *glsl_type::sampler_types = NULL;
hash_table* glsl_type::StructuredBufferTypes = nullptr;
hash_table *glsl_type::outputstream_types = NULL;
hash_table *glsl_type::inputpatch_types = NULL;
hash_table *glsl_type::outputpatch_types = NULL;
hash_table *glsl_type::image_types = NULL;
hash_table *glsl_type::array_types = NULL;
hash_table *glsl_type::record_types = NULL;
void *glsl_type::mem_ctx = NULL;
void* glsl_type::BaseTypesContext = nullptr;

void glsl_type::init_ralloc_type_ctx(void)
{
	if (BaseTypesContext == nullptr)
	{
		BaseTypesContext = ralloc_autofree_context();
		check(BaseTypesContext);
	}
}

glsl_type::glsl_type(glsl_base_type base_type, unsigned vector_elements,
	unsigned matrix_columns, const char *name, const char* InHlslName) :
	base_type(base_type),
	sampler_dimensionality(0), sampler_shadow(0), sampler_array(0),
	sampler_ms(false), sampler_buffer(false), outputstream_type(GLSL_OUTPUTSTREAM_NONE),
	sample_count(1), inner_type(0), vector_elements(vector_elements),
	matrix_columns(matrix_columns), length(0), patch_length(0)
{
	init_ralloc_type_ctx();
	this->name = ralloc_strdup(BaseTypesContext, name);
	this->HlslName = ralloc_strdup(BaseTypesContext, InHlslName);
	/* Neither dimension is zero or both dimensions are zero.
	*/
	check((vector_elements == 0) == (matrix_columns == 0));
	memset(&fields, 0, sizeof(fields));
}

glsl_type::glsl_type(
enum glsl_sampler_dim dim, bool shadow, bool array,
	bool multisample, int samples, bool is_sampler_buffer,
	const struct glsl_type* type, const char *name, const char* InHlslName) :
	base_type(GLSL_TYPE_SAMPLER),
	sampler_dimensionality(dim), sampler_shadow(shadow),
	sampler_array(array), sampler_ms(multisample), sampler_buffer(is_sampler_buffer),
	outputstream_type(GLSL_OUTPUTSTREAM_NONE), sample_count(samples),
	inner_type(type), vector_elements(0), matrix_columns(0),
	length(0), patch_length(0)
{
	init_ralloc_type_ctx();
	this->name = ralloc_strdup(this->mem_ctx, name);
	this->HlslName = ralloc_strdup(this->mem_ctx, InHlslName);
	memset(&fields, 0, sizeof(fields));
}

glsl_type::glsl_type(enum glsl_outputstream_type output_stream_type,
	const struct glsl_type* type, const char *name) :
	base_type(GLSL_TYPE_OUTPUTSTREAM),
	sampler_dimensionality(0), sampler_shadow(0), sampler_array(0),
	sampler_ms(false), sampler_buffer(false), outputstream_type(output_stream_type),
	sample_count(1), inner_type(type), vector_elements(0), matrix_columns(0), HlslName(nullptr), length(0), patch_length(0)
{
	init_ralloc_type_ctx();
	this->name = ralloc_strdup(this->mem_ctx, name);
	memset(&fields, 0, sizeof(fields));
}

glsl_type::glsl_type(enum glsl_base_type patch_type,
	unsigned patch_length, const struct glsl_type* type, const char *name) :
	base_type(patch_type),
	sampler_dimensionality(0), sampler_shadow(0), sampler_array(0),
	sampler_ms(false), sampler_buffer(false), outputstream_type(GLSL_OUTPUTSTREAM_NONE),
	sample_count(1), inner_type(type), vector_elements(0), matrix_columns(0), HlslName(nullptr), length(0), patch_length(patch_length)
{
	init_ralloc_type_ctx();
	this->name = ralloc_strdup(this->mem_ctx, name);
	memset(&fields, 0, sizeof(fields));
}


glsl_type::glsl_type(
	enum glsl_sampler_dim dim, bool array, bool is_sampler_buffer,
	const struct glsl_type* type, const char *name) :
	base_type(GLSL_TYPE_IMAGE),
	sampler_dimensionality(dim), sampler_shadow(0),
	sampler_array(array), sampler_ms(false), sampler_buffer(is_sampler_buffer),
	outputstream_type(GLSL_OUTPUTSTREAM_NONE), sample_count(1),
	inner_type(type), vector_elements(0), matrix_columns(0), HlslName(nullptr),
	length(0), patch_length(0)
{
	init_ralloc_type_ctx();
	this->name = ralloc_strdup(this->mem_ctx, name);
	memset(&fields, 0, sizeof(fields));
}

glsl_type::glsl_type(const glsl_struct_field *fields, unsigned num_fields,
	const char *name) :
	base_type(GLSL_TYPE_STRUCT),
	sampler_dimensionality(0), sampler_shadow(0), sampler_array(0),
	sampler_ms(false), sampler_buffer(false), outputstream_type(GLSL_OUTPUTSTREAM_NONE),
	sample_count(1), inner_type(0), vector_elements(0),
	matrix_columns(0), HlslName(nullptr), length(num_fields), patch_length(0)
{
	unsigned int i;

	init_ralloc_type_ctx();
	this->name = ralloc_strdup(this->mem_ctx, name);
	this->fields.structure = ralloc_array(this->mem_ctx,
		glsl_struct_field, length);
	for (i = 0; i < length; i++)
	{
		this->fields.structure[i].type = fields[i].type;
		this->fields.structure[i].name = ralloc_strdup(this->mem_ctx, fields[i].name);
		this->fields.structure[i].semantic = ralloc_strdup(this->mem_ctx, fields[i].semantic);
		this->fields.structure[i].centroid = fields[i].centroid;
		this->fields.structure[i].interpolation = fields[i].interpolation;
		this->fields.structure[i].geometryinput = fields[i].geometryinput;
		this->fields.structure[i].patchconstant = fields[i].patchconstant;
	}
}

void glsl_type::add_structure_member(const glsl_struct_field* field)
{
	check(this->is_record());

	unsigned int i;

	glsl_struct_field* new_fields = ralloc_array(this->mem_ctx, glsl_struct_field, this->length + 1);
	for (i = 0; i < this->length; i++)
	{
		new_fields[i].type = this->fields.structure[i].type;
		new_fields[i].name = this->fields.structure[i].name;
		new_fields[i].semantic = this->fields.structure[i].semantic;
		new_fields[i].centroid = this->fields.structure[i].centroid;
		new_fields[i].interpolation = this->fields.structure[i].interpolation;
		new_fields[i].geometryinput = this->fields.structure[i].geometryinput;
		new_fields[i].patchconstant = this->fields.structure[i].patchconstant;

	}

	new_fields[i].type = field->type;
	new_fields[i].name = ralloc_strdup(new_fields, field->name);
	new_fields[i].semantic = ralloc_strdup(new_fields, field->semantic);
	new_fields[i].centroid = field->centroid;
	new_fields[i].interpolation = field->interpolation;
	new_fields[i].geometryinput = field->geometryinput;
	new_fields[i].patchconstant = field->patchconstant;

	this->fields.structure = new_fields;
	++this->length;
}

void glsl_type::replace_structure_member(int memberIndex, const glsl_struct_field* new_field)
{
	this->fields.structure[memberIndex].type = new_field->type;
	this->fields.structure[memberIndex].name = ralloc_strdup(this->fields.structure, new_field->name);
	this->fields.structure[memberIndex].semantic = ralloc_strdup(this->fields.structure, new_field->semantic);
	this->fields.structure[memberIndex].centroid = new_field->centroid;
	this->fields.structure[memberIndex].interpolation = new_field->interpolation;
	this->fields.structure[memberIndex].geometryinput = new_field->geometryinput;
	this->fields.structure[memberIndex].patchconstant = new_field->patchconstant;
}

static void add_types_to_symbol_table(glsl_symbol_table *symtab,
	const struct glsl_type *types, unsigned num_types, bool warn)
{
	(void)warn;

	for (unsigned i = 0; i < num_types; i++)
	{
		symtab->add_type(types[i].name, &types[i]);
	}
}

bool glsl_type::contains_sampler() const
{
	if (this->is_array())
	{
		return this->fields.array->contains_sampler();
	}
	else if (this->is_record())
	{
		for (unsigned int i = 0; i < this->length; i++)
		{
			if (this->fields.structure[i].type->contains_sampler())
				return true;
		}
		return false;
	}
	else
	{
		return this->is_sampler();
	}
}

void glsl_type::generate_100ES_types(glsl_symbol_table *symtab)
{
	add_types_to_symbol_table(symtab, builtin_core_types,
		Elements(builtin_core_types),
		false);
	add_types_to_symbol_table(symtab, builtin_structure_types,
		Elements(builtin_structure_types),
		false);
	add_types_to_symbol_table(symtab, void_type, 1, false);
}

void glsl_type::generate_110_types(glsl_symbol_table *symtab, bool add_deprecated)
{
	generate_100ES_types(symtab);

	//   add_types_to_symbol_table(symtab, builtin_110_types,
	//			     Elements(builtin_110_types),
	//			     false);
	//   add_types_to_symbol_table(symtab, &_sampler3D_type, 1, false);
	if (add_deprecated)
	{
		add_types_to_symbol_table(symtab, builtin_110_deprecated_structure_types,
			Elements(builtin_110_deprecated_structure_types),
			false);
	}
}


void glsl_type::generate_120_types(glsl_symbol_table *symtab, bool add_deprecated)
{
	generate_110_types(symtab, add_deprecated);

	add_types_to_symbol_table(symtab, builtin_120_types,
		Elements(builtin_120_types), false);
}


void glsl_type::generate_130_types(glsl_symbol_table *symtab, bool add_deprecated)
{
	generate_120_types(symtab, add_deprecated);

	add_types_to_symbol_table(symtab, builtin_130_types,
		Elements(builtin_130_types), false);
}


void glsl_type::generate_140_types(glsl_symbol_table *symtab)
{
	generate_130_types(symtab, false);
}


void _mesa_glsl_initialize_types(struct _mesa_glsl_parse_state *state)
{
	switch (state->language_version)
	{
	case 100:
		glsl_type::generate_100ES_types(state->symbols);
		break;
	case 110:
		glsl_type::generate_110_types(state->symbols, true);
		break;
	case 120:
		glsl_type::generate_120_types(state->symbols, true);
		break;
	case 130:
		glsl_type::generate_130_types(state->symbols, true);
		break;
	case 140:
		glsl_type::generate_140_types(state->symbols);
		break;
	case 150:
		glsl_type::generate_140_types(state->symbols);
		break;
	case 310:
	case 430:
		glsl_type::generate_140_types(state->symbols);
		break;
	default:
		/* error */
		break;
	}
}


const glsl_type *glsl_type::get_base_type() const
{
	switch (base_type)
	{
	case GLSL_TYPE_UINT:
		return uint_type;
	case GLSL_TYPE_INT:
		return int_type;
	case GLSL_TYPE_HALF:
		return half_type;
	case GLSL_TYPE_FLOAT:
		return float_type;
	case GLSL_TYPE_BOOL:
		return bool_type;
	default:
		return error_type;
	}
}


const glsl_type *glsl_type::get_scalar_type() const
{
	const glsl_type *type = this;

	/* Handle arrays */
	while (type->base_type == GLSL_TYPE_ARRAY)
	{
		type = type->fields.array;
	}

	/* Handle vectors and matrices */
	switch (type->base_type)
	{
	case GLSL_TYPE_UINT:
		return uint_type;
	case GLSL_TYPE_INT:
		return int_type;
	case GLSL_TYPE_HALF:
		return half_type;
	case GLSL_TYPE_FLOAT:
		return float_type;
	default:
		/* Handle everything else */
		return type;
	}
}


void
_mesa_glsl_release_types(void)
{
	if (glsl_type::sampler_types != NULL)
	{
		hash_table_dtor_FreeData(glsl_type::sampler_types);
		glsl_type::sampler_types = NULL;
	}

	if (glsl_type::StructuredBufferTypes != NULL)
	{
		hash_table_dtor_FreeData(glsl_type::StructuredBufferTypes);
		glsl_type::StructuredBufferTypes = nullptr;
	}

	if (glsl_type::outputstream_types != NULL)
	{
		hash_table_dtor_FreeData(glsl_type::outputstream_types);
		glsl_type::outputstream_types = NULL;
	}

	if (glsl_type::inputpatch_types != NULL)
	{
		hash_table_dtor_FreeData(glsl_type::inputpatch_types);
		glsl_type::inputpatch_types = NULL;
	}

	if (glsl_type::outputpatch_types != NULL)
	{
		hash_table_dtor_FreeData(glsl_type::outputpatch_types);
		glsl_type::outputpatch_types = NULL;
	}


	if (glsl_type::image_types != NULL)
	{
		hash_table_dtor_FreeData(glsl_type::image_types);
		glsl_type::image_types = NULL;
	}

	if (glsl_type::array_types != NULL)
	{
		hash_table_dtor_FreeData(glsl_type::array_types);
		glsl_type::array_types = NULL;
	}

	if (glsl_type::record_types != NULL)
	{
		hash_table_dtor_FreeData(glsl_type::record_types);
		glsl_type::record_types = NULL;
	}
}


glsl_type::glsl_type(const glsl_type *array, unsigned length) :
base_type(GLSL_TYPE_ARRAY),
sampler_dimensionality(0), sampler_shadow(0), sampler_array(0),
inner_type(0),
vector_elements(0), matrix_columns(0),
name(NULL), length(length), patch_length(0)
{
	this->fields.array = array;

	/* Allow a maximum of 10 characters for the array size.  This is enough
	* for 32-bits of ~0.  The extra 3 are for the '[', ']', and terminating
	* NUL.
	*/
	const size_t name_length = strlen(array->name) + 10 + 3;
	char *const n = (char *)ralloc_size(this->mem_ctx, name_length);

	if (length == 0)
		snprintf(n, name_length, "%s[]", array->name);
	else
		snprintf(n, name_length, "%s[%u]", array->name, length);

	this->name = n;
}


const glsl_type *
glsl_type::get_instance(unsigned base_type, unsigned rows, unsigned columns)
{
	if (base_type == GLSL_TYPE_VOID)
	{
		return void_type;
	}

	if ((rows < 1) || (rows > 4) || (columns < 1) || (columns > 4))
	{
		return error_type;
	}

	/* Treat GLSL vectors as Nx1 matrices.
	*/
	if (columns == 1)
	{
		switch (base_type)
		{
		case GLSL_TYPE_UINT:
			return uint_type + (rows - 1);
		case GLSL_TYPE_INT:
			return int_type + (rows - 1);
		case GLSL_TYPE_HALF:
			return half_type + (rows - 1);
		case GLSL_TYPE_FLOAT:
			return float_type + (rows - 1);
		case GLSL_TYPE_BOOL:
			return bool_type + (rows - 1);
		default:
			return error_type;
		}
	}
	else
	{
		if ((base_type != GLSL_TYPE_FLOAT && base_type != GLSL_TYPE_HALF) || (rows == 1))
		{
			return error_type;
		}

		/* GLSL matrix types are named mat{COLUMNS}x{ROWS}.  Only the following
		* combinations are valid:
		*
		*   1 2 3 4
		* 1
		* 2   x x x
		* 3   x x x
		* 4   x x x
		*/
#define IDX(c,r) (((c-1)*3) + (r-1))

		if (base_type == GLSL_TYPE_FLOAT)
		{
			switch (IDX(columns, rows))
			{
			case IDX(2, 2): return mat2_type;
			case IDX(2, 3): return mat2x3_type;
			case IDX(2, 4): return mat2x4_type;
			case IDX(3, 2): return mat3x2_type;
			case IDX(3, 3): return mat3_type;
			case IDX(3, 4): return mat3x4_type;
			case IDX(4, 2): return mat4x2_type;
			case IDX(4, 3): return mat4x3_type;
			case IDX(4, 4): return mat4_type;
			default: return error_type;
			}
		}
		else
		{
			switch (IDX(columns, rows))
			{
			case IDX(2, 2): return half2x2_type;
			case IDX(2, 3): return half2x3_type;
			case IDX(2, 4): return half2x4_type;
			case IDX(3, 2): return half3x2_type;
			case IDX(3, 3): return half3x3_type;
			case IDX(3, 4): return half3x4_type;
			case IDX(4, 2): return half4x2_type;
			case IDX(4, 3): return half4x3_type;
			case IDX(4, 4): return half4x4_type;
			default: return error_type;
			}
		}
#undef IDX
	}

	check(!"Should not get here.");
	return error_type;
}

const glsl_type * glsl_type::get_templated_instance(const glsl_type *base, const char *name, int num_samples, int patch_size)
{
	if (sampler_types == NULL)
	{
		sampler_types = hash_table_ctor(64, hash_table_string_hash,
			hash_table_string_compare);

		// Base sampler types.
		hash_table_insert(sampler_types, new glsl_type(GLSL_SAMPLER_DIM_1D,   /*shadow=*/ false, /*array=*/ false, /*multisample=*/ false, /*samples=*/ 0, /*sampler_buffer=*/ true,  /*type=*/ NULL, "samplerBuffer", "sampler"), "Buffer");
		hash_table_insert(sampler_types, new glsl_type(GLSL_SAMPLER_DIM_1D,   /*shadow=*/ false, /*array=*/ false, /*multisample=*/ false, /*samples=*/ 0, /*sampler_buffer=*/ false, /*type=*/ NULL, "sampler1D", "texture1d"), "Texture1D");
		hash_table_insert(sampler_types, new glsl_type(GLSL_SAMPLER_DIM_1D,   /*shadow=*/ false, /*array=*/ true,  /*multisample=*/ false, /*samples=*/ 0, /*sampler_buffer=*/ false, /*type=*/ NULL, "sampler1DArray", nullptr), "Texture1DArray");
		hash_table_insert(sampler_types, new glsl_type(GLSL_SAMPLER_DIM_2D,   /*shadow=*/ false, /*array=*/ false, /*multisample=*/ false, /*samples=*/ 0, /*sampler_buffer=*/ false, /*type=*/ NULL, "sampler2D", "texture2d"), "Texture2D");
		hash_table_insert(sampler_types, new glsl_type(GLSL_SAMPLER_DIM_2D,   /*shadow=*/ false, /*array=*/ false, /*multisample=*/ false, /*samples=*/ 0, /*sampler_buffer=*/ false, /*type=*/ NULL, "samplerExternalOES", "texture2d"), "TextureExternal");
		hash_table_insert(sampler_types, new glsl_type(GLSL_SAMPLER_DIM_2D,   /*shadow=*/ false, /*array=*/ true,  /*multisample=*/ false, /*samples=*/ 0, /*sampler_buffer=*/ false, /*type=*/ NULL, "sampler2DArray", nullptr), "Texture2DArray");
		hash_table_insert(sampler_types, new glsl_type(GLSL_SAMPLER_DIM_2D,   /*shadow=*/ false, /*array=*/ false, /*multisample=*/ true,  /*samples=*/ 0, /*sampler_buffer=*/ false, /*type=*/ NULL, "sampler2DMS", nullptr), "Texture2DMS");
		hash_table_insert(sampler_types, new glsl_type(GLSL_SAMPLER_DIM_2D,   /*shadow=*/ false, /*array=*/ true,  /*multisample=*/ true,  /*samples=*/ 0, /*sampler_buffer=*/ false, /*type=*/ NULL, "sampler2DMSArray", nullptr), "Texture2DMSArray");
		hash_table_insert(sampler_types, new glsl_type(GLSL_SAMPLER_DIM_3D,   /*shadow=*/ false, /*array=*/ false, /*multisample=*/ false, /*samples=*/ 0, /*sampler_buffer=*/ false, /*type=*/ NULL, "sampler3D", "texture3d"), "Texture3D");
		hash_table_insert(sampler_types, new glsl_type(GLSL_SAMPLER_DIM_CUBE, /*shadow=*/ false, /*array=*/ false, /*multisample=*/ false, /*samples=*/ 0, /*sampler_buffer=*/ false, /*type=*/ NULL, "samplerCube", "texturecube"), "TextureCube");
		hash_table_insert(sampler_types, new glsl_type(GLSL_SAMPLER_DIM_CUBE, /*shadow=*/ false, /*array=*/ true,  /*multisample=*/ false, /*samples=*/ 0, /*sampler_buffer=*/ false, /*type=*/ NULL, "samplerCubeArray", nullptr), "TextureCubeArray");
	}

	if (outputstream_types == NULL)
	{
		outputstream_types = hash_table_ctor(64, hash_table_string_hash,
			hash_table_string_compare);

		// Base outputstream types.
		hash_table_insert(outputstream_types, new glsl_type(GLSL_OUTPUTSTREAM_POINTS, /*type=*/ NULL, "point_stream"), "PointStream");
		hash_table_insert(outputstream_types, new glsl_type(GLSL_OUTPUTSTREAM_LINES, /*type=*/ NULL, "line_stream"), "LineStream");
		hash_table_insert(outputstream_types, new glsl_type(GLSL_OUTPUTSTREAM_TRIANGLES, /*type=*/ NULL, "triangle_stream"), "TriangleStream");
	}

	if (inputpatch_types == NULL)
	{
		inputpatch_types = hash_table_ctor(64, hash_table_string_hash,
			hash_table_string_compare);

		// Base input patch types.
		hash_table_insert(inputpatch_types, new glsl_type(GLSL_TYPE_INPUTPATCH, 0, (glsl_type*)NULL, "input_patch"), "InputPatch");
	}

	if (outputpatch_types == NULL)
	{
		outputpatch_types = hash_table_ctor(64, hash_table_string_hash,
			hash_table_string_compare);

		// Base output patch types.
		hash_table_insert(outputpatch_types, new glsl_type(GLSL_TYPE_OUTPUTPATCH, 0, (glsl_type*)NULL, "output_patch"), "OutputPatch");
	}

	if (image_types == NULL)
	{
		image_types = hash_table_ctor(64, hash_table_string_hash, hash_table_string_compare);

		// Base sampler types.
		hash_table_insert(image_types, new glsl_type(GLSL_SAMPLER_DIM_1D, /*array=*/ false, /*sampler_buffer=*/ true,  /*type=*/ NULL, "imageBuffer"), "RWBuffer");
		hash_table_insert(image_types, new glsl_type(GLSL_SAMPLER_DIM_1D, /*array=*/ false, /*sampler_buffer=*/ false, /*type=*/ NULL, "image1D"), "RWTexture1D");
		hash_table_insert(image_types, new glsl_type(GLSL_SAMPLER_DIM_1D, /*array=*/ true,  /*sampler_buffer=*/ false, /*type=*/ NULL, "image1DArray"), "RWTexture1DArray");
		hash_table_insert(image_types, new glsl_type(GLSL_SAMPLER_DIM_2D, /*array=*/ false, /*sampler_buffer=*/ false, /*type=*/ NULL, "image2D"), "RWTexture2D");
		hash_table_insert(image_types, new glsl_type(GLSL_SAMPLER_DIM_2D, /*array=*/ true,  /*sampler_buffer=*/ false, /*type=*/ NULL, "image2DArray"), "RWTexture2DArray");
		hash_table_insert(image_types, new glsl_type(GLSL_SAMPLER_DIM_3D, /*array=*/ false, /*sampler_buffer=*/ false, /*type=*/ NULL, "image3D"), "RWTexture3D");
		hash_table_insert(image_types, new glsl_type(GLSL_SAMPLER_DIM_BUF, /*array=*/ false, /*sampler_buffer=*/ true, /*type=*/ NULL, "StructuredBuffer"), "RWStructuredBuffer");
		hash_table_insert(image_types, new glsl_type(GLSL_SAMPLER_DIM_BUF, /*array=*/ false, /*sampler_buffer=*/ true, /*type=*/ NULL, "ByteAddressBuffer"), "RWByteAddressBuffer");
	}

	if (base == NULL)
	{
		return NULL;
	}

	const glsl_type* outputstream_base_type = (const glsl_type*)hash_table_find(outputstream_types, name);

	if (outputstream_base_type != NULL)
	{
		/** Generate a key that is the combination of outputstream type and inner type. */
		char key[128];
		snprintf(key, sizeof(key), "%s<%s>", outputstream_base_type->name, base->name);

		const glsl_type *actual_type = (glsl_type *)hash_table_find(outputstream_types, key);
		if (actual_type == NULL)
		{
			actual_type = new glsl_type(
				(glsl_outputstream_type)(outputstream_base_type->outputstream_type),
				base,
				key);

			hash_table_insert(outputstream_types, (void *)actual_type, ralloc_strdup(mem_ctx, key));
		}

		return actual_type;
	}



	const glsl_type* inputpatch_base_type = (const glsl_type*)hash_table_find(inputpatch_types, name);
	if (inputpatch_base_type != NULL)
	{
		/** Generate a key that is the combination of input patch  type and inner type. */
		char key[128];

		snprintf(key, sizeof(key), "%s<%s>", inputpatch_base_type->name, base->name);

		const glsl_type *actual_type = (glsl_type *)hash_table_find(inputpatch_types, key);
		if (actual_type == NULL)
		{
			actual_type = new glsl_type(GLSL_TYPE_INPUTPATCH, patch_size, base, key);
			hash_table_insert(inputpatch_types, (void *)actual_type, ralloc_strdup(mem_ctx, key));
		}

		return actual_type;
	}


	const glsl_type* outputpatch_base_type = (const glsl_type*)hash_table_find(outputpatch_types, name);
	if (outputpatch_base_type != NULL)
	{
		/** Generate a key that is the combination of input patch  type and inner type. */
		char key[128];
		snprintf(key, sizeof(key), "%s<%s>", outputpatch_base_type->name, base->name);

		const glsl_type *actual_type = (glsl_type *)hash_table_find(outputpatch_types, key);
		if (actual_type == NULL)
		{
			actual_type = new glsl_type(GLSL_TYPE_OUTPUTPATCH, patch_size, base, key);
			hash_table_insert(outputpatch_types, (void *)actual_type, ralloc_strdup(mem_ctx, key));
		}

		return actual_type;
	}

	if (!base->is_numeric())
	{
		// Hack!
		//#todo-rco: Proper support!
		//if (strcmp(name, "RWStructuredBuffer") && strcmp(name, "RWByteAddressBuffer"))
		{
			return nullptr;
		}
	}

	const glsl_type* image_base_type = (const glsl_type*)hash_table_find(image_types, name);

	if (image_base_type != NULL)
	{
		/** Generate a key that is the combination of sampler type and inner type. */
		char key[128];
		snprintf(key, sizeof(key), "%s<%s>", image_base_type->name, base->name);

		const glsl_type *actual_type = (glsl_type *)hash_table_find(image_types, key);
		if (actual_type == NULL)
		{
			actual_type = new glsl_type(
				(glsl_sampler_dim)(image_base_type->sampler_dimensionality),
				image_base_type->sampler_array,
				image_base_type->sampler_buffer,
				base,
				ralloc_asprintf(mem_ctx, "%s%s", sampler_type_prefix[base->base_type], image_base_type->name));

			hash_table_insert(image_types, (void *)actual_type, ralloc_strdup(mem_ctx, key));
		}
		return actual_type;
	}

	const glsl_type* sampler_base_type = (const glsl_type*)hash_table_find(sampler_types, name);

	if (sampler_base_type == NULL)
	{
		return NULL;
	}

	/** Generate a key that is the combination of sampler type and inner type. */
	char key[128];
	if (num_samples>1)
	{
		snprintf(key, sizeof(key), "%s<%s,%d>", sampler_base_type->name, base->name, num_samples);
	}
	else
	{
		snprintf(key, sizeof(key), "%s<%s>", sampler_base_type->name, base->name);
	}

	const glsl_type *actual_type = (glsl_type *)hash_table_find(sampler_types, key);
	if (actual_type == NULL)
	{
		actual_type = new glsl_type(
			(glsl_sampler_dim)(sampler_base_type->sampler_dimensionality),
			sampler_base_type->sampler_shadow,
			sampler_base_type->sampler_array,
			sampler_base_type->sampler_ms,
			num_samples,
			sampler_base_type->sampler_buffer,
			base,
			ralloc_asprintf(mem_ctx, "%s%s", sampler_type_prefix[base->base_type], sampler_base_type->name),
			sampler_base_type->HlslName);

		hash_table_insert(sampler_types, (void *)actual_type, ralloc_strdup(mem_ctx, key));
	}

	return actual_type;
}

const glsl_type* glsl_type::GetStructuredBufferInstance(const char* TypeName, const glsl_type* InnerType)
{
	/** Generate a key that is the combination of outputstream type and inner type. */
	char Key[128];
	snprintf(Key, sizeof(Key), "%s<%s>", TypeName, InnerType->name);

	if (!StructuredBufferTypes)
	{
		StructuredBufferTypes = hash_table_ctor(64, hash_table_string_hash, hash_table_string_compare);
	}

	const glsl_type* FoundType = (glsl_type *)hash_table_find(StructuredBufferTypes, Key);
	if (!FoundType)
	{
		FoundType = new glsl_type(glsl_base_type::GLSL_TYPE_IMAGE, 1, 1, ralloc_strdup(mem_ctx, Key), ralloc_strdup(mem_ctx, Key));
		((glsl_type*)FoundType)->inner_type = InnerType;
		((glsl_type*)FoundType)->sampler_buffer = 1;

		hash_table_insert(StructuredBufferTypes, (void*)FoundType, ralloc_strdup(mem_ctx, Key));
	}

	return FoundType;
}


const glsl_type * glsl_type::get_shadow_sampler_type() const
{
	const glsl_type* shadow_type = NULL;

	if (base_type == GLSL_TYPE_SAMPLER && sampler_types)
	{
		/** Generate a key that is the combination of sampler type and inner type. */
		char key[128];
		snprintf(key, sizeof(key), "%sShadow", name);
		shadow_type = (const glsl_type*)hash_table_find(sampler_types, key);
		if (shadow_type == NULL)
		{
			glsl_type* new_shadow_type = new glsl_type(*this);
			new_shadow_type->sampler_shadow = true;
			new_shadow_type->name = ralloc_asprintf(mem_ctx, "%sShadow", name);
			new_shadow_type->inner_type = glsl_type::uint_type;
			shadow_type = new_shadow_type;
		}
	}

	return shadow_type;
}

const glsl_type * glsl_type::get_array_instance(const glsl_type *base, unsigned array_size)
{

	if (array_types == NULL)
	{
		array_types = hash_table_ctor(64, hash_table_string_hash,
			hash_table_string_compare);
	}

	/* Generate a name using the base type pointer in the key.  This is
	* done because the name of the base type may not be unique across
	* shaders.  For example, two shaders may have different record types
	* named 'foo'.
	*/
	char key[128];
	snprintf(key, sizeof(key), "%p[%u]", (void *)base, array_size);

	const glsl_type *t = (glsl_type *)hash_table_find(array_types, key);
	if (t == NULL)
	{
		t = new glsl_type(base, array_size);

		hash_table_insert(array_types, (void *)t, ralloc_strdup(mem_ctx, key));
	}

	check(t->base_type == GLSL_TYPE_ARRAY);
	check(t->length == array_size);
	check(t->fields.array == base);

	return t;
}


int glsl_type::record_key_compare(const void *a, const void *b)
{
	const glsl_type *const key1 = (glsl_type *)a;
	const glsl_type *const key2 = (glsl_type *)b;

	/* Return zero is the types match (there is zero difference) or non-zero
	* otherwise.
	*/
	if (strcmp(key1->name, key2->name) != 0)
		return 1;

	if (key1->length != key2->length)
		return 1;

	for (unsigned i = 0; i < key1->length; i++)
	{
		if (key1->fields.structure[i].type != key2->fields.structure[i].type)
			return 1;
		if (strcmp(key1->fields.structure[i].name,
			key2->fields.structure[i].name) != 0)
			return 1;
	}

	return 0;
}


unsigned glsl_type::record_key_hash(const void *a)
{
	const glsl_type *const key = (glsl_type *)a;
	char hash_key[128];
	unsigned size = 0;

	size = snprintf(hash_key, sizeof(hash_key), "%08x", key->length);

	for (unsigned i = 0; i < key->length; i++)
	{
		if (size >= sizeof(hash_key))
		{
			break;
		}

		size += snprintf(&hash_key[size], sizeof(hash_key)-size,
			"%p", (void *)key->fields.structure[i].type);
	}

	return hash_table_string_hash(&hash_key);
}


const glsl_type* glsl_type::get_record_instance(const glsl_struct_field *fields,
	unsigned num_fields, const char *name)
{
	const glsl_type key(fields, num_fields, name);

	if (record_types == NULL)
	{
		record_types = hash_table_ctor(64, record_key_hash, record_key_compare);
	}

	const glsl_type *t = (glsl_type *)hash_table_find(record_types, &key);
	if (t == NULL)
	{
		t = new glsl_type(fields, num_fields, name);

		hash_table_insert(record_types, (void *)t, t);
	}

	check(t->base_type == GLSL_TYPE_STRUCT);
	check(t->length == num_fields);
	check(strcmp(t->name, name) == 0);

	return t;
}


const glsl_type * glsl_type::field_type(const char *name) const
{
	if (this->base_type != GLSL_TYPE_STRUCT)
	{
		return error_type;
	}

	for (unsigned i = 0; i < this->length; i++)
	{
		if (strcmp(name, this->fields.structure[i].name) == 0)
		{
			return this->fields.structure[i].type;
		}
	}

	return error_type;
}


int glsl_type::field_index(const char *name) const
{
	if (this->base_type != GLSL_TYPE_STRUCT)
		return -1;

	for (unsigned i = 0; i < this->length; i++)
	{
		if (strcmp(name, this->fields.structure[i].name) == 0)
		{
			return i;
		}
	}

	return -1;
}


unsigned glsl_type::component_slots() const
{
	switch (this->base_type)
	{
	case GLSL_TYPE_UINT:
	case GLSL_TYPE_INT:
	case GLSL_TYPE_HALF:
	case GLSL_TYPE_FLOAT:
	case GLSL_TYPE_BOOL:
		return this->components();

	case GLSL_TYPE_STRUCT:
	{
		unsigned size = 0;
		for (unsigned i = 0; i < this->length; i++)
		{
			size += this->fields.structure[i].type->component_slots();
		}
		return size;
	}

	case GLSL_TYPE_ARRAY:
		return this->length * this->fields.array->component_slots();

	case GLSL_TYPE_SAMPLER_STATE:
	case GLSL_TYPE_SAMPLER:
		return 1;

	default:
		return 0;
	}
}

bool glsl_type::can_implicitly_convert_to(const glsl_type *desired) const
{
	// Trivial.
	if (this == desired)
	{
		return true;
	}

	// No implicit conversions for structures.
	if (this->is_record() || desired->is_record())
	{
		return false;
	}

	// Nor arrays.
	if (this->is_array() || desired->is_array())
	{
		return false;
	}

	// Implicit conversions can drop information.
	if (this->vector_elements >= desired->vector_elements &&
		this->matrix_columns >= desired->matrix_columns)
	{
		return true;
	}

	// Scalars can always be implicitly converted.
	if (this->is_scalar() || desired->is_scalar())
	{
		return true;
	}

	return false;
}
