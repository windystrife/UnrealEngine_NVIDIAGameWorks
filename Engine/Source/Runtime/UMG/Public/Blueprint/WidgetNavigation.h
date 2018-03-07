// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/WeakObjectPtr.h"
#include "Types/NavigationMetaData.h"

#include "WidgetNavigation.generated.h"

class UWidget;

/**
 *
 */
USTRUCT(BlueprintType)
struct UMG_API FWidgetNavigationData
{
	GENERATED_USTRUCT_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Navigation")
	EUINavigationRule Rule;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Navigation")
	FName WidgetToFocus;

	UPROPERTY()
	TWeakObjectPtr<UWidget> Widget;
};

/**
 * 
 */
UCLASS()
class UMG_API UWidgetNavigation : public UObject
{
	GENERATED_UCLASS_BODY()
	
public:
	/** Happens when the user presses up arrow, joystick, d-pad. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Navigation")
	FWidgetNavigationData Up;

	/** Happens when the user presses down arrow, joystick, d-pad. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Navigation")
	FWidgetNavigationData Down;

	/** Happens when the user presses left arrow, joystick, d-pad. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Navigation")
	FWidgetNavigationData Left;

	/** Happens when the user presses right arrow, joystick, d-pad. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Navigation")
	FWidgetNavigationData Right;

	/** Happens when the user presses Tab. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Navigation")
	FWidgetNavigationData Next;

	/** Happens when the user presses Shift+Tab. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Navigation")
	FWidgetNavigationData Previous;

public:

#if WITH_EDITOR

	/**  */
	FWidgetNavigationData& GetNavigationData(EUINavigation Nav);

	/**  */
	EUINavigationRule GetNavigationRule(EUINavigation Nav);

#endif

	/** Resolve widget names */
	void ResolveExplictRules(class UWidgetTree* WidgetTree);

	/** Updates a slate metadata object to match this configured navigation ruleset. */
	void UpdateMetaData(TSharedRef<FNavigationMetaData> MetaData);

	/** @return true if the configured navigation object is the same as an un-customized navigation rule set. */
	bool IsDefault() const;

private:

	void UpdateMetaDataEntry(TSharedRef<FNavigationMetaData> MetaData, const FWidgetNavigationData & NavData, EUINavigation Nav);
};
