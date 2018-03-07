// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneObjectBindingIDCustomization.h"
#include "IPropertyUtilities.h"
#include "MovieSceneBindingOwnerInterface.h"
#include "MovieSceneSequence.h"
#include "MovieScene.h"
#include "PropertyHandle.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "SDropTarget.h"
#include "SComboButton.h"
#include "STextBlock.h"
#include "SequencerObjectBindingDragDropOp.h"

#define LOCTEXT_NAMESPACE "MovieSceneObjectBindingIDCustomization"

void FMovieSceneObjectBindingIDCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	StructProperty = PropertyHandle;

	Initialize();

	auto IsAcceptable = [](TSharedPtr<FDragDropOperation> Operation)
	{
		return Operation->IsOfType<FSequencerObjectBindingDragDropOp>() && static_cast<FSequencerObjectBindingDragDropOp*>(Operation.Get())->GetDraggedBindings().Num() == 1;
	};

	HeaderRow
	.NameContent()
	[
		StructProperty->CreatePropertyNameWidget()
	]
	.ValueContent()
	[
		SNew(SDropTarget)
		.OnDrop(this, &FMovieSceneObjectBindingIDCustomization::OnDrop)
		.OnAllowDrop_Static(IsAcceptable)
		.OnIsRecognized_Static(IsAcceptable)
		[
			SNew(SComboButton)
			.ToolTipText(this, &FMovieSceneObjectBindingIDCustomization::GetToolTipText)
			.OnGetMenuContent(this, &FMovieSceneObjectBindingIDCustomization::GetPickerMenu)
			.ContentPadding(FMargin(4.0, 2.0))
			.ButtonContent()
			[
				GetCurrentItemWidget(
					SNew(STextBlock)
					.Font(CustomizationUtils.GetRegularFont())
				)
			]
		]
	];
}

FReply FMovieSceneObjectBindingIDCustomization::OnDrop(TSharedPtr<FDragDropOperation> InOperation)
{
	FSequencerObjectBindingDragDropOp* SequencerOp = InOperation->IsOfType<FSequencerObjectBindingDragDropOp>() ? static_cast<FSequencerObjectBindingDragDropOp*>(InOperation.Get()) : nullptr;
	if (SequencerOp)
	{
		TArray<FMovieSceneObjectBindingID> Bindings = SequencerOp->GetDraggedBindings();
		if (Bindings.Num() == 1)
		{
			SetBindingId(Bindings[0]);
		}
	}

	return FReply::Handled();
}

UMovieSceneSequence* FMovieSceneObjectBindingIDCustomization::GetSequence() const
{
	TArray<UObject*> OuterObjects;
	StructProperty->GetOuterObjects(OuterObjects);

	if (OuterObjects.Num() != 1)
	{
		return nullptr;
	}

	for ( UObject* NextOuter = OuterObjects[0]; NextOuter; NextOuter = NextOuter->GetOuter() )
	{
		if (IMovieSceneBindingOwnerInterface* Result = Cast<IMovieSceneBindingOwnerInterface>(NextOuter))
		{
			return Result->RetrieveOwnedSequence();
		}
	}
	return nullptr;
}

FMovieSceneObjectBindingID FMovieSceneObjectBindingIDCustomization::GetCurrentValue() const
{
	TArray<void*> Ptrs;
	StructProperty->AccessRawData(Ptrs);

	return ensure(Ptrs.Num() == 1) ? *static_cast<FMovieSceneObjectBindingID*>(Ptrs[0]) : FMovieSceneObjectBindingID();
}

void FMovieSceneObjectBindingIDCustomization::SetCurrentValue(const FMovieSceneObjectBindingID& InObjectBinding)
{
	TArray<void*> Ptrs;
	StructProperty->AccessRawData(Ptrs);

	if (ensure(Ptrs.Num() == 1))
	{
		*static_cast<FMovieSceneObjectBindingID*>(Ptrs[0]) = InObjectBinding;
	}
}

#undef LOCTEXT_NAMESPACE
