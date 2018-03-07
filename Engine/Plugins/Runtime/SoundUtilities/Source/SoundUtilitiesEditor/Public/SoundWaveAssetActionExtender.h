// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AudioEditorModule.h"


class FSoundWaveAssetActionExtender :	public ISoundWaveAssetActionExtensions,
												public TSharedFromThis<FSoundWaveAssetActionExtender>
{
public:
	virtual ~FSoundWaveAssetActionExtender() {}

	virtual void GetExtendedActions(const TArray<TWeakObjectPtr<USoundWave>>& InObjects, FMenuBuilder& MenuBuilder) override;

	void ExecuteCreateSimpleSound(TArray<TWeakObjectPtr<USoundWave>> Objects);
};

