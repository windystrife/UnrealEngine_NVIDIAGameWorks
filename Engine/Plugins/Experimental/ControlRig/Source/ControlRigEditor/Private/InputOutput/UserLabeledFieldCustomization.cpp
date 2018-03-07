// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UserLabeledFieldCustomization.h"
#include "K2Node_ControlRig.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyHandle.h"
#include "Containers/Algo/Transform.h"
#include "HierarchicalRig.h"
#include "DetailWidgetRow.h"
#include "SComboBox.h"
#include "DetailLayoutBuilder.h"
#include "PropertyCustomizationHelpers.h"
#include "SEditableTextBox.h"
#include "SButton.h"
#include "SImage.h"

#define LOCTEXT_NAMESPACE "UserLabeledFieldCustomization"

TSharedRef<IPropertyTypeCustomization> FUserLabeledFieldCustomization::MakeInstance()
{
	return MakeShareable(new FUserLabeledFieldCustomization);
}

void FUserLabeledFieldCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	TArray<UObject*> EditingObjects;
	PropertyHandle->GetOuterObjects(EditingObjects);

	Algo::TransformIf(EditingObjects, ControlRigs, [](UObject* Object) { return Object != nullptr && Object->IsA<UK2Node_ControlRig>(); }, [](UObject* Object) { return Cast<UK2Node_ControlRig>(Object); });

	const bool bInput = PropertyHandle->GetParentHandle().IsValid() ? PropertyHandle->GetParentHandle()->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(UK2Node_ControlRig, LabeledInputs) : false;
	const bool bOutput = PropertyHandle->GetParentHandle().IsValid() ? PropertyHandle->GetParentHandle()->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(UK2Node_ControlRig, LabeledOutputs) : false;
	PropertyHandleArray = PropertyHandle->GetParentHandle().IsValid() ? PropertyHandle->GetParentHandle()->AsArray() : nullptr;

	bHasHierarchicalData = false;

	LabeledFieldNames.Add(MakeShareable(new FName(NAME_None)));

	for (const TWeakObjectPtr<UK2Node_ControlRig>& ControlRigPtr : ControlRigs)
	{
		UK2Node_ControlRig* ControlRig = ControlRigPtr.Get();

		// Check if we have a hierarchy ControlRig
		if (UClass* Class = ControlRig->GetControlRigClass())
		{
			if (Class->IsChildOf(UHierarchicalRig::StaticClass()) || Class->HasMetaData(TEXT("UsesHierarchy")))
			{
				bHasHierarchicalData = true;

				TArray<UField*> Fields;
				if (bInput)
				{
					ControlRig->GetPotentialLabeledInputFields(Fields);
				}
				else if (bOutput)
				{
					ControlRig->GetPotentialLabeledOutputFields(Fields);
				}

				for (UField* Field : Fields)
				{
					LabeledFieldNames.AddUnique(MakeShareable(new FName(Field->GetFName())));
				}
			}
		}
	}
}

void FUserLabeledFieldCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	if (bHasHierarchicalData && LabeledFieldNames.Num() > 0)
	{
		int32 ArrayIndex = PropertyHandle->GetIndexInArray();

		TSharedPtr<IPropertyHandle> LabelPropertyHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FUserLabeledField, Label));
		TSharedPtr<IPropertyHandle> FieldNamePropertyHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FUserLabeledField, FieldName));
		if (LabelPropertyHandle.IsValid() && FieldNamePropertyHandle.IsValid())
		{
			TSharedPtr<FName> InitialItem = LabeledFieldNames[0];
			FName FieldName(NAME_None);
			FieldNamePropertyHandle->GetValue(FieldName);
			if (FieldName != NAME_None)
			{
				TSharedPtr<FName>* FoundItem = LabeledFieldNames.FindByPredicate([FieldName](const TSharedPtr<FName>& InName) { return *InName == FieldName; });
				if (FoundItem != nullptr)
				{
					InitialItem = *FoundItem;
				}
			}

			ChildBuilder.AddCustomRow(LOCTEXT("Node", "Node"))
			.NameContent()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SEditableTextBox)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.ToolTipText(LOCTEXT("NodeLabelTooltip", "The node to expose an input/output value on."))
					.Text_Lambda([LabelPropertyHandle]()
					{
						FString LocalFieldName;
						LabelPropertyHandle->GetValue(LocalFieldName);
						return FText::FromString(LocalFieldName);
					})
					.OnTextCommitted(this, &FUserLabeledFieldCustomization::HandleFieldLabelCommitted, LabelPropertyHandle)
				]
			]
			.ValueContent()
			.MaxDesiredWidth(800.0f)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SComboBox<TSharedPtr<FName>>)
					.OptionsSource(&LabeledFieldNames)
					.InitiallySelectedItem(InitialItem)
					.ToolTipText(LOCTEXT("NodeValueTooltip", "The function to use to get/set the value on the specifed node."))
					.OnSelectionChanged(this, &FUserLabeledFieldCustomization::HandleFieldNameSelectionChanged, FieldNamePropertyHandle)
					.OnGenerateWidget_Lambda([](TSharedPtr<FName> Value)
					{
						return SNew(STextBlock)
							.Font(IDetailLayoutBuilder::GetDetailFont())
							.Text(FText::FromName(*Value));
					})
					[
						SNew(STextBlock)
						.Font(IDetailLayoutBuilder::GetDetailFont())
						.Text_Lambda([FieldNamePropertyHandle]()
						{
							FName LocalFieldName(NAME_None);
							FieldNamePropertyHandle->GetValue(LocalFieldName);
							return FText::FromName(LocalFieldName);
						})
					]
				]
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Right)
				.FillWidth(1.0f)
				[
					SNew(SHorizontalBox)
					.Visibility_Lambda([this]() { return PropertyHandleArray.IsValid() ? EVisibility::Visible : EVisibility::Collapsed; })
					+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(1.0f)
					[
						SNew(SButton)
						.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
						.ContentPadding(FMargin(4, 6))
						.OnClicked(this, &FUserLabeledFieldCustomization::HandleMoveUp, ArrayIndex)
						.ToolTipText(LOCTEXT("NamedParameterArrayMoveValueUpToolTip", "Move this parameter up in the list."))
						[
							SNew(SImage)
							.Image(FEditorStyle::GetBrush("Symbols.UpArrow"))
						]
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(1.0f)
					[
						SNew(SButton)
						.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
						.ContentPadding(FMargin(4, 6))
						.OnClicked(this, &FUserLabeledFieldCustomization::HandleMoveDown, ArrayIndex)
						.ToolTipText(LOCTEXT("NamedParameterArrayMoveValueDownToolTip", "Move this parameter down in the list."))
						[
							SNew(SImage)
							.Image(FEditorStyle::GetBrush("Symbols.DownArrow"))
						]
					]
					+SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.AutoWidth()
					.Padding(1.0f)
					[
						PropertyCustomizationHelpers::MakeClearButton(FSimpleDelegate::CreateSP(this, &FUserLabeledFieldCustomization::HandleRemove, ArrayIndex), LOCTEXT("NamedParameterArrayRemoveToolTip", "Remove this parameter."), true)
					]
				]
			];
		}
	}
}

void FUserLabeledFieldCustomization::HandleFieldNameSelectionChanged(TSharedPtr<FName> Value, ESelectInfo::Type SelectionInfo, TSharedPtr<IPropertyHandle> FieldNamePropertyHandle)
{
	FieldNamePropertyHandle->SetValue(*Value);

	for (const TWeakObjectPtr<UK2Node_ControlRig>& ControlRigPtr : ControlRigs)
	{
		if (UK2Node_ControlRig* ControlRig = ControlRigPtr.Get())
		{
			ControlRig->ReconstructNode();
		}
	}
}

void FUserLabeledFieldCustomization::HandleFieldLabelCommitted(const FText& NewText, ETextCommit::Type CommitType, TSharedPtr<IPropertyHandle> LabelPropertyHandle)
{
	LabelPropertyHandle->SetValue(NewText.ToString());

	for (const TWeakObjectPtr<UK2Node_ControlRig>& ControlRigPtr : ControlRigs)
	{
		if (UK2Node_ControlRig* ControlRig = ControlRigPtr.Get())
		{
			ControlRig->ReconstructNode();
		}
	}
}

FReply FUserLabeledFieldCustomization::HandleMoveUp(int32 Index)
{
	check(PropertyHandleArray.IsValid());
	if (PropertyHandleArray->SwapItems(Index, Index - 1) == FPropertyAccess::Success)
	{
		for (const TWeakObjectPtr<UK2Node_ControlRig>& ControlRigPtr : ControlRigs)
		{
			if (UK2Node_ControlRig* ControlRig = ControlRigPtr.Get())
			{
				ControlRig->ReconstructNode();
			}
		}
	}

	return FReply::Handled();
}

FReply FUserLabeledFieldCustomization::HandleMoveDown(int32 Index)
{
	check(PropertyHandleArray.IsValid());
	if (PropertyHandleArray->SwapItems(Index, Index + 1) == FPropertyAccess::Success)
	{
		for (const TWeakObjectPtr<UK2Node_ControlRig>& ControlRigPtr : ControlRigs)
		{
			if (UK2Node_ControlRig* ControlRig = ControlRigPtr.Get())
			{
				ControlRig->ReconstructNode();
			}
		}
	}

	return FReply::Handled();
}

void FUserLabeledFieldCustomization::HandleRemove(int32 Index)
{
	if (PropertyHandleArray->DeleteItem(Index) == FPropertyAccess::Success)
	{
		for (const TWeakObjectPtr<UK2Node_ControlRig>& ControlRigPtr : ControlRigs)
		{
			if (UK2Node_ControlRig* ControlRig = ControlRigPtr.Get())
			{
				ControlRig->ReconstructNode();
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE