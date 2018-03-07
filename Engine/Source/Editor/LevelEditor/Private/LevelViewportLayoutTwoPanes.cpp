// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LevelViewportLayoutTwoPanes.h"
#include "Framework/Docking/LayoutService.h"
#include "Editor.h"
#include "Misc/ConfigCacheIni.h"
#include "Modules/ModuleManager.h"
#include "Framework/Application/SlateApplication.h"
#include "LevelEditor.h"

namespace ViewportLayoutTwoPanesDefs
{
	/** Default splitters to equal 50/50 split */
	static const float DefaultSplitterPercentage = 0.5f;
}


// TLevelViewportLayoutTwoPanes<T> /////////////////////////////

template <EOrientation TOrientation>
TSharedRef<SWidget> TLevelViewportLayoutTwoPanes<TOrientation>::MakeViewportLayout(const FString& LayoutString)
{
	FString SpecificLayoutString = GetTypeSpecificLayoutString(LayoutString);

	FString ViewportKey0, ViewportKey1;
	FString ViewportType0, ViewportType1;
	float SplitterPercentage = ViewportLayoutTwoPanesDefs::DefaultSplitterPercentage;

	if (!SpecificLayoutString.IsEmpty())
	{
		// The Layout String only holds the unique ID of the Additional Layout Configs to use
		const FString& IniSection = FLayoutSaveRestore::GetAdditionalLayoutConfigIni();

		ViewportKey0 = SpecificLayoutString + TEXT(".Viewport0");
		ViewportKey1 = SpecificLayoutString + TEXT(".Viewport1");

		GConfig->GetString(*IniSection, *(ViewportKey0 + TEXT(".TypeWithinLayout")), ViewportType0, GEditorPerProjectIni);
		GConfig->GetString(*IniSection, *(ViewportKey1 + TEXT(".TypeWithinLayout")), ViewportType1, GEditorPerProjectIni);

		FString PercentageString;
		if(GConfig->GetString(*IniSection, *(SpecificLayoutString + TEXT(".Percentage")), PercentageString, GEditorPerProjectIni))
		{
			TTypeFromString<float>::FromString(SplitterPercentage, *PercentageString);
		}
	}

	FLevelEditorModule& LevelEditor = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");

	// Set up the viewports
	FViewportConstructionArgs Args;
	Args.ParentLayout = AsShared();
	Args.ParentLevelEditor = ParentLevelEditor;
	Args.IsEnabled = FSlateApplication::Get().GetNormalExecutionAttribute();

	Args.bRealtime = false;
	Args.ConfigKey = ViewportKey0;
	Args.ViewportType = LVT_OrthoXY;
	TSharedRef<IViewportLayoutEntity> Viewport0 = LevelEditor.FactoryViewport(*ViewportType0, Args);

	Args.bRealtime = true;
	Args.ConfigKey = ViewportKey1;
	Args.ViewportType = LVT_Perspective;
	TSharedRef<IViewportLayoutEntity> Viewport1 = LevelEditor.FactoryViewport(*ViewportType1, Args);

	Viewports.Add( *ViewportKey0, Viewport0 );
	Viewports.Add( *ViewportKey1, Viewport1 );

	// Make newly-created perspective viewports active by default
	GCurrentLevelEditingViewportClient = &Viewport0->GetLevelViewportClient();

	SplitterWidget = 
		SNew( SSplitter )
		.Orientation(TOrientation)
		+SSplitter::Slot()
		.Value(SplitterPercentage)
		[
			Viewport0->AsWidget()
		]
	+SSplitter::Slot()
		.Value(1.0f - SplitterPercentage)
		[
			Viewport1->AsWidget()
		];

	InitCommonLayoutFromString(SpecificLayoutString, *ViewportKey1);

	return SplitterWidget.ToSharedRef();
}

template <EOrientation TOrientation>
void TLevelViewportLayoutTwoPanes<TOrientation>::SaveLayoutString(const FString& LayoutString) const
{
	if (!bIsTransitioning)
	{
		FString SpecificLayoutString = GetTypeSpecificLayoutString(LayoutString);

		const FString& IniSection = FLayoutSaveRestore::GetAdditionalLayoutConfigIni();

		check (SplitterWidget->GetChildren()->Num() == 2);
		float Percentage = SplitterWidget->SlotAt(0).SizeValue.Get();

		GConfig->SetString(*IniSection, *(SpecificLayoutString + TEXT(".Percentage")), *TTypeToString<float>::ToString(Percentage), GEditorPerProjectIni);

		SaveCommonLayoutString(SpecificLayoutString);
	}
}

template <EOrientation TOrientation>
void TLevelViewportLayoutTwoPanes<TOrientation>::ReplaceWidget( TSharedRef< SWidget > Source, TSharedRef< SWidget > Replacement )
{
	bool bWasFound = false;

	for (int32 SlotIdx = 0; SlotIdx < SplitterWidget->GetChildren()->Num(); SlotIdx++)
	{
		if  (SplitterWidget->GetChildren()->GetChildAt(SlotIdx) == Source)
		{
			SplitterWidget->SlotAt(SlotIdx)
				[
					Replacement
				];
			bWasFound = true;
			break;
		}
	}

	// Source widget should have already been a content widget for the splitter
	check( bWasFound );
}


/**
 * Function avoids linker errors on the template class functions in this cpp file.
 * It avoids the need to put the contents of this file into the header.
 * It doesn't get called.
 */
void ViewportLayoutTwoPanes_LinkErrorFixFunc()
{
	check(0);
	FLevelViewportLayoutTwoPanesVert DummyVert;
	FLevelViewportLayoutTwoPanesHoriz DummyHoriz;
}
