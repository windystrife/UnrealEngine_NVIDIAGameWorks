// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Components/RetainerBox.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

#include "Slate/SRetainerWidget.h"

#define LOCTEXT_NAMESPACE "UMG"

static FName DefaultTextureParameterName("Texture");

/////////////////////////////////////////////////////
// URetainerBox

URetainerBox::URetainerBox(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Visibility = ESlateVisibility::Visible;
	Phase = 0;
	PhaseCount = 1;
	RenderOnPhase = true;
	RenderOnInvalidation = false;
	TextureParameter = DefaultTextureParameterName;
}

void URetainerBox::RequestRender()
{
	if ( MyRetainerWidget.IsValid() )
	{
		MyRetainerWidget->RequestRender();
	}
}

UMaterialInstanceDynamic* URetainerBox::GetEffectMaterial() const
{
	if ( MyRetainerWidget.IsValid() )
	{
		return MyRetainerWidget->GetEffectMaterial();
	}

	return nullptr;
}

void URetainerBox::SetEffectMaterial(UMaterialInterface* InEffectMaterial)
{
	EffectMaterial = InEffectMaterial;
	if ( MyRetainerWidget.IsValid() )
	{
		MyRetainerWidget->SetEffectMaterial(EffectMaterial);
	}
}

void URetainerBox::SetTextureParameter(FName InTextureParameter)
{
	TextureParameter = InTextureParameter;
	if ( MyRetainerWidget.IsValid() )
	{
		MyRetainerWidget->SetTextureParameter(TextureParameter);
	}
}

void URetainerBox::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyRetainerWidget.Reset();
}

TSharedRef<SWidget> URetainerBox::RebuildWidget()
{
	MyRetainerWidget =
		SNew(SRetainerWidget)
		.RenderOnInvalidation(RenderOnInvalidation)
		.RenderOnPhase(RenderOnPhase)
		.Phase(Phase)
		.PhaseCount(PhaseCount)
#if STATS
		.StatId( FName( *FString::Printf(TEXT("%s [%s]"), *GetFName().ToString(), *GetClass()->GetName() ) ) )
#endif//STATS
		;

	MyRetainerWidget->SetRetainedRendering(IsDesignTime() ? false : true);
	
	if ( GetChildrenCount() > 0 )
	{
		MyRetainerWidget->SetContent(GetContentSlot()->Content ? GetContentSlot()->Content->TakeWidget() : SNullWidget::NullWidget);
	}
	
	return MyRetainerWidget.ToSharedRef();
}

void URetainerBox::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	MyRetainerWidget->SetEffectMaterial(EffectMaterial);
	MyRetainerWidget->SetTextureParameter(TextureParameter);
	MyRetainerWidget->SetWorld(GetWorld());
}

void URetainerBox::OnSlotAdded(UPanelSlot* InSlot)
{
	// Add the child to the live slot if it already exists
	if ( MyRetainerWidget.IsValid() )
	{
		MyRetainerWidget->SetContent(InSlot->Content ? InSlot->Content->TakeWidget() : SNullWidget::NullWidget);
	}
}

void URetainerBox::OnSlotRemoved(UPanelSlot* InSlot)
{
	// Remove the widget from the live slot if it exists.
	if ( MyRetainerWidget.IsValid() )
	{
		MyRetainerWidget->SetContent(SNullWidget::NullWidget);
	}
}

#if WITH_EDITOR

const FText URetainerBox::GetPaletteCategory()
{
	return LOCTEXT("Optimization", "Optimization");
}

#endif

const FGeometry& URetainerBox::GetCachedAllottedGeometry() const
{
	if (MyRetainerWidget.IsValid())
	{
		return MyRetainerWidget->GetCachedAllottedGeometry();
	}

	static const FGeometry TempGeo;
	return TempGeo;
}

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
