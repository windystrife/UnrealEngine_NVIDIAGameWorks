// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "HierarchicalSimplificationCustomizations.h"
#include "Modules/ModuleManager.h"
#include "GameFramework/WorldSettings.h"
#include "IDetailChildrenBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailGroup.h"
#include "IDetailPropertyRow.h"
#include "MeshUtilities.h"
#include "IMeshReductionManagerModule.h"

#define LOCTEXT_NAMESPACE "HierarchicalSimplificationCustomizations"

TSharedRef<IPropertyTypeCustomization> FHierarchicalSimplificationCustomizations::MakeInstance() 
{
	return MakeShareable( new FHierarchicalSimplificationCustomizations );
}

void FHierarchicalSimplificationCustomizations::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	FFormatOrderedArguments Args;
	Args.Add(StructPropertyHandle->GetPropertyDisplayName());
	FText Name = FText::Format(LOCTEXT("HLODLevelName", "HLOD Level {0}"), Args);
	
	HeaderRow.
	NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget(Name)
	]
	.ValueContent()
	[
		StructPropertyHandle->CreatePropertyValueWidget(false)
	];
}

void FHierarchicalSimplificationCustomizations::CustomizeChildren( TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils )
{
	// Retrieve structure's child properties
	uint32 NumChildren;
	StructPropertyHandle->GetNumChildren( NumChildren );	
	TMap<FName, TSharedPtr< IPropertyHandle > > PropertyHandles;	
	for( uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex )
	{
		TSharedRef<IPropertyHandle> ChildHandle = StructPropertyHandle->GetChildHandle( ChildIndex ).ToSharedRef();
		const FName PropertyName = ChildHandle->GetProperty()->GetFName();

		PropertyHandles.Add(PropertyName, ChildHandle);
	}
	
	// Create two sub-settings groups for clean overview
	IDetailGroup& ClusterGroup = ChildBuilder.AddGroup(NAME_None, FText::FromString("Cluster generation settings"));
	IDetailGroup& MergeGroup = ChildBuilder.AddGroup(NAME_None, FText::FromString("Mesh generation settings"));

	// Retrieve special case properties
	SimplifyMeshPropertyHandle = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FHierarchicalSimplification, bSimplifyMesh));
	TSharedPtr< IPropertyHandle > ProxyMeshSettingPropertyHandle = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FHierarchicalSimplification, ProxySetting));
	TSharedPtr< IPropertyHandle > MergeMeshSettingPropertyHandle = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FHierarchicalSimplification, MergeSetting));
	TSharedPtr< IPropertyHandle > TransitionScreenSizePropertyHandle = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FHierarchicalSimplification, TransitionScreenSize));

	for (auto Iter(PropertyHandles.CreateConstIterator()); Iter; ++Iter)
	{
		// Handle special property cases (done inside the loop to maintain order according to the struct
		if (Iter.Value() == SimplifyMeshPropertyHandle)
		{
			IDetailPropertyRow& SimplifyMeshRow = MergeGroup.AddPropertyRow(SimplifyMeshPropertyHandle.ToSharedRef());
			SimplifyMeshRow.Visibility(TAttribute<EVisibility>(this, &FHierarchicalSimplificationCustomizations::IsSimplifyMeshVisible));
		}
		else if (Iter.Value() == ProxyMeshSettingPropertyHandle)
		{
			IDetailPropertyRow& SettingsRow = MergeGroup.AddPropertyRow(Iter.Value().ToSharedRef());
			SettingsRow.Visibility(TAttribute<EVisibility>(this, &FHierarchicalSimplificationCustomizations::IsProxyMeshSettingVisible));
		}
		else if (Iter.Value() == MergeMeshSettingPropertyHandle)
		{
			IDetailPropertyRow& SettingsRow = MergeGroup.AddPropertyRow(Iter.Value().ToSharedRef());
			SettingsRow.Visibility(TAttribute<EVisibility>(this, &FHierarchicalSimplificationCustomizations::IsMergeMeshSettingVisible));
		}
		else  if (Iter.Value() == TransitionScreenSizePropertyHandle)
		{
			IDetailPropertyRow& SettingsRow = MergeGroup.AddPropertyRow(Iter.Value().ToSharedRef());
		}
		else
		{
			IDetailPropertyRow& SettingsRow = ClusterGroup.AddPropertyRow(Iter.Value().ToSharedRef());
		}
	}
}

EVisibility FHierarchicalSimplificationCustomizations::IsSimplifyMeshVisible() const
{
	// Determine whether or not there is a mesh merging interface available (SimplygonMeshReduction/SimplygonSwarm)
	IMeshReductionModule& ReductionModule = FModuleManager::Get().LoadModuleChecked<IMeshReductionModule>("MeshReductionInterface");
	if (ReductionModule.GetMeshMergingInterface() != nullptr)
	{
		return EVisibility::Visible;
	}

	return EVisibility::Hidden;
}

EVisibility FHierarchicalSimplificationCustomizations::IsProxyMeshSettingVisible() const
{
	bool bSimplifyMesh;

	if (SimplifyMeshPropertyHandle->GetValue(bSimplifyMesh) == FPropertyAccess::Result::Success)
	{
		if (IsSimplifyMeshVisible() == EVisibility::Visible && bSimplifyMesh)
		{
			return EVisibility::Visible;
		}
	}
		
	return EVisibility::Hidden;
}

EVisibility FHierarchicalSimplificationCustomizations::IsMergeMeshSettingVisible() const
{
	if(IsProxyMeshSettingVisible() == EVisibility::Hidden)
	{
		return EVisibility::Visible;
	}

	return EVisibility::Hidden;
}

#undef LOCTEXT_NAMESPACE
