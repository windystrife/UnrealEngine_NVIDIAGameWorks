// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SimulationEditorExtenderNv.h"

#if WITH_NVCLOTH

#include "ClothingSimulation.h"
#include "ClothingSimulationNv.h"
#include "ClothingSimulationFactory.h"
#include "IPersonaPreviewScene.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "UIAction.h"
#include "MultiBoxBuilder.h"

#define LOCTEXT_NAMESPACE "NvSimEditorExtender"

FNvVisualizationOptions::FNvVisualizationOptions()
{
	FMemory::Memset(Flags, 0, (int32)EVisualizationOption::Max);

	OptionData[(int32)EVisualizationOption::PhysMesh].DisplayName = LOCTEXT("VisName_PhysMesh", "Physical Mesh");
	OptionData[(int32)EVisualizationOption::Normals].DisplayName = LOCTEXT("VisName_Normals", "Simulation Normals");
	OptionData[(int32)EVisualizationOption::Collision].DisplayName = LOCTEXT("VisName_Collision", "Collisions");
	OptionData[(int32)EVisualizationOption::Backstop].DisplayName = LOCTEXT("VisName_Backstop", "Backstops");
	OptionData[(int32)EVisualizationOption::MaxDistances].DisplayName = LOCTEXT("VisName_MaxDistance", "Max Distances");
	OptionData[(int32)EVisualizationOption::SelfCollision].DisplayName = LOCTEXT("VisName_SelfCollision", "Self Collision Radii");

	OptionData[(int32)EVisualizationOption::Backstop].bDisablesSimulation = true;
	OptionData[(int32)EVisualizationOption::MaxDistances].bDisablesSimulation = true;
}

bool FNvVisualizationOptions::ShouldDisableSimulation() const
{
	for(int32 OptionIndex = 0; OptionIndex < (int32)FNvVisualizationOptions::EVisualizationOption::Max; ++OptionIndex)
	{
		if(Flags[OptionIndex])
		{
			const FOptionData& Data = OptionData[OptionIndex];

			if(Data.bDisablesSimulation)
			{
				return true;
			}
		}
	}

	return false;
}

UClass* FSimulationEditorExtenderNv::GetSupportedSimulationFactoryClass()
{
	return UClothingSimulationFactoryNv::StaticClass();
}

void FSimulationEditorExtenderNv::ExtendViewportShowMenu(FMenuBuilder& InMenuBuilder, TSharedRef<IPersonaPreviewScene> InPreviewScene)
{
	InMenuBuilder.BeginSection(TEXT("NvSim_Visualizations"), LOCTEXT("VisSection", "Visualizations"));
	{
		for(int32 OptionIndex = 0; OptionIndex < (int32)FNvVisualizationOptions::EVisualizationOption::Max; ++OptionIndex)
		{
			FNvVisualizationOptions::EVisualizationOption Option = (FNvVisualizationOptions::EVisualizationOption)OptionIndex;

			FExecuteAction ExecuteAction = FExecuteAction::CreateRaw(this, &FSimulationEditorExtenderNv::OnEntryClicked, Option, InPreviewScene);
			FIsActionChecked IsActionChecked = FIsActionChecked::CreateRaw(this, &FSimulationEditorExtenderNv::IsEntryChecked, Option);

			FUIAction Action(ExecuteAction, FCanExecuteAction(), IsActionChecked);

			InMenuBuilder.AddMenuEntry(VisulizationFlags.OptionData[OptionIndex].DisplayName, FText::GetEmpty(), FSlateIcon(), Action, NAME_None, EUserInterfaceActionType::ToggleButton);
		}
	}
	InMenuBuilder.EndSection();
}

void FSimulationEditorExtenderNv::DebugDrawSimulation(const IClothingSimulation* InSimulation, USkeletalMeshComponent* InOwnerComponent, FPrimitiveDrawInterface* PDI)
{
	if(!InSimulation)
	{
		return;
	}

	const FClothingSimulationNv* NvSimulation = static_cast<const FClothingSimulationNv*>(InSimulation);

	for(int32 OptionIndex = 0; OptionIndex < (int32)FNvVisualizationOptions::EVisualizationOption::Max; ++OptionIndex)
	{
		FNvVisualizationOptions::EVisualizationOption Option = (FNvVisualizationOptions::EVisualizationOption)OptionIndex;

		if(VisulizationFlags.IsSet(Option))
		{
			switch(Option)
			{
				default:
					break;
				case FNvVisualizationOptions::EVisualizationOption::PhysMesh:
					NvSimulation->DebugDraw_PhysMesh(InOwnerComponent, PDI);
					break;
				case FNvVisualizationOptions::EVisualizationOption::Normals:
					NvSimulation->DebugDraw_Normals(InOwnerComponent, PDI);
					break;
				case FNvVisualizationOptions::EVisualizationOption::Collision:
					NvSimulation->DebugDraw_Collision(InOwnerComponent, PDI);
					break;
				case FNvVisualizationOptions::EVisualizationOption::Backstop:
					NvSimulation->DebugDraw_Backstops(InOwnerComponent, PDI);
					break;
				case FNvVisualizationOptions::EVisualizationOption::MaxDistances:
					NvSimulation->DebugDraw_MaxDistances(InOwnerComponent, PDI);
					break;
				case FNvVisualizationOptions::EVisualizationOption::SelfCollision:
					NvSimulation->DebugDraw_SelfCollision(InOwnerComponent, PDI);
					break;
			}
		}
	}
}

void FSimulationEditorExtenderNv::OnEntryClicked(FNvVisualizationOptions::EVisualizationOption InOption, TSharedRef<IPersonaPreviewScene> InPreviewScene)
{
	VisulizationFlags.Toggle(InOption);

	if(UDebugSkelMeshComponent* MeshComponent = InPreviewScene->GetPreviewMeshComponent())
	{
		bool bShouldDisableSim = VisulizationFlags.ShouldDisableSimulation();

		// If we need to toggle the disabled state, handle it
		if(bShouldDisableSim && MeshComponent->bDisableClothSimulation!= bShouldDisableSim)
		{
			MeshComponent->bDisableClothSimulation = !MeshComponent->bDisableClothSimulation;
		}
	}
}

bool FSimulationEditorExtenderNv::IsEntryChecked(FNvVisualizationOptions::EVisualizationOption InOption) const
{
	return VisulizationFlags.IsSet(InOption);
}

#undef LOCTEXT_NAMESPACE

#endif
