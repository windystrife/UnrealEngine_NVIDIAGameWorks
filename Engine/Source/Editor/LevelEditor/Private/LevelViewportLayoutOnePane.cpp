// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LevelViewportLayoutOnePane.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SBoxPanel.h"
#include "Framework/Docking/LayoutService.h"
#include "ShowFlags.h"
#include "Editor.h"
#include "Misc/ConfigCacheIni.h"
#include "Modules/ModuleManager.h"
#include "Framework/Application/SlateApplication.h"
#include "LevelEditor.h"

// FLevelViewportLayoutOnePane /////////////////////////////

void FLevelViewportLayoutOnePane::SaveLayoutString(const FString& LayoutString) const
{
	if (!bIsTransitioning)
	{
		FString SpecificLayoutString = GetTypeSpecificLayoutString(LayoutString);

		SaveCommonLayoutString(SpecificLayoutString);
	}
}

TSharedRef<SWidget> FLevelViewportLayoutOnePane::MakeViewportLayout(const FString& LayoutString)
{
	// single viewport layout blocks maximize feature as it doesn't make sense
	bIsMaximizeSupported = false;

	FString SpecificLayoutString = GetTypeSpecificLayoutString(LayoutString);

	FEngineShowFlags OrthoShowFlags(ESFIM_Editor);
	ApplyViewMode(VMI_BrushWireframe, false, OrthoShowFlags);

	FEngineShowFlags PerspectiveShowFlags(ESFIM_Editor);
	ApplyViewMode(VMI_Lit, true, PerspectiveShowFlags);

	FString ViewportKey, ViewportType;
	if (!SpecificLayoutString.IsEmpty())
	{
		const FString& IniSection = FLayoutSaveRestore::GetAdditionalLayoutConfigIni();

		ViewportKey = SpecificLayoutString + TEXT(".Viewport0");
		GConfig->GetString(*IniSection, *(ViewportKey + TEXT(".TypeWithinLayout")), ViewportType, GEditorPerProjectIni);
	}

	FLevelEditorModule& LevelEditor = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");

	// Set up the viewport
	FViewportConstructionArgs Args;
	Args.ParentLayout = AsShared();
	Args.ParentLevelEditor = ParentLevelEditor;
	Args.IsEnabled = FSlateApplication::Get().GetNormalExecutionAttribute();
	Args.bRealtime = true;
	Args.ConfigKey = ViewportKey;
	Args.ViewportType = LVT_Perspective;
	TSharedRef<IViewportLayoutEntity> Viewport = LevelEditor.FactoryViewport(*ViewportType, Args);

	ViewportBox =
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		[
			Viewport->AsWidget()
		];

	Viewports.Add( *ViewportKey, Viewport );

	// Make newly-created perspective viewports active by default
	GCurrentLevelEditingViewportClient = &Viewport->GetLevelViewportClient();

	InitCommonLayoutFromString(SpecificLayoutString, NAME_None);

	return ViewportBox.ToSharedRef();
}

void FLevelViewportLayoutOnePane::ReplaceWidget(TSharedRef<SWidget> Source, TSharedRef<SWidget> Replacement)
{
	check(ViewportBox->GetChildren()->Num() == 1)
	TSharedRef<SWidget> ViewportWidget = ViewportBox->GetChildren()->GetChildAt(0);

	check(ViewportWidget == Source);
	ViewportBox->RemoveSlot(Source);
	ViewportBox->AddSlot()
		[
			Replacement
		];
}
