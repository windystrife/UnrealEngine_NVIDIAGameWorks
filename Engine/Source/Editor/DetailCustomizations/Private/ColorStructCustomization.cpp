// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ColorStructCustomization.h"
#include "UObject/UnrealType.h"
#include "Widgets/Text/STextBlock.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "Editor.h"
#include "Widgets/Colors/SColorBlock.h"
#include "DetailWidgetRow.h"
#include "DetailLayoutBuilder.h"
#include "IPropertyUtilities.h"
#include "Widgets/Colors/SColorPicker.h"
#include "SlateApplication.h"


#define LOCTEXT_NAMESPACE "FColorStructCustomization"


TSharedRef<IPropertyTypeCustomization> FColorStructCustomization::MakeInstance() 
{
	return MakeShareable(new FColorStructCustomization);
}


void FColorStructCustomization::CustomizeHeader(TSharedRef<class IPropertyHandle> InStructPropertyHandle, class FDetailWidgetRow& InHeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	StructPropertyHandle = InStructPropertyHandle;

	bIsLinearColor = CastChecked<UStructProperty>(StructPropertyHandle->GetProperty())->Struct->GetFName() == NAME_LinearColor;
	bIgnoreAlpha = StructPropertyHandle->GetProperty()->HasMetaData(TEXT("HideAlphaChannel"));
	
	if (StructPropertyHandle->GetProperty()->HasMetaData(TEXT("sRGB")))
	{
		sRGBOverride = StructPropertyHandle->GetProperty()->GetBoolMetaData(TEXT("sRGB"));
	}

	auto PropertyUtils = StructCustomizationUtils.GetPropertyUtilities();
	bDontUpdateWhileEditing = PropertyUtils.IsValid() ? PropertyUtils->DontUpdateValueWhileEditing() : false;

	FMathStructCustomization::CustomizeHeader(InStructPropertyHandle, InHeaderRow, StructCustomizationUtils);
}


void FColorStructCustomization::MakeHeaderRow(TSharedRef<class IPropertyHandle>& InStructPropertyHandle, FDetailWidgetRow& Row)
{
	// We'll set up reset to default ourselves
	const bool bDisplayResetToDefault = false;
	const FText DisplayNameOverride = FText::GetEmpty();
	const FText DisplayToolTipOverride = FText::GetEmpty();
	
	TSharedPtr<SWidget> ColorWidget;
	float ContentWidth = 250.0f;

	TWeakPtr<IPropertyHandle> StructWeakHandlePtr = StructPropertyHandle;

	if (InStructPropertyHandle->HasMetaData("InlineColorPicker"))
	{
		ColorWidget = CreateInlineColorPicker(StructWeakHandlePtr);
		ContentWidth = 384.0f;
	}
	else
	{
		ColorWidget = CreateColorWidget(StructWeakHandlePtr);
	}

	Row.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget(DisplayNameOverride, DisplayToolTipOverride, bDisplayResetToDefault)
	]
	.ValueContent()
	.MinDesiredWidth(ContentWidth)
	[
		ColorWidget.ToSharedRef()
	];
}


TSharedRef<SWidget> FColorStructCustomization::CreateColorWidget(TWeakPtr<IPropertyHandle> StructWeakHandlePtr)
{
	FSlateFontInfo NormalText = IDetailLayoutBuilder::GetDetailFont();

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.Padding(0.0f, 2.0f)
		[
			SNew(SOverlay)
			+SOverlay::Slot()
			[
				// Displays the color with alpha unless it is ignored
				SAssignNew(ColorPickerParentWidget, SColorBlock)
				.Color(this, &FColorStructCustomization::OnGetColorForColorBlock)
				.ShowBackgroundForAlpha(true)
				.IgnoreAlpha(bIgnoreAlpha)
				.OnMouseButtonDown(this, &FColorStructCustomization::OnMouseButtonDownColorBlock)
				.Size(FVector2D(35.0f, 12.0f))
				.IsEnabled(this, &FColorStructCustomization::IsValueEnabled, StructWeakHandlePtr)
			]
			+SOverlay::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("PropertyEditor", "MultipleValues", "Multiple Values"))
				.Font(NormalText)
				.ColorAndOpacity(FSlateColor(FLinearColor::Black)) // we know the background is always white, so can safely set this to black
				.Visibility(this, &FColorStructCustomization::GetMultipleValuesTextVisibility)
			]
		]
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.Padding(0.0f, 2.0f)
		[
			// Displays the color without alpha
			SNew(SColorBlock)
			.Color(this, &FColorStructCustomization::OnGetColorForColorBlock)
			.ShowBackgroundForAlpha(false)
			.IgnoreAlpha(true)
			.OnMouseButtonDown(this, &FColorStructCustomization::OnMouseButtonDownColorBlock)
			.Size(FVector2D(35.0f, 12.0f))
		];
}


void FColorStructCustomization::GetSortedChildren(TSharedRef<IPropertyHandle> InStructPropertyHandle, TArray< TSharedRef<IPropertyHandle> >& OutChildren)
{
	static const FName Red("R");
	static const FName Green("G");
	static const FName Blue("B");
	static const FName Alpha("A");

	// We control the order of the colors via this array so it always ends up R,G,B,A
	TSharedPtr< IPropertyHandle > ColorProperties[4];

	uint32 NumChildren;
	InStructPropertyHandle->GetNumChildren(NumChildren);

	for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
	{
		TSharedRef<IPropertyHandle> ChildHandle = InStructPropertyHandle->GetChildHandle(ChildIndex).ToSharedRef();
		const FName PropertyName = ChildHandle->GetProperty()->GetFName();

		if (PropertyName == Red)
		{
			ColorProperties[0] = ChildHandle;
		}
		else if (PropertyName == Green)
		{
			ColorProperties[1] = ChildHandle;
		}
		else if (PropertyName == Blue)
		{
			ColorProperties[2] = ChildHandle;
		}
		else
		{
			ColorProperties[3] = ChildHandle;
		}
	}

	OutChildren.Add(ColorProperties[0].ToSharedRef());
	OutChildren.Add(ColorProperties[1].ToSharedRef());
	OutChildren.Add(ColorProperties[2].ToSharedRef());

	// Alpha channel may not be used
	if (!bIgnoreAlpha && ColorProperties[3].IsValid())
	{
		OutChildren.Add(ColorProperties[3].ToSharedRef());
	}
}


void FColorStructCustomization::CreateColorPicker(bool bUseAlpha)
{
	int32 NumObjects = StructPropertyHandle->GetNumOuterObjects();

	SavedPreColorPickerColors.Empty();
	TArray<FString> PerObjectValues;
	StructPropertyHandle->GetPerObjectValues(PerObjectValues);

	for (int32 ObjectIndex = 0; ObjectIndex < NumObjects; ++ObjectIndex)
	{
		if (bIsLinearColor)
		{
			FLinearColor Color;
			Color.InitFromString(PerObjectValues[ObjectIndex]);
			SavedPreColorPickerColors.Add(FLinearOrSrgbColor(Color));
		}
		else
		{
			FColor Color;
			Color.InitFromString(PerObjectValues[ObjectIndex]);
			SavedPreColorPickerColors.Add(FLinearOrSrgbColor(Color));
		}
	}

	FLinearColor InitialColor;
	GetColorAsLinear(InitialColor);

	const bool bRefreshOnlyOnOk = bDontUpdateWhileEditing || StructPropertyHandle->HasMetaData("DontUpdateWhileEditing");

	FColorPickerArgs PickerArgs;
	{
		PickerArgs.bUseAlpha = !bIgnoreAlpha;
		PickerArgs.bOnlyRefreshOnMouseUp = false;
		PickerArgs.bOnlyRefreshOnOk = bRefreshOnlyOnOk;
		PickerArgs.sRGBOverride = sRGBOverride;
		PickerArgs.DisplayGamma = TAttribute<float>::Create(TAttribute<float>::FGetter::CreateUObject(GEngine, &UEngine::GetDisplayGamma));
		PickerArgs.OnColorCommitted = FOnLinearColorValueChanged::CreateSP(this, &FColorStructCustomization::OnSetColorFromColorPicker);
		PickerArgs.OnColorPickerCancelled = FOnColorPickerCancelled::CreateSP(this, &FColorStructCustomization::OnColorPickerCancelled);
		PickerArgs.OnInteractivePickBegin = FSimpleDelegate::CreateSP(this, &FColorStructCustomization::OnColorPickerInteractiveBegin);
		PickerArgs.OnInteractivePickEnd = FSimpleDelegate::CreateSP(this, &FColorStructCustomization::OnColorPickerInteractiveEnd);
		PickerArgs.InitialColorOverride = InitialColor;
		PickerArgs.ParentWidget = ColorPickerParentWidget;

		FWidgetPath ParentWidgetPath;
		if (FSlateApplication::Get().FindPathToWidget(ColorPickerParentWidget.ToSharedRef(), ParentWidgetPath))
		{
			PickerArgs.bOpenAsMenu = FSlateApplication::Get().FindMenuInWidgetPath(ParentWidgetPath).IsValid();
		}
	}

	OpenColorPicker(PickerArgs);
}


TSharedRef<SColorPicker> FColorStructCustomization::CreateInlineColorPicker(TWeakPtr<IPropertyHandle> StructWeakHandlePtr)
{
	int32 NumObjects = StructPropertyHandle->GetNumOuterObjects();

	SavedPreColorPickerColors.Empty();
	TArray<FString> PerObjectValues;
	StructPropertyHandle->GetPerObjectValues(PerObjectValues);

	for (int32 ObjectIndex = 0; ObjectIndex < NumObjects; ++ObjectIndex)
	{
		if (bIsLinearColor)
		{
			FLinearColor Color;
			Color.InitFromString(PerObjectValues[ObjectIndex]);
			SavedPreColorPickerColors.Add(FLinearOrSrgbColor(Color));	
		}
		else
		{
			FColor Color;
			Color.InitFromString(PerObjectValues[ObjectIndex]);
			SavedPreColorPickerColors.Add(FLinearOrSrgbColor(Color));
		}
	}

	FLinearColor InitialColor;
	GetColorAsLinear(InitialColor);

	const bool bRefreshOnlyOnOk = /*bDontUpdateWhileEditing ||*/ StructPropertyHandle->HasMetaData("DontUpdateWhileEditing");
	
	return SNew(SColorPicker)
		.DisplayInlineVersion(true)
		.OnlyRefreshOnMouseUp(false)
		.OnlyRefreshOnOk(bRefreshOnlyOnOk)
		.DisplayGamma(TAttribute<float>::Create(TAttribute<float>::FGetter::CreateUObject(GEngine, &UEngine::GetDisplayGamma)))
		.OnColorCommitted(FOnLinearColorValueChanged::CreateSP(this, &FColorStructCustomization::OnSetColorFromColorPicker))
		.OnColorPickerCancelled(FOnColorPickerCancelled::CreateSP(this, &FColorStructCustomization::OnColorPickerCancelled))
		.OnInteractivePickBegin(FSimpleDelegate::CreateSP(this, &FColorStructCustomization::OnColorPickerInteractiveBegin))
		.OnInteractivePickEnd(FSimpleDelegate::CreateSP(this, &FColorStructCustomization::OnColorPickerInteractiveEnd))
		.sRGBOverride(sRGBOverride)
		.TargetColorAttribute(InitialColor)
		.IsEnabled(this, &FColorStructCustomization::IsValueEnabled, StructWeakHandlePtr);
}


void FColorStructCustomization::OnSetColorFromColorPicker(FLinearColor NewColor)
{
	FString ColorString;
	if (bIsLinearColor)
	{
		ColorString = NewColor.ToString();
	}
	else
	{
		const bool bSRGB = true;
		FColor NewFColor = NewColor.ToFColor(bSRGB);
		ColorString = NewFColor.ToString();
	}

	StructPropertyHandle->SetValueFromFormattedString(ColorString, bIsInteractive ? EPropertyValueSetFlags::InteractiveChange : 0);
	StructPropertyHandle->NotifyFinishedChangingProperties();
}


void FColorStructCustomization::OnColorPickerCancelled(FLinearColor OriginalColor)
{
	TArray<FString> PerObjectColors;

	for (int32 ColorIndex = 0; ColorIndex < SavedPreColorPickerColors.Num(); ++ColorIndex)
	{
		if (bIsLinearColor)
		{
			PerObjectColors.Add(SavedPreColorPickerColors[ColorIndex].GetLinear().ToString());
		}
		else
		{
			FColor Color = SavedPreColorPickerColors[ColorIndex].GetSrgb();
			PerObjectColors.Add(Color.ToString());
		}
	}

	if (PerObjectColors.Num() > 0)
	{
		StructPropertyHandle->SetPerObjectValues(PerObjectColors);
	}
}


void FColorStructCustomization::OnColorPickerInteractiveBegin()
{
	bIsInteractive = true;

	GEditor->BeginTransaction(FText::Format(LOCTEXT("SetColorProperty", "Edit {0}"), StructPropertyHandle->GetPropertyDisplayName()));
}


void FColorStructCustomization::OnColorPickerInteractiveEnd()
{
	bIsInteractive = false;

	if (!bDontUpdateWhileEditing)
	{
		// pushes the last value from the interactive change without the interactive flag
		FString ColorString;
		StructPropertyHandle->GetValueAsFormattedString(ColorString);
		StructPropertyHandle->SetValueFromFormattedString(ColorString);
	}

	GEditor->EndTransaction();
}


FLinearColor FColorStructCustomization::OnGetColorForColorBlock() const
{
	FLinearColor Color;
	GetColorAsLinear(Color);
	return Color;
}


FPropertyAccess::Result FColorStructCustomization::GetColorAsLinear(FLinearColor& OutColor) const
{
	// Default to full alpha in case the alpha component is disabled.
	OutColor.A = 1.0f;

	FString StringValue;
	FPropertyAccess::Result Result = StructPropertyHandle->GetValueAsFormattedString(StringValue);

	if(Result == FPropertyAccess::Success)
	{
		if (bIsLinearColor)
		{
			OutColor.InitFromString(StringValue);
		}
		else
		{
			FColor SrgbColor;
			SrgbColor.InitFromString(StringValue);
			OutColor = FLinearColor(SrgbColor);
		}
	}
	else if(Result == FPropertyAccess::MultipleValues)
	{
		OutColor = FLinearColor::White;
	}

	return Result;
}


EVisibility FColorStructCustomization::GetMultipleValuesTextVisibility() const
{
	FLinearColor Color;
	const FPropertyAccess::Result ValueResult = GetColorAsLinear(Color);
	return (ValueResult == FPropertyAccess::MultipleValues) ? EVisibility::Visible : EVisibility::Collapsed;
}


FReply FColorStructCustomization::OnMouseButtonDownColorBlock(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}
	
	bool CanShowColorPicker = true;
	if (StructPropertyHandle.IsValid() && StructPropertyHandle->GetProperty() != nullptr)
	{
		CanShowColorPicker = !StructPropertyHandle->IsEditConst();
	}
	if (CanShowColorPicker)
	{
		CreateColorPicker(true /*bUseAlpha*/);
	}

	return FReply::Handled();
}


FReply FColorStructCustomization::OnOpenFullColorPickerClicked()
{
	CreateColorPicker(true /*bUseAlpha*/);
	bIsInlineColorPickerVisible = false;

	return FReply::Handled();
}


#undef LOCTEXT_NAMESPACE
