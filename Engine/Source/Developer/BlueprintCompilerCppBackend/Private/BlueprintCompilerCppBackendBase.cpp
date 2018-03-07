// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BlueprintCompilerCppBackendBase.h"
#include "UObject/UnrealType.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Misc/Paths.h"
#include "UObject/Interface.h"
#include "Engine/Blueprint.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Engine/UserDefinedEnum.h"
#include "Engine/UserDefinedStruct.h"
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CreateDelegate.h"
#include "SourceCodeNavigation.h"
#include "IBlueprintCompilerCppBackendModule.h"
#include "BlueprintCompilerCppBackendGatherDependencies.h"
#include "BlueprintCompilerCppBackendUtils.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Animation/AnimNodeBase.h"

TArray<class UFunction*> IBlueprintCompilerCppBackendModule::CollectBoundFunctions(class UBlueprint* BP)
{
	// it would be clean to recover info about delegates from bytecode, but then the owner class is unknown, that's why we check the nodes.
	TArray<class UFunction*> Result;
	check(BP);
	TArray<UEdGraph*> Graphs;
	BP->GetAllGraphs(Graphs);
	for (UEdGraph* Graph : Graphs)
	{
		if (!Graph)
		{
			continue;
		}
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			FName FunctionName = NAME_None;
			UClass* FunctionOwnerClass = nullptr;
			if (UK2Node_CreateDelegate* CreateDelegate = Cast<UK2Node_CreateDelegate>(Node))
			{
				FunctionName = CreateDelegate->GetFunctionName();
				FunctionOwnerClass = CreateDelegate->GetScopeClass(true);
			}
			else if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
			{
				UEdGraphPin* DelegateOutPin = EventNode->FindPin(UK2Node_Event::DelegateOutputName);
				if (DelegateOutPin && DelegateOutPin->LinkedTo.Num())
				{
					FunctionOwnerClass = BP->GeneratedClass;
					FunctionName = EventNode->GetFunctionName();
				}
			}

			FunctionOwnerClass = FunctionOwnerClass ? FunctionOwnerClass->GetAuthoritativeClass() : nullptr;
			UFunction* Func = FunctionOwnerClass ? FunctionOwnerClass->FindFunctionByName(FunctionName) : nullptr;
			Func = Func ? FEmitHelper::GetOriginalFunction(Func) : nullptr;
			if (Func)
			{
				Result.Add(Func);
			}
		}
	}

	return Result;
}

void FBlueprintCompilerCppBackendBase::EmitStructProperties(FEmitterLocalContext& EmitterContext, UStruct* SourceClass)
{
	// Emit class variables
	for (TFieldIterator<UProperty> It(SourceClass, EFieldIteratorFlags::ExcludeSuper); It; ++It)
	{
		UProperty* Property = *It;
		check(Property);
		FString PropertyMacro(TEXT("UPROPERTY("));
		{
			TArray<FString> Tags = FEmitHelper::ProperyFlagsToTags(Property->PropertyFlags, nullptr != Cast<UClass>(SourceClass));
			Tags.Emplace(FEmitHelper::HandleRepNotifyFunc(Property));
			Tags.Emplace(FEmitHelper::HandleMetaData(Property, false));
			Tags.Remove(FString());

			FString AllTags;
			FEmitHelper::ArrayToString(Tags, AllTags, TEXT(", "));
			PropertyMacro += AllTags;
		}
		PropertyMacro += TEXT(")");
		EmitterContext.Header.AddLine(PropertyMacro);

		const FString CppDeclaration = EmitterContext.ExportCppDeclaration(Property, EExportedDeclaration::Member, EPropertyExportCPPFlags::CPPF_CustomTypeName | EPropertyExportCPPFlags::CPPF_BlueprintCppBackend | EPropertyExportCPPFlags::CPPF_NoConst);
		EmitterContext.Header.AddLine(CppDeclaration + TEXT(";"));
	}
}

void FBlueprintCompilerCppBackendBase::DeclareDelegates(FEmitterLocalContext& EmitterContext, TIndirectArray<FKismetFunctionContext>& Functions)
{
	// MC DELEGATE DECLARATION
	{
		FEmitHelper::EmitMulticastDelegateDeclarations(EmitterContext);
	}

	// GATHER ALL SC DELEGATES
	{
		TArray<UDelegateProperty*> Delegates;
		for (TFieldIterator<UDelegateProperty> It(EmitterContext.GetCurrentlyGeneratedClass(), EFieldIteratorFlags::ExcludeSuper); It; ++It)
		{
			Delegates.Add(*It);
		}

		for (auto& FuncContext : Functions)
		{
			for (TFieldIterator<UDelegateProperty> It(FuncContext.Function, EFieldIteratorFlags::ExcludeSuper); It; ++It)
			{
				Delegates.Add(*It);
			}
		}

		TArray<UFunction*> SCDelegateSignaturesWithoutType;
		// Don't declare signatures, that are already declared in a native class
		for (int32 I = 0; I < Delegates.Num();)
		{
			auto Delegate = Delegates[I];
			auto DelegateSignature = Delegate ? Delegate->SignatureFunction : nullptr;
			auto DelegateSignatureOwner = DelegateSignature ? DelegateSignature->GetOwnerStruct() : nullptr;
			if (DelegateSignatureOwner && DelegateSignatureOwner->HasAnyInternalFlags(EInternalObjectFlags::Native))
			{
				if (DelegateSignature->HasAllFunctionFlags(FUNC_MulticastDelegate))
				{
					SCDelegateSignaturesWithoutType.AddUnique(DelegateSignature);
				}
				Delegates.RemoveAtSwap(I);
			}
			else
			{
				I++;
			}
		}

		// remove duplicates, n^2, but n is small:
		for (int32 I = 0; I < Delegates.Num(); ++I)
		{
			UFunction* TargetFn = Delegates[I]->SignatureFunction;
			for (int32 J = I + 1; J < Delegates.Num();)
			{
				if (TargetFn == Delegates[J]->SignatureFunction)
				{
					// swap erase:
					Delegates[J] = Delegates[Delegates.Num() - 1];
					Delegates.RemoveAt(Delegates.Num() - 1);
				}
				else
				{
					J++;
				}
			}
		}
		
		int32 UniqeSCDelegateIndex = 0;
		for (UFunction* SCDelegateSignature : SCDelegateSignaturesWithoutType)
		{
			FString SCType = FString::Printf(TEXT("F__%s__SC_%d"), *FEmitHelper::GetCppName(SCDelegateSignature), UniqeSCDelegateIndex);
			UniqeSCDelegateIndex++;
			FEmitHelper::EmitSinglecastDelegateDeclarations_Inner(EmitterContext, SCDelegateSignature, SCType);
			EmitterContext.MCDelegateSignatureToSCDelegateType.Add(SCDelegateSignature, SCType);
		}

		FEmitHelper::EmitSinglecastDelegateDeclarations(EmitterContext, Delegates);
	}
}

struct FIncludeHeaderHelper
{
	static void EmitIncludeHeader(FCodeText& Dst, const TCHAR* Message, bool bAddDotH)
	{
		Dst.AddLine(FString::Printf(TEXT("#include \"%s%s\""), Message, bAddDotH ? TEXT(".h") : TEXT("")));
	}

	static void EmitInner(FCodeText& Dst, const TSet<UField*>& Src, const TSet<UField*>& Declarations, const FCompilerNativizationOptions& NativizationOptions, TSet<FString>& AlreadyIncluded)
	{
		auto EngineSourceDir = FPaths::EngineSourceDir();
		auto GameSourceDir = FPaths::GameSourceDir();

		for (UField* Field : Src)
		{
			if (!Field)
			{
				continue;
			}
			const bool bWantedType = Field->IsA<UBlueprintGeneratedClass>() || Field->IsA<UUserDefinedEnum>() || Field->IsA<UUserDefinedStruct>();

			// Wanted no-native type, that will be converted
			if (bWantedType)
			{
				// @TODO: Need to query if this asset will actually be converted

				const FString Name = Field->GetPathName();
				bool bAlreadyIncluded = false;
				AlreadyIncluded.Add(Name, &bAlreadyIncluded);
				if (!bAlreadyIncluded)
				{
					const FString GeneratedFilename = FEmitHelper::GetBaseFilename(Field, NativizationOptions);

					// In some cases the caller may have already primed this array with the generated filename.
					if (!AlreadyIncluded.Contains(GeneratedFilename))
					{
						FIncludeHeaderHelper::EmitIncludeHeader(Dst, *GeneratedFilename, true);
					}
				}
			}
			// headers for native items
			else
			{
				FString PackPath;
				if (FSourceCodeNavigation::FindClassHeaderPath(Field, PackPath))
				{
					if (!PackPath.RemoveFromStart(EngineSourceDir))
					{
						if (!PackPath.RemoveFromStart(GameSourceDir))
						{
							PackPath = FPaths::GetCleanFilename(PackPath);
						}
					}
					bool bAlreadyIncluded = false;
					AlreadyIncluded.Add(PackPath, &bAlreadyIncluded);
					if (!bAlreadyIncluded)
					{
						FIncludeHeaderHelper::EmitIncludeHeader(Dst, *PackPath, false);
					}
				}
			}
		}

		for (auto Type : Declarations)
		{
			if (auto ForwardDeclaredType = Cast<UClass>(Type))
			{
				Dst.AddLine(FString::Printf(TEXT("class %s;"), *FEmitHelper::GetCppName(ForwardDeclaredType)));
			}
		}
	}
};

/*
 *	This struct adds included headers of necessary wrappers (for unconverted BPs). The list of needed wrappers is filled while generating the code. See FEmitterLocalContext::MarkUnconvertedClassAsNecessary.
 */
struct FIncludedUnconvertedWrappers
{
	FEmitterLocalContext& Context;
	bool bInludeInBody;

	static const TCHAR* Placeholder()
	{
		return TEXT("//PlaceholderForUnconvertedWrappersInlude");
	}

	static void AddPlaceholder(FEmitterLocalContext& InContext, bool bInInludeInBody)
	{
		(bInInludeInBody ? InContext.Body : InContext.Header).AddLine(Placeholder());
	}

	static void FillPlaceholder(FEmitterLocalContext& InContext, bool bInInludeInBody)
	{
		FCodeText AdditionalIncludes;
		TSet<FString> DummyStrSet;
		FIncludeHeaderHelper::EmitInner(AdditionalIncludes, InContext.UsedUnconvertedWrapper, TSet<UField*>{}, InContext.NativizationOptions, DummyStrSet);
		(bInInludeInBody ? InContext.Body : InContext.Header).Result.ReplaceInline(Placeholder(), *AdditionalIncludes.Result);
	}

	FIncludedUnconvertedWrappers(FEmitterLocalContext& InContext, bool bInInludeInBody)
		: Context(InContext)
		, bInludeInBody(bInInludeInBody)
	{
		AddPlaceholder(Context, bInludeInBody);
	}

	~FIncludedUnconvertedWrappers()
	{
		FillPlaceholder(Context, bInludeInBody);
	}
};

FString FBlueprintCompilerCppBackendBase::GenerateCodeFromClass(UClass* SourceClass, TIndirectArray<FKismetFunctionContext>& Functions, bool bGenerateStubsOnly, const FCompilerNativizationOptions& NativizationOptions, FString& OutCppBody)
{
	CleanBackend();
	for (auto& FunctionContext : Functions)
	{
		if (FunctionContext.bIsUbergraph)
		{
			UberGraphContext = &FunctionContext;
			for (int32 ExecutionGroupIndex = 0; ExecutionGroupIndex < FunctionContext.UnsortedSeparateExecutionGroups.Num(); ++ExecutionGroupIndex)
			{
				TSet<UEdGraphNode*>& ExecutionGroup = FunctionContext.UnsortedSeparateExecutionGroups[ExecutionGroupIndex];
				for (UEdGraphNode* LocNode : ExecutionGroup)
				{
					TArray<FBlueprintCompiledStatement*>* LocStatementsPtr = FunctionContext.StatementsPerNode.Find(LocNode);
					if (ensure(LocStatementsPtr))
					{
						for (FBlueprintCompiledStatement* LocStatement : *LocStatementsPtr)
						{
							UberGraphStatementToExecutionGroup.Add(LocStatement, ExecutionGroupIndex);
						}
					}
				}
			}
			break;
		}
	}

	// use GetBaseFilename() so that we can coordinate #includes and filenames
	auto CleanCppClassName = FEmitHelper::GetBaseFilename(SourceClass, NativizationOptions);
	auto CppClassName = FEmitHelper::GetCppName(SourceClass);
	
	FGatherConvertedClassDependencies Dependencies(SourceClass, NativizationOptions);
	FNativizationSummaryHelper::RegisterRequiredModules(NativizationOptions.PlatformName, Dependencies.RequiredModuleNames);
	FEmitterLocalContext EmitterContext(Dependencies, NativizationOptions);

	UClass* OriginalSourceClass = Dependencies.FindOriginalClass(SourceClass);
	ensure(OriginalSourceClass != SourceClass);

	FNativizationSummaryHelper::RegisterClass(OriginalSourceClass);

	EmitFileBeginning(CleanCppClassName, EmitterContext);

	const TCHAR* PlaceholderForInlinedStructInlude = TEXT("//PlaceholderForInlinedStructInlude");
	const bool bIsInterface = SourceClass->IsChildOf<UInterface>();
	if (!bIsInterface)
	{
		EmitterContext.Body.AddLine(PlaceholderForInlinedStructInlude);
	}

	const bool bHasStaticSearchableValues = FBackendHelperStaticSearchableValues::HasSearchableValues(SourceClass);

	{
		FIncludedUnconvertedWrappers IncludedUnconvertedWrappers(EmitterContext, true);

		// C4883 is a strange error (for big functions), introduced in VS2015 update 2
		FDisableUnwantedWarningOnScope DisableUnwantedWarningOnScope(EmitterContext.Body);

		// Class declaration
		if (bIsInterface)
		{
			EmitterContext.Header.AddLine(FString::Printf(TEXT("UINTERFACE(Blueprintable, %s)"), *FEmitHelper::ReplaceConvertedMetaData(OriginalSourceClass)));
			EmitterContext.Header.AddLine(FString::Printf(TEXT("class %s : public UInterface"), *FEmitHelper::GetCppName(SourceClass, true)));
			EmitterContext.Header.AddLine(TEXT("{"));
			EmitterContext.Header.IncreaseIndent();
			EmitterContext.Header.AddLine(TEXT("GENERATED_BODY()"));
			EmitterContext.Header.DecreaseIndent();
			EmitterContext.Header.AddLine(TEXT("};"));
			EmitterContext.Header.AddLine(FString::Printf(TEXT("class %s"), *CppClassName));
		}
		else
		{
			TArray<FString> AdditionalMD;
			const FString ReplaceConvertedMD = FEmitHelper::GenerateReplaceConvertedMD(OriginalSourceClass);
			if (!ReplaceConvertedMD.IsEmpty())
			{
				AdditionalMD.Add(ReplaceConvertedMD);
			}

			if (bHasStaticSearchableValues)
			{
				AdditionalMD.Add(FBackendHelperStaticSearchableValues::GenerateClassMetaData(SourceClass));
			}

			// AdditionalMD.Add(FString::Printf(TEXT("CustomDynamicClassInitialization=\"%s::__CustomDynamicClassInitialization\""), *CppClassName));
			const FString DefinedConfigName = (OriginalSourceClass->ClassConfigName == NAME_None) ? FString() : FString::Printf(TEXT("config=%s, "), *OriginalSourceClass->ClassConfigName.ToString());
			EmitterContext.Header.AddLine(FString::Printf(TEXT("UCLASS(%s%s%s)")
				, *DefinedConfigName
				, (!SourceClass->IsChildOf<UBlueprintFunctionLibrary>()) ? TEXT("Blueprintable, BlueprintType, ") : TEXT("")
				, *FEmitHelper::HandleMetaData(nullptr, false, &AdditionalMD)));

			UClass* SuperClass = SourceClass->GetSuperClass();
			FString ClassDefinition = FString::Printf(TEXT("class %s : public %s"), *CppClassName, *FEmitHelper::GetCppName(SuperClass));

			for (auto& ImplementedInterface : SourceClass->Interfaces)
			{
				if (ImplementedInterface.Class)
				{
					ClassDefinition += FString::Printf(TEXT(", public %s"), *FEmitHelper::GetCppName(ImplementedInterface.Class));
				}
			}
			EmitterContext.Header.AddLine(ClassDefinition);
		}

		// Begin scope
		EmitterContext.Header.AddLine(TEXT("{"));
		EmitterContext.Header.AddLine(TEXT("public:"));
		EmitterContext.Header.IncreaseIndent();
		EmitterContext.Header.AddLine(TEXT("GENERATED_BODY()"));

		DeclareDelegates(EmitterContext, Functions);

		EmitStructProperties(EmitterContext, SourceClass);

		{
			IBlueprintCompilerCppBackendModule& BackEndModule = (IBlueprintCompilerCppBackendModule&)IBlueprintCompilerCppBackendModule::Get();
			TSharedPtr<FNativizationSummary> NativizationSummary = BackEndModule.NativizationSummary();
			if (NativizationSummary.IsValid())
			{
				for (TFieldIterator<UProperty> It(SourceClass, EFieldIteratorFlags::ExcludeSuper); It; ++It)
				{
					UProperty* Property = *It;
					if (Property && Property->HasAllPropertyFlags(CPF_Transient | CPF_DuplicateTransient))
					{
						NativizationSummary->MemberVariablesFromGraph++;
					}
				}
			}
		}

		TSharedPtr<FGatherConvertedClassDependencies> ParentDependencies;
		// Emit function declarations and definitions (writes to header and body simultaneously)
		if (!bIsInterface)
		{
			UBlueprintGeneratedClass* BPGC = CastChecked<UBlueprintGeneratedClass>(EmitterContext.GetCurrentlyGeneratedClass());
			UBlueprintGeneratedClass* ParentBPGC = Cast<UBlueprintGeneratedClass>(BPGC->GetSuperClass());
			ParentDependencies = TSharedPtr<FGatherConvertedClassDependencies>( ParentBPGC 
				? new FGatherConvertedClassDependencies(ParentBPGC, NativizationOptions) 
				: nullptr);

			EmitterContext.Header.AddLine(FString::Printf(TEXT("%s(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());"), *CppClassName));
			EmitterContext.Header.AddLine(TEXT("virtual void PostLoadSubobjects(FObjectInstancingGraph* OuterInstanceGraph) override;"));
			EmitterContext.Header.AddLine(TEXT("static void __CustomDynamicClassInitialization(UDynamicClass* InDynamicClass);"));

			EmitterContext.Header.AddLine(TEXT("static void __StaticDependenciesAssets(TArray<FBlueprintDependencyData>& AssetsToLoad);"));
			EmitterContext.Header.AddLine(TEXT("static void __StaticDependencies_DirectlyUsedAssets(TArray<FBlueprintDependencyData>& AssetsToLoad);"));
			if (bHasStaticSearchableValues)
			{
				FBackendHelperStaticSearchableValues::EmitFunctionDeclaration(EmitterContext);
				FBackendHelperStaticSearchableValues::EmitFunctionDefinition(EmitterContext);
			}
			FEmitDefaultValueHelper::GenerateConstructor(EmitterContext);
			FEmitDefaultValueHelper::GenerateCustomDynamicClassInitialization(EmitterContext, ParentDependencies);
		}

		// Create the state map
		for (int32 i = 0; i < Functions.Num(); ++i)
		{
			StateMapPerFunction.Add(FFunctionLabelInfo());
			FunctionIndexMap.Add(&Functions[i], i);
		}

		for (int32 i = 0; i < Functions.Num(); ++i)
		{
			if (Functions[i].IsValid())
			{
				ConstructFunction(Functions[i], EmitterContext, bGenerateStubsOnly);
			}
		}

		EmitterContext.Header.DecreaseIndent();
		EmitterContext.Header.AddLine(TEXT("public:"));
		EmitterContext.Header.IncreaseIndent();

		FBackendHelperUMG::WidgetFunctionsInHeader(EmitterContext);

		EmitterContext.Header.DecreaseIndent();
		EmitterContext.Header.AddLine(TEXT("};"));

		if (!bIsInterface)
		{
			// must be called after GenerateConstructor and GenerateCustomDynamicClassInitialization and other functions implementation
			// now we knows which assets are directly used in source code
			FEmitDefaultValueHelper::AddStaticFunctionsForDependencies(EmitterContext, ParentDependencies, NativizationOptions);
			FEmitDefaultValueHelper::AddRegisterHelper(EmitterContext);
		}

		FEmitHelper::EmitLifetimeReplicatedPropsImpl(EmitterContext);
	}

	if (!bIsInterface)
	{
		FCodeText AdditionalIncludes;
		TSet<FString> DummyStrSet;
		FIncludeHeaderHelper::EmitInner(AdditionalIncludes, EmitterContext.StructsUsedAsInlineValues, TSet<UField*>{}, EmitterContext.NativizationOptions, DummyStrSet);
		EmitterContext.Body.Result.ReplaceInline(PlaceholderForInlinedStructInlude, *AdditionalIncludes.Result);
	}

	CleanBackend();

	OutCppBody = EmitterContext.Body.Result;
	return EmitterContext.Header.Result;
}

static void PropertiesUsedByStatement(FBlueprintCompiledStatement* Statement, TSet<UProperty*>& Properties)
{
	if (Statement)
	{
		for (FBPTerminal* Terminal : Statement->RHS)
		{
			if (Terminal)
			{
				Properties.Add(Terminal->AssociatedVarProperty);
				PropertiesUsedByStatement(Terminal->InlineGeneratedParameter, Properties);
			}
		}

		if (Statement->FunctionContext)
		{
			Properties.Add(Statement->FunctionContext->AssociatedVarProperty);
			PropertiesUsedByStatement(Statement->FunctionContext->InlineGeneratedParameter, Properties);
		}

		if (Statement->LHS)
		{
			Properties.Add(Statement->LHS->AssociatedVarProperty);
			PropertiesUsedByStatement(Statement->LHS->InlineGeneratedParameter, Properties);
		}
	}
}

/** Emits local variable declarations for a function */
static void DeclareLocalVariables(FEmitterLocalContext& EmitterContext, TArray<UProperty*>& LocalVariables, FKismetFunctionContext& FunctionContext, int32 ExecutionGroup)
{
	const bool bUseExecutionGroup = ExecutionGroup >= 0;
	TSet<UProperty*> PropertiesUsedByCurrentExecutionGroup;
	if (bUseExecutionGroup)
	{
		for (UEdGraphNode* Node : FunctionContext.UnsortedSeparateExecutionGroups[ExecutionGroup])
		{
			TArray<FBlueprintCompiledStatement*>* StatementList = FunctionContext.StatementsPerNode.Find(Node);
			if (StatementList)
			{
				for (FBlueprintCompiledStatement* Statement : *StatementList)
				{
					PropertiesUsedByStatement(Statement, PropertiesUsedByCurrentExecutionGroup);
				}
			}
		}
	}

	for (int32 i = 0; i < LocalVariables.Num(); ++i)
	{
		UProperty* LocalVariable = LocalVariables[i];
		if (!bUseExecutionGroup || PropertiesUsedByCurrentExecutionGroup.Contains(LocalVariable))
		{
			const FString CppDeclaration = EmitterContext.ExportCppDeclaration(LocalVariable, EExportedDeclaration::Local, EPropertyExportCPPFlags::CPPF_CustomTypeName | EPropertyExportCPPFlags::CPPF_BlueprintCppBackend | EPropertyExportCPPFlags::CPPF_NoConst);
			UStructProperty* StructProperty = Cast<UStructProperty>(LocalVariable);
			const TCHAR* EmptyDefaultConstructor = FEmitHelper::EmptyDefaultConstructor(StructProperty ? StructProperty->Struct : nullptr);
			EmitterContext.AddLine(CppDeclaration + EmptyDefaultConstructor + TEXT(";"));
		}
	}
}

void FBlueprintCompilerCppBackendBase::ConstructFunction(FKismetFunctionContext& FunctionContext, FEmitterLocalContext& EmitterContext, bool bGenerateStubOnly)
{
	if (FunctionContext.IsDelegateSignature())
	{
		return;
	}

	TArray<UProperty*> LocalVariables;
	TArray<UProperty*> ArgumentList;
	// Split the function property list into arguments, a return value (if any), and local variable declarations
	for (UProperty* Property : TFieldRange<UProperty>(FunctionContext.Function))
	{
		const bool bNeedLocalVariable = !Property->HasAnyPropertyFlags(CPF_Parm) || Property->HasAnyPropertyFlags(CPF_ReturnParm);
		TArray<UProperty*>& PropertyDest = bNeedLocalVariable ? LocalVariables : ArgumentList;
		PropertyDest.Add(Property);
	}

	static const FBoolConfigValueHelper UsePRAGMA_DISABLE_OPTIMIZATION(TEXT("BlueprintNativizationSettings"), TEXT("bUsePRAGMA_DISABLE_OPTIMIZATION"));
	if (FunctionContext.bIsUbergraph && UsePRAGMA_DISABLE_OPTIMIZATION)
	{
		EmitterContext.AddLine(TEXT("PRAGMA_DISABLE_OPTIMIZATION"));
	}

	TArray<FString> BodyFunctionsDeclaration = ConstructFunctionDeclaration(EmitterContext, FunctionContext, ArgumentList);
	ensure((BodyFunctionsDeclaration.Num() == FunctionContext.UnsortedSeparateExecutionGroups.Num())
		|| ((1 == BodyFunctionsDeclaration.Num()) && (0 == FunctionContext.UnsortedSeparateExecutionGroups.Num())));

	const bool bIsConstFunction = FunctionContext.Function->HasAllFunctionFlags(FUNC_Const);
	const bool bUseInnerFunctionImplementation = bIsConstFunction && !FunctionContext.Function->HasAnyFunctionFlags(FUNC_Static);
	if (bUseInnerFunctionImplementation)
	{
		ensure(0 == FunctionContext.UnsortedSeparateExecutionGroups.Num());
		ensure(1 == BodyFunctionsDeclaration.Num());
		const FString InnerImplementationFunctionName = FString::Printf(TEXT("%s_Inner_%d")
			, *FEmitHelper::GetCppName(FunctionContext.Function)
			, FEmitHelper::GetInheritenceLevel(FunctionContext.Function->GetOwnerStruct()));

		const FString ReturnType = GenerateReturnType(EmitterContext, FunctionContext.Function);
		const FString ArgList = GenerateArgList(EmitterContext, ArgumentList);
		const FString ArgListNoTypes = GenerateArgList(EmitterContext, ArgumentList, true);
		const FString ClassCppName = FEmitHelper::GetCppName(EmitterContext.GetCurrentlyGeneratedClass());

		// Inner header declaration
		EmitterContext.Header.AddLine(FString::Printf(TEXT("%s %s%s;")
			, *ReturnType, *InnerImplementationFunctionName, *ArgList));

		// Function original declaration
		EmitterContext.AddLine(BodyFunctionsDeclaration[0]);
		//Original implementation
		EmitterContext.AddLine(TEXT("{"));
		EmitterContext.IncreaseIndent();
		EmitterContext.AddLine(FString::Printf(TEXT("%sconst_cast<%s*>(this)->%s%s;")
			, FunctionContext.Function->GetReturnProperty() ? TEXT("return ") : TEXT("")
			, *ClassCppName
			, *InnerImplementationFunctionName
			, *ArgListNoTypes));
		EmitterContext.DecreaseIndent();
		EmitterContext.AddLine(TEXT("}"));

		// Inner body declaration
		EmitterContext.AddLine(FString::Printf(TEXT("%s %s::%s%s")
			, *ReturnType, *ClassCppName, *InnerImplementationFunctionName, *ArgList));

	}
	const bool bManyExecutionGroups = FunctionContext.UnsortedSeparateExecutionGroups.Num() > 0;
	for (int32 ExecutionGroupIndex = bManyExecutionGroups ? 0 : -1; ExecutionGroupIndex < FunctionContext.UnsortedSeparateExecutionGroups.Num(); ExecutionGroupIndex++)
	{
		if (!bUseInnerFunctionImplementation)
		{
			EmitterContext.AddLine(BodyFunctionsDeclaration[bManyExecutionGroups ? ExecutionGroupIndex : 0]);
		}
		// Start the body of the implementation
		EmitterContext.AddLine(TEXT("{"));
		EmitterContext.IncreaseIndent();
		if (!bGenerateStubOnly)
		{
			for (UProperty* Property : ArgumentList)
			{
				if (FEmitHelper::PropertyForConstCast(Property))
				{
					const uint32 ExportFlags = EPropertyExportCPPFlags::CPPF_CustomTypeName | EPropertyExportCPPFlags::CPPF_BlueprintCppBackend | EPropertyExportCPPFlags::CPPF_NoConst | EPropertyExportCPPFlags::CPPF_NoRef;
					const FString NoConstNoRefType = EmitterContext.ExportCppDeclaration(Property, EExportedDeclaration::Parameter, ExportFlags, FEmitterLocalContext::EPropertyNameInDeclaration::Skip);
					const FString TypeDefName = FString(TEXT("T")) + EmitterContext.GenerateUniqueLocalName();
					EmitterContext.AddLine(FString::Printf(TEXT("typedef %s %s;"), *NoConstNoRefType, *TypeDefName));

					const FString ParamName = FEmitHelper::GetCppName(Property);
					EmitterContext.AddLine(FString::Printf(TEXT("%s& %s = *const_cast<%s *>(&%s__const);"), *TypeDefName, *ParamName, *TypeDefName, *ParamName));
				}
			}
			const int32 ExecutionGroup = bManyExecutionGroups ? ExecutionGroupIndex : -1;
			DeclareLocalVariables(EmitterContext, LocalVariables, FunctionContext, ExecutionGroup);
			ConstructFunctionBody(EmitterContext, FunctionContext, ExecutionGroup);
		}

		if (UProperty* ReturnValue = FunctionContext.Function->GetReturnProperty())
		{
			EmitterContext.AddLine(FString::Printf(TEXT("return %s;"), *FEmitHelper::GetCppName(ReturnValue)));
		}
		
		EmitterContext.DecreaseIndent();
		EmitterContext.AddLine(TEXT("}"));
	}

	if (FunctionContext.bIsUbergraph && UsePRAGMA_DISABLE_OPTIMIZATION)
	{
		EmitterContext.AddLine(TEXT("PRAGMA_ENABLE_OPTIMIZATION"));
	}
}

TArray<FString> FBlueprintCompilerCppBackendBase::ConstructFunctionDeclaration(FEmitterLocalContext &EmitterContext, FKismetFunctionContext &FunctionContext, TArray<UProperty*> &ArgumentList)
{
	FString FunctionHeaderName = FEmitHelper::GetCppName(FunctionContext.Function);
	FString FunctionBodyName = FunctionHeaderName;
	const bool bStaticFunction = FunctionContext.Function->HasAllFunctionFlags(FUNC_Static);
	const bool bInInterface = FunctionContext.Function->GetOwnerClass()->IsChildOf<UInterface>();
	bool bAddConst = false;
	bool bIsOverride = false;
	bool bIsVirtual = !bInInterface && !bStaticFunction && !FunctionContext.IsEventGraph();

	FString MacroUFUNCTION;
	{
		UFunction* const Function = FunctionContext.Function;
		UFunction* const OriginalFunction = FEmitHelper::GetOriginalFunction(Function);
		TArray<FString> AdditionalMetaData;
		TArray<FString> AdditionalTags;
		bool bGenerateAsNativeEventImplementation = false;
		const bool bNetImplementation = !bInInterface && Function->HasAllFunctionFlags(FUNC_Net) && !Function->HasAnyFunctionFlags(FUNC_NetResponse);

		const UBlueprintGeneratedClass* const OriginalFuncOwnerAsBPGC = Cast<UBlueprintGeneratedClass>(OriginalFunction->GetOwnerClass());
		const bool bBPInterfaceImplementation = OriginalFuncOwnerAsBPGC && OriginalFuncOwnerAsBPGC->IsChildOf<UInterface>();

		if (bInInterface)
		{
			AdditionalTags.Emplace(TEXT("BlueprintImplementableEvent"));
		}
		else if (bNetImplementation)
		{
			FunctionBodyName = FunctionHeaderName + TEXT("_Implementation");
		}
		else if (FEmitHelper::ShouldHandleAsNativeEvent(Function))
		{
			bGenerateAsNativeEventImplementation = true;
			FunctionBodyName = FunctionHeaderName = FEmitHelper::GetCppName(OriginalFunction) + TEXT("_Implementation");
			bAddConst = OriginalFunction->HasAllFunctionFlags(FUNC_Const);
		}
		else if (FEmitHelper::ShouldHandleAsImplementableEvent(Function) || bBPInterfaceImplementation)
		{
			//The function "bpf__BIE__pf" should never be called directly. Only via function "BIE" with generated implementation.
			bIsVirtual = false;
			AdditionalMetaData.Emplace(TEXT("CppFromBpEvent"));
		}

		ensure(!bIsVirtual || Function->IsSignatureCompatibleWith(OriginalFunction));
		bIsOverride = bGenerateAsNativeEventImplementation || (bIsVirtual && (Function != OriginalFunction));

		auto PreliminaryConditionsToSkipMacroUFUNC = [](UFunction* InFunction) -> bool 
		{
			check(InFunction);
			return !FEmitHelper::ShouldHandleAsNativeEvent(InFunction)
				&& !FEmitHelper::ShouldHandleAsImplementableEvent(InFunction)
				&& !InFunction->GetOwnerClass()->IsChildOf<UInterface>()
				&& !InFunction->HasAnyFunctionFlags(FUNC_Exec | FUNC_Static | FUNC_Native
					| FUNC_Net | FUNC_NetServer | FUNC_NetClient | FUNC_NetMulticast | FUNC_NetReliable
					| FUNC_BlueprintAuthorityOnly | FUNC_BlueprintCosmetic | FUNC_NetValidate
					| FUNC_MulticastDelegate | FUNC_Delegate);
		};

		auto FunctionIsBoundToAnyDelegate = [](UFunction* InFunction)
		{
			check(InFunction);
			IBlueprintCompilerCppBackendModule& BackEndModule = (IBlueprintCompilerCppBackendModule&)IBlueprintCompilerCppBackendModule::Get();
			auto& IsFunctionUsedInADelegate = BackEndModule.GetIsFunctionUsedInADelegateCallback();
			return ensure(IsFunctionUsedInADelegate.IsBound()) ? IsFunctionUsedInADelegate.Execute(InFunction) : true;
		};

		auto IsFunctionUsedByReplication = [](UFunction* InFunction)
		{
			check(InFunction);
			for (UProperty* Prop : TFieldRange<UProperty>(InFunction->GetOwnerClass(), EFieldIteratorFlags::ExcludeSuper))
			{
				if (Prop && FEmitHelper::HasAllFlags(Prop->PropertyFlags, CPF_Net | CPF_RepNotify) && (Prop->RepNotifyFunc == InFunction->GetFName()))
				{
					return true;
				}
			}
			return false;
		};

		static const FBoolConfigValueHelper TryToSkipMacroUFUNC(TEXT("BlueprintNativizationSettings"), TEXT("bSkipUFUNCTION"));
		const bool bTryToSkipMacroUFUNC = TryToSkipMacroUFUNC; // should be enabled only when all BP are compiled.
		const bool bSkipMacro = bTryToSkipMacroUFUNC
			&& !FunctionContext.bIsUbergraph // uber-graph is necessary for latent actions
			&& PreliminaryConditionsToSkipMacroUFUNC(Function)
			&& ((Function == OriginalFunction) || PreliminaryConditionsToSkipMacroUFUNC(OriginalFunction))
			&& !IsFunctionUsedByReplication(Function)
			&& !FunctionIsBoundToAnyDelegate(OriginalFunction);

		if (bNetImplementation && bIsOverride)
		{
			FunctionHeaderName = FunctionBodyName;
		}
		else if (!bGenerateAsNativeEventImplementation && !bSkipMacro)
		{
			MacroUFUNCTION = FEmitHelper::EmitUFuntion(Function, AdditionalTags, AdditionalMetaData);
		}
	}

	const bool bManyExecutionGroups = FunctionContext.UnsortedSeparateExecutionGroups.Num() > 0;

	TArray<FString> Result;
	for (int32 ExecutionGroupIndex = bManyExecutionGroups ? 0 : -1; ExecutionGroupIndex < FunctionContext.UnsortedSeparateExecutionGroups.Num(); ExecutionGroupIndex++)
	{
		bool bNeedMacro = true;
		if (bManyExecutionGroups)
		{
			bNeedMacro = false;
			for (UEdGraphNode* NodeIt : FunctionContext.UnsortedSeparateExecutionGroups[ExecutionGroupIndex])
			{
				UK2Node_CallFunction* CallFuncNode = Cast<UK2Node_CallFunction>(NodeIt);
				if (CallFuncNode && CallFuncNode->IsLatentFunction())
				{
					bNeedMacro = true;
					break;
				}
			}
		}
		if (!MacroUFUNCTION.IsEmpty() && bNeedMacro)
		{
			const FString OldExecutionFunctionName = UEdGraphSchema_K2::FN_ExecuteUbergraphBase.ToString() + TEXT("_") + FunctionContext.Blueprint->GetName();
			const FString NewExecutionFunctionName = OldExecutionFunctionName + FString::Printf(TEXT("_%d"), ExecutionGroupIndex);
			const FString LocMacroUFUNCTION = bManyExecutionGroups 
				? MacroUFUNCTION.Replace(*OldExecutionFunctionName, *NewExecutionFunctionName)
				: MacroUFUNCTION;
			EmitterContext.Header.AddLine(LocMacroUFUNCTION);
		}

		const FString ReturnType = GenerateReturnType(EmitterContext, FunctionContext.Function);
		const FString ArgList = GenerateArgList(EmitterContext, ArgumentList);
		const FString FunctionNamePostfix = (-1 == ExecutionGroupIndex) ? FString() : FString::Printf(TEXT("_%d"), ExecutionGroupIndex);

		Result.Add(FString::Printf(TEXT("%s %s::%s%s%s%s")
			, *ReturnType
			, *FEmitHelper::GetCppName(EmitterContext.GetCurrentlyGeneratedClass())
			, *FunctionBodyName
			, *FunctionNamePostfix
			, *ArgList
			, bAddConst ? TEXT(" const") : TEXT("")));

		EmitterContext.Header.AddLine(FString::Printf(TEXT("%s%s%s %s%s%s%s%s;")
			, bStaticFunction ? TEXT("static ") : TEXT("")
			, bIsVirtual ? TEXT("virtual ") : TEXT("")
			, *ReturnType
			, *FunctionHeaderName
			, *FunctionNamePostfix
			, *ArgList
			, bAddConst ? TEXT(" const") : TEXT("")
			, bIsOverride ? TEXT(" override") : TEXT("")));
	}
	return Result;
}

FString FBlueprintCompilerCppBackendBase::GenerateArgList(const FEmitterLocalContext &EmitterContext, const TArray<UProperty*> &ArgumentList, bool bOnlyParamName)
{
	FString ArgListStr = TEXT("(");
	for (int32 i = 0; i < ArgumentList.Num(); ++i)
	{
		UProperty* ArgProperty = ArgumentList[i];

		if (i > 0)
		{
			ArgListStr += TEXT(", ");
		}

		const FString NamePostFix = FEmitHelper::PropertyForConstCast(ArgProperty) ? TEXT("__const") : TEXT("");
		if (bOnlyParamName)
		{
			ArgListStr += FEmitHelper::GetCppName(ArgProperty) + NamePostFix;
		}
		else
		{
			if (ArgProperty->HasAnyPropertyFlags(CPF_OutParm))
			{
				ArgListStr += TEXT("/*out*/ ");
			}
			ArgListStr += EmitterContext.ExportCppDeclaration(ArgProperty
				, EExportedDeclaration::Parameter
				, EPropertyExportCPPFlags::CPPF_CustomTypeName | EPropertyExportCPPFlags::CPPF_BlueprintCppBackend | EPropertyExportCPPFlags::CPPF_ArgumentOrReturnValue
				, FEmitterLocalContext::EPropertyNameInDeclaration::Regular
				, NamePostFix);
		}
	}
	ArgListStr += TEXT(")");
	return ArgListStr;
}

FString FBlueprintCompilerCppBackendBase::GenerateReturnType(const FEmitterLocalContext &EmitterContext, const UFunction* Function)
{
	UProperty* ReturnValue = Function->GetReturnProperty();
	if (ReturnValue)
	{
		const uint32 LocalExportCPPFlags = EPropertyExportCPPFlags::CPPF_CustomTypeName
			| EPropertyExportCPPFlags::CPPF_NoConst
			| EPropertyExportCPPFlags::CPPF_NoRef
			| EPropertyExportCPPFlags::CPPF_NoStaticArray
			| EPropertyExportCPPFlags::CPPF_BlueprintCppBackend
			| EPropertyExportCPPFlags::CPPF_ArgumentOrReturnValue;
		return EmitterContext.ExportCppDeclaration(ReturnValue, EExportedDeclaration::Parameter, LocalExportCPPFlags, FEmitterLocalContext::EPropertyNameInDeclaration::Skip);
	}
	return TEXT("void");
}

void FBlueprintCompilerCppBackendBase::ConstructFunctionBody(FEmitterLocalContext& EmitterContext, FKismetFunctionContext &FunctionContext, int32 ExecutionGroup)
{
	if (FunctionContext.UnsortedSeparateExecutionGroups.Num() && (ExecutionGroup < 0))
	{
		// use only for latent actions..
		return;
	}

	// Run thru code looking only at things marked as jump targets, to make sure the jump targets are ordered in order of appearance in the linear execution list
	// Emit code in the order specified by the linear execution list (the first node is always the entry point for the function)
	for (int32 NodeIndex = 0; NodeIndex < FunctionContext.LinearExecutionList.Num(); ++NodeIndex)
	{
		UEdGraphNode* StatementNode = FunctionContext.LinearExecutionList[NodeIndex];
		TArray<FBlueprintCompiledStatement*>* StatementList = FunctionContext.StatementsPerNode.Find(StatementNode);

		if (StatementList != NULL)
		{
			for (int32 StatementIndex = 0; StatementIndex < StatementList->Num(); ++StatementIndex)
			{
				FBlueprintCompiledStatement& Statement = *((*StatementList)[StatementIndex]);

				if (Statement.bIsJumpTarget)
				{
					// Just making sure we number them in order of appearance, so jump statements don't influence the order
					const int32 StateNum = StatementToStateIndex(FunctionContext, &Statement);
				}
			}
		}
	}

	bool bIsFunctionNotReducible = InnerFunctionImplementation(FunctionContext, EmitterContext, ExecutionGroup);
	if (!bIsFunctionNotReducible)
	{
		FNativizationSummaryHelper::ReducibleFunciton(EmitterContext.Dependencies.FindOriginalClass(EmitterContext.GetCurrentlyGeneratedClass()));
	}
}

void FBlueprintCompilerCppBackendBase::GenerateCodeFromEnum(UUserDefinedEnum* SourceEnum, const FCompilerNativizationOptions& NativizationOptions, FString& OutHeaderCode, FString& OutCPPCode)
{
	check(SourceEnum);
	FCodeText Header;
	Header.AddLine(TEXT("#pragma once"));
	const FString EnumCppName = *FEmitHelper::GetCppName(SourceEnum);
	// use GetBaseFilename() so that we can coordinate #includes and filenames
	Header.AddLine(FString::Printf(TEXT("#include \"%s.generated.h\""), *FEmitHelper::GetBaseFilename(SourceEnum, NativizationOptions)));
	Header.AddLine(FString::Printf(TEXT("UENUM(BlueprintType, %s )"), *FEmitHelper::ReplaceConvertedMetaData(SourceEnum)));
	Header.AddLine(FString::Printf(TEXT("enum class %s  : uint8"), *EnumCppName));
	Header.AddLine(TEXT("{"));
	Header.IncreaseIndent();

	auto EnumItemName = [&](int32 InIndex)
	{
		const int64 ElemValue = SourceEnum->GetValueByIndex(InIndex);
		if (ElemValue == SourceEnum->GetMaxEnumValue())
		{
			return FString::Printf(TEXT("%s_MAX"), *EnumCppName);
		}
		return SourceEnum->GetNameStringByIndex(InIndex);
	};

	for (int32 Index = 0; Index < SourceEnum->NumEnums(); ++Index)
	{
		const FString ElemCppName = EnumItemName(Index);
		const int64 ElemValue = SourceEnum->GetValueByIndex(Index);

		const FString& DisplayNameMD = SourceEnum->GetMetaData(TEXT("DisplayName"), ElemValue);// TODO: value or index?
		const FString MetaDisplayName = DisplayNameMD.IsEmpty() ? FString() : FString::Printf(TEXT("DisplayName = \"%s\","), *DisplayNameMD.ReplaceCharWithEscapedChar());
		const FString MetaOverrideName = FString::Printf(TEXT("OverrideName = \"%s\""), *SourceEnum->GetNameByIndex(Index).ToString());
		Header.AddLine(FString::Printf(TEXT("%s = %lld UMETA(%s%s),"), *ElemCppName, ElemValue, *MetaDisplayName, *MetaOverrideName));
	}

	Header.DecreaseIndent();
	Header.AddLine(TEXT("};"));

	Header.AddLine(FString::Printf(TEXT("FText %s__GetUserFriendlyName(int32 InValue);"), *EnumCppName));

	OutHeaderCode = MoveTemp(Header.Result);
	
	FCodeText Body;
	
	const FString PCHFilename = FEmitHelper::GetPCHFilename();
	if (!PCHFilename.IsEmpty())
	{
		Body.AddLine(FString::Printf(TEXT("#include \"%s\""), *PCHFilename));	
	}
	else
	{
		// Used when generated code is not in a separate module
		const FString MainHeaderFilename = FEmitHelper::GetGameMainHeaderFilename();
		if (!MainHeaderFilename.IsEmpty())
		{
			Body.AddLine(FString::Printf(TEXT("#include \"%s\""), *MainHeaderFilename));
		}
	}

	Body.AddLine(FString::Printf(TEXT("#include \"%s.h\""), *FEmitHelper::GetBaseFilename(SourceEnum, NativizationOptions)));

	// generate implementation of GetUserFriendlyName:
	Body.AddLine(FString::Printf(TEXT("FText %s__GetUserFriendlyName(int32 InValue)"), *EnumCppName, *EnumCppName));
	Body.AddLine(TEXT("{"));
	Body.IncreaseIndent();

	Body.AddLine(TEXT("FText Text;"));
	Body.AddLine(FString::Printf(TEXT("const auto EnumValue = static_cast<%s>(InValue);"), *EnumCppName));
	Body.AddLine(TEXT("switch(EnumValue)"));
	Body.AddLine(TEXT("{"));
	Body.IncreaseIndent();
	for (int32 Index = 0; Index < SourceEnum->NumEnums(); ++Index)
	{
		const FString ElemName = EnumItemName(Index);
		FString DisplayNameStr;
		FTextStringHelper::WriteToString(DisplayNameStr, SourceEnum->GetDisplayNameTextByIndex(Index));
		Body.AddLine(FString::Printf(TEXT("case %s::%s: FTextStringHelper::%s(TEXT(\"%s\"), Text); break;")
			, *EnumCppName, *ElemName
			, GET_FUNCTION_NAME_STRING_CHECKED(FTextStringHelper, ReadFromString)
			, *DisplayNameStr.ReplaceCharWithEscapedChar()));
	}

	Body.AddLine(TEXT("default: ensure(false);"));
	Body.DecreaseIndent();
	Body.AddLine(TEXT("};"));

	Body.AddLine(TEXT("return Text;"));
	Body.DecreaseIndent();
	Body.AddLine(TEXT("};"));

	OutCPPCode = MoveTemp(Body.Result);
}

void FBlueprintCompilerCppBackendBase::GenerateCodeFromStruct(UUserDefinedStruct* SourceStruct, const FCompilerNativizationOptions& NativizationOptions, FString& OutHeaderCode, FString& OutCPPCode)
{
	check(SourceStruct);
	FGatherConvertedClassDependencies Dependencies(SourceStruct, NativizationOptions);
	FNativizationSummaryHelper::RegisterRequiredModules(NativizationOptions.PlatformName, Dependencies.RequiredModuleNames);
	FEmitterLocalContext EmitterContext(Dependencies, NativizationOptions);
	// use GetBaseFilename() so that we can coordinate #includes and filenames
	EmitFileBeginning(FEmitHelper::GetBaseFilename(SourceStruct, NativizationOptions), EmitterContext, true, true);
	{
		FIncludedUnconvertedWrappers IncludedUnconvertedWrappers(EmitterContext, false);
		const FString CppStructName = FEmitHelper::GetCppName(SourceStruct);
		EmitterContext.Header.AddLine(FString::Printf(TEXT("USTRUCT(BlueprintType, %s)"), *FEmitHelper::ReplaceConvertedMetaData(SourceStruct)));
		EmitterContext.Header.AddLine(FString::Printf(TEXT("struct %s"), *CppStructName));
		EmitterContext.Header.AddLine(TEXT("{"));
		EmitterContext.Header.AddLine(TEXT("public:"));
		EmitterContext.Header.IncreaseIndent();
		EmitterContext.Header.AddLine(TEXT("GENERATED_BODY()"));
		EmitStructProperties(EmitterContext, SourceStruct);

		FEmitDefaultValueHelper::GenerateGetDefaultValue(SourceStruct, EmitterContext);

		EmitterContext.Header.AddLine(TEXT("static void __StaticDependenciesAssets(TArray<FBlueprintDependencyData>& AssetsToLoad);"));
		EmitterContext.Header.AddLine(TEXT("static void __StaticDependencies_DirectlyUsedAssets(TArray<FBlueprintDependencyData>& AssetsToLoad);"));

		EmitterContext.Header.AddLine(FString::Printf(TEXT("bool operator== (const %s& __Other) const"), *CppStructName));
		EmitterContext.Header.AddLine(TEXT("{"));
		EmitterContext.Header.IncreaseIndent();
		EmitterContext.Header.AddLine(FString::Printf(TEXT("return %s::StaticStruct()->%s(this, &__Other, 0);"), *CppStructName, GET_FUNCTION_NAME_STRING_CHECKED(UScriptStruct, CompareScriptStruct)));
		EmitterContext.Header.DecreaseIndent();
		EmitterContext.Header.AddLine(TEXT("};"));

		// Provide GetTypeHash if the struct is hashable:
		if (FBlueprintEditorUtils::StructHasGetTypeHash(SourceStruct))
		{
			EmitterContext.Header.AddLine(FString::Printf(TEXT("friend uint32 GetTypeHash(const %s& __Other) { return UUserDefinedStruct::GetUserDefinedStructTypeHash( &__Other, %s::StaticStruct()); }"), *CppStructName, *CppStructName));
		}

		EmitterContext.Header.DecreaseIndent();
		EmitterContext.Header.AddLine(TEXT("};"));
	}

	FEmitDefaultValueHelper::AddStaticFunctionsForDependencies(EmitterContext, nullptr, NativizationOptions);
	FEmitDefaultValueHelper::AddRegisterHelper(EmitterContext);

	OutCPPCode = MoveTemp(EmitterContext.Body.Result);
	OutHeaderCode = MoveTemp(EmitterContext.Header.Result);
}

FString FBlueprintCompilerCppBackendBase::GenerateWrapperForClass(UClass* SourceClass, const FCompilerNativizationOptions& NativizationOptions)
{
	FGatherConvertedClassDependencies Dependencies(SourceClass, NativizationOptions);
	FNativizationSummaryHelper::RegisterRequiredModules(NativizationOptions.PlatformName, Dependencies.RequiredModuleNames);
	FEmitterLocalContext EmitterContext(Dependencies, NativizationOptions);

	UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(SourceClass);

	TArray<UFunction*> FunctionsToGenerate;
	for (auto Func : TFieldRange<UFunction>(SourceClass, EFieldIteratorFlags::ExcludeSuper))
	{
		if (!Func->HasAnyFunctionFlags(FUNC_BlueprintCallable | FUNC_BlueprintPure))
		{
			continue;
		}

		if (BPGC && (Func == BPGC->UberGraphFunction))
		{
			continue;
		}

		// Exclude native events.. Unexpected.
		// Exclude delegate signatures.
		static const FName __UCSName(TEXT("UserConstructionScript"));
		if (__UCSName == Func->GetFName())
		{
			continue;
		}

		FunctionsToGenerate.Add(Func);
	}

	TArray<UMulticastDelegateProperty*> MCDelegateProperties;
	for (auto MCDelegateProp : TFieldRange<UMulticastDelegateProperty>(SourceClass, EFieldIteratorFlags::ExcludeSuper))
	{
		MCDelegateProperties.Add(MCDelegateProp);
	}

	const bool bGenerateAnyMCDelegateProperty = (0 != MCDelegateProperties.Num());

	FString ParentStruct;
	UClass* SuperClassToUse = SourceClass->GetSuperClass();
	{
		static const FBoolConfigValueHelper DontNativizeDataOnlyBP(TEXT("BlueprintNativizationSettings"), TEXT("bDontNativizeDataOnlyBP"));
		if (DontNativizeDataOnlyBP)
		{
			// Find first Native or Converted Or Not Data Only class
			for (; SuperClassToUse; SuperClassToUse = SuperClassToUse->GetSuperClass())
			{
				if (SuperClassToUse->HasAnyClassFlags(CLASS_Native))
				{
					break;
				}
				UBlueprintGeneratedClass* SuperBPGC = Cast<UBlueprintGeneratedClass>(SuperClassToUse);
				if (SuperBPGC && Dependencies.WillClassBeConverted(SuperBPGC))
				{
					break;
				}
				else if (SuperBPGC)
				{
					UBlueprint* SuperBP = Cast<UBlueprint>(SuperBPGC->ClassGeneratedBy);
					if (!ensure(SuperBP) || !FBlueprintEditorUtils::IsDataOnlyBlueprint(SuperBP))
					{
						break;
					}
				}
			}
		}

		UBlueprintGeneratedClass* SuperBPGC = Cast<UBlueprintGeneratedClass>(SuperClassToUse);
		if (SuperBPGC && !Dependencies.WillClassBeConverted(SuperBPGC))
		{
			ParentStruct = FString::Printf(TEXT("FUnconvertedWrapper__%s"), *FEmitHelper::GetCppName(SuperBPGC));
			EmitterContext.MarkUnconvertedClassAsNecessary(SuperBPGC);
		}
		else
		{
			ParentStruct = FString::Printf(TEXT("FUnconvertedWrapper<%s>"), *FEmitHelper::GetCppName(SuperClassToUse));
		}
	}

	// Include standard stuff
	EmitFileBeginning(FEmitHelper::GetBaseFilename(SourceClass, NativizationOptions), EmitterContext, bGenerateAnyMCDelegateProperty, true, true, SuperClassToUse);

	{
		FIncludedUnconvertedWrappers IncludedUnconvertedWrappers(EmitterContext, false);

		// DELEGATES
		const FString DelegatesClassName = FString::Printf(TEXT("U__Delegates__%s"), *FEmitHelper::GetCppName(SourceClass));
		auto GenerateMulticastDelegateTypeName = [](UMulticastDelegateProperty* MCDelegateProp) -> FString
		{
			return FString::Printf(TEXT("F__MulticastDelegate__%s"), *FEmitHelper::GetCppName(MCDelegateProp));
		};
		if (bGenerateAnyMCDelegateProperty)
		{
			EmitterContext.Header.AddLine(TEXT("UCLASS()"));
			EmitterContext.Header.AddLine(FString::Printf(TEXT("class %s : public UObject"), *DelegatesClassName));
			EmitterContext.Header.AddLine(TEXT("{"));
			EmitterContext.Header.AddLine(TEXT("public:"));
			EmitterContext.Header.IncreaseIndent();
			EmitterContext.Header.AddLine(TEXT("GENERATED_BODY()"));
			for (auto MCDelegateProp : MCDelegateProperties)
			{
				FString ParamNumberStr, Parameters;
				FEmitHelper::ParseDelegateDetails(EmitterContext, MCDelegateProp->SignatureFunction, Parameters, ParamNumberStr);
				EmitterContext.Header.AddLine(FString::Printf(TEXT("DECLARE_DYNAMIC_MULTICAST_DELEGATE%s(%s%s);"), *ParamNumberStr, *GenerateMulticastDelegateTypeName(MCDelegateProp), *Parameters));
			}
			EmitterContext.Header.DecreaseIndent();
			EmitterContext.Header.AddLine(TEXT("};"));
		}

		// Begin the struct
		const FString WrapperName = FString::Printf(TEXT("FUnconvertedWrapper__%s"), *FEmitHelper::GetCppName(SourceClass));

		EmitterContext.Header.AddLine(FString::Printf(TEXT("struct %s : public %s"), *WrapperName, *ParentStruct));
		EmitterContext.Header.AddLine(TEXT("{"));
		EmitterContext.Header.IncreaseIndent();

		// Constructor
		EmitterContext.Header.AddLine(FString::Printf(TEXT("%s(const %s* __InObject) : %s(__InObject){}"), *WrapperName, *FEmitHelper::GetCppName(EmitterContext.GetFirstNativeOrConvertedClass(SourceClass)), *ParentStruct));

		static const FBoolConfigValueHelper UseStaticVariables(TEXT("BlueprintNativizationSettings"), TEXT("bUseStaticVariablesInWrappers"));
		const bool bUseStaticVariables = UseStaticVariables;

		// PROPERTIES:
		for (auto Property : TFieldRange<UProperty>(SourceClass, EFieldIteratorFlags::ExcludeSuper))
		{
			if (BPGC && (Property == BPGC->UberGraphFramePointerProperty))
			{
				continue;
			}

			if (Cast<UAnimBlueprintGeneratedClass>(BPGC))
			{
				// Dont generate Getters for inner properties
				UStructProperty* StructProperty = Cast<UStructProperty>(Property);
				UScriptStruct* InnerStruct = StructProperty ? StructProperty->Struct : nullptr;
				if (InnerStruct && InnerStruct->IsChildOf(FAnimNode_Base::StaticStruct()))
				{
					continue;
				}
			}

			//TODO: check if the property is really used?
			const FString TypeDeclaration = Property->IsA<UMulticastDelegateProperty>()
				? FString::Printf(TEXT("%s::%s"), *DelegatesClassName, *GenerateMulticastDelegateTypeName(CastChecked<UMulticastDelegateProperty>(Property)))
				: EmitterContext.ExportCppDeclaration(Property
					, EExportedDeclaration::Parameter
					, EPropertyExportCPPFlags::CPPF_CustomTypeName | EPropertyExportCPPFlags::CPPF_BlueprintCppBackend | EPropertyExportCPPFlags::CPPF_NoRef | EPropertyExportCPPFlags::CPPF_NoConst
					, FEmitterLocalContext::EPropertyNameInDeclaration::Skip);
			EmitterContext.Header.AddLine(FString::Printf(TEXT("FORCENOINLINE %s& GetRef__%s()"), *TypeDeclaration, *UnicodeToCPPIdentifier(Property->GetName(), false, nullptr)));
			EmitterContext.Header.AddLine(TEXT("{"));
			EmitterContext.Header.IncreaseIndent();
			if (bUseStaticVariables)
			{
				EmitterContext.Header.AddLine(TEXT("static TWeakObjectPtr<UProperty> __PropertyPtr{};"));
				EmitterContext.Header.AddLine(TEXT("const UProperty* __Property = __PropertyPtr.Get();"));
				EmitterContext.Header.AddLine(TEXT("if (nullptr == __Property)"));
				EmitterContext.Header.AddLine(TEXT("{"));
				EmitterContext.Header.IncreaseIndent();
				EmitterContext.Header.AddLine(FString::Printf(TEXT("__Property = GetClass()->%s(FName(TEXT(\"%s\")));"), GET_FUNCTION_NAME_STRING_CHECKED(UClass, FindPropertyByName), *Property->GetName()));
				EmitterContext.Header.AddLine(TEXT("check(__Property);"));
				EmitterContext.Header.AddLine(TEXT("__PropertyPtr = __Property;"));
				EmitterContext.Header.DecreaseIndent();
				EmitterContext.Header.AddLine(TEXT("}"));
			}
			else
			{
				EmitterContext.Header.AddLine(FString::Printf(TEXT("const UProperty* __Property = GetClass()->%s(FName(TEXT(\"%s\")));"), GET_FUNCTION_NAME_STRING_CHECKED(UClass, FindPropertyByName), *Property->GetName()));
			}
			EmitterContext.Header.AddLine(FString::Printf(TEXT("return *(__Property->ContainerPtrToValuePtr<%s>(__Object));"), *TypeDeclaration));
			EmitterContext.Header.DecreaseIndent();
			EmitterContext.Header.AddLine(TEXT("}"));
		}

		// FUNCTIONS:
		for (auto Func : FunctionsToGenerate)
		{
			TArray<FString> FuncParameters;
			const FString ParamNameInStructPostfix(TEXT("_"));
			const FString FuncCppName = FEmitHelper::GetCppName(Func);
			FString DelareFunction = FString::Printf(TEXT("FORCENOINLINE void %s("), *FuncCppName);
			FString RawParameterList;
			{
				bool bFirst = true;
				for (TFieldIterator<UProperty> It(Func); It; ++It)
				{
					UProperty* Property = *It;
					if (!Property || !Property->HasAnyPropertyFlags(CPF_Parm))
					{
						continue;
					}

					if (!bFirst)
					{
						DelareFunction += TEXT(", ");
						RawParameterList += TEXT(", ");
					}
					else
					{
						bFirst = false;
					}

					if (Property->HasAnyPropertyFlags(CPF_OutParm))
					{
						DelareFunction += TEXT("/*out*/ ");
					}

					DelareFunction += EmitterContext.ExportCppDeclaration(Property, EExportedDeclaration::Parameter, EPropertyExportCPPFlags::CPPF_CustomTypeName | EPropertyExportCPPFlags::CPPF_BlueprintCppBackend);
					FString ParamAsStructMember;

					{	// copied from FNativeClassHeaderGenerator::ExportEventParm
						bool bEmitConst = Property->HasAnyPropertyFlags(CPF_ConstParm) && Property->IsA<UObjectProperty>();
						const bool bIsConstParam = (Property->IsA(UInterfaceProperty::StaticClass()) && !Property->HasAllPropertyFlags(CPF_OutParm));
						const bool bIsOnConstClass = (Property->IsA(UObjectProperty::StaticClass()) && ((UObjectProperty*)Property)->PropertyClass != NULL && ((UObjectProperty*)Property)->PropertyClass->HasAnyClassFlags(CLASS_Const));
						if (bIsConstParam || bIsOnConstClass)
						{
							bEmitConst = false; // ExportCppDeclaration will do it for us
						}

						if (bEmitConst)
						{
							ParamAsStructMember = TEXT("const ");
						}
					}
					ParamAsStructMember += EmitterContext.ExportCppDeclaration(Property
						, EExportedDeclaration::Local
						, EPropertyExportCPPFlags::CPPF_CustomTypeName | EPropertyExportCPPFlags::CPPF_BlueprintCppBackend
						, FEmitterLocalContext::EPropertyNameInDeclaration::Regular
						, ParamNameInStructPostfix);
					FuncParameters.Emplace(ParamAsStructMember);
					RawParameterList += UnicodeToCPPIdentifier(Property->GetName(), Property->HasAnyPropertyFlags(CPF_Deprecated), TEXT("bpp__"));
				}
			}
			DelareFunction += TEXT(")");
			EmitterContext.Header.AddLine(DelareFunction);
			EmitterContext.Header.AddLine(TEXT("{"));
			EmitterContext.Header.IncreaseIndent();

			if (bUseStaticVariables)
			{
				EmitterContext.Header.AddLine(FString::Printf(TEXT("static const FName __FunctionName(TEXT(\"%s\"));"), *Func->GetName()));
			}
			const FString FuncNameStr = bUseStaticVariables ? TEXT("__FunctionName") : FString::Printf(TEXT("FName(TEXT(\"%s\"))"), *Func->GetName());
			EmitterContext.Header.AddLine(FString::Printf(TEXT("UFunction* __Function = __Object->%s(%s);"), GET_FUNCTION_NAME_STRING_CHECKED(UObject, FindFunctionChecked), *FuncNameStr));

			const FString FuncParametersStructName = FuncParameters.Num() ? (FuncCppName + TEXT("_Parameters")) : FString();
			if (FuncParameters.Num())
			{
				EmitterContext.Header.AddLine(FString::Printf(TEXT("struct %s"), *FuncParametersStructName));
				EmitterContext.Header.AddLine(TEXT("{"));
				EmitterContext.Header.IncreaseIndent();
				for (FString& Param : FuncParameters)
				{
					EmitterContext.Header.AddLine(FString::Printf(TEXT("%s;"), *Param));
				}
				EmitterContext.Header.DecreaseIndent();
				EmitterContext.Header.AddLine(TEXT("};"));

				EmitterContext.Header.AddLine(FString::Printf(TEXT("%s __Parameters { %s };"), *FuncParametersStructName, *RawParameterList));
			}
			EmitterContext.Header.AddLine(FString::Printf(TEXT("__Object->%s(__Function, %s);"), GET_FUNCTION_NAME_STRING_CHECKED(UObject, ProcessEvent), FuncParameters.Num() ? TEXT("&__Parameters") : TEXT("nullptr")));
			EmitterContext.Header.DecreaseIndent();
			EmitterContext.Header.AddLine(TEXT("}"));
		}

		// close struct
		EmitterContext.Header.DecreaseIndent();
		EmitterContext.Header.AddLine(TEXT("};"));
	}
	return EmitterContext.Header.Result;
}

void FBlueprintCompilerCppBackendBase::EmitFileBeginning(const FString& CleanName, FEmitterLocalContext& EmitterContext, bool bIncludeGeneratedH, bool bIncludeCodeHelpersInHeader, bool bFullyIncludedDeclaration, UField* AdditionalFieldToIncludeInHeader)
{
	EmitterContext.Header.AddLine(TEXT("#pragma once"));

	const FString PCHFilename = FEmitHelper::GetPCHFilename();
	if (!PCHFilename.IsEmpty())
	{
		FIncludeHeaderHelper::EmitIncludeHeader(EmitterContext.Body, *PCHFilename, false);
	}
	else
	{
		// Used when generated code is not in a separate module
		const FString MainHeaderFilename = FEmitHelper::GetGameMainHeaderFilename();
		if (!MainHeaderFilename.IsEmpty())
		{
			FIncludeHeaderHelper::EmitIncludeHeader(EmitterContext.Body, *MainHeaderFilename, false);
		}
	}
	
	FIncludeHeaderHelper::EmitIncludeHeader(EmitterContext.Body, *CleanName, true);
	FIncludeHeaderHelper::EmitIncludeHeader(bIncludeCodeHelpersInHeader ? EmitterContext.Header : EmitterContext.Body, TEXT("GeneratedCodeHelpers"), true);
	FIncludeHeaderHelper::EmitIncludeHeader(EmitterContext.Header, TEXT("Blueprint/BlueprintSupport"), true);

	FBackendHelperUMG::AdditionalHeaderIncludeForWidget(EmitterContext);
	FBackendHelperAnim::AddHeaders(EmitterContext);

	TSet<FString> AlreadyIncluded;
	AlreadyIncluded.Add(CleanName);

	TSet<UField*> IncludeInBody = EmitterContext.Dependencies.IncludeInBody;
	TSet<UField*> IncludeInHeader = EmitterContext.Dependencies.IncludeInHeader;
	if (AdditionalFieldToIncludeInHeader)
	{
		IncludeInHeader.Add(AdditionalFieldToIncludeInHeader);
	}
	FIncludeHeaderHelper::EmitInner(EmitterContext.Header, IncludeInHeader, bFullyIncludedDeclaration ? TSet<UField*>() : EmitterContext.Dependencies.DeclareInHeader, EmitterContext.NativizationOptions, AlreadyIncluded);
	if (bFullyIncludedDeclaration)
	{
		FIncludeHeaderHelper::EmitInner(EmitterContext.Header, EmitterContext.Dependencies.DeclareInHeader, TSet<UField*>(), EmitterContext.NativizationOptions, AlreadyIncluded);
	}
	else
	{
		IncludeInBody.Append(EmitterContext.Dependencies.DeclareInHeader);
	}
	FIncludeHeaderHelper::EmitInner(EmitterContext.Body, IncludeInBody, TSet<UField*>(), EmitterContext.NativizationOptions, AlreadyIncluded);

	if (bIncludeGeneratedH)
	{
		EmitterContext.Header.AddLine(FString::Printf(TEXT("#include \"%s.generated.h\""), *CleanName));
	}
}

void FBlueprintCompilerCppBackendBase::CleanBackend()
{
	StateMapPerFunction.Empty();
	FunctionIndexMap.Empty();
	UberGraphContext = nullptr;
	UberGraphStatementToExecutionGroup.Empty();
}
