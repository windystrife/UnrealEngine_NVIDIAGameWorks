// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Array.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Templates/SharedPointer.h"
#include "Widgets/Views/SListView.h"

class FDeviceManagerModel;
class SDeviceQuickInfo;

struct FDeviceDetailsFeature;


/**
 * Implements the device details widget.
 */
class SDeviceDetails
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SDeviceDetails) { }
	SLATE_END_ARGS()

public:

	/** Destructor. */
	~SDeviceDetails();

public:

	/**
	 * Construct the widget.
	 *
	 * @param InArgs The construction arguments.
	 * @param InModel The view model to use.
	 */
	void Construct(const FArguments& InArgs, const TSharedRef<FDeviceManagerModel>& InModel);

private:

	/** The list of device features. */
	TArray<TSharedPtr<FDeviceDetailsFeature>> FeatureList;

	/** The device's feature list view. */
	TSharedPtr<SListView<TSharedPtr<FDeviceDetailsFeature>> > FeatureListView;

	/** Pointer the device manager's view model. */
	TSharedPtr<FDeviceManagerModel> Model;

	/** The quick information widget. */
	TSharedPtr<SDeviceQuickInfo> QuickInfo;
};
