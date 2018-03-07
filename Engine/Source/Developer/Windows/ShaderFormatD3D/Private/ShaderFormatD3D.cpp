// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ShaderFormatD3D.h"
#include "ModuleInterface.h"
#include "ModuleManager.h"
#include "Interfaces/IShaderFormat.h"
#include "Interfaces/IShaderFormatModule.h"

static FName NAME_PCD3D_SM5(TEXT("PCD3D_SM5"));
static FName NAME_PCD3D_SM4(TEXT("PCD3D_SM4"));
static FName NAME_PCD3D_ES3_1(TEXT("PCD3D_ES31"));
static FName NAME_PCD3D_ES2(TEXT("PCD3D_ES2"));

class FShaderFormatD3D : public IShaderFormat
{
	enum
	{
		/** Version for shader format, this becomes part of the DDC key. */
		UE_SHADER_PCD3D_SM5_VER = 7,
		UE_SHADER_PCD3D_SM4_VER = 7,
		UE_SHADER_PCD3D_ES2_VER = 7,
		UE_SHADER_PCD3D_ES3_1_VER = 7,
	};

	void CheckFormat(FName Format) const
	{
		check(Format == NAME_PCD3D_SM5 ||  Format == NAME_PCD3D_SM4 ||  Format == NAME_PCD3D_ES2 || Format == NAME_PCD3D_ES3_1);
	}

public:
	virtual uint32 GetVersion(FName Format) const override
	{
		CheckFormat(Format);
		if (Format == NAME_PCD3D_SM5) 
		{
			return UE_SHADER_PCD3D_SM5_VER;
		}
		else if (Format == NAME_PCD3D_SM4) 
		{
			return UE_SHADER_PCD3D_SM4_VER;
		}
		else if (Format == NAME_PCD3D_ES3_1) 
		{
			return UE_SHADER_PCD3D_ES3_1_VER;
		}
		else if (Format == NAME_PCD3D_ES2)
		{
			return UE_SHADER_PCD3D_ES2_VER;
		}
		check(0);
		return 0;
	}
	virtual void GetSupportedFormats(TArray<FName>& OutFormats) const
	{
		OutFormats.Add(NAME_PCD3D_SM5);
		OutFormats.Add(NAME_PCD3D_SM4);
		OutFormats.Add(NAME_PCD3D_ES3_1);
		OutFormats.Add(NAME_PCD3D_ES2);
	}
	virtual void CompileShader(FName Format, const struct FShaderCompilerInput& Input, struct FShaderCompilerOutput& Output,const FString& WorkingDirectory) const
	{
		CheckFormat(Format);
		if (Format == NAME_PCD3D_SM5)
		{
			CompileShader_Windows_SM5(Input, Output, WorkingDirectory);
		}
		else if (Format == NAME_PCD3D_SM4)
		{
			CompileShader_Windows_SM4(Input, Output, WorkingDirectory);
		}
		else if (Format == NAME_PCD3D_ES2)
		{
			CompileShader_Windows_ES2(Input, Output, WorkingDirectory);
		}
		else if (Format == NAME_PCD3D_ES3_1)
		{
			CompileShader_Windows_ES3_1(Input, Output, WorkingDirectory);
		}
		else
		{
			check(0);
		}
	}
};


/**
 * Module for D3D shaders
 */

static IShaderFormat* Singleton = NULL;

class FShaderFormatD3DModule : public IShaderFormatModule
{
public:
	virtual ~FShaderFormatD3DModule()
	{
		delete Singleton;
		Singleton = NULL;
	}
	virtual IShaderFormat* GetShaderFormat()
	{
		if (!Singleton)
		{
			Singleton = new FShaderFormatD3D();
		}
		return Singleton;
	}
};

IMPLEMENT_MODULE( FShaderFormatD3DModule, ShaderFormatD3D);
