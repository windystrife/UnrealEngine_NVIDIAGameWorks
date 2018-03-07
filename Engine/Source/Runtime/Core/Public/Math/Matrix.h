// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "HAL/UnrealMemory.h"
#include "Math/UnrealMathUtility.h"
#include "Containers/UnrealString.h"
#include "Math/Vector.h"
#include "Math/Vector4.h"
#include "Math/Plane.h"
#include "Math/Rotator.h"
#include "Math/Axis.h"

/**
 * 4x4 matrix of floating point values.
 * Matrix-matrix multiplication happens with a pre-multiple of the transpose --
 * in other words, Res = Mat1.operator*(Mat2) means Res = Mat2^T * Mat1, as
 * opposed to Res = Mat1 * Mat2.
 * Matrix elements are accessed with M[RowIndex][ColumnIndex].
 */
struct FMatrix
{
public:
	union
	{
		MS_ALIGN(16) float M[4][4] GCC_ALIGN(16);
	};

	//Identity matrix
	MS_ALIGN(16) static CORE_API const FMatrix Identity GCC_ALIGN(16);

	// Constructors.
	FORCEINLINE FMatrix();

	/**
	 * Constructor.
	 *
	 * @param EForceInit Force Init Enum.
	 */
	explicit FORCEINLINE FMatrix(EForceInit)
	{
		FMemory::Memzero(this,sizeof(*this));
	}

	/**
	 * Constructor.
	 *
	 * @param InX X plane 
	 * @param InY Y plane
	 * @param InZ Z plane
	 * @param InW W plane
	 */
	FORCEINLINE FMatrix(const FPlane& InX,const FPlane& InY,const FPlane& InZ,const FPlane& InW);

	/**
	 * Constructor.
	 *
	 * @param InX X vector
	 * @param InY Y vector
	 * @param InZ Z vector
	 * @param InW W vector
	 */
	FORCEINLINE FMatrix(const FVector& InX,const FVector& InY,const FVector& InZ,const FVector& InW);

	// Set this to the identity matrix
	inline void SetIdentity();

	/**
	 * Gets the result of multiplying a Matrix to this.
	 *
	 * @param Other The matrix to multiply this by.
	 * @return The result of multiplication.
	 */
	FORCEINLINE FMatrix operator* (const FMatrix& Other) const;

	/**
	 * Multiply this by a matrix.
	 *
	 * @param Other the matrix to multiply by this.
	 * @return reference to this after multiply.
	 */
	FORCEINLINE void operator*=(const FMatrix& Other);

	/**
	 * Gets the result of adding a matrix to this.
	 *
	 * @param Other The Matrix to add.
	 * @return The result of addition.
	 */
	FORCEINLINE FMatrix operator+ (const FMatrix& Other) const;

	/**
	 * Adds to this matrix.
	 *
	 * @param Other The matrix to add to this.
	 * @return Reference to this after addition.
	 */
	FORCEINLINE void operator+=(const FMatrix& Other);

	/** 
	  * This isn't applying SCALE, just multiplying float to all members - i.e. weighting
	  */
	FORCEINLINE FMatrix operator* (float Other) const;

	/**
	 * Multiply this matrix by a weighting factor.
	 *
	 * @param other The weight.
	 * @return a reference to this after weighting.
	 */
	FORCEINLINE void operator*=(float Other);

 	/**
	 * Checks whether two matrix are identical.
	 *
	 * @param Other The other matrix.
	 * @return true if two matrix are identical, otherwise false.
	 */
	inline bool operator==(const FMatrix& Other) const;

	/**
	 * Checks whether another Matrix is equal to this, within specified tolerance.
	 *
	 * @param Other The other Matrix.
	 * @param Tolerance Error Tolerance.
	 * @return true if two Matrix are equal, within specified tolerance, otherwise false.
	 */
	inline bool Equals(const FMatrix& Other, float Tolerance=KINDA_SMALL_NUMBER) const;

	/**
	 * Checks whether another Matrix is not equal to this, within specified tolerance.
	 *
	 * @param Other The other Matrix.
	 * @return true if two Matrix are not equal, within specified tolerance, otherwise false.
	 */
	inline bool operator!=(const FMatrix& Other) const;

	// Homogeneous transform.
	FORCEINLINE FVector4 TransformFVector4(const FVector4& V) const;

	/** Transform a location - will take into account translation part of the FMatrix. */
	FORCEINLINE FVector4 TransformPosition(const FVector &V) const;

	/** Inverts the matrix and then transforms V - correctly handles scaling in this matrix. */
	FORCEINLINE FVector InverseTransformPosition(const FVector &V) const;

	/** 
	 *	Transform a direction vector - will not take into account translation part of the FMatrix. 
	 *	If you want to transform a surface normal (or plane) and correctly account for non-uniform scaling you should use TransformByUsingAdjointT.
	 */
	FORCEINLINE FVector4 TransformVector(const FVector& V) const;

	/** 
	 *	Transform a direction vector by the inverse of this matrix - will not take into account translation part.
	 *	If you want to transform a surface normal (or plane) and correctly account for non-uniform scaling you should use TransformByUsingAdjointT with adjoint of matrix inverse.
	 */
	FORCEINLINE FVector InverseTransformVector(const FVector &V) const;

	// Transpose.

	FORCEINLINE FMatrix GetTransposed() const;

	// @return determinant of this matrix.

	inline float Determinant() const;

	/** @return the determinant of rotation 3x3 matrix */
	inline float RotDeterminant() const;

	/** Fast path, doesn't check for nil matrices in final release builds */
	inline FMatrix InverseFast() const;

	/** Fast path, and handles nil matrices. */
	inline FMatrix Inverse() const;

	inline FMatrix TransposeAdjoint() const;

	// NOTE: There is some compiler optimization issues with WIN64 that cause FORCEINLINE to cause a crash
	// Remove any scaling from this matrix (ie magnitude of each row is 1) with error Tolerance
	inline void RemoveScaling(float Tolerance=SMALL_NUMBER);

	// Returns matrix after RemoveScaling with error Tolerance
	inline FMatrix GetMatrixWithoutScale(float Tolerance=SMALL_NUMBER) const;

	/** Remove any scaling from this matrix (ie magnitude of each row is 1) and return the 3D scale vector that was initially present with error Tolerance */
	inline FVector ExtractScaling(float Tolerance=SMALL_NUMBER);

	/** return a 3D scale vector calculated from this matrix (where each component is the magnitude of a row vector) with error Tolerance. */
	inline FVector GetScaleVector(float Tolerance=SMALL_NUMBER) const;

	// Remove any translation from this matrix
	inline FMatrix RemoveTranslation() const;

	/** Returns a matrix with an additional translation concatenated. */
	inline FMatrix ConcatTranslation(const FVector& Translation) const;

	/** Returns true if any element of this matrix is NaN */
	inline bool ContainsNaN() const;

	/** Scale the translation part of the matrix by the supplied vector. */
	inline void ScaleTranslation(const FVector& Scale3D);

	/** @return the maximum magnitude of any row of the matrix. */
	inline float GetMaximumAxisScale() const;

	/** Apply Scale to this matrix **/
	inline FMatrix ApplyScale(float Scale);

	// @return the origin of the co-ordinate system
	inline FVector GetOrigin() const;

	/**
	 * get axis of this matrix scaled by the scale of the matrix
	 *
	 * @param i index into the axis of the matrix
	 * @ return vector of the axis
	 */
	inline FVector GetScaledAxis(EAxis::Type Axis) const;

	/**
	 * get axes of this matrix scaled by the scale of the matrix
	 *
	 * @param X axes returned to this param
	 * @param Y axes returned to this param
	 * @param Z axes returned to this param
	 */
	inline void GetScaledAxes(FVector &X, FVector &Y, FVector &Z) const;

	/**
	 * get unit length axis of this matrix
	 *
	 * @param i index into the axis of the matrix
	 * @return vector of the axis
	 */
	inline FVector GetUnitAxis(EAxis::Type Axis) const;

	/**
	 * get unit length axes of this matrix
	 *
	 * @param X axes returned to this param
	 * @param Y axes returned to this param
	 * @param Z axes returned to this param
	 */
	inline void GetUnitAxes(FVector &X, FVector &Y, FVector &Z) const;

	/**
	 * set an axis of this matrix
	 *
	 * @param i index into the axis of the matrix
	 * @param Axis vector of the axis
	 */
	inline void SetAxis( int32 i, const FVector& Axis );

	// Set the origin of the coordinate system to the given vector
	inline void SetOrigin( const FVector& NewOrigin );

	/**
	 * Update the axes of the matrix if any value is NULL do not update that axis
	 *
	 * @param Axis0 set matrix row 0
	 * @param Axis1 set matrix row 1 
	 * @param Axis2 set matrix row 2
	 * @param Origin set matrix row 3
	 */
	inline void SetAxes(FVector* Axis0 = NULL, FVector* Axis1 = NULL, FVector* Axis2 = NULL, FVector* Origin = NULL);

	
	/**
	 * get a column of this matrix
	 *
	 * @param i index into the column of the matrix
	 * @return vector of the column 
	 */
	inline FVector GetColumn(int32 i) const;

	/** @return rotator representation of this matrix */
	CORE_API FRotator Rotator() const;

	/** 
	 * Transform a rotation matrix into a quaternion.
	 *
	 * @warning rotation part will need to be unit length for this to be right!
	 */
	CORE_API FQuat ToQuat() const;

	// Frustum plane extraction.
	/** @param OutPlane the near plane of the Frustum of this matrix */
	FORCEINLINE bool GetFrustumNearPlane(FPlane& OutPlane) const;

	/** @param OutPlane the far plane of the Frustum of this matrix */
	FORCEINLINE bool GetFrustumFarPlane(FPlane& OutPlane) const;

	/** @param OutPlane the left plane of the Frustum of this matrix */
	FORCEINLINE bool GetFrustumLeftPlane(FPlane& OutPlane) const;

	/** @param OutPlane the right plane of the Frustum of this matrix */
	FORCEINLINE bool GetFrustumRightPlane(FPlane& OutPlane) const;

	/** @param OutPlane the top plane of the Frustum of this matrix */
	FORCEINLINE bool GetFrustumTopPlane(FPlane& OutPlane) const;

	/** @param OutPlane the bottom plane of the Frustum of this matrix */
	FORCEINLINE bool GetFrustumBottomPlane(FPlane& OutPlane) const;

	/**
	 * Utility for mirroring this transform across a certain plane, and flipping one of the axis as well.
	 */
	inline void Mirror(EAxis::Type MirrorAxis, EAxis::Type FlipAxis);

	/**
	 * Get a textual representation of the vector.
	 *
	 * @return Text describing the vector.
	 */
	CORE_API FString ToString() const;

	/** Output ToString */
	void DebugPrint() const;

	/** For debugging purpose, could be changed */
	CORE_API uint32 ComputeHash() const; 

	/**
	 * Serializes the Matrix.
	 *
	 * @param Ar Reference to the serialization archive.
	 * @param M Reference to the matrix being serialized.
	 * @return Reference to the Archive after serialization.
	 */
	friend CORE_API FArchive& operator<<(FArchive& Ar,FMatrix& M);

	bool Serialize( FArchive& Ar )
	{
		if (Ar.UE4Ver() >= VER_UE4_ADDED_NATIVE_SERIALIZATION_FOR_IMMUTABLE_STRUCTURES)
		{
			Ar << *this;
			return true;
		}
		return false;
	}

	/**
	 * Convert this Atom to the 3x4 transpose of the transformation matrix.
	 */
	void To3x4MatrixTranspose( float* Out ) const
	{
		const float* RESTRICT Src = &(M[0][0]);
		float* RESTRICT Dest = Out;

		Dest[0] = Src[0];   // [0][0]
		Dest[1] = Src[4];   // [1][0]
		Dest[2] = Src[8];   // [2][0]
		Dest[3] = Src[12];  // [3][0]

		Dest[4] = Src[1];   // [0][1]
		Dest[5] = Src[5];   // [1][1]
		Dest[6] = Src[9];   // [2][1]
		Dest[7] = Src[13];  // [3][1]

		Dest[8] = Src[2];   // [0][2]
		Dest[9] = Src[6];   // [1][2]
		Dest[10] = Src[10]; // [2][2]
		Dest[11] = Src[14]; // [3][2]
	}	

private:

	/** 
	 * Output an error message and trigger an ensure 
	 */
	static CORE_API void ErrorEnsure(const TCHAR* Message);
};


/**
 * A storage class for compile-time fixed size matrices.
 */
template<uint32 NumRows,uint32 NumColumns>
class TMatrix
{
public:

	// Variables.
	MS_ALIGN(16) float M[NumRows][NumColumns] GCC_ALIGN(16);

	// Constructor
	TMatrix();

	/** 
	 * Constructor 
	 *
	 * @param InMatrix FMatrix reference
	 */
	TMatrix(const FMatrix& InMatrix);
};


template<uint32 NumRows,uint32 NumColumns>
FORCEINLINE TMatrix<NumRows,NumColumns>::TMatrix() { }


template<uint32 NumRows,uint32 NumColumns>
FORCEINLINE TMatrix<NumRows,NumColumns>::TMatrix(const FMatrix& InMatrix)
{
	for (uint32 RowIndex = 0; (RowIndex < NumRows) && (RowIndex < 4); RowIndex++)
	{
		for (uint32 ColumnIndex = 0; (ColumnIndex < NumColumns) && (ColumnIndex < 4); ColumnIndex++)
		{
			M[RowIndex][ColumnIndex] = InMatrix.M[RowIndex][ColumnIndex];
		}
	}
}


struct FBasisVectorMatrix: FMatrix
{
	// Create Basis matrix from 3 axis vectors and the origin
	FBasisVectorMatrix(const FVector& XAxis,const FVector& YAxis,const FVector& ZAxis,const FVector& Origin);
};


struct FLookAtMatrix : FMatrix
{
	/** 
	 * Creates a view matrix given an eye position, a position to look at, and an up vector. 
	 * This does the same thing as D3DXMatrixLookAtLH.
	 */
	FLookAtMatrix(const FVector& EyePosition, const FVector& LookAtPosition, const FVector& UpVector);
};


template <> struct TIsPODType<FMatrix> { enum { Value = true }; };


// very high quality 4x4 matrix inverse
static inline void Inverse4x4( double* dst, const float* src )
{
	const double s0  = (double)(src[ 0]); const double s1  = (double)(src[ 1]); const double s2  = (double)(src[ 2]); const double s3  = (double)(src[ 3]);
	const double s4  = (double)(src[ 4]); const double s5  = (double)(src[ 5]); const double s6  = (double)(src[ 6]); const double s7  = (double)(src[ 7]);
	const double s8  = (double)(src[ 8]); const double s9  = (double)(src[ 9]); const double s10 = (double)(src[10]); const double s11 = (double)(src[11]);
	const double s12 = (double)(src[12]); const double s13 = (double)(src[13]); const double s14 = (double)(src[14]); const double s15 = (double)(src[15]);

	double inv[16];
	inv[0]  =  s5 * s10 * s15 - s5 * s11 * s14 - s9 * s6 * s15 + s9 * s7 * s14 + s13 * s6 * s11 - s13 * s7 * s10;
	inv[1]  = -s1 * s10 * s15 + s1 * s11 * s14 + s9 * s2 * s15 - s9 * s3 * s14 - s13 * s2 * s11 + s13 * s3 * s10;
	inv[2]  =  s1 * s6  * s15 - s1 * s7  * s14 - s5 * s2 * s15 + s5 * s3 * s14 + s13 * s2 * s7  - s13 * s3 * s6;
	inv[3]  = -s1 * s6  * s11 + s1 * s7  * s10 + s5 * s2 * s11 - s5 * s3 * s10 - s9  * s2 * s7  + s9  * s3 * s6;
	inv[4]  = -s4 * s10 * s15 + s4 * s11 * s14 + s8 * s6 * s15 - s8 * s7 * s14 - s12 * s6 * s11 + s12 * s7 * s10;
	inv[5]  =  s0 * s10 * s15 - s0 * s11 * s14 - s8 * s2 * s15 + s8 * s3 * s14 + s12 * s2 * s11 - s12 * s3 * s10;
	inv[6]  = -s0 * s6  * s15 + s0 * s7  * s14 + s4 * s2 * s15 - s4 * s3 * s14 - s12 * s2 * s7  + s12 * s3 * s6;
	inv[7]  =  s0 * s6  * s11 - s0 * s7  * s10 - s4 * s2 * s11 + s4 * s3 * s10 + s8  * s2 * s7  - s8  * s3 * s6;
	inv[8]  =  s4 * s9  * s15 - s4 * s11 * s13 - s8 * s5 * s15 + s8 * s7 * s13 + s12 * s5 * s11 - s12 * s7 * s9;
	inv[9]  = -s0 * s9  * s15 + s0 * s11 * s13 + s8 * s1 * s15 - s8 * s3 * s13 - s12 * s1 * s11 + s12 * s3 * s9;
	inv[10] =  s0 * s5  * s15 - s0 * s7  * s13 - s4 * s1 * s15 + s4 * s3 * s13 + s12 * s1 * s7  - s12 * s3 * s5;
	inv[11] = -s0 * s5  * s11 + s0 * s7  * s9  + s4 * s1 * s11 - s4 * s3 * s9  - s8  * s1 * s7  + s8  * s3 * s5;
	inv[12] = -s4 * s9  * s14 + s4 * s10 * s13 + s8 * s5 * s14 - s8 * s6 * s13 - s12 * s5 * s10 + s12 * s6 * s9;
	inv[13] =  s0 * s9  * s14 - s0 * s10 * s13 - s8 * s1 * s14 + s8 * s2 * s13 + s12 * s1 * s10 - s12 * s2 * s9;
	inv[14] = -s0 * s5  * s14 + s0 * s6  * s13 + s4 * s1 * s14 - s4 * s2 * s13 - s12 * s1 * s6  + s12 * s2 * s5;
	inv[15] =  s0 * s5  * s10 - s0 * s6  * s9  - s4 * s1 * s10 + s4 * s2 * s9  + s8  * s1 * s6  - s8  * s2 * s5;

	double det = s0 * inv[0] + s1 * inv[4] + s2 * inv[8] + s3 * inv[12];
	if( det != 0.0 )
	{
		det = 1.0 / det;
	}
	for( int i = 0; i < 16; i++ )
	{
		dst[i] = inv[i] * det;
	}
}

#include "Math/Matrix.inl"
