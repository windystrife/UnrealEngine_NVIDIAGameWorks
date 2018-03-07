// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Kismet/BlueprintFunctionLibrary.h"
#include "CoreMinimal.h"
#include "UObject/Script.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"

#if WITH_EDITOR
#include "Settings/LevelEditorPlaySettings.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogBlueprintFuncLibrary, Log, All);

UBlueprintFunctionLibrary::UBlueprintFunctionLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

int32 UBlueprintFunctionLibrary::GetFunctionCallspace(UFunction* Function, void* Parameters, FFrame* Stack)
{
	const bool bIsAuthoritativeFunc = Function->HasAnyFunctionFlags(FUNC_BlueprintAuthorityOnly);
	const bool bIsCosmeticFunc      = Function->HasAnyFunctionFlags(FUNC_BlueprintCosmetic);

	bool bAbsorbFunctionCall = false;
	if (bIsAuthoritativeFunc || bIsCosmeticFunc)
	{
#if !WITH_EDITOR
		// without an actor or world to give us context, we don't know what net-
		// driver to look at for "net-mode" - these functions will loop over all 
		// available worlds and make a judgment off of that
		//
		// this is relatively fast, but can be inaccurate in PIE when we're 
		// running a single process running a dedicated server as well
		// (hence, why we do all the heavy logic below to find a world context)
		const bool bAbsorbCosmeticCalls  = GEngine->ShouldAbsorbCosmeticOnlyEvent();
		const bool bAbsorbAuthorityCalls = GEngine->ShouldAbsorbAuthorityOnlyEvent();
#else
		bool bAbsorbCosmeticCalls  = false;
		bool bAbsorbAuthorityCalls = false;

		const ULevelEditorPlaySettings* PlayInSettings = GetDefault<ULevelEditorPlaySettings>();

		bool bIsOnePIEProcessEnabled = false;
		// if we're set to run our simulation under one process, then we run the 
		// risk of having conflicting world net-modes; in that scenario, the 
		// ShouldAbsorb...Event() calls are unreliable - you want to explicitly 
		// check which world the cosmetic/authoritative call would run in
		if (PlayInSettings->GetRunUnderOneProcess(bIsOnePIEProcessEnabled) && bIsOnePIEProcessEnabled)
		{
			UWorld* WorldContext = nullptr;
			if (Stack != nullptr)
			{
//				const FName WorldContextTag = TEXT("WorldContext");
				
				if (Stack->Object)
				{
					WorldContext = GEngine->GetWorldFromContextObject(Stack->Object, EGetWorldErrorMode::ReturnNull);
				}
// 				else if (Function->HasMetaData(WorldContextTag))
// 				{
// 					// @TODO: we could try to read the world context value off 
// 					//        the stack; though I am not entirely comfortable 
// 					//        without looking into it more - this would mean 
// 					//        that the script feeding the function parameters 
// 					//        would be ran twice (once here, to preview the 
// 					//        value, and then again if we decide to proceed with
// 					//        calling the function though normal stack execution),
// 					//        and side effects will be ran more than expected.
// 				}
			}

			if (WorldContext != nullptr)
			{
				ENetMode WorldNetMode = WorldContext->GetNetMode();
				bAbsorbCosmeticCalls  = (WorldNetMode == NM_DedicatedServer);
				bAbsorbAuthorityCalls = (WorldNetMode == NM_Client);
			}
			else
			{
				// fallback to old (possibly incorrect) behavior 
				bAbsorbCosmeticCalls  = GEngine->ShouldAbsorbCosmeticOnlyEvent();
				bAbsorbAuthorityCalls = GEngine->ShouldAbsorbAuthorityOnlyEvent();

				// @TODO: consider warning, if we're dropping cosmetic events 
				//        in a single process PIE session, using a dedicated 
				//        server (we don't know if this is supposed to be 
				//        running for the server world, or the client one 
				//
				//        conversely, do we want to warn when authoritative 
				//        calls are dropped? in case they're supposed to be 
				//        running on that dedicated server world?
			}
		}
		else
		{
			// presumably, these calls are sufficient if there aren't worlds 
			// with differing net-modes running within this one process... these
			// will just loop through the running worlds, and look at their 
			// net-mode
			bAbsorbCosmeticCalls  = GEngine->ShouldAbsorbCosmeticOnlyEvent();
			bAbsorbAuthorityCalls = GEngine->ShouldAbsorbAuthorityOnlyEvent();
		}
#endif //

		bAbsorbFunctionCall = (bIsAuthoritativeFunc && bAbsorbAuthorityCalls) || (bIsCosmeticFunc && bAbsorbCosmeticCalls);
	}
	// else, there is no reason to check for authoritative/cosmetic absorption
	
	return bAbsorbFunctionCall ? FunctionCallspace::Absorbed : FunctionCallspace::Local;
}

