// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#include "AIModule.h"
#include "EngineDefines.h"
#include "Templates/SubclassOf.h"
#include "AISystem.h"
#include "VisualLogger/VisualLogger.h"

#if WITH_EDITOR
#include "Developer/AssetTools/Public/IAssetTools.h"
#include "Developer/AssetTools/Public/AssetToolsModule.h"
#if ENABLE_VISUAL_LOG
	#include "VisualLoggerExtension.h"
#endif // ENABLE_VISUAL_LOG
#endif


#if WITH_GAMEPLAY_DEBUGGER
#include "GameplayDebugger.h"
#include "GameplayDebugger/GameplayDebuggerCategory_AI.h"
#include "GameplayDebugger/GameplayDebuggerCategory_BehaviorTree.h"
#include "GameplayDebugger/GameplayDebuggerCategory_EQS.h"
#include "GameplayDebugger/GameplayDebuggerCategory_Navmesh.h"
#include "GameplayDebugger/GameplayDebuggerCategory_Perception.h"
#include "GameplayDebugger/GameplayDebuggerCategory_NavLocalGrid.h"
#endif // WITH_GAMEPLAY_DEBUGGER

#define LOCTEXT_NAMESPACE "AIModule"

DEFINE_LOG_CATEGORY_STATIC(LogAIModule, Log, All);

class FAIModule : public IAIModule
{
	// Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	
	virtual UAISystemBase* CreateAISystemInstance(UWorld* World) override;
	// End IModuleInterface
#if WITH_EDITOR
	virtual EAssetTypeCategories::Type GetAIAssetCategoryBit() const override { return AIAssetCategoryBit; }
protected:
	EAssetTypeCategories::Type AIAssetCategoryBit;
#if ENABLE_VISUAL_LOG
	FVisualLoggerExtension	VisualLoggerExtension;
#endif // ENABLE_VISUAL_LOG
#endif
};

IMPLEMENT_MODULE(FAIModule, AIModule)

void FAIModule::StartupModule()
{ 
#if WITH_EDITOR 
	FModuleManager::LoadModulePtr< IModuleInterface >("AITestSuite");

	if (GIsEditor)
	{
		// Register asset types
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

		// register AI category so that AI assets can register to it
		AIAssetCategoryBit = AssetTools.RegisterAdvancedAssetCategory(FName(TEXT("AI")), LOCTEXT("AIAssetCategory", "Artificial Intelligence"));
	}
#endif // WITH_EDITOR 

#if WITH_EDITOR && ENABLE_VISUAL_LOG
	FVisualLogger::Get().RegisterExtension(*EVisLogTags::TAG_EQS, &VisualLoggerExtension);
#endif

#if WITH_GAMEPLAY_DEBUGGER
	IGameplayDebugger& GameplayDebuggerModule = IGameplayDebugger::Get();
	GameplayDebuggerModule.RegisterCategory("AI", IGameplayDebugger::FOnGetCategory::CreateStatic(&FGameplayDebuggerCategory_AI::MakeInstance), EGameplayDebuggerCategoryState::EnabledInGameAndSimulate, 1);
	GameplayDebuggerModule.RegisterCategory("BehaviorTree", IGameplayDebugger::FOnGetCategory::CreateStatic(&FGameplayDebuggerCategory_BehaviorTree::MakeInstance), EGameplayDebuggerCategoryState::EnabledInGame, 2);
	GameplayDebuggerModule.RegisterCategory("EQS", IGameplayDebugger::FOnGetCategory::CreateStatic(&FGameplayDebuggerCategory_EQS::MakeInstance));
	GameplayDebuggerModule.RegisterCategory("Navmesh", IGameplayDebugger::FOnGetCategory::CreateStatic(&FGameplayDebuggerCategory_Navmesh::MakeInstance), EGameplayDebuggerCategoryState::Disabled, 0);
	GameplayDebuggerModule.RegisterCategory("Perception", IGameplayDebugger::FOnGetCategory::CreateStatic(&FGameplayDebuggerCategory_Perception::MakeInstance));
	GameplayDebuggerModule.RegisterCategory("NavGrid", IGameplayDebugger::FOnGetCategory::CreateStatic(&FGameplayDebuggerCategory_NavLocalGrid::MakeInstance), EGameplayDebuggerCategoryState::Hidden);
	GameplayDebuggerModule.NotifyCategoriesChanged();
#endif
}

void FAIModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
#if WITH_EDITOR && ENABLE_VISUAL_LOG
	FVisualLogger::Get().UnregisterExtension(*EVisLogTags::TAG_EQS, &VisualLoggerExtension);
#endif

#if WITH_GAMEPLAY_DEBUGGER
	if (IGameplayDebugger::IsAvailable())
	{
		IGameplayDebugger& GameplayDebuggerModule = IGameplayDebugger::Get();
		GameplayDebuggerModule.UnregisterCategory("AI");
		GameplayDebuggerModule.UnregisterCategory("BehaviorTree");
		GameplayDebuggerModule.UnregisterCategory("EQS");
		GameplayDebuggerModule.UnregisterCategory("Navmesh");
		GameplayDebuggerModule.UnregisterCategory("Perception");
		GameplayDebuggerModule.UnregisterCategory("NavGrid");
		GameplayDebuggerModule.NotifyCategoriesChanged();
	}
#endif
}

UAISystemBase* FAIModule::CreateAISystemInstance(UWorld* World)
{
	UE_LOG(LogAIModule, Log, TEXT("Creating AISystem for world %s"), *GetNameSafe(World));
	
	TSubclassOf<UAISystemBase> AISystemClass = LoadClass<UAISystemBase>(NULL, *UAISystem::GetAISystemClassName().ToString(), NULL, LOAD_None, NULL);
	return NewObject<UAISystemBase>(World, AISystemClass);
}

#undef LOCTEXT_NAMESPACE
