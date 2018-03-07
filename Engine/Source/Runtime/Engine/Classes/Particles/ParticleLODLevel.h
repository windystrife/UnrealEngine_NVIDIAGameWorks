// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
 *	ParticleLODLevel
 *
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "ParticleLODLevel.generated.h"

class UInterpCurveEdSetup;
class UParticleModule;

UCLASS(collapsecategories, hidecategories=Object, editinlinenew, MinimalAPI)
class UParticleLODLevel : public UObject
{
	GENERATED_UCLASS_BODY()

	/** The index value of the LOD level												*/
	UPROPERTY()
	int32 Level;

	/** True if the LOD level is enabled, meaning it should be updated and rendered.	*/
	UPROPERTY()
	uint32 bEnabled:1;

	/** The required module for this LOD level											*/
	UPROPERTY(instanced)
	class UParticleModuleRequired* RequiredModule;

	/** An array of particle modules that contain the adjusted data for the LOD level	*/
	UPROPERTY(instanced)
	TArray<class UParticleModule*> Modules;

	// Module<SINGULAR> used for emitter type "extension".
	UPROPERTY(export)
	class UParticleModuleTypeDataBase* TypeDataModule;

	/** The SpawnRate/Burst module - required by all emitters. */
	UPROPERTY(export)
	class UParticleModuleSpawn* SpawnModule;

	/** The optional EventGenerator module. */
	UPROPERTY(export)
	class UParticleModuleEventGenerator* EventGenerator;

	/** SpawningModules - These are called to determine how many particles to spawn.	*/
	UPROPERTY(transient, duplicatetransient)
	TArray<class UParticleModuleSpawnBase*> SpawningModules;

	/** SpawnModules - These are called when particles are spawned.						*/
	UPROPERTY(transient, duplicatetransient)
	TArray<class UParticleModule*> SpawnModules;

	/** UpdateModules - These are called when particles are updated.					*/
	UPROPERTY(transient, duplicatetransient)
	TArray<class UParticleModule*> UpdateModules;

	/** OrbitModules 
	 *	These are used to do offsets of the sprite from the particle location.
	 */
	UPROPERTY(transient, duplicatetransient)
	TArray<class UParticleModuleOrbit*> OrbitModules;

	/** Event receiver modules only! */
	UPROPERTY(transient, duplicatetransient)
	TArray<class UParticleModuleEventReceiverBase*> EventReceiverModules;

	UPROPERTY()
	uint32 ConvertedModules:1;

	UPROPERTY()
	int32 PeakActiveParticles;


	//~ Begin UObject Interface
	virtual void	PostLoad() override;
	//~ End UObject Interface

	// @todo document
	virtual void	UpdateModuleLists();

	// @todo document
	virtual bool	GenerateFromLODLevel(UParticleLODLevel* SourceLODLevel, float Percentage, bool bGenerateModuleData = true);

	/**
	 *	CalculateMaxActiveParticleCount
	 *	Determine the maximum active particles that could occur with this emitter.
	 *	This is to avoid reallocation during the life of the emitter.
	 *
	 *	@return		The maximum active particle count for the LOD level.
	 */
	virtual int32	CalculateMaxActiveParticleCount();

	/** Update to the new SpawnModule method */
	void	ConvertToSpawnModule();
		
	/** @return the index of the given module if it is contained in the LOD level */
	int32		GetModuleIndex(UParticleModule* InModule);

	/** @return the module at the given index if it is contained in the LOD level */
	ENGINE_API UParticleModule* GetModuleAtIndex(int32 InIndex);

	/**
	 *	Sets the LOD 'Level' to the given value, properly updating the modules LOD validity settings.
	 *	This function assumes that any error-checking of values was done by the caller!
	 *	It also assumes that when inserting an LOD level, indices will be shifted from lowest to highest...
	 *	When removing one, the will go from highest to lowest.
	 */
	virtual void	SetLevelIndex(int32 InLevelIndex);

	// For Cascade
	void	AddCurvesToEditor(UInterpCurveEdSetup* EdSetup);
	void	RemoveCurvesFromEditor(UInterpCurveEdSetup* EdSetup);
	void	ChangeEditorColor(FColor& Color, UInterpCurveEdSetup* EdSetup);

	/**
	 *	Return true if the given module is editable for this LOD level.
	 *	
	 *	@param	InModule	The module of interest.
	 *	@return	true		If it is editable for this LOD level.
	 *			false		If it is not.
	 */
	ENGINE_API bool	IsModuleEditable(UParticleModule* InModule);

	/**
	 * Compiles all modules for this LOD level.
	 * @param EmitterBuildInfo - Where to store emitter information.
	 */
	void CompileModules( struct FParticleEmitterBuildInfo& EmitterBuildInfo );
};



