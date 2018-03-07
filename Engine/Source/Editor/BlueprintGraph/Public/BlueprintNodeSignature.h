// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Misc/Guid.h"
#include "Templates/SubclassOf.h"
#include "EdGraph/EdGraphNode.h"

struct BLUEPRINTGRAPH_API FBlueprintNodeSignature
{
public:
	FBlueprintNodeSignature() {}
	FBlueprintNodeSignature(FString const& UserString);
	FBlueprintNodeSignature(TSubclassOf<UEdGraphNode> NodeClass);

	/**
	 * 
	 * 
	 * @param  NodeClass	
	 * @return 
	 */
	void SetNodeClass(TSubclassOf<UEdGraphNode> NodeClass);

	/**
	 * 
	 * 
	 * @param  SignatureObj	
	 * @return 
	 */
	void AddSubObject(UObject const* SignatureObj);

	/**
	 * 
	 * 
	 * @param  Value	
	 * @return 
	 */
	void AddKeyValue(FString const& KeyValue);

	/**
	 * 
	 * 
	 * @param  SignatureKey	
	 * @param  Value	
	 * @return 
	 */
	void AddNamedValue(FName SignatureKey, FString const& Value);

	/**
	 * 
	 * 
	 * @return 
	 */
	bool IsValid() const;

	/**
	 * 
	 * 
	 * @return 
	 */
	FString const& ToString() const;

	/**
	*
	*
	* @return
	*/
	FGuid const& AsGuid() const;

private:
	/**
	 * 
	 * 
	 * @return 
	 */
	void MarkDirty();

private:
	typedef TMap<FName, FString> FSignatureSet;
	FSignatureSet SignatureSet;

	mutable FGuid   CachedSignatureGuid;
	mutable FString CachedSignatureString;
};

