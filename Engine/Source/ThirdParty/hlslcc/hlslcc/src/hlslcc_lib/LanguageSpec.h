// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

// Determines what intrinsics will the compiler accept based off target language (e.g., ES 2 doesn't have transpose)
struct ILanguageSpec
{
	virtual ~ILanguageSpec() {}

	// If Supports* return false, the Language has to implement them on SetupLanguageIntrinsics()
	virtual bool SupportsDeterminantIntrinsic() const = 0;
	virtual bool SupportsTransposeIntrinsic() const = 0;
	virtual bool SupportsIntegerModulo() const = 0;
	
	// Whether the backend can generate a fused-multiply-add instruction
	virtual bool SupportsFusedMultiplyAdd() const { return false; }

	// half3x3 <-> float3x3
	virtual bool SupportsMatrixConversions() const = 0;
	virtual void SetupLanguageIntrinsics(_mesa_glsl_parse_state* State, exec_list* ir) = 0;

	// If true, a sampler can be used with multiple textures
	virtual bool AllowsSharingSamplers() const = 0;

	// Some platforms don't allow implicit math/conversion between float & half types
	virtual bool CanConvertBetweenHalfAndFloat() const { return true; }

	// Some platforms have/need atomic_load/atomic_store intrinsics
	virtual bool NeedsAtomicLoadStore() const { return false; }

	// Experimental!
	virtual bool UseSamplerInnerType() const { return false; }
	
	// Some platforms require input variable structs to be split, others require that they aren't
	virtual bool SplitInputVariableStructs() const { return true; }
	
	// Whether the backend can generate a saturate instruction
	virtual bool SupportsSaturateIntrinsic() const { return false; }
	
	// Whether the backend can generate a sincos instruction
    virtual bool SupportsSinCosIntrinsic() const { return false; }
    
    // Whether the backend can generate correct native matrix intrinsics (given HLSL row-major matrices)
    virtual bool SupportsMatrixIntrinsics() const { return false; }
    
    // Whether the backend allows reads from non-scalar UAVs
    virtual bool AllowsImageLoadsForNonScalar() const { return true; }
};

enum
{
	// The intrinsic is valid for unsigned vector types.
	IR_INTRINSIC_UINT = 0x0001,
	// The intrinsic is valid for integer vector types.
	IR_INTRINSIC_INT = 0x0002,
	// The intrinsic is valid for float vector types.
	IR_INTRINSIC_HALF = 0x0004,
	// The intrinsic is valid for float vector types.
	IR_INTRINSIC_FLOAT = 0x0008,
	// The intrinsic is valid for float vector types.
	IR_INTRINSIC_ALL_FLOATING = IR_INTRINSIC_FLOAT | IR_INTRINSIC_HALF,
	// The intrinsic is valid for boolean vector types.
	IR_INTRINSIC_BOOL = 0x0010,
	// The intrinsic is valid for unsigned vector types but is a noop.
	IR_INTRINSIC_UINT_THRU = 0x0020 | IR_INTRINSIC_UINT,
	// The intrinsic is valid for integer vector types but is a noop.
	IR_INTRINSIC_INT_THRU = 0x0040 | IR_INTRINSIC_INT,
	// The intrinsic is valid for float vector types but is a noop.
	IR_INTRINSIC_FLOAT_THRU = 0x0080 | IR_INTRINSIC_ALL_FLOATING,
	// The intrinsic is valid for boolean vector types but is a noop.
	IR_INTRINSIC_BOOL_THRU = 0x0100 | IR_INTRINSIC_BOOL,
	// The return type of the intrinsic is a scalar.
	IR_INTRINSIC_SCALAR = 0x0200,
	// The intrinsic should accept matrix parameters.
	IR_INTRINSIC_MATRIX = 0x0400,
	// The intrinsic returns a boolean vector. Pass thru types result in false.
	IR_INTRINSIC_RETURNS_BOOL = 0x0800,
	// The intrinsic returns a boolean vector. Pass thru types result in true.
	IR_INTRINSIC_RETURNS_BOOL_TRUE = 0x1000,
	// The intrinsic returns void.
	IR_INTRINSIC_RETURNS_VOID = 0x2000,
	// The intrinsic takes an int or uint and promotes the arg to float, and returns float (eg sqrt(2))
	IR_INTRINSIC_PROMOTE_ARG_FLOAT_RETURN_FLOAT = 0x4000,
};

extern void make_intrinsic_genType(
	exec_list *ir, _mesa_glsl_parse_state *state, const char *name, int op,
	unsigned flags, unsigned num_args,
	unsigned min_vec = 1, unsigned max_vec = 4);
