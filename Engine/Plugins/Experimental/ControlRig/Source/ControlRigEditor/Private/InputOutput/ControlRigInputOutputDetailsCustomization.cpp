// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ControlRigInputOutputDetailsCustomization.h"
#include "K2Node_ControlRig.h"
#include "ControlRig.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "Containers/Algo/Transform.h"
#include "ControlRigField.h"
#include "HierarchicalRig.h"
#include "STextBlock.h"
#include "SCheckBox.h"

#define LOCTEXT_NAMESPACE "ControlRigInputOutputDetailsCustomization"

TSharedRef<IDetailCustomization> FControlRigInputOutputDetailsCustomization::MakeInstance()
{
	return MakeShareable(new FControlRigInputOutputDetailsCustomization());
}

void FControlRigInputOutputDetailsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	DetailLayout.HideProperty(GET_MEMBER_NAME_CHECKED(UK2Node_ControlRig, LabeledInputs));
	DetailLayout.HideProperty(GET_MEMBER_NAME_CHECKED(UK2Node_ControlRig, LabeledOutputs));

	TArray<TWeakObjectPtr<UObject>> EditingObjects;
	DetailLayout.GetObjectsBeingCustomized(EditingObjects);

	Algo::TransformIf(EditingObjects, ControlRigs, [](TWeakObjectPtr<UObject> WeakObjectPtr) { return WeakObjectPtr.IsValid() && WeakObjectPtr.Get()->IsA<UK2Node_ControlRig>(); }, [](TWeakObjectPtr<UObject> WeakObjectPtr) { return Cast<UK2Node_ControlRig>(WeakObjectPtr.Get()); });

	TSharedPtr<IPropertyHandle> DisabledInputsProperty = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UK2Node_ControlRig, DisabledInputs));
	TSharedPtr<IPropertyHandle> DisabledOutputsProperty = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UK2Node_ControlRig, DisabledOutputs));

	DetailLayout.HideProperty(DisabledInputsProperty);

	bool bHasHierarchicalData = false;
	bShowInputs = false;
	bShowOutputs = false;

	TMap<FName, TSharedRef<IControlRigField>> Inputs;
	for (const TWeakObjectPtr<UK2Node_ControlRig>& ControlRigPtr : ControlRigs)
	{
		UK2Node_ControlRig* ControlRig = ControlRigPtr.Get();
		bShowInputs |= ControlRig->HasInputs();

		// get all valid inputs for these ControlRigs
		TArray<TSharedRef<IControlRigField>> InputInfos = ControlRig->GetInputVariableInfo();
		for (const TSharedRef<IControlRigField>& InputInfo : InputInfos)
		{
			Inputs.Add(InputInfo->GetName(), InputInfo);
		}

		// Check if we have a hierarchy ControlRig
		if (UClass* Class = ControlRig->GetControlRigClass())
		{
			if (Class->IsChildOf(UHierarchicalRig::StaticClass()) || Class->HasMetaData(TEXT("UsesHierarchy")))
			{
				bHasHierarchicalData = true;
			}
		}
	}

	if (Inputs.Num() > 0)
	{
		IDetailCategoryBuilder& InputCategoryBuilder = DetailLayout.EditCategory("Inputs");

		for (const TPair<FName, TSharedRef<IControlRigField>>& Input : Inputs)
		{
			if(Input.Value->CanBeDisabled())
			{
				FText TooltipText = FText::Format(LOCTEXT("InputTooltipFormat", "Enable or disable the {0} input pin on this node."), Input.Value->GetDisplayNameText());

				InputCategoryBuilder.AddCustomRow(Input.Value->GetDisplayNameText())
				.NameContent()
				[
					SNew(STextBlock)
					.Font(DetailLayout.GetDetailFont())
					.Text(Input.Value->GetDisplayNameText())
					.ToolTipText(Input.Value->GetField()->GetToolTipText())
				]
				.ValueContent()
				[
					SNew(SCheckBox)
					.IsChecked_Raw(this, &FControlRigInputOutputDetailsCustomization::IsAnimationPinChecked, Input.Key, true)
					.OnCheckStateChanged_Raw(this, &FControlRigInputOutputDetailsCustomization::HandleAnimationPinCheckStateChanged, Input.Key, true)
					.ToolTipText(TooltipText)
				];
			}
		}
	}

	DetailLayout.HideProperty(DisabledOutputsProperty);

	// get all valid Outputs for these ControlRigs
	TMap<FName, TSharedRef<IControlRigField>> Outputs;
	for (const TWeakObjectPtr<UK2Node_ControlRig>& ControlRigPtr : ControlRigs)
	{
		UK2Node_ControlRig* ControlRig = ControlRigPtr.Get();
		bShowOutputs |= ControlRig->HasOutputs();

		TArray<TSharedRef<IControlRigField>> OutputInfos = ControlRig->GetOutputVariableInfo();
		for (const TSharedRef<IControlRigField>& OutputInfo : OutputInfos)
		{
			Outputs.Add(OutputInfo->GetName(), OutputInfo);
		}
	}

	if (Outputs.Num() > 0)
	{
		IDetailCategoryBuilder& OutputCategoryBuilder = DetailLayout.EditCategory("Outputs");

		for (const TPair<FName, TSharedRef<IControlRigField>>& Output : Outputs)
		{
			if (Output.Value->CanBeDisabled())
			{
				FText TooltipText = FText::Format(LOCTEXT("OutputTooltipFormat", "Enable or disable the {0} output pin on this node."), Output.Value->GetDisplayNameText());

				OutputCategoryBuilder.AddCustomRow(Output.Value->GetDisplayNameText())
				.NameContent()
				[
					SNew(STextBlock)
					.Font(DetailLayout.GetDetailFont())
					.Text(Output.Value->GetDisplayNameText())
					.ToolTipText(Output.Value->GetField()->GetToolTipText())
				]
				.ValueContent()
				[
					SNew(SCheckBox)
					.IsChecked_Raw(this, &FControlRigInputOutputDetailsCustomization::IsAnimationPinChecked, Output.Key, false)
					.OnCheckStateChanged_Raw(this, &FControlRigInputOutputDetailsCustomization::HandleAnimationPinCheckStateChanged, Output.Key, false)
					.ToolTipText(TooltipText)
				];
			}
		}
	}

	if (bHasHierarchicalData)
	{
		CustomizeHierarchicalDetails(DetailLayout);
	}
}

ECheckBoxState FControlRigInputOutputDetailsCustomization::IsAnimationPinChecked(FName InName, bool bInput) const
{
	bool bPinDisabled = false;
	bool bPinEnabled = false;
	for (const TWeakObjectPtr<UK2Node_ControlRig>& ControlRigPtr : ControlRigs)
	{
		if (UK2Node_ControlRig* ControlRig = ControlRigPtr.Get())
		{
			bool bDisabledOnThisControlRig = bInput ? ControlRig->IsInputPinDisabled(InName) : ControlRig->IsOutputPinDisabled(InName);
			bool bExistsOnThisControlRig = bInput ? ControlRig->HasInputPin(InName) : ControlRig->HasOutputPin(InName);

			bPinDisabled |= bDisabledOnThisControlRig && bExistsOnThisControlRig;
			bPinEnabled |= !bDisabledOnThisControlRig && bExistsOnThisControlRig;
		}
	}

	if (bPinEnabled && !bPinDisabled)
	{
		return ECheckBoxState::Checked;
	}
	else if (!bPinEnabled && bPinDisabled)
	{
		return ECheckBoxState::Unchecked;
	}

	return ECheckBoxState::Undetermined;
}

void FControlRigInputOutputDetailsCustomization::HandleAnimationPinCheckStateChanged(ECheckBoxState CheckBoxState, FName InName, bool bInput)
{
	for (const TWeakObjectPtr<UK2Node_ControlRig>& ControlRigPtr : ControlRigs)
	{
		if (UK2Node_ControlRig* ControlRig = ControlRigPtr.Get())
		{
			bool bHasPin = bInput ? ControlRig->HasInputPin(InName) : ControlRig->HasOutputPin(InName);
			if (bHasPin)
			{
				bInput ? ControlRig->SetInputPinDisabled(InName, CheckBoxState != ECheckBoxState::Checked) : ControlRig->SetOutputPinDisabled(InName, CheckBoxState != ECheckBoxState::Checked);
				ControlRig->ReconstructNode();
			}
		}
	}
}

void FControlRigInputOutputDetailsCustomization::CustomizeHierarchicalDetails(IDetailLayoutBuilder& DetailLayout)
{
	if (bShowInputs)
	{
		IDetailCategoryBuilder& InputCategoryBuilder = DetailLayout.EditCategory("Inputs");
		InputCategoryBuilder.AddProperty(GET_MEMBER_NAME_CHECKED(UK2Node_ControlRig, LabeledInputs));
	}

	if (bShowOutputs)
	{
		IDetailCategoryBuilder& OutputCategoryBuilder = DetailLayout.EditCategory("Outputs");
		OutputCategoryBuilder.AddProperty(GET_MEMBER_NAME_CHECKED(UK2Node_ControlRig, LabeledOutputs));
	}
}

#undef LOCTEXT_NAMESPACE
