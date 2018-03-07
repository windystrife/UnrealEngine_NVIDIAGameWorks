// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "Engine/MeshMerging.h"
#include "IMergeActorsTool.h"

/**
* Mesh Proxy Tool
*/
class FMeshProxyTool : public IMergeActorsTool
{
	friend class SMeshProxyDialog;

public:

	// IMergeActorsTool interface
	virtual TSharedRef<SWidget> GetWidget() override;
	virtual FName GetIconName() const override { return "MergeActors.MeshProxyTool"; }
	virtual FText GetTooltipText() const override;
	virtual FString GetDefaultPackageName() const override;
	virtual bool RunMerge(const FString& PackageName) override;
	virtual bool CanMerge() const override;
private:

	FMeshProxySettings ProxySettings;
};
