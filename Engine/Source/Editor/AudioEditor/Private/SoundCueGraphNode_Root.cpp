// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SoundCueGraphNode_Root.cpp
=============================================================================*/

#include "SoundCueGraph/SoundCueGraphNode_Root.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "GraphEditorSettings.h"
#include "SoundCueGraphEditorCommands.h"

#define LOCTEXT_NAMESPACE "SoundCueGraphNode_Root"

/////////////////////////////////////////////////////
// USoundCueGraphNode_Root

USoundCueGraphNode_Root::USoundCueGraphNode_Root(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FLinearColor USoundCueGraphNode_Root::GetNodeTitleColor() const
{
	return GetDefault<UGraphEditorSettings>()->ResultNodeTitleColor;
}

FText USoundCueGraphNode_Root::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("RootTitle", "Output");
}

FText USoundCueGraphNode_Root::GetTooltipText() const
{
	return LOCTEXT("RootToolTip", "Wire the final Sound Node into this node");
}

void USoundCueGraphNode_Root::CreateInputPins()
{
	CreatePin(EGPD_Input, TEXT("SoundNode"), TEXT("Root"), nullptr, FString());
}

void USoundCueGraphNode_Root::GetContextMenuActions(const FGraphNodeContextMenuBuilder& Context) const
{
	if (Context.Pin)
	{
	}
	else if (Context.Node)
	{
		Context.MenuBuilder->BeginSection("SoundCueGraphNodePlay");
		{
			Context.MenuBuilder->AddMenuEntry(FSoundCueGraphEditorCommands::Get().PlayNode);
		}
		Context.MenuBuilder->EndSection();
	}
}

#undef LOCTEXT_NAMESPACE
