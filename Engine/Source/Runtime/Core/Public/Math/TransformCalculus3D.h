// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Math/Vector.h"
#include "Math/Rotator.h"
#include "Math/Matrix.h"
#include "Math/RotationMatrix.h"
#include "Math/TranslationMatrix.h"
#include "Math/Quat.h"
#include "Math/ScaleMatrix.h"
#include "Math/TransformCalculus.h"


//////////////////////////////////////////////////////////////////////////
// Transform calculus for 3D types. Since UE4 already has existing 3D transform
// types, this is mostly a set of adapter overloads for the primitive operations
// requires by the transform calculus framework.
//
// The following types are adapted.
// * float           -> represents a uniform scale.
// * FScale          -> represents a 3D non-uniform scale.
// * FVector         -> represents a 3D translation.
// * FRotator        -> represents a pure rotation.
// * FQuat           -> represents a pure rotation.
// * FMatrix         -> represents a general 3D homogeneous transform.
//
//////////////////////////////////////////////////////////////////////////

/**
 * Represents a 3D non-uniform scale (to disambiguate from an FVector, which is used for translation).
 * 
 * Serves as a good base example of how to write a class that supports the basic transform calculus
 * operations.
 */
class FScale
{
public:
	/** Ctor. initialize to an identity scale, 1.0. */
	FScale() :Scale(1.0f) {}
	/** Ctor. initialize from a uniform scale. */
	explicit FScale(float InScale) :Scale(InScale) {}
	/** Ctor. initialize from an FVector defining the 3D scale. */
	explicit FScale(const FVector& InScale) :Scale(InScale) {}
	/** Access to the underlying FVector that stores the scale. */
	const FVector& GetVector() const { return Scale; }
	/** Concatenate two scales. */
	const FScale Concatenate(const FScale& RHS) const
	{
		return FScale(Scale * RHS.GetVector());
	}
	/** Invert the scale. */
	const FScale Inverse() const
	{
		return FScale(FVector(1.0f / Scale.X, 1.0f / Scale.Y, 1.0f / Scale.Z));
	}
private:
	/** Underlying storage of the 3D scale. */
	FVector Scale;
};

/** Specialization for converting a FMatrix to an FRotator. It uses a non-standard explicit conversion function. */
template<> template<> inline FRotator TransformConverter<FRotator>::Convert<FMatrix>(const FMatrix& Transform)
{
	return Transform.Rotator();
}

//////////////////////////////////////////////////////////////////////////
// FMatrix Support
//////////////////////////////////////////////////////////////////////////

/**
 * Converts a generic transform to a matrix using a ToMatrix() member function.
 * Uses decltype to allow some classes to return const-ref types for efficiency.
 * 
 * @param Transform
 * @return the FMatrix stored by the Transform.
 */
template<typename TransformType>
inline auto ToMatrix(const TransformType& Transform) -> decltype(Transform.ToMatrix())
{
	return Transform.ToMatrix();
}

/**
 * Specialization for the NULL Matrix conversion.
 * 
 * @param Scale Uniform Scale
 * @return Matrix that represents the uniform Scale space.
 */
inline const FMatrix& ToMatrix(const FMatrix& Transform)
{
	return Transform;
}

/**
 * Specialization for floats as a uniform scale.
 * 
 * @param Scale Uniform Scale
 * @return Matrix that represents the uniform Scale space.
 */
inline FMatrix ToMatrix(float Scale)
{
	return FScaleMatrix(Scale);
}

/**
 * Specialization for non-uniform Scale.
 * 
 * @param Scale Non-uniform Scale
 * @return Matrix that represents the non-uniform Scale space.
 */
inline FMatrix ToMatrix(const FScale& Scale)
{
	return FScaleMatrix(Scale.GetVector());
}

/**
 * Specialization for translation.
 * 
 * @param Translation Translation
 * @return Matrix that represents the translated space.
 */
inline FMatrix ToMatrix(const FVector& Translation)
{
	return FTranslationMatrix(Translation);
}

/**
 * Specialization for rotation.
 * 
 * @param Rotation Rotation
 * @return Matrix that represents the rotated space.
 */
inline FMatrix ToMatrix(const FRotator& Rotation)
{
	return FRotationMatrix(Rotation);
}

/**
 * Specialization for rotation.
 * 
 * @param Rotation Rotation
 * @return Matrix that represents the rotated space.
 */
inline FMatrix ToMatrix(const FQuat& Rotation)
{
	return FRotationMatrix::Make(Rotation);
}

/**
 * Specialization of TransformConverter for FMatrix. Calls ToMatrix() by default.
 * Allows custom types to easily provide support via a ToMatrix() overload or a ToMatrix() member function.
 * Uses decltype to support efficient passthrough of classes that can convert to a FMatrix without creating
 * a new instance.
 */
template<>
struct TransformConverter<FMatrix>
{
	template<typename OtherTransformType>
	static auto Convert(const OtherTransformType& Transform) -> decltype(ToMatrix(Transform))
	{
		return ToMatrix(Transform);
	}
};

/** concatenation rules for basic UE4 types. */
template<> struct ConcatenateRules<float        , FScale       > { typedef FScale ResultType; };
template<> struct ConcatenateRules<FScale       , float        > { typedef FScale ResultType; };
template<> struct ConcatenateRules<float        , FVector      > { typedef FMatrix ResultType; };
template<> struct ConcatenateRules<FVector      , float        > { typedef FMatrix ResultType; };
template<> struct ConcatenateRules<float        , FRotator     > { typedef FMatrix ResultType; };
template<> struct ConcatenateRules<FRotator     , float        > { typedef FMatrix ResultType; };
template<> struct ConcatenateRules<float        , FQuat        > { typedef FMatrix ResultType; };
template<> struct ConcatenateRules<FQuat        , float        > { typedef FMatrix ResultType; };
template<> struct ConcatenateRules<float        , FMatrix      > { typedef FMatrix ResultType; };
template<> struct ConcatenateRules<FMatrix      , float        > { typedef FMatrix ResultType; };
template<> struct ConcatenateRules<FScale       , FVector      > { typedef FMatrix ResultType; };
template<> struct ConcatenateRules<FVector      , FScale       > { typedef FMatrix ResultType; };
template<> struct ConcatenateRules<FScale       , FRotator     > { typedef FMatrix ResultType; };
template<> struct ConcatenateRules<FRotator     , FScale       > { typedef FMatrix ResultType; };
template<> struct ConcatenateRules<FScale       , FQuat        > { typedef FMatrix ResultType; };
template<> struct ConcatenateRules<FQuat        , FScale       > { typedef FMatrix ResultType; };
template<> struct ConcatenateRules<FScale       , FMatrix      > { typedef FMatrix ResultType; };
template<> struct ConcatenateRules<FMatrix      , FScale       > { typedef FMatrix ResultType; };
template<> struct ConcatenateRules<FVector      , FRotator     > { typedef FMatrix ResultType; };
template<> struct ConcatenateRules<FRotator     , FVector      > { typedef FMatrix ResultType; };
template<> struct ConcatenateRules<FVector      , FQuat        > { typedef FMatrix ResultType; };
template<> struct ConcatenateRules<FQuat        , FVector      > { typedef FMatrix ResultType; };
template<> struct ConcatenateRules<FVector      , FMatrix      > { typedef FMatrix ResultType; };
template<> struct ConcatenateRules<FMatrix      , FVector      > { typedef FMatrix ResultType; };
template<> struct ConcatenateRules<FRotator     , FQuat        > { typedef FQuat ResultType; };
template<> struct ConcatenateRules<FQuat        , FRotator     > { typedef FQuat ResultType; };
template<> struct ConcatenateRules<FRotator     , FMatrix      > { typedef FMatrix ResultType; };
template<> struct ConcatenateRules<FMatrix      , FRotator     > { typedef FMatrix ResultType; };
template<> struct ConcatenateRules<FQuat        , FMatrix      > { typedef FMatrix ResultType; };
template<> struct ConcatenateRules<FMatrix      , FQuat        > { typedef FMatrix ResultType; };

//////////////////////////////////////////////////////////////////////////
// Concatenate overloads. 
// 
// Since these are existing UE4 types, we cannot rely on the default
// template that calls member functions. Instead, we provide direct overloads.
//////////////////////////////////////////////////////////////////////////

/**
 * Specialization for concatenating two Matrices.
 * 
 * @param LHS rotation that goes from space A to space B
 * @param RHS rotation that goes from space B to space C.
 * @return a new rotation representing the transformation from the input space of LHS to the output space of RHS.
 */
inline FMatrix Concatenate(const FMatrix& LHS, const FMatrix& RHS)
{
	return LHS * RHS;
}

/**
 * Specialization for concatenating two rotations.
 *
 * NOTE: FQuat concatenates right to left, opposite of how FMatrix implements it.
 *       Confusing, no? That's why we have these high level functions!
 * 
 * @param LHS rotation that goes from space A to space B
 * @param RHS rotation that goes from space B to space C.
 * @return a new rotation representing the transformation from the input space of LHS to the output space of RHS.
 */
inline FQuat Concatenate(const FQuat& LHS, const FQuat& RHS)
{
	return RHS * LHS;
}

/**
 * Specialization for concatenating two rotations.
 *
 * @param LHS rotation that goes from space A to space B
 * @param RHS rotation that goes from space B to space C.
 * @return a new rotation representing the transformation from the input space of LHS to the output space of RHS.
 */
inline FRotator Concatenate(const FRotator& LHS, const FRotator& RHS)
{
	//@todo implement a more efficient way to do this.
	return TransformCast<FRotator>(Concatenate(TransformCast<FMatrix>(LHS), TransformCast<FMatrix>(RHS)));
}

/**
 * Specialization for concatenating two translations.
 * 
 * @param LHS Translation that goes from space A to space B
 * @param RHS Translation that goes from space B to space C.
 * @return a new Translation representing the transformation from the input space of LHS to the output space of RHS.
 */
inline FVector Concatenate(const FVector& LHS, const FVector& RHS)
{
	return LHS + RHS;
}

//////////////////////////////////////////////////////////////////////////
// Inverse overloads. 
// 
// Since these are existing UE4 types, we cannot rely on the default
// template that calls member functions. Instead, we provide direct overloads.
//////////////////////////////////////////////////////////////////////////

/**
 * Inverts a transform from space A to space B so it transforms from space B to space A.
 * Specialization for FMatrix.
 * 
 * @param Transform Input transform from space A to space B.
 * @return Inverted transform from space B to space A.
 */
inline FMatrix Inverse(const FMatrix& Transform)
{
	return Transform.Inverse();
}

/**
 * Inverts a transform from space A to space B so it transforms from space B to space A.
 * Specialization for FRotator.
 * 
 * @param Transform Input transform from space A to space B.
 * @return Inverted transform from space B to space A.
 */
inline FRotator Inverse(const FRotator& Transform)
{
	FVector EulerAngles = Transform.Euler();
	return FRotator::MakeFromEuler(FVector(-EulerAngles.Z, -EulerAngles.Y, -EulerAngles.X));
}

/**
 * Inverts a transform from space A to space B so it transforms from space B to space A.
 * Specialization for FQuat.
 * 
 * @param Transform Input transform from space A to space B.
 * @return Inverted transform from space B to space A.
 */
inline FQuat Inverse(const FQuat& Transform)
{
	return Transform.Inverse();
}

/**
 * Inverts a transform from space A to space B so it transforms from space B to space A.
 * Specialization for translation.
 * 
 * @param Transform Input transform from space A to space B.
 * @return Inverted transform from space B to space A.
 */
inline FVector Inverse(const FVector& Transform)
{
	return -Transform;
}

//////////////////////////////////////////////////////////////////////////
// TransformPoint overloads. 
// 
// Since these are existing UE4 types, we cannot rely on the default
// template that calls member functions. Instead, we provide direct overloads.
//////////////////////////////////////////////////////////////////////////

/**
 * Specialization for FMatrix as it's member function is called something slightly different.
 */
inline FVector TransformPoint(const FMatrix& Transform, const FVector& Point)
{
	return Transform.TransformPosition(Point);
}

/**
 * Specialization for FQuat as it's member function is called something slightly different.
 */
inline FVector TransformPoint(const FQuat& Transform, const FVector& Point)
{
	return Transform.RotateVector(Point);
}

/**
 * Specialization for FQuat as it's member function is called something slightly different.
 */
inline FVector TransformVector(const FQuat& Transform, const FVector& Vector)
{
	return Transform.RotateVector(Vector);
}

/**
 * Specialization for FRotator as it's member function is called something slightly different.
 */
inline FVector TransformPoint(const FRotator& Transform, const FVector& Point)
{
	return Transform.RotateVector(Point);
}

/**
 * Specialization for FRotator as it's member function is called something slightly different.
 */
inline FVector TransformVector(const FRotator& Transform, const FVector& Vector)
{
	return Transform.RotateVector(Vector);
}

/**
 * Specialization for FVector Translation.
 */
inline FVector TransformPoint(const FVector& Transform, const FVector& Point)
{
	return Transform + Point;
}

/**
 * Specialization for FVector Translation (does nothing).
 */
inline const FVector& TransformVector(const FVector& Transform, const FVector& Vector)
{
	return Vector;
}

/**
 * Specialization for Scale.
 */
inline FVector TransformPoint(const FScale& Transform, const FVector& Point)
{
	return Transform.GetVector() * Point;
}

/**
 * Specialization for Scale.
 */
inline FVector TransformVector(const FScale& Transform, const FVector& Vector)
{
	return Transform.GetVector() * Vector;
}

