// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SkeletonNotifyDetails.h"
#include "Fonts/SlateFontInfo.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/SListView.h"
#include "Animation/EditorSkeletonNotifyObj.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailPropertyRow.h"
#include "DetailCategoryBuilder.h"
#include "IDetailsView.h"

#define LOCTEXT_NAMESPACE "SkeletonNotifyDetails"

TSharedRef<IDetailCustomization> FSkeletonNotifyDetails::MakeInstance()
{
	return MakeShareable( new FSkeletonNotifyDetails );
}

void FSkeletonNotifyDetails::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{
	IDetailCategoryBuilder& Category = DetailBuilder.EditCategory("Skeleton Notify", LOCTEXT("SkeletonNotifyCategoryName", "Skeleton Notify") );
	const FSlateFontInfo DetailFontInfo = IDetailLayoutBuilder::GetDetailFont();

	Category.AddProperty("Name").DisplayName( LOCTEXT("SkeletonNotifyName", "Notify Name") );

	TSharedPtr<IPropertyHandle> InPropertyHandle = DetailBuilder.GetProperty("AnimationNames");

	TArray< TWeakObjectPtr<UObject> > SelectedObjects =  DetailBuilder.GetSelectedObjects();

	UEditorSkeletonNotifyObj* EdObj = NULL;
	for(int i = 0; i < SelectedObjects.Num(); ++i)
	{
		UObject* Obj = SelectedObjects[i].Get();
		EdObj = Cast<UEditorSkeletonNotifyObj>(Obj);
		if(EdObj)
		{
			break;
		}
	}

	if(EdObj)
	{
		Category.AddCustomRow(LOCTEXT("AnimationsLabel","Animations"))
		.NameContent()
		[
			SNew(STextBlock)
			.ToolTipText(LOCTEXT("Animations_Tooltip", "List of animations that reference this notify"))
			.Text( LOCTEXT("AnimationsLabel","Animations") )
			.Font( DetailFontInfo )
		]
		.ValueContent()
		[
			SNew(SListView<TSharedPtr<FString>>)
			.ListItemsSource(&EdObj->AnimationNames)
			.OnGenerateRow(this, &FSkeletonNotifyDetails::MakeAnimationRow)
		];
	}
}

TSharedRef< ITableRow > FSkeletonNotifyDetails::MakeAnimationRow( TSharedPtr<FString> Item, const TSharedRef< STableViewBase >& OwnerTable )
{
	return SNew(STableRow<TSharedPtr<FString>>, OwnerTable)
	[
		SNew(STextBlock).Text(FText::FromString(*Item.Get()))
	];
}

#undef LOCTEXT_NAMESPACE
