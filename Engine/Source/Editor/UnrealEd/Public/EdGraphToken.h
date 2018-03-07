// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"
#include "EdGraph/EdGraphPin.h"
#include "Logging/TokenizedMessage.h"

class FCompilerResultsLog;

/**
 * A Message Log token that links to an elemnt (node or pin) in an EdGraph
 */
class FEdGraphToken : public IMessageToken
{
public:
	/** Factory method, tokens can only be constructed as shared refs */
	UNREALED_API static TSharedRef<IMessageToken> Create(const UObject* InObject, const FCompilerResultsLog* Log, UEdGraphNode*& OutSourceNode);
	UNREALED_API static TSharedRef<IMessageToken> Create(const UEdGraphPin* InPin, const FCompilerResultsLog* Log, UEdGraphNode*& OutSourceNode);
	UNREALED_API static TSharedRef<IMessageToken> Create(const TCHAR* String, const FCompilerResultsLog* Log, UEdGraphNode*& OutSourceNode);

	/** Begin IMessageToken interface */
	virtual EMessageToken::Type GetType() const override
	{
		return EMessageToken::EdGraph;
	}

	UNREALED_API const UEdGraphPin* GetPin() const;
	UNREALED_API const UObject* GetGraphObject() const;

private:
	/** Private constructor */
	FEdGraphToken( const UObject* InObject, const UEdGraphPin* InPin );

	/** An object being referenced by this token, if any */
	FWeakObjectPtr ObjectBeingReferenced;

	FEdGraphPinReference PinBeingReferenced;
};

