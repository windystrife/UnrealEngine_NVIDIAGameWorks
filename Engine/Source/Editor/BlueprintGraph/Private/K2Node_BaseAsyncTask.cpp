// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "K2Node_BaseAsyncTask.h"
#include "EdGraph/EdGraphPin.h"
#include "UObject/UnrealType.h"
#include "EdGraphSchema_K2.h"
#include "Misc/ConfigCacheIni.h"
#include "Kismet/KismetSystemLibrary.h"
#include "K2Node_CallFunction.h"
#include "K2Node_AddDelegate.h"
#include "K2Node_AssignmentStatement.h"
#include "K2Node_CreateDelegate.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_Self.h"
#include "K2Node_TemporaryVariable.h"
#include "Kismet2/BlueprintEditorUtils.h"

#include "KismetCompiler.h"
#include "BlueprintNodeSpawner.h"
#include "BlueprintActionDatabaseRegistrar.h"

#define LOCTEXT_NAMESPACE "UK2Node_BaseAsyncTask"

UK2Node_BaseAsyncTask::UK2Node_BaseAsyncTask(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, ProxyFactoryFunctionName(NAME_None)
	, ProxyFactoryClass(nullptr)
	, ProxyClass(nullptr)
	, ProxyActivateFunctionName(NAME_None)
{
}

FText UK2Node_BaseAsyncTask::GetTooltipText() const
{
	const FString FunctionToolTipText = UK2Node_CallFunction::GetDefaultTooltipForFunction(GetFactoryFunction());
	return FText::FromString(FunctionToolTipText);
}

FText UK2Node_BaseAsyncTask::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (GetFactoryFunction() == nullptr)
	{
		return FText(LOCTEXT("UK2Node_BaseAsyncTaskGetNodeTitle", "Async Task: Missing Function"));
	}
	const FText FunctionToolTipText = UK2Node_CallFunction::GetUserFacingFunctionName(GetFactoryFunction());
	return FunctionToolTipText;
}

bool UK2Node_BaseAsyncTask::IsCompatibleWithGraph(const UEdGraph* TargetGraph) const
{
	bool bIsCompatible = false;
	// Can only place events in ubergraphs and macros (other code will help prevent macros with latents from ending up in functions), and basicasync task creates an event node:
	EGraphType GraphType = TargetGraph->GetSchema()->GetGraphType(TargetGraph);
	if (GraphType == EGraphType::GT_Ubergraph || GraphType == EGraphType::GT_Macro)
	{
		bIsCompatible = true;
	}
	return bIsCompatible && Super::IsCompatibleWithGraph(TargetGraph);
}

void UK2Node_BaseAsyncTask::AllocateDefaultPins()
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	CreatePin(EGPD_Input, K2Schema->PC_Exec, FString(), nullptr, K2Schema->PN_Execute);

	bool bExposeProxy = false;
	bool bHideThen = false;
	for (const UStruct* TestStruct = ProxyClass; TestStruct; TestStruct = TestStruct->GetSuperStruct())
	{
		bExposeProxy |= TestStruct->HasMetaData(TEXT("ExposedAsyncProxy"));
		bHideThen |= TestStruct->HasMetaData(TEXT("HideThen"));
	}

	if (!bHideThen)
	{
		CreatePin(EGPD_Output, K2Schema->PC_Exec, FString(), nullptr, K2Schema->PN_Then);
	}

	if (bExposeProxy)
	{
		CreatePin(EGPD_Output, K2Schema->PC_Object, FString(), ProxyClass, FBaseAsyncTaskHelper::GetAsyncTaskProxyName());
	}

	UFunction* DelegateSignatureFunction = NULL;
	for (TFieldIterator<UProperty> PropertyIt(ProxyClass); PropertyIt; ++PropertyIt)
	{
		if (UMulticastDelegateProperty* Property = Cast<UMulticastDelegateProperty>(*PropertyIt))
		{
			CreatePin(EGPD_Output, K2Schema->PC_Exec, FString(), nullptr, *Property->GetName());
			if (!DelegateSignatureFunction)
			{
				DelegateSignatureFunction = Property->SignatureFunction;
			}
		}
	}

	if (DelegateSignatureFunction)
	{
		for (TFieldIterator<UProperty> PropIt(DelegateSignatureFunction); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
		{
			UProperty* Param = *PropIt;
			const bool bIsFunctionInput = !Param->HasAnyPropertyFlags(CPF_OutParm) || Param->HasAnyPropertyFlags(CPF_ReferenceParm);
			if (bIsFunctionInput)
			{
				UEdGraphPin* Pin = CreatePin(EGPD_Output, FString(), FString(), nullptr, Param->GetName());
				K2Schema->ConvertPropertyToPinType(Param, /*out*/ Pin->PinType);
			}
		}
	}

	bool bAllPinsGood = true;
	UFunction* Function = ProxyFactoryClass ? ProxyFactoryClass->FindFunctionByName(ProxyFactoryFunctionName) : nullptr;
	if (Function)
	{
		TSet<FString> PinsToHide;
		FBlueprintEditorUtils::GetHiddenPinsForFunction(GetGraph(), Function, PinsToHide);
		for (TFieldIterator<UProperty> PropIt(Function); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
		{
			UProperty* Param = *PropIt;
			const bool bIsFunctionInput = !Param->HasAnyPropertyFlags(CPF_OutParm) || Param->HasAnyPropertyFlags(CPF_ReferenceParm);
			if (!bIsFunctionInput)
			{
				// skip function output, it's internal node data 
				continue;
			}

			const bool bIsRefParam = Param->HasAnyPropertyFlags(CPF_ReferenceParm) && bIsFunctionInput;
			UEdGraphPin* Pin = CreatePin(EGPD_Input, FString(), FString(), nullptr, Param->GetName(), EPinContainerType::None, bIsRefParam);
			const bool bPinGood = (Pin != NULL) && K2Schema->ConvertPropertyToPinType(Param, /*out*/ Pin->PinType);

			if (bPinGood)
			{
				//Flag pin as read only for const reference property
				Pin->bDefaultValueIsIgnored = Param->HasAllPropertyFlags(CPF_ConstParm | CPF_ReferenceParm) && (!Function->HasMetaData(FBlueprintMetadata::MD_AutoCreateRefTerm) || Pin->PinType.IsContainer());

				const bool bAdvancedPin = Param->HasAllPropertyFlags(CPF_AdvancedDisplay);
				Pin->bAdvancedView = bAdvancedPin;
				if(bAdvancedPin && (ENodeAdvancedPins::NoPins == AdvancedPinDisplay))
				{
					AdvancedPinDisplay = ENodeAdvancedPins::Hidden;
				}

				FString ParamValue;
				if (K2Schema->FindFunctionParameterDefaultValue(Function, Param, ParamValue))
				{
					K2Schema->SetPinAutogeneratedDefaultValue(Pin, ParamValue);
				}
				else
				{
					K2Schema->SetPinAutogeneratedDefaultValueBasedOnType(Pin);
				}

				if (PinsToHide.Contains(Pin->PinName))
				{
					Pin->bHidden = true;
				}
			}

			bAllPinsGood = bAllPinsGood && bPinGood;
		}
	}


	Super::AllocateDefaultPins();
}

const FString& UK2Node_BaseAsyncTask::FBaseAsyncTaskHelper::GetAsyncTaskProxyName()
{
	static const FString Name(TEXT("AsyncTaskProxy"));
	return Name;
}

bool UK2Node_BaseAsyncTask::FBaseAsyncTaskHelper::ValidDataPin(const UEdGraphPin* Pin, EEdGraphPinDirection Direction, const UEdGraphSchema_K2* Schema)
{
	check(Schema);
	const bool bValidDataPin = Pin
		&& !Pin->bOrphanedPin
		&& (Pin->PinType.PinCategory != Schema->PC_Exec);

	const bool bProperDirection = Pin && (Pin->Direction == Direction);

	return bValidDataPin && bProperDirection;
}

bool UK2Node_BaseAsyncTask::FBaseAsyncTaskHelper::CreateDelegateForNewFunction(UEdGraphPin* DelegateInputPin, FName FunctionName, UK2Node* CurrentNode, UEdGraph* SourceGraph, FKismetCompilerContext& CompilerContext)
{
	const UEdGraphSchema_K2* Schema = CompilerContext.GetSchema();
	check(DelegateInputPin && Schema && CurrentNode && SourceGraph && (FunctionName != NAME_None));
	bool bResult = true;

	// WORKAROUND, so we can create delegate from nonexistent function by avoiding check at expanding step
	// instead simply: Schema->TryCreateConnection(AddDelegateNode->GetDelegatePin(), CurrentCENode->FindPinChecked(UK2Node_CustomEvent::DelegateOutputName));
	UK2Node_Self* SelfNode = CompilerContext.SpawnIntermediateNode<UK2Node_Self>(CurrentNode, SourceGraph);
	SelfNode->AllocateDefaultPins();

	UK2Node_CreateDelegate* CreateDelegateNode = CompilerContext.SpawnIntermediateNode<UK2Node_CreateDelegate>(CurrentNode, SourceGraph);
	CreateDelegateNode->AllocateDefaultPins();
	bResult &= Schema->TryCreateConnection(DelegateInputPin, CreateDelegateNode->GetDelegateOutPin());
	bResult &= Schema->TryCreateConnection(SelfNode->FindPinChecked(Schema->PN_Self), CreateDelegateNode->GetObjectInPin());
	CreateDelegateNode->SetFunction(FunctionName);

	return bResult;
}

bool UK2Node_BaseAsyncTask::FBaseAsyncTaskHelper::CopyEventSignature(UK2Node_CustomEvent* CENode, UFunction* Function, const UEdGraphSchema_K2* Schema)
{
	check(CENode && Function && Schema);

	bool bResult = true;
	for (TFieldIterator<UProperty> PropIt(Function); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
	{
		const UProperty* Param = *PropIt;
		if (!Param->HasAnyPropertyFlags(CPF_OutParm) || Param->HasAnyPropertyFlags(CPF_ReferenceParm))
		{
			FEdGraphPinType PinType;
			bResult &= Schema->ConvertPropertyToPinType(Param, /*out*/ PinType);
			bResult &= (NULL != CENode->CreateUserDefinedPin(Param->GetName(), PinType, EGPD_Output));
		}
	}
	return bResult;
}

bool UK2Node_BaseAsyncTask::FBaseAsyncTaskHelper::HandleDelegateImplementation(
	UMulticastDelegateProperty* CurrentProperty, const TArray<FBaseAsyncTaskHelper::FOutputPinAndLocalVariable>& VariableOutputs,
	UEdGraphPin* ProxyObjectPin, UEdGraphPin*& InOutLastThenPin,
	UK2Node* CurrentNode, UEdGraph* SourceGraph, FKismetCompilerContext& CompilerContext)
{
	bool bIsErrorFree = true;
	const UEdGraphSchema_K2* Schema = CompilerContext.GetSchema();
	check(CurrentProperty && ProxyObjectPin && InOutLastThenPin && CurrentNode && SourceGraph && Schema);

	UEdGraphPin* PinForCurrentDelegateProperty = CurrentNode->FindPin(CurrentProperty->GetName());
	if (!PinForCurrentDelegateProperty || (Schema->PC_Exec != PinForCurrentDelegateProperty->PinType.PinCategory))
	{
		FText ErrorMessage = FText::Format(LOCTEXT("WrongDelegateProperty", "BaseAsyncTask: Cannot find execution pin for delegate "), FText::FromString(CurrentProperty->GetName()));
		CompilerContext.MessageLog.Error(*ErrorMessage.ToString(), CurrentNode);
		return false;
	}

	UK2Node_CustomEvent* CurrentCENode = CompilerContext.SpawnIntermediateEventNode<UK2Node_CustomEvent>(CurrentNode, PinForCurrentDelegateProperty, SourceGraph);
	{
	UK2Node_AddDelegate* AddDelegateNode = CompilerContext.SpawnIntermediateNode<UK2Node_AddDelegate>(CurrentNode, SourceGraph);
	AddDelegateNode->SetFromProperty(CurrentProperty, false);
	AddDelegateNode->AllocateDefaultPins();
		bIsErrorFree &= Schema->TryCreateConnection(AddDelegateNode->FindPinChecked(Schema->PN_Self), ProxyObjectPin);
		bIsErrorFree &= Schema->TryCreateConnection(InOutLastThenPin, AddDelegateNode->FindPinChecked(Schema->PN_Execute));
		InOutLastThenPin = AddDelegateNode->FindPinChecked(Schema->PN_Then);
		CurrentCENode->CustomFunctionName = *FString::Printf(TEXT("%s_%s"), *CurrentProperty->GetName(), *CompilerContext.GetGuid(CurrentNode));
		CurrentCENode->AllocateDefaultPins();

		bIsErrorFree &= FBaseAsyncTaskHelper::CreateDelegateForNewFunction(AddDelegateNode->GetDelegatePin(), CurrentCENode->GetFunctionName(), CurrentNode, SourceGraph, CompilerContext);
		bIsErrorFree &= FBaseAsyncTaskHelper::CopyEventSignature(CurrentCENode, AddDelegateNode->GetDelegateSignature(), Schema);
	}

	UEdGraphPin* LastActivatedNodeThen = CurrentCENode->FindPinChecked(Schema->PN_Then);
	for (const FBaseAsyncTaskHelper::FOutputPinAndLocalVariable& OutputPair : VariableOutputs) // CREATE CHAIN OF ASSIGMENTS
	{
		UEdGraphPin* PinWithData = CurrentCENode->FindPin(OutputPair.OutputPin->PinName);
		if (PinWithData == nullptr)
		{
			/*FText ErrorMessage = FText::Format(LOCTEXT("MissingDataPin", "ICE: Pin @@ was expecting a data output pin named {0} on @@ (each delegate must have the same signature)"), FText::FromString(OutputPair.OutputPin->PinName));
			CompilerContext.MessageLog.Error(*ErrorMessage.ToString(), OutputPair.OutputPin, CurrentCENode);
			return false;*/
			continue;
		}

		UK2Node_AssignmentStatement* AssignNode = CompilerContext.SpawnIntermediateNode<UK2Node_AssignmentStatement>(CurrentNode, SourceGraph);
		AssignNode->AllocateDefaultPins();
		bIsErrorFree &= Schema->TryCreateConnection(LastActivatedNodeThen, AssignNode->GetExecPin());
		bIsErrorFree &= Schema->TryCreateConnection(OutputPair.TempVar->GetVariablePin(), AssignNode->GetVariablePin());
		AssignNode->NotifyPinConnectionListChanged(AssignNode->GetVariablePin());
		bIsErrorFree &= Schema->TryCreateConnection(AssignNode->GetValuePin(), PinWithData);
		AssignNode->NotifyPinConnectionListChanged(AssignNode->GetValuePin());

		LastActivatedNodeThen = AssignNode->GetThenPin();
	}

	bIsErrorFree &= CompilerContext.MovePinLinksToIntermediate(*PinForCurrentDelegateProperty, *LastActivatedNodeThen).CanSafeConnect();
	return bIsErrorFree;
}


void UK2Node_BaseAsyncTask::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
    Super::ExpandNode(CompilerContext, SourceGraph);

	const UEdGraphSchema_K2* Schema = CompilerContext.GetSchema();
	check(SourceGraph && Schema);
	bool bIsErrorFree = true;

	// Create a call to factory the proxy object
	UK2Node_CallFunction* const CallCreateProxyObjectNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	CallCreateProxyObjectNode->FunctionReference.SetExternalMember(ProxyFactoryFunctionName, ProxyFactoryClass);
	CallCreateProxyObjectNode->AllocateDefaultPins();
	if (CallCreateProxyObjectNode->GetTargetFunction() == nullptr)
	{
		const FString ClassName = ProxyFactoryClass ? ProxyFactoryClass->GetName() : LOCTEXT("MissingClassString", "Unknown Class").ToString();
		const FString RawMessage = LOCTEXT("AsyncTaskError", "BaseAsyncTask: Missing function %s from class %s for async task @@").ToString();
		const FString FormattedMessage = FString::Printf(*RawMessage, *ProxyFactoryFunctionName.GetPlainNameString(), *ClassName);
		CompilerContext.MessageLog.Error(*FormattedMessage, this);
		return;
	}

	bIsErrorFree &= CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(Schema->PN_Execute), *CallCreateProxyObjectNode->FindPinChecked(Schema->PN_Execute)).CanSafeConnect();

	for (UEdGraphPin* CurrentPin : Pins)
	{
		if (FBaseAsyncTaskHelper::ValidDataPin(CurrentPin, EGPD_Input, Schema))
		{
			UEdGraphPin* DestPin = CallCreateProxyObjectNode->FindPin(CurrentPin->PinName); // match function inputs, to pass data to function from CallFunction node
			bIsErrorFree &= DestPin && CompilerContext.MovePinLinksToIntermediate(*CurrentPin, *DestPin).CanSafeConnect();
		}
	}

	UEdGraphPin* const ProxyObjectPin = CallCreateProxyObjectNode->GetReturnValuePin();
	check(ProxyObjectPin);
	UEdGraphPin* OutputAsyncTaskProxy = FindPin(FBaseAsyncTaskHelper::GetAsyncTaskProxyName());
	bIsErrorFree &= !OutputAsyncTaskProxy || CompilerContext.MovePinLinksToIntermediate(*OutputAsyncTaskProxy, *ProxyObjectPin).CanSafeConnect();

	// GATHER OUTPUT PARAMETERS AND PAIR THEM WITH LOCAL VARIABLES
	TArray<FBaseAsyncTaskHelper::FOutputPinAndLocalVariable> VariableOutputs;
	for (UEdGraphPin* CurrentPin : Pins)
	{
		if ((OutputAsyncTaskProxy != CurrentPin) && FBaseAsyncTaskHelper::ValidDataPin(CurrentPin, EGPD_Output, Schema))
		{
			const FEdGraphPinType& PinType = CurrentPin->PinType;
			UK2Node_TemporaryVariable* TempVarOutput = CompilerContext.SpawnInternalVariable(
				this, PinType.PinCategory, PinType.PinSubCategory, PinType.PinSubCategoryObject.Get(), PinType.ContainerType, PinType.PinValueType);
			bIsErrorFree &= TempVarOutput->GetVariablePin() && CompilerContext.MovePinLinksToIntermediate(*CurrentPin, *TempVarOutput->GetVariablePin()).CanSafeConnect();
			VariableOutputs.Add(FBaseAsyncTaskHelper::FOutputPinAndLocalVariable(CurrentPin, TempVarOutput));
		}
	}

	// FOR EACH DELEGATE DEFINE EVENT, CONNECT IT TO DELEGATE AND IMPLEMENT A CHAIN OF ASSIGMENTS
	UEdGraphPin* LastThenPin = CallCreateProxyObjectNode->FindPinChecked(Schema->PN_Then);

	UK2Node_CallFunction* IsValidFuncNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	const FName IsValidFuncName = GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, IsValid);
	IsValidFuncNode->FunctionReference.SetExternalMember(IsValidFuncName, UKismetSystemLibrary::StaticClass());
	IsValidFuncNode->AllocateDefaultPins();
	UEdGraphPin* IsValidInputPin = IsValidFuncNode->FindPinChecked(TEXT("Object"));

	bIsErrorFree &= Schema->TryCreateConnection(ProxyObjectPin, IsValidInputPin);

	UK2Node_IfThenElse* ValidateProxyNode = CompilerContext.SpawnIntermediateNode<UK2Node_IfThenElse>(this, SourceGraph);
	ValidateProxyNode->AllocateDefaultPins();
	bIsErrorFree &= Schema->TryCreateConnection(IsValidFuncNode->GetReturnValuePin(), ValidateProxyNode->GetConditionPin());

	bIsErrorFree &= Schema->TryCreateConnection(LastThenPin, ValidateProxyNode->GetExecPin());
	LastThenPin = ValidateProxyNode->GetThenPin();

	for (TFieldIterator<UMulticastDelegateProperty> PropertyIt(ProxyClass); PropertyIt && bIsErrorFree; ++PropertyIt)
	{
		bIsErrorFree &= FBaseAsyncTaskHelper::HandleDelegateImplementation(*PropertyIt, VariableOutputs, ProxyObjectPin, LastThenPin, this, SourceGraph, CompilerContext);
	}

	if (CallCreateProxyObjectNode->FindPinChecked(Schema->PN_Then) == LastThenPin)
	{
		CompilerContext.MessageLog.Error(*LOCTEXT("MissingDelegateProperties", "BaseAsyncTask: Proxy has no delegates defined. @@").ToString(), this);
		return;
	}

	// Create a call to activate the proxy object if necessary
	if (ProxyActivateFunctionName != NAME_None)
	{
		UK2Node_CallFunction* const CallActivateProxyObjectNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
		CallActivateProxyObjectNode->FunctionReference.SetExternalMember(ProxyActivateFunctionName, ProxyClass);
		CallActivateProxyObjectNode->AllocateDefaultPins();

		// Hook up the self connection
		UEdGraphPin* ActivateCallSelfPin = Schema->FindSelfPin(*CallActivateProxyObjectNode, EGPD_Input);
		check(ActivateCallSelfPin);

		bIsErrorFree &= Schema->TryCreateConnection(ProxyObjectPin, ActivateCallSelfPin);

		// Hook the activate node up in the exec chain
		UEdGraphPin* ActivateExecPin = CallActivateProxyObjectNode->FindPinChecked(Schema->PN_Execute);
		UEdGraphPin* ActivateThenPin = CallActivateProxyObjectNode->FindPinChecked(Schema->PN_Then);

		bIsErrorFree &= Schema->TryCreateConnection(LastThenPin, ActivateExecPin);

		LastThenPin = ActivateThenPin;
	}

	// Move the connections from the original node then pin to the last internal then pin

	UEdGraphPin* OriginalThenPin = FindPin(Schema->PN_Then);

	if (OriginalThenPin)
	{
		bIsErrorFree &= CompilerContext.MovePinLinksToIntermediate(*OriginalThenPin, *LastThenPin).CanSafeConnect();
	}
	bIsErrorFree &= CompilerContext.CopyPinLinksToIntermediate(*LastThenPin, *ValidateProxyNode->GetElsePin()).CanSafeConnect();

	if (!bIsErrorFree)
	{
		CompilerContext.MessageLog.Error(*LOCTEXT("InternalConnectionError", "BaseAsyncTask: Internal connection error. @@").ToString(), this);
	}

	// Make sure we caught everything
	BreakAllNodeLinks();
}

bool UK2Node_BaseAsyncTask::HasExternalDependencies(TArray<class UStruct*>* OptionalOutput) const
{
	const UBlueprint* SourceBlueprint = GetBlueprint();

	const bool bProxyFactoryResult = (ProxyFactoryClass != NULL) && (ProxyFactoryClass->ClassGeneratedBy != SourceBlueprint);
	if (bProxyFactoryResult && OptionalOutput)
	{
		OptionalOutput->AddUnique(ProxyFactoryClass);
	}

	const bool bProxyResult = (ProxyClass != NULL) && (ProxyClass->ClassGeneratedBy != SourceBlueprint);
	if (bProxyResult && OptionalOutput)
	{
		OptionalOutput->AddUnique(ProxyClass);
	}

	const bool bSuperResult = Super::HasExternalDependencies(OptionalOutput);
	return bProxyFactoryResult || bProxyResult || bSuperResult;
}

FName UK2Node_BaseAsyncTask::GetCornerIcon() const
{
	return TEXT("Graph.Latent.LatentIcon");
}

FText UK2Node_BaseAsyncTask::GetMenuCategory() const
{	
	UFunction* TargetFunction = GetFactoryFunction();
	return UK2Node_CallFunction::GetDefaultCategoryForFunction(TargetFunction, FText::GetEmpty());
}

void UK2Node_BaseAsyncTask::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	// actions get registered under specific object-keys; the idea is that 
	// actions might have to be updated (or deleted) if their object-key is  
	// mutated (or removed)... here we use the node's class (so if the node 
	// type disappears, then the action should go with it)
	UClass* ActionKey = GetClass();
	// to keep from needlessly instantiating a UBlueprintNodeSpawner, first   
	// check to make sure that the registrar is looking for actions of this type
	// (could be regenerating actions for a specific asset, and therefore the 
	// registrar would only accept actions corresponding to that asset)
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);

		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

UFunction* UK2Node_BaseAsyncTask::GetFactoryFunction() const
{
	if (ProxyFactoryClass == nullptr)
	{
		UE_LOG(LogBlueprint, Error, TEXT("ProxyFactoryClass null in %s. Was a class deleted or saved on a non promoted build?"), *GetFullName());
		return nullptr;
	}
	
	UFunction* FactoryFunction = ProxyFactoryClass->FindFunctionByName(ProxyFactoryFunctionName);
	
	if (FactoryFunction == nullptr)
	{
		UE_LOG(LogBlueprint, Error, TEXT("FactoryFunction %s null in %s. Was a class deleted or saved on a non promoted build?"), *ProxyFactoryFunctionName.ToString(), *GetFullName());
		return nullptr;
	}

	return FactoryFunction;
}

void UK2Node_BaseAsyncTask::ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const
{
	Super::ValidateNodeDuringCompilation(MessageLog);

	UEdGraphSchema_K2 const* K2Schema = GetDefault<UEdGraphSchema_K2>();

	if(UObject const* SourceObject = MessageLog.FindSourceObject(this))
	{
		// Lets check if it's a result of macro expansion, to give a helpful error
		if(UK2Node_MacroInstance const* MacroInstance = Cast<UK2Node_MacroInstance>(SourceObject))
		{
			// Since it's not possible to check the graph's type, just check if this is a ubergraph using the schema's name for it
			if(!(GetGraph()->HasAnyFlags(RF_Transient) && GetGraph()->GetName().StartsWith(K2Schema->FN_ExecuteUbergraphBase.ToString())))
			{
				MessageLog.Error(*LOCTEXT("AsyncTaskInFunctionFromMacro", "@@ is being used in Function '@@' resulting from expansion of Macro '@@'").ToString(), this, GetGraph(), MacroInstance);
			}
		}
	}
}

TMap<FString, FAsyncTaskPinRedirectMapInfo> UK2Node_BaseAsyncTask::AsyncTaskPinRedirectMap;
bool UK2Node_BaseAsyncTask::bAsyncTaskPinRedirectMapInitialized = false;

UK2Node::ERedirectType UK2Node_BaseAsyncTask::DoPinsMatchForReconstruction(const UEdGraphPin* NewPin, int32 NewPinIndex, const UEdGraphPin* OldPin, int32 OldPinIndex) const
{
	if (GConfig && ProxyClass)
	{
		// Initialize remap table from INI
		if (!bAsyncTaskPinRedirectMapInitialized)
		{
			bAsyncTaskPinRedirectMapInitialized = true;
			FConfigSection* PackageRedirects = GConfig->GetSectionPrivate(TEXT("/Script/Engine.Engine"), false, true, GEngineIni);
			for (FConfigSection::TIterator It(*PackageRedirects); It; ++It)
			{
				if (It.Key() == TEXT("K2AsyncTaskPinRedirects"))
				{
					FString ProxyClassString;
					FString OldPinString;
					FString NewPinString;

					FParse::Value(*It.Value().GetValue(), TEXT("ProxyClassName="), ProxyClassString);
					FParse::Value(*It.Value().GetValue(), TEXT("OldPinName="), OldPinString);
					FParse::Value(*It.Value().GetValue(), TEXT("NewPinName="), NewPinString);

					UClass* RedirectProxyClass = FindObject<UClass>(ANY_PACKAGE, *ProxyClassString);
					if (RedirectProxyClass)
					{
						FAsyncTaskPinRedirectMapInfo& PinRedirectInfo = AsyncTaskPinRedirectMap.FindOrAdd(OldPinString);
						TArray<UClass*>& ProxyClassArray = PinRedirectInfo.OldPinToProxyClassMap.FindOrAdd(NewPinString);
						ProxyClassArray.AddUnique(RedirectProxyClass);
					}
				}
			}
		}

		// See if these pins need to be remapped.
		if (FAsyncTaskPinRedirectMapInfo* PinRedirectInfo = AsyncTaskPinRedirectMap.Find(OldPin->PinName))
		{
			if (TArray<UClass*>* ProxyClassArray = PinRedirectInfo->OldPinToProxyClassMap.Find(NewPin->PinName))
			{
				for (UClass* RedirectedProxyClass : *ProxyClassArray)
				{
					if (ProxyClass->IsChildOf(RedirectedProxyClass))
					{
						return UK2Node::ERedirectType_Name;
					}
				}
			}
		}
	}

	return Super::DoPinsMatchForReconstruction(NewPin, NewPinIndex, OldPin, OldPinIndex);
}

#undef LOCTEXT_NAMESPACE
