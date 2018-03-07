// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectHash.h"
#include "UObject/Object.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#include "Serialization/ArchiveObjectCrc32.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

class FArchiveSkipTransientObjectCRC32 : public FArchiveObjectCrc32
{
public:
	static bool CanPropertyBeDifferentInConvertedCDO(const UProperty* InProperty)
	{
		check(InProperty);
		return InProperty->HasAllPropertyFlags(CPF_Transient)
			|| InProperty->HasAllPropertyFlags(CPF_EditorOnly)
			|| InProperty->GetName() == TEXT("BlueprintCreatedComponents")
			|| InProperty->GetName() == TEXT("CreationMethod")
			|| InProperty->GetName() == TEXT("InstanceComponents")
			|| InProperty->GetName() == TEXT("bNetAddressable")
			|| InProperty->GetName() == TEXT("OwnedComponents");
	}
	// Begin FArchive Interface
	virtual bool ShouldSkipProperty(const UProperty* InProperty) const override
	{
		return FArchiveObjectCrc32::ShouldSkipProperty(InProperty)
			|| CanPropertyBeDifferentInConvertedCDO(InProperty);
	}
	// End FArchive Interface
};

void ClearNativeRecursive(UObject* OnObject)
{
	OnObject->ClearInternalFlags(EInternalObjectFlags::Native);
	TArray<UObject*> Children;
	GetObjectsWithOuter(OnObject, Children, false);
	for (UObject* Entry : Children)
	{
		ClearNativeRecursive(Entry);
	}
}

// We mess with the rootset flag instead of using FGCObject because it's an error for any test data to remain in the rootset after the test runs
struct FOwnedObjectsHelper
{
	~FOwnedObjectsHelper()
	{
		for (UObject* Entry : OwnedObjects)
		{
			Entry->RemoveFromRoot();
			Entry->ClearFlags(RF_Standalone);
			if (Entry->IsNative())
			{
				ClearNativeRecursive(Entry);
			}

			// Actors need to be explicitly destroyed, probably just to remove them from their owning
			// level:
			if (AActor* AsActor = Cast<AActor>(Entry))
			{
				AsActor->Destroy();
			}
		}
		CollectGarbage(RF_NoFlags);
	}

	void Push(UObject* Obj)
	{
		Obj->AddToRoot();
		OwnedObjects.Push(Obj);
	}
private:
	TArray<UObject*> OwnedObjects;
};

// Helper functions introduced to load classes (generated or native:
static UClass* GetGeneratedClass(const TCHAR* TestFolder, const TCHAR* ClassName, FAutomationTestBase* Context, FOwnedObjectsHelper &OwnedObjects)
{
	FString FullName = FString::Printf(TEXT("/RuntimeTests/CompilerTests/%s/%s.%s"), TestFolder, ClassName, ClassName);
	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *FullName);
	
	if (!Blueprint)
	{
		Context->AddWarning(FString::Printf(TEXT("Missing blueprint for test: '%s'"), *FullName));
		return nullptr;
	}

	TArray<UObject*> Objects;
	GetObjectsWithOuter(Blueprint->GetOuter(), Objects, false);
	for (UObject* Entry : Objects)
	{
		OwnedObjects.Push(Entry);
	}

	CollectGarbage(RF_NoFlags);

	return Blueprint->GeneratedClass;
}

static UClass* GetNativeClass(const TCHAR* TestFolder, const TCHAR* ClassName, FAutomationTestBase* Context, FOwnedObjectsHelper &OwnedObjects)
{
	CollectGarbage(RF_NoFlags);

	FString FullName = FString::Printf(TEXT("/RuntimeTests/CompilerTests/%s/%s"), TestFolder, ClassName);
	UPackage* NativePackage = CreatePackage(nullptr, *FullName);
	check(NativePackage);

	const FString FStringFullPathName = FString::Printf(TEXT("%s.%s_C"), *FullName, ClassName);
	UClass* Ret = (UClass*)ConstructDynamicType(*FStringFullPathName, EConstructDynamicType::CallZConstructor); //FindObjectFast<UClass>(NativePackage, *(FString(ClassName) + TEXT("_C")));

	if (!Ret)
	{
		Context->AddWarning(FString::Printf(TEXT("Missing native type for test: '%s'"), ClassName));
		return nullptr;
	}

	TArray<UObject*> Objects;
	GetObjectsWithOuter(NativePackage, Objects, false);
	for (UObject* Entry : Objects)
	{
		OwnedObjects.Push(Entry);
	}

	CollectGarbage(RF_NoFlags);

	return Ret;
}

typedef UClass* (*ClassAccessor)(const TCHAR*, const TCHAR*, FAutomationTestBase*, FOwnedObjectsHelper&);

typedef uint32(*TestImpl)(ClassAccessor F, FAutomationTestBase* Context);

// This pattern is repeated in each test, so I'm just writing a helper function rather than copy/pasting it:
static bool RunTestHelper(TestImpl T, FAutomationTestBase* Context)
{
	uint32 ResultsGenerated = T(&GetGeneratedClass, Context);
	uint32 ResultsNative = T(&GetNativeClass, Context);

	if (ResultsGenerated == 0)
	{
		Context->AddError(TEXT("Test failed to run!"));
	}
	else if (ResultsGenerated != ResultsNative)
	{
		Context->AddError(TEXT("Native differs from generated!"));
	}

	return true;
}

// Helper functions introduced to avoid crashing if classes are missing:
static UObject* NewTestObject(UClass* Class, FOwnedObjectsHelper &OwnedObjects)
{
	if (Class)
	{
		UObject* Result = NewObject<UObject>(GetTransientPackage(), Class, NAME_None);
		if (Result)
		{
			OwnedObjects.Push(Result);
		}
		return Result;
	}
	return nullptr;
}

static UObject* NewTestActor(UClass* ActorClass, FOwnedObjectsHelper &OwnedObjects)
{
	if (ActorClass)
	{
		AActor* Actor = GWorld->SpawnActor(ActorClass);
#if WITH_EDITORONLY_DATA
		if (Actor)
		{
			OwnedObjects.Push(Actor);
			Actor->GetRootComponent()->bVisualizeComponent = true;
		}
#endif
		return Actor;
	}
	return nullptr;
}

static void Call(UObject* Target, const TCHAR* FunctionName, void* Args = nullptr)
{
	if (!Target)
	{
		return;
	}
	UFunction* Fn = Target->FindFunction(FunctionName);
	if (Fn)
	{
		Target->ProcessEvent(Fn, Args);
	}
}

// Remove EAutomationTestFlags::Disabled to enable these tests, note that these will need to be moved into the ClientContext because we can only test cooked content:
static const uint32 CompilerTestFlags = EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter;

// Tests:
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBPCompilerArrayTest, "Project.Blueprints.NativeBackend.ArrayTest", CompilerTestFlags)
bool FBPCompilerArrayTest::RunTest(const FString& Parameters)
{
	auto TestBody = [](ClassAccessor F, FAutomationTestBase* Context)
	{
		FOwnedObjectsHelper OwnedObjects;

		TArray<FString> Input;
		Input.Push(TEXT("addedString"));

		UObject* TestInstance = NewTestObject(F(TEXT("Array"), TEXT("BP_Array_Basic"), Context, OwnedObjects), OwnedObjects);
		if (!TestInstance)
		{
			return 0u;
		}

		Call(TestInstance, TEXT("RunArrayTest"), &Input);

		FArchiveSkipTransientObjectCRC32 Results;
		return Results.Crc32(TestInstance);
	};

	return RunTestHelper(TestBody, this);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBPCompilerCDOTest, "Project.Blueprints.NativeBackend.CDOTest", CompilerTestFlags)
bool FBPCompilerCDOTest::RunTest(const FString& Parameters)
{
	FOwnedObjectsHelper OwnedObjects;
	UObject* GeneratedTestInstance = NewTestObject(GetGeneratedClass(TEXT("CDO"), TEXT("BP_CDO_Basic"), this, OwnedObjects), OwnedObjects);
	UObject* NativeTestInstance = NewTestObject(GetNativeClass(TEXT("CDO"), TEXT("BP_CDO_Basic"), this, OwnedObjects), OwnedObjects);
	
	if (!GeneratedTestInstance || !NativeTestInstance)
	{
		AddError(TEXT("Test failed to run!"));
		return true;
	}

	for (UProperty* NativeProperty : TFieldRange<UProperty>(NativeTestInstance->GetClass()))
	{
		if ((NativeProperty->GetOwnerClass() == UObject::StaticClass()) || (FArchiveSkipTransientObjectCRC32::CanPropertyBeDifferentInConvertedCDO(NativeProperty)))
		{
			continue;
		}
		UProperty* BPProperty = FindField<UProperty>(GeneratedTestInstance->GetClass(), *NativeProperty->GetName());
		if (!BPProperty)
		{
			AddError(*FString::Printf(TEXT("Cannot find property %s in BPGC"), *NativeProperty->GetName()));
			return true;
		}

		uint8* NativeValue = NativeProperty->ContainerPtrToValuePtr<uint8>(NativeTestInstance->GetClass()->GetDefaultObject());
		uint8* BPGCValue = BPProperty->ContainerPtrToValuePtr<uint8>(GeneratedTestInstance->GetClass()->GetDefaultObject());
		if (!NativeProperty->Identical(NativeValue, BPGCValue))
		{
			AddError(*FString::Printf(TEXT("Different value of property %s"), *NativeProperty->GetName()));
			return true;
		}
	}

	return true;

}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBPCompilerCommunicationTest, "Project.Blueprints.NativeBackend.CommunicationTest", CompilerTestFlags)
bool FBPCompilerCommunicationTest::RunTest(const FString& Parameters)
{
	auto TestBody = [](ClassAccessor F, FAutomationTestBase* Context)
	{
		FOwnedObjectsHelper OwnedObjects;

		UObject* A = NewTestObject(F(TEXT("Communication"), TEXT("BP_Comm_Test_A"), Context, OwnedObjects), OwnedObjects);
		UObject* B = NewTestObject(F(TEXT("Communication"), TEXT("BP_Comm_Test_B"), Context, OwnedObjects), OwnedObjects);
		if (!A || !B)
		{
			return 0u;
		}

		Call(A, TEXT("Flop"));
		Call(B, TEXT("Flip"));

		FArchiveSkipTransientObjectCRC32 Results;
		return Results.Crc32(B, Results.Crc32(A));
	};

	return RunTestHelper(TestBody, this);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBPCompilerConstructionScriptTest, "Project.Blueprints.NativeBackend.ConstructionScriptTest", CompilerTestFlags)
bool FBPCompilerConstructionScriptTest::RunTest(const FString& Parameters)
{
	auto TestBody = [](ClassAccessor F, FAutomationTestBase* Context)
	{
		FOwnedObjectsHelper OwnedObjects;

		UObject* TestInstance = NewTestActor(F(TEXT("ConstructionScript"), TEXT("BP_ConstructionScript_Test"), Context, OwnedObjects), OwnedObjects);
		if (!TestInstance)
		{
			return 0u;
		}

		FArchiveSkipTransientObjectCRC32 Results;
		return Results.Crc32(TestInstance);
	};

	return RunTestHelper(TestBody, this);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBPCompilerControlFlowTest, "Project.Blueprints.NativeBackend.ControlFlow", CompilerTestFlags)
bool FBPCompilerControlFlowTest::RunTest(const FString& Parameters)
{
	auto TestBody = [](ClassAccessor F, FAutomationTestBase* Context)
	{
		FOwnedObjectsHelper OwnedObjects;

		UObject* TestInstance = NewTestObject(F(TEXT("ControlFlow"), TEXT("BP_ControlFlow_Basic"), Context, OwnedObjects), OwnedObjects);
		if (!TestInstance)
		{
			return 0u;
		}

		Call(TestInstance, TEXT("RunControlFlowTest"));

		FArchiveSkipTransientObjectCRC32 Results;
		return Results.Crc32(TestInstance);
	};

	return RunTestHelper(TestBody, this);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBPCompilerEnumTest, "Project.Blueprints.NativeBackend.EnumTest", CompilerTestFlags)
bool FBPCompilerEnumTest::RunTest(const FString& Parameters)
{
	auto TestBody = [](ClassAccessor F, FAutomationTestBase* Context)
	{
		FOwnedObjectsHelper OwnedObjects;

		UObject* TestInstance = NewTestObject(F(TEXT("Enum"), TEXT("BP_Enum_Reader_Writer"), Context, OwnedObjects), OwnedObjects);
		if (!TestInstance)
		{
			return 0u;
		}

		Call(TestInstance, TEXT("UpdateEnum"));

		FArchiveSkipTransientObjectCRC32 Results;
		return Results.Crc32(TestInstance);
	};

	return RunTestHelper(TestBody, this);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBPCompilerEventTest, "Project.Blueprints.NativeBackend.Event", CompilerTestFlags)
bool FBPCompilerEventTest::RunTest(const FString& Parameters)
{
	auto TestBody = [](ClassAccessor F, FAutomationTestBase* Context)
	{
		FOwnedObjectsHelper OwnedObjects;

		UObject* TestInstance = NewTestObject(F(TEXT("Event"), TEXT("BP_Event_Basic"), Context, OwnedObjects), OwnedObjects);
		if (!TestInstance)
		{
			return 0u;
		}

		Call(TestInstance, TEXT("BeginEventChain"));

		FArchiveSkipTransientObjectCRC32 Results;
		return Results.Crc32(TestInstance);
	};

	return RunTestHelper(TestBody, this);
}

struct FInheritenceTestParams
{
	bool bFlag;
	TArray<FString> Strings;
	TArray<int32> Result;
};

inline bool operator==(const FInheritenceTestParams& LHS, const FInheritenceTestParams& RHS)
{
	return
		LHS.bFlag == RHS.bFlag &&
		LHS.Strings == RHS.Strings &&
		LHS.Result == RHS.Result;
}

inline bool operator!=(const FInheritenceTestParams& LHS, const FInheritenceTestParams& RHS)
{
	return !(LHS == RHS);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBPCompilerInheritenceTest, "Project.Blueprints.NativeBackend.Inheritence", CompilerTestFlags)
bool FBPCompilerInheritenceTest::RunTest(const FString& Parameters)
{
	auto TestBody = [](ClassAccessor F, FAutomationTestBase* Context)
	{
		FOwnedObjectsHelper OwnedObjects;

		UObject* TestInstance = NewTestObject(F(TEXT("Inheritence"), TEXT("BP_Child_Basic"), Context, OwnedObjects), OwnedObjects);
		if (!TestInstance)
		{
			return 0u;
		}

		FInheritenceTestParams Params;
		Params.bFlag = true;
		Call(TestInstance, TEXT("VirtualFunction"), &Params);

		FArchiveSkipTransientObjectCRC32 Results;
		return Results.Crc32(TestInstance);
	};

	return RunTestHelper(TestBody, this);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBPCompilerStructureTest, "Project.Blueprints.NativeBackend.Structure", CompilerTestFlags)
bool FBPCompilerStructureTest::RunTest(const FString& Parameters)
{
	auto TestBody = [](ClassAccessor F, FAutomationTestBase* Context)
	{
		FOwnedObjectsHelper OwnedObjects;

		UObject* TestInstance = NewTestObject(F(TEXT("Structure"), TEXT("BP_Structure_Driver"), Context, OwnedObjects), OwnedObjects);
		if (!TestInstance)
		{
			return 0u;
		}

		Call(TestInstance, TEXT("RunStructTest"));

		FArchiveSkipTransientObjectCRC32 Results;
		return Results.Crc32(TestInstance);
	};

	return RunTestHelper(TestBody, this);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBPCompilerNodeTest, "Project.Blueprints.NativeBackend.Node", CompilerTestFlags)
bool FBPCompilerNodeTest::RunTest(const FString& Parameters)
{
	auto TestBody = [](ClassAccessor F, FAutomationTestBase* Context)
	{
		FOwnedObjectsHelper OwnedObjects;

		UObject* TestInstance = NewTestObject(F(TEXT("Node"), TEXT("BP_Node_Basic"), Context, OwnedObjects), OwnedObjects);
		if (!TestInstance)
		{
			return 0u;
		}

		Call(TestInstance, TEXT("RunNodes"));

		FArchiveSkipTransientObjectCRC32 Results;
		return Results.Crc32(TestInstance);
	};

	return RunTestHelper(TestBody, this);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBPCompilerLatentTest, "Project.Blueprints.NativeBackend.Latent", CompilerTestFlags)
bool FBPCompilerLatentTest::RunTest(const FString& Parameters)
{
	auto TestBody = [](ClassAccessor F, FAutomationTestBase* Context)
	{
		FOwnedObjectsHelper OwnedObjects;
		TGuardValue<bool> AutoRestore(GAllowActorScriptExecutionInEditor, true);

		UObject* TestInstance = NewTestActor(F(TEXT("Node"), TEXT("BP_Latent_Basic"), Context, OwnedObjects), OwnedObjects);
		if (!TestInstance)
		{
			return 0u;
		}

		Call(TestInstance, TEXT("RunDelayTest"));
		Call(TestInstance, TEXT("RunDownloadTest"));

		FArchiveSkipTransientObjectCRC32 Results;
		return Results.Crc32(TestInstance);
	};

	return RunTestHelper(TestBody, this);
}
