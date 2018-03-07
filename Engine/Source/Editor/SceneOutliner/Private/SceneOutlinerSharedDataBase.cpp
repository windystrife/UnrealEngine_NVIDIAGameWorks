// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"

#include "SceneOutlinerPublicTypes.h"

#include "SceneOutlinerModule.h"

namespace SceneOutliner
{

void FSharedDataBase::UseDefaultColumns()
{
	FSceneOutlinerModule& SceneOutlinerModule = FModuleManager::LoadModuleChecked<FSceneOutlinerModule>("SceneOutliner");

	for (auto& DefaultColumn : SceneOutlinerModule.DefaultColumnMap)
	{
		if (!DefaultColumn.Value.ValidMode.IsSet() || Mode == DefaultColumn.Value.ValidMode.GetValue())
		{
			ColumnMap.Add(DefaultColumn.Key, DefaultColumn.Value.ColumnInfo);
		}
	}
}

}	// namespace SceneOutliner 