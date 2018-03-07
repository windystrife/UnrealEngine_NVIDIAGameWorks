// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimStateNodeDetails.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Text/STextBlock.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "DetailCategoryBuilder.h"

#define LOCTEXT_NAMESPACE "FAnimStateNodeDetails"

/////////////////////////////////////////////////////////////////////////


TSharedRef<IDetailCustomization> FAnimStateNodeDetails::MakeInstance()
{
	return MakeShareable( new FAnimStateNodeDetails );
}

void FAnimStateNodeDetails::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{
	IDetailCategoryBuilder& SegmentCategory = DetailBuilder.EditCategory("Animation State", LOCTEXT("AnimationStateCategoryTitle", "Animation State") );

	{
		SegmentCategory.AddCustomRow( LOCTEXT("EnteredAnimationStateEventLabel", "Entered State Event") )
		[
			SNew( STextBlock )
			.Text( LOCTEXT("EnteredAnimationStateEventLabel", "Entered State Event") )
			.Font( IDetailLayoutBuilder::GetDetailFontBold() )
		];
		CreateTransitionEventPropertyWidgets(SegmentCategory, TEXT("StateEntered"));
	}


	{
		SegmentCategory.AddCustomRow( LOCTEXT("ExitedAnimationStateEventLabel", "Left State Event") )
		[
			SNew( STextBlock )
			.Text( LOCTEXT("ExitedAnimationStateEventLabel", "Left State Event")  )
			.Font( IDetailLayoutBuilder::GetDetailFontBold() )
		];
		CreateTransitionEventPropertyWidgets(SegmentCategory, TEXT("StateLeft"));
	}

	{

		SegmentCategory.AddCustomRow( LOCTEXT("FullyBlendedAnimationStateEventLabel", "Fully Blended State Event") )
		[
			SNew( STextBlock )
			.Text( LOCTEXT("FullyBlendedAnimationStateEventLabel", "Fully Blended State Event") )
			.Font( IDetailLayoutBuilder::GetDetailFontBold() )
		];
		CreateTransitionEventPropertyWidgets(SegmentCategory, TEXT("StateFullyBlended"));
	}


	DetailBuilder.HideProperty("StateEntered");
	DetailBuilder.HideProperty("StateLeft");
	DetailBuilder.HideProperty("StateFullyBlended");
}

#undef LOCTEXT_NAMESPACE
