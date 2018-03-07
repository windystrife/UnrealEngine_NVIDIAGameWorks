// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/SListView.h"

class FMessagingDebuggerModel;
class ISlateStyle;

struct FMessageTracerAddressInfo;


/**
 * Implements a row widget for the dispatch state list.
 */
class SMessagingAddressTableRow
	: public SMultiColumnTableRow<TSharedPtr<FMessageTracerAddressInfo>>
{
public:

	SLATE_BEGIN_ARGS(SMessagingAddressTableRow) { }
		SLATE_ARGUMENT(TSharedPtr<FMessageTracerAddressInfo>, AddressInfo)
		SLATE_ARGUMENT(TSharedPtr<ISlateStyle>, Style)
	SLATE_END_ARGS()

public:

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs The construction arguments.
	 * @param InOwnerTableView The table view that owns this row.
	 */
	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, const TSharedRef<FMessagingDebuggerModel>& InModel);

public:

	//~ SMultiColumnTableRow interface

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;

private:

	/** Callback for getting the timestamp at which the address was unregistered. */
	FText HandleTimeUnregisteredText() const;

private:

	/** Holds the address information. */
	TSharedPtr<FMessageTracerAddressInfo> AddressInfo;

	/** Holds a pointer to the view model. */
	TSharedPtr<FMessagingDebuggerModel> Model;

	/** Holds the widget's visual style. */
	TSharedPtr<ISlateStyle> Style;
};
