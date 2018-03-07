// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "NiagaraCommon.h"
#include "NiagaraDataSet.h"
#include "NiagaraEmitterInstance.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraSystem.generated.h"

//TODO:
/** Defines a number of values and defaults shared among all Niagara Systems that use this category. */
// UCLASS()
// class NIAGARA_API UNiagaraSystemCategory : public UObject
// {
// 	GENERATED_UCLASS_BODY()
// 
// 	/** Name of this System Category. */
// 	UPROPERTY(EditAnywhere, Category=Category)
// 	FName Name;
// 
// 	/** Category specific to the global instance of a particular Niagara Parameter Collection. */
// 	UPROPERTY(EditAnywhere, Category=Category)
// 	TArray<UNiagaraParameterCollectionInstance*> ParameterCollectionOverrides;
// 
// 	/** TODO: How to do an icon? */
// 	//UPROPERTY(EditAnywhere, Category = Category)
// 	//UTexture2D* Icon
// };

USTRUCT()
struct FNiagaraEmitterSpawnAttributes
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TArray<FName> SpawnAttributes;
};

/** Container for multiple emitters that combine together to create a particle system effect.*/
UCLASS()
class NIAGARA_API UNiagaraSystem : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	//~ UObject interface
	void PostInitProperties();
	void Serialize(FArchive& Ar)override;
	virtual void PostLoad() override;

	/** Gets an array of the emitter handles. */
	const TArray<FNiagaraEmitterHandle>& GetEmitterHandles();

	/** Returns true if this system is valid and can be instanced. False otherwise. */
	bool IsValid()const;

#if WITH_EDITORONLY_DATA
	/** Adds a new emitter handle to this System.  The new handle exposes an Instance value which is a copy of the
		original asset. */
	FNiagaraEmitterHandle AddEmitterHandle(const UNiagaraEmitter& SourceEmitter, FName EmitterName);

	/** Adds a new emitter handle to this System.  The new handle will not copy the emitter and any changes made to it's
		Instance value will modify the original asset.  This should only be used in the emitter toolkit for simulation
		purposes. */
	FNiagaraEmitterHandle AddEmitterHandleWithoutCopying(UNiagaraEmitter& Emitter);

	/** Duplicates an existing emitter handle and adds it to the System.  The new handle will reference the same source asset,
		but will have a copy of the duplicated Instance value. */
	FNiagaraEmitterHandle DuplicateEmitterHandle(const FNiagaraEmitterHandle& EmitterHandleToDuplicate, FName EmitterName);
#endif

	/** Removes the provided emitter handle. */
	void RemoveEmitterHandle(const FNiagaraEmitterHandle& EmitterHandleToDelete);

	/** Removes the emitter handles which have an Id in the supplied set. */
	void RemoveEmitterHandlesById(const TSet<FGuid>& HandlesToRemove);

	FNiagaraEmitterHandle& GetEmitterHandle(int Idx)
	{
		check(Idx < EmitterHandles.Num());
		return EmitterHandles[Idx];
	};

	const FNiagaraEmitterHandle& GetEmitterHandle(int Idx) const
	{
		check(Idx < EmitterHandles.Num());
		return EmitterHandles[Idx];
	};

	int GetNumEmitters()
	{
		return EmitterHandles.Num();
	}

	/** From the last compile, what are the variables that were exported out of the system for external use?*/
	const FNiagaraParameterStore& GetExposedParameters() const {	return ExposedParameters; }
	FNiagaraParameterStore& GetExposedParameters()  { return ExposedParameters; }


	/** Gets the System script which is used to populate the System parameters and parameter bindings. */
	UNiagaraScript* GetSystemSpawnScript(bool bSolo=false);
	UNiagaraScript* GetSystemUpdateScript(bool bSolo=false);

#if WITH_EDITORONLY_DATA
	/** Get whether or not we auto-import applied changes. See bAutoImportChangedEmitters member documentation for details.*/
	bool GetAutoImportChangedEmitters() const;
	/** Set whether or not we auto-import applied changes. See bAutoImportChangedEmitters member documentation for details.*/
	void SetAutoImportChangedEmitters(bool bShouldImport);
		
	/** Called to query whether or not this emitter is referenced as the source to any emitter handles for this System.*/
	bool ReferencesSourceEmitter(UNiagaraEmitter* Emitter);

	/** Goes through all handles on the System and compares them to the latest on their source. Refreshes from source if needed. Returns true if any were refreshed.*/
	bool ResynchronizeAllHandles();

	/** Recompile all emitters and the System script associated with this System.*/
	void Compile();

	/** Compile just the System script associated with this System.*/
	void CompileScripts(TArray<ENiagaraScriptCompileStatus>& OutScriptStatuses, TArray<FString>& OutGraphLevelErrorMessages, TArray<FString>& PathNames, TArray<UNiagaraScript*>& Scripts);

	/** Uses whether or not the GetAutoImportChangedEmitters is set to ResynchronizeAllHandles.*/
	bool CheckForUpdates();

	/** Gets editor specific data stored with this system. */
	UObject* GetEditorData();

	/** Gets editor specific data stored with this system. */
	const UObject* GetEditorData() const;

	/** Sets editor specific data stored with this system. */
	void SetEditorData(UObject* InEditorData);

#endif

	const TArray<FNiagaraEmitterSpawnAttributes>& GetEmitterSpawnAttributes()const {	return EmitterSpawnAttributes;	};

protected:
	/** Handles to the emitter this System will simulate. */
	UPROPERTY(VisibleAnywhere, Category = "Emitters")
	TArray<FNiagaraEmitterHandle> EmitterHandles;

// 	/** Category of this system. */
// 	UPROPERTY(EditAnywhere, Category = System)
// 	UNiagaraSystemCategory* Category;

	/** The script which defines the System parameters, and which generates the bindings from System
		parameter to emitter parameter. */
	UPROPERTY()
	UNiagaraScript* SystemSpawnScript;

	/** The script which defines the System parameters, and which generates the bindings from System
	parameter to emitter parameter. */
	UPROPERTY()
	UNiagaraScript* SystemUpdateScript;

	/** Spawn script compiled to be run individually on a single instance of the system rather than batched as the main spawn script */
	UPROPERTY()
	UNiagaraScript* SystemSpawnScriptSolo;

	/** Update script compiled to be run individually on a single instance of the system rather than batched as the main spawn script */
	UPROPERTY()
	UNiagaraScript* SystemUpdateScriptSolo;

	/** Attribute names in the data set that are driving each emitter's spawning. */
	UPROPERTY()
	TArray<FNiagaraEmitterSpawnAttributes> EmitterSpawnAttributes;

	/** Variables exposed to the outside work for tweaking*/
	UPROPERTY()
	FNiagaraParameterStore ExposedParameters;

#if WITH_EDITORONLY_DATA
	/** Systems are the final step in the process of creating a Niagara system. Artists may wish to lock an System so that it only uses
	the handle's cached version of the scripts, rather than the external assets that may be subject to changes. If this flag is set, we will only update
	the emitters if told to do so explicitly by the user.*/
	UPROPERTY()
	uint32 bAutoImportChangedEmitters : 1;	

	/** Data used by the editor to maintain UI state etc.. */
	UPROPERTY()
	UObject* EditorData;
#endif

	void InitEmitterSpawnAttributes();
};
