// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimationPins/SGraphPinPose.h"

/////////////////////////////////////////////////////
// SGraphPinPose

void SGraphPinPose::Construct(const FArguments& InArgs, UEdGraphPin* InPin)
{
	SGraphPin::Construct(SGraphPin::FArguments(), InPin);

	CachedImg_Pin_ConnectedHovered = FEditorStyle::GetBrush(TEXT("Graph.PosePin.ConnectedHovered"));
	CachedImg_Pin_Connected = FEditorStyle::GetBrush(TEXT("Graph.PosePin.Connected"));
	CachedImg_Pin_DisconnectedHovered = FEditorStyle::GetBrush(TEXT("Graph.PosePin.DisconnectedHovered"));
	CachedImg_Pin_Disconnected = FEditorStyle::GetBrush(TEXT("Graph.PosePin.Disconnected"));
}

const FSlateBrush* SGraphPinPose::GetPinIcon() const
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
