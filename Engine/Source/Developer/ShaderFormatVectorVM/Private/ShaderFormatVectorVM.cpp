// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
//

#include "ShaderFormatVectorVM.h"
#include "CoreMinimal.h"
#include "ModuleInterface.h"
#include "ModuleManager.h"
#include "IShaderFormat.h"
#include "IShaderFormatModule.h"
#include "hlslcc.h"
#include "ShaderCore.h"


//TODO: Much of the below is some partial work I did towards having the VVM be treat just as a shader platform.
//This seems like a reasonable way to go but I'm not 100% certain yet.

static FName NAME_VVM_1_0(TEXT("VVM_1_0"));

class FShaderFormatVectorVM : public IShaderFormat
{
	enum class VectorVMFormats : uint8
	{
		VVM_1_0,
	};

	void CheckFormat(FName Format) const
	{
		check(Format == NAME_VVM_1_0);
	}

public:
	virtual uint32 GetVersion(FName Format) const override
	{
		CheckFormat(Format);
		uint32 VVMVersion = 0;
		if (Format == NAME_VVM_1_0)
		{
			VVMVersion = (int32)VectorVMFormats::VVM_1_0;
		}
		else
		{
			check(0);
		}
		const uint16 Version = ((HLSLCC_VersionMinor & 0xff) << 8) | (VVMVersion & 0xff);
		return Version;
	}
	virtual void GetSupportedFormats(TArray<FName>& OutFormats) const override
	{
		OutFormats.Add(NAME_VVM_1_0);
	}

	virtual void CompileShader(FName Format, const struct FShaderCompilerInput& Input, struct FShaderCompilerOutput& Output,const FString& WorkingDirectory) const override
	{
		CheckFormat(Format);

		if (Format == NAME_VVM_1_0)
		{
			CompileShader_VectorVM(Input, Output, WorkingDirectory, (int8)VectorVMFormats::VVM_1_0);
		}
		else
		{
			check(0);
		}
	}
};

/**
 * Module for VectorVM shaders
 */

static IShaderFormat* Singleton = NULL;

class FShaderFormatVectorVMModule : public IShaderFormatModule
{
public:
	virtual ~FShaderFormatVectorVMModule()
	{
		delete Singleton;
		Singleton = NULL;
	}
	virtual IShaderFormat* GetShaderFormat()
	{
		if (!Singleton)
		{
			Singleton = new FShaderFormatVectorVM();
		}
		return Singleton;
	}
};

IMPLEMENT_MODULE( FShaderFormatVectorVMModule, ShaderFormatVectorVM);
