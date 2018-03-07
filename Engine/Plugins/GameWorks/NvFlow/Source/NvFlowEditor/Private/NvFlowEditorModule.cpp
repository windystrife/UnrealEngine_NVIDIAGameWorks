#include "NvFlowEditorModule.h"
#include "NvFlowEditorCommon.h"

#include "FlowGridComponentVisualizer.h"
#include "AssetTypeActions_FlowGridAsset.h"
#include "AssetTypeActions_FlowMaterial.h"
#include "AssetTypeActions_FlowRenderMaterial.h"

#include "PropertyEditorModule.h"

#include "ComponentVisualizers.h"
#include "GameWorks/RendererHooksNvFlow.h"

IMPLEMENT_MODULE( FNvFlowEditorModule, NvFlowEditor );
DEFINE_LOG_CATEGORY(LogNvFlowEditor);

struct EditorRendererHooksNvFlowImpl : public EditorRendererHooksNvFlow
{
	virtual void NvFlowRegisterVisualizer(FComponentVisualizersModule* module)
	{
		module->RegisterComponentVisualizer(UFlowGridComponent::StaticClass()->GetFName(), MakeShareable(new FFlowGridComponentVisualizer));
	}
};
EditorRendererHooksNvFlowImpl GEditorRendererHooksNvFlowImpl;

void FNvFlowEditorModule::StartupModule()
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	FlowGridAssetTypeActions = MakeShareable(new FAssetTypeActions_FlowGridAsset);
	AssetTools.RegisterAssetTypeActions(FlowGridAssetTypeActions.ToSharedRef());

	FlowMaterialTypeActions = MakeShareable(new FAssetTypeActions_FlowMaterial);
	AssetTools.RegisterAssetTypeActions(FlowMaterialTypeActions.ToSharedRef());

	FlowRenderMaterialTypeActions = MakeShareable(new FAssetTypeActions_FlowRenderMaterial);
	AssetTools.RegisterAssetTypeActions(FlowRenderMaterialTypeActions.ToSharedRef());

	GEditorRendererHooksNvFlow = &GEditorRendererHooksNvFlowImpl;
}

void FNvFlowEditorModule::ShutdownModule()
{
	if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();

		if (FlowRenderMaterialTypeActions.IsValid())
		{
			AssetTools.UnregisterAssetTypeActions(FlowRenderMaterialTypeActions.ToSharedRef());
		}
		if (FlowMaterialTypeActions.IsValid())
		{
			AssetTools.UnregisterAssetTypeActions(FlowMaterialTypeActions.ToSharedRef());
		}
		if (FlowGridAssetTypeActions.IsValid())
		{
			AssetTools.UnregisterAssetTypeActions(FlowGridAssetTypeActions.ToSharedRef());
		}
	}
}
