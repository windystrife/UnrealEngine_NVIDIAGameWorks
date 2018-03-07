// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SScreenShotImagePopup.cpp: Implements the SScreenImagePopup class.
=============================================================================*/

#include "Widgets/SScreenShotImagePopup.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "EditorStyleSet.h"

void SScreenShotImagePopup::Construct( const FArguments& InArgs )
{
	// Load the image
	DynamicImageBrush = InArgs._ImageBrush;

	// Create the screen shot popup widget.
	ChildSlot
	[
		SNew(SBorder)
		. BorderImage(FEditorStyle::GetBrush("Menu.Background"))
		. Padding(10)
		[
			SNew(SBox)
			.WidthOverride(InArgs._ImageSize.X)
			.HeightOverride(InArgs._ImageSize.Y)
			[
				SNew(SImage)
				.Image( DynamicImageBrush.Get() )
			]
		]
	];
}
