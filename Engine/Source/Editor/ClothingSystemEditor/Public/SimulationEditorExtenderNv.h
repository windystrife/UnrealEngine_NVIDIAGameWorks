// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_NVCLOTH

#include "SimulationEditorExtender.h"
#include "Text.h"

class USkeletalMeshComponent;
class FPrimitiveDrawInterface;

struct FNvVisualizationOptions
{
	FNvVisualizationOptions();

	// FOptionData contains a localised name and other state for visualization menu options
	struct FOptionData
	{
		FOptionData()
			: bDisablesSimulation(false)
		{}

		// Localized name of the entry
		FText DisplayName;

		// Whether or not this option requires the simulation to be disabled
		bool bDisablesSimulation;
	};

	// List of options
	enum class EVisualizationOption : uint8
	{
		PhysMesh = 0,
		Normals,
		Collision,
		Backstop,
		MaxDistances,
		SelfCollision,
		Max
	};

	// Actual option entries
	FOptionData OptionData[(int32)EVisualizationOption::Max];

	// Flags determining which options are enabled
	bool Flags[(int32)EVisualizationOption::Max];

	// Is an option set
	bool IsSet(const EVisualizationOption InOption) const
	{
		return Flags[(int32)InOption];
	}

	// Toggle an option
	void Toggle(const EVisualizationOption InOption)
	{
		Flags[(int32)InOption] = !Flags[(int32)InOption];
	}

	// Whether or not - given the current enabled options - the simulation should be disabled
	bool ShouldDisableSimulation() const;
};

class FSimulationEditorExtenderNv : public ISimulationEditorExtender
{

public:

	// ISimulationEditorExtender Interface
	virtual UClass* GetSupportedSimulationFactoryClass() override;
	virtual void ExtendViewportShowMenu(FMenuBuilder& InMenuBuilder, TSharedRef<IPersonaPreviewScene> InPreviewScene) override;
	virtual void DebugDrawSimulation(const IClothingSimulation* InSimulation, USkeletalMeshComponent* InOwnerComponent, FPrimitiveDrawInterface* PDI) override;
	// End ISimulationEditorExtender Interface

private:

	// Handler for visualization entry being clicked
	void OnEntryClicked(FNvVisualizationOptions::EVisualizationOption InOption, TSharedRef<IPersonaPreviewScene> InPreviewScene);

	// Checkstate function for visualization entries
	bool IsEntryChecked(FNvVisualizationOptions::EVisualizationOption InOption) const;

	// Visualization options for NvCloth
	FNvVisualizationOptions VisulizationFlags;

};

#endif
