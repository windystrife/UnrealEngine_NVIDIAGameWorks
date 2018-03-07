// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Components/Spacer.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Layout/SSpacer.h"

#define LOCTEXT_NAMESPACE "UMG"

/////////////////////////////////////////////////////
// USpacer

USpacer::USpacer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, Size(1.0f, 1.0f)
{
	bIsVariable = false;
	Visibility = ESlateVisibility::SelfHitTestInvisible;
}

void USpacer::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MySpacer.Reset();
}

void USpacer::SetSize(FVector2D InSize)
{
	Size = InSize;

	if ( MySpacer.IsValid() )
	{
		MySpacer->SetSize(InSize);
	}
}

TSharedRef<SWidget> USpacer::RebuildWidget()
{
	MySpacer = SNew(SSpacer);

	//TODO UMG Consider using a design time wrapper for spacer to show expandy arrows or some other
	// indicator that there's a widget at work here.
	
	return MySpacer.ToSharedRef();
}

void USpacer::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	
	MySpacer->SetSize(Size);
}

#if WITH_EDITOR

const FText USpacer::GetPaletteCategory()
{
	return LOCTEXT("Primitive", "Primitive");
}

#endif

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
