// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR
#include "DerivedDataPluginInterface.h"
#endif

class UAnimSequence;
struct FAnimCompressContext;

#if WITH_EDITOR

//////////////////////////////////////////////////////////////////////////
// FDerivedDataAnimationCompression
class FDerivedDataAnimationCompression : public FDerivedDataPluginInterface
{
private:
	// Anim sequence we are providing DDC data for
	UAnimSequence* OriginalAnimSequence;

	// Possible duplicate animation for doing actual build work on
	UAnimSequence* DuplicateSequence;

	// FAnimCompressContext to use during compression if we don't pull from the DDC
	TSharedPtr<FAnimCompressContext> CompressContext;

	// Whether to do compression work on original animation or duplicate it first.
	bool bDoCompressionInPlace;

public:
	FDerivedDataAnimationCompression(UAnimSequence* InAnimSequence, TSharedPtr<FAnimCompressContext> InCompressContext, bool bInDoCompressionInPlace);
	virtual ~FDerivedDataAnimationCompression();

	virtual const TCHAR* GetPluginName() const override
	{
		return TEXT("AnimSeq");
	}

	virtual const TCHAR* GetVersionString() const override
	{
		// This is a version string that mimics the old versioning scheme. If you
		// want to bump this version, generate a new guid using VS->Tools->Create GUID and
		// return it here. Ex.
		return TEXT("44870622F1374957B3252BC65DDCFA9F");
	}

	virtual FString GetPluginSpecificCacheKeySuffix() const override;


	virtual bool IsBuildThreadsafe() const override
	{
		return false;
	}

	virtual bool Build( TArray<uint8>& OutData ) override;

	/** Return true if we can build **/
	bool CanBuild()
	{
		return !!OriginalAnimSequence;
	}
};

#endif	//WITH_EDITOR
