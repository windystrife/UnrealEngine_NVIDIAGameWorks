// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "IPlacementMode.h"

class FEditorViewportClient;
class FViewport;
class UActorFactory;
struct FViewportClick;

class FPlacementMode : public IPlacementMode
{
public:
	FPlacementMode();
	~FPlacementMode();

	// FEdMode interface
	virtual bool UsesToolkits() const override;
	void Enter() override;
	void Exit() override;
	virtual void Tick(FEditorViewportClient* ViewportClient,float DeltaTime) override;
	virtual bool MouseEnter( FEditorViewportClient* ViewportClient,FViewport* Viewport,int32 x, int32 y ) override;
	virtual bool MouseLeave( FEditorViewportClient* ViewportClient,FViewport* Viewport ) override;
	virtual bool MouseMove( FEditorViewportClient* ViewportClient, FViewport* Viewport, int32 x, int32 y ) override;
	virtual bool InputKey( FEditorViewportClient* InViewportClient, FViewport* InViewport, FKey InKey, EInputEvent InEvent ) override;
	virtual bool EndTracking( FEditorViewportClient* InViewportClient, FViewport* InViewport ) override;
	virtual bool StartTracking( FEditorViewportClient* InViewportClient, FViewport* InViewport ) override;

	virtual bool HandleClick(FEditorViewportClient* InViewportClient, HHitProxy *HitProxy, const FViewportClick &Click ) override;
	virtual bool InputDelta( FEditorViewportClient* InViewportClient, FViewport* InViewport, FVector& InDrag, FRotator& InRot, FVector& InScale ) override;
	virtual bool ShouldDrawWidget() const override;
	virtual bool UsesPropertyWidgets() const override;
	virtual bool IsCompatibleWith(FEditorModeID OtherModeID) const override;
	// End of FEdMode interface

public: //IPlacementMode

	virtual void StopPlacing() override;

	virtual bool IsCurrentlyPlacing() const override { return AssetsToPlace.Num() > 0; };

	virtual void StartPlacing( const TArray< UObject* >& Assets, UActorFactory* Factory = NULL ) override;

	virtual UActorFactory* GetPlacingFactory() const override { return PlacementFactory.Get(); }
	virtual void SetPlacingFactory( UActorFactory* Factory ) override;
	virtual UActorFactory* FindLastUsedFactoryForAssetType( UObject* Asset ) const override;

	virtual void AddValidFocusTargetForPlacement( const TWeakPtr< SWidget >& Widget ) override { ValidFocusTargetsForPlacement.Add( Widget ); }
	virtual void RemoveValidFocusTargetForPlacement( const TWeakPtr< SWidget >& Widget ) override { ValidFocusTargetsForPlacement.Remove( Widget ); }

	virtual const TArray< TWeakObjectPtr<UObject> >& GetCurrentlyPlacingObjects() const override;

private:
	virtual void Initialize() override;

	void ClearAssetsToPlace();

	void SelectPlacedActors();

	bool AllowPreviewActors( FEditorViewportClient* ViewportClient ) const;

	void UpdatePreviewActors( FEditorViewportClient* ViewportClient, FViewport* Viewport, int32 x, int32 y );

private:

	TWeakObjectPtr< UActorFactory > PlacementFactory;
	TArray< TWeakPtr< SWidget > > ValidFocusTargetsForPlacement;

	TArray< TWeakObjectPtr< UObject > > AssetsToPlace;
	TArray< TWeakObjectPtr< AActor > > PlacedActors;

	int32 ActiveTransactionIndex;

	bool PlacementsChanged;
	bool CreatedPreviewActors;
	bool PlacedActorsThisTrackingSession;
	bool AllowPreviewActorsWhileTracking;

	TMap< FName, TWeakObjectPtr< UActorFactory > > AssetTypeToFactory;
};
