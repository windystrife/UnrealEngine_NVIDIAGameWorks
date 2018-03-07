// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Box.cpp: Implements the FBox class.
=============================================================================*/

#include "Math/Box.h"
#include "Math/Vector4.h"
#include "Math/VectorRegister.h"
#include "Math/Transform.h"


/* FBox structors
 *****************************************************************************/

FBox::FBox(const FVector* Points, int32 Count)
	: Min(0, 0, 0)
	, Max(0, 0, 0)
	, IsValid(0)
{
	for (int32 i = 0; i < Count; i++)
	{
		*this += Points[i];
	}
}


FBox::FBox(const TArray<FVector>& Points)
	: Min(0, 0, 0)
	, Max(0, 0, 0)
	, IsValid(0)
{
	for (int32 i = 0; i < Points.Num(); ++i)
	{
		*this += Points[i];
	}
}


/* FBox interface
 *****************************************************************************/

FBox FBox::TransformBy(const FMatrix& M) const
{
	// if we are not valid, return another invalid box.
	if (!IsValid)
	{
		return FBox(ForceInit);
	}

	FBox NewBox;

	const VectorRegister VecMin = VectorLoadFloat3(&Min);
	const VectorRegister VecMax = VectorLoadFloat3(&Max);

	const VectorRegister m0 = VectorLoadAligned(M.M[0]);
	const VectorRegister m1 = VectorLoadAligned(M.M[1]);
	const VectorRegister m2 = VectorLoadAligned(M.M[2]);
	const VectorRegister m3 = VectorLoadAligned(M.M[3]);

	const VectorRegister Half = VectorSetFloat3(0.5f, 0.5f, 0.5f);
	const VectorRegister Origin = VectorMultiply(VectorAdd(VecMax, VecMin), Half);
	const VectorRegister Extent = VectorMultiply(VectorSubtract(VecMax, VecMin), Half);

	VectorRegister NewOrigin = VectorMultiply(VectorReplicate(Origin, 0), m0);
	NewOrigin = VectorMultiplyAdd(VectorReplicate(Origin, 1), m1, NewOrigin);
	NewOrigin = VectorMultiplyAdd(VectorReplicate(Origin, 2), m2, NewOrigin);
	NewOrigin = VectorAdd(NewOrigin, m3);

	VectorRegister NewExtent = VectorAbs(VectorMultiply(VectorReplicate(Extent, 0), m0));
	NewExtent = VectorAdd(NewExtent, VectorAbs(VectorMultiply(VectorReplicate(Extent, 1), m1)));
	NewExtent = VectorAdd(NewExtent, VectorAbs(VectorMultiply(VectorReplicate(Extent, 2), m2)));

	const VectorRegister NewVecMin = VectorSubtract(NewOrigin, NewExtent);
	const VectorRegister NewVecMax = VectorAdd(NewOrigin, NewExtent);
	
	VectorStoreFloat3(NewVecMin, &NewBox.Min );
	VectorStoreFloat3(NewVecMax, &NewBox.Max );
	
	NewBox.IsValid = 1;

	return NewBox;
}

FBox FBox::TransformBy(const FTransform& M) const
{
	return TransformBy(M.ToMatrixWithScale());
}

FBox FBox::InverseTransformBy(const FTransform& M) const
{
	FVector Vertices[8] = 
	{
		FVector(Min),
		FVector(Min.X, Min.Y, Max.Z),
		FVector(Min.X, Max.Y, Min.Z),
		FVector(Max.X, Min.Y, Min.Z),
		FVector(Max.X, Max.Y, Min.Z),
		FVector(Max.X, Min.Y, Max.Z),
		FVector(Min.X, Max.Y, Max.Z),
		FVector(Max)
	};

	FBox NewBox(ForceInit);

	for (int32 VertexIndex = 0; VertexIndex < ARRAY_COUNT(Vertices); VertexIndex++)
	{
		FVector4 ProjectedVertex = M.InverseTransformPosition(Vertices[VertexIndex]);
		NewBox += ProjectedVertex;
	}

	return NewBox;
}


FBox FBox::TransformProjectBy(const FMatrix& ProjM) const
{
	FVector Vertices[8] = 
	{
		FVector(Min),
		FVector(Min.X, Min.Y, Max.Z),
		FVector(Min.X, Max.Y, Min.Z),
		FVector(Max.X, Min.Y, Min.Z),
		FVector(Max.X, Max.Y, Min.Z),
		FVector(Max.X, Min.Y, Max.Z),
		FVector(Min.X, Max.Y, Max.Z),
		FVector(Max)
	};

	FBox NewBox(ForceInit);

	for (int32 VertexIndex = 0; VertexIndex < ARRAY_COUNT(Vertices); VertexIndex++)
	{
		FVector4 ProjectedVertex = ProjM.TransformPosition(Vertices[VertexIndex]);
		NewBox += ((FVector)ProjectedVertex) / ProjectedVertex.W;
	}

	return NewBox;
}

FBox FBox::Overlap( const FBox& Other ) const
{
	if(Intersect(Other) == false)
	{
		static FBox EmptyBox(ForceInit);
		return EmptyBox;
	}

	// otherwise they overlap
	// so find overlapping box
	FVector MinVector, MaxVector;

	MinVector.X = FMath::Max(Min.X, Other.Min.X);
	MaxVector.X = FMath::Min(Max.X, Other.Max.X);

	MinVector.Y = FMath::Max(Min.Y, Other.Min.Y);
	MaxVector.Y = FMath::Min(Max.Y, Other.Max.Y);

	MinVector.Z = FMath::Max(Min.Z, Other.Min.Z);
	MaxVector.Z = FMath::Min(Max.Z, Other.Max.Z);

	return FBox(MinVector, MaxVector);
}
