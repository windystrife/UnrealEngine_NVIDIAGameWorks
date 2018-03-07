// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Math/TransformCalculus.h"
#include "Math/TransformCalculus2D.h"

/**
 * Represents a 2D transformation in the following order: scale then translate.
 * Used by FGeometry for it's layout transformations.
 *
 * Matrix form looks like:
 *   [Vx Vy 1] * [ S   0   0 ]
 *               [ 0   S   0 ]
 *               [ Tx  Ty  1 ]
 *
 */
class FSlateLayoutTransform
{
public:
	/** Ctor from a scale followed by translate. Shortcut to Concatenate(InScale, InTranslation). */
	explicit FSlateLayoutTransform(float InScale = 1.0f, const FVector2D& InTranslation = FVector2D(ForceInit))
		:Scale(InScale)
		,Translation(InTranslation)
	{
	}

	/** Ctor from a 2D translation followed by a scale. Shortcut to Concatenate(InTranslation, InScale). While this is the opposite order we internally store them, we can represent this correctly. */
	explicit FSlateLayoutTransform(const FVector2D& InTranslation/*, float InScale = 1.0f*/)
		:Scale(1.0f/*InScale*/)
		,Translation(InTranslation/**InScale*/)
	{
	}

	/** Access to the 2D translation */
	const FVector2D& GetTranslation() const
	{
		return Translation;
	}

	/** Access to the scale. */
	float GetScale() const
	{
		return Scale;
	}

	/** Support for converting to an FMatrix. */
	FMatrix ToMatrix() const
	{
		FMatrix Matrix = FScaleMatrix(GetScale());
		Matrix.SetOrigin(FVector(GetTranslation(), 0.0f));
		return Matrix;
	}

	/** 2D transform support. */
	FVector2D TransformPoint(const FVector2D& Point) const
	{
		return ::TransformPoint(Translation, ::TransformPoint(Scale, Point));
	}

	/** 2D transform support. */
	FVector2D TransformVector(const FVector2D& Vector) const
	{
		return ::TransformVector(Translation, ::TransformVector(Scale, Vector));
	}

	/**
	 * This works by transforming the origin through LHS then RHS.
	 * In matrix form, looks like this:
	 * [ Sa  0   0 ]   [ Sb  0   0 ]
	 * [ 0   Sa  0 ] * [ 0   Sb  0 ] 
	 * [ Tax Tay 1 ]   [ Tbx Tby 1 ]
	 */
	FSlateLayoutTransform Concatenate(const FSlateLayoutTransform& RHS) const
	{
		// New Translation is essentially: RHS.TransformPoint(TransformPoint(FVector2D::ZeroVector))
		// Since Zero through LHS -> Translation we optimize it slightly to skip the zero multiplies.
		return FSlateLayoutTransform(::Concatenate(Scale, RHS.Scale), RHS.TransformPoint(Translation));
	}

	/** Invert the transform/scale. */
	FSlateLayoutTransform Inverse() const
	{
		return FSlateLayoutTransform(::Inverse(Scale), ::Inverse(Translation) * ::Inverse(Scale));
	}

	/** Equality. */
	bool operator==(const FSlateLayoutTransform& Other) const
	{
		return Scale == Other.Scale && Translation == Other.Translation;
	}
	
	/** Inequality. */
	bool operator!=(const FSlateLayoutTransform& Other) const
	{
		return !operator==(Other);
	}

private:
	float Scale;
	FVector2D Translation;
};

/** Specialization for concatenating a uniform scale and 2D Translation. */
inline FSlateLayoutTransform Concatenate(float Scale, const FVector2D& Translation)
{
	return FSlateLayoutTransform(Scale, Translation);
}

/** Specialization for concatenating a 2D Translation and uniform scale. */
inline FSlateLayoutTransform Concatenate(const FVector2D& Translation, float Scale)
{
	return FSlateLayoutTransform(Scale, TransformPoint(Scale, Translation));
}

/** concatenation rules for LayoutTransform. */
template<> struct ConcatenateRules<FSlateLayoutTransform, float                > { typedef FSlateLayoutTransform ResultType; };
/** concatenation rules for LayoutTransform. */
template<> struct ConcatenateRules<float                , FSlateLayoutTransform> { typedef FSlateLayoutTransform ResultType; };
/** concatenation rules for LayoutTransform. */
template<> struct ConcatenateRules<FSlateLayoutTransform, FVector2D            > { typedef FSlateLayoutTransform ResultType; };
/** concatenation rules for LayoutTransform. */
template<> struct ConcatenateRules<FVector2D            , FSlateLayoutTransform> { typedef FSlateLayoutTransform ResultType; };

/** concatenation rules for LayoutTransform. */
template<> struct ConcatenateRules<FSlateLayoutTransform, FMatrix              > { typedef FMatrix ResultType; };
/** concatenation rules for LayoutTransform. */
template<> struct ConcatenateRules<FMatrix              , FSlateLayoutTransform> { typedef FMatrix ResultType; };

/** concatenation rules for layout transforms and 2x2 generalized transforms. Need to be upcast to FTransform2D. */
template<> struct ConcatenateRules<FScale2D             , FSlateLayoutTransform> { typedef FTransform2D ResultType; };
template<> struct ConcatenateRules<FShear2D             , FSlateLayoutTransform> { typedef FTransform2D ResultType; };
template<> struct ConcatenateRules<FQuat2D              , FSlateLayoutTransform> { typedef FTransform2D ResultType; };
template<> struct ConcatenateRules<FMatrix2x2           , FSlateLayoutTransform> { typedef FTransform2D ResultType; };
template<> struct ConcatenateRules<FSlateLayoutTransform, FScale2D             > { typedef FTransform2D ResultType; };
template<> struct ConcatenateRules<FSlateLayoutTransform, FShear2D             > { typedef FTransform2D ResultType; };
template<> struct ConcatenateRules<FSlateLayoutTransform, FQuat2D              > { typedef FTransform2D ResultType; };
template<> struct ConcatenateRules<FSlateLayoutTransform, FMatrix2x2           > { typedef FTransform2D ResultType; };

//////////////////////////////////////////////////////////////////////////
// FSlateLayoutTransform adapters.
// 
// Adapt FTransform2D to accept FSlateLayoutTransforms as well.
//////////////////////////////////////////////////////////////////////////
template<> template<> inline FTransform2D TransformConverter<FTransform2D>::Convert<FSlateLayoutTransform>(const FSlateLayoutTransform& Transform)
{
	return FTransform2D(FScale2D(Transform.GetScale()), Transform.GetTranslation());
}

