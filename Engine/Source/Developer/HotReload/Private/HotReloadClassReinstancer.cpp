// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "HotReloadClassReinstancer.h"
#include "Serialization/MemoryWriter.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "UObject/Package.h"
#include "Serialization/ArchiveReplaceObjectRef.h"
#if WITH_ENGINE
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#endif

#if WITH_ENGINE

void FHotReloadClassReinstancer::SetupNewClassReinstancing(UClass* InNewClass, UClass* InOldClass)
{
	// Set base class members to valid values
	ClassToReinstance = InNewClass;
	DuplicatedClass = InOldClass;
	OriginalCDO = InOldClass->GetDefaultObject();
	bHasReinstanced = false;
	bNeedsReinstancing = true;
	NewClass = InNewClass;

	// Collect the original CDO property values
	SerializeCDOProperties(InOldClass->GetDefaultObject(), OriginalCDOProperties);
	// Collect the property values of the new CDO
	SerializeCDOProperties(InNewClass->GetDefaultObject(), ReconstructedCDOProperties);

	SaveClassFieldMapping(InOldClass);

	ObjectsThatShouldUseOldStuff.Add(InOldClass); //CDO of REINST_ class can be used as archetype

	TArray<UClass*> ChildrenOfClass;
	GetDerivedClasses(InOldClass, ChildrenOfClass);
	for (auto ClassIt = ChildrenOfClass.CreateConstIterator(); ClassIt; ++ClassIt)
	{
		UClass* ChildClass = *ClassIt;
		UBlueprint* ChildBP = Cast<UBlueprint>(ChildClass->ClassGeneratedBy);
		if (ChildBP && !ChildBP->HasAnyFlags(RF_BeingRegenerated))
		{
			// If this is a direct child, change the parent and relink so the property chain is valid for reinstancing
			if (!ChildBP->HasAnyFlags(RF_NeedLoad))
			{
				if (ChildClass->GetSuperClass() == InOldClass)
				{
					ReparentChild(ChildBP);
				}

				Children.AddUnique(ChildBP);
				if (ChildBP->ParentClass == InOldClass)
				{
					ChildBP->ParentClass = NewClass;
				}
			}
			else
			{
				// If this is a child that caused the load of their parent, relink to the REINST class so that we can still serialize in the CDO, but do not add to later processing
				ReparentChild(ChildClass);
			}
		}
	}

	// Finally, remove the old class from Root so that it can get GC'd and mark it as CLASS_NewerVersionExists
	InOldClass->RemoveFromRoot();
	InOldClass->ClassFlags |= CLASS_NewerVersionExists;
}

void FHotReloadClassReinstancer::SerializeCDOProperties(UObject* InObject, FHotReloadClassReinstancer::FCDOPropertyData& OutData)
{
	// Creates a mem-comparable CDO data
	class FCDOWriter : public FMemoryWriter
	{
		/** Objects already visited by this archive */
		TSet<UObject*>& VisitedObjects;
		/** Output property data */
		FCDOPropertyData& PropertyData;
		/** Current subobject being serialized */
		FName SubobjectName;

	public:
		/** Serializes all script properties of the provided DefaultObject */
		FCDOWriter(FCDOPropertyData& InOutData, UObject* DefaultObject, TSet<UObject*>& InVisitedObjects, FName InSubobjectName = NAME_None)
			: FMemoryWriter(InOutData.Bytes, /* bIsPersistent = */ false, /* bSetOffset = */ true)
			, VisitedObjects(InVisitedObjects)
			, PropertyData(InOutData)
			, SubobjectName(InSubobjectName)
		{
			// Disable delta serialization, we want to serialize everything
			ArNoDelta = true;
			DefaultObject->SerializeScriptProperties(*this);
		}
		virtual void Serialize(void* Data, int64 Num) override
		{
			// Collect serialized properties so we can later update their values on instances if they change
			UProperty* SerializedProperty = GetSerializedProperty();
			if (SerializedProperty != nullptr)
			{
				FCDOProperty& PropertyInfo = PropertyData.Properties.FindOrAdd(SerializedProperty->GetFName());
				if (PropertyInfo.Property == nullptr)
				{
					PropertyInfo.Property = SerializedProperty;
					PropertyInfo.SubobjectName = SubobjectName;
					PropertyInfo.SerializedValueOffset = Tell();
					PropertyInfo.SerializedValueSize = Num;
				}
				else
				{
					PropertyInfo.SerializedValueSize += Num;
				}
			}
			FMemoryWriter::Serialize(Data, Num);
		}
		/** Serializes an object. Only name and class for normal references, deep serialization for DSOs */
		virtual FArchive& operator<<(class UObject*& InObj) override
		{
			FArchive& Ar = *this;
			if (InObj)
			{
				FName ClassName = InObj->GetClass()->GetFName();
				FName ObjectName = InObj->GetFName();
				Ar << ClassName;
				Ar << ObjectName;
				if (!VisitedObjects.Contains(InObj))
				{
					VisitedObjects.Add(InObj);
					if (Ar.GetSerializedProperty() && Ar.GetSerializedProperty()->ContainsInstancedObjectProperty())
					{
						// Serialize all DSO properties too					
						FCDOWriter DefaultSubobjectWriter(PropertyData, InObj, VisitedObjects, InObj->GetFName());
						Seek(PropertyData.Bytes.Num());
					}
				}
			}
			else
			{
				FName UnusedName = NAME_None;
				Ar << UnusedName;
				Ar << UnusedName;
			}

			return *this;
		}
		/** Serializes an FName as its index and number */
		virtual FArchive& operator<<(FName& InName) override
		{
			FArchive& Ar = *this;
			NAME_INDEX ComparisonIndex = InName.GetComparisonIndex();
			NAME_INDEX DisplayIndex = InName.GetDisplayIndex();
			int32 Number = InName.GetNumber();
			Ar << ComparisonIndex;
			Ar << DisplayIndex;
			Ar << Number;
			return Ar;
		}
		virtual FArchive& operator<<(FLazyObjectPtr& LazyObjectPtr) override
		{
			FArchive& Ar = *this;
			FUniqueObjectGuid UniqueID = LazyObjectPtr.GetUniqueID();
			Ar << UniqueID;
			return *this;
		}
		virtual FArchive& operator<<(FSoftObjectPtr& Value) override
		{
			FArchive& Ar = *this;
			FSoftObjectPath UniqueID = Value.GetUniqueID();
			Ar << UniqueID;
			return Ar;
		}
		virtual FArchive& operator<<(FSoftObjectPath& Value) override
		{
			FArchive& Ar = *this;

			FString Path = Value.ToString();

			Ar << Path;

			if (IsLoading())
			{
				Value.SetPath(MoveTemp(Path));
			}

			return Ar;
		}
		FArchive& operator<<(FWeakObjectPtr& WeakObjectPtr) override
		{
			WeakObjectPtr.Serialize(*this);
			return *this;
		}
		/** Archive name, for debugging */
		virtual FString GetArchiveName() const override { return TEXT("FCDOWriter"); }
	};
	TSet<UObject*> VisitedObjects;
	VisitedObjects.Add(InObject);
	FCDOWriter Ar(OutData, InObject, VisitedObjects);
}

void FHotReloadClassReinstancer::ReconstructClassDefaultObject(UClass* InClass, UObject* InOuter, FName InName, EObjectFlags InFlags)
{
	// Get the parent CDO
	UClass* ParentClass = InClass->GetSuperClass();
	UObject* ParentDefaultObject = NULL;
	if (ParentClass != NULL)
	{
		ParentDefaultObject = ParentClass->GetDefaultObject(); // Force the default object to be constructed if it isn't already
	}

	// Re-create
	InClass->ClassDefaultObject = StaticAllocateObject(InClass, InOuter, InName, InFlags, EInternalObjectFlags::None, false);
	check(InClass->ClassDefaultObject);
	const bool bShouldInitializeProperties = false;
	const bool bCopyTransientsFromClassDefaults = false;
	(*InClass->ClassConstructor)(FObjectInitializer(InClass->ClassDefaultObject, ParentDefaultObject, bCopyTransientsFromClassDefaults, bShouldInitializeProperties));
}

void FHotReloadClassReinstancer::RecreateCDOAndSetupOldClassReinstancing(UClass* InOldClass)
{
	// Set base class members to valid values
	ClassToReinstance = InOldClass;
	DuplicatedClass = InOldClass;
	OriginalCDO = InOldClass->GetDefaultObject();
	bHasReinstanced = false;
	bNeedsReinstancing = false;
	NewClass = InOldClass; // The class doesn't change in this case

	// Collect the original property values
	SerializeCDOProperties(InOldClass->GetDefaultObject(), OriginalCDOProperties);

	// Remember all the basic info about the object before we rename it
	EObjectFlags CDOFlags = OriginalCDO->GetFlags();
	UObject* CDOOuter = OriginalCDO->GetOuter();
	FName CDOName = OriginalCDO->GetFName();

	// Rename original CDO, so we can store this one as OverridenArchetypeForCDO
	// and create new one with the same name and outer.
	OriginalCDO->Rename(
		*MakeUniqueObjectName(
			GetTransientPackage(),
			OriginalCDO->GetClass(),
			*FString::Printf(TEXT("BPGC_ARCH_FOR_CDO_%s"), *InOldClass->GetName())
		).ToString(),
		GetTransientPackage(),
		REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional | REN_SkipGeneratedClasses | REN_ForceNoResetLoaders);
	
	// Re-create the CDO, re-running its constructor
	ReconstructClassDefaultObject(InOldClass, CDOOuter, CDOName, CDOFlags);

	ReconstructedCDOsMap.Add(OriginalCDO, InOldClass->GetDefaultObject());

	// Collect the property values after re-constructing the CDO
	SerializeCDOProperties(InOldClass->GetDefaultObject(), ReconstructedCDOProperties);

	// We only want to re-instance the old class if its CDO's values have changed or any of its DSOs' property values have changed
	if (DefaultPropertiesHaveChanged())
	{
		bNeedsReinstancing = true;
		SaveClassFieldMapping(InOldClass);

		TArray<UClass*> ChildrenOfClass;
		GetDerivedClasses(InOldClass, ChildrenOfClass);
		for (auto ClassIt = ChildrenOfClass.CreateConstIterator(); ClassIt; ++ClassIt)
		{
			UClass* ChildClass = *ClassIt;
			UBlueprint* ChildBP = Cast<UBlueprint>(ChildClass->ClassGeneratedBy);
			if (ChildBP && !ChildBP->HasAnyFlags(RF_BeingRegenerated))
			{
				if (!ChildBP->HasAnyFlags(RF_NeedLoad))
				{
					Children.AddUnique(ChildBP);
					UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(ChildBP->GeneratedClass);
					UObject* CurrentCDO = BPGC ? BPGC->GetDefaultObject(false) : nullptr;
					if (CurrentCDO && (OriginalCDO == CurrentCDO->GetArchetype()))
					{
						BPGC->OverridenArchetypeForCDO = OriginalCDO;
					}
				}
			}
		}
	}
}

FHotReloadClassReinstancer::FHotReloadClassReinstancer(UClass* InNewClass, UClass* InOldClass, const TMap<UClass*, UClass*>& InOldToNewClassesMap, TMap<UObject*, UObject*>& OutReconstructedCDOsMap, TSet<UBlueprint*>& InBPSetToRecompile, TSet<UBlueprint*>& InBPSetToRecompileBytecodeOnly)
	: NewClass(nullptr)
	, bNeedsReinstancing(false)
	, CopyOfPreviousCDO(nullptr)
	, ReconstructedCDOsMap(OutReconstructedCDOsMap)
	, BPSetToRecompile(InBPSetToRecompile)
	, BPSetToRecompileBytecodeOnly(InBPSetToRecompileBytecodeOnly)
	, OldToNewClassesMap(InOldToNewClassesMap)
{
	ensure(InOldClass);
	ensure(!HotReloadedOldClass && !HotReloadedNewClass);
	HotReloadedOldClass = InOldClass;
	HotReloadedNewClass = InNewClass ? InNewClass : InOldClass;

	for (const TPair<UClass*, UClass*>& OldToNewClass : OldToNewClassesMap)
	{
		ObjectsThatShouldUseOldStuff.Add(OldToNewClass.Key);
	}

	// If InNewClass is NULL, then the old class has not changed after hot-reload.
	// However, we still need to check for changes to its constructor code (CDO values).
	if (InNewClass)
	{
		SetupNewClassReinstancing(InNewClass, InOldClass);

		TMap<UObject*, UObject*> ClassRedirects;
		ClassRedirects.Add(InOldClass, InNewClass);

		for (TObjectIterator<UBlueprint> BlueprintIt; BlueprintIt; ++BlueprintIt)
		{
			FArchiveReplaceObjectRef<UObject> ReplaceObjectArch(*BlueprintIt, ClassRedirects, false, true, true);
			if (ReplaceObjectArch.GetCount())
			{
				EnlistDependentBlueprintToRecompile(*BlueprintIt, false);
			}
		}
	}
	else
	{
		RecreateCDOAndSetupOldClassReinstancing(InOldClass);
	}
}

FHotReloadClassReinstancer::~FHotReloadClassReinstancer()
{
	// Make sure the base class does not remove the DuplicatedClass from root, we not always want it.
	// For example when we're just reconstructing CDOs. Other cases are handled by HotReloadClassReinstancer.
	DuplicatedClass = nullptr;

	ensure(HotReloadedOldClass);
	HotReloadedOldClass = nullptr;
	HotReloadedNewClass = nullptr;
}

/** Helper for finding subobject in an array. Usually there's not that many subobjects on a class to justify a TMap */
FORCEINLINE static UObject* FindDefaultSubobject(TArray<UObject*>& InDefaultSubobjects, FName SubobjectName)
{
	for (UObject* Subobject : InDefaultSubobjects)
	{
		if (Subobject->GetFName() == SubobjectName)
		{
			return Subobject;
		}
	}
	return nullptr;
}

void FHotReloadClassReinstancer::UpdateDefaultProperties()
{
	struct FPropertyToUpdate
	{
		UProperty* Property;
		FName SubobjectName;
		uint8* OldSerializedValuePtr;
		uint8* NewValuePtr;
		int64 OldSerializedSize;
	};
	/** Memory writer archive that supports UObject values the same way as FCDOWriter. */
	class FPropertyValueMemoryWriter : public FMemoryWriter
	{
	public:
		FPropertyValueMemoryWriter(TArray<uint8>& OutData)
			: FMemoryWriter(OutData)
		{}
		virtual FArchive& operator<<(class UObject*& InObj) override
		{
			FArchive& Ar = *this;
			if (InObj)
			{
				FName ClassName = InObj->GetClass()->GetFName();
				FName ObjectName = InObj->GetFName();
				Ar << ClassName;
				Ar << ObjectName;
			}
			else
			{
				FName UnusedName = NAME_None;
				Ar << UnusedName;
				Ar << UnusedName;
			}
			return *this;
		}
		virtual FArchive& operator<<(FName& InName) override
		{
			FArchive& Ar = *this;
			NAME_INDEX ComparisonIndex = InName.GetComparisonIndex();
			NAME_INDEX DisplayIndex = InName.GetDisplayIndex();
			int32 Number = InName.GetNumber();
			Ar << ComparisonIndex;
			Ar << DisplayIndex;
			Ar << Number;
			return Ar;
		}
		virtual FArchive& operator<<(FLazyObjectPtr& LazyObjectPtr) override
		{
			FArchive& Ar = *this;
			FUniqueObjectGuid UniqueID = LazyObjectPtr.GetUniqueID();
			Ar << UniqueID;
			return *this;
		}
		virtual FArchive& operator<<(FSoftObjectPtr& Value) override
		{
			FArchive& Ar = *this;
			FSoftObjectPath UniqueID = Value.GetUniqueID();
			Ar << UniqueID;
			return Ar;
		}
		virtual FArchive& operator<<(FSoftObjectPath& Value) override
		{
			FArchive& Ar = *this;

			FString Path = Value.ToString();

			Ar << Path;

			if (IsLoading())
			{
				Value.SetPath(MoveTemp(Path));
			}

			return Ar;
		}
		FArchive& operator<<(FWeakObjectPtr& WeakObjectPtr) override
		{
			WeakObjectPtr.Serialize(*this);
			return *this;
		}
	};

	// Collect default subobjects to update their properties too
	const int32 DefaultSubobjectArrayCapacity = 16;
	TArray<UObject*> DefaultSubobjectArray;
	DefaultSubobjectArray.Empty(DefaultSubobjectArrayCapacity);
	NewClass->GetDefaultObject()->CollectDefaultSubobjects(DefaultSubobjectArray);

	TArray<FPropertyToUpdate> PropertiesToUpdate;
	// Collect all properties that have actually changed
	for (const TPair<FName, FCDOProperty>& Pair : ReconstructedCDOProperties.Properties)
	{
		FCDOProperty* OldPropertyInfo = OriginalCDOProperties.Properties.Find(Pair.Key);
		if (OldPropertyInfo)
		{
			const FCDOProperty& NewPropertyInfo = Pair.Value;

			uint8* OldSerializedValuePtr = OriginalCDOProperties.Bytes.GetData() + OldPropertyInfo->SerializedValueOffset;
			uint8* NewSerializedValuePtr = ReconstructedCDOProperties.Bytes.GetData() + NewPropertyInfo.SerializedValueOffset;
			if (OldPropertyInfo->SerializedValueSize != NewPropertyInfo.SerializedValueSize ||
				FMemory::Memcmp(OldSerializedValuePtr, NewSerializedValuePtr, OldPropertyInfo->SerializedValueSize) != 0)
			{
				// Property value has changed so add it to the list of properties that need updating on instances
				FPropertyToUpdate PropertyToUpdate;
				PropertyToUpdate.Property = NewPropertyInfo.Property;
				PropertyToUpdate.NewValuePtr = nullptr;
				PropertyToUpdate.SubobjectName = NewPropertyInfo.SubobjectName;

				if (NewPropertyInfo.Property->GetOuter() == NewClass)
				{
					PropertyToUpdate.NewValuePtr = PropertyToUpdate.Property->ContainerPtrToValuePtr<uint8>(NewClass->GetDefaultObject());
				}
				else if (NewPropertyInfo.SubobjectName != NAME_None)
				{
					UObject* DefaultSubobjectPtr = FindDefaultSubobject(DefaultSubobjectArray, NewPropertyInfo.SubobjectName);
					if (DefaultSubobjectPtr && NewPropertyInfo.Property->GetOuter() == DefaultSubobjectPtr->GetClass())
					{
						PropertyToUpdate.NewValuePtr = PropertyToUpdate.Property->ContainerPtrToValuePtr<uint8>(DefaultSubobjectPtr);
					}
				}
				if (PropertyToUpdate.NewValuePtr)
				{					
					PropertyToUpdate.OldSerializedValuePtr = OldSerializedValuePtr;
					PropertyToUpdate.OldSerializedSize = OldPropertyInfo->SerializedValueSize;

					PropertiesToUpdate.Add(PropertyToUpdate);
				}					
			}
		}
	}
	if (PropertiesToUpdate.Num())
	{
		TArray<uint8> CurrentValueSerializedData;		

		// Update properties on all existing instances of the class
		for (FObjectIterator It(NewClass); It; ++It)
		{
			UObject* ObjectPtr = *It;
			DefaultSubobjectArray.Empty(DefaultSubobjectArrayCapacity);
			ObjectPtr->CollectDefaultSubobjects(DefaultSubobjectArray);

			for (auto& PropertyToUpdate : PropertiesToUpdate)
			{
				uint8* InstanceValuePtr = nullptr;
				if (PropertyToUpdate.SubobjectName == NAME_None)
				{
					InstanceValuePtr = PropertyToUpdate.Property->ContainerPtrToValuePtr<uint8>(ObjectPtr);
				}
				else
				{
					UObject* DefaultSubobjectPtr = FindDefaultSubobject(DefaultSubobjectArray, PropertyToUpdate.SubobjectName);
					if (DefaultSubobjectPtr && PropertyToUpdate.Property->GetOuter() == DefaultSubobjectPtr->GetClass())
					{
						InstanceValuePtr = PropertyToUpdate.Property->ContainerPtrToValuePtr<uint8>(DefaultSubobjectPtr);
					}
				}

				if (InstanceValuePtr)
				{
					// Serialize current value to a byte array as we don't have the previous CDO to compare against, we only have its serialized property data
					CurrentValueSerializedData.Empty(CurrentValueSerializedData.Num() + CurrentValueSerializedData.GetSlack());
					FPropertyValueMemoryWriter CurrentValueWriter(CurrentValueSerializedData);
					PropertyToUpdate.Property->SerializeItem(CurrentValueWriter, InstanceValuePtr);

					// Update only when the current value on the instance is identical to the original CDO
					if (CurrentValueSerializedData.Num() == PropertyToUpdate.OldSerializedSize &&
						FMemory::Memcmp(CurrentValueSerializedData.GetData(), PropertyToUpdate.OldSerializedValuePtr, CurrentValueSerializedData.Num()) == 0)
					{
						// Update with the new value
						PropertyToUpdate.Property->CopyCompleteValue(InstanceValuePtr, PropertyToUpdate.NewValuePtr);
					}
				}
			}
		}
	}
}

void FHotReloadClassReinstancer::ReinstanceObjectsAndUpdateDefaults()
{
	ReinstanceObjects(true);
	UpdateDefaultProperties();
}

void FHotReloadClassReinstancer::AddReferencedObjects(FReferenceCollector& Collector)
{
	FBlueprintCompileReinstancer::AddReferencedObjects(Collector);
	Collector.AllowEliminatingReferences(false);
	Collector.AddReferencedObject(CopyOfPreviousCDO);
	Collector.AllowEliminatingReferences(true);
}

void FHotReloadClassReinstancer::EnlistDependentBlueprintToRecompile(UBlueprint* BP, bool bBytecodeOnly)
{
	if (IsValid(BP))
	{
		if (bBytecodeOnly)
		{
			if (!BPSetToRecompile.Contains(BP) && !BPSetToRecompileBytecodeOnly.Contains(BP))
			{
				BPSetToRecompileBytecodeOnly.Add(BP);
			}
		}
		else
		{
			if (!BPSetToRecompile.Contains(BP))
			{
				if (BPSetToRecompileBytecodeOnly.Contains(BP))
				{
					BPSetToRecompileBytecodeOnly.Remove(BP);
				}

				BPSetToRecompile.Add(BP);
			}
		}
	}
}

void FHotReloadClassReinstancer::BlueprintWasRecompiled(UBlueprint* BP, bool bBytecodeOnly)
{
	BPSetToRecompile.Remove(BP);
	BPSetToRecompileBytecodeOnly.Remove(BP);

	FBlueprintCompileReinstancer::BlueprintWasRecompiled(BP, bBytecodeOnly);
}

#endif
