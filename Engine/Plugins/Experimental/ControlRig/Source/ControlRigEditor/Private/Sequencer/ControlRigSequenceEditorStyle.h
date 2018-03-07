// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SlateStyle.h"
#include "SlateStyleRegistry.h"


class FControlRigSequenceEditorStyle
	: public FSlateStyleSet
{
public:
	FControlRigSequenceEditorStyle()
		: FSlateStyleSet("ControlRigSequenceEditorStyle")
	{
		const FVector2D Icon16x16(16.0f, 16.0f);
		const FVector2D Icon20x20(20.0f, 20.0f);
		const FVector2D Icon24x24(24.0f, 24.0f);
		const FVector2D Icon40x40(40.0f, 40.0f);
		SetContentRoot(FPaths::EnginePluginsDir() / TEXT("Experimental/ControlRig/Content"));

		Set("ClassIcon.ControlRigSequence", new FSlateImageBrush(RootToContentDir(TEXT("ControlRigSequence_16x.png")), Icon16x16));

		Set("ControlRigEditMode", new FSlateImageBrush(RootToContentDir(TEXT("ControlRigMode_40x.png")), Icon40x40));
		Set("ControlRigEditMode.Small", new FSlateImageBrush(RootToContentDir(TEXT("ControlRigMode_40x.png")), Icon20x20));

		Set("ControlRig.ExportAnimSequence", new FSlateImageBrush(RootToContentDir(TEXT("ExportAnimSequence_24x.png")), Icon24x24));
		Set("ControlRig.ExportAnimSequence.Small", new FSlateImageBrush(RootToContentDir(TEXT("ExportAnimSequence_24x.png")), Icon24x24));
		Set("ControlRig.ReExportAnimSequence", new FSlateImageBrush(RootToContentDir(TEXT("ExportAnimSequence_24x.png")), Icon24x24));
		Set("ControlRig.ReExportAnimSequence.Small", new FSlateImageBrush(RootToContentDir(TEXT("ExportAnimSequence_24x.png")), Icon24x24));
		Set("ControlRig.ImportFromRigSequence", new FSlateImageBrush(RootToContentDir(TEXT("ReImportRigSequence_16x.png")), Icon16x16));
		Set("ControlRig.ImportFromRigSequence.Small", new FSlateImageBrush(RootToContentDir(TEXT("ReImportRigSequence_16x.png")), Icon16x16));
		Set("ControlRig.ReImportFromRigSequence", new FSlateImageBrush(RootToContentDir(TEXT("ReImportRigSequence_16x.png")), Icon16x16));
		Set("ControlRig.ReImportFromRigSequence.Small", new FSlateImageBrush(RootToContentDir(TEXT("ReImportRigSequence_16x.png")), Icon16x16));

		FSlateStyleRegistry::RegisterSlateStyle(*this);
	}

	static FControlRigSequenceEditorStyle& Get()
	{
		static FControlRigSequenceEditorStyle Inst;
		return Inst;
	}
	
	~FControlRigSequenceEditorStyle()
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*this);
	}
};
