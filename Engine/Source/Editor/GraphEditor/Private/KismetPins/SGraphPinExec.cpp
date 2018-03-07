// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "KismetPins/SGraphPinExec.h"
#include "Widgets/Layout/SSpacer.h"

void SGraphPinExec::Construct(const FArguments& InArgs, UEdGraphPin* InPin)
{
	SGraphPin::Construct(SGraphPin::FArguments(), InPin);

	// Call utility function so inheritors can also call it since arguments can't be passed through
	CachePinIcons();
}

void SGraphPinExec::CachePinIcons()
{
	CachedImg_Pin_ConnectedHovered = FEditorStyle::GetBrush(TEXT("Graph.ExecPin.ConnectedHovered"));
	CachedImg_Pin_Connected = FEditorStyle::GetBrush(TEXT("Graph.ExecPin.Connected"));
	CachedImg_Pin_DisconnectedHovered = FEditorStyle::GetBrush(TEXT("Graph.ExecPin.DisconnectedHovered"));
	CachedImg_Pin_Disconnected = FEditorStyle::GetBrush(TEXT("Graph.ExecPin.Disconnected"));
}

TSharedRef<SWidget>	SGraphPinExec::GetDefaultValueWidget()
{
	return SNew(SSpacer); // not used for exec pin		
}

const FSlateBrush* SGraphPinExec::GetPinIcon() const
{
	const FSlateBrush* Brush = NULL;

	if (IsConnected())
	{
		Brush = IsHovered() ? CachedImg_Pin_ConnectedHovered : CachedImg_Pin_Connected;
	}
	else
	{
		Brush = IsHovered() ? CachedImg_Pin_DisconnectedHovered : CachedImg_Pin_Disconnected;
	}

	return Brush;
}
