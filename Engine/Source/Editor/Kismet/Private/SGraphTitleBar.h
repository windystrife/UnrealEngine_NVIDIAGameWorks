// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlateFwd.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "BlueprintUtilities.h"

class FBlueprintEditor;
class UEdGraph;
struct FSlateBrush;

//////////////////////////////////////////////////////////////////////////
// SGraphTitleBar

class SGraphTitleBar : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SGraphTitleBar )
		: _EdGraphObj(NULL)
		, _Kismet2()
		{}

		SLATE_ARGUMENT( UEdGraph*, EdGraphObj )
		SLATE_ARGUMENT( TWeakPtr<FBlueprintEditor>, Kismet2 )
		SLATE_EVENT( FEdGraphEvent, OnDifferentGraphCrumbClicked )
		SLATE_ARGUMENT( TSharedPtr<SWidget>, HistoryNavigationWidget )
	SLATE_END_ARGS()

	/** SGraphTitleBar destructor */
	~SGraphTitleBar();

	void Construct(const FArguments& InArgs);

	/** Refresh the toolbar */
	void Refresh();

protected:
	/** Owning Kismet 2 */
	TWeakPtr<FBlueprintEditor> Kismet2Ptr;

	/** Edited graph */
	UEdGraph* EdGraphObj;

	/** Pointer to the function editor widget */
	TWeakPtr<class SFunctionEditor>	FuncEditorPtr;

	/** Breadcrumb trail widget */
	TSharedPtr< SBreadcrumbTrail<UEdGraph*> > BreadcrumbTrail;

	/** Callback to call when the user wants to change the active graph via the breadcrumb trail */
	FEdGraphEvent OnDifferentGraphCrumbClicked;

	/** Should we show graph's blueprint title */
	bool bShowBlueprintTitle;

	/** Blueprint title being displayed for toolbar */
	FText BlueprintTitle;


protected:
	/** Get the icon to use */
	const FSlateBrush* GetTypeGlyph() const;

	/** Get the extra title text */
	FText GetTitleExtra() const;

	/** Helper methods */
	EVisibility IsGraphBlueprintNameVisible() const;

	void OnBreadcrumbClicked(UEdGraph* const & Item);

	void RebuildBreadcrumbTrail();

	static FText GetTitleForOneCrumb(const UEdGraph* Graph);

	/** Function to fetch outer class which is of type UEGraph. */
	UEdGraph* GetOuterGraph( UObject* Obj );

	/** Helper method used to show blueprint title in breadcrumbs */
	FText GetBlueprintTitle() const;
};
