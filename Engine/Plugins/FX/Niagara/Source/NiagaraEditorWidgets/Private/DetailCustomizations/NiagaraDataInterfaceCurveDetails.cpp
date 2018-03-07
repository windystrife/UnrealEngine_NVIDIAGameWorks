// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataInterfaceCurveDetails.h"
#include "NiagaraCurveOwner.h"
#include "NiagaraDataInterfaceCurve.h"
#include "NiagaraDataInterfaceVector2DCurve.h"
#include "NiagaraDataInterfaceVectorCurve.h"
#include "NiagaraDataInterfaceVector4Curve.h"
#include "NiagaraDataInterfaceColorCurve.h"
#include "NiagaraEditorWidgetsModule.h"

#include "SCurveEditor.h"
#include "SCurveEditor.h"
#include "PropertyHandle.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "Widgets/Layout/SBox.h"
#include "Misc/Optional.h"
#include "Brushes/SlateColorBrush.h"
#include "Modules/ModuleManager.h"

class SNiagaraResizeBox : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_OneParam(FOnContentHeightChanged, float);

public:
	SLATE_BEGIN_ARGS(SNiagaraResizeBox)
		: _ContentHeight(50)
		, _HandleHeight(5)
		, _HandleColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f))
		, _HandleHighlightColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.5f))
	{}
		SLATE_ARGUMENT(float, HandleHeight)
		SLATE_ATTRIBUTE(float, ContentHeight)
		SLATE_ATTRIBUTE(FLinearColor, HandleColor)
		SLATE_ATTRIBUTE(FLinearColor, HandleHighlightColor)
		SLATE_EVENT(FOnContentHeightChanged, ContentHeightChanged)
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		ContentHeight = InArgs._ContentHeight;
		HandleHeight = InArgs._HandleHeight;
		HandleColor = InArgs._HandleColor;
		HandleHighlightColor = InArgs._HandleHighlightColor;
		HandleBrush = FSlateColorBrush(FLinearColor::White);
		ContentHeightChanged = InArgs._ContentHeightChanged;

		ChildSlot
		[
			SNew(SBox)
			.HeightOverride(this, &SNiagaraResizeBox::GetHeightOverride)
			.Padding(FMargin(0, 0, 0, HandleHeight))
			[
				InArgs._Content.Widget
			]
		];
	}

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
		{
			FVector2D MouseLocation = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
			if (MyGeometry.GetLocalSize().Y - MouseLocation.Y < HandleHeight)
			{
				DragStartLocation = MouseLocation.Y;
				DragStartContentHeight = ContentHeight.Get();
				return FReply::Handled().CaptureMouse(SharedThis(this));
			}
		}
		return FReply::Unhandled();
	}

	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if (this->HasMouseCapture())
		{
			return FReply::Handled().ReleaseMouseCapture();
		}
		return FReply::Unhandled();
	}

	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		FVector2D MouseLocation = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
		LastMouseLocation = MouseLocation.Y;
		if (this->HasMouseCapture())
		{
			float NewContentHeight = DragStartContentHeight + (MouseLocation.Y - DragStartLocation);
			if (ContentHeight.IsBound() && ContentHeightChanged.IsBound())
			{
				ContentHeightChanged.Execute(NewContentHeight);
			}
			else
			{
				ContentHeight = NewContentHeight;
			}
			return FReply::Handled();
		}
		return FReply::Unhandled();
	}

	int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
	{
		FLinearColor HandleBoxColor;
		int32 HandleLayerId = LayerId + 1;
		FVector2D LocalSize = AllottedGeometry.GetLocalSize();
		if (IsHovered() && LastMouseLocation.IsSet() && LastMouseLocation.GetValue() >= LocalSize.Y - HandleHeight && LastMouseLocation.GetValue() <= LocalSize.Y)
		{
			HandleBoxColor = HandleHighlightColor.Get();
		}
		else
		{
			HandleBoxColor = HandleColor.Get();
		}

		FVector2D HandleLocation(0, AllottedGeometry.GetLocalSize().Y - HandleHeight);
		FVector2D HandleSize(AllottedGeometry.GetLocalSize().X, HandleHeight);
		FSlateDrawElement::MakeBox
		(
			OutDrawElements,
			HandleLayerId,
			AllottedGeometry.ToPaintGeometry(HandleLocation, HandleSize),
			&HandleBrush,
			ESlateDrawEffect::None,
			HandleBoxColor
		);

		return SCompoundWidget::OnPaint(Args, AllottedGeometry, MyClippingRect, OutDrawElements, HandleLayerId, InWidgetStyle, bParentEnabled);
	}

private:
	FOptionalSize GetHeightOverride() const
	{
		return ContentHeight.Get() + HandleHeight;
	}

private:
	TOptional<float> LastMouseLocation;

	TAttribute<float> ContentHeight;
	float HandleHeight;

	float DragStartLocation;
	float DragStartContentHeight;

	TAttribute<FLinearColor> HandleColor;
	TAttribute<FLinearColor> HandleHighlightColor;
	FSlateBrush HandleBrush;

	FOnContentHeightChanged ContentHeightChanged;
};

class SNiagaraDataInterfaceCurveEditor : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNiagaraDataInterfaceCurveEditor) {}
	SLATE_END_ARGS()

	void Construct(
		const FArguments& InArgs,
		TArray<TSharedRef<IPropertyHandle>> InCurveProperties,
		bool bIsColorCurve,
		TSharedRef<FNiagaraStackCurveEditorOptions> InStackCurveEditorOptions)
	{
		CurveProperties = InCurveProperties;
		StackCurveEditorOptions = InStackCurveEditorOptions;

		TArray<UObject*> OuterObjects;
		CurveProperties[0]->GetOuterObjects(OuterObjects);
		UObject* CurveOwnerObject = OuterObjects[0];

		CurveOwner = MakeShared<FNiagaraCurveOwner>();
		if (bIsColorCurve)
		{
			CurveOwner->SetColorCurves(
				*GetCurveFromPropertyHandle(CurveProperties[0]),
				*GetCurveFromPropertyHandle(CurveProperties[1]),
				*GetCurveFromPropertyHandle(CurveProperties[2]),
				*GetCurveFromPropertyHandle(CurveProperties[3]),
				NAME_None,
				*CurveOwnerObject,
				FNiagaraCurveOwner::FNotifyCurveChanged::CreateRaw(this, &SNiagaraDataInterfaceCurveEditor::CurveChanged));
		}
		else
		{
			TArray<FLinearColor> CurveColors{ FLinearColor::Red, FLinearColor::Green, FLinearColor::Blue, FLinearColor::White };
			int32 ColorIndex = 0;
			for (TSharedRef<IPropertyHandle> CurveProperty : CurveProperties)
			{
				CurveOwner->AddCurve(
					*GetCurveFromPropertyHandle(CurveProperty),
					*CurveProperty->GetProperty()->GetDisplayNameText().ToString(),
					CurveColors[ColorIndex],
					*CurveOwnerObject,
					FNiagaraCurveOwner::FNotifyCurveChanged::CreateRaw(this, &SNiagaraDataInterfaceCurveEditor::CurveChanged));
				ColorIndex++;
			}
		}

		ViewMinInput = 0;
		ViewMaxOutput = 1;

		for (const FRichCurveEditInfo& CurveEditInfo : CurveOwner->GetCurves())
		{
			if (CurveEditInfo.CurveToEdit->GetNumKeys())
			{
				ViewMinInput = FMath::Min(ViewMinInput, CurveEditInfo.CurveToEdit->GetFirstKey().Time);
				ViewMaxOutput = FMath::Max(ViewMaxOutput, CurveEditInfo.CurveToEdit->GetLastKey().Time);
			}
		}

		ChildSlot
		[
			SAssignNew(CurveEditor, SCurveEditor)
			.HideUI(false)
			.ViewMinInput(StackCurveEditorOptions.ToSharedRef(), &FNiagaraStackCurveEditorOptions::GetViewMinInput)
			.ViewMaxInput(StackCurveEditorOptions.ToSharedRef(), &FNiagaraStackCurveEditorOptions::GetViewMaxInput)
			.ViewMinOutput(StackCurveEditorOptions.ToSharedRef(), &FNiagaraStackCurveEditorOptions::GetViewMinOutput)
			.ViewMaxOutput(StackCurveEditorOptions.ToSharedRef(), &FNiagaraStackCurveEditorOptions::GetViewMaxOutput)
			.AreCurvesVisible(StackCurveEditorOptions.ToSharedRef(), &FNiagaraStackCurveEditorOptions::GetAreCurvesVisible)
			.ZoomToFitVertical(false)
			.ZoomToFitHorizontal(false)
			.TimelineLength(StackCurveEditorOptions.ToSharedRef(), &FNiagaraStackCurveEditorOptions::GetTimelineLength)
			.OnSetInputViewRange(StackCurveEditorOptions.ToSharedRef(), &FNiagaraStackCurveEditorOptions::SetInputViewRange)
			.OnSetOutputViewRange(StackCurveEditorOptions.ToSharedRef(), &FNiagaraStackCurveEditorOptions::SetOutputViewRange)
			.OnSetAreCurvesVisible(StackCurveEditorOptions.ToSharedRef(), &FNiagaraStackCurveEditorOptions::SetAreCurvesVisible)
		];

		CurveEditor->SetCurveOwner(CurveOwner.Get());
	}

private:
	FRichCurve* GetCurveFromPropertyHandle(TSharedPtr<IPropertyHandle> Handle)
	{
		TArray<void*> RawData;
		Handle->AccessRawData(RawData);
		return RawData.Num() == 1 ? static_cast<FRichCurve*>(RawData[0]) : nullptr;
	}

	void CurveChanged(FRichCurve* ChangedCurve, UObject* CurveOwnerObject)
	{
		for (TSharedRef<IPropertyHandle> CurveProperty : CurveProperties)
		{
			if (GetCurveFromPropertyHandle(CurveProperty) == ChangedCurve)
			{
				CurveProperty->NotifyPostChange();
				break;
			}
		}
	}

private:
	float ViewMinInput;
	float ViewMaxOutput;
	TArray<TSharedRef<IPropertyHandle>> CurveProperties;
	TSharedPtr<FNiagaraStackCurveEditorOptions> StackCurveEditorOptions;
	TSharedPtr<FNiagaraCurveOwner> CurveOwner;
	TSharedPtr<SCurveEditor> CurveEditor;
};


// Curve Base
void FNiagaraDataInterfaceCurveDetailsBase::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	// Only support single objects.
	TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;
	DetailBuilder.GetObjectsBeingCustomized(ObjectsBeingCustomized);
	if (ObjectsBeingCustomized.Num() > 1)
	{
		return;
	}

	FNiagaraEditorWidgetsModule& NiagaraEditorWidgetsModule = FModuleManager::GetModuleChecked<FNiagaraEditorWidgetsModule>("NiagaraEditorWidgets");
	TSharedRef<FNiagaraStackCurveEditorOptions> StackCurveEditorOptions = NiagaraEditorWidgetsModule.GetOrCreateStackCurveEditorOptionsForObject(
		ObjectsBeingCustomized[0].Get(), GetDefaultAreCurvesVisible(), GetDefaultHeight());

	TArray<TSharedRef<IPropertyHandle>> CurveProperties;
	GetCurveProperties(DetailBuilder, CurveProperties);

	for (TSharedRef<IPropertyHandle> CurveProperty : CurveProperties)
	{
		CurveProperty->MarkHiddenByCustomization();
	}

	IDetailCategoryBuilder& CurveCategory = DetailBuilder.EditCategory("Curve");
	CurveCategory.AddCustomRow(NSLOCTEXT("NiagaraDataInterfaceCurveDetails", "CurveFilterText", "Curve"))
		.WholeRowContent()
		[
			SNew(SNiagaraResizeBox)
			.ContentHeight(StackCurveEditorOptions, &FNiagaraStackCurveEditorOptions::GetHeight)
			.ContentHeightChanged(StackCurveEditorOptions, &FNiagaraStackCurveEditorOptions::SetHeight)
			.Content()
			[
				SNew(SNiagaraDataInterfaceCurveEditor, CurveProperties, GetIsColorCurve(), StackCurveEditorOptions)
			]
		];
}


// Curve
TSharedRef<IDetailCustomization> FNiagaraDataInterfaceCurveDetails::MakeInstance()
{
	return MakeShared<FNiagaraDataInterfaceCurveDetails>();
}

void FNiagaraDataInterfaceCurveDetails::GetCurveProperties(IDetailLayoutBuilder& DetailBuilder, TArray<TSharedRef<IPropertyHandle>>& CurveProperties) const
{
	CurveProperties.Add(DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UNiagaraDataInterfaceCurve, Curve)));
}


// Vector 2D Curve
TSharedRef<IDetailCustomization> FNiagaraDataInterfaceVector2DCurveDetails::MakeInstance()
{
	return MakeShared<FNiagaraDataInterfaceVector2DCurveDetails>();
}

void FNiagaraDataInterfaceVector2DCurveDetails::GetCurveProperties(IDetailLayoutBuilder& DetailBuilder, TArray<TSharedRef<IPropertyHandle>>& OutCurveProperties) const
{
	OutCurveProperties.Add(DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UNiagaraDataInterfaceVector2DCurve, XCurve)));
	OutCurveProperties.Add(DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UNiagaraDataInterfaceVector2DCurve, YCurve)));
}


// Vector Curve
TSharedRef<IDetailCustomization> FNiagaraDataInterfaceVectorCurveDetails::MakeInstance()
{
	return MakeShared<FNiagaraDataInterfaceVectorCurveDetails>();
}

void FNiagaraDataInterfaceVectorCurveDetails::GetCurveProperties(IDetailLayoutBuilder& DetailBuilder, TArray<TSharedRef<IPropertyHandle>>& OutCurveProperties) const
{
	OutCurveProperties.Add(DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UNiagaraDataInterfaceVectorCurve, XCurve)));
	OutCurveProperties.Add(DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UNiagaraDataInterfaceVectorCurve, YCurve)));
	OutCurveProperties.Add(DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UNiagaraDataInterfaceVectorCurve, ZCurve)));
}


// Vector 4 Curve
TSharedRef<IDetailCustomization> FNiagaraDataInterfaceVector4CurveDetails::MakeInstance()
{
	return MakeShared<FNiagaraDataInterfaceVector4CurveDetails>();
}

void FNiagaraDataInterfaceVector4CurveDetails::GetCurveProperties(IDetailLayoutBuilder& DetailBuilder, TArray<TSharedRef<IPropertyHandle>>& OutCurveProperties) const
{
	OutCurveProperties.Add(DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UNiagaraDataInterfaceVector4Curve, XCurve)));
	OutCurveProperties.Add(DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UNiagaraDataInterfaceVector4Curve, YCurve)));
	OutCurveProperties.Add(DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UNiagaraDataInterfaceVector4Curve, ZCurve)));
	OutCurveProperties.Add(DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UNiagaraDataInterfaceVector4Curve, WCurve)));
}


// Color Curve
TSharedRef<IDetailCustomization> FNiagaraDataInterfaceColorCurveDetails::MakeInstance()
{
	return MakeShared<FNiagaraDataInterfaceColorCurveDetails>();
}

void FNiagaraDataInterfaceColorCurveDetails::GetCurveProperties(IDetailLayoutBuilder& DetailBuilder, TArray<TSharedRef<IPropertyHandle>>& OutCurveProperties) const
{
	OutCurveProperties.Add(DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UNiagaraDataInterfaceColorCurve, RedCurve)));
	OutCurveProperties.Add(DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UNiagaraDataInterfaceColorCurve, GreenCurve)));
	OutCurveProperties.Add(DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UNiagaraDataInterfaceColorCurve, BlueCurve)));
	OutCurveProperties.Add(DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UNiagaraDataInterfaceColorCurve, AlphaCurve)));
}