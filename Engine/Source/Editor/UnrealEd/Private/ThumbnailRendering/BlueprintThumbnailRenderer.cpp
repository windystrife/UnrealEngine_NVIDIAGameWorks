// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ThumbnailRendering/BlueprintThumbnailRenderer.h"
#include "ShowFlags.h"
#include "SceneView.h"
#include "Misc/App.h"

// FPreviewScene derived helpers for rendering
#include "RendererInterface.h"
#include "EngineModule.h"
#include "Engine/SCS_Node.h"

UBlueprintThumbnailRenderer::UBlueprintThumbnailRenderer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UBlueprintThumbnailRenderer::CanVisualizeAsset(UObject* Object)
{
	UBlueprint* Blueprint = Cast<UBlueprint>(Object);

	// Only visualize actor based blueprints
	if (Blueprint && Blueprint->GeneratedClass && Blueprint->GeneratedClass->IsChildOf(AActor::StaticClass()))
	{
		// Try to find any visible primitive components in the native class' CDO
		AActor* CDO = Blueprint->GeneratedClass->GetDefaultObject<AActor>();

		TInlineComponentArray<UActorComponent*> Components;
		CDO->GetComponents(Components);

		for (auto CompIt = Components.CreateConstIterator(); CompIt; ++CompIt)
		{
			if (FBlueprintThumbnailScene::IsValidComponentForVisualization(*CompIt))
			{
				return true;
			}
		}

		// Try to find any visible primitive components in the simple construction script
		// Do this for all parent blueprint generated classes as well
		UBlueprint* BlueprintToHarvestComponents = Blueprint;
		TSet<UBlueprint*> AllVisitedBlueprints;
		while (BlueprintToHarvestComponents)
		{
			AllVisitedBlueprints.Add(BlueprintToHarvestComponents);

			if (BlueprintToHarvestComponents->SimpleConstructionScript)
			{
				for (USCS_Node* Node : BlueprintToHarvestComponents->SimpleConstructionScript->GetAllNodes())
				{
					if (FBlueprintThumbnailScene::IsValidComponentForVisualization(Node->ComponentTemplate))
					{
						return true;
					}
				}
			}

			UClass* ParentClass = BlueprintToHarvestComponents->ParentClass;
			BlueprintToHarvestComponents = nullptr;

			// If the parent class was a blueprint generated class, check it's simple construction script components as well
			if (ParentClass)
			{
				UBlueprint* ParentBlueprint = Cast<UBlueprint>(ParentClass->ClassGeneratedBy);

				// Also make sure we haven't visited the blueprint already. This would only happen if there was a loop of parent classes.
				if (ParentBlueprint && !AllVisitedBlueprints.Contains(ParentBlueprint))
				{
					BlueprintToHarvestComponents = ParentBlueprint;
				}
			}
		}
	}

	return false;
}

void UBlueprintThumbnailRenderer::Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget* RenderTarget, FCanvas* Canvas)
{
	UBlueprint* Blueprint = Cast<UBlueprint>(Object);

	// Strict validation - it may hopefully fix UE-35705.
	const bool bIsBlueprintValid = IsValid(Blueprint)
		&& IsValid(Blueprint->GeneratedClass)
		&& Blueprint->bHasBeenRegenerated
		//&& Blueprint->IsUpToDate() - This condition makes the thumbnail blank whenever the BP is dirty. It seems too strict.
		&& !Blueprint->bBeingCompiled
		&& !Blueprint->HasAnyFlags(RF_Transient);
	if (bIsBlueprintValid)
	{
		TSharedRef<FBlueprintThumbnailScene> ThumbnailScene = ThumbnailScenes.EnsureThumbnailScene(Blueprint->GeneratedClass);

		ThumbnailScene->SetBlueprint(Blueprint);
		FSceneViewFamilyContext ViewFamily( FSceneViewFamily::ConstructionValues( RenderTarget, ThumbnailScene->GetScene(), FEngineShowFlags(ESFIM_Game) )
			.SetWorldTimes(FApp::GetCurrentTime() - GStartTime, FApp::GetDeltaTime(), FApp::GetCurrentTime() - GStartTime));

		ViewFamily.EngineShowFlags.DisableAdvancedFeatures();
		ViewFamily.EngineShowFlags.MotionBlur = 0;

		ThumbnailScene->GetView(&ViewFamily, X, Y, Width, Height);
		GetRendererModule().BeginRenderingViewFamily(Canvas,&ViewFamily);
	}
}

void UBlueprintThumbnailRenderer::BeginDestroy()
{
	ThumbnailScenes.Clear();

	Super::BeginDestroy();
}

void UBlueprintThumbnailRenderer::BlueprintChanged(class UBlueprint* Blueprint)
{
	if (Blueprint && Blueprint->GeneratedClass)
	{
		TSharedPtr<FBlueprintThumbnailScene> ThumbnailScene = ThumbnailScenes.FindThumbnailScene(Blueprint->GeneratedClass);
		if (ThumbnailScene.IsValid())
		{
			ThumbnailScene->BlueprintChanged(Blueprint);
		}
	}
}
