// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BlueprintModes/WidgetBlueprintApplicationModes.h"

// Mode constants
const FName FWidgetBlueprintApplicationModes::DesignerMode("DesignerName");
const FName FWidgetBlueprintApplicationModes::GraphMode("GraphName");

FText FWidgetBlueprintApplicationModes::GetLocalizedMode(const FName InMode)
{
	static TMap< FName, FText > LocModes;

	if ( LocModes.Num() == 0 )
	{
		LocModes.Add(DesignerMode, NSLOCTEXT("WidgetBlueprintModes", "DesignerMode", "Designer"));
		LocModes.Add(GraphMode, NSLOCTEXT("WidgetBlueprintModes", "GraphMode", "Graph"));
	}

	check(InMode != NAME_None);
	const FText* OutDesc = LocModes.Find(InMode);
	check(OutDesc);

	return *OutDesc;
}
