// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "SoundEffectPreset.generated.h"

struct FAssetData;
class FMenuBuilder;
class IToolkitHost;
class FSoundEffectBase;

UCLASS(config = Engine, abstract, editinlinenew, BlueprintType)
class ENGINE_API USoundEffectPreset : public UObject
{
	GENERATED_BODY()

public:
	USoundEffectPreset(const FObjectInitializer& ObjectInitializer);

	virtual FText GetAssetActionName() const PURE_VIRTUAL(USoundEffectPreset::GetAssetActionName, return FText(););
	virtual UClass* GetSupportedClass() const PURE_VIRTUAL(USoundEffectPreset::GetSupportedClass, return nullptr;);
	virtual USoundEffectPreset* CreateNewPreset(UObject* InParent, FName Name, EObjectFlags Flags) const PURE_VIRTUAL(USoundEffectPreset::CreateNewPreset, return nullptr;);
	virtual FSoundEffectBase* CreateNewEffect() const PURE_VIRTUAL(USoundEffectPreset::CreateNewEffect, return nullptr;);
	virtual bool HasAssetActions() const { return true; }
	virtual void Init() PURE_VIRTUAL(USoundEffectPreset::Init, ); 
	virtual void OnInit() {};
	virtual FColor GetPresetColor() const { return FColor(200.0f, 100.0f, 100.0f); }

	void EffectCommand(TFunction<void()> Command);
	void Update();
	void AddEffectInstance(FSoundEffectBase* InSource);
	void RemoveEffectInstance(FSoundEffectBase* InSource);

protected:

#if WITH_EDITORONLY_DATA
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	// Array of instances which are using this preset
	TArray<FSoundEffectBase*> Instances;
	bool bInitialized;
};