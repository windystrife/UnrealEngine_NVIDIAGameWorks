// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphNode_RandomPlayer.h"

#include "EditorCategoryUtils.h"

#define LOCTEXT_NAMESPACE "AnimGraphNode_RandomPlayer"

FLinearColor UAnimGraphNode_RandomPlayer::GetNodeTitleColor() const
{
	return FLinearColor(0.10f, 0.60f, 0.12f);
}

FText UAnimGraphNode_RandomPlayer::GetTooltipText() const
{
	return LOCTEXT("NodeToolTip", "Plays sequences picked from a provided list in random orders.");
}

FText UAnimGraphNode_RandomPlayer::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("NodeTitle", "Random Sequence Player");
}

FText UAnimGraphNode_RandomPlayer::GetMenuCategory() const
{
	return FEditorCategoryUtils::GetCommonCategory(FCommonEditorCategory::Animation);
}

#undef LOCTEXT_NAMESPACE
