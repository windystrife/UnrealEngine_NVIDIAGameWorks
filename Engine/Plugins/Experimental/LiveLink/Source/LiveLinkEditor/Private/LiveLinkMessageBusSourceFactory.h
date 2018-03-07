// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LiveLinkSourceFactory.h"
#include "LiveLinkMessageBusSourceFactory.generated.h"


class SLiveLinkMessageBusSourceEditor;

UCLASS()
class ULiveLinkMessageBusSourceFactory : public ULiveLinkSourceFactory
{
public:

	GENERATED_BODY()

	virtual FText GetSourceDisplayName() const;
	virtual FText GetSourceTooltip() const;

	virtual TSharedPtr<SWidget> CreateSourceCreationPanel();
	virtual TSharedPtr<ILiveLinkSource> OnSourceCreationPanelClosed(bool bMakeSource);

	TSharedPtr<SLiveLinkMessageBusSourceEditor> ActiveSourceEditor;
};