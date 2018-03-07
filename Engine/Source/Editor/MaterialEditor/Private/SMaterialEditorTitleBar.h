// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "MaterialEditor.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"

//////////////////////////////////////////////////////////////////////////
// SMaterialEditorTitleBar

class SMaterialEditorTitleBar : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SMaterialEditorTitleBar )
		: _TitleText()
		, _MaterialInfoList(NULL)
	{}

		SLATE_ATTRIBUTE( FText, TitleText )

		SLATE_ARGUMENT( const TArray<TSharedPtr<FMaterialInfo>>*, MaterialInfoList )
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Callback used to populate SListView */
	TSharedRef<ITableRow> MakeMaterialInfoWidget(TSharedPtr<FMaterialInfo> Item, const TSharedRef<STableViewBase>& OwnerTable);

	/** Request a refresh of the list view */
	void RequestRefresh();

protected:
	TSharedPtr<SListView<TSharedPtr<FMaterialInfo>>>	MaterialInfoList;
};
