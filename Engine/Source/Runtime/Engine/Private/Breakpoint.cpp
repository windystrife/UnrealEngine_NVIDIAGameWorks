// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "Engine/Breakpoint.h"
#if WITH_EDITOR
#include "EdGraph/EdGraphNode.h"
#endif

UBreakpoint::UBreakpoint(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bEnabled = false;
	bStepOnce = false;
	bStepOnce_WasPreviouslyDisabled = false;
	bStepOnce_RemoveAfterHit = false;
}

FText UBreakpoint::GetLocationDescription() const
{
#if WITH_EDITORONLY_DATA
	if (Node != NULL)
	{
		FString Result
#if WITH_EDITOR
			= Node->GetDescriptiveCompiledName()
#endif
			;
		if (!Node->NodeComment.IsEmpty())
		{
			Result += TEXT(" // ");
			Result += Node->NodeComment;
		}

		return FText::FromString(Result);
	}
	else
	{
		return NSLOCTEXT("UBreakpoint", "ErrorInvalidLocation", "Error: Invalid location");
	}
#else	//#if WITH_EDITORONLY_DATA
	return NSLOCTEXT("UBreakpoint", "NoEditorData", "--- NO EDITOR DATA! ---");
#endif	//#if WITH_EDITORONLY_DATA
}
