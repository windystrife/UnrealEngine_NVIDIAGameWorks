// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/ScriptMacros.h"
#include "Curves/CurveOwnerInterface.h"
#include "CurveBase.generated.h"

/**
 * Defines a curve of interpolated points to evaluate over a given range
 */
UCLASS(abstract)
class ENGINE_API UCurveBase
	: public UObject
	, public FCurveOwnerInterface
{
	GENERATED_UCLASS_BODY()

	/** Get the time range across all curves */
	UFUNCTION(BlueprintCallable, Category="Math|Curves")
	void GetTimeRange(float& MinTime, float& MaxTime) const;

	/** Get the value range across all curves */
	UFUNCTION(BlueprintCallable, Category="Math|Curves")
	void GetValueRange(float& MinValue, float& MaxValue) const;

	/** 
	 *	Create curve from CSV style comma-separated string.
	 *
	 * @param InString The input string to parse.
	 *	@return	Set of problems encountered while processing input
	 */
	TArray<FString> CreateCurveFromCSVString(const FString& InString);

	/** Reset all curve data */
	void ResetCurve();

public:

	//~ FCurveOwnerInterface interface

	virtual TArray<FRichCurveEditInfoConst> GetCurves() const override
	{
		TArray<FRichCurveEditInfoConst> Curves;
		return Curves;
	}

	virtual TArray<FRichCurveEditInfo> GetCurves() override
	{
		TArray<FRichCurveEditInfo> Curves;
		return Curves;
	}

	virtual void ModifyOwner() override;
	virtual TArray<const UObject*> GetOwners() const override;
	virtual void MakeTransactional() override;
	virtual void OnCurveChanged(const TArray<FRichCurveEditInfo>& ChangedCurveEditInfos) override;
	
	virtual bool RepointCurveOwner(const FPackageReloadedEvent& InPackageReloadedEvent, FCurveOwnerInterface*& OutNewCurveOwner) const override
	{
		return RepointCurveOwnerAsset(InPackageReloadedEvent, this, OutNewCurveOwner);
	}

	virtual bool IsValidCurve(FRichCurveEditInfo CurveInfo) override
	{
		return false;
	};

public:

	//~ UObject interface

#if WITH_EDITORONLY_DATA

	/** Override to ensure we write out the asset import data */
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
	virtual void PostInitProperties() override;
	virtual void PostLoad() override;

	UPROPERTY(VisibleAnywhere, Instanced, Category=ImportSettings)
	class UAssetImportData* AssetImportData;

public:

	/** The filename imported to create this object. Relative to this object's package, BaseDir() or absolute */
	UPROPERTY()
	FString ImportPath_DEPRECATED;

#endif
};
