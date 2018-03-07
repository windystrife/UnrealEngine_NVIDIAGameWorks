// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "K2Node_AIMoveTo.h"
#include "Blueprint/AIAsyncTaskBlueprintProxy.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "EditorCategoryUtils.h"

#define LOCTEXT_NAMESPACE "K2Node_AIMoveTo"

UK2Node_AIMoveTo::UK2Node_AIMoveTo(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ProxyFactoryFunctionName = GET_FUNCTION_NAME_CHECKED(UAIBlueprintHelperLibrary, CreateMoveToProxyObject);
	ProxyFactoryClass = UAIBlueprintHelperLibrary::StaticClass();
	ProxyClass = UAIAsyncTaskBlueprintProxy::StaticClass();
}

FText UK2Node_AIMoveTo::GetMenuCategory() const
{
	return FEditorCategoryUtils::GetCommonCategory(FCommonEditorCategory::AI);
}

FText UK2Node_AIMoveTo::GetTooltipText() const
{
	return LOCTEXT("AIMoveTo_Tooltip", "Simple order for Pawn with AIController to move to a specific location");
}

FText UK2Node_AIMoveTo::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("AIMoveTo", "AI MoveTo");
}

#undef LOCTEXT_NAMESPACE


