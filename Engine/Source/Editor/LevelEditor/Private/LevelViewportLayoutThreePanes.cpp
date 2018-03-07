// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LevelViewportLayoutThreePanes.h"
#include "Framework/Docking/LayoutService.h"
#include "ShowFlags.h"
#include "Misc/ConfigCacheIni.h"
#include "Modules/ModuleManager.h"
#include "Framework/Application/SlateApplication.h"
#include "LevelEditor.h"

namespace ViewportLayoutThreePanesDefs
{
	/** Default splitters to equal 50/50 split */
	static const float DefaultSplitterPercentage = 0.5f;
}


// FLevelViewportLayoutThreePanes /////////////////////////////

TSharedRef<SWidget> FLevelViewportLayoutThreePanes::MakeViewportLayout(const FString& LayoutString)
{
	FString SpecificLayoutString = GetTypeSpecificLayoutString(LayoutString);

	FEngineShowFlags OrthoShowFlags(ESFIM_Editor);	
	ApplyViewMode(VMI_BrushWireframe, false, OrthoShowFlags);

	FEngineShowFlags PerspectiveShowFlags(ESFIM_Editor);	
	ApplyViewMode(VMI_Lit, true, PerspectiveShowFlags);

	FString ViewportKey0, ViewportKey1, ViewportKey2;
	FString ViewportType0 = TEXT("Default"), ViewportType1 = TEXT("Default"), ViewportType2 = TEXT("Default");
	float PrimarySplitterPercentage = ViewportLayoutThreePanesDefs::DefaultSplitterPercentage;
	float SecondarySplitterPercentage = ViewportLayoutThreePanesDefs::DefaultSplitterPercentage;
	
	if (!SpecificLayoutString.IsEmpty())
	{
		// The Layout String only holds the unique ID of the Additional Layout Configs to use
		const FString& IniSection = FLayoutSaveRestore::GetAdditionalLayoutConfigIni();
		
		ViewportKey0 = SpecificLayoutString + TEXT(".Viewport0");
		ViewportKey1 = SpecificLayoutString + TEXT(".Viewport1");
		ViewportKey2 = SpecificLayoutString + TEXT(".Viewport2");

		GConfig->GetString(*IniSection, *(ViewportKey0 + TEXT(".TypeWithinLayout")), ViewportType0, GEditorPerProjectIni);
		GConfig->GetString(*IniSection, *(ViewportKey1 + TEXT(".TypeWithinLayout")), ViewportType1, GEditorPerProjectIni);
		GConfig->GetString(*IniSection, *(ViewportKey2 + TEXT(".TypeWithinLayout")), ViewportType2, GEditorPerProjectIni);

		FString PercentageString;
		if(GConfig->GetString(*IniSection, *(SpecificLayoutString + TEXT(".Percentage0")), PercentageString, GEditorPerProjectIni))
		{
			TTypeFromString<float>::FromString(PrimarySplitterPercentage, *PercentageString);
		}		
		if(GConfig->GetString(*IniSection, *(SpecificLayoutString + TEXT(".Percentage1")), PercentageString, GEditorPerProjectIni))
		{
			TTypeFromString<float>::FromString(SecondarySplitterPercentage, *PercentageString);
		}	
	}

	FLevelEditorModule& LevelEditor = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");

	// Set up the viewports
	FViewportConstructionArgs Args;
	Args.ParentLayout = AsShared();
	Args.ParentLevelEditor = ParentLevelEditor;
	Args.IsEnabled = FSlateApplication::Get().GetNormalExecutionAttribute();

	Args.bRealtime = true;
	Args.ConfigKey = ViewportKey0;
	Args.ViewportType = LVT_Perspective;
	TSharedRef<IViewportLayoutEntity> Viewport0 = LevelEditor.FactoryViewport(*ViewportType0, Args);

	Args.bRealtime = false;
	Args.ConfigKey = ViewportKey1;
	Args.ViewportType = LVT_OrthoXY;
	TSharedRef<IViewportLayoutEntity> Viewport1 = LevelEditor.FactoryViewport(*ViewportType1, Args);

	// Front viewport
	Args.bRealtime = false;
	Args.ConfigKey = ViewportKey2;
	Args.ViewportType = LVT_OrthoXZ;
	TSharedRef<IViewportLayoutEntity> Viewport2 = LevelEditor.FactoryViewport(*ViewportType2, Args);

	Viewports.Add( *ViewportKey0, Viewport0 );
	Viewports.Add( *ViewportKey1, Viewport1 );
	Viewports.Add( *ViewportKey2, Viewport2 );

	TSharedRef<SWidget> LayoutWidget = MakeThreePanelWidget(
		Viewports,
		Viewport0->AsWidget(), Viewport1->AsWidget(), Viewport2->AsWidget(),
		PrimarySplitterPercentage, SecondarySplitterPercentage);

	InitCommonLayoutFromString(SpecificLayoutString, *ViewportKey1);

	return LayoutWidget;
}

void FLevelViewportLayoutThreePanes::SaveLayoutString(const FString& LayoutString) const
{
	if (!bIsTransitioning)
	{
		FString SpecificLayoutString = GetTypeSpecificLayoutString(LayoutString);

		const FString& IniSection = FLayoutSaveRestore::GetAdditionalLayoutConfigIni();

		check (PrimarySplitterWidget->GetChildren()->Num() == 2);
		float PrimaryPercentage = PrimarySplitterWidget->SlotAt(0).SizeValue.Get();
		check (SecondarySplitterWidget->GetChildren()->Num() == 2);
		float SecondaryPercentage = SecondarySplitterWidget->SlotAt(0).SizeValue.Get();

		GConfig->SetString(*IniSection, *(SpecificLayoutString + TEXT(".Percentage0")), *TTypeToString<float>::ToString(PrimaryPercentage), GEditorPerProjectIni);
		GConfig->SetString(*IniSection, *(SpecificLayoutString + TEXT(".Percentage1")), *TTypeToString<float>::ToString(SecondaryPercentage), GEditorPerProjectIni);

		SaveCommonLayoutString(SpecificLayoutString);
	}
}

void FLevelViewportLayoutThreePanes::ReplaceWidget( TSharedRef< SWidget > Source, TSharedRef< SWidget > Replacement )
{
	bool bWasFound = false;

	for (int32 SlotIdx = 0; SlotIdx < PrimarySplitterWidget->GetChildren()->Num(); SlotIdx++)
	{
		if  (PrimarySplitterWidget->GetChildren()->GetChildAt(SlotIdx) == Source)
		{
			PrimarySplitterWidget->SlotAt(SlotIdx)
			[
				Replacement
			];
			bWasFound = true;
			break;
		}
	}

	for (int32 SlotIdx = 0; SlotIdx < SecondarySplitterWidget->GetChildren()->Num(); SlotIdx++)
	{
		if  (SecondarySplitterWidget->GetChildren()->GetChildAt(SlotIdx) == Source)
		{
			SecondarySplitterWidget->SlotAt(SlotIdx)
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


// FLevelViewportLayoutThreePanesLeft /////////////////////////////

TSharedRef<SWidget> FLevelViewportLayoutThreePanesLeft::MakeThreePanelWidget(
	TMap< FName, TSharedPtr< IViewportLayoutEntity >>& ViewportWidgets,
	const TSharedRef<SWidget>& Viewport0, const TSharedRef<SWidget>& Viewport1, const TSharedRef<SWidget>& Viewport2,
	float PrimarySplitterPercentage, float SecondarySplitterPercentage)
{
	TSharedRef<SWidget> Widget = 
		SAssignNew( PrimarySplitterWidget, SSplitter )
		.Orientation(EOrientation::Orient_Horizontal)
		+SSplitter::Slot()
		.Value(PrimarySplitterPercentage)
		[
			Viewport0
		]
		+SSplitter::Slot()
		.Value(1.0f - PrimarySplitterPercentage)
		[
			SAssignNew( SecondarySplitterWidget, SSplitter )
			.Orientation(EOrientation::Orient_Vertical)
			+SSplitter::Slot()
			.Value(SecondarySplitterPercentage)
			[
				Viewport1
			]
			+SSplitter::Slot()
			.Value(1.0f - SecondarySplitterPercentage)
			[
				Viewport2
			]
		];

	return Widget;
}


// FLevelViewportLayoutThreePanesRight /////////////////////////////

TSharedRef<SWidget> FLevelViewportLayoutThreePanesRight::MakeThreePanelWidget(
	TMap< FName, TSharedPtr< IViewportLayoutEntity >>& ViewportWidgets,
	const TSharedRef<SWidget>& Viewport0, const TSharedRef<SWidget>& Viewport1, const TSharedRef<SWidget>& Viewport2,
	float PrimarySplitterPercentage, float SecondarySplitterPercentage)
{
	TSharedRef<SWidget> Widget = 
		SAssignNew( PrimarySplitterWidget, SSplitter )
		.Orientation(EOrientation::Orient_Horizontal)
		+SSplitter::Slot()
		.Value(PrimarySplitterPercentage)
		[
			SAssignNew( SecondarySplitterWidget, SSplitter )
			.Orientation(EOrientation::Orient_Vertical)
			+SSplitter::Slot()
			.Value(SecondarySplitterPercentage)
			[
				Viewport1
			]
			+SSplitter::Slot()
			.Value(1.0f - SecondarySplitterPercentage)
			[
				Viewport2
			]
		]
		+SSplitter::Slot()
		.Value(1.0f - PrimarySplitterPercentage)
		[
			Viewport0
		];

	return Widget;
}


// FLevelViewportLayoutThreePanesTop /////////////////////////////

TSharedRef<SWidget> FLevelViewportLayoutThreePanesTop::MakeThreePanelWidget(
	TMap< FName, TSharedPtr< IViewportLayoutEntity >>& ViewportWidgets,
	const TSharedRef<SWidget>& Viewport0, const TSharedRef<SWidget>& Viewport1, const TSharedRef<SWidget>& Viewport2,
	float PrimarySplitterPercentage, float SecondarySplitterPercentage)
{
	TSharedRef<SWidget> Widget = 
		SAssignNew( PrimarySplitterWidget, SSplitter )
		.Orientation(EOrientation::Orient_Vertical)
		+SSplitter::Slot()
		.Value(PrimarySplitterPercentage)
		[
			Viewport0
		]
		+SSplitter::Slot()
		.Value(1.0f - PrimarySplitterPercentage)
		[
			SAssignNew( SecondarySplitterWidget, SSplitter )
			.Orientation(EOrientation::Orient_Horizontal)
			+SSplitter::Slot()
			.Value(SecondarySplitterPercentage)
			[
				Viewport1
			]
			+SSplitter::Slot()
			.Value(1.0f - SecondarySplitterPercentage)
			[
				Viewport2
			]
		];

	return Widget;
}


// FLevelViewportLayoutThreePanesBottom /////////////////////////////

TSharedRef<SWidget> FLevelViewportLayoutThreePanesBottom::MakeThreePanelWidget(
	TMap< FName, TSharedPtr< IViewportLayoutEntity >>& ViewportWidgets,
	const TSharedRef<SWidget>& Viewport0, const TSharedRef<SWidget>& Viewport1, const TSharedRef<SWidget>& Viewport2,
	float PrimarySplitterPercentage, float SecondarySplitterPercentage)
{
	TSharedRef<SWidget> Widget = 
		SAssignNew( PrimarySplitterWidget, SSplitter )
		.Orientation(EOrientation::Orient_Vertical)
		+SSplitter::Slot()
		.Value(PrimarySplitterPercentage)
		[
			SAssignNew( SecondarySplitterWidget, SSplitter )
			.Orientation(EOrientation::Orient_Horizontal)
			+SSplitter::Slot()
			.Value(SecondarySplitterPercentage)
			[
				Viewport1
			]
			+SSplitter::Slot()
			.Value(1.0f - SecondarySplitterPercentage)
			[
				Viewport2
			]
		]
		+SSplitter::Slot()
		.Value(1.0f - PrimarySplitterPercentage)
		[
			Viewport0
		];

	return Widget;
}
