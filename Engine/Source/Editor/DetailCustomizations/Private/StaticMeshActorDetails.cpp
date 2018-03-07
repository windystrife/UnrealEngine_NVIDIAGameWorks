// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "StaticMeshActorDetails.h"
#include "Modules/ModuleManager.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Framework/Commands/UICommandList.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SComboButton.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "DetailCategoryBuilder.h"
#include "LevelEditor.h"
#include "LevelEditorActions.h"

#include "MeshUtilities.h"
#include "Engine/StaticMeshActor.h"
#include "Public/Widgets/Input/SButton.h"
#include "IMeshMergeUtilities.h"
#include "MeshMergeModule.h"
#include "Settings/EditorExperimentalSettings.h"

#define LOCTEXT_NAMESPACE "StaticMeshActorDetails"

TSharedRef<IDetailCustomization> FStaticMeshActorDetails::MakeInstance()
{
	return MakeShareable( new FStaticMeshActorDetails );
}

void FStaticMeshActorDetails::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{
	FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>( TEXT("LevelEditor") );

	const FLevelEditorCommands& Commands = LevelEditor.GetLevelEditorCommands();
	TSharedRef<const FUICommandList> CommandBindings = LevelEditor.GetGlobalLevelEditorActions();

	FMenuBuilder BlockingVolumeBuilder( true, CommandBindings );
	{
		BlockingVolumeBuilder.BeginSection("StaticMeshActorDetailsBlockingVolume");
		{
			BlockingVolumeBuilder.AddMenuEntry( Commands.CreateBoundingBoxVolume, NAME_None, LOCTEXT("CreateBlockingVolume", "Blocking Volume")  );
		}
		BlockingVolumeBuilder.EndSection();

		BlockingVolumeBuilder.BeginSection("StaticMeshActorDetailsBlockingVolume2");
		{
			BlockingVolumeBuilder.AddMenuEntry( Commands.CreateHeavyConvexVolume, NAME_None, LOCTEXT("CreateHeavyConvexVolume", "Heavy Convex Volume") );
			BlockingVolumeBuilder.AddMenuEntry( Commands.CreateNormalConvexVolume, NAME_None,LOCTEXT("CreateNormalConvexVolume", "Normal Convex Volume") );
			BlockingVolumeBuilder.AddMenuEntry( Commands.CreateLightConvexVolume, NAME_None, LOCTEXT("CreateLightConvexVolume", "Light Convex Volume") );
			BlockingVolumeBuilder.AddMenuEntry( Commands.CreateRoughConvexVolume, NAME_None, LOCTEXT("CreateRoughConvexVolume", "Rough Convex Volume") );
		}
		BlockingVolumeBuilder.EndSection();
	}

	IDetailCategoryBuilder& StaticMeshCategory = DetailBuilder.EditCategory( "StaticMesh" );

	// The blocking volume menu is advanced
	const bool bForAdvanced = true;

	const FText CreateBlockingVolumeString = LOCTEXT("BlockingVolumeMenu", "Create Blocking Volume");

	StaticMeshCategory.AddCustomRow( CreateBlockingVolumeString, bForAdvanced )
	.NameContent()
	[
		SNullWidget::NullWidget
	]
	.ValueContent()
	.VAlign(VAlign_Center)
	.MaxDesiredWidth(250)
	[
		SNew(SComboButton)
		.VAlign(VAlign_Center)
		.ToolTipText(LOCTEXT("CreateBlockingVolumeTooltip", "Creates a blocking volume from the static mesh"))
		.ButtonContent()
		[
			SNew( STextBlock )
			.Text( CreateBlockingVolumeString ) 
			.Font( IDetailLayoutBuilder::GetDetailFont() )
		]
		.MenuContent()
		[
			BlockingVolumeBuilder.MakeWidget()
		]
	];

	// If experimental feature is enabled addd a Bake Materials button to the SMA's details, this allows baking out the materials for the given instance data
	if (GetDefault<UEditorExperimentalSettings>()->bAssetMaterialBaking)
	{
		TArray<TWeakObjectPtr<UObject>> Objects;
		DetailBuilder.GetObjectsBeingCustomized(Objects);
		IDetailCategoryBuilder& MaterialsCategory = DetailBuilder.EditCategory("Materials");
		FDetailWidgetRow& ButtonRow = MaterialsCategory.AddCustomRow(LOCTEXT("RowLabel", "BakeMaterials"), true);
		ButtonRow.ValueWidget
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("BakeLabel", "Bake Materials"))
				.OnClicked_Lambda([Objects]() -> FReply
				{
					const IMeshMergeUtilities& MeshMergeUtilities = FModuleManager::Get().LoadModuleChecked<IMeshMergeModule>("MeshMergeUtilities").GetUtilities();

					// Retrieve all currently selected static mesh components
					TArray<UStaticMeshComponent*> StaticMeshComponents;
					for (TWeakObjectPtr<UObject> WeakObject : Objects)
					{
						if (WeakObject.IsValid())
						{
							UObject* Object = WeakObject.Get();
							if (AStaticMeshActor* Actor = Cast<AStaticMeshActor>(Object))
							{
								StaticMeshComponents.Add(Actor->GetStaticMeshComponent());
							}
						}
					}

					for (UStaticMeshComponent* Component : StaticMeshComponents)
					{
						MeshMergeUtilities.BakeMaterialsForComponent(Component);
					}
				
					return FReply::Handled();
				})
			]
		];		
	}
}

#undef LOCTEXT_NAMESPACE
