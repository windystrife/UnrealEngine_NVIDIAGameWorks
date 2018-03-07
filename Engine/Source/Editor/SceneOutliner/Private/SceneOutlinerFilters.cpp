// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SceneOutlinerFilters.h"
#include "MultiBoxBuilder.h"

namespace SceneOutliner
{

void FOutlinerFilterInfo::InitFilter(TSharedPtr<FOutlinerFilters> InFilters)
{
	Filters = InFilters;

	ApplyFilter(bActive);
}

void FOutlinerFilterInfo::AddMenu(FMenuBuilder& InMenuBuilder)
{
	InMenuBuilder.AddMenuEntry(
		FilterTitle,
		FilterTooltip,
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateRaw( this, &FOutlinerFilterInfo::ToggleFilterActive ),
			FCanExecuteAction(),
			FIsActionChecked::CreateRaw( this, &FOutlinerFilterInfo::IsFilterActive )
		),
		NAME_None,
		EUserInterfaceActionType::ToggleButton
	);
}

void FOutlinerFilterInfo::ApplyFilter(bool bInActive)
{
	if ( !Filter.IsValid() )
	{
		Filter = Factory.Execute();
	}

	if ( bInActive )
	{			
		Filters.Pin()->Add( Filter );
	}
	else
	{
		Filters.Pin()->Remove( Filter );
	}
}

void FOutlinerFilterInfo::ToggleFilterActive()
{
	bActive = !bActive;

	ApplyFilter(bActive);
}

bool FOutlinerFilterInfo::IsFilterActive() const
{
	return bActive;
}

} // namespace SceneOutliner