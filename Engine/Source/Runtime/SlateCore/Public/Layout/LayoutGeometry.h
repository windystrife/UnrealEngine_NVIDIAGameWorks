// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/SlateRect.h"
#include "Rendering/SlateLayoutTransform.h"

class FLayoutGeometry
{
public:
	FLayoutGeometry()
		: LocalSize(1.0f, 1.0f)
	{
	}

	explicit FLayoutGeometry(const FSlateLayoutTransform& InLocalToParent, const FVector2D& SizeInLocalSpace)
		: LocalToParent(InLocalToParent)
		, LocalSize(SizeInLocalSpace)
	{
	}

	const FSlateLayoutTransform& GetLocalToParentTransform() const 
	{ 
		return LocalToParent; 
	}

	const FVector2D& GetSizeInLocalSpace() const
	{
		return LocalSize;
	}

	FVector2D GetSizeInParentSpace() const
	{
		return TransformVector(LocalToParent, LocalSize);
	}

	FVector2D GetOffsetInParentSpace() const
	{
		return LocalToParent.GetTranslation();
	}

	FSlateRect GetRectInLocalSpace() const
	{
		return FSlateRect(FVector2D(0.0f, 0.0f), LocalSize);
	}

	FSlateRect GetRectInParentSpace() const
	{
		return TransformRect(LocalToParent, GetRectInLocalSpace());
	}

public:
	//FLayoutGeometry MakeChild(const FSlateLayoutTransform& ChildToThisTransform)
	//{
	//	return FLayoutGeometry(Concatenate(ChildToThisTransform, LocalToParent), LocalSize);
	//}

	//FLayoutGeometry MakeChild(const FSlateLayoutTransform& ChildToThisTransform, const FVector2D& SizeInChildSpace)
	//{
	//	return FLayoutGeometry(Concatenate(ChildToThisTransform, LocalToParent), SizeInChildSpace);
	//}

	//FLayoutGeometry MakeOffsetChild(const FVector2D& ChildOffset, const FVector2D& SizeInChildSpace)
	//{
	//	return FLayoutGeometry(Concatenate(ChildOffset, LocalToParent), SizeInChildSpace);
	//}

	//FLayoutGeometry MakeInflatedChild(const FVector2D& InflateAmount)
	//{
	//	return FLayoutGeometry(Concatenate(Inverse(InflateAmount), LocalToParent), LocalSize + 2.0f*InflateAmount);
	//}

private:
	FSlateLayoutTransform LocalToParent;
	FVector2D LocalSize;
};

