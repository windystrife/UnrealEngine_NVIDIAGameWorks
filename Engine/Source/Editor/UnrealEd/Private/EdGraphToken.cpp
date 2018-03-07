// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "EdGraphToken.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/BlueprintEditorUtils.h"

TSharedRef<IMessageToken> FEdGraphToken::Create(const UObject* InObject, const FCompilerResultsLog* Log, UEdGraphNode*& OutSourceNode)
{
	const UObject* SourceNode = Log->FindSourceObject(InObject);
	if (OutSourceNode == nullptr)
	{
		OutSourceNode = const_cast<UEdGraphNode*>(Cast<UEdGraphNode>(SourceNode));
	}
	return MakeShareable(new FEdGraphToken(Log->FindSourceObject(InObject), nullptr));
}

TSharedRef<IMessageToken> FEdGraphToken::Create(const UEdGraphPin* InPin, const FCompilerResultsLog* Log, UEdGraphNode*& OutSourceNode)
{
	const UObject* SourceNode = InPin ? Log->FindSourceObject(InPin->GetOwningNode()) : nullptr;
	if (OutSourceNode == nullptr)
	{
		OutSourceNode = const_cast<UEdGraphNode*>(Cast<UEdGraphNode>(SourceNode));
	}
	return MakeShareable(new FEdGraphToken(SourceNode, Log->FindSourcePin(InPin)));
}

TSharedRef<IMessageToken> FEdGraphToken::Create(const TCHAR* String, const FCompilerResultsLog* Log, UEdGraphNode*& OutSourceNode)
{
	return FTextToken::Create(FText::FromString(FString(String)));
}

const UEdGraphPin* FEdGraphToken::GetPin() const
{
	return PinBeingReferenced.Get();
}

const UObject* FEdGraphToken::GetGraphObject() const
{
	return ObjectBeingReferenced.Get();
}

FEdGraphToken::FEdGraphToken(const UObject* InObject, const UEdGraphPin* InPin)
	: ObjectBeingReferenced(InObject)
	, PinBeingReferenced(InPin)
{
	if (InPin)
	{
		CachedText = InPin->GetDisplayName();
		if (CachedText.IsEmpty())
		{
			CachedText = NSLOCTEXT("MessageLog", "UnnamedPin", "<Unnamed>");
		}
	}
	else if (InObject)
	{
		if (const UEdGraphNode* Node = Cast<UEdGraphNode>(InObject))
		{
			CachedText = Node->GetNodeTitle(ENodeTitleType::ListView);
		}
		else if(const UClass* Class = Cast<UClass>(InObject))
		{
			// Remove the trailing C if that is the users preference:
			CachedText = FBlueprintEditorUtils::GetFriendlyClassDisplayName(Class);
		}
		else if(const UField* Field = Cast<UField>(InObject))
		{
			CachedText = Field->GetDisplayNameText();
		}
		else
		{
			CachedText = FText::FromString(InObject->GetName());
		}
	}
	else
	{
		CachedText = NSLOCTEXT("MessageLog", "NoneObjectToken", "<None>");
	}
}
