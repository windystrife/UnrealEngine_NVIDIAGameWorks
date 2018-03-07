// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "KismetEditorUtilities.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimClassInterface.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimInstance.h"
#include "AnimNode_Root.h"

DEFINE_LOG_CATEGORY_STATIC(LogAnimBlueprintFastPathTests, Log, All);

struct FCheckFastPathCommandPayload
{
	/** The filename to check */
	FString Filename;

	/** Whether to check enabled or disabled */
	bool bCheckEnabled;
};

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FCompileAnimBlueprintLatentCommand, FString, Filename);

bool FCompileAnimBlueprintLatentCommand::Update()
{
	UAnimBlueprint* AnimBlueprint = LoadObject<UAnimBlueprint>(nullptr, *Filename);
	if (AnimBlueprint != nullptr)
	{
		FKismetEditorUtilities::CompileBlueprint(AnimBlueprint);
		if (AnimBlueprint->Status == EBlueprintStatus::BS_UpToDate)
		{
			UE_LOG(LogAnimBlueprintFastPathTests, Display, TEXT("Anim blueprint compiled successfully (%s)"), *AnimBlueprint->GetName());
		}
		else
		{
			UE_LOG(LogAnimBlueprintFastPathTests, Error, TEXT("Anim blueprint is in an unexpected state after compiling (%s)"), *AnimBlueprint->GetName());
		}
	}

	return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FCheckFastPathLatentCommand, FCheckFastPathCommandPayload, Payload);

bool FCheckFastPathLatentCommand::Update()
{
	bool bIsStructTest = Payload.Filename.Contains(TEXT("SubStruct"));

	UAnimBlueprint* AnimBlueprint = LoadObject<UAnimBlueprint>(nullptr, *Payload.Filename);
	if (AnimBlueprint != nullptr)
	{
		if (AnimBlueprint->GeneratedClass)
		{
			UAnimInstance* DefaultAnimInstance = Cast<UAnimInstance>(AnimBlueprint->GeneratedClass->GetDefaultObject());
			IAnimClassInterface* AnimClassInterface = Cast<IAnimClassInterface>(AnimBlueprint->GeneratedClass);
			if (AnimClassInterface && DefaultAnimInstance)
			{
				for (UStructProperty* Property : AnimClassInterface->GetAnimNodeProperties())
				{
					if (Property->Struct->IsChildOf(FAnimNode_Base::StaticStruct()) && !Property->Struct->IsChildOf(FAnimNode_Root::StaticStruct()))
					{
						FAnimNode_Base* AnimNode = Property->ContainerPtrToValuePtr<FAnimNode_Base>(DefaultAnimInstance);
						if (AnimNode)
						{
							if (Payload.bCheckEnabled)
							{
								if (AnimNode->EvaluateGraphExposedInputs.BoundFunction != NAME_None)
								{
									UE_LOG(LogAnimBlueprintFastPathTests, Error, TEXT("Found blueprint VM call (%s) in fast-path Anim Blueprint (%s)"), *AnimNode->EvaluateGraphExposedInputs.BoundFunction.ToString(), *AnimBlueprint->GetName());
								}
								for (FExposedValueCopyRecord& CopyRecord : AnimNode->EvaluateGraphExposedInputs.CopyRecords)
								{
									if (CopyRecord.SourcePropertyName == NAME_None)
									{
										UE_LOG(LogAnimBlueprintFastPathTests, Error, TEXT("Anim blueprint has an invalid source property name (%s)"), *AnimBlueprint->GetName());
									}
									if (bIsStructTest)
									{
										if (CopyRecord.SourceSubPropertyName == NAME_None)
										{
											UE_LOG(LogAnimBlueprintFastPathTests, Error, TEXT("Anim blueprint has an invalid source sub struct property name (%s)"), *AnimBlueprint->GetName());
										}
									}
									else
									{
										if (CopyRecord.SourceSubPropertyName != NAME_None)
										{
											UE_LOG(LogAnimBlueprintFastPathTests, Error, TEXT("Anim blueprint specifies a sub struct when it shouldnt (%s)"), *AnimBlueprint->GetName());
										}
									}
									if (CopyRecord.DestProperty == nullptr)
									{
										UE_LOG(LogAnimBlueprintFastPathTests, Error, TEXT("Anim blueprint has an invalid dest property ptr (%s)"), *AnimBlueprint->GetName());
									}
									if (CopyRecord.DestArrayIndex < 0)
									{
										UE_LOG(LogAnimBlueprintFastPathTests, Error, TEXT("Anim blueprint has an invalid dest array index (%s)"), *AnimBlueprint->GetName());
									}
									if (CopyRecord.Size <= 0)
									{
										UE_LOG(LogAnimBlueprintFastPathTests, Error, TEXT("Anim blueprint has an invalid size (%s)"), *AnimBlueprint->GetName());
									}
								}
							}
							else
							{
								if (AnimNode->EvaluateGraphExposedInputs.BoundFunction == NAME_None)
								{
									UE_LOG(LogAnimBlueprintFastPathTests, Error, TEXT("No function bound for node evaluation (%s)"), *AnimBlueprint->GetName());
								}
								if (AnimNode->EvaluateGraphExposedInputs.CopyRecords.Num() > 0)
								{
									UE_LOG(LogAnimBlueprintFastPathTests, Error, TEXT("Found copy records in non-fast-path node evaluator (%s)"), *AnimBlueprint->GetName());
								}
							}
						}
					}
				}
			}
			else
			{
				UE_LOG(LogAnimBlueprintFastPathTests, Error, TEXT("Anim blueprint has an invalid generated class or CDO (%s)"), *AnimBlueprint->GetName());
			}
		}
		else
		{
			UE_LOG(LogAnimBlueprintFastPathTests, Error, TEXT("Anim blueprint has no generated class (%s)"), *AnimBlueprint->GetName());
		}
	}

	return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FVerifyFastPathTest, "System.Engine.Animation.Verify Fast Path", (EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter))

void FVerifyFastPathTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	OutTestCommands.Add(TEXT("/Game/Tests/Animation/FastPath/SingleIntegralType.SingleIntegralType"));
	OutBeautifiedNames.Add(TEXT("IntegralType"));

	OutTestCommands.Add(TEXT("/Game/Tests/Animation/FastPath/MultiIntegralType.MultiIntegralType"));
	OutBeautifiedNames.Add(TEXT("MultiIntegralType"));

	OutTestCommands.Add(TEXT("/Game/Tests/Animation/FastPath/Struct.Struct"));
	OutBeautifiedNames.Add(TEXT("Struct"));

	OutTestCommands.Add(TEXT("/Game/Tests/Animation/FastPath/MultiStruct.MultiStruct"));
	OutBeautifiedNames.Add(TEXT("MultiStruct"));

	OutTestCommands.Add(TEXT("/Game/Tests/Animation/FastPath/SubStructBreak.SubStructBreak"));
	OutBeautifiedNames.Add(TEXT("SubStructBreak"));

	OutTestCommands.Add(TEXT("/Game/Tests/Animation/FastPath/SubStructSplit.SubStructSplit"));
	OutBeautifiedNames.Add(TEXT("SubStructSplit"));

	OutTestCommands.Add(TEXT("/Game/Tests/Animation/FastPath/NegatedBool.NegatedBool"));
	OutBeautifiedNames.Add(TEXT("NegatedBool"));

	OutTestCommands.Add(TEXT("/Game/Tests/Animation/FastPath/MultiPropertyToArray.MultiPropertyToArray"));
	OutBeautifiedNames.Add(TEXT("MultiPropertyToArray"));
}

bool FVerifyFastPathTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FCompileAnimBlueprintLatentCommand(Parameters));
	ADD_LATENT_AUTOMATION_COMMAND(FCheckFastPathLatentCommand({ Parameters, true }));

	return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FVerifyNotFastPathTest, "System.Engine.Animation.Verify Not Fast Path", (EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter))

void FVerifyNotFastPathTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	OutTestCommands.Add(TEXT("/Game/Tests/Animation/FastPath/BlueprintLogic.BlueprintLogic"));
	OutBeautifiedNames.Add(TEXT("BlueprintLogic"));

	OutTestCommands.Add(TEXT("/Game/Tests/Animation/FastPath/NativeFunctionCall.NativeFunctionCall"));
	OutBeautifiedNames.Add(TEXT("NativeFunctionCall"));

	OutTestCommands.Add(TEXT("/Game/Tests/Animation/FastPath/BlueprintFunctionCall.BlueprintFunctionCall"));
	OutBeautifiedNames.Add(TEXT("BlueprintFunctionCall"));
}

bool FVerifyNotFastPathTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FCompileAnimBlueprintLatentCommand(Parameters));
	ADD_LATENT_AUTOMATION_COMMAND(FCheckFastPathLatentCommand({ Parameters, false }));

	return true;
}