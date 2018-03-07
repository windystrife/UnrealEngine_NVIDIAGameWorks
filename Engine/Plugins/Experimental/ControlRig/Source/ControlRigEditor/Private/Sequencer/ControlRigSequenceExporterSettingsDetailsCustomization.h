// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "SlateTypes.h"

class IDetailLayoutBuilder;
class UAnimSequenceFactory;
class IPropertyHandle;
struct FAssetData;

class FControlRigSequenceExporterSettingsDetailsCustomization : public IDetailCustomization
{
public:
	FControlRigSequenceExporterSettingsDetailsCustomization();

	~FControlRigSequenceExporterSettingsDetailsCustomization();

	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	// IDetailCustomization interface
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;


private:
	bool IsAnimSequenceEnabled(TSharedRef<IPropertyHandle> SkeletalMeshHandle) const;

	void OnSkeletalMeshChanged(TSharedRef<IPropertyHandle> SkeletalMeshHandle);

	bool HandleShouldFilterAsset(const FAssetData& InAssetData, TSharedRef<IPropertyHandle> SkeletalMeshHandle);

private:
	UAnimSequenceFactory* Factory;
};
