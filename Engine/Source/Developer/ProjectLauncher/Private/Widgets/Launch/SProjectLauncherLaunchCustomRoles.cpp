// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SProjectLauncherLaunchCustomRoles.h"


#define LOCTEXT_NAMESPACE "SProjectLauncherLaunchCustomRoles"


/* SProjectLauncherLaunchCustomRoles structors
 *****************************************************************************/

SProjectLauncherLaunchCustomRoles::~SProjectLauncherLaunchCustomRoles()
{
	if (Model.IsValid())
	{
		Model->OnProfileSelected().RemoveAll(this);
	}
}


/* SProjectLauncherLaunchCustomRoles interface
 *****************************************************************************/

void SProjectLauncherLaunchCustomRoles::Construct(const FArguments& InArgs, const TSharedRef<FProjectLauncherModel>& InModel)
{
	Model = InModel;

	ChildSlot
	[
		SNullWidget::NullWidget
	];
}


#undef LOCTEXT_NAMESPACE
