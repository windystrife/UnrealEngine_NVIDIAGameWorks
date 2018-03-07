// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Internationalization/Text.h"
#include "Misc/Attribute.h"
#include "Templates/SharedPointer.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

class FMessagingDebuggerModel;
class ISlateStyle;
class STableViewBase;
class SWidget;

struct FMessageTracerEndpointInfo;


/**
 * Implements a row widget for the session console log.
 */
class SMessagingEndpointsTableRow
	: public SMultiColumnTableRow<TSharedPtr<FMessageTracerEndpointInfo>>
{
public:

	SLATE_BEGIN_ARGS(SMessagingEndpointsTableRow) { }
		SLATE_ATTRIBUTE(FText, HighlightText)
		SLATE_ARGUMENT(TSharedPtr<FMessageTracerEndpointInfo>, EndpointInfo)
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

	/** Holds the endpoint's debug information. */
	TSharedPtr<FMessageTracerEndpointInfo> EndpointInfo;

	/** Holds the highlight string for the message. */
	TAttribute<FText> HighlightText;

	/** Holds a pointer to the view model. */
	TSharedPtr<FMessagingDebuggerModel> Model;

	/** Holds the widget's visual style. */
	TSharedPtr<ISlateStyle> Style;
};
