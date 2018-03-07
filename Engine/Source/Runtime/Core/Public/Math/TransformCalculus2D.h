// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Templates/AreTypesEqual.h"
#include "Math/UnrealMathUtility.h"
#include "Math/Vector2D.h"
#include "Math/TransformCalculus.h"

class FMatrix2x2;

//////////////////////////////////////////////////////////////////////////
// Transform calculus for 2D types. UE4 already has a 2D Vector class that we
// will adapt to be interpreted as a translate transform. The rest we create
// new classes for.
//
// The following types are supported
// * float           -> represents a uniform scale.
// * FScale2D        -> represents a 2D non-uniform scale.
// * FVector2D       -> represents a 2D translation.
// * FShear2D        -> represents a "2D shear", interpreted as a shear parallel to the X axis followed by a shear parallel to the Y axis.
// * FQuat2D         -> represents a pure 2D rotation.
// * FMatrix2x2      -> represents a general 2D transform.
//
//////////////////////////////////////////////////////////////////////////

class FMatrix2x2;

//////////////////////////////////////////////////////////////////////////
// Adapters for FVector2D. 
// 
// Since it is an existing UE4 types, we cannot rely on the default
// template that calls member functions. Instead, we provide direct overloads.
//////////////////////////////////////////////////////////////////////////

/** Specialization for concatenating two 2D Translations. */
inline FVector2D Concatenate(const FVector2D& LHS, const FVector2D& RHS)
{
	return LHS + RHS;
}

/** Specialization for inverting a 2D translation. */
inline FVector2D Inverse(const FVector2D& Transform)
{
	return -Transform;
}

/** Specialization for FVector2D Translation. */
inline FVector2D TransformPoint(const FVector2D& Transform, const FVector2D& Point)
{
	return Transform + Point;
}

/** Specialization for FVector2D Translation (does nothing). */
inline const FVector2D& TransformVector(const FVector2D& Transform, const FVector2D& Vector)
{
	return Vector;
}

//////////////////////////////////////////////////////////////////////////
// Adapters for 2D uniform scale.
// 
// Since it is a fundamental type, we cannot rely on the default
// template that calls member functions. Instead, we provide direct overloads.
//////////////////////////////////////////////////////////////////////////

/**
 * Specialization for uniform Scale.
 */
inline FVector2D TransformPoint(float Transform, const FVector2D& Point)
{
	return Transform * Point;
}

/**
 * Specialization for uniform Scale.
 */
inline FVector2D TransformVector(float Transform, const FVector2D& Vector)
{
	return Transform * Vector;
}

/** Represents a 2D non-uniform scale (to disambiguate from an FVector2D, which is used for translation) */
class FScale2D
{
public:
	/** Ctor. initialize to an identity scale, 1.0. */
	FScale2D() :Scale(1.0f, 1.0f) {}
	/** Ctor. initialize from a uniform scale. */
	explicit FScale2D(float InScale) :Scale(InScale, InScale) {}
	/** Ctor. initialize from a non-uniform scale. */
	explicit FScale2D(float InScaleX, float InScaleY) :Scale(InScaleX, InScaleY) {}
	/** Ctor. initialize from an FVector defining the 3D scale. */
	explicit FScale2D(const FVector2D& InScale) :Scale(InScale) {}
	
	/** Transform 2D Point */
	FVector2D TransformPoint(const FVector2D& Point) const
	{
		return Scale * Point;
	}
	/** Transform 2D Vector*/
	FVector2D TransformVector(const FVector2D& Vector) const
	{
		return TransformPoint(Vector);
	}

	/** Concatenate two scales. */
	FScale2D Concatenate(const FScale2D& RHS) const
	{
		return FScale2D(Scale * RHS.Scale);
	}
	/** Invert the scale. */
	FScale2D Inverse() const
	{
		return FScale2D(FVector2D(1.0f / Scale.X, 1.0f / Scale.Y));
	}

	/** Equality. */
	bool operator==(const FScale2D& Other) const
	{
		return Scale == Other.Scale;
	}
	
	/** Inequality. */
	bool operator!=(const FScale2D& Other) const
	{
		return !operator==(Other);
	}

	/** Access to the underlying FVector2D that stores the scale. */
	const FVector2D& GetVector() const { return Scale; }
private:
	/** Underlying storage of the 2D scale. */
	FVector2D Scale;
};

/** concatenation rules for 2D scales. */
template<> struct ConcatenateRules<float          , FScale2D  > { typedef FScale2D ResultType; };
/** concatenation rules for 2D scales. */
template<> struct ConcatenateRules<FScale2D  , float          > { typedef FScale2D ResultType; };

/** 
 * Represents a 2D shear:
 *   [1 YY]
 *   [XX 1]
 * XX represents a shear parallel to the X axis. YY represents a shear parallel to the Y axis.
 */
class FShear2D
{
public:
	/** Ctor. initialize to an identity. */
	FShear2D() :Shear(0,0) {}
	/** Ctor. initialize from a set of shears parallel to the X and Y axis, respectively. */
	explicit FShear2D(float ShearX, float ShearY) :Shear(ShearX, ShearY) {}
	/** Ctor. initialize from a 2D vector representing a set of shears parallel to the X and Y axis, respectively. */
	explicit FShear2D(const FVector2D& InShear) :Shear(InShear) {}
	
	/**
	 * Generates a shear structure based on angles instead of slope.
	 * @param InShearAngles The angles of shear.
	 * @return the sheare structure.
	 */
	static FShear2D FromShearAngles(const FVector2D& InShearAngles)
	{
		// Compute the M (Shear Slot) = CoTan(90 - SlopeAngle)

		// 0 is a special case because Tan(90) == infinity
		float ShearX = InShearAngles.X == 0 ? 0 : ( 1.0f / FMath::Tan(FMath::DegreesToRadians(90 - FMath::Clamp(InShearAngles.X, -89.0f, 89.0f))) );
		float ShearY = InShearAngles.Y == 0 ? 0 : ( 1.0f / FMath::Tan(FMath::DegreesToRadians(90 - FMath::Clamp(InShearAngles.Y, -89.0f, 89.0f))) );

		return FShear2D(ShearX, ShearY);
	}

	/** 
	 * Transform 2D Point 
	 * [X Y] * [1 YY] == [X+Y*XX Y+X*YY]
	 *         [XX 1]
	 */
	FVector2D TransformPoint(const FVector2D& Point) const
	{
		return Point + FVector2D(Point.Y, Point.X) * Shear;
	}
	/** Transform 2D Vector*/
	FVector2D TransformVector(const FVector2D& Vector) const
	{
		return TransformPoint(Vector);
	}

	/**
	 * Concatenate two shears. The result is NOT a shear, but must be represented by a generalized 2x2 transform.
	 * Defer the implementation until we can declare a 2x2 matrix.
	 * [1 YYA] * [1 YYB] == [1+YYA*XXB YYB*YYA]
	 * [XXA 1]   [XXB 1]    [XXA+XXB XXA*XXB+1]
	 */
	inline FMatrix2x2 Concatenate(const FShear2D& RHS) const;

	/**
	 * Invert the shear. The result is NOT a shear, but must be represented by a generalized 2x2 transform.
	 * Defer the implementation until we can declare a 2x2 matrix.
	 * [1 YY]^-1  == 1/(1-YY*XX) * [1 -YY]
	 * [XX 1]                      [-XX 1]
	 */
	FMatrix2x2 Inverse() const;


	/** Equality. */
	bool operator==(const FShear2D& Other) const
	{
		return Shear == Other.Shear;
	}
	
	/** Inequality. */
	bool operator!=(const FShear2D& Other) const
	{
		return !operator==(Other);
	}

	/** Access to the underlying FVector2D that stores the scale. */
	const FVector2D& GetVector() const { return Shear; }
private:
	/** Underlying storage of the 2D shear. */
	FVector2D Shear;
};

/** 
 * Represents a 2D rotation as a complex number (analagous to quaternions). 
 *   Rot(theta) == cos(theta) + i * sin(theta)
 *   General transformation follows complex number algebra from there.
 * Does not use "spinor" notation using theta/2 as we don't need that decomposition for our purposes.
 * This makes the implementation for straightforward and efficient for 2D.
 */
class FQuat2D
{
public:
	/** Ctor. initialize to an identity rotation. */
	FQuat2D() :Rot(1.0f, 0.0f) {}
	/** Ctor. initialize from a rotation in radians. */
	explicit FQuat2D(float RotRadians) :Rot(FMath::Cos(RotRadians), FMath::Sin(RotRadians)) {}
	/** Ctor. initialize from an FVector2D, representing a complex number. */
	explicit FQuat2D(const FVector2D& InRot) :Rot(InRot) {}

	/**
	 * Transform a 2D point by the 2D complex number representing the rotation:
	 * In imaginary land: (x + yi) * (u + vi) == (xu - yv) + (xv + yu)i
	 * 
	 * Looking at this as a matrix, x == cos(A), y == sin(A)
	 * 
	 * [x y] * [ cosA  sinA] == [x y] * [ u v] == [xu-yv xv+yu]
	 *         [-sinA  cosA]            [-v u]
	 *         
	 * Looking at the above results, we see the equivalence with matrix multiplication.
	 */
	FVector2D TransformPoint(const FVector2D& Point) const
	{
		return FVector2D(
			Point.X * Rot.X - Point.Y * Rot.Y,
			Point.X * Rot.Y + Point.Y * Rot.X);
	}
	/**
	 * Vector rotation is equivalent to rotating a point.
	 */
	FVector2D TransformVector(const FVector2D& Vector) const
	{
		return TransformPoint(Vector);
	}
	/**
	 * Transform 2 rotations defined by complex numbers:
	 * In imaginary land: (A + Bi) * (C + Di) == (AC - BD) + (AD + BC)i
	 * 
	 * Looking at this as a matrix, A == cos(theta), B == sin(theta), C == cos(sigma), D == sin(sigma):
	 * 
	 * [ A B] * [ C D] == [  AC-BD  AD+BC]
	 * [-B A]   [-D C]    [-(AD+BC) AC-BD]
	 * 
	 * If you look at how the vector multiply works out: [X(AC-BD)+Y(-BC-AD)  X(AD+BC)+Y(-BD+AC)]
	 * you can see it follows the same form of the imaginary form. Indeed, check out how the matrix nicely works
	 * out to [ A B] for a visual proof of the results.
	 *        [-B A]
	 */
	FQuat2D Concatenate(const FQuat2D& RHS) const
	{
		return FQuat2D(TransformPoint(RHS.Rot));
	}
	/**
	 * Invert the rotation  defined by complex numbers:
	 * In imaginary land, an inverse is a complex conjugate, which is equivalent to reflecting about the X axis:
	 * Conj(A + Bi) == A - Bi
	 */
	FQuat2D Inverse() const
	{
		return FQuat2D(FVector2D(Rot.X, -Rot.Y));
	}

	/** Equality. */
	bool operator==(const FQuat2D& Other) const
	{
		return Rot == Other.Rot;
	}
	
	/** Inequality. */
	bool operator!=(const FQuat2D& Other) const
	{
		return !operator==(Other);
	}

	/** Access to the underlying FVector2D that stores the complex number. */
	const FVector2D& GetVector() const { return Rot; }
private:
	/** Underlying storage of the rotation (X = cos(theta), Y = sin(theta). */
	FVector2D Rot;
};

/**
 * 2x2 generalized matrix. As FMatrix, we assume row vectors, row major storage:
 *    [X Y] * [m00 m01]
 *            [m10 m11]
 */
class FMatrix2x2
{
public:
	/** Ctor. initialize to an identity. */
	FMatrix2x2()
	{
		M[0][0] = 1; M[0][1] = 0;
		M[1][0] = 0; M[1][1] = 1;
	}

	FMatrix2x2(float m00, float m01, float m10, float m11)
	{
		M[0][0] = m00; M[0][1] = m01;
		M[1][0] = m10; M[1][1] = m11;
	}


	/** Ctor. initialize from a scale. */
	explicit FMatrix2x2(float UniformScale)
	{
		M[0][0] = UniformScale; M[0][1] = 0;
		M[1][0] = 0; M[1][1] = UniformScale;
	}

	/** Ctor. initialize from a scale. */
	explicit FMatrix2x2(const FScale2D& Scale)
	{
		float ScaleX = Scale.GetVector().X;
		float ScaleY = Scale.GetVector().Y;
		M[0][0] = ScaleX; M[0][1] = 0;
		M[1][0] = 0; M[1][1] = ScaleY;
	}

	/** Factory function. initialize from a 2D shear. */
	explicit FMatrix2x2(const FShear2D& Shear)
	{
		float XX = Shear.GetVector().X;
		float YY = Shear.GetVector().Y;
		M[0][0] = 1; M[0][1] =YY;
		M[1][0] =XX; M[1][1] = 1;
	}

	/** Ctor. initialize from a rotation. */
	explicit FMatrix2x2(const FQuat2D& Rotation)
	{
		float CosAngle = Rotation.GetVector().X;
		float SinAngle = Rotation.GetVector().Y;
		M[0][0] = CosAngle; M[0][1] = SinAngle;
		M[1][0] = -SinAngle; M[1][1] = CosAngle;
	}

	/**
	 * Transform a 2D point
	 *    [X Y] * [m00 m01]
	 *            [m10 m11]
	 */
	FVector2D TransformPoint(const FVector2D& Point) const
	{
		return FVector2D(
			Point.X * M[0][0] + Point.Y * M[1][0],
			Point.X * M[0][1] + Point.Y * M[1][1]);
	}
	/**
	 * Vector transformation is equivalent to point transformation as our matrix is not homogeneous.
	 */
	FVector2D TransformVector(const FVector2D& Vector) const
	{
		return TransformPoint(Vector);
	}
	/**
	 * Concatenate 2 matrices:
	 * [A B] * [E F] == [AE+BG AF+BH]
	 * [C D]   [G H]    [CE+DG CF+DH]
	 */
	FMatrix2x2 Concatenate(const FMatrix2x2& RHS) const
	{
		float A, B, C, D;
		GetMatrix(A, B, C, D);
		float E, F, G, H;
		RHS.GetMatrix(E, F, G, H);
		return FMatrix2x2(
			A*E + B*G, A*F + B*H,
			C*E + D*G, C*F + D*H);
	}
	/**
	 * Invert the transform.
	 */
	FMatrix2x2 Inverse() const
	{
		float A, B, C, D;
		GetMatrix(A, B, C, D);
		float InvDet = InverseDeterminant();
		return FMatrix2x2(
			 D*InvDet, -B*InvDet,
			-C*InvDet,  A*InvDet);
	}

	/** Equality. */
	bool operator==(const FMatrix2x2& RHS) const
	{
		float A, B, C, D;
		GetMatrix(A, B, C, D);
		float E, F, G, H;
		RHS.GetMatrix(E, F, G, H);
		return
			FMath::IsNearlyEqual(A, E, KINDA_SMALL_NUMBER) &&
			FMath::IsNearlyEqual(B, F, KINDA_SMALL_NUMBER) &&
			FMath::IsNearlyEqual(C, G, KINDA_SMALL_NUMBER) &&
			FMath::IsNearlyEqual(D, H, KINDA_SMALL_NUMBER);
	}

	/** Inequality. */
	bool operator!=(const FMatrix2x2& Other) const
	{
		return !operator==(Other);
	}

	void GetMatrix(float &A, float &B, float &C, float &D) const
	{
		A = M[0][0]; B = M[0][1];
		C = M[1][0]; D = M[1][1];
	}

	float Determinant() const
	{
		float A, B, C, D;
		GetMatrix(A, B, C, D);
		return (A*D - B*C);
	}

	float InverseDeterminant() const
	{
		float Det = Determinant();
		checkSlow(Det != 0.0f);
		return 1.0f / Det;
	}

	/** Extracts the squared scale from the matrix (avoids sqrt). */
	FScale2D GetScaleSquared() const
	{
		float A, B, C, D;
		GetMatrix(A, B, C, D);
		return FScale2D(A*A + B*B, C*C + D*D);
	}

	/** Gets the scale from the matrix. */
	FScale2D GetScale() const
	{
		FScale2D ScaleSquared = GetScaleSquared();
		return FScale2D(FMath::Sqrt(ScaleSquared.GetVector().X), FMath::Sqrt(ScaleSquared.GetVector().Y));
	}

	/** Gets the rotation angle of the matrix. */
	float GetRotationAngle() const
	{
		float A, B, C, D;
		GetMatrix(A, B, C, D);
		return FMath::Atan(C / D);
	}

	/** Determines if the matrix is identity or not. Uses exact float comparison, so rounding error is not considered. */
	bool IsIdentity() const
	{
		return M[0][0] == 1.0f && M[0][1] == 0.0f
			&& M[1][0] == 0.0f && M[1][1] == 1.0f;
	}

	bool IsNearlyIdentity(float ErrorTolerance = KINDA_SMALL_NUMBER) const
	{
		return
			FMath::IsNearlyEqual(M[0][0], 1.0f, ErrorTolerance) &&
			FMath::IsNearlyEqual(M[0][1], 0.0f, ErrorTolerance) &&
			FMath::IsNearlyEqual(M[1][0], 0.0f, ErrorTolerance) &&
			FMath::IsNearlyEqual(M[1][1], 1.0f, ErrorTolerance);
	}

private:
	float M[2][2];
};

inline FMatrix2x2 FShear2D::Concatenate(const FShear2D& RHS) const
{
	float XXA = Shear.X;
	float YYA = Shear.Y;
	float XXB = RHS.Shear.X;
	float YYB = RHS.Shear.Y;
	return FMatrix2x2(
		1+YYA*XXB, YYB*YYA,
		XXA+XXB, XXA*XXB+1);
}

inline FMatrix2x2 FShear2D::Inverse() const
{
	float InvDet = 1.0f / (1.0f - Shear.X*Shear.Y);
	return FMatrix2x2(
		InvDet, -Shear.Y * InvDet,
		-Shear.X * InvDet, InvDet);
}

/** Partial specialization of ConcatenateRules for 2x2 and any other type via Upcast to 2x2 first. Requires a conversion ctor on FMatrix2x2. Funky template logic so we don't hide the default rule for NULL conversions. */
template<typename TransformType> struct ConcatenateRules<typename TEnableIf<!TAreTypesEqual<FMatrix2x2, TransformType>::Value, FMatrix2x2>::Type, TransformType  > { typedef FMatrix2x2 ResultType; };
/** Partial specialization of ConcatenateRules for 2x2 and any other type via Upcast to 2x2 first. Requires a conversion ctor on FMatrix2x2. Funky template logic so we don't hide the default rule for NULL conversions. */
template<typename TransformType> struct ConcatenateRules<TransformType  , typename TEnableIf<!TAreTypesEqual<FMatrix2x2, TransformType>::Value, FMatrix2x2>::Type> { typedef FMatrix2x2 ResultType; };

/** concatenation rules for 2x2 transform types. Convert to 2x2 matrix as the fully decomposed math is not that perf critical right now. */
template<> struct ConcatenateRules<FScale2D  , FShear2D  > { typedef FMatrix2x2 ResultType; };
template<> struct ConcatenateRules<FScale2D  , FQuat2D   > { typedef FMatrix2x2 ResultType; };
template<> struct ConcatenateRules<FShear2D  , FScale2D  > { typedef FMatrix2x2 ResultType; };
template<> struct ConcatenateRules<FQuat2D   , FScale2D  > { typedef FMatrix2x2 ResultType; };
template<> struct ConcatenateRules<FShear2D  , FQuat2D   > { typedef FMatrix2x2 ResultType; };
template<> struct ConcatenateRules<FQuat2D   , FShear2D  > { typedef FMatrix2x2 ResultType; };

 /**
 * Support for generalized 2D affine transforms. 
 * Implemented as a 2x2 transform followed by translation. In matrix form:
 *   [A B 0]
 *   [C D 0]
 *   [X Y 1]
 */
class FTransform2D
{
public:
	/** Initialize the transform using an identity matrix and a translation. */
	FTransform2D(const FVector2D& Translation = FVector2D(0.f,0.f))
		: Trans(Translation)
	{
	}

	/** Initialize the transform using a uniform scale and a translation. */
	explicit FTransform2D(float UniformScale, const FVector2D& Translation = FVector2D(0.f,0.f))
		: M(FScale2D(UniformScale)), Trans(Translation)
	{
	}

	/** Initialize the transform using a 2D scale and a translation. */
	explicit FTransform2D(const FScale2D& Scale, const FVector2D& Translation = FVector2D(0.f,0.f))
		: M(Scale), Trans(Translation)
	{
	}

	/** Initialize the transform using a 2D shear and a translation. */
	explicit FTransform2D(const FShear2D& Shear, const FVector2D& Translation = FVector2D(0.f,0.f))
		: M(Shear), Trans(Translation)
	{
	}

	/** Initialize the transform using a 2D rotation and a translation. */
	explicit FTransform2D(const FQuat2D& Rot, const FVector2D& Translation = FVector2D(0.f,0.f))
		: M(Rot), Trans(Translation)
	{
	}

	/** Initialize the transform using a general 2x2 transform and a translation. */
	explicit FTransform2D(const FMatrix2x2& Transform, const FVector2D& Translation = FVector2D(0.f,0.f))
		: M(Transform), Trans(Translation)
	{
	}

	/**
	 * 2D transformation of a point.  Transforms position, rotation, and scale.
	 */
	FVector2D TransformPoint(const FVector2D& Point) const
	{
		return ::TransformPoint(Trans, ::TransformPoint(M, Point));
	}

	/**
	 * 2D transformation of a vector.  Transforms rotation and scale.
	 */
	FVector2D TransformVector(const FVector2D& Vector) const
	{
		return ::TransformVector(M, Vector);
	}

	/** 
	 * Concatenates two transforms. Result is equivalent to transforming first by this, followed by RHS.
	 *  Concat(A,B) == (P * MA + TA) * MB + TB
	 *              == (P * MA * MB) + TA*MB + TB
	 *  NewM == MA * MB
	 *  NewT == TA * MB + TB
	 */
	FTransform2D Concatenate(const FTransform2D& RHS) const
	{
		return FTransform2D(
			::Concatenate(M, RHS.M),
			::Concatenate(::TransformPoint(RHS.M, Trans), RHS.Trans));
	}

	/** 
	 * Inverts a transform. So a transform from space A to space B results in a transform from space B to space A. 
	 * Since this class applies the 2x2 transform followed by translation, our inversion logic needs to be able to recast
	 * the result as a M * T. It does it using the following identity:
	 *   (M * T)^-1 == T^-1 * M^-1
	 *   
	 * In homogeneous form, we represent our affine transform like so:
	 *      M    *    T
	 *   [A B 0]   [1 0 0]   [A B 0]
	 *   [C D 0] * [0 1 0] = [C D 0]. This class simply decomposes the 2x2 transform and translation.
	 *   [0 0 1]   [X Y 1]   [X Y 1]
	 * 
	 * But if we were applying the transforms in reverse order (as we need to for the inverse identity above):
	 *    T^-1   *  M^-1
	 *   [1 0 0]   [A B 0]   [A  B  0]  where [X' Y'] = [X Y] * [A B]
	 *   [0 1 0] * [C D 0] = [C  D  0]                          [C D]
	 *   [X Y 1]   [0 0 1]   [X' Y' 1]
	 * 
	 * This can be conceptualized by seeing that a translation effectively defines a new local origin for that 
	 * frame of reference. Since there is a 2x2 transform AFTER that, the concatenated frame of reference has an origin
	 * that is the old origin transformed by the 2x2 transform.
	 * 
	 * In the last equation:
	 * We know that [X Y] is the translation induced by inverting T, or -Translate.
	 * We know that [[A B][C D]] == Inverse(M), so we can represent T^-1 * M^-1 as M'* T' where:
	 *   M' == Inverse(M)
	 *   T' == Inverse(Translate) * Inverse(M)
	 */
	FTransform2D Inverse() const
	{
		FMatrix2x2 InvM = ::Inverse(M);
		FVector2D InvTrans = ::TransformPoint(InvM, ::Inverse(Trans));
		return FTransform2D(InvM, InvTrans);
	}
	
	/** Equality. */
	bool operator==(const FTransform2D& Other) const
	{
		return M == Other.M && Trans == Other.Trans;
	}
	
	/** Inequality. */
	bool operator!=(const FTransform2D& Other) const
	{
		return !operator==(Other);
	}

	/** Access to the 2x2 transform */
	const FMatrix2x2& GetMatrix() const { return M; }
	/** Access to the translation */
	const FVector2D& GetTranslation() const { return Trans; }

	/**
	 * Specialized function to determine if a transform is precisely the identity transform. Uses exact float comparison, so rounding error is not considered.
	 */
	bool IsIdentity() const
	{
		return M.IsIdentity() && Trans == FVector2D::ZeroVector;
	}

	/**
	 * Converts this affine 2D Transform into an affine 3D transform.
	 */
	FMatrix To3DMatrix() const
	{
		float A, B, C, D;
		M.GetMatrix(A, B, C, D);

		return FMatrix(
			FPlane(      A,      B, 0.0f, 0.0f),
			FPlane(      C,      D, 0.0f, 0.0f),
			FPlane(   0.0f,   0.0f, 1.0f, 0.0f),
			FPlane(Trans.X, Trans.Y, 0.0f, 1.0f)
		);
	}

private:
	FMatrix2x2 M;
	FVector2D Trans;
};

template<> struct TIsPODType<FTransform2D> { enum { Value = true }; };

//////////////////////////////////////////////////////////////////////////
// Concatenate overloads. 
// 
// Efficient overloads for concatenating 2D affine transforms.
// Better than simply upcasting both to FTransform2D first.
//////////////////////////////////////////////////////////////////////////

/** Specialization for concatenating a 2D scale and 2D Translation. */
inline FTransform2D Concatenate(const FScale2D& Scale, const FVector2D& Translation)
{
	return FTransform2D(Scale, Translation);
}

/** Specialization for concatenating a 2D shear and 2D Translation. */
inline FTransform2D Concatenate(const FShear2D& Shear, const FVector2D& Translation)
{
	return FTransform2D(Shear, Translation);
}

/** Specialization for concatenating 2D Rotation and 2D Translation. */
inline FTransform2D Concatenate(const FQuat2D& Rot, const FVector2D& Translation)
{
	return FTransform2D(Rot, Translation);
}

/** Specialization for concatenating 2D generalized transform and 2D Translation. */
inline FTransform2D Concatenate(const FMatrix2x2& Transform, const FVector2D& Translation)
{
	return FTransform2D(Transform, Translation);
}

/** Specialization for concatenating transform and 2D Translation. */
inline FTransform2D Concatenate(const FTransform2D& Transform, const FVector2D& Translation)
{
	return FTransform2D(Transform.GetMatrix(), Concatenate(Transform.GetTranslation(), Translation));
}

/** Specialization for concatenating a 2D Translation and 2D scale. */
inline FTransform2D Concatenate(const FVector2D& Translation, const FScale2D& Scale)
{
	return FTransform2D(Scale, ::TransformPoint(Scale, Translation));
}

/** Specialization for concatenating a 2D Translation and 2D shear. */
inline FTransform2D Concatenate(const FVector2D& Translation, const FShear2D& Shear)
{
	return FTransform2D(Shear, ::TransformPoint(Shear, Translation));
}

/** Specialization for concatenating 2D Translation and 2D Rotation. */
inline FTransform2D Concatenate(const FVector2D& Translation, const FQuat2D& Rot)
{
	return FTransform2D(Rot, ::TransformPoint(Rot, Translation));
}

/** Specialization for concatenating 2D Translation and 2D generalized transform. See docs for FTransform2D::Inverse for details on how this math is derived. */
inline FTransform2D Concatenate(const FVector2D& Translation, const FMatrix2x2& Transform)
{
	return FTransform2D(Transform, ::TransformPoint(Transform, Translation));
}

/** Specialization for concatenating 2D Translation and transform. See docs for FTransform2D::Inverse for details on how this math is derived. */
inline FTransform2D Concatenate(const FVector2D& Translation, const FTransform2D& Transform)
{
	return FTransform2D(Transform.GetMatrix(), Concatenate(::TransformPoint(Transform.GetMatrix(), Translation), Transform.GetTranslation()));
}

/** Partial specialization of ConcatenateRules for FTransform2D and any other type via Upcast to FTransform2D first. Requires a conversion ctor on FTransform2D. Funky template logic so we don't hide the default rule for NULL conversions. */
template<typename TransformType> struct ConcatenateRules<typename TEnableIf<!TAreTypesEqual<FTransform2D, TransformType>::Value, FTransform2D>::Type, TransformType  > { typedef FTransform2D ResultType; };
/** Partial specialization of ConcatenateRules for FTransform2D and any other type via Upcast to FTransform2D first. Requires a conversion ctor on FTransform2D. Funky template logic so we don't hide the default rule for NULL conversions. */
template<typename TransformType> struct ConcatenateRules<TransformType  , typename TEnableIf<!TAreTypesEqual<FTransform2D, TransformType>::Value, FTransform2D>::Type> { typedef FTransform2D ResultType; };

/** Provide a disambiguating overload for 2x2 and FTransform2D, since both try to provide a generic set of ConcatenateRules for all types. */
template<> struct ConcatenateRules<FMatrix2x2, FTransform2D> { typedef FTransform2D ResultType; };
template<> struct ConcatenateRules<FTransform2D, FMatrix2x2> { typedef FTransform2D ResultType; };
