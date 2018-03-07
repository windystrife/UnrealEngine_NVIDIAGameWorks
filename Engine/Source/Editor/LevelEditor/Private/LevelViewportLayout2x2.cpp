// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LevelViewportLayout2x2.h"
#include "Framework/Docking/LayoutService.h"
#include "Editor.h"
#include "Misc/ConfigCacheIni.h"
#include "Modules/ModuleManager.h"
#include "Framework/Application/SlateApplication.h"
#include "LevelEditor.h"

namespace ViewportLayout2x2Defs
{
	/** Default 2x2 splitters to equal 50/50 splits */
	static const FVector2D DefaultSplitterPercentages(0.5f, 0.5f);
}

// FLevelViewportLayout2x2 //////////////////////////////////////////

TSharedRef<SWidget> FLevelViewportLayout2x2::MakeViewportLayout(const FString& LayoutString)
{
	FString SpecificLayoutString = GetTypeSpecificLayoutString(LayoutString);

	FString TopLeftKey, BottomLeftKey, TopRightKey, BottomRightKey;

	FString TopLeftType = TEXT("Default"), BottomLeftType = TEXT("Default"), TopRightType = TEXT("Default"), BottomRightType = TEXT("Default");

	TArray<FVector2D> SplitterPercentages;
	
	if (!SpecificLayoutString.IsEmpty())
	{
		// The Layout String only holds the unique ID of the Additional Layout Configs to use
		const FString& IniSection = FLayoutSaveRestore::GetAdditionalLayoutConfigIni();
		
		TopLeftKey = SpecificLayoutString + TEXT(".Viewport0");
		BottomLeftKey = SpecificLayoutString + TEXT(".Viewport1");
		TopRightKey = SpecificLayoutString + TEXT(".Viewport2");
		BottomRightKey = SpecificLayoutString + TEXT(".Viewport3");

		GConfig->GetString(*IniSection, *(TopLeftKey + TEXT(".TypeWithinLayout")), TopLeftType, GEditorPerProjectIni);
		GConfig->GetString(*IniSection, *(TopRightKey + TEXT(".TypeWithinLayout")), TopRightType, GEditorPerProjectIni);
		GConfig->GetString(*IniSection, *(BottomLeftKey + TEXT(".TypeWithinLayout")), BottomLeftType, GEditorPerProjectIni);
		GConfig->GetString(*IniSection, *(BottomRightKey + TEXT(".TypeWithinLayout")), BottomRightType, GEditorPerProjectIni);

		for (int32 i = 0; i < 4; ++i)
		{
			FString PercentageString;
			FVector2D NewPercentage = ViewportLayout2x2Defs::DefaultSplitterPercentages;
			if(GConfig->GetString(*IniSection, *(SpecificLayoutString + FString::Printf(TEXT(".Percentages%i"), i)), PercentageString, GEditorPerProjectIni))
			{
				NewPercentage.InitFromString(PercentageString);
			}						
			SplitterPercentages.Add(NewPercentage);
		}
	}

	FLevelEditorModule& LevelEditor = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");

	// Set up the viewports
	FViewportConstructionArgs Args;
	Args.ParentLayout = AsShared();
	Args.ParentLevelEditor = ParentLevelEditor;
	Args.IsEnabled = FSlateApplication::Get().GetNormalExecutionAttribute();

	// Left viewport
	Args.bRealtime = false;
	Args.ConfigKey = TopLeftKey;
	Args.ViewportType = LVT_OrthoYZ;
	TSharedPtr< IViewportLayoutEntity > ViewportTL = LevelEditor.FactoryViewport(*TopLeftType, Args);

	// Persp viewport
	Args.bRealtime = true;
	Args.ConfigKey = BottomLeftKey;
	Args.ViewportType = LVT_Perspective;
	TSharedPtr< IViewportLayoutEntity > ViewportBL = LevelEditor.FactoryViewport(*BottomLeftType, Args);

	// Front viewport
	Args.bRealtime = false;
	Args.ConfigKey = TopRightKey;
	Args.ViewportType = LVT_OrthoXZ;
	TSharedPtr< IViewportLayoutEntity > ViewportTR = LevelEditor.FactoryViewport(*TopRightType, Args);

	// Top Viewport
	Args.bRealtime = false;
	Args.ConfigKey = BottomRightKey;
	Args.ViewportType = LVT_OrthoXY;
	TSharedPtr< IViewportLayoutEntity > ViewportBR = LevelEditor.FactoryViewport(*BottomRightType, Args);

	Viewports.Add( *TopLeftKey, ViewportTL );
	Viewports.Add( *BottomLeftKey, ViewportBL );
	Viewports.Add( *TopRightKey, ViewportTR );
	Viewports.Add( *BottomRightKey, ViewportBR );

	// Set up the splitter
	SplitterWidget = 
	SNew( SSplitter2x2 )
	.TopLeft()
	[
		ViewportTL->AsWidget()
	]
	.BottomLeft()
	[
		ViewportBL->AsWidget()
	]
	.TopRight()
	[
		ViewportTR->AsWidget()
	]
	.BottomRight()
	[
		ViewportBR->AsWidget()
	];
	
	// Make newly-created perspective viewports active by default
	GCurrentLevelEditingViewportClient = &ViewportBL->GetLevelViewportClient();

	if (SplitterPercentages.Num() > 0)
	{
		SplitterWidget->SetSplitterPercentages(SplitterPercentages);
	}

	InitCommonLayoutFromString(SpecificLayoutString, *BottomLeftKey);

	return SplitterWidget.ToSharedRef();
}



void FLevelViewportLayout2x2::SaveLayoutString(const FString& LayoutString) const
{
	if (!bIsTransitioning)
	{
		FString SpecificLayoutString = GetTypeSpecificLayoutString(LayoutString);

		const FString& IniSection = FLayoutSaveRestore::GetAdditionalLayoutConfigIni();

		TArray<FVector2D> Percentages;
		SplitterWidget->GetSplitterPercentages(Percentages);
		for (int32 i = 0; i < Percentages.Num(); ++i)
		{
			GConfig->SetString(*IniSection, *(SpecificLayoutString + FString::Printf(TEXT(".Percentages%i"), i)), *Percentages[i].ToString(), GEditorPerProjectIni);
		}

		SaveCommonLayoutString(SpecificLayoutString);
	}
}


void FLevelViewportLayout2x2::ReplaceWidget( TSharedRef< SWidget > Source, TSharedRef< SWidget > Replacement )
{
	bool bWasFound = false;

	if( SplitterWidget->GetTopLeftContent() == Source )
	{
		SplitterWidget->SetTopLeftContent( Replacement );
		bWasFound = true;
	}

	else if( SplitterWidget->GetBottomLeftContent() == Source )
	{
		SplitterWidget->SetBottomLeftContent( Replacement );
		bWasFound = true;
	}

	else if( SplitterWidget->GetTopRightContent() == Source )
	{
		SplitterWidget->SetTopRightContent( Replacement );
		bWasFound = true;
	}

	else if( SplitterWidget->GetBottomRightContent() == Source )
	{
		SplitterWidget->SetBottomRightContent( Replacement );
		bWasFound = true;
	}

	// Source widget should have already been a content widget for the splitter
	check( bWasFound );
}
