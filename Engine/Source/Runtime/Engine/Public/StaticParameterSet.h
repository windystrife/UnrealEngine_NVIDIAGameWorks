// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"

class FSHA1;

/**
* Holds the information for a static switch parameter
*/
class FStaticSwitchParameter
{
public:
	FName ParameterName;
	bool Value;
	bool bOverride;
	FGuid ExpressionGUID;

	FStaticSwitchParameter() :
		ParameterName(TEXT("None")),
		Value(false),
		bOverride(false),
		ExpressionGUID(0, 0, 0, 0)
	{ }

	FStaticSwitchParameter(FName InName, bool InValue, bool InOverride, FGuid InGuid) :
		ParameterName(InName),
		Value(InValue),
		bOverride(InOverride),
		ExpressionGUID(InGuid)
	{ }

	friend FArchive& operator<<(FArchive& Ar, FStaticSwitchParameter& P)
	{
		Ar << P.ParameterName << P.Value << P.bOverride;
		Ar << P.ExpressionGUID;
		return Ar;
	}
};

/**
* Holds the information for a static component mask parameter
*/
class FStaticComponentMaskParameter
{
public:
	FName ParameterName;
	bool R, G, B, A;
	bool bOverride;
	FGuid ExpressionGUID;

	FStaticComponentMaskParameter() :
		ParameterName(TEXT("None")),
		R(false),
		G(false),
		B(false),
		A(false),
		bOverride(false),
		ExpressionGUID(0, 0, 0, 0)
	{ }

	FStaticComponentMaskParameter(FName InName, bool InR, bool InG, bool InB, bool InA, bool InOverride, FGuid InGuid) :
		ParameterName(InName),
		R(InR),
		G(InG),
		B(InB),
		A(InA),
		bOverride(InOverride),
		ExpressionGUID(InGuid)
	{ }

	friend FArchive& operator<<(FArchive& Ar, FStaticComponentMaskParameter& P)
	{
		Ar << P.ParameterName << P.R << P.G << P.B << P.A << P.bOverride;
		Ar << P.ExpressionGUID;
		return Ar;
	}
};

/**
* Holds the information for a static switch parameter
*/
class FStaticTerrainLayerWeightParameter
{
public:
	FName ParameterName;
	bool bOverride;
	FGuid ExpressionGUID;

	int32 WeightmapIndex;

	FStaticTerrainLayerWeightParameter() :
		ParameterName(TEXT("None")),
		bOverride(false),
		ExpressionGUID(0, 0, 0, 0),
		WeightmapIndex(INDEX_NONE)
	{ }

	FStaticTerrainLayerWeightParameter(FName InName, int32 InWeightmapIndex, bool InOverride, FGuid InGuid) :
		ParameterName(InName),
		bOverride(InOverride),
		ExpressionGUID(InGuid),
		WeightmapIndex(InWeightmapIndex)
	{ }

	friend FArchive& operator<<(FArchive& Ar, FStaticTerrainLayerWeightParameter& P)
	{
		Ar << P.ParameterName << P.WeightmapIndex << P.bOverride;
		Ar << P.ExpressionGUID;
		return Ar;
	}
};

/** Contains all the information needed to identify a single permutation of static parameters. */
class FStaticParameterSet
{
public:
	/** An array of static switch parameters in this set */
	TArray<FStaticSwitchParameter> StaticSwitchParameters;

	/** An array of static component mask parameters in this set */
	TArray<FStaticComponentMaskParameter> StaticComponentMaskParameters;

	/** An array of terrain layer weight parameters in this set */
	TArray<FStaticTerrainLayerWeightParameter> TerrainLayerWeightParameters;

	FStaticParameterSet() {}

	/** 
	* Checks if this set contains any parameters
	* 
	* @return	true if this set has no parameters
	*/
	bool IsEmpty() const
	{
		return StaticSwitchParameters.Num() == 0 && StaticComponentMaskParameters.Num() == 0 && TerrainLayerWeightParameters.Num() == 0;
	}

	void Serialize(FArchive& Ar)
	{
		// Note: FStaticParameterSet is saved both in packages (UMaterialInstance) and the DDC (FMaterialShaderMap)
		// Backwards compatibility only works with FStaticParameterSet's stored in packages.  
		// You must bump MATERIALSHADERMAP_DERIVEDDATA_VER as well if changing the serialization of FStaticParameterSet.

		Ar << StaticSwitchParameters << StaticComponentMaskParameters;
		Ar << TerrainLayerWeightParameters;
	}

	/** Updates the hash state with the static parameter names and values. */
	void UpdateHash(FSHA1& HashState) const;

	/** 
	* Indicates whether this set is equal to another, copying override settings.
	* 
	* @param ReferenceSet	The set to compare against
	* @return				true if the sets are not equal
	*/
	bool ShouldMarkDirty(const FStaticParameterSet* ReferenceSet);

	/** 
	* Tests this set against another for equality, disregarding override settings.
	* 
	* @param ReferenceSet	The set to compare against
	* @return				true if the sets are equal
	*/
	bool operator==(const FStaticParameterSet& ReferenceSet) const;

	bool operator!=(const FStaticParameterSet& ReferenceSet) const
	{
		return !(*this == ReferenceSet);
	}

	FString GetSummaryString() const;

	void AppendKeyString(FString& KeyString) const;
};
