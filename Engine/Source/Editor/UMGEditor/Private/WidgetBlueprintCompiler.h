// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WidgetBlueprint.h"
#include "KismetCompiler.h"


//////////////////////////////////////////////////////////////////////////
// FWidgetBlueprintCompiler

class FWidgetBlueprintCompiler : public FKismetCompilerContext
{
protected:
	typedef FKismetCompilerContext Super;

public:
	FWidgetBlueprintCompiler(UWidgetBlueprint* SourceSketch, FCompilerResultsLog& InMessageLog, const FKismetCompilerOptions& InCompilerOptions, TArray<UObject*>* InObjLoaded);
	virtual ~FWidgetBlueprintCompiler();

	static bool CanAllowTemplate(FCompilerResultsLog& MessageLog, UWidgetBlueprintGeneratedClass* InClass);
	static bool CanTemplateWidget(FCompilerResultsLog& MessageLog, UUserWidget* ThisWidget, TArray<FText>& OutErrors);

protected:
	UWidgetBlueprint* WidgetBlueprint() const { return Cast<UWidgetBlueprint>(Blueprint); }

	void ValidateWidgetNames();

	// FKismetCompilerContext
	virtual UEdGraphSchema_K2* CreateSchema() override;
	virtual void CreateFunctionList() override;
	virtual void SpawnNewClass(const FString& NewClassName) override;
	virtual void OnNewClassSet(UBlueprintGeneratedClass* ClassToUse) override;
	virtual void PrecompileFunction(FKismetFunctionContext& Context, EInternalCompilerFlags InternalFlags) override;
	virtual void CleanAndSanitizeClass(UBlueprintGeneratedClass* ClassToClean, UObject*& InOutOldCDO) override;
	virtual void SaveSubObjectsFromCleanAndSanitizeClass(FSubobjectCollection& SubObjectsToSave, UBlueprintGeneratedClass* ClassToClean) override;
	virtual void EnsureProperGeneratedClass(UClass*& TargetClass) override;
	virtual void CreateClassVariablesFromBlueprint() override;
	virtual void CopyTermDefaultsToDefaultObject(UObject* DefaultObject);
	virtual void FinishCompilingClass(UClass* Class) override;
	virtual bool ValidateGeneratedClass(UBlueprintGeneratedClass* Class) override;
	virtual void PostCompile() override;
	// End FKismetCompilerContext

	void VerifyEventReplysAreNotEmpty(FKismetFunctionContext& Context);

protected:

	UWidgetBlueprintGeneratedClass* NewWidgetBlueprintClass;

	class UWidgetGraphSchema* WidgetSchema;

	// Map of properties created for widgets; to aid in debug data generation
	TMap<class UWidget*, class UProperty*> WidgetToMemberVariableMap;

	///----------------------------------------------------------------
};

