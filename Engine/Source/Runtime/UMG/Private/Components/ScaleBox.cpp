// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "UObject/EditorObjectVersion.h"

#define LOCTEXT_NAMESPACE "UMG"

/////////////////////////////////////////////////////
// UScaleBox

UScaleBox::UScaleBox(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsVariable = false;
	Visibility = ESlateVisibility::SelfHitTestInvisible;

	StretchDirection = EStretchDirection::Both;
	Stretch = EStretch::ScaleToFit;
	UserSpecifiedScale = 1.0f;
	IgnoreInheritedScale = false;
	// @todo: - DO NOT copy this change to UE4/Main
	bSingleLayoutPass = true;
	// @todo: - DO NOT copy this change to UE4/Main
}

void UScaleBox::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyScaleBox.Reset();
}

TSharedRef<SWidget> UScaleBox::RebuildWidget()
{
	MyScaleBox = SNew(SScaleBox)
		.SingleLayoutPass(bSingleLayoutPass);
	
	if ( GetChildrenCount() > 0 )
	{
		CastChecked<UScaleBoxSlot>(GetContentSlot())->BuildSlot(MyScaleBox.ToSharedRef());
	}

	return MyScaleBox.ToSharedRef();
}

void UScaleBox::SetStretch(EStretch::Type InStretch)
{
	Stretch = InStretch;
	if(MyScaleBox.IsValid())
	{
		MyScaleBox->SetStretch(InStretch);
	}
}

void UScaleBox::SetStretchDirection(EStretchDirection::Type InStretchDirection)
{
	StretchDirection = InStretchDirection;
	if (MyScaleBox.IsValid())
	{
		MyScaleBox->SetStretchDirection(InStretchDirection);
	}
}

void UScaleBox::SetUserSpecifiedScale(float InUserSpecifiedScale)
{
	UserSpecifiedScale = InUserSpecifiedScale;
	if (MyScaleBox.IsValid())
	{
		MyScaleBox->SetUserSpecifiedScale(InUserSpecifiedScale);
	}
}

void UScaleBox::SetIgnoreInheritedScale(bool bInIgnoreInheritedScale)
{
	IgnoreInheritedScale = bInIgnoreInheritedScale;
	if (MyScaleBox.IsValid())
	{
		MyScaleBox->SetIgnoreInheritedScale(bInIgnoreInheritedScale);
	}
}

void UScaleBox::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	MyScaleBox->SetStretchDirection(StretchDirection);
	MyScaleBox->SetStretch(Stretch);
	MyScaleBox->SetUserSpecifiedScale(UserSpecifiedScale);
	MyScaleBox->SetIgnoreInheritedScale(IgnoreInheritedScale);
}

UClass* UScaleBox::GetSlotClass() const
{
	return UScaleBoxSlot::StaticClass();
}

void UScaleBox::OnSlotAdded(UPanelSlot* InSlot)
{
	// Add the child to the live slot if it already exists
	if ( MyScaleBox.IsValid() )
	{
		CastChecked<UScaleBoxSlot>(InSlot)->BuildSlot(MyScaleBox.ToSharedRef());
	}
}

void UScaleBox::OnSlotRemoved(UPanelSlot* InSlot)
{
	// Remove the widget from the live slot if it exists.
	if ( MyScaleBox.IsValid() )
	{
		MyScaleBox->SetContent(SNullWidget::NullWidget);
	}
}

void UScaleBox::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FEditorObjectVersion::GUID);
}

#if WITH_EDITOR

const FText UScaleBox::GetPaletteCategory()
{
	return LOCTEXT("Panel", "Panel");
}

bool UScaleBox::CanEditChange(const UProperty* InProperty) const
{
	bool bIsEditable = Super::CanEditChange(InProperty);
	if (bIsEditable && InProperty)
	{
		const FName PropertyName = InProperty->GetFName();

		if (PropertyName == GET_MEMBER_NAME_CHECKED(UScaleBox, StretchDirection))
		{
			return Stretch != EStretch::None && Stretch != EStretch::ScaleBySafeZone && Stretch != EStretch::UserSpecified;
		}

		if (PropertyName == GET_MEMBER_NAME_CHECKED(UScaleBox, UserSpecifiedScale))
		{
			return Stretch == EStretch::UserSpecified;
		}
	}

	return bIsEditable;
}

#endif

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
