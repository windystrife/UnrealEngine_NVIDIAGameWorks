// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BspModeModule.h"
#include "Modules/ModuleManager.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Textures/SlateIcon.h"
#include "EditorStyleSet.h"
#include "Engine/BrushBuilder.h"
#include "Builders/ConeBuilder.h"
#include "Builders/CubeBuilder.h"
#include "Builders/CurvedStairBuilder.h"
#include "Builders/CylinderBuilder.h"
#include "Builders/LinearStairBuilder.h"
#include "Builders/SpiralStairBuilder.h"
#include "Builders/TetrahedronBuilder.h"
#include "Widgets/SWidget.h"
#include "EditorModeRegistry.h"
#include "EditorModes.h"
#include "BspModeStyle.h"
#include "BspMode.h"
#include "SBspPalette.h"
#include "IPlacementModeModule.h"

#define LOCTEXT_NAMESPACE "BspMode"

void FBspModeModule::StartupModule()
{
	FBspModeStyle::Initialize();

	FEditorModeRegistry::Get().RegisterMode<FBspMode>(
		FBuiltinEditorModes::EM_Bsp,
		NSLOCTEXT("GeometryMode", "DisplayName", "Geometry Editing"),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.BspMode", "LevelEditor.BspMode.Small"),
		false,		// Visible
		100			// UI priority order
		);

	RegisterBspBuilderType(UCubeBuilder::StaticClass(), LOCTEXT("CubeBuilderName", "Box"), LOCTEXT("CubeBuilderToolTip", "Make a box brush"), FBspModeStyle::Get().GetBrush(TEXT("BspMode.BoxBrush")));
	RegisterBspBuilderType(UConeBuilder::StaticClass(), LOCTEXT("ConeBuilderName", "Cone"), LOCTEXT("ConeBuilderToolTip", "Make a cone brush"), FBspModeStyle::Get().GetBrush(TEXT("BspMode.ConeBrush")));
	RegisterBspBuilderType(UCylinderBuilder::StaticClass(), LOCTEXT("CylinderBuilderName", "Cylinder"), LOCTEXT("CylinderBuilderToolTip", "Make a cylinder brush"), FBspModeStyle::Get().GetBrush(TEXT("BspMode.CylinderBrush")));
	RegisterBspBuilderType(UCurvedStairBuilder::StaticClass(), LOCTEXT("CurvedStairBuilderName", "Curved Stair"), LOCTEXT("CurvedStairBuilderToolTip", "Make a curved stair brush"), FBspModeStyle::Get().GetBrush(TEXT("BspMode.CurvedStairBrush")));
	RegisterBspBuilderType(ULinearStairBuilder::StaticClass(), LOCTEXT("LinearStairBuilderName", "Linear Stair"), LOCTEXT("LinearStairBuilderToolTip", "Make a linear stair brush"), FBspModeStyle::Get().GetBrush(TEXT("BspMode.LinearStairBrush")));
	RegisterBspBuilderType(USpiralStairBuilder::StaticClass(), LOCTEXT("SpiralStairBuilderName", "Spiral Stair"), LOCTEXT("SpiralStairBuilderToolTip", "Make a spiral stair brush"), FBspModeStyle::Get().GetBrush(TEXT("BspMode.SpiralStairBrush")));
	RegisterBspBuilderType(UTetrahedronBuilder::StaticClass(), LOCTEXT("SphereBuilderName", "Sphere"), LOCTEXT("SphereBuilderToolTip", "Make a sphere brush"), FBspModeStyle::Get().GetBrush(TEXT("BspMode.SphereBrush")));

	IPlacementModeModule& PlacementModeModule = IPlacementModeModule::Get();

	FPlacementCategoryInfo CategoryInfo(LOCTEXT("PlacementMode_Geometry", "Geometry"), "Geometry", TEXT("PMGeometry"), 35);
	CategoryInfo.CustomGenerator = []() -> TSharedRef<SWidget> { return SNew(SBspPalette); };
	PlacementModeModule.RegisterPlacementCategory(CategoryInfo);
}


void FBspModeModule::ShutdownModule()
{
	FEditorModeRegistry::Get().UnregisterMode( FBuiltinEditorModes::EM_Bsp );

	BspBuilderTypes.Empty();
}


void FBspModeModule::RegisterBspBuilderType( class UClass* InBuilderClass, const FText& InBuilderName, const FText& InBuilderTooltip, const FSlateBrush* InBuilderIcon )
{
	check(InBuilderClass->IsChildOf(UBrushBuilder::StaticClass()));
	BspBuilderTypes.Add(MakeShareable(new FBspBuilderType(InBuilderClass, InBuilderName, InBuilderTooltip, InBuilderIcon)));
}


void FBspModeModule::UnregisterBspBuilderType( class UClass* InBuilderClass )
{
	BspBuilderTypes.RemoveAll( 
		[InBuilderClass] ( const TSharedPtr<FBspBuilderType>& RemovalCandidate ) -> bool
		{
			return (RemovalCandidate->BuilderClass == InBuilderClass);
		}
	);
}


const TArray< TSharedPtr<FBspBuilderType> >& FBspModeModule::GetBspBuilderTypes()
{
	return BspBuilderTypes;
}


TSharedPtr<FBspBuilderType> FBspModeModule::FindBspBuilderType(UClass* InBuilderClass) const
{
	const TSharedPtr<FBspBuilderType>* FoundBuilder = BspBuilderTypes.FindByPredicate(
		[InBuilderClass] ( const TSharedPtr<FBspBuilderType>& FindCandidate ) -> bool
		{
			return (FindCandidate->BuilderClass == InBuilderClass);
		}
	);

	return FoundBuilder != nullptr ? *FoundBuilder : TSharedPtr<FBspBuilderType>();
}

IMPLEMENT_MODULE( FBspModeModule, BspMode );

#undef LOCTEXT_NAMESPACE
