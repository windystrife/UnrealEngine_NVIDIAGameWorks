// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

typedef uint16 FBoneIndexType;


struct FBoneIndexBase
{
	FBoneIndexBase() : BoneIndex(INDEX_NONE) {}

	FORCEINLINE int32 GetInt() const { return BoneIndex; }

	FORCEINLINE bool IsRootBone() const { return BoneIndex == 0; }

protected:
	int32 BoneIndex;
};

FORCEINLINE int32 GetIntFromComp(const int32 InComp)
{
	return InComp;
}

FORCEINLINE int32 GetIntFromComp(const FBoneIndexBase& InComp)
{
	return InComp.GetInt();
}

template<class RealBoneIndexType>
struct FBoneIndexWithOperators : public FBoneIndexBase
{
	// BoneIndexType
	FORCEINLINE bool operator==(const RealBoneIndexType& Rhs) const
	{
		return BoneIndex == GetIntFromComp(Rhs);
	}

	FORCEINLINE bool operator!=(const RealBoneIndexType& Rhs) const
	{
		return BoneIndex != GetIntFromComp(Rhs);
	}

	FORCEINLINE bool operator>(const RealBoneIndexType& Rhs) const
	{
		return BoneIndex > GetIntFromComp(Rhs);
	}

	FORCEINLINE bool operator>=(const RealBoneIndexType& Rhs) const
	{
		return BoneIndex >= GetIntFromComp(Rhs);
	}

	FORCEINLINE bool operator<(const RealBoneIndexType& Rhs) const
	{
		return BoneIndex < GetIntFromComp(Rhs);
	}

	FORCEINLINE bool operator<=(const RealBoneIndexType& Rhs) const
	{
		return BoneIndex <= GetIntFromComp(Rhs);
	}

	// FBoneIndexType
	FORCEINLINE bool operator==(const int32 Rhs) const
	{
		return BoneIndex == GetIntFromComp(Rhs);
	}

	FORCEINLINE bool operator!=(const int32 Rhs) const
	{
		return BoneIndex != GetIntFromComp(Rhs);
	}

	FORCEINLINE bool operator>(const int32 Rhs) const
	{
		return BoneIndex > GetIntFromComp(Rhs);
	}

	FORCEINLINE bool operator>=(const int32 Rhs) const
	{
		return BoneIndex >= GetIntFromComp(Rhs);
	}

	FORCEINLINE bool operator<(const int32 Rhs) const
	{
		return BoneIndex < GetIntFromComp(Rhs);
	}

	FORCEINLINE bool operator<=(const int32 Rhs) const
	{
		return BoneIndex <= GetIntFromComp(Rhs);
	}

	RealBoneIndexType& operator++()
	{
		++BoneIndex;
		return *((RealBoneIndexType*)this);
	}

	RealBoneIndexType& operator--()
	{
		--BoneIndex;
		return *((RealBoneIndexType*)this);
	}

	const RealBoneIndexType& operator=(const RealBoneIndexType& Rhs)
	{
		BoneIndex = Rhs.BoneIndex;
		return Rhs;
	}
};

struct FCompactPoseBoneIndex : public FBoneIndexWithOperators < FCompactPoseBoneIndex >
{
public:
	explicit FCompactPoseBoneIndex(int32 InBoneIndex) { BoneIndex = InBoneIndex; }
};

struct FMeshPoseBoneIndex : public FBoneIndexWithOperators < FMeshPoseBoneIndex >
{
public:
	explicit FMeshPoseBoneIndex(int32 InBoneIndex) { BoneIndex = InBoneIndex; }
};

struct FSkeletonPoseBoneIndex : public FBoneIndexWithOperators < FSkeletonPoseBoneIndex >
{
public:
	explicit FSkeletonPoseBoneIndex(int32 InBoneIndex) { BoneIndex = InBoneIndex; }
};
