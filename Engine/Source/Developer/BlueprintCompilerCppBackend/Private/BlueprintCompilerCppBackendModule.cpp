// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "IBlueprintCompilerCppBackendModule.h"
#include "BlueprintCompilerCppBackend.h"
#include "BlueprintCompilerCppBackendUtils.h"

class FBlueprintCompilerCppBackendModule : public IBlueprintCompilerCppBackendModule
{
public:
	//~ Begin IBlueprintCompilerCppBackendModuleInterface interface
	virtual IBlueprintCompilerCppBackend* Create() override;
	//~ End IBlueprintCompilerCppBackendModuleInterface interface

	//~ Begin IBlueprintCompilerCppBackendModule interface
	virtual FString ConstructBaseFilename(const UObject* AssetObj, const FCompilerNativizationOptions& NativizationOptions) override;
	virtual FPCHFilenameQuery& OnPCHFilenameQuery() override;
	virtual FIsTargetedForConversionQuery& OnIsTargetedForConversionQuery() override;
	virtual TMap<TWeakObjectPtr<UClass>, TWeakObjectPtr<UClass> >& GetOriginalClassMap() override;
	virtual FMarkUnconvertedBlueprintAsNecessary& OnIncludingUnconvertedBP() override;
	virtual FIsFunctionUsedInADelegate& GetIsFunctionUsedInADelegateCallback() override;
	virtual TSharedPtr<FNativizationSummary>& NativizationSummary() override;
	virtual FString DependenciesGlobalMapHeaderCode() override;
	virtual FString DependenciesGlobalMapBodyCode(const FString& PCHFilename) override;
	//~ End IBlueprintCompilerCppBackendModule interface

private: 
	FPCHFilenameQuery PCHFilenameQuery;
	FIsTargetedForConversionQuery IsTargetedForConversionQuery;
	FMarkUnconvertedBlueprintAsNecessary MarkUnconvertedBlueprintAsNecessary;
	FIsFunctionUsedInADelegate IsFunctionUsedInADelegate;
	TMap<TWeakObjectPtr<UClass>, TWeakObjectPtr<UClass> > OriginalClassMap;
	TSharedPtr<FNativizationSummary> NativizationSummaryPtr;
};

IBlueprintCompilerCppBackend* FBlueprintCompilerCppBackendModule::Create()
{
	return new FBlueprintCompilerCppBackend();
}

TSharedPtr<FNativizationSummary>& FBlueprintCompilerCppBackendModule::NativizationSummary()
{
	return NativizationSummaryPtr;
}

FString FBlueprintCompilerCppBackendModule::ConstructBaseFilename(const UObject* AssetObj, const FCompilerNativizationOptions& NativizationOptions)
{
	// use the same function that the backend uses for #includes
	return FEmitHelper::GetBaseFilename(AssetObj, NativizationOptions);
}

IBlueprintCompilerCppBackendModule::FPCHFilenameQuery& FBlueprintCompilerCppBackendModule::OnPCHFilenameQuery()
{
	return PCHFilenameQuery;
}

IBlueprintCompilerCppBackendModule::FIsTargetedForConversionQuery& FBlueprintCompilerCppBackendModule::OnIsTargetedForConversionQuery()
{
	return IsTargetedForConversionQuery;
}

IBlueprintCompilerCppBackendModule::FMarkUnconvertedBlueprintAsNecessary& FBlueprintCompilerCppBackendModule::OnIncludingUnconvertedBP()
{
	return MarkUnconvertedBlueprintAsNecessary;
}

TMap<TWeakObjectPtr<UClass>, TWeakObjectPtr<UClass> >& FBlueprintCompilerCppBackendModule::GetOriginalClassMap()
{
	return OriginalClassMap;
}

IBlueprintCompilerCppBackendModule::FIsFunctionUsedInADelegate& FBlueprintCompilerCppBackendModule::GetIsFunctionUsedInADelegateCallback()
{
	return IsFunctionUsedInADelegate;
}

FString FBlueprintCompilerCppBackendModule::DependenciesGlobalMapHeaderCode()
{
	return FDependenciesGlobalMapHelper::EmitHeaderCode();
}

FString FBlueprintCompilerCppBackendModule::DependenciesGlobalMapBodyCode(const FString& PCHFilename)
{
	return FDependenciesGlobalMapHelper::EmitBodyCode(PCHFilename);
}

IMPLEMENT_MODULE(FBlueprintCompilerCppBackendModule, BlueprintCompilerCppBackend)
