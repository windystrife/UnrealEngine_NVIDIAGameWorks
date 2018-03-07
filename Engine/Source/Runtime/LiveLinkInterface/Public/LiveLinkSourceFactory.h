// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ObjectMacros.h"
#include "Object.h"
#include "LiveLinkSourceFactory.generated.h"

class ILiveLinkSource;
class ILiveLinkClient;
class SWidget;

UCLASS()
class LIVELINKINTERFACE_API ULiveLinkSourceFactory : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	virtual FText GetSourceDisplayName() const PURE_VIRTUAL( ULiveLinkSourceFactory::GetSourceDisplayName, return FText(); );
	virtual FText GetSourceTooltip() const PURE_VIRTUAL(ULiveLinkSourceFactory::GetSourceTooltip, return FText(); );

	virtual TSharedPtr<SWidget> CreateSourceCreationPanel() PURE_VIRTUAL(ULiveLinkSourceFactory::CreateSourceCreationPanel, return nullptr;);
	virtual TSharedPtr<ILiveLinkSource> OnSourceCreationPanelClosed(bool bCreateSource) PURE_VIRTUAL(ULiveLinkSourceFactory::OnSourceCreationPanelClosed, return nullptr;);
};