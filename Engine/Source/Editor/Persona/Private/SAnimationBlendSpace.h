// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "SAnimationBlendSpaceBase.h"
#include "AnimationBlendSpaceHelpers.h"

class SBlendSpaceEditor : public SBlendSpaceEditorBase
{
public:
	SLATE_BEGIN_ARGS(SBlendSpaceEditor)
		: _BlendSpace(NULL)			
		{}
		SLATE_ARGUMENT(UBlendSpace*, BlendSpace)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<class IPersonaPreviewScene>& InPreviewScene, FSimpleMulticastDelegate& OnPostUndo);
protected:
	virtual void ResampleData() override;

	/**
	* Triangle Generator
	 */
	FDelaunayTriangleGenerator Generator;
	/**
	 * Blend Space Grid to represent data
	 */
	FBlendSpaceGrid	BlendSpaceGrid;
};
