// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UnrealMemory.h"

/*=============================================================================
 *	Helpers:
 *============================================================================*/

/**
 *	float4 vector register type, where the first float (X) is stored in the lowest 32 bits, and so on.
 */
struct VectorRegister
{
	float	V[4];
};

/**
*	int32[4] vector register type, where the first int32 (X) is stored in the lowest 32 bits, and so on.
*/
struct VectorRegisterInt
{
	int32	V[4];
};

/**
*	double[2] vector register type, where the first double (X) is stored in the lowest 64 bits, and so on.
*/
struct VectorRegisterDouble
{
	double	V[2];
};

// For a struct of 4 floats, we need the double braces
#define DECLARE_VECTOR_REGISTER(X, Y, Z, W) { { X, Y, Z, W } }

/**
 * Returns a bitwise equivalent vector based on 4 DWORDs.
 *
 * @param X		1st uint32 component
 * @param Y		2nd uint32 component
 * @param Z		3rd uint32 component
 * @param W		4th uint32 component
 * @return		Bitwise equivalent vector with 4 floats
 */
FORCEINLINE VectorRegister MakeVectorRegister( uint32 X, uint32 Y, uint32 Z, uint32 W )
{
	VectorRegister Vec;
	((uint32&)Vec.V[0]) = X;
	((uint32&)Vec.V[1]) = Y;
	((uint32&)Vec.V[2]) = Z;
	((uint32&)Vec.V[3]) = W;
	return Vec;
}

/**
 * Returns a vector based on 4 FLOATs.
 *
 * @param X		1st float component
 * @param Y		2nd float component
 * @param Z		3rd float component
 * @param W		4th float component
 * @return		Vector of the 4 FLOATs
 */
FORCEINLINE VectorRegister MakeVectorRegister( float X, float Y, float Z, float W )
{
	VectorRegister Vec = { { X, Y, Z, W } };
	return Vec;
}

/**
* Returns a vector based on 4 int32.
*
* @param X		1st int32 component
* @param Y		2nd int32 component
* @param Z		3rd int32 component
* @param W		4th int32 component
* @return		Vector of the 4 int32
*/
FORCEINLINE VectorRegisterInt MakeVectorRegisterInt(int32 X, int32 Y, int32 Z, int32 W)
{
	VectorRegisterInt Vec;
	((int32&)Vec.V[0]) = X;
	((int32&)Vec.V[1]) = Y;
	((int32&)Vec.V[2]) = Z;
	((int32&)Vec.V[3]) = W;
	return Vec;
}

/*=============================================================================
 *	Constants:
 *============================================================================*/

#include "UnrealMathVectorConstants.h"


/*=============================================================================
 *	Intrinsics:
 *============================================================================*/

/**
 * Returns a vector with all zeros.
 *
 * @return		VectorRegister(0.0f, 0.0f, 0.0f, 0.0f)
 */
#define VectorZero()					(GlobalVectorConstants::FloatZero)

/**
 * Returns a vector with all ones.
 *
 * @return		VectorRegister(1.0f, 1.0f, 1.0f, 1.0f)
 */
#define VectorOne()						(GlobalVectorConstants::FloatOne)

/**
 * Loads 4 FLOATs from unaligned memory.
 *
 * @param Ptr	Unaligned memory pointer to the 4 FLOATs
 * @return		VectorRegister(Ptr[0], Ptr[1], Ptr[2], Ptr[3])
 */
#define VectorLoad( Ptr )				MakeVectorRegister( ((const float*)(Ptr))[0], ((const float*)(Ptr))[1], ((const float*)(Ptr))[2], ((const float*)(Ptr))[3] )

/**
 * Loads 3 FLOATs from unaligned memory and leaves W undefined.
 *
 * @param Ptr	Unaligned memory pointer to the 3 FLOATs
 * @return		VectorRegister(Ptr[0], Ptr[1], Ptr[2], undefined)
 */
#define VectorLoadFloat3( Ptr )			MakeVectorRegister( ((const float*)(Ptr))[0], ((const float*)(Ptr))[1], ((const float*)(Ptr))[2], 0.0f )

/**
 * Loads 3 FLOATs from unaligned memory and sets W=0.
 *
 * @param Ptr	Unaligned memory pointer to the 3 FLOATs
 * @return		VectorRegister(Ptr[0], Ptr[1], Ptr[2], 0.0f)
 */
#define VectorLoadFloat3_W0( Ptr )		MakeVectorRegister( ((const float*)(Ptr))[0], ((const float*)(Ptr))[1], ((const float*)(Ptr))[2], 0.0f )

/**
 * Loads 3 FLOATs from unaligned memory and sets W=1.
 *
 * @param Ptr	Unaligned memory pointer to the 3 FLOATs
 * @return		VectorRegister(Ptr[0], Ptr[1], Ptr[2], 1.0f)
 */
#define VectorLoadFloat3_W1( Ptr )		MakeVectorRegister( ((const float*)(Ptr))[0], ((const float*)(Ptr))[1], ((const float*)(Ptr))[2], 1.0f )

/**
 * Loads 4 FLOATs from aligned memory.
 *
 * @param Ptr	Aligned memory pointer to the 4 FLOATs
 * @return		VectorRegister(Ptr[0], Ptr[1], Ptr[2], Ptr[3])
 */
#define VectorLoadAligned( Ptr )		MakeVectorRegister( ((const float*)(Ptr))[0], ((const float*)(Ptr))[1], ((const float*)(Ptr))[2], ((const float*)(Ptr))[3] )

/**
 * Loads 1 float from unaligned memory and replicates it to all 4 elements.
 *
 * @param Ptr	Unaligned memory pointer to the float
 * @return		VectorRegister(Ptr[0], Ptr[0], Ptr[0], Ptr[0])
 */
#define VectorLoadFloat1( Ptr )			MakeVectorRegister( ((const float*)(Ptr))[0], ((const float*)(Ptr))[0], ((const float*)(Ptr))[0], ((const float*)(Ptr))[0] )

/**
 * Creates a vector out of three FLOATs and leaves W undefined.
 *
 * @param X		1st float component
 * @param Y		2nd float component
 * @param Z		3rd float component
 * @return		VectorRegister(X, Y, Z, undefined)
 */
#define VectorSetFloat3( X, Y, Z )		MakeVectorRegister( X, Y, Z, 0.0f )

/**
 * Creates a vector out of four FLOATs.
 *
 * @param X		1st float component
 * @param Y		2nd float component
 * @param Z		3rd float component
 * @param W		4th float component
 * @return		VectorRegister(X, Y, Z, W)
 */
#define VectorSet( X, Y, Z, W )			MakeVectorRegister( X, Y, Z, W )

/**
 * Stores a vector to aligned memory.
 *
 * @param Vec	Vector to store
 * @param Ptr	Aligned memory pointer
 */
#define VectorStoreAligned( Vec, Ptr )	FMemory::Memcpy( Ptr, &(Vec), 16 )

/**
 * Performs non-temporal store of a vector to aligned memory without polluting the caches
 *
 * @param Vec	Vector to store
 * @param Ptr	Aligned memory pointer
 */
#define VectorStoreAlignedStreamed( Vec, Ptr )	VectorStoreAligned( Vec , Ptr )

/**
 * Stores a vector to memory (aligned or unaligned).
 *
 * @param Vec	Vector to store
 * @param Ptr	Memory pointer
 */
#define VectorStore( Vec, Ptr )			FMemory::Memcpy( Ptr, &(Vec), 16 )

/**
 * Stores the XYZ components of a vector to unaligned memory.
 *
 * @param Vec	Vector to store XYZ
 * @param Ptr	Unaligned memory pointer
 */
#define VectorStoreFloat3( Vec, Ptr )	FMemory::Memcpy( Ptr, &(Vec), 12 )

/**
 * Stores the X component of a vector to unaligned memory.
 *
 * @param Vec	Vector to store X
 * @param Ptr	Unaligned memory pointer
 */
#define VectorStoreFloat1( Vec, Ptr )	FMemory::Memcpy( Ptr, &(Vec), 4 )

/**
 * Replicates one element into all four elements and returns the new vector.
 *
 * @param Vec			Source vector
 * @param ElementIndex	Index (0-3) of the element to replicate
 * @return				VectorRegister( Vec[ElementIndex], Vec[ElementIndex], Vec[ElementIndex], Vec[ElementIndex] )
 */
#define VectorReplicate( Vec, ElementIndex )	MakeVectorRegister( (Vec).V[ElementIndex], (Vec).V[ElementIndex], (Vec).V[ElementIndex], (Vec).V[ElementIndex] )

/**
 * Returns the absolute value (component-wise).
 *
 * @param Vec			Source vector
 * @return				VectorRegister( abs(Vec.x), abs(Vec.y), abs(Vec.z), abs(Vec.w) )
 */
FORCEINLINE VectorRegister VectorAbs( const VectorRegister &Vec )
{
	VectorRegister Vec2;
	Vec2.V[0] = FMath::Abs(Vec.V[0]);
	Vec2.V[1] = FMath::Abs(Vec.V[1]);
	Vec2.V[2] = FMath::Abs(Vec.V[2]);
	Vec2.V[3] = FMath::Abs(Vec.V[3]);
	return Vec2;
}

/**
 * Returns the negated value (component-wise).
 *
 * @param Vec			Source vector
 * @return				VectorRegister( -Vec.x, -Vec.y, -Vec.z, -Vec.w )
 */
#define VectorNegate( Vec )				MakeVectorRegister( -(Vec).V[0], -(Vec).V[1], -(Vec).V[2], -(Vec).V[3] )

/**
 * Adds two vectors (component-wise) and returns the result.
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		VectorRegister( Vec1.x+Vec2.x, Vec1.y+Vec2.y, Vec1.z+Vec2.z, Vec1.w+Vec2.w )
 */
FORCEINLINE VectorRegister VectorAdd( const VectorRegister& Vec1, const VectorRegister& Vec2 )
{
	VectorRegister Vec;
	Vec.V[0] = Vec1.V[0] + Vec2.V[0];
	Vec.V[1] = Vec1.V[1] + Vec2.V[1];
	Vec.V[2] = Vec1.V[2] + Vec2.V[2];
	Vec.V[3] = Vec1.V[3] + Vec2.V[3];
	return Vec;
}

/**
 * Subtracts a vector from another (component-wise) and returns the result.
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		VectorRegister( Vec1.x-Vec2.x, Vec1.y-Vec2.y, Vec1.z-Vec2.z, Vec1.w-Vec2.w )
 */
FORCEINLINE VectorRegister VectorSubtract( const VectorRegister& Vec1, const VectorRegister& Vec2 )
{
	VectorRegister Vec;
	Vec.V[0] = Vec1.V[0] - Vec2.V[0];
	Vec.V[1] = Vec1.V[1] - Vec2.V[1];
	Vec.V[2] = Vec1.V[2] - Vec2.V[2];
	Vec.V[3] = Vec1.V[3] - Vec2.V[3];
	return Vec;
}

/**
 * Multiplies two vectors (component-wise) and returns the result.
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		VectorRegister( Vec1.x*Vec2.x, Vec1.y*Vec2.y, Vec1.z*Vec2.z, Vec1.w*Vec2.w )
 */
FORCEINLINE VectorRegister VectorMultiply( const VectorRegister& Vec1, const VectorRegister& Vec2 )
{
	VectorRegister Vec;
	Vec.V[0] = Vec1.V[0] * Vec2.V[0];
	Vec.V[1] = Vec1.V[1] * Vec2.V[1];
	Vec.V[2] = Vec1.V[2] * Vec2.V[2];
	Vec.V[3] = Vec1.V[3] * Vec2.V[3];
	return Vec;
}

/**
 * Multiplies two vectors (component-wise), adds in the third vector and returns the result.
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @param Vec3	3rd vector
 * @return		VectorRegister( Vec1.x*Vec2.x + Vec3.x, Vec1.y*Vec2.y + Vec3.y, Vec1.z*Vec2.z + Vec3.z, Vec1.w*Vec2.w + Vec3.w )
 */
FORCEINLINE VectorRegister VectorMultiplyAdd( const VectorRegister& Vec1, const VectorRegister& Vec2, const VectorRegister& Vec3 )
{
	VectorRegister Vec;
	Vec.V[0] = Vec1.V[0] * Vec2.V[0] + Vec3.V[0];
	Vec.V[1] = Vec1.V[1] * Vec2.V[1] + Vec3.V[1];
	Vec.V[2] = Vec1.V[2] * Vec2.V[2] + Vec3.V[2];
	Vec.V[3] = Vec1.V[3] * Vec2.V[3] + Vec3.V[3];
	return Vec;
}

/**
* Divides two vectors (component-wise) and returns the result.
*
* @param Vec1	1st vector
* @param Vec2	2nd vector
* @return		VectorRegister( Vec1.x/Vec2.x, Vec1.y/Vec2.y, Vec1.z/Vec2.z, Vec1.w/Vec2.w )
*/
FORCEINLINE VectorRegister VectorDivide(const VectorRegister& Vec1, const VectorRegister& Vec2)
{
	VectorRegister Vec;
	Vec.V[0] = Vec1.V[0] / Vec2.V[0];
	Vec.V[1] = Vec1.V[1] / Vec2.V[1];
	Vec.V[2] = Vec1.V[2] / Vec2.V[2];
	Vec.V[3] = Vec1.V[3] / Vec2.V[3];
	return Vec;
}

/**
 * Calculates the dot3 product of two vectors and returns a vector with the result in all 4 components.
 * Only really efficient on Xbox 360.
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		d = dot3(Vec1.xyz, Vec2.xyz), VectorRegister( d, d, d, d )
 */
FORCEINLINE VectorRegister VectorDot3( const VectorRegister& Vec1, const VectorRegister& Vec2 )
{
	float D = Vec1.V[0] * Vec2.V[0] + Vec1.V[1] * Vec2.V[1] + Vec1.V[2] * Vec2.V[2];
	VectorRegister Vec = { { D, D, D, D } };
	return Vec;
}

/**
 * Calculates the dot4 product of two vectors and returns a vector with the result in all 4 components.
 * Only really efficient on Xbox 360.
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		d = dot4(Vec1.xyzw, Vec2.xyzw), VectorRegister( d, d, d, d )
 */
FORCEINLINE VectorRegister VectorDot4( const VectorRegister& Vec1, const VectorRegister& Vec2 )
{
	float D = Vec1.V[0] * Vec2.V[0] + Vec1.V[1] * Vec2.V[1] + Vec1.V[2] * Vec2.V[2] + Vec1.V[3] * Vec2.V[3];
	VectorRegister Vec = { { D, D, D, D } };
	return Vec;
}

/**
 * Creates a four-part mask based on component-wise == compares of the input vectors
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		VectorRegister( Vec1.x == Vec2.x ? 0xFFFFFFFF : 0, same for yzw )
 */

FORCEINLINE VectorRegister VectorCompareEQ( const VectorRegister& Vec1, const VectorRegister& Vec2 )
{
	return MakeVectorRegister(
		(uint32)(Vec1.V[0] == Vec2.V[0] ? 0xFFFFFFFF : 0), 
		Vec1.V[1] == Vec2.V[1] ? 0xFFFFFFFF : 0, 
		Vec1.V[2] == Vec2.V[2] ? 0xFFFFFFFF : 0, 
		Vec1.V[3] == Vec2.V[3] ? 0xFFFFFFFF : 0);
}

/**
 * Creates a four-part mask based on component-wise != compares of the input vectors
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		VectorRegister( Vec1.x != Vec2.x ? 0xFFFFFFFF : 0, same for yzw )
 */

FORCEINLINE VectorRegister VectorCompareNE( const VectorRegister& Vec1, const VectorRegister& Vec2 )
{
	return MakeVectorRegister(
		(uint32)(Vec1.V[0] != Vec2.V[0] ? 0xFFFFFFFF : 0),
		Vec1.V[1] != Vec2.V[1] ? 0xFFFFFFFF : 0, 
		Vec1.V[2] != Vec2.V[2] ? 0xFFFFFFFF : 0, 
		Vec1.V[3] != Vec2.V[3] ? 0xFFFFFFFF : 0);
}

/**
 * Creates a four-part mask based on component-wise > compares of the input vectors
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		VectorRegister( Vec1.x > Vec2.x ? 0xFFFFFFFF : 0, same for yzw )
 */

FORCEINLINE VectorRegister VectorCompareGT( const VectorRegister& Vec1, const VectorRegister& Vec2 )
{
	return MakeVectorRegister(
		(uint32)(Vec1.V[0] > Vec2.V[0] ? 0xFFFFFFFF : 0),
		Vec1.V[1] > Vec2.V[1] ? 0xFFFFFFFF : 0, 
		Vec1.V[2] > Vec2.V[2] ? 0xFFFFFFFF : 0, 
		Vec1.V[3] > Vec2.V[3] ? 0xFFFFFFFF : 0);
}

/**
 * Creates a four-part mask based on component-wise >= compares of the input vectors
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		VectorRegister( Vec1.x >= Vec2.x ? 0xFFFFFFFF : 0, same for yzw )
 */

FORCEINLINE VectorRegister VectorCompareGE( const VectorRegister& Vec1, const VectorRegister& Vec2 )
{
	return MakeVectorRegister(
		(uint32)(Vec1.V[0] >= Vec2.V[0] ? 0xFFFFFFFF : 0),
		Vec1.V[1] >= Vec2.V[1] ? 0xFFFFFFFF : 0,
		Vec1.V[2] >= Vec2.V[2] ? 0xFFFFFFFF : 0,
		Vec1.V[3] >= Vec2.V[3] ? 0xFFFFFFFF : 0);
}

/**
* Creates a four-part mask based on component-wise < compares of the input vectors
*
* @param Vec1	1st vector
* @param Vec2	2nd vector
* @return		VectorRegister( Vec1.x < Vec2.x ? 0xFFFFFFFF : 0, same for yzw )
*/
FORCEINLINE VectorRegister VectorCompareLT(const VectorRegister& Vec1, const VectorRegister& Vec2)
{
	return MakeVectorRegister(
		(uint32)(Vec1.V[0] < Vec2.V[0] ? 0xFFFFFFFF : 0),
		Vec1.V[1] < Vec2.V[1] ? 0xFFFFFFFF : 0,
		Vec1.V[2] < Vec2.V[2] ? 0xFFFFFFFF : 0,
		Vec1.V[3] < Vec2.V[3] ? 0xFFFFFFFF : 0);
}

/**
* Creates a four-part mask based on component-wise <= compares of the input vectors
*
* @param Vec1	1st vector
* @param Vec2	2nd vector
* @return		VectorRegister( Vec1.x <= Vec2.x ? 0xFFFFFFFF : 0, same for yzw )
*/
FORCEINLINE VectorRegister VectorCompareLE(const VectorRegister& Vec1, const VectorRegister& Vec2)
{
	return MakeVectorRegister(
		(uint32)(Vec1.V[0] <= Vec2.V[0] ? 0xFFFFFFFF : 0),
		Vec1.V[1] <= Vec2.V[1] ? 0xFFFFFFFF : 0,
		Vec1.V[2] <= Vec2.V[2] ? 0xFFFFFFFF : 0,
		Vec1.V[3] <= Vec2.V[3] ? 0xFFFFFFFF : 0);
}

/**
 * Does a bitwise vector selection based on a mask (e.g., created from VectorCompareXX)
 *
 * @param Mask  Mask (when 1: use the corresponding bit from Vec1 otherwise from Vec2)
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		VectorRegister( for each bit i: Mask[i] ? Vec1[i] : Vec2[i] )
 *
 */

FORCEINLINE VectorRegister VectorSelect(const VectorRegister& Mask, const VectorRegister& Vec1, const VectorRegister& Vec2 )
{
	uint32* V1 = (uint32*)(&(Vec1.V[0]));
	uint32* V2 = (uint32*)(&(Vec2.V[0]));
	uint32* M = (uint32*)(&(Mask.V[0]));

	return MakeVectorRegister(
		V2[0] ^ (M[0] & (V2[0] ^ V1[0])),
		V2[1] ^ (M[1] & (V2[1] ^ V1[1])),
		V2[2] ^ (M[2] & (V2[2] ^ V1[2])),
		V2[3] ^ (M[3] & (V2[3] ^ V1[3]))
	);
}

/**
 * Combines two vectors using bitwise OR (treating each vector as a 128 bit field)
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		VectorRegister( for each bit i: Vec1[i] | Vec2[i] )
 */
FORCEINLINE VectorRegister VectorBitwiseOr(const VectorRegister& Vec1, const VectorRegister& Vec2 )
{
	return MakeVectorRegister(
		(uint32)( ((uint32*)(Vec1.V))[0] | ((uint32*)(Vec2.V))[0] ),
		((uint32*)(Vec1.V))[1] | ((uint32*)(Vec2.V))[1],
		((uint32*)(Vec1.V))[2] | ((uint32*)(Vec2.V))[2],
		((uint32*)(Vec1.V))[3] | ((uint32*)(Vec2.V))[3]);
}

/**
 * Combines two vectors using bitwise AND (treating each vector as a 128 bit field)
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		VectorRegister( for each bit i: Vec1[i] & Vec2[i] )
 */
FORCEINLINE VectorRegister VectorBitwiseAnd(const VectorRegister& Vec1, const VectorRegister& Vec2 )
{
	return MakeVectorRegister(
		(uint32)( ((uint32*)(Vec1.V))[0] & ((uint32*)(Vec2.V))[0] ),
		((uint32*)(Vec1.V))[1] & ((uint32*)(Vec2.V))[1],
		((uint32*)(Vec1.V))[2] & ((uint32*)(Vec2.V))[2],
		((uint32*)(Vec1.V))[3] & ((uint32*)(Vec2.V))[3]);
}

/**
 * Combines two vectors using bitwise XOR (treating each vector as a 128 bit field)
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		VectorRegister( for each bit i: Vec1[i] ^ Vec2[i] )
 */
FORCEINLINE VectorRegister VectorBitwiseXor(const VectorRegister& Vec1, const VectorRegister& Vec2 )
{
	return MakeVectorRegister(
		(uint32)( ((uint32*)(Vec1.V))[0] ^ ((uint32*)(Vec2.V))[0] ),
		((uint32*)(Vec1.V))[1] ^ ((uint32*)(Vec2.V))[1],
		((uint32*)(Vec1.V))[2] ^ ((uint32*)(Vec2.V))[2],
		((uint32*)(Vec1.V))[3] ^ ((uint32*)(Vec2.V))[3]);
}


/**
 * Calculates the cross product of two vectors (XYZ components). W is set to 0.
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		cross(Vec1.xyz, Vec2.xyz). W is set to 0.
 */
FORCEINLINE VectorRegister VectorCross( const VectorRegister& Vec1, const VectorRegister& Vec2 )
{
	VectorRegister Vec;
	Vec.V[0] = Vec1.V[1] * Vec2.V[2] - Vec1.V[2] * Vec2.V[1];
	Vec.V[1] = Vec1.V[2] * Vec2.V[0] - Vec1.V[0] * Vec2.V[2];
	Vec.V[2] = Vec1.V[0] * Vec2.V[1] - Vec1.V[1] * Vec2.V[0];
	Vec.V[3] = 0.0f;
	return Vec;
}

/**
 * Calculates x raised to the power of y (component-wise).
 *
 * @param Base		Base vector
 * @param Exponent	Exponent vector
 * @return			VectorRegister( Base.x^Exponent.x, Base.y^Exponent.y, Base.z^Exponent.z, Base.w^Exponent.w )
 */
FORCEINLINE VectorRegister VectorPow( const VectorRegister& Base, const VectorRegister& Exponent )
{
	VectorRegister Vec;
	Vec.V[0] = FMath::Pow(Base.V[0], Exponent.V[0]);
	Vec.V[1] = FMath::Pow(Base.V[1], Exponent.V[1]);
	Vec.V[2] = FMath::Pow(Base.V[2], Exponent.V[2]);
	Vec.V[3] = FMath::Pow(Base.V[3], Exponent.V[3]);
	return Vec;
}

/**
* Returns an estimate of 1/sqrt(c) for each component of the vector
*
* @param Vector		Vector 
* @return			VectorRegister(1/sqrt(t), 1/sqrt(t), 1/sqrt(t), 1/sqrt(t))
*/
FORCEINLINE VectorRegister VectorReciprocalSqrt(const VectorRegister& Vec)
{
	return MakeVectorRegister(1.0f / FMath::Sqrt(Vec.V[0]), 1.0f / FMath::Sqrt(Vec.V[1]), 1.0f / FMath::Sqrt(Vec.V[2]), 1.0f / FMath::Sqrt(Vec.V[3]));
}

/**
 * Computes an estimate of the reciprocal of a vector (component-wise) and returns the result.
 *
 * @param Vec	1st vector
 * @return		VectorRegister( (Estimate) 1.0f / Vec.x, (Estimate) 1.0f / Vec.y, (Estimate) 1.0f / Vec.z, (Estimate) 1.0f / Vec.w )
 */
FORCEINLINE VectorRegister VectorReciprocal(const VectorRegister& Vec)
{
	return MakeVectorRegister(1.0f / Vec.V[0], 1.0f / Vec.V[1], 1.0f / Vec.V[2], 1.0f / Vec.V[3]);
}

/**
* Return Reciprocal Length of the vector
*
* @param Vector		Vector 
* @return			VectorRegister(rlen, rlen, rlen, rlen) when rlen = 1/sqrt(dot4(V))
*/
FORCEINLINE VectorRegister VectorReciprocalLen( const VectorRegister& Vector )
{
	VectorRegister Len = VectorDot4( Vector, Vector );
	float rlen = 1.0f / FMath::Sqrt(Len.V[0]);
	
	VectorRegister Result;
	Result.V[0] = rlen;
	Result.V[1] = rlen;
	Result.V[2] = rlen;
	Result.V[3] = rlen;
	return Result;
}

/**
* Return the reciprocal of the square root of each component
*
* @param Vector		Vector 
* @return			VectorRegister(1/sqrt(Vec.X), 1/sqrt(Vec.Y), 1/sqrt(Vec.Z), 1/sqrt(Vec.W))
*/
#define VectorReciprocalSqrtAccurate(Vec)	VectorReciprocalSqrt(Vec)

/**
 * Computes the reciprocal of a vector (component-wise) and returns the result.
 *
 * @param Vec	1st vector
 * @return		VectorRegister( 1.0f / Vec.x, 1.0f / Vec.y, 1.0f / Vec.z, 1.0f / Vec.w )
 */
#define VectorReciprocalAccurate(Vec)	VectorReciprocal(Vec)

/**
* Normalize vector
*
* @param Vector		Vector to normalize
* @return			Normalized VectorRegister
*/
FORCEINLINE VectorRegister VectorNormalize( const VectorRegister& Vector )
{
	return VectorMultiply( Vector, VectorReciprocalLen( Vector ) );
}

/**
* Loads XYZ and sets W=0
*
* @param Vector	VectorRegister
* @return		VectorRegister(X, Y, Z, 0.0f)
*/
#define VectorSet_W0( Vec )		MakeVectorRegister( (Vec).V[0], (Vec).V[1], (Vec).V[2], 0.0f )

/**
* Loads XYZ and sets W=1
*
* @param Vector	VectorRegister
* @return		VectorRegister(X, Y, Z, 1.0f)
*/
#define VectorSet_W1( Vec )		MakeVectorRegister( (Vec).V[0], (Vec).V[1], (Vec).V[2], 1.0f )


// 40% faster version of the Quaternion multiplication.
#define USE_FAST_QUAT_MUL 1

/**
* Multiplies two quaternions; the order matters.
*
* When composing quaternions: VectorQuaternionMultiply(C, A, B) will yield a quaternion C = A * B
* that logically first applies B then A to any subsequent transformation (right first, then left).
*
* @param Result	Pointer to where the result Quat1 * Quat2 should be stored
* @param Quat1	Pointer to the first quaternion (must not be the destination)
* @param Quat2	Pointer to the second quaternion (must not be the destination)
*/
FORCEINLINE void VectorQuaternionMultiply( void *Result, const void* Quat1, const void* Quat2)
{
	typedef float Float4[4];
	const Float4& A = *((const Float4*) Quat1);
	const Float4& B = *((const Float4*) Quat2);
	Float4 & R = *((Float4*) Result);

#if USE_FAST_QUAT_MUL
	const float T0 = (A[2] - A[1]) * (B[1] - B[2]);
	const float T1 = (A[3] + A[0]) * (B[3] + B[0]);
	const float T2 = (A[3] - A[0]) * (B[1] + B[2]);
	const float T3 = (A[1] + A[2]) * (B[3] - B[0]);
	const float T4 = (A[2] - A[0]) * (B[0] - B[1]);
	const float T5 = (A[2] + A[0]) * (B[0] + B[1]);
	const float T6 = (A[3] + A[1]) * (B[3] - B[2]);
	const float T7 = (A[3] - A[1]) * (B[3] + B[2]);
	const float T8 = T5 + T6 + T7;
	const float T9 = 0.5f * (T4 + T8);

	R[0] = T1 + T9 - T8;
	R[1] = T2 + T9 - T7;
	R[2] = T3 + T9 - T6;
	R[3] = T0 + T9 - T5;
#else
	// store intermediate results in temporaries
	const float TX = A[3]*B[0] + A[0]*B[3] + A[1]*B[2] - A[2]*B[1];
	const float TY = A[3]*B[1] - A[0]*B[2] + A[1]*B[3] + A[2]*B[0];
	const float TZ = A[3]*B[2] + A[0]*B[1] - A[1]*B[0] + A[2]*B[3];
	const float TW = A[3]*B[3] - A[0]*B[0] - A[1]*B[1] - A[2]*B[2];

	// copy intermediate result to *this
	R[0] = TX;
	R[1] = TY;
	R[2] = TZ;
	R[3] = TW;
#endif
}

/**
* Multiplies two quaternions; the order matters.
*
* Order matters when composing quaternions: C = VectorQuaternionMultiply2(A, B) will yield a quaternion C = A * B
* that logically first applies B then A to any subsequent transformation (right first, then left).
*
* @param Quat1	Pointer to the first quaternion
* @param Quat2	Pointer to the second quaternion
* @return Quat1 * Quat2
*/
FORCEINLINE VectorRegister VectorQuaternionMultiply2( const VectorRegister& Quat1, const VectorRegister& Quat2 )
{
	VectorRegister Result;
	VectorQuaternionMultiply(&Result, &Quat1, &Quat2);
	return Result;
}

/**
 * Multiplies two 4x4 matrices.
 *
 * @param Result	Pointer to where the result should be stored
 * @param Matrix1	Pointer to the first matrix
 * @param Matrix2	Pointer to the second matrix
 */
FORCEINLINE void VectorMatrixMultiply( void* Result, const void* Matrix1, const void* Matrix2 )
{
	typedef float Float4x4[4][4];
	const Float4x4& A = *((const Float4x4*) Matrix1);
	const Float4x4& B = *((const Float4x4*) Matrix2);
	Float4x4 Temp;
	Temp[0][0] = A[0][0] * B[0][0] + A[0][1] * B[1][0] + A[0][2] * B[2][0] + A[0][3] * B[3][0];
	Temp[0][1] = A[0][0] * B[0][1] + A[0][1] * B[1][1] + A[0][2] * B[2][1] + A[0][3] * B[3][1];
	Temp[0][2] = A[0][0] * B[0][2] + A[0][1] * B[1][2] + A[0][2] * B[2][2] + A[0][3] * B[3][2];
	Temp[0][3] = A[0][0] * B[0][3] + A[0][1] * B[1][3] + A[0][2] * B[2][3] + A[0][3] * B[3][3];

	Temp[1][0] = A[1][0] * B[0][0] + A[1][1] * B[1][0] + A[1][2] * B[2][0] + A[1][3] * B[3][0];
	Temp[1][1] = A[1][0] * B[0][1] + A[1][1] * B[1][1] + A[1][2] * B[2][1] + A[1][3] * B[3][1];
	Temp[1][2] = A[1][0] * B[0][2] + A[1][1] * B[1][2] + A[1][2] * B[2][2] + A[1][3] * B[3][2];
	Temp[1][3] = A[1][0] * B[0][3] + A[1][1] * B[1][3] + A[1][2] * B[2][3] + A[1][3] * B[3][3];

	Temp[2][0] = A[2][0] * B[0][0] + A[2][1] * B[1][0] + A[2][2] * B[2][0] + A[2][3] * B[3][0];
	Temp[2][1] = A[2][0] * B[0][1] + A[2][1] * B[1][1] + A[2][2] * B[2][1] + A[2][3] * B[3][1];
	Temp[2][2] = A[2][0] * B[0][2] + A[2][1] * B[1][2] + A[2][2] * B[2][2] + A[2][3] * B[3][2];
	Temp[2][3] = A[2][0] * B[0][3] + A[2][1] * B[1][3] + A[2][2] * B[2][3] + A[2][3] * B[3][3];

	Temp[3][0] = A[3][0] * B[0][0] + A[3][1] * B[1][0] + A[3][2] * B[2][0] + A[3][3] * B[3][0];
	Temp[3][1] = A[3][0] * B[0][1] + A[3][1] * B[1][1] + A[3][2] * B[2][1] + A[3][3] * B[3][1];
	Temp[3][2] = A[3][0] * B[0][2] + A[3][1] * B[1][2] + A[3][2] * B[2][2] + A[3][3] * B[3][2];
	Temp[3][3] = A[3][0] * B[0][3] + A[3][1] * B[1][3] + A[3][2] * B[2][3] + A[3][3] * B[3][3];
	memcpy( Result, &Temp, 16*sizeof(float) );
}

/**
 * Calculate the inverse of an FMatrix.
 *
 * @param DstMatrix		FMatrix pointer to where the result should be stored
 * @param SrcMatrix		FMatrix pointer to the Matrix to be inversed
 */
FORCEINLINE void VectorMatrixInverse( void* DstMatrix, const void* SrcMatrix )
{
	typedef float Float4x4[4][4];
	const Float4x4& M = *((const Float4x4*) SrcMatrix);
	Float4x4 Result;
	float Det[4];
	Float4x4 Tmp;

	Tmp[0][0]	= M[2][2] * M[3][3] - M[2][3] * M[3][2];
	Tmp[0][1]	= M[1][2] * M[3][3] - M[1][3] * M[3][2];
	Tmp[0][2]	= M[1][2] * M[2][3] - M[1][3] * M[2][2];

	Tmp[1][0]	= M[2][2] * M[3][3] - M[2][3] * M[3][2];
	Tmp[1][1]	= M[0][2] * M[3][3] - M[0][3] * M[3][2];
	Tmp[1][2]	= M[0][2] * M[2][3] - M[0][3] * M[2][2];

	Tmp[2][0]	= M[1][2] * M[3][3] - M[1][3] * M[3][2];
	Tmp[2][1]	= M[0][2] * M[3][3] - M[0][3] * M[3][2];
	Tmp[2][2]	= M[0][2] * M[1][3] - M[0][3] * M[1][2];

	Tmp[3][0]	= M[1][2] * M[2][3] - M[1][3] * M[2][2];
	Tmp[3][1]	= M[0][2] * M[2][3] - M[0][3] * M[2][2];
	Tmp[3][2]	= M[0][2] * M[1][3] - M[0][3] * M[1][2];

	Det[0]		= M[1][1]*Tmp[0][0] - M[2][1]*Tmp[0][1] + M[3][1]*Tmp[0][2];
	Det[1]		= M[0][1]*Tmp[1][0] - M[2][1]*Tmp[1][1] + M[3][1]*Tmp[1][2];
	Det[2]		= M[0][1]*Tmp[2][0] - M[1][1]*Tmp[2][1] + M[3][1]*Tmp[2][2];
	Det[3]		= M[0][1]*Tmp[3][0] - M[1][1]*Tmp[3][1] + M[2][1]*Tmp[3][2];

	float Determinant = M[0][0]*Det[0] - M[1][0]*Det[1] + M[2][0]*Det[2] - M[3][0]*Det[3];
	const float	RDet = 1.0f / Determinant;

	Result[0][0] =  RDet * Det[0];
	Result[0][1] = -RDet * Det[1];
	Result[0][2] =  RDet * Det[2];
	Result[0][3] = -RDet * Det[3];
	Result[1][0] = -RDet * (M[1][0]*Tmp[0][0] - M[2][0]*Tmp[0][1] + M[3][0]*Tmp[0][2]);
	Result[1][1] =  RDet * (M[0][0]*Tmp[1][0] - M[2][0]*Tmp[1][1] + M[3][0]*Tmp[1][2]);
	Result[1][2] = -RDet * (M[0][0]*Tmp[2][0] - M[1][0]*Tmp[2][1] + M[3][0]*Tmp[2][2]);
	Result[1][3] =  RDet * (M[0][0]*Tmp[3][0] - M[1][0]*Tmp[3][1] + M[2][0]*Tmp[3][2]);
	Result[2][0] =  RDet * (
					M[1][0] * (M[2][1] * M[3][3] - M[2][3] * M[3][1]) -
					M[2][0] * (M[1][1] * M[3][3] - M[1][3] * M[3][1]) +
					M[3][0] * (M[1][1] * M[2][3] - M[1][3] * M[2][1])
				);
	Result[2][1] = -RDet * (
					M[0][0] * (M[2][1] * M[3][3] - M[2][3] * M[3][1]) -
					M[2][0] * (M[0][1] * M[3][3] - M[0][3] * M[3][1]) +
					M[3][0] * (M[0][1] * M[2][3] - M[0][3] * M[2][1])
				);
	Result[2][2] =  RDet * (
					M[0][0] * (M[1][1] * M[3][3] - M[1][3] * M[3][1]) -
					M[1][0] * (M[0][1] * M[3][3] - M[0][3] * M[3][1]) +
					M[3][0] * (M[0][1] * M[1][3] - M[0][3] * M[1][1])
				);
	Result[2][3] = -RDet * (
					M[0][0] * (M[1][1] * M[2][3] - M[1][3] * M[2][1]) -
					M[1][0] * (M[0][1] * M[2][3] - M[0][3] * M[2][1]) +
					M[2][0] * (M[0][1] * M[1][3] - M[0][3] * M[1][1])
				);
	Result[3][0] = -RDet * (
					M[1][0] * (M[2][1] * M[3][2] - M[2][2] * M[3][1]) -
					M[2][0] * (M[1][1] * M[3][2] - M[1][2] * M[3][1]) +
					M[3][0] * (M[1][1] * M[2][2] - M[1][2] * M[2][1])
				);
	Result[3][1] =  RDet * (
					M[0][0] * (M[2][1] * M[3][2] - M[2][2] * M[3][1]) -
					M[2][0] * (M[0][1] * M[3][2] - M[0][2] * M[3][1]) +
					M[3][0] * (M[0][1] * M[2][2] - M[0][2] * M[2][1])
				);
	Result[3][2] = -RDet * (
					M[0][0] * (M[1][1] * M[3][2] - M[1][2] * M[3][1]) -
					M[1][0] * (M[0][1] * M[3][2] - M[0][2] * M[3][1]) +
					M[3][0] * (M[0][1] * M[1][2] - M[0][2] * M[1][1])
				);
	Result[3][3] =  RDet * (
				M[0][0] * (M[1][1] * M[2][2] - M[1][2] * M[2][1]) -
				M[1][0] * (M[0][1] * M[2][2] - M[0][2] * M[2][1]) +
				M[2][0] * (M[0][1] * M[1][2] - M[0][2] * M[1][1])
			);

	memcpy( DstMatrix, &Result, 16*sizeof(float) );
}

/**
 * Calculate Homogeneous transform.
 *
 * @param VecP			VectorRegister 
 * @param MatrixM		FMatrix pointer to the Matrix to apply transform
 * @return VectorRegister = VecP*MatrixM
 */
FORCEINLINE VectorRegister VectorTransformVector(const VectorRegister&  VecP,  const void* MatrixM )
{
	typedef float Float4x4[4][4];
	union { VectorRegister v; float f[4]; } Tmp, Result;
	Tmp.v = VecP;
	const Float4x4& M = *((const Float4x4*)MatrixM);	

	Result.f[0] = Tmp.f[0] * M[0][0] + Tmp.f[1] * M[1][0] + Tmp.f[2] * M[2][0] + Tmp.f[3] * M[3][0];
	Result.f[1] = Tmp.f[0] * M[0][1] + Tmp.f[1] * M[1][1] + Tmp.f[2] * M[2][1] + Tmp.f[3] * M[3][1];
	Result.f[2] = Tmp.f[0] * M[0][2] + Tmp.f[1] * M[1][2] + Tmp.f[2] * M[2][2] + Tmp.f[3] * M[3][2];
	Result.f[3] = Tmp.f[0] * M[0][3] + Tmp.f[1] * M[1][3] + Tmp.f[2] * M[2][3] + Tmp.f[3] * M[3][3];

	return Result.v;
}

/**
 * Returns the minimum values of two vectors (component-wise).
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		VectorRegister( min(Vec1.x,Vec2.x), min(Vec1.y,Vec2.y), min(Vec1.z,Vec2.z), min(Vec1.w,Vec2.w) )
 */
FORCEINLINE VectorRegister VectorMin( const VectorRegister& Vec1, const VectorRegister& Vec2 )
{
	VectorRegister Vec;
	Vec.V[0] = FMath::Min(Vec1.V[0], Vec2.V[0]);
	Vec.V[1] = FMath::Min(Vec1.V[1], Vec2.V[1]);
	Vec.V[2] = FMath::Min(Vec1.V[2], Vec2.V[2]);
	Vec.V[3] = FMath::Min(Vec1.V[3], Vec2.V[3]);
	return Vec;
}

/**
 * Returns the maximum values of two vectors (component-wise).
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		VectorRegister( max(Vec1.x,Vec2.x), max(Vec1.y,Vec2.y), max(Vec1.z,Vec2.z), max(Vec1.w,Vec2.w) )
 */
FORCEINLINE VectorRegister VectorMax( const VectorRegister& Vec1, const VectorRegister& Vec2 )
{
	VectorRegister Vec;
	Vec.V[0] = FMath::Max(Vec1.V[0], Vec2.V[0]);
	Vec.V[1] = FMath::Max(Vec1.V[1], Vec2.V[1]);
	Vec.V[2] = FMath::Max(Vec1.V[2], Vec2.V[2]);
	Vec.V[3] = FMath::Max(Vec1.V[3], Vec2.V[3]);
	return Vec;
}

/**
 * Swizzles the 4 components of a vector and returns the result.
 *
 * @param Vec		Source vector
 * @param X			Index for which component to use for X (literal 0-3)
 * @param Y			Index for which component to use for Y (literal 0-3)
 * @param Z			Index for which component to use for Z (literal 0-3)
 * @param W			Index for which component to use for W (literal 0-3)
 * @return			The swizzled vector
 */
#define VectorSwizzle( Vec, X, Y, Z, W )	MakeVectorRegister( (Vec).V[X], (Vec).V[Y], (Vec).V[Z], (Vec).V[W] )


/**
 * Creates a vector through selecting two components from each vector via a shuffle mask. 
 *
 * @param Vec1		Source vector1
 * @param Vec2		Source vector2
 * @param X			Index for which component of Vector1 to use for X (literal 0-3)
 * @param Y			Index for which component to Vector1 to use for Y (literal 0-3)
 * @param Z			Index for which component to Vector2 to use for Z (literal 0-3)
 * @param W			Index for which component to Vector2 to use for W (literal 0-3)
 * @return			The swizzled vector
 */
#define VectorShuffle( Vec1, Vec2, X, Y, Z, W )	MakeVectorRegister( (Vec1).V[X], (Vec1).V[Y], (Vec2).V[Z], (Vec2).V[W] )

/**
 * Merges the XYZ components of one vector with the W component of another vector and returns the result.
 *
 * @param VecXYZ	Source vector for XYZ_
 * @param VecW		Source register for ___W (note: the fourth component is used, not the first)
 * @return			VectorRegister(VecXYZ.x, VecXYZ.y, VecXYZ.z, VecW.w)
 */
FORCEINLINE VectorRegister VectorMergeVecXYZ_VecW(const VectorRegister& VecXYZ, const VectorRegister& VecW)
{
	return MakeVectorRegister(VecXYZ.V[0], VecXYZ.V[1], VecXYZ.V[2], VecW.V[3]);
}

/**
 * Loads 4 BYTEs from unaligned memory and converts them into 4 FLOATs.
 * IMPORTANT: You need to call VectorResetFloatRegisters() before using scalar FLOATs after you've used this intrinsic!
 *
 * @param Ptr			Unaligned memory pointer to the 4 BYTEs.
 * @return				VectorRegister( float(Ptr[0]), float(Ptr[1]), float(Ptr[2]), float(Ptr[3]) )
 */
#define VectorLoadByte4( Ptr )			MakeVectorRegister( float(((const uint8*)(Ptr))[0]), float(((const uint8*)(Ptr))[1]), float(((const uint8*)(Ptr))[2]), float(((const uint8*)(Ptr))[3]) )

/**
 * Loads 4 BYTEs from unaligned memory and converts them into 4 FLOATs in reversed order.
 * IMPORTANT: You need to call VectorResetFloatRegisters() before using scalar FLOATs after you've used this intrinsic!
 *
 * @param Ptr			Unaligned memory pointer to the 4 BYTEs.
 * @return				VectorRegister( float(Ptr[3]), float(Ptr[2]), float(Ptr[1]), float(Ptr[0]) )
 */
#define VectorLoadByte4Reverse( Ptr )	MakeVectorRegister( float(((const uint8*)(Ptr))[3]), float(((const uint8*)(Ptr))[2]), float(((const uint8*)(Ptr))[1]), float(((const uint8*)(Ptr))[0]) )

/**
 * Converts the 4 FLOATs in the vector to 4 BYTEs, clamped to [0,255], and stores to unaligned memory.
 * IMPORTANT: You need to call VectorResetFloatRegisters() before using scalar FLOATs after you've used this intrinsic!
 *
 * @param Vec			Vector containing 4 FLOATs
 * @param Ptr			Unaligned memory pointer to store the 4 BYTEs.
 */
FORCEINLINE void VectorStoreByte4( const VectorRegister& Vec, void* Ptr )
{
	uint8 *BytePtr = (uint8*) Ptr;
	BytePtr[0] = uint8( Vec.V[0] );
	BytePtr[1] = uint8( Vec.V[1] );
	BytePtr[2] = uint8( Vec.V[2] );
	BytePtr[3] = uint8( Vec.V[3] );
}

/**
* Loads packed RGB10A2(4 bytes) from unaligned memory and converts them into 4 FLOATs.
* IMPORTANT: You need to call VectorResetFloatRegisters() before using scalar FLOATs after you've used this intrinsic!
*
* @param Ptr			Unaligned memory pointer to the RGB10A2(4 bytes).
* @return				VectorRegister with 4 FLOATs loaded from Ptr.
*/
FORCEINLINE VectorRegister VectorLoadURGB10A2N(void* Ptr)
{
	float V[4];
	uint32 E = *(uint32*)Ptr;

	V[0] = float((E >> 00) & 0x3FF) / 1023.0f;
	V[1] = float((E >> 10) & 0x3FF) / 1023.0f;
	V[2] = float((E >> 20) & 0x3FF) / 1023.0f;
	V[3] = float((E >> 30) & 0x3)   / 3.0f;

	return MakeVectorRegister(V[0], V[1], V[2], V[3]);
}

/**
* Converts the 4 FLOATs in the vector RGB10A2, clamped to [0, 1023] and [0, 3], and stores to unaligned memory.
* IMPORTANT: You need to call VectorResetFloatRegisters() before using scalar FLOATs after you've used this intrinsic!
*
* @param Vec			Vector containing 4 FLOATs
* @param Ptr			Unaligned memory pointer to store the packed RGB10A2(4 bytes).
*/
FORCEINLINE void VectorStoreURGB10A2N(const VectorRegister& Vec, void* Ptr)
{
	VectorRegister Tmp;
	Tmp = VectorMax(Vec, MakeVectorRegister(0.0f, 0.0f, 0.0f, 0.0f));
	Tmp = VectorMin(Tmp, MakeVectorRegister(1.0f, 1.0f, 1.0f, 1.0f));
	Tmp = VectorMultiply(Tmp, MakeVectorRegister(1023.0f, 1023.0f, 1023.0f, 3.0f));
	
	uint32* Out = (uint32*)Ptr;
	*Out = 
		(uint32(Tmp.V[0]) & 0x3FF) << 00 |
		(uint32(Tmp.V[1]) & 0x3FF) << 10 |
		(uint32(Tmp.V[2]) & 0x3FF) << 20 |
		(uint32(Tmp.V[3]) & 0x003) << 30;
}

/**
 * Returns non-zero if any element in Vec1 is greater than the corresponding element in Vec2, otherwise 0.
 *
 * @param Vec1			1st source vector
 * @param Vec2			2nd source vector
 * @return				Non-zero integer if (Vec1.x > Vec2.x) || (Vec1.y > Vec2.y) || (Vec1.z > Vec2.z) || (Vec1.w > Vec2.w)
 */
FORCEINLINE uint32 VectorAnyGreaterThan(const VectorRegister& Vec1, const VectorRegister& Vec2)
{
	// Note: Bitwise OR:ing all results together to avoid branching.
	return (Vec1.V[0] > Vec2.V[0]) | (Vec1.V[1] > Vec2.V[1]) | (Vec1.V[2] > Vec2.V[2]) | (Vec1.V[3] > Vec2.V[3]);
}

/**
 * Resets the floating point registers so that they can be used again.
 * Some intrinsics use these for MMX purposes (e.g. VectorLoadByte4 and VectorStoreByte4).
 */
#define VectorResetFloatRegisters()

/**
 * Returns the control register.
 *
 * @return			The uint32 control register
 */
#define VectorGetControlRegister()		0

/**
 * Returns an component from a vector.
 *
 * @param Vec				Vector register
 * @param ComponentIndex	Which component to get, X=0, Y=1, Z=2, W=3
 * @return					The component as a float
 */
FORCEINLINE float VectorGetComponent(VectorRegister Vec, uint32 ComponentIndex)
{
	return (((float*)&(Vec))[ComponentIndex]);
}


/**
 * Sets the control register.
 *
 * @param ControlStatus		The uint32 control status value to set
 */
#define	VectorSetControlRegister(ControlStatus)

/**
 * Control status bit to round all floating point math results towards zero.
 */
#define VECTOR_ROUND_TOWARD_ZERO		0


/**
* Computes the sine and cosine of each component of a Vector.
*
* @param VSinAngles	VectorRegister Pointer to where the Sin result should be stored
* @param VCosAngles	VectorRegister Pointer to where the Cos result should be stored
* @param VAngles VectorRegister Pointer to the input angles 
*/
FORCEINLINE void VectorSinCos(  VectorRegister* VSinAngles, VectorRegister* VCosAngles, const VectorRegister* VAngles )
{	
	union { VectorRegister v; float f[4]; } VecSin, VecCos, VecAngles;
	VecAngles.v = *VAngles;

	FMath::SinCos(&VecSin.f[0], &VecCos.f[0], VecAngles.f[0]);
	FMath::SinCos(&VecSin.f[1], &VecCos.f[1], VecAngles.f[1]);
	FMath::SinCos(&VecSin.f[2], &VecCos.f[2], VecAngles.f[2]);
	FMath::SinCos(&VecSin.f[3], &VecCos.f[3], VecAngles.f[3]);

	*VSinAngles = VecSin.v;
	*VCosAngles = VecCos.v;
}

// Returns true if the vector contains a component that is either NAN or +/-infinite.
inline bool VectorContainsNaNOrInfinite(const VectorRegister& Vec)
{
	return FMath::IsNaN(Vec.V[0]) || FMath::IsNaN(Vec.V[1]) || FMath::IsNaN(Vec.V[2]) || FMath::IsNaN(Vec.V[3]) ||
		!FMath::IsFinite(Vec.V[0]) || !FMath::IsFinite(Vec.V[1]) || !FMath::IsFinite(Vec.V[2]) || !FMath::IsFinite(Vec.V[3]);
}


//TODO: Vectorize
FORCEINLINE VectorRegister VectorExp(const VectorRegister& X)
{
	return MakeVectorRegister(FMath::Exp(VectorGetComponent(X, 0)), FMath::Exp(VectorGetComponent(X, 1)), FMath::Exp(VectorGetComponent(X, 2)), FMath::Exp(VectorGetComponent(X, 3)));
}

//TODO: Vectorize
FORCEINLINE VectorRegister VectorExp2(const VectorRegister& X)
{
	return MakeVectorRegister(FMath::Exp2(VectorGetComponent(X, 0)), FMath::Exp2(VectorGetComponent(X, 1)), FMath::Exp2(VectorGetComponent(X, 2)), FMath::Exp2(VectorGetComponent(X, 3)));
}

//TODO: Vectorize
FORCEINLINE VectorRegister VectorLog(const VectorRegister& X)
{
	return MakeVectorRegister(FMath::Loge(VectorGetComponent(X, 0)), FMath::Loge(VectorGetComponent(X, 1)), FMath::Loge(VectorGetComponent(X, 2)), FMath::Loge(VectorGetComponent(X, 3)));
}

//TODO: Vectorize
FORCEINLINE VectorRegister VectorLog2(const VectorRegister& X)
{
	return MakeVectorRegister(FMath::Log2(VectorGetComponent(X, 0)), FMath::Log2(VectorGetComponent(X, 1)), FMath::Log2(VectorGetComponent(X, 2)), FMath::Log2(VectorGetComponent(X, 3)));
}

//TODO: Vectorize
FORCEINLINE VectorRegister VectorSin(const VectorRegister& X)
{
	return MakeVectorRegister(FMath::Sin(VectorGetComponent(X, 0)), FMath::Sin(VectorGetComponent(X, 1)), FMath::Sin(VectorGetComponent(X, 2)), FMath::Sin(VectorGetComponent(X, 3)));
}

//TODO: Vectorize
FORCEINLINE VectorRegister VectorCos(const VectorRegister& X)
{
	return MakeVectorRegister(FMath::Cos(VectorGetComponent(X, 0)), FMath::Cos(VectorGetComponent(X, 1)), FMath::Cos(VectorGetComponent(X, 2)), FMath::Cos(VectorGetComponent(X, 3)));
}

//TODO: Vectorize
FORCEINLINE VectorRegister VectorTan(const VectorRegister& X)
{
	return MakeVectorRegister(FMath::Tan(VectorGetComponent(X, 0)), FMath::Tan(VectorGetComponent(X, 1)), FMath::Tan(VectorGetComponent(X, 2)), FMath::Tan(VectorGetComponent(X, 3)));
}

//TODO: Vectorize
FORCEINLINE VectorRegister VectorASin(const VectorRegister& X)
{
	return MakeVectorRegister(FMath::Asin(VectorGetComponent(X, 0)), FMath::Asin(VectorGetComponent(X, 1)), FMath::Asin(VectorGetComponent(X, 2)), FMath::Asin(VectorGetComponent(X, 3)));
}

//TODO: Vectorize
FORCEINLINE VectorRegister VectorACos(const VectorRegister& X)
{
	return MakeVectorRegister(FMath::Acos(VectorGetComponent(X, 0)), FMath::Acos(VectorGetComponent(X, 1)), FMath::Acos(VectorGetComponent(X, 2)), FMath::Acos(VectorGetComponent(X, 3)));
}

//TODO: Vectorize
FORCEINLINE VectorRegister VectorATan(const VectorRegister& X)
{
	return MakeVectorRegister(FMath::Atan(VectorGetComponent(X, 0)), FMath::Atan(VectorGetComponent(X, 1)), FMath::Atan(VectorGetComponent(X, 2)), FMath::Atan(VectorGetComponent(X, 3)));
}

//TODO: Vectorize
FORCEINLINE VectorRegister VectorATan2(const VectorRegister& X, const VectorRegister& Y)
{
	return MakeVectorRegister(FMath::Atan2(VectorGetComponent(X, 0), VectorGetComponent(Y, 0)),
		FMath::Atan2(VectorGetComponent(X, 1), VectorGetComponent(Y, 1)),
		FMath::Atan2(VectorGetComponent(X, 2), VectorGetComponent(Y, 2)),
		FMath::Atan2(VectorGetComponent(X, 3), VectorGetComponent(Y, 3)));
}

//TODO: Vectorize
FORCEINLINE VectorRegister VectorCeil(const VectorRegister& X)
{
	return MakeVectorRegister(FMath::CeilToFloat(VectorGetComponent(X, 0)), FMath::CeilToFloat(VectorGetComponent(X, 1)), FMath::CeilToFloat(VectorGetComponent(X, 2)), FMath::CeilToFloat(VectorGetComponent(X, 3)));
}

//TODO: Vectorize
FORCEINLINE VectorRegister VectorFloor(const VectorRegister& X)
{
	return MakeVectorRegister(FMath::FloorToFloat(VectorGetComponent(X, 0)), FMath::FloorToFloat(VectorGetComponent(X, 1)), FMath::FloorToFloat(VectorGetComponent(X, 2)), FMath::FloorToFloat(VectorGetComponent(X, 3)));
}

//TODO: Vectorize
FORCEINLINE VectorRegister VectorTruncate(const VectorRegister& X)
{
	return MakeVectorRegister(FMath::TruncToFloat(VectorGetComponent(X, 0)), FMath::TruncToFloat(VectorGetComponent(X, 1)), FMath::TruncToFloat(VectorGetComponent(X, 2)), FMath::TruncToFloat(VectorGetComponent(X, 3)));
}

//TODO: Vectorize
FORCEINLINE VectorRegister VectorFractional(const VectorRegister& X)
{
	return VectorSubtract(X, VectorTruncate(X));
}

//TODO: Vectorize
FORCEINLINE VectorRegister VectorMod(const VectorRegister& X, const VectorRegister& Y)
{
	return MakeVectorRegister(FMath::Fmod(VectorGetComponent(X, 0), VectorGetComponent(Y, 0)),
		FMath::Fmod(VectorGetComponent(X, 1), VectorGetComponent(Y, 1)),
		FMath::Fmod(VectorGetComponent(X, 2), VectorGetComponent(Y, 2)),
		FMath::Fmod(VectorGetComponent(X, 3), VectorGetComponent(Y, 3)));
}

//TODO: Vectorize
FORCEINLINE VectorRegister VectorSign(const VectorRegister& X)
{
	return MakeVectorRegister(
		(float)(VectorGetComponent(X, 0) >= 0.0f ? 1.0f : 0.0f),
		(float)(VectorGetComponent(X, 1) >= 0.0f ? 1.0f : 0.0f),
		(float)(VectorGetComponent(X, 2) >= 0.0f ? 1.0f : 0.0f),
		(float)(VectorGetComponent(X, 3) >= 0.0f ? 1.0f : 0.0f));
}

//TODO: Vectorize
FORCEINLINE VectorRegister VectorStep(const VectorRegister& X)
{
	return MakeVectorRegister(
		(float)(VectorGetComponent(X, 0) >= 0.0f ? 1.0f : -1.0f),
		(float)(VectorGetComponent(X, 1) >= 0.0f ? 1.0f : -1.0f),
		(float)(VectorGetComponent(X, 2) >= 0.0f ? 1.0f : -1.0f),
		(float)(VectorGetComponent(X, 3) >= 0.0f ? 1.0f : -1.0f));
}

/**
* Loads packed RGBA16(4 bytes) from unaligned memory and converts them into 4 FLOATs.
* IMPORTANT: You need to call VectorResetFloatRegisters() before using scalar FLOATs after you've used this intrinsic!
*
* @param Ptr			Unaligned memory pointer to the RGBA16(8 bytes).
* @return				VectorRegister with 4 FLOATs loaded from Ptr.
*/
FORCEINLINE VectorRegister VectorLoadURGBA16N(void* Ptr)
{
	float V[4];
	uint16* E = (uint16*)Ptr;

	V[0] = float(E[0]) / 65535.0f;
	V[1] = float(E[1]) / 65535.0f;
	V[2] = float(E[2]) / 65535.0f;
	V[3] = float(E[3]) / 65535.0f;

	return MakeVectorRegister(V[0], V[1], V[2], V[3]);
}

/**
* Converts the 4 FLOATs in the vector RGBA16, clamped to [0, 65535], and stores to unaligned memory.
* IMPORTANT: You need to call VectorResetFloatRegisters() before using scalar FLOATs after you've used this intrinsic!
*
* @param Vec			Vector containing 4 FLOATs
* @param Ptr			Unaligned memory pointer to store the packed RGBA16(8 bytes).
*/
FORCEINLINE void VectorStoreURGBA16N(const VectorRegister& Vec, void* Ptr)
{
	VectorRegister Tmp;
	Tmp = VectorMax(Vec, MakeVectorRegister(0.0f, 0.0f, 0.0f, 0.0f));
	Tmp = VectorMin(Tmp, MakeVectorRegister(1.0f, 1.0f, 1.0f, 1.0f));
	Tmp = VectorMultiplyAdd(Tmp, MakeVectorRegister(65535.0f, 65535.0f, 65535.0f, 65535.0f), MakeVectorRegister(0.5f, 0.5f, 0.5f, 0.5f));
	Tmp = VectorTruncate(Tmp);

	uint16* Out = (uint16*)Ptr;
	Out[0] = (uint16)Tmp.V[0];
	Out[1] = (uint16)Tmp.V[0];
	Out[2] = (uint16)Tmp.V[0];
	Out[3] = (uint16)Tmp.V[0];
}

//////////////////////////////////////////////////////////////////////////
//Integer ops

//Bitwise
/** = a & b */
FORCEINLINE VectorRegisterInt VectorIntAnd(const VectorRegisterInt& A, const VectorRegisterInt& B)
{
	return MakeVectorRegisterInt(
		A.V[0] & B.V[0],
		A.V[1] & B.V[1],
		A.V[2] & B.V[2],
		A.V[3] & B.V[3]);
}

/** = a | b */
FORCEINLINE VectorRegisterInt VectorIntOr(const VectorRegisterInt& A, const VectorRegisterInt& B)
{
	return MakeVectorRegisterInt(
		A.V[0] | B.V[0],
		A.V[1] | B.V[1],
		A.V[2] | B.V[2],
		A.V[3] | B.V[3]);
}
/** = a ^ b */
FORCEINLINE VectorRegisterInt VectorIntXor(const VectorRegisterInt& A, const VectorRegisterInt& B)
{
	return MakeVectorRegisterInt(
		A.V[0] ^ B.V[0],
		A.V[1] ^ B.V[1],
		A.V[2] ^ B.V[2],
		A.V[3] ^ B.V[3]);
}

/** = (~a) & b to match _mm_andnot_si128 */
FORCEINLINE VectorRegisterInt VectorIntAndNot(const VectorRegisterInt& A, const VectorRegisterInt& B)
{
	return MakeVectorRegisterInt(
		(~A.V[0]) & B.V[0],
		(~A.V[1]) & B.V[1],
		(~A.V[2]) & B.V[2],
		(~A.V[3]) & B.V[3]);
}
/** = ~a */
FORCEINLINE VectorRegisterInt VectorIntNot(const VectorRegisterInt& A)
{
	return MakeVectorRegisterInt(
		~A.V[0],
		~A.V[1],
		~A.V[2],
		~A.V[3]);
}

//Comparison
FORCEINLINE VectorRegisterInt VectorIntCompareEQ(const VectorRegisterInt& A, const VectorRegisterInt& B)
{
	return MakeVectorRegisterInt(
		A.V[0] == B.V[0] ? 0xFFFFFFFF : 0,
		A.V[1] == B.V[1] ? 0xFFFFFFFF : 0,
		A.V[2] == B.V[2] ? 0xFFFFFFFF : 0,
		A.V[3] == B.V[3] ? 0xFFFFFFFF : 0);
}

FORCEINLINE VectorRegisterInt VectorIntCompareNEQ(const VectorRegisterInt& A, const VectorRegisterInt& B)
{
	return MakeVectorRegisterInt(
		A.V[0] != B.V[0] ? 0xFFFFFFFF : 0,
		A.V[1] != B.V[1] ? 0xFFFFFFFF : 0,
		A.V[2] != B.V[2] ? 0xFFFFFFFF : 0,
		A.V[3] != B.V[3] ? 0xFFFFFFFF : 0);
}

FORCEINLINE VectorRegisterInt VectorIntCompareGT(const VectorRegisterInt& A, const VectorRegisterInt& B)
{
	return MakeVectorRegisterInt(
		A.V[0] > B.V[0] ? 0xFFFFFFFF : 0,
		A.V[1] > B.V[1] ? 0xFFFFFFFF : 0,
		A.V[2] > B.V[2] ? 0xFFFFFFFF : 0,
		A.V[3] > B.V[3] ? 0xFFFFFFFF : 0);
}

FORCEINLINE VectorRegisterInt VectorIntCompareLT(const VectorRegisterInt& A, const VectorRegisterInt& B)
{
	return MakeVectorRegisterInt(
		A.V[0] < B.V[0] ? 0xFFFFFFFF : 0,
		A.V[1] < B.V[1] ? 0xFFFFFFFF : 0,
		A.V[2] < B.V[2] ? 0xFFFFFFFF : 0,
		A.V[3] < B.V[3] ? 0xFFFFFFFF : 0);
}

FORCEINLINE VectorRegisterInt VectorIntCompareGE(const VectorRegisterInt& A, const VectorRegisterInt& B)
{
	return MakeVectorRegisterInt(
		A.V[0] >= B.V[0] ? 0xFFFFFFFF : 0,
		A.V[1] >= B.V[1] ? 0xFFFFFFFF : 0,
		A.V[2] >= B.V[2] ? 0xFFFFFFFF : 0,
		A.V[3] >= B.V[3] ? 0xFFFFFFFF : 0);
}

FORCEINLINE VectorRegisterInt VectorIntCompareLE(const VectorRegisterInt& A, const VectorRegisterInt& B)
{
	return MakeVectorRegisterInt(
		A.V[0] <= B.V[0] ? 0xFFFFFFFF : 0,
		A.V[1] <= B.V[1] ? 0xFFFFFFFF : 0,
		A.V[2] <= B.V[2] ? 0xFFFFFFFF : 0,
		A.V[3] <= B.V[3] ? 0xFFFFFFFF : 0);
}


FORCEINLINE VectorRegisterInt VectorIntSelect(const VectorRegisterInt& Mask, const VectorRegisterInt& Vec1, const VectorRegisterInt& Vec2)
{
	return VectorIntXor(Vec2, VectorIntAnd(Mask, VectorIntXor(Vec1, Vec2)));
}

//Arithmetic
FORCEINLINE VectorRegisterInt VectorIntAdd(const VectorRegisterInt& A, const VectorRegisterInt& B)
{
	return MakeVectorRegisterInt(
		A.V[0] + B.V[0],
		A.V[1] + B.V[1],
		A.V[2] + B.V[2],
		A.V[3] + B.V[3]);
}

FORCEINLINE VectorRegisterInt VectorIntSubtract(const VectorRegisterInt& A, const VectorRegisterInt& B)
{
	return MakeVectorRegisterInt(
		A.V[0] - B.V[0],
		A.V[1] - B.V[1],
		A.V[2] - B.V[2],
		A.V[3] - B.V[3]);
}

FORCEINLINE VectorRegisterInt VectorIntMultiply(const VectorRegisterInt& A, const VectorRegisterInt& B)
{
	return MakeVectorRegisterInt(
		A.V[0] * B.V[0],
		A.V[1] * B.V[1],
		A.V[2] * B.V[2],
		A.V[3] * B.V[3]);
}

FORCEINLINE VectorRegisterInt VectorIntNegate(const VectorRegisterInt& A)
{
	return MakeVectorRegisterInt(
		-A.V[0],
		-A.V[1],
		-A.V[2],
		-A.V[3]);
}

FORCEINLINE VectorRegisterInt VectorIntMin(const VectorRegisterInt& A, const VectorRegisterInt& B)
{
	return MakeVectorRegisterInt(
		FMath::Min(A.V[0] , B.V[0]),
		FMath::Min(A.V[1] , B.V[1]),
		FMath::Min(A.V[2] , B.V[2]),
		FMath::Min(A.V[3] , B.V[3]));
}

FORCEINLINE VectorRegisterInt VectorIntMax(const VectorRegisterInt& A, const VectorRegisterInt& B)
{
	return MakeVectorRegisterInt(
		FMath::Max(A.V[0], B.V[0]),
		FMath::Max(A.V[1], B.V[1]),
		FMath::Max(A.V[2], B.V[2]),
		FMath::Max(A.V[3], B.V[3]));
}

FORCEINLINE VectorRegisterInt VectorIntAbs(const VectorRegisterInt& A)
{
	return MakeVectorRegisterInt(
		FMath::Abs(A.V[0]),
		FMath::Abs(A.V[1]),
		FMath::Abs(A.V[2]),
		FMath::Abs(A.V[3]));
}

#define VectorIntSign(A) VectorIntSelect( VectorIntCompareGE(A, GlobalVectorConstants::IntZero), GlobalVectorConstants::IntOne, GlobalVectorConstants::IntMinusOne )

FORCEINLINE VectorRegister VectorIntToFloat(const VectorRegisterInt& A)
{
	return MakeVectorRegister(
		(float)A.V[0],
		(float)A.V[1],
		(float)A.V[2],
		(float)A.V[3]);
}

FORCEINLINE VectorRegisterInt VectorFloatToInt(const VectorRegister& A)
{
	return MakeVectorRegisterInt(
		(int32)A.V[0],
		(int32)A.V[1],
		(int32)A.V[2],
		(int32)A.V[3]);
}

//Loads and stores

/**
* Stores a vector to memory (aligned or unaligned).
*
* @param Vec	Vector to store
* @param Ptr	Memory pointer
*/
FORCEINLINE void VectorIntStore(const VectorRegisterInt& A, const void* Ptr)
{
	int32* IntPtr = (int32*)Ptr;	
	IntPtr[0] = A.V[0];
	IntPtr[1] = A.V[1];
	IntPtr[2] = A.V[2];
	IntPtr[3] = A.V[3];
}

/**
* Loads 4 int32s from unaligned memory.
*
* @param Ptr	Unaligned memory pointer to the 4 int32s
* @return		VectorRegisterInt(Ptr[0], Ptr[1], Ptr[2], Ptr[3])
*/

FORCEINLINE VectorRegisterInt VectorIntLoad(const void* Ptr)
{
	int32* IntPtr = (int32*)Ptr;
	return MakeVectorRegisterInt(
		IntPtr[0],
		IntPtr[1],
		IntPtr[2],
		IntPtr[3]);
}

/**
* Stores a vector to memory (aligned).
*
* @param Vec	Vector to store
* @param Ptr	Aligned Memory pointer
*/
FORCEINLINE void VectorIntStoreAligned(const VectorRegisterInt& A, const void* Ptr)
{
	int32* IntPtr = (int32*)Ptr;
	IntPtr[0] = A.V[0];
	IntPtr[1] = A.V[1];
	IntPtr[2] = A.V[2];
	IntPtr[3] = A.V[3];
}

/**
* Loads 4 int32s from aligned memory.
*
* @param Ptr	Aligned memory pointer to the 4 int32s
* @return		VectorRegisterInt(Ptr[0], Ptr[1], Ptr[2], Ptr[3])
*/
FORCEINLINE VectorRegisterInt VectorIntLoadAligned(const void* Ptr)
{
	int32* IntPtr = (int32*)Ptr;
	return MakeVectorRegisterInt(
		IntPtr[0],
		IntPtr[1],
		IntPtr[2],
		IntPtr[3]);
}

/**
* Loads 1 int32 from unaligned memory into all components of a vector register.
*
* @param Ptr	Unaligned memory pointer to the 4 int32s
* @return		VectorRegisterInt(*Ptr, *Ptr, *Ptr, *Ptr)
*/
FORCEINLINE VectorRegisterInt VectorIntLoad1(const void* Ptr)
{
	int32 IntSplat = *(int32*)Ptr;

	return MakeVectorRegisterInt(
		IntSplat,
		IntSplat,
		IntSplat,
		IntSplat);
}

// To be continued...


