// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SAddContentDialog.h"
#include "Widgets/Layout/SBorder.h"
#include "EditorStyleSet.h"
#include "SAddContentWidget.h"

#define LOCTEXT_NAMESPACE "AddContentDialog"

void SAddContentDialog::Construct(const FArguments& InArgs)
{
	SAssignNew(AddContentWidget, SAddContentWidget);

	SWindow::Construct(SWindow::FArguments()
		.Title(LOCTEXT("AddContentDialogTitle", "Add Content to the Project"))
		.SizingRule(ESizingRule::UserSized)
		.ClientSize(FVector2D(900, 500))
		.SupportsMinimize(false)
		.SupportsMaximize(false)
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(FMargin(15))
			[
				SNew(SVerticalBox)

				// Add content widget.
				+ SVerticalBox::Slot()
				[
					AddContentWidget.ToSharedRef()
				]
			]
		]);
}

SAddContentDialog::~SAddContentDialog()
{
}

#undef LOCTEXT_NAMESPACE
