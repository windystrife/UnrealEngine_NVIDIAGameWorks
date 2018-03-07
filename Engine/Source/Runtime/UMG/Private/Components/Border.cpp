// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Components/Border.h"
#include "Slate/SlateBrushAsset.h"
#include "Materials/MaterialInterface.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Layout/SBorder.h"
#include "Components/BorderSlot.h"
#include "ObjectEditorUtils.h"

#define LOCTEXT_NAMESPACE "UMG"

/////////////////////////////////////////////////////
// UBorder

UBorder::UBorder(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsVariable = false;

	ContentColorAndOpacity = FLinearColor::White;
	BrushColor = FLinearColor::White;

	Padding = FMargin(4, 2);

	HorizontalAlignment = HAlign_Fill;
	VerticalAlignment = VAlign_Fill;

	DesiredSizeScale = FVector2D(1, 1);

	bShowEffectWhenDisabled = true;
}

void UBorder::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyBorder.Reset();
}

TSharedRef<SWidget> UBorder::RebuildWidget()
{
	MyBorder = SNew(SBorder);
	
	if ( GetChildrenCount() > 0 )
	{
		Cast<UBorderSlot>(GetContentSlot())->BuildSlot(MyBorder.ToSharedRef());
	}

	return MyBorder.ToSharedRef();
}

void UBorder::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	
	TAttribute<FLinearColor> ContentColorAndOpacityBinding = PROPERTY_BINDING(FLinearColor, ContentColorAndOpacity);
	TAttribute<FSlateColor> BrushColorBinding = OPTIONAL_BINDING_CONVERT(FLinearColor, BrushColor, FSlateColor, ConvertLinearColorToSlateColor);
	TAttribute<const FSlateBrush*> ImageBinding = OPTIONAL_BINDING_CONVERT(FSlateBrush, Background, const FSlateBrush*, ConvertImage);
	
	MyBorder->SetPadding(Padding);
	MyBorder->SetBorderBackgroundColor(BrushColorBinding);
	MyBorder->SetColorAndOpacity(ContentColorAndOpacityBinding);

	MyBorder->SetBorderImage(ImageBinding);
	
	MyBorder->SetDesiredSizeScale(DesiredSizeScale);
	MyBorder->SetShowEffectWhenDisabled(bShowEffectWhenDisabled != 0);
	
	MyBorder->SetOnMouseButtonDown(BIND_UOBJECT_DELEGATE(FPointerEventHandler, HandleMouseButtonDown));
	MyBorder->SetOnMouseButtonUp(BIND_UOBJECT_DELEGATE(FPointerEventHandler, HandleMouseButtonUp));
	MyBorder->SetOnMouseMove(BIND_UOBJECT_DELEGATE(FPointerEventHandler, HandleMouseMove));
	MyBorder->SetOnMouseDoubleClick(BIND_UOBJECT_DELEGATE(FPointerEventHandler, HandleMouseDoubleClick));
}

UClass* UBorder::GetSlotClass() const
{
	return UBorderSlot::StaticClass();
}

void UBorder::OnSlotAdded(UPanelSlot* InSlot)
{
	// Copy the content properties into the new slot so that it matches what has been setup
	// so far by the user.
	UBorderSlot* BorderSlot = CastChecked<UBorderSlot>(InSlot);
	BorderSlot->Padding = Padding;
	BorderSlot->HorizontalAlignment = HorizontalAlignment;
	BorderSlot->VerticalAlignment = VerticalAlignment;

	// Add the child to the live slot if it already exists
	if ( MyBorder.IsValid() )
	{
		// Construct the underlying slot.
		BorderSlot->BuildSlot(MyBorder.ToSharedRef());
	}
}

void UBorder::OnSlotRemoved(UPanelSlot* InSlot)
{
	// Remove the widget from the live slot if it exists.
	if ( MyBorder.IsValid() )
	{
		MyBorder->SetContent(SNullWidget::NullWidget);
	}
}

void UBorder::SetContentColorAndOpacity(FLinearColor Color)
{
	ContentColorAndOpacity = Color;
	if ( MyBorder.IsValid() )
	{
		MyBorder->SetColorAndOpacity(Color);
	}
}

void UBorder::SetPadding(FMargin InPadding)
{
	Padding = InPadding;
	if ( MyBorder.IsValid() )
	{
		MyBorder->SetPadding(InPadding);
	}
}

void UBorder::SetHorizontalAlignment(EHorizontalAlignment InHorizontalAlignment)
{
	HorizontalAlignment = InHorizontalAlignment;
	if ( MyBorder.IsValid() )
	{
		MyBorder->SetHAlign(InHorizontalAlignment);
	}
}

void UBorder::SetVerticalAlignment(EVerticalAlignment InVerticalAlignment)
{
	VerticalAlignment = InVerticalAlignment;
	if ( MyBorder.IsValid() )
	{
		MyBorder->SetVAlign(InVerticalAlignment);
	}
}

void UBorder::SetBrushColor(FLinearColor Color)
{
	BrushColor = Color;
	if ( MyBorder.IsValid() )
	{
		MyBorder->SetBorderBackgroundColor(Color);
	}
}

FReply UBorder::HandleMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& MouseEvent)
{
	if ( OnMouseButtonDownEvent.IsBound() )
	{
		return OnMouseButtonDownEvent.Execute(Geometry, MouseEvent).NativeReply;
	}

	return FReply::Unhandled();
}

FReply UBorder::HandleMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& MouseEvent)
{
	if ( OnMouseButtonUpEvent.IsBound() )
	{
		return OnMouseButtonUpEvent.Execute(Geometry, MouseEvent).NativeReply;
	}

	return FReply::Unhandled();
}

FReply UBorder::HandleMouseMove(const FGeometry& Geometry, const FPointerEvent& MouseEvent)
{
	if ( OnMouseMoveEvent.IsBound() )
	{
		return OnMouseMoveEvent.Execute(Geometry, MouseEvent).NativeReply;
	}

	return FReply::Unhandled();
}

FReply UBorder::HandleMouseDoubleClick(const FGeometry& Geometry, const FPointerEvent& MouseEvent)
{
	if ( OnMouseDoubleClickEvent.IsBound() )
	{
		return OnMouseDoubleClickEvent.Execute(Geometry, MouseEvent).NativeReply;
	}

	return FReply::Unhandled();
}

void UBorder::SetBrush(const FSlateBrush& Brush)
{
	Background = Brush;

	if ( MyBorder.IsValid() )
	{
		MyBorder->SetBorderImage(&Background);
	}
}

void UBorder::SetBrushFromAsset(USlateBrushAsset* Asset)
{
	Background = Asset ? Asset->Brush : FSlateBrush();

	if ( MyBorder.IsValid() )
	{
		MyBorder->SetBorderImage(&Background);
	}
}

void UBorder::SetBrushFromTexture(UTexture2D* Texture)
{
	Background.SetResourceObject(Texture);

	if ( MyBorder.IsValid() )
	{
		MyBorder->SetBorderImage(&Background);
	}
}

void UBorder::SetBrushFromMaterial(UMaterialInterface* Material)
{
	Background.SetResourceObject(Material);

	//TODO UMG Check if the material can be used with the UI

	if ( MyBorder.IsValid() )
	{
		MyBorder->SetBorderImage(&Background);
	}
}

UMaterialInstanceDynamic* UBorder::GetDynamicMaterial()
{
	UMaterialInterface* Material = NULL;

	UObject* Resource = Background.GetResourceObject();
	Material = Cast<UMaterialInterface>(Resource);

	if ( Material )
	{
		UMaterialInstanceDynamic* DynamicMaterial = Cast<UMaterialInstanceDynamic>(Material);

		if ( !DynamicMaterial )
		{
			DynamicMaterial = UMaterialInstanceDynamic::Create(Material, this);
			Background.SetResourceObject(DynamicMaterial);

			if ( MyBorder.IsValid() )
			{
				MyBorder->SetBorderImage(&Background);
			}
		}

		return DynamicMaterial;
	}

	//TODO UMG can we do something for textures?  General purpose dynamic material for them?

	return NULL;
}

void UBorder::SetDesiredSizeScale(FVector2D InScale)
{
	DesiredSizeScale = InScale;
	if (MyBorder.IsValid())
	{
		MyBorder->SetDesiredSizeScale(InScale);
	}
}

const FSlateBrush* UBorder::ConvertImage(TAttribute<FSlateBrush> InImageAsset) const
{
	UBorder* MutableThis = const_cast<UBorder*>( this );
	MutableThis->Background = InImageAsset.Get();

	return &Background;
}

void UBorder::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITORONLY_DATA
	if ( GetLinkerUE4Version() < VER_UE4_DEPRECATE_UMG_STYLE_ASSETS && Brush_DEPRECATED != nullptr )
	{
		Background = Brush_DEPRECATED->Brush;
		Brush_DEPRECATED = nullptr;
	}
#endif

	if ( GetChildrenCount() > 0 )
	{
		//TODO UMG Pre-Release Upgrade, now have slots of their own.  Convert existing slot to new slot.
		if ( UPanelSlot* PanelSlot = GetContentSlot() )
		{
			UBorderSlot* BorderSlot = Cast<UBorderSlot>(PanelSlot);
			if ( BorderSlot == NULL )
			{
				BorderSlot = NewObject<UBorderSlot>(this);
				BorderSlot->Content = GetContentSlot()->Content;
				BorderSlot->Content->Slot = BorderSlot;
				Slots[0] = BorderSlot;
			}
		}
	}
}

#if WITH_EDITOR

void UBorder::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	static bool IsReentrant = false;

	if ( !IsReentrant )
	{
		IsReentrant = true;

		if ( PropertyChangedEvent.Property )
		{
			static const FName PaddingName("Padding");
			static const FName HorizontalAlignmentName("HorizontalAlignment");
			static const FName VerticalAlignmentName("VerticalAlignment");

			FName PropertyName = PropertyChangedEvent.Property->GetFName();

			if ( UBorderSlot* BorderSlot = Cast<UBorderSlot>(GetContentSlot()) )
			{
				if (PropertyName == PaddingName)
				{
					FObjectEditorUtils::MigratePropertyValue(this, PaddingName, BorderSlot, PaddingName);
				}
				else if (PropertyName == HorizontalAlignmentName)
				{
					FObjectEditorUtils::MigratePropertyValue(this, HorizontalAlignmentName, BorderSlot, HorizontalAlignmentName);
				}
				else if (PropertyName == VerticalAlignmentName)
				{
					FObjectEditorUtils::MigratePropertyValue(this, VerticalAlignmentName, BorderSlot, VerticalAlignmentName);
				}
			}
		}

		IsReentrant = false;
	}
}

const FText UBorder::GetPaletteCategory()
{
	return LOCTEXT("Common", "Common");
}

#endif

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
