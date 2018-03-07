// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

class UMaterial;

class FBufferVisualizationData
{
public:
	
	FBufferVisualizationData()
	: bIsInitialized(false)
	{

	}

	/** Initialise the system */
	void Initialize();

	/** Check if system was initialized */
	bool IsInitialized() const { return bIsInitialized; }

	/** Get a named material from the available material map **/
	ENGINE_API UMaterial* GetMaterial(FName InMaterialName);

	/** We cache the overview material name list from the console command here, so all dynamically created views can re-use the existing cached list of materials */
	void SetCurrentOverviewMaterialNames(const FString& InNameList);
	bool IsDifferentToCurrentOverviewMaterialNames(const FString& InNameList);

	/** Access the list of materials currently in use by the buffer visualization overview */
	TArray<UMaterial*>& GetOverviewMaterials();

	/** Iterator function for iterating over available materials */
	template<class T> void IterateOverAvailableMaterials(T& Iterator) const
	{
		for (TMaterialMap::TConstIterator It = MaterialMap.CreateConstIterator(); It; ++It)
		{
			const Record& Rec = It.Value();
			Iterator.ProcessValue(Rec.Name, Rec.Material, Rec.DisplayName);
		}
	}

	/** Return the console command name for enabling single buffer visualization */
	static const TCHAR* GetVisualizationTargetConsoleCommandName()
	{
		return TEXT("r.BufferVisualizationTarget");
	}

private:

	/** Describes a single available visualization material */
	struct Record
	{
		FString Name;
		FText DisplayName;
		UMaterial* Material;
	};

	/** Mapping of FName (first parameter in ini file material list) to a material record */
	typedef TMultiMap<FName, Record> TMaterialMap;

	/** The name->material mapping table */
	TMaterialMap MaterialMap;
	
	/** List of material names to use in the buffer visualization overview */
	FString CurrentOverviewMaterialNames;

	/** List of material currently in use by the buffer visualization overview */
	TArray<UMaterial*> OverviewMaterials;
	
	/** Storage for console variable documentation strings **/
	FString ConsoleDocumentationVisualizationMode;
	FString ConsoleDocumentationOverviewTargets;

	/** Flag indicating if system is initialized **/
	bool bIsInitialized;

	/** Internal helper function for creating the buffer visualization system console commands */
	void ConfigureConsoleCommand();
};

ENGINE_API FBufferVisualizationData& GetBufferVisualizationData();
