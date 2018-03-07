// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameplayEffect.h"
#include "GameplayEffectTemplate.generated.h"

/**
 *	The idea here is that UGameplayEffectTemplates are templates created by system designers for commonly used
 *	GameplayEffects. For example: StandardPhysicalDamage, StandardSnare, etc. 
 *	
 *	This allows for A) Less experienced designers to create new ones based on the templates B) Easier/quicker creation
 *	of GameplayEffects: don't have to worry about forgeting a tag or stacking rule, etc.
 *	
 *	The template provides: 
 *		-Default values
 *		-What properties are editable by default
 *		
 *	Default values are simply the default values of the UGameplayEffectTemplate (since it is a UGameplayEffect). Whatever values
 *	are different than CDO values will be copied over when making a new GameplayEffect from a template.
 *	
 *	EditableProperties describes what properties should be exposed by default when editing a templated gameplayeffect. This is just
 *	a string list. Properties whose name match strings in that list will be shown, the rest will be hidden. All properties can always be
 *	seen/edited by just checking "ShowAllProperties" on the GameplayEffect itself. 
 *	
 *	
 */
UCLASS()
class UGameplayEffectTemplate : public UGameplayEffect
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITORONLY_DATA
	/** Names of the properties that should be exposed in the limited/basic view when editing a GameplayEffect based on this template */
	UPROPERTY(EditDefaultsOnly, Category="Template")
	TArray<FString>	EditableProperties;
#endif
};
