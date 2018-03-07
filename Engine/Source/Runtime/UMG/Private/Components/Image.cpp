// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Components/Image.h"
#include "Slate/SlateBrushAsset.h"
#include "Materials/MaterialInterface.h"
#include "Engine/Texture2D.h"
#include "Engine/Texture2DDynamic.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Images/SImage.h"

#define LOCTEXT_NAMESPACE "UMG"

/////////////////////////////////////////////////////
// UImage

UImage::UImage(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, ColorAndOpacity(FLinearColor::White)
{
}

#if WITH_EDITORONLY_DATA
void UImage::PostLoad()
{
	Super::PostLoad();

	if ( GetLinkerUE4Version() < VER_UE4_DEPRECATE_UMG_STYLE_ASSETS && Image_DEPRECATED != nullptr )
	{
		Brush = Image_DEPRECATED->Brush;
		Image_DEPRECATED = nullptr;
	}
}
#endif

void UImage::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyImage.Reset();
}

TSharedRef<SWidget> UImage::RebuildWidget()
{
	MyImage = SNew(SImage);
	return MyImage.ToSharedRef();
}

void UImage::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	TAttribute<FSlateColor> ColorAndOpacityBinding = PROPERTY_BINDING(FSlateColor, ColorAndOpacity);
	TAttribute<const FSlateBrush*> ImageBinding = OPTIONAL_BINDING_CONVERT(FSlateBrush, Brush, const FSlateBrush*, ConvertImage);

	if (MyImage.IsValid())
	{
		MyImage->SetImage(ImageBinding);
		MyImage->SetColorAndOpacity(ColorAndOpacityBinding);
		MyImage->SetOnMouseButtonDown(BIND_UOBJECT_DELEGATE(FPointerEventHandler, HandleMouseButtonDown));
	}
}

void UImage::SetColorAndOpacity(FLinearColor InColorAndOpacity)
{
	ColorAndOpacity = InColorAndOpacity;
	if ( MyImage.IsValid() )
	{
		MyImage->SetColorAndOpacity(ColorAndOpacity);
	}
}

void UImage::SetOpacity(float InOpacity)
{
	ColorAndOpacity.A = InOpacity;
	if ( MyImage.IsValid() )
	{
		MyImage->SetColorAndOpacity(ColorAndOpacity);
	}
}

const FSlateBrush* UImage::ConvertImage(TAttribute<FSlateBrush> InImageAsset) const
{
	UImage* MutableThis = const_cast<UImage*>( this );
	MutableThis->Brush = InImageAsset.Get();

	return &Brush;
}

void UImage::SetBrush(const FSlateBrush& InBrush)
{
	Brush = InBrush;

	if ( MyImage.IsValid() )
	{
		MyImage->SetImage(&Brush);
	}
}

void UImage::SetBrushFromAsset(USlateBrushAsset* Asset)
{
	Brush = Asset ? Asset->Brush : FSlateBrush();

	if ( MyImage.IsValid() )
	{
		MyImage->SetImage(&Brush);
	}
}

void UImage::SetBrushFromTexture(UTexture2D* Texture, bool bMatchSize)
{
	Brush.SetResourceObject(Texture);

	if (Texture) // Since this texture is used as UI, don't allow it affected by budget.
	{
		Texture->bIgnoreStreamingMipBias = true;
	}

	if (bMatchSize && Texture)
	{
		Brush.ImageSize.X = Texture->GetSizeX();
		Brush.ImageSize.Y = Texture->GetSizeY();
	}

	if ( MyImage.IsValid() )
	{
		MyImage->SetImage(&Brush);
	}
}

void UImage::SetBrushFromTextureDynamic(UTexture2DDynamic* Texture, bool bMatchSize)
{
	Brush.SetResourceObject(Texture);

	if (bMatchSize && Texture)
	{
		Brush.ImageSize.X = Texture->SizeX;
		Brush.ImageSize.Y = Texture->SizeY;
	}

	if (MyImage.IsValid())
	{
		MyImage->SetImage(&Brush);
	}
}

void UImage::SetBrushFromMaterial(UMaterialInterface* Material)
{
	Brush.SetResourceObject(Material);

	//TODO UMG Check if the material can be used with the UI

	if ( MyImage.IsValid() )
	{
		MyImage->SetImage(&Brush);
	}
}

UMaterialInstanceDynamic* UImage::GetDynamicMaterial()
{
	UMaterialInterface* Material = NULL;

	UObject* Resource = Brush.GetResourceObject();
	Material = Cast<UMaterialInterface>(Resource);

	if ( Material )
	{
		UMaterialInstanceDynamic* DynamicMaterial = Cast<UMaterialInstanceDynamic>(Material);

		if ( !DynamicMaterial )
		{
			DynamicMaterial = UMaterialInstanceDynamic::Create(Material, this);
			Brush.SetResourceObject(DynamicMaterial);

			if ( MyImage.IsValid() )
			{
				MyImage->SetImage(&Brush);
			}
		}
		
		return DynamicMaterial;
	}

	//TODO UMG can we do something for textures?  General purpose dynamic material for them?

	return NULL;
}

FReply UImage::HandleMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& MouseEvent)
{
	if ( OnMouseButtonDownEvent.IsBound() )
	{
		return OnMouseButtonDownEvent.Execute(Geometry, MouseEvent).NativeReply;
	}

	return FReply::Unhandled();
}

#if WITH_EDITOR

const FText UImage::GetPaletteCategory()
{
	return LOCTEXT("Common", "Common");
}

#endif


/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
