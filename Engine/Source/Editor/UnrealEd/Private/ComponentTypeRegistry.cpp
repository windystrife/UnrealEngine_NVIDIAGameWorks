// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ComponentTypeRegistry.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "Misc/PackageName.h"
#include "Engine/Blueprint.h"
#include "Components/StaticMeshComponent.h"
#include "TickableEditorObject.h"
#include "ActorFactories/ActorFactoryBasicShape.h"
#include "Materials/Material.h"
#include "Settings/EditorExperimentalSettings.h"
#include "Engine/StaticMesh.h"
#include "AssetData.h"

#include "AssetRegistryModule.h"
#include "ClassIconFinder.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/HotReloadInterface.h"
#include "SComponentClassCombo.h"

#define LOCTEXT_NAMESPACE "ComponentTypeRegistry"

//////////////////////////////////////////////////////////////////////////
// FComponentTypeRegistryData

struct FComponentTypeRegistryData
	: public FTickableEditorObject
{
	FComponentTypeRegistryData();

	// Force a refresh of the components list right now (also calls the ComponentListChanged delegate to notify watchers)
	void ForceRefreshComponentList();

	static void AddBasicShapeComponents(TArray<FComponentClassComboEntryPtr>& SortedClassList);

	/** Implementation of FTickableEditorObject */
	virtual void Tick(float) override;
	virtual bool IsTickable() const override { return true; }
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(FTypeDatabaseUpdater, STATGROUP_Tickables); }

	// Request a refresh of the components list next frame
	void Invalidate()
	{
		bNeedsRefreshNextTick = true;
	}
public:
	/** End implementation of FTickableEditorObject */
	TArray<FComponentClassComboEntryPtr> ComponentClassList;
	TArray<FComponentTypeEntry> ComponentTypeList;
	TArray<FAssetData> PendingAssetData;
	FOnComponentTypeListChanged ComponentListChanged;
	bool bNeedsRefreshNextTick;
};

static const FString CommonClassGroup(TEXT("Common"));
// This has to stay in sync with logic in FKismetCompilerContext::FinishCompilingClass
static const FString BlueprintComponents(TEXT("Custom"));

template <typename ObjectType>
static ObjectType* FindOrLoadObject( const FString& ObjectPath )
{
	ObjectType* Object = FindObject<ObjectType>( nullptr, *ObjectPath );
	if( !Object )
	{
		Object = LoadObject<ObjectType>( nullptr, *ObjectPath );
	}

	return Object;
}

void FComponentTypeRegistryData::AddBasicShapeComponents(TArray<FComponentClassComboEntryPtr>& SortedClassList)
{
	FString BasicShapesHeading = LOCTEXT("BasicShapesHeading", "Basic Shapes").ToString();

	const auto OnBasicShapeCreated = [](UActorComponent* Component)
	{
		UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(Component);
		if (SMC)
		{
			const FString MaterialName = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");
			UMaterial* MaterialAsset = FindOrLoadObject<UMaterial>(MaterialName);
			SMC->SetMaterial(0, MaterialAsset);

			// If the component object is an archetype (template), propagate the material setting to any instances, as instances
			// of the archetype will end up being created BEFORE we are able to set the override material on the template object.
			if (SMC->HasAnyFlags(RF_ArchetypeObject))
			{
				TArray<UObject*> ArchetypeInstances;
				SMC->GetArchetypeInstances(ArchetypeInstances);
				for (UObject* ArchetypeInstance : ArchetypeInstances)
				{
					CastChecked<UStaticMeshComponent>(ArchetypeInstance)->SetMaterial(0, MaterialAsset);
				}
			}
		}
	};

	{
		FComponentEntryCustomizationArgs Args;
		Args.AssetOverride = FindOrLoadObject<UStaticMesh>(UActorFactoryBasicShape::BasicCube.ToString());
		Args.OnComponentCreated = FOnComponentCreated::CreateStatic(OnBasicShapeCreated);
		Args.ComponentNameOverride = LOCTEXT("BasicCubeShapeDisplayName", "Cube").ToString();
		Args.IconOverrideBrushName = FName("ClassIcon.Cube");
		Args.SortPriority = 2;

		{
			FComponentClassComboEntryPtr NewShape = MakeShareable(new FComponentClassComboEntry(BasicShapesHeading, UStaticMeshComponent::StaticClass(), true, EComponentCreateAction::SpawnExistingClass, Args));
			SortedClassList.Add(NewShape);
		}

		{
			//Cube also goes in the common group
			FComponentClassComboEntryPtr NewShape = MakeShareable(new FComponentClassComboEntry(CommonClassGroup, UStaticMeshComponent::StaticClass(), false, EComponentCreateAction::SpawnExistingClass, Args));
			SortedClassList.Add(NewShape);
		}
	}

	{
		FComponentEntryCustomizationArgs Args;
		Args.AssetOverride = FindOrLoadObject<UStaticMesh>(UActorFactoryBasicShape::BasicPlane.ToString());
		Args.OnComponentCreated = FOnComponentCreated::CreateStatic(OnBasicShapeCreated);
		Args.ComponentNameOverride = LOCTEXT("BasicPlaneShapeDisplayName", "Plane").ToString();
		Args.IconOverrideBrushName = FName("ClassIcon.Plane");
		Args.SortPriority = 2;

		{
			FComponentClassComboEntryPtr NewShape = MakeShareable(new FComponentClassComboEntry(BasicShapesHeading, UStaticMeshComponent::StaticClass(), true, EComponentCreateAction::SpawnExistingClass, Args));
			SortedClassList.Add(NewShape);
		}

		{
			//Quad also goes in the common group
			FComponentClassComboEntryPtr NewShape = MakeShareable(new FComponentClassComboEntry(CommonClassGroup, UStaticMeshComponent::StaticClass(), false, EComponentCreateAction::SpawnExistingClass, Args));
			SortedClassList.Add(NewShape);
		}
	}

	{
		FComponentEntryCustomizationArgs Args;
		Args.AssetOverride = FindOrLoadObject<UStaticMesh>(UActorFactoryBasicShape::BasicSphere.ToString());
		Args.OnComponentCreated = FOnComponentCreated::CreateStatic(OnBasicShapeCreated);
		Args.ComponentNameOverride = LOCTEXT("BasicSphereShapeDisplayName", "Sphere").ToString();
		Args.IconOverrideBrushName = FName("ClassIcon.Sphere");
		Args.SortPriority = 2;
		{
			FComponentClassComboEntryPtr NewShape = MakeShareable(new FComponentClassComboEntry(BasicShapesHeading, UStaticMeshComponent::StaticClass(), true, EComponentCreateAction::SpawnExistingClass, Args));
			SortedClassList.Add(NewShape);
		}

		{
			// Sphere also goes in the common group
			FComponentClassComboEntryPtr NewShape = MakeShareable(new FComponentClassComboEntry(CommonClassGroup, UStaticMeshComponent::StaticClass(), false, EComponentCreateAction::SpawnExistingClass, Args));
			SortedClassList.Add(NewShape);
		}
	}

	{
		FComponentEntryCustomizationArgs Args;
		Args.AssetOverride = FindOrLoadObject<UStaticMesh>(UActorFactoryBasicShape::BasicCylinder.ToString());
		Args.OnComponentCreated = FOnComponentCreated::CreateStatic(OnBasicShapeCreated);
		Args.ComponentNameOverride = LOCTEXT("BasicCylinderShapeDisplayName", "Cylinder").ToString();
		Args.IconOverrideBrushName = FName("ClassIcon.Cylinder");
		Args.SortPriority = 3;
		FComponentClassComboEntryPtr NewShape = MakeShareable(new FComponentClassComboEntry(BasicShapesHeading, UStaticMeshComponent::StaticClass(), true, EComponentCreateAction::SpawnExistingClass, Args));
		SortedClassList.Add(NewShape);
	}

	{
		FComponentEntryCustomizationArgs Args;
		Args.AssetOverride = FindOrLoadObject<UStaticMesh>(UActorFactoryBasicShape::BasicCone.ToString());
		Args.OnComponentCreated = FOnComponentCreated::CreateStatic(OnBasicShapeCreated);
		Args.ComponentNameOverride = LOCTEXT("BasicConeShapeDisplayName", "Cone").ToString();
		Args.IconOverrideBrushName = FName("ClassIcon.Cone");
		Args.SortPriority = 4;
		FComponentClassComboEntryPtr NewShape = MakeShareable(new FComponentClassComboEntry(BasicShapesHeading, UStaticMeshComponent::StaticClass(), true, EComponentCreateAction::SpawnExistingClass, Args));
		SortedClassList.Add(NewShape);
	}
}

FComponentTypeRegistryData::FComponentTypeRegistryData()
	: bNeedsRefreshNextTick(false)
{
	const auto HandleAdded = [](const FAssetData& Data, FComponentTypeRegistryData* Parent)
	{
		Parent->PendingAssetData.Push(Data);
	};

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	AssetRegistryModule.Get().OnAssetAdded().AddStatic(HandleAdded, this);
	AssetRegistryModule.Get().OnAssetRemoved().AddStatic(HandleAdded, this);

	const auto HandleRenamed = [](const FAssetData& Data, const FString&, FComponentTypeRegistryData* Parent)
	{
		Parent->PendingAssetData.Push(Data);
	};
	AssetRegistryModule.Get().OnAssetRenamed().AddStatic(HandleRenamed, this);
}

void FComponentTypeRegistryData::ForceRefreshComponentList()
{
	ComponentClassList.Empty();
	ComponentTypeList.Empty();

	struct SortComboEntry
	{
		bool operator () (const FComponentClassComboEntryPtr& A, const FComponentClassComboEntryPtr& B) const
		{
			bool bResult = false;

			// check headings first, if they are the same compare the individual entries
			int32 HeadingCompareResult = FCString::Stricmp(*A->GetHeadingText(), *B->GetHeadingText());
			if (HeadingCompareResult == 0)
			{
				if( A->GetSortPriority() == 0 && B->GetSortPriority() == 0 )
				{
					bResult = FCString::Stricmp(*A->GetClassName(), *B->GetClassName()) < 0;
				}
				else
				{
					bResult = A->GetSortPriority() < B->GetSortPriority();
				}
			}
			else if (CommonClassGroup == A->GetHeadingText())
			{
				bResult = true;
			}
			else if (CommonClassGroup == B->GetHeadingText())
			{
				bResult = false;
			}
			else
			{
				bResult = HeadingCompareResult < 0;
			}

			return bResult;
		}
	};

	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	{
		FString NewComponentsHeading = LOCTEXT("NewComponentsHeading", "Scripting").ToString();
		// Add new C++ component class
		FComponentClassComboEntryPtr NewClassHeader = MakeShareable(new FComponentClassComboEntry(NewComponentsHeading));
		ComponentClassList.Add(NewClassHeader);

		FComponentClassComboEntryPtr NewBPClass = MakeShareable(new FComponentClassComboEntry(NewComponentsHeading, UActorComponent::StaticClass(), true, EComponentCreateAction::CreateNewBlueprintClass));
		ComponentClassList.Add(NewBPClass);

		FComponentClassComboEntryPtr NewCPPClass = MakeShareable(new FComponentClassComboEntry(NewComponentsHeading, UActorComponent::StaticClass(), true, EComponentCreateAction::CreateNewCPPClass));
		ComponentClassList.Add(NewCPPClass);

		FComponentClassComboEntryPtr NewSeparator(new FComponentClassComboEntry());
		ComponentClassList.Add(NewSeparator);
	}

	TArray<FComponentClassComboEntryPtr> SortedClassList;

	AddBasicShapeComponents(SortedClassList);

	TArray<FName> InMemoryClasses;
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		// If this is a subclass of Actor Component, not abstract, and tagged as spawnable from Kismet
		if (Class->IsChildOf(UActorComponent::StaticClass()))
		{
			InMemoryClasses.Push(Class->GetFName());

			bool const bOutOfDateClass = Class->HasAnyClassFlags(CLASS_NewerVersionExists);
			bool const bBlueprintSkeletonClass = FKismetEditorUtilities::IsClassABlueprintSkeleton(Class);
			if (!Class->HasAnyClassFlags(CLASS_Abstract) && Class->HasMetaData(FBlueprintMetadata::MD_BlueprintSpawnableComponent) && !bOutOfDateClass && !bBlueprintSkeletonClass) //@TODO: Fold this logic together with the one in UEdGraphSchema_K2::GetAddComponentClasses
			{
				TArray<FString> ClassGroupNames;
				Class->GetClassGroupNames(ClassGroupNames);

				if (ClassGroupNames.Contains(CommonClassGroup))
				{
					FString ClassGroup = CommonClassGroup;
					FComponentClassComboEntryPtr NewEntry(new FComponentClassComboEntry(ClassGroup, Class, ClassGroupNames.Num() <= 1, EComponentCreateAction::SpawnExistingClass));
					SortedClassList.Add(NewEntry);
				}
				if (ClassGroupNames.Num() && !ClassGroupNames[0].Equals(CommonClassGroup))
				{
					const bool bIncludeInFilter = true;

					FString ClassGroup = ClassGroupNames[0];
					FComponentClassComboEntryPtr NewEntry(new FComponentClassComboEntry(ClassGroup, Class, bIncludeInFilter, EComponentCreateAction::SpawnExistingClass));
					SortedClassList.Add(NewEntry);
				}
				else if(ClassGroupNames.Num() == 0 )
				{
					// No class group name found. Just add it to a "custom" category
					
					const bool bIncludeInFilter = true;
					FString ClassGroup = LOCTEXT("CustomClassGroup", "Custom").ToString();
					FComponentClassComboEntryPtr NewEntry(new FComponentClassComboEntry(ClassGroup, Class, bIncludeInFilter, EComponentCreateAction::SpawnExistingClass));
					SortedClassList.Add(NewEntry);
				}
			}
			
			if (!bOutOfDateClass && !bBlueprintSkeletonClass)
			{
				FComponentTypeEntry Entry = { Class->GetName(), FString(), Class };
				ComponentTypeList.Add(Entry);
			}
		}
	}

	{
		// make sure that we add any user created classes immediately, generally this will not create anything (because assets have not been discovered yet), but asset discovery
		// should be allowed to take place at any time:
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		TArray<FName> ClassNames;
		ClassNames.Add(UActorComponent::StaticClass()->GetFName());
		TSet<FName> DerivedClassNames;
		AssetRegistryModule.Get().GetDerivedClassNames(ClassNames, TSet<FName>(), DerivedClassNames);

		const bool bIncludeInFilter = true;

		auto InMemoryClassesSet = TSet<FName>(InMemoryClasses);
		auto OnDiskClasses = DerivedClassNames.Difference(InMemoryClassesSet);

		if (OnDiskClasses.Num() > 0)
		{
			// GetAssetsByClass call is a kludge to get the full asset paths for the blueprints we care about, Bob T. thinks 
			// that the Asset Registry could just keep asset paths:
			TArray<FAssetData> BlueprintAssetData;
			AssetRegistryModule.Get().GetAssetsByClass(UBlueprint::StaticClass()->GetFName(), BlueprintAssetData);
			TMap<FString, FAssetData> BlueprintNames;
			for (const FAssetData& Blueprint : BlueprintAssetData)
			{
				const auto& name = Blueprint.AssetName.ToString();
				BlueprintNames.Add(name, Blueprint);
			}

			for (const FName& OnDiskClass : OnDiskClasses)
			{
				FString FixedString = OnDiskClass.ToString();
				FixedString.RemoveFromEnd(TEXT("_C"));

				FAssetData AssetData;
				if (const FAssetData* Value = BlueprintNames.Find(FixedString))
				{
					AssetData = *Value;
				}

				FComponentTypeEntry Entry = { FixedString, AssetData.ObjectPath.ToString(), nullptr };
				ComponentTypeList.Add(Entry);

				// The blueprint is unloaded, so we need to work out which icon to use for it using its asset data
				const UClass* BlueprintIconClass = FClassIconFinder::GetIconClassForAssetData(AssetData);

				FComponentClassComboEntryPtr NewEntry(new FComponentClassComboEntry(BlueprintComponents, FixedString, AssetData.ObjectPath, BlueprintIconClass, bIncludeInFilter));
				SortedClassList.Add(NewEntry);
			}
		}
	}

	if (SortedClassList.Num() > 0)
	{
		Sort(SortedClassList.GetData(), SortedClassList.Num(), SortComboEntry());

		FString PreviousHeading;
		for (int32 ClassIndex = 0; ClassIndex < SortedClassList.Num(); ClassIndex++)
		{
			FComponentClassComboEntryPtr& CurrentEntry = SortedClassList[ClassIndex];

			const FString& CurrentHeadingText = CurrentEntry->GetHeadingText();

			if (CurrentHeadingText != PreviousHeading)
			{
				// This avoids a redundant separator being added to the very top of the list
				if (ClassIndex > 0)
				{
					FComponentClassComboEntryPtr NewSeparator(new FComponentClassComboEntry());
					ComponentClassList.Add(NewSeparator);
				}
				FComponentClassComboEntryPtr NewHeading(new FComponentClassComboEntry(CurrentHeadingText));
				ComponentClassList.Add(NewHeading);

				PreviousHeading = CurrentHeadingText;
			}

			ComponentClassList.Add(CurrentEntry);
		}
	}

	ComponentListChanged.Broadcast();
}

void FComponentTypeRegistryData::Tick(float)
{
	bool bRequiresRefresh = bNeedsRefreshNextTick;
	bNeedsRefreshNextTick = false;

	if (PendingAssetData.Num() != 0)
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		TArray<FName> ClassNames;
		ClassNames.Add(UActorComponent::StaticClass()->GetFName());
		TSet<FName> DerivedClassNames;
		AssetRegistryModule.Get().GetDerivedClassNames(ClassNames, TSet<FName>(), DerivedClassNames);

		for (auto Asset : PendingAssetData)
		{
			const FName BPParentClassName(GET_MEMBER_NAME_CHECKED(UBlueprint, ParentClass));
			const FString TagValue = Asset.GetTagValueRef<FString>(BPParentClassName);
			const FString ObjectPath = FPackageName::ExportTextPathToObjectPath(TagValue);
			FName ObjectName = FName(*FPackageName::ObjectPathToObjectName(ObjectPath));
			if (DerivedClassNames.Contains(ObjectName))
			{
				bRequiresRefresh = true;
				break;
			}
		}
		PendingAssetData.Empty();
	}

	if (bRequiresRefresh)
	{
		ForceRefreshComponentList();
	}
}

//////////////////////////////////////////////////////////////////////////
// FComponentTypeRegistry

FComponentTypeRegistry& FComponentTypeRegistry::Get()
{
	static FComponentTypeRegistry ComponentRegistry;
	return ComponentRegistry;
}

FOnComponentTypeListChanged& FComponentTypeRegistry::SubscribeToComponentList(TArray<FComponentClassComboEntryPtr>*& OutComponentList)
{
	check(Data);
	OutComponentList = &Data->ComponentClassList;
	return Data->ComponentListChanged;
}

FOnComponentTypeListChanged& FComponentTypeRegistry::SubscribeToComponentList(const TArray<FComponentTypeEntry>*& OutComponentList)
{
	check(Data);
	OutComponentList = &Data->ComponentTypeList;
	return Data->ComponentListChanged;
}

FOnComponentTypeListChanged& FComponentTypeRegistry::GetOnComponentTypeListChanged()
{
	check(Data);
	return Data->ComponentListChanged;
}

FComponentTypeRegistry::FComponentTypeRegistry()
	: Data(nullptr)
{
	Data = new FComponentTypeRegistryData();
	Data->ForceRefreshComponentList();

	IHotReloadInterface& HotReloadSupport = FModuleManager::LoadModuleChecked<IHotReloadInterface>("HotReload");
	HotReloadSupport.OnHotReload().AddRaw(this, &FComponentTypeRegistry::OnProjectHotReloaded);
}

FComponentTypeRegistry::~FComponentTypeRegistry()
{
	if( FModuleManager::Get().IsModuleLoaded("HotReload") )
	{
		IHotReloadInterface& HotReloadSupport = FModuleManager::GetModuleChecked<IHotReloadInterface>("HotReload");
		HotReloadSupport.OnHotReload().RemoveAll(this);
	}
}

void FComponentTypeRegistry::OnProjectHotReloaded( bool bWasTriggeredAutomatically )
{
	Data->ForceRefreshComponentList();
}

void FComponentTypeRegistry::InvalidateClass(TSubclassOf<UActorComponent> /*ClassToUpdate*/)
{
	Data->Invalidate();
}


#undef LOCTEXT_NAMESPACE
