// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Binding/DynamicPropertyPath.h"
#include "Engine/BlueprintGeneratedClass.h"

#include "WidgetBlueprintGeneratedClass.generated.h"

class UUserWidget;
class UWidgetAnimation;
class UWidgetTree;

UENUM()
enum class EBindingKind : uint8
{
	Function,
	Property
};

USTRUCT()
struct FDelegateRuntimeBinding
{
	GENERATED_USTRUCT_BODY()

	/** The widget that will be bound to the live data. */
	UPROPERTY()
	FString ObjectName;

	/** The property on the widget that will have a binding placed on it. */
	UPROPERTY()
	FName PropertyName;

	/** The function or property we're binding to on the source object. */
	UPROPERTY()
	FName FunctionName;

	/**  */
	UPROPERTY()
	FDynamicPropertyPath SourcePath;

	/** The kind of binding we're performing, are we binding to a property or a function. */
	UPROPERTY()
	EBindingKind Kind;
};


/**
 * The widget blueprint generated class allows us to create blueprint-able widgets for UMG at runtime.
 * All WBPGC's are of UUserWidget classes, and they perform special post initialization using this class
 * to give themselves many of the same capabilities as AActor blueprints, like dynamic delegate binding for
 * widgets.
 */
UCLASS()
class UMG_API UWidgetBlueprintGeneratedClass : public UBlueprintGeneratedClass
{
	GENERATED_UCLASS_BODY()

public:

	/** A tree of the widget templates to be created */
	UPROPERTY()
	UWidgetTree* WidgetTree;

#if WITH_EDITORONLY_DATA

	UPROPERTY()
	uint8 bCookSlowConstructionWidgetTree:1;

#endif

	UPROPERTY()
	uint8 bAllowTemplate:1;

private:

	UPROPERTY()
	uint8 bValidTemplate:1;

	UPROPERTY(Transient)
	uint8 bTemplateInitialized:1;

	UPROPERTY(Transient)
	uint8 bCookedTemplate:1;

public:
	UPROPERTY()
	TArray< FDelegateRuntimeBinding > Bindings;

	UPROPERTY()
	TArray< UWidgetAnimation* > Animations;

	UPROPERTY()
	TArray< FName > NamedSlots;

public:

	bool HasTemplate() const;

	void SetTemplate(UUserWidget* InTemplate);
	UUserWidget* GetTemplate();

	// UObject interface
	virtual void PreSave(const class ITargetPlatform* TargetPlatform) override;
	virtual void Serialize(FArchive& Ar) override;

	virtual UObject* CreateDefaultObject() override;
	virtual void PostLoad() override;
	virtual bool NeedsLoadForServer() const override;
	// End UObject interface

	virtual void PurgeClass(bool bRecompilingOnLoad) override;

	/**
	 * This is the function that makes UMG work.  Once a user widget is constructed, it will post load
	 * call into its generated class and ask to be initialized.  The class will perform all the delegate
	 * binding and wiring necessary to have the user's widget perform as desired.
	 */
	void InitializeWidget(UUserWidget* UserWidget) const;

	static void InitializeBindingsStatic(UUserWidget* UserWidget, const TArray< FDelegateRuntimeBinding >& InBindings);

	static void InitializeWidgetStatic(UUserWidget* UserWidget
		, const UClass* InClass
		, bool InCanTemplate
		, UWidgetTree* InWidgetTree
		, const TArray< UWidgetAnimation* >& InAnimations
		, const TArray< FDelegateRuntimeBinding >& InBindings);

private:
	void InitializeTemplate(const ITargetPlatform* TargetPlatform);

	UPROPERTY()
	TSoftObjectPtr<UUserWidget> TemplateAsset;

	UPROPERTY(Transient)
	mutable UUserWidget* Template;

#if WITH_EDITORONLY_DATA
	UPROPERTY(Transient)
	mutable UUserWidget* EditorTemplate;
#endif
};
