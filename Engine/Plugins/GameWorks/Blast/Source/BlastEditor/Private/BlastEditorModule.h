#pragma once

#include "ModuleManager.h"
#include "IAssetTypeActions.h"
#include "EditorBuildUtils.h"
#include "LevelEditor.h"

class ABlastExtendedSupportStructure;

DECLARE_LOG_CATEGORY_EXTERN(LogBlastEditor, Verbose, All);

class BLASTEDITOR_API FBlastEditorModule : public IModuleInterface
{
public:
	FBlastEditorModule();

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/**
	* Singleton-like access to this module's interface.  This is just for convenience!
	* Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	*
	* @return Returns singleton instance, loading the module on demand if needed
	*/
	static inline FBlastEditorModule& GetModule()
	{
		static const FName ModuleName = "BlastEditor";
		return FModuleManager::LoadModuleChecked< FBlastEditorModule >(ModuleName);
	}

	EEditorBuildResult DoBlastBuild(class UWorld* World, FName Step);
	bool BuildExtendedSupport(ABlastExtendedSupportStructure* ExtSupportActor);

	static const FName BlastBuildStepId;

private:
	TSharedPtr<IAssetTypeActions> BlastMeshAssetTypeActions;

	TSharedPtr<FUICommandList>	CommandList;
	FDelegateHandle OnScreenMessageHandle;
	FDelegateHandle RefreshPhysicsAssetHandle;
	FLevelEditorModule::FLevelEditorMenuExtender BuildMenuExtender;
	FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors ActorMenuExtender;

	void BindCommands();
	void HandleGetOnScreenMessages(FCoreDelegates::FSeverityMessageMap& OutMessages);
	void HandleRefreshPhysicsAssetChange(const class UPhysicsAsset* Asset);
	TArray<AActor*> GetActorsWithBlastComponents(const TArray<AActor*>& Actors);
	void PopulateBlastMenuForActors(FMenuBuilder& InMenuBuilder, const TArray<AActor*>& Actors);

};