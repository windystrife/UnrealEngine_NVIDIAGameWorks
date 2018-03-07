// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ReferenceViewer.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Framework/Docking/TabManager.h"
#include "EdGraph_ReferenceViewer.h"
#include "EdGraphNode_Reference.h"
#include "SGraphNode.h"
#include "SReferenceNode.h"
#include "SReferenceViewer.h"

#include "EdGraphUtilities.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "ReferenceViewer"
//DEFINE_LOG_CATEGORY(LogReferenceViewer);

class FGraphPanelNodeFactory_ReferenceViewer : public FGraphPanelNodeFactory
{
	virtual TSharedPtr<class SGraphNode> CreateNode(UEdGraphNode* Node) const override
	{
		if ( UEdGraphNode_Reference* DependencyNode = Cast<UEdGraphNode_Reference>(Node) )
		{
			return SNew(SReferenceNode, DependencyNode);
		}

		return NULL;
	}
};

class FReferenceViewerModule : public IReferenceViewerModule
{
public:
	FReferenceViewerModule()
		: ReferenceViewerTabId("ReferenceViewer")
	{
		
	}

	virtual void StartupModule() override
	{
		GraphPanelNodeFactory = MakeShareable( new FGraphPanelNodeFactory_ReferenceViewer() );
		FEdGraphUtilities::RegisterVisualNodeFactory(GraphPanelNodeFactory);

		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(ReferenceViewerTabId, FOnSpawnTab::CreateRaw( this, &FReferenceViewerModule::SpawnReferenceViewerTab ) )
			.SetDisplayName( LOCTEXT("ReferenceViewerTitle", "Reference Viewer") )
			.SetMenuType( ETabSpawnerMenuType::Hidden );
	}

	virtual void ShutdownModule() override
	{
		if ( GraphPanelNodeFactory.IsValid() )
		{
			FEdGraphUtilities::UnregisterVisualNodeFactory(GraphPanelNodeFactory);
			GraphPanelNodeFactory.Reset();
		}

		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ReferenceViewerTabId);
	}

	virtual void InvokeReferenceViewerTab(const TArray<FAssetIdentifier>& GraphRootIdentifiers) override
	{
		TSharedRef<SDockTab> NewTab = FGlobalTabmanager::Get()->InvokeTab( ReferenceViewerTabId );
		TSharedRef<SReferenceViewer> ReferenceViewer = StaticCastSharedRef<SReferenceViewer>( NewTab->GetContent() );
		ReferenceViewer->SetGraphRootPackageNames(GraphRootIdentifiers);
	}

	virtual bool GetSelectedAssetsForMenuExtender(const class UEdGraph* Graph, const class UEdGraphNode* Node, TArray<FAssetIdentifier>& SelectedAssets) override
	{
		const UEdGraph_ReferenceViewer* ReferenceGraph = Cast<UEdGraph_ReferenceViewer>(Graph);

		if (!ReferenceGraph)
		{
			return false;
		}

		return ReferenceGraph->GetSelectedAssetsForMenuExtender(Node, SelectedAssets);
	}
private:
	TSharedRef<SDockTab> SpawnReferenceViewerTab( const FSpawnTabArgs& SpawnTabArgs )
	{
		TSharedRef<SDockTab> NewTab = SNew(SDockTab)
			.TabRole(ETabRole::NomadTab);

		NewTab->SetContent( SNew(SReferenceViewer) );

		return NewTab;
	}

private:
	TSharedPtr<FGraphPanelNodeFactory> GraphPanelNodeFactory;
	FName ReferenceViewerTabId;
};

IMPLEMENT_MODULE( FReferenceViewerModule, ReferenceViewer )

#undef LOCTEXT_NAMESPACE
