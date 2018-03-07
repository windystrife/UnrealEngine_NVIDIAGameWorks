// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PropertyRowGenerator.h"
#include "PropertyNode.h"
#include "ObjectPropertyNode.h"
#include "EditorStyleSettings.h"
#include "DetailLayoutBuilderImpl.h"
#include "CategoryPropertyNode.h"
#include "SPropertyEditorEditInline.h"
#include "DetailCategoryBuilderImpl.h"
#include "ModuleManager.h"
#include "DetailLayoutHelpers.h"

class FPropertyRowGeneratorUtilities : public IPropertyUtilities
{
public:
	FPropertyRowGeneratorUtilities(FPropertyRowGenerator& InGenerator)
		: Generator(InGenerator)
	{}

	/** IPropertyUtilities interface */
	virtual class FNotifyHook* GetNotifyHook() const override
	{
		return Generator.GetNotifyHook();
	}
	virtual bool AreFavoritesEnabled() const override
	{
		return false;
	}

	virtual void ToggleFavorite(const TSharedRef< class FPropertyEditor >& PropertyEditor) const override {}
	virtual void CreateColorPickerWindow(const TSharedRef< class FPropertyEditor >& PropertyEditor, bool bUseAlpha) const override {}
	virtual void EnqueueDeferredAction(FSimpleDelegate DeferredAction) override
	{
		Generator.EnqueueDeferredAction(DeferredAction);
	}
	virtual bool IsPropertyEditingEnabled() const override
	{
		return Generator.IsPropertyEditingEnabled();
	}

	virtual void ForceRefresh() override
	{
		Generator.ForceRefresh();
	}
	virtual void RequestRefresh() override {}

	virtual TSharedPtr<class FAssetThumbnailPool> GetThumbnailPool() const override
	{
		return Generator.GetThumbnailPool();
	}

	virtual void NotifyFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent) override {}

	virtual bool DontUpdateValueWhileEditing() const override { return false; }

	const TArray<TWeakObjectPtr<UObject>>& GetSelectedObjects() const override
	{
		return Generator.GetSelectedObjects();
	}

	virtual bool HasClassDefaultObject() const override
	{
		return Generator.HasClassDefaultObject();
	}
private:
	FPropertyRowGenerator& Generator;
};


FPropertyRowGenerator::FPropertyRowGenerator(const FPropertyRowGeneratorArgs& InArgs, TSharedPtr<FAssetThumbnailPool> InThumbnailPool)
	: Args(InArgs)
	, ThumbnailPool(InThumbnailPool)
	, PropertyUtilities(new FPropertyRowGeneratorUtilities(*this))
{
}

FPropertyRowGenerator::~FPropertyRowGenerator()
{

}

void FPropertyRowGenerator::SetObjects(const TArray<UObject*>& InObjects)
{
	// We're setting objects not structs
	const bool bHasStructRoots = false;

	PreSetObject(InObjects.Num(), bHasStructRoots);
	
	bViewingClassDefaultObject = InObjects.Num() > 0 ? true : false;
	
	SelectedObjects.Reset(InObjects.Num());

	for (int32 ObjectIndex = 0; ObjectIndex < InObjects.Num(); ++ObjectIndex)
	{
		UObject* Object = InObjects[ObjectIndex];

		SelectedObjects.Add(Object);

		bViewingClassDefaultObject &= Object->HasAnyFlags(RF_ClassDefaultObject);

		if (Args.bAllowMultipleTopLevelObjects)
		{
			check(RootPropertyNodes.Num() == InObjects.Num());
			RootPropertyNodes[ObjectIndex]->AsObjectNode()->AddObject(Object);
		}
		else
		{
			RootPropertyNodes[0]->AsObjectNode()->AddObject(Object);
		}
	}

	PostSetObject();
}

const TArray<TSharedRef<IDetailTreeNode>>& FPropertyRowGenerator::GetRootTreeNodes() const
{
	return RootTreeNodes;
}

void FPropertyRowGenerator::RegisterInstancedCustomPropertyLayout(UStruct* Class, FOnGetDetailCustomizationInstance DetailLayoutDelegate)
{
	check(Class);

	FDetailLayoutCallback Callback;
	Callback.DetailLayoutDelegate = DetailLayoutDelegate;
	Callback.Order = InstancedClassToDetailLayoutMap.Num();

	InstancedClassToDetailLayoutMap.Add(Class, Callback);
}

void FPropertyRowGenerator::RegisterInstancedCustomPropertyTypeLayout(FName PropertyTypeName, FOnGetPropertyTypeCustomizationInstance PropertyTypeLayoutDelegate, TSharedPtr<IPropertyTypeIdentifier> Identifier /*= nullptr*/)
{
	FPropertyTypeLayoutCallback Callback;
	Callback.PropertyTypeLayoutDelegate = PropertyTypeLayoutDelegate;
	Callback.PropertyTypeIdentifier = Identifier;

	FPropertyTypeLayoutCallbackList* LayoutCallbacks = InstancedTypeToLayoutMap.Find(PropertyTypeName);
	if (LayoutCallbacks)
	{
		LayoutCallbacks->Add(Callback);
	}
	else
	{
		FPropertyTypeLayoutCallbackList NewLayoutCallbacks;
		NewLayoutCallbacks.Add(Callback);
		InstancedTypeToLayoutMap.Add(PropertyTypeName, NewLayoutCallbacks);
	}
}

void FPropertyRowGenerator::UnregisterInstancedCustomPropertyLayout(UStruct* Class)
{
	check(Class);

	InstancedClassToDetailLayoutMap.Remove(Class);
}

void FPropertyRowGenerator::UnregisterInstancedCustomPropertyTypeLayout(FName PropertyTypeName, TSharedPtr<IPropertyTypeIdentifier> Identifier /*= nullptr*/)
{
	FPropertyTypeLayoutCallbackList* LayoutCallbacks = InstancedTypeToLayoutMap.Find(PropertyTypeName);

	if (LayoutCallbacks)
	{
		LayoutCallbacks->Remove(Identifier);
	}
}

void FPropertyRowGenerator::Tick(float DeltaTime)
{
	for (TSharedPtr<IDetailCustomization>& Customization : CustomizationClassInstancesPendingDelete)
	{
		ensure(Customization.IsUnique());
	}

	// Release any pending kill nodes.
	for (TSharedPtr<FComplexPropertyNode>& PendingKillNode : RootNodesPendingKill)
	{
		if (PendingKillNode.IsValid())
		{
			PendingKillNode->Disconnect();
			PendingKillNode.Reset();
		}
	}

	RootNodesPendingKill.Empty();
	CustomizationClassInstancesPendingDelete.Empty();

	if (DeferredActions.Num() > 0)
	{
		// Execute any deferred actions
		for (FSimpleDelegate& Action : DeferredActions)
		{
			Action.ExecuteIfBound();
		}
		DeferredActions.Empty();
	}

	bool bFullRefresh = ValidatePropertyNodes(RootPropertyNodes);

	if (!bFullRefresh)
	{
	
	}

	for (FDetailLayoutData& LayoutData : DetailLayouts)
	{
		if (LayoutData.DetailLayout.IsValid())
		{
			if (!bFullRefresh)
			{
				ValidatePropertyNodes(LayoutData.DetailLayout->GetExternalRootPropertyNodes());
			}
			LayoutData.DetailLayout->Tick(DeltaTime);
		}
	}

}

TStatId FPropertyRowGenerator::GetStatId() const 
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FPropertyRowGenerator, STATGROUP_Tickables);
}

void FPropertyRowGenerator::EnqueueDeferredAction(FSimpleDelegate DeferredAction)
{
	DeferredActions.Add(DeferredAction);
}

void FPropertyRowGenerator::ForceRefresh()
{
	TArray<UObject*> NewObjectList;

	for (const TSharedPtr<FComplexPropertyNode>& ComplexRootNode : RootPropertyNodes)
	{
		FObjectPropertyNode* RootNode = ComplexRootNode->AsObjectNode();
		if (RootNode)
		{
			// Simply re-add the same existing objects to cause a refresh
			for (TPropObjectIterator Itor(RootNode->ObjectIterator()); Itor; ++Itor)
			{
				TWeakObjectPtr<UObject> Object = *Itor;
				if (Object.IsValid())
				{
					NewObjectList.Add(Object.Get());
				}
			}
		}
	}

	SetObjects(NewObjectList);
}

TSharedPtr<class FAssetThumbnailPool> FPropertyRowGenerator::GetThumbnailPool() const
{
	return ThumbnailPool;
}

void FPropertyRowGenerator::PreSetObject(int32 NumNewObjects, bool bHasStructRoots)
{
	// Save existing expanded items first
	for (TSharedPtr<FComplexPropertyNode>& RootNode : RootPropertyNodes)
	{
		RootNodesPendingKill.Add(RootNode);
		FObjectPropertyNode* RootObjectNode = RootNode->AsObjectNode();
		RootObjectNode->RemoveAllObjects();
		RootObjectNode->ClearCachedReadAddresses(true);
		RootObjectNode->ClearObjectPackageOverrides();
	}

	RootPropertyNodes.Empty(NumNewObjects);

	if(!bHasStructRoots)
	{
		if (Args.bAllowMultipleTopLevelObjects)
		{
			for (int32 NewRootIndex = 0; NewRootIndex < NumNewObjects; ++NewRootIndex)
			{
				RootPropertyNodes.Add(MakeShared<FObjectPropertyNode>());
			}
		}
		else
		{
			RootPropertyNodes.Add(MakeShared<FObjectPropertyNode>());
		}
	}
	else
	{
		// todo structs
	}

}

void FPropertyRowGenerator::PostSetObject()
{
	FPropertyNodeInitParams InitParams;
	InitParams.ParentNode = nullptr;
	InitParams.Property = nullptr;
	InitParams.ArrayOffset = 0;
	InitParams.ArrayIndex = INDEX_NONE;
	InitParams.bAllowChildren = true;
	InitParams.bForceHiddenPropertyVisibility = FPropertySettings::Get().ShowHiddenProperties();
	
	switch (Args.DefaultsOnlyVisibility)
	{
	case EEditDefaultsOnlyNodeVisibility::Hide:
		InitParams.bCreateDisableEditOnInstanceNodes = false;
		break;
	case EEditDefaultsOnlyNodeVisibility::Show:
		InitParams.bCreateDisableEditOnInstanceNodes = true;
		break;
	case EEditDefaultsOnlyNodeVisibility::Automatic:
		InitParams.bCreateDisableEditOnInstanceNodes = HasClassDefaultObject();
		break;
	default:
		check(false);
	}

	for (TSharedPtr<FComplexPropertyNode>& ComplexRootNode : RootPropertyNodes)
	{
		ComplexRootNode->InitNode(InitParams);
	}

	UpdatePropertyMaps();

	UpdateDetailRows();
}

void FPropertyRowGenerator::UpdateDetailRows()
{
	RootTreeNodes.Reset();

	FDetailNodeList InitialRootNodeList;

	//NumVisbleTopLevelObjectNodes = 0;

	FDetailFilter CurrentFilter;

	for (int32 RootNodeIndex = 0; RootNodeIndex < RootPropertyNodes.Num(); ++RootNodeIndex)
	{
		TSharedPtr<FComplexPropertyNode>& RootPropertyNode = RootPropertyNodes[RootNodeIndex];
		if (RootPropertyNode.IsValid())
		{
			RootPropertyNode->FilterNodes(CurrentFilter.FilterStrings);
			RootPropertyNode->ProcessSeenFlags(true);

			TSharedPtr<FDetailLayoutBuilderImpl>& DetailLayout = DetailLayouts[RootNodeIndex].DetailLayout;
			if (DetailLayout.IsValid())
			{
				const FRootPropertyNodeList& ExternalPropertyNodeList = DetailLayout->GetExternalRootPropertyNodes();
				for (int32 NodeIndex = 0; NodeIndex < ExternalPropertyNodeList.Num(); ++NodeIndex)
				{
					TSharedPtr<FPropertyNode> PropertyNode = ExternalPropertyNodeList[NodeIndex];

					if (PropertyNode.IsValid())
					{
						PropertyNode->FilterNodes(CurrentFilter.FilterStrings);
						PropertyNode->ProcessSeenFlags(true);
					}
				}

				DetailLayout->FilterDetailLayout(CurrentFilter);

				FDetailNodeList& LayoutRoots = DetailLayout->GetFilteredRootTreeNodes();
				if (LayoutRoots.Num() > 0)
				{
					// A top level object nodes has a non-filtered away root so add one to the total number we have
					//++NumVisbleTopLevelObjectNodes;

					InitialRootNodeList.Append(LayoutRoots);
				}
			}
		}
	}


	// for multiple top level object we need to do a secondary pass on top level object nodes after we have determined if there is any nodes visible at all.  If there are then we ask the details panel if it wants to show childen
	for (TSharedRef<class FDetailTreeNode> RootNode : InitialRootNodeList)
	{
		if (RootNode->ShouldShowOnlyChildren())
		{
			FDetailNodeList TreeNodes;
			RootNode->GetChildren(TreeNodes);
			for (auto& Node : TreeNodes)
			{
				RootTreeNodes.Add(Node);
			}
		}
		else
		{
			RootTreeNodes.Add(RootNode);
		}
	}

	RefreshRowsDelegate.Broadcast();

}

void FPropertyRowGenerator::UpdatePropertyMaps()
{
	RootTreeNodes.Empty();


	for (FDetailLayoutData& LayoutData : DetailLayouts)
	{
		// Check uniqueness.  It is critical that detail layouts can be destroyed
		// We need to be able to create a new detail layout and properly clean up the old one in the process
		check(!LayoutData.DetailLayout.IsValid() || LayoutData.DetailLayout.IsUnique());

		// All the current customization instances need to be deleted when it is safe
		CustomizationClassInstancesPendingDelete.Append(LayoutData.CustomizationClassInstances);

		for (auto ExternalRootNode : LayoutData.DetailLayout->GetExternalRootPropertyNodes())
		{
			if (ExternalRootNode.IsValid())
			{
				FComplexPropertyNode* ComplexNode = ExternalRootNode->AsComplexNode();
				if (ComplexNode)
				{
					ComplexNode->Disconnect();
				}
			}
		}
	}

	DetailLayouts.Empty(RootPropertyNodes.Num());

	// There should be one detail layout for each root node
	DetailLayouts.AddDefaulted(RootPropertyNodes.Num());

	for (int32 RootNodeIndex = 0; RootNodeIndex < RootPropertyNodes.Num(); ++RootNodeIndex)
	{
		FDetailLayoutData& LayoutData = DetailLayouts[RootNodeIndex];
		UpdateSinglePropertyMap(RootPropertyNodes[RootNodeIndex], LayoutData);
	}
}

void FPropertyRowGenerator::UpdateSinglePropertyMap(TSharedPtr<FComplexPropertyNode> InRootPropertyNode, FDetailLayoutData& LayoutData)
{
	// Reset everything
	LayoutData.ClassToPropertyMap.Empty();

	TSharedPtr<FDetailLayoutBuilderImpl> DetailLayout = MakeShareable(new FDetailLayoutBuilderImpl(InRootPropertyNode, LayoutData.ClassToPropertyMap, PropertyUtilities, nullptr));
	LayoutData.DetailLayout = DetailLayout;

	TSharedPtr<FComplexPropertyNode> RootPropertyNode = InRootPropertyNode;
	check(RootPropertyNode.IsValid());

	DetailLayoutHelpers::FUpdatePropertyMapArgs LayoutArgs;

	LayoutArgs.LayoutData = &LayoutData;
	LayoutArgs.InstancedPropertyTypeToDetailLayoutMap = &InstancedTypeToLayoutMap;
	LayoutArgs.IsPropertyReadOnly = [this](const FPropertyAndParent& PropertyAndParent) { return false; };
	LayoutArgs.IsPropertyVisible = [this](const FPropertyAndParent& PropertyAndParent) { return true; };
	LayoutArgs.bEnableFavoriteSystem = false;
	LayoutArgs.bUpdateFavoriteSystemOnly = false;

	DetailLayoutHelpers::UpdateSinglePropertyMapRecursive(*RootPropertyNode, NAME_None, RootPropertyNode.Get(), LayoutArgs);

	DetailLayoutHelpers::QueryCustomDetailLayout(LayoutData, InstancedClassToDetailLayoutMap, FOnGetDetailCustomizationInstance());

	LayoutData.DetailLayout->GenerateDetailLayout();
}


bool FPropertyRowGenerator::ValidatePropertyNodes(const FRootPropertyNodeList &PropertyNodeList)
{
	bool bFullRefresh = false;

	for (const TSharedPtr<FComplexPropertyNode>& RootPropertyNode : RootPropertyNodes)
	{
		// Purge any objects that are marked pending kill from the object list
		if (auto ObjectRoot = RootPropertyNode->AsObjectNode())
		{
			ObjectRoot->PurgeKilledObjects();
		}

		EPropertyDataValidationResult Result = RootPropertyNode->EnsureDataIsValid();
		if (Result == EPropertyDataValidationResult::PropertiesChanged || Result == EPropertyDataValidationResult::EditInlineNewValueChanged)
		{
			UpdatePropertyMaps();
			UpdateDetailRows();
			break;
		}
		else if (Result == EPropertyDataValidationResult::ArraySizeChanged)
		{
			UpdateDetailRows();
		}
		else if (Result == EPropertyDataValidationResult::ObjectInvalid)
		{
			ForceRefresh();
			bFullRefresh = true;
			break;
		}
	}

	return bFullRefresh;
}

