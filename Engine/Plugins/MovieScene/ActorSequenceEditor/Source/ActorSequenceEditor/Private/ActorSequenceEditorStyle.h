// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SlateStyle.h"
#include "SlateStyleRegistry.h"


class FActorSequenceEditorStyle
	: public FSlateStyleSet
{
public:
	FActorSequenceEditorStyle()
		: FSlateStyleSet("ActorSequenceEditorStyle")
	{
		const FVector2D Icon16x16(16.0f, 16.0f);
		SetContentRoot(FPaths::EnginePluginsDir() / TEXT("MovieScene/ActorSequenceEditor/Content"));

		Set("ClassIcon.ActorSequence", new FSlateImageBrush(RootToContentDir(TEXT("ActorSequence_16x.png")), Icon16x16));
		Set("ClassIcon.ActorSequenceComponent", new FSlateImageBrush(RootToContentDir(TEXT("ActorSequence_16x.png")), Icon16x16));

		FSlateStyleRegistry::RegisterSlateStyle(*this);
	}

	static FActorSequenceEditorStyle& Get()
	{
		static FActorSequenceEditorStyle Inst;
		return Inst;
	}
	
	~FActorSequenceEditorStyle()
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*this);
	}
};
