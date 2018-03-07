// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "UObject/GCObject.h"
#include "AssetTypeActions_Base.h"
#include "GameFramework/ForceFeedbackEffect.h"
#include "TickableEditorObject.h"

struct FAssetData;
class FMenuBuilder;

struct FPreviewForceFeedbackEffect : public FActiveForceFeedbackEffect, public FTickableEditorObject, public FGCObject
{
	// FTickableEditorObject Implementation
	virtual bool IsTickable() const override;
	virtual void Tick( float DeltaTime ) override;
	virtual TStatId GetStatId() const override;

	// FGCObject Implementation
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override;
};

class FAssetTypeActions_ForceFeedbackEffect : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_ForceFeedbackEffect", "Force Feedback Effect"); }
	virtual FColor GetTypeColor() const override { return FColor(175, 0, 0); }
	virtual UClass* GetSupportedClass() const override;
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Misc; }
	virtual bool HasActions ( const TArray<UObject*>& InObjects ) const override { return true; }
	virtual void GetActions( const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder ) override;
	virtual void AssetsActivated( const TArray<UObject*>& InObjects, EAssetTypeActivationMethod::Type ActivationType ) override;
	virtual TSharedPtr<SWidget> GetThumbnailOverlay(const FAssetData& AssetData) const override;

	/** Return if the specified effect is playing*/
	bool IsEffectPlaying(const TArray<TWeakObjectPtr<UForceFeedbackEffect>>& Objects) const;

	/** Handler for when PlayEffect is selected */
	void ExecutePlayEffect(TArray<TWeakObjectPtr<UForceFeedbackEffect>> Objects);

	/** Handler for when StopEffect is selected */
	void ExecuteStopEffect(TArray<TWeakObjectPtr<UForceFeedbackEffect>> Objects);

	/** Returns true if only one effect is selected to play */
	bool CanExecutePlayCommand(TArray<TWeakObjectPtr<UForceFeedbackEffect>> Objects) const;

private:
	/** Plays the specified sound wave */
	void PlayEffect(UForceFeedbackEffect* Effect);

	/** Stops any currently playing sounds */
	void StopEffect();

	FPreviewForceFeedbackEffect PreviewForceFeedbackEffect;
};
