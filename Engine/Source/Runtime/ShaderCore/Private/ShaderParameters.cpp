// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ShaderParameters.cpp: Shader parameter implementation.
=============================================================================*/

#include "ShaderParameters.h"
#include "Containers/List.h"
#include "UniformBuffer.h"
#include "ShaderCore.h"
#include "Shader.h"
#include "VertexFactory.h"

void FShaderParameter::Bind(const FShaderParameterMap& ParameterMap,const TCHAR* ParameterName,EShaderParameterFlags Flags)
{
#if UE_BUILD_DEBUG
	bInitialized = true;
#endif

	if (!ParameterMap.FindParameterAllocation(ParameterName,BufferIndex,BaseIndex,NumBytes) && Flags == SPF_Mandatory)
	{
		if (!UE_LOG_ACTIVE(LogShaders, Log))
		{
			UE_LOG(LogShaders, Fatal,TEXT("Failure to bind non-optional shader parameter %s!  The parameter is either not present in the shader, or the shader compiler optimized it out."),ParameterName);
		}
		else
		{
			// We use a non-Slate message box to avoid problem where we haven't compiled the shaders for Slate.
			FPlatformMisc::MessageBoxExt( EAppMsgType::Ok, *FText::Format(
				NSLOCTEXT("UnrealEd", "Error_FailedToBindShaderParameter", "Failure to bind non-optional shader parameter {0}! The parameter is either not present in the shader, or the shader compiler optimized it out. This will be an assert with LogShaders suppressed!"),
				FText::FromString(ParameterName)).ToString(), TEXT("Warning"));
		}
	}
}

FArchive& operator<<(FArchive& Ar,FShaderParameter& P)
{
#if UE_BUILD_DEBUG
	if (Ar.IsLoading())
	{
		P.bInitialized = true;
	}
#endif

	uint16& PBufferIndex = P.BufferIndex;
	return Ar << P.BaseIndex << P.NumBytes << PBufferIndex;
}

void FShaderResourceParameter::Bind(const FShaderParameterMap& ParameterMap,const TCHAR* ParameterName,EShaderParameterFlags Flags)
{
	uint16 UnusedBufferIndex = 0;

#if UE_BUILD_DEBUG
	bInitialized = true;
#endif

	if(!ParameterMap.FindParameterAllocation(ParameterName,UnusedBufferIndex,BaseIndex,NumResources) && Flags == SPF_Mandatory)
	{
		if (!UE_LOG_ACTIVE(LogShaders, Log))
		{
			UE_LOG(LogShaders, Fatal,TEXT("Failure to bind non-optional shader resource parameter %s!  The parameter is either not present in the shader, or the shader compiler optimized it out."),ParameterName);
		}
		else
		{
			// We use a non-Slate message box to avoid problem where we haven't compiled the shaders for Slate.
			FPlatformMisc::MessageBoxExt( EAppMsgType::Ok, *FText::Format(
				NSLOCTEXT("UnrealEd", "Error_FailedToBindShaderParameter", "Failure to bind non-optional shader parameter {0}! The parameter is either not present in the shader, or the shader compiler optimized it out. This will be an assert with LogShaders suppressed!"),
				FText::FromString(ParameterName)).ToString(), TEXT("Warning"));
		}
	}
}

FArchive& operator<<(FArchive& Ar,FShaderResourceParameter& P)
{
#if UE_BUILD_DEBUG
	if (Ar.IsLoading())
	{
		P.bInitialized = true;
	}
#endif

	return Ar << P.BaseIndex << P.NumResources;
}

void FShaderUniformBufferParameter::ModifyCompilationEnvironment(const TCHAR* ParameterName,const FUniformBufferStruct& Struct,EShaderPlatform Platform,FShaderCompilerEnvironment& OutEnvironment)
{
	const FString IncludeName = FString::Printf(TEXT("/Engine/Generated/UniformBuffers/%s.ush"),ParameterName);
	// Add the uniform buffer declaration to the compilation environment as an include: UniformBuffers/<ParameterName>.usf
	FString Declaration = CreateUniformBufferShaderDeclaration(ParameterName,Struct,Platform);
	OutEnvironment.IncludeVirtualPathToContentsMap.Add(IncludeName, StringToArray<ANSICHAR>(*Declaration, Declaration.Len() + 1));

	TArray<ANSICHAR>& GeneratedUniformBuffersInclude = OutEnvironment.IncludeVirtualPathToContentsMap.FindOrAdd("/Engine/Generated/GeneratedUniformBuffers.ush");
	FString Include = FString::Printf(TEXT("#include \"/Engine/Generated/UniformBuffers/%s.ush\"") LINE_TERMINATOR, ParameterName);
	if (GeneratedUniformBuffersInclude.Num() > 0)
	{
		GeneratedUniformBuffersInclude.RemoveAt(GeneratedUniformBuffersInclude.Num() - 1);
	}
	GeneratedUniformBuffersInclude.Append(StringToArray<ANSICHAR>(*Include, Include.Len() + 1));
	Struct.AddResourceTableEntries(OutEnvironment.ResourceTableMap, OutEnvironment.ResourceTableLayoutHashes);
}

void FShaderUniformBufferParameter::Bind(const FShaderParameterMap& ParameterMap,const TCHAR* ParameterName,EShaderParameterFlags Flags)
{
	uint16 UnusedBaseIndex = 0;
	uint16 UnusedNumBytes = 0;

#if UE_BUILD_DEBUG
	bInitialized = true;
#endif

	if(!ParameterMap.FindParameterAllocation(ParameterName,BaseIndex,UnusedBaseIndex,UnusedNumBytes))
	{
		bIsBound = false;
		if(Flags == SPF_Mandatory)
		{
			if (!UE_LOG_ACTIVE(LogShaders, Log))
			{
				UE_LOG(LogShaders, Fatal,TEXT("Failure to bind non-optional shader resource parameter %s!  The parameter is either not present in the shader, or the shader compiler optimized it out."),ParameterName);
			}
			else
			{
				// We use a non-Slate message box to avoid problem where we haven't compiled the shaders for Slate.
				FPlatformMisc::MessageBoxExt( EAppMsgType::Ok, *FText::Format(
					NSLOCTEXT("UnrealEd", "Error_FailedToBindShaderParameter", "Failure to bind non-optional shader parameter {0}! The parameter is either not present in the shader, or the shader compiler optimized it out. This will be an assert with LogShaders suppressed!"),
					FText::FromString(ParameterName)).ToString(), TEXT("Warning"));
			}
		}
	}
	else
	{
		bIsBound = true;
	}
}

/** The individual bits of a uniform buffer declaration. */
struct FUniformBufferDecl
{
	/** Members to place in the constant buffer. */
	FString ConstantBufferMembers;
	/** Members to place in the resource table. */
	FString ResourceMembers;
	/** Members in the struct HLSL shader code will access. */
	FString StructMembers;
	/** The HLSL initializer that will copy constants and resources in to the struct. */
	FString Initializer;
};

/** Generates a HLSL struct declaration for a uniform buffer struct. */
static void CreateHLSLUniformBufferStructMembersDeclaration(FUniformBufferDecl& Decl, const FUniformBufferStruct& UniformBufferStruct, const FString& NamePrefix, bool bExplicitPadding)
{
	uint32 HLSLBaseOffset = 0;
	bool bFoundResourceMember = false;
	int32 MemberIndex = 0;
	const TArray<FUniformBufferStruct::FMember>& StructMembers = UniformBufferStruct.GetMembers();
	
	Decl.Initializer += TEXT("{");
	int32 OpeningBraceLocPlusOne = Decl.Initializer.Len();

	for(;MemberIndex < StructMembers.Num();++MemberIndex)
	{
		const FUniformBufferStruct::FMember& Member = StructMembers[MemberIndex];
		
		if (IsUniformBufferResourceType(Member.GetBaseType()))
		{
			bFoundResourceMember = true;
			break;
		}

		FString ArrayDim;
		if(Member.GetNumElements() > 0)
		{
			ArrayDim = FString::Printf(TEXT("[%u]"),Member.GetNumElements());
		}

		if(Member.GetBaseType() == UBMT_STRUCT)
		{
			Decl.StructMembers += TEXT("struct {\r\n");
			Decl.Initializer += TEXT(",");
			CreateHLSLUniformBufferStructMembersDeclaration(Decl, *Member.GetStruct(), FString::Printf(TEXT("%s%s_"), *NamePrefix, Member.GetName()), bExplicitPadding);
			Decl.StructMembers += FString::Printf(TEXT("} %s%s;\r\n"),Member.GetName(),*ArrayDim);
			HLSLBaseOffset += Member.GetStruct()->GetSize() * Member.GetNumElements();
		}
		else
		{
			// Generate the base type name.
			FString BaseTypeName;
			switch(Member.GetBaseType())
			{
			case UBMT_BOOL:    BaseTypeName = TEXT("bool"); break;
			case UBMT_INT32:   BaseTypeName = TEXT("int"); break;
			case UBMT_UINT32:  BaseTypeName = TEXT("uint"); break;
			case UBMT_FLOAT32: 
				if (Member.GetPrecision() == EShaderPrecisionModifier::Float)
				{
					BaseTypeName = TEXT("float"); 
				}
				else if (Member.GetPrecision() == EShaderPrecisionModifier::Half)
				{
					BaseTypeName = TEXT("half"); 
				}
				else if (Member.GetPrecision() == EShaderPrecisionModifier::Fixed)
				{
					BaseTypeName = TEXT("fixed"); 
				}
				break;
			default:           UE_LOG(LogShaders, Fatal,TEXT("Unrecognized uniform buffer struct member base type."));
			};

			// Generate the type dimensions for vectors and matrices.
			FString TypeDim;
			uint32 HLSLMemberSize = 4;
			if(Member.GetNumRows() > 1)
			{
				TypeDim = FString::Printf(TEXT("%ux%u"),Member.GetNumRows(),Member.GetNumColumns());

				// Each row of a matrix is 16 byte aligned.
				HLSLMemberSize = (Member.GetNumRows() - 1) * 16 + Member.GetNumColumns() * 4;
			}
			else if(Member.GetNumColumns() > 1)
			{
				TypeDim = FString::Printf(TEXT("%u"),Member.GetNumColumns());
				HLSLMemberSize = Member.GetNumColumns() * 4;
			}

			// Array elements are 16 byte aligned.
			if(Member.GetNumElements() > 0)
			{
				HLSLMemberSize = (Member.GetNumElements() - 1) * Align(HLSLMemberSize,16) + HLSLMemberSize;
			}

			// If the HLSL offset doesn't match the C++ offset, generate padding to fix it.
			if(HLSLBaseOffset != Member.GetOffset())
			{
				check(HLSLBaseOffset < Member.GetOffset());
				while(HLSLBaseOffset < Member.GetOffset())
				{
					if(bExplicitPadding)
					{
						Decl.ConstantBufferMembers += FString::Printf(TEXT("\tfloat1 _%sPrePadding%u;\r\n"), *NamePrefix, HLSLBaseOffset);
					}
					HLSLBaseOffset += 4;
				};
				check(HLSLBaseOffset == Member.GetOffset());
			}

			HLSLBaseOffset = Member.GetOffset() + HLSLMemberSize;

			// Generate the member declaration.
			FString ParameterName = FString::Printf(TEXT("%s%s"),*NamePrefix,Member.GetName());
			Decl.ConstantBufferMembers += FString::Printf(TEXT("\t%s%s %s%s;\r\n"),*BaseTypeName,*TypeDim,*ParameterName,*ArrayDim);
			Decl.StructMembers += FString::Printf(TEXT("\t%s%s %s%s;\r\n"),*BaseTypeName,*TypeDim,Member.GetName(),*ArrayDim);
			Decl.Initializer += FString::Printf(TEXT(",%s"),*ParameterName);
		}
	}

	for(;MemberIndex < StructMembers.Num();++MemberIndex)
	{
		const FUniformBufferStruct::FMember& Member = StructMembers[MemberIndex];

		if (!IsUniformBufferResourceType(Member.GetBaseType()))
		{
			check(!bFoundResourceMember);
			continue;
		}
		bFoundResourceMember = true;

		// TODO: handle arrays?
		FString ParameterName = FString::Printf(TEXT("%s%s"),*NamePrefix,Member.GetName());
		Decl.ResourceMembers += FString::Printf(TEXT("%s %s;\r\n"), Member.GetShaderType(), *ParameterName);
		Decl.StructMembers += FString::Printf(TEXT("\t%s %s;\r\n"), Member.GetShaderType(), Member.GetName());
		Decl.Initializer += FString::Printf(TEXT(",%s"), *ParameterName);
	}

	Decl.Initializer += TEXT("}");
	if (Decl.Initializer[OpeningBraceLocPlusOne] == TEXT(','))
	{
		Decl.Initializer[OpeningBraceLocPlusOne] = TEXT(' ');
	}
}

/** Creates a HLSL declaration of a uniform buffer with the given structure. */
static FString CreateHLSLUniformBufferDeclaration(const TCHAR* Name,const FUniformBufferStruct& UniformBufferStruct, bool bExplicitPadding)
{
	// If the uniform buffer has no members, we don't want to write out anything.  Shader compilers throw errors when faced with empty cbuffers and structs.
	if (UniformBufferStruct.GetMembers().Num() > 0)
{
		FString NamePrefix(FString(Name) + FString(TEXT("_")));
		FUniformBufferDecl Decl;
		CreateHLSLUniformBufferStructMembersDeclaration(Decl, UniformBufferStruct, NamePrefix, bExplicitPadding);

	return FString::Printf(
		TEXT("#ifndef __UniformBuffer_%s_Definition__\r\n")
		TEXT("#define __UniformBuffer_%s_Definition__\r\n")
		TEXT("cbuffer %s\r\n")
		TEXT("{\r\n")
		TEXT("%s")
		TEXT("}\r\n")
			TEXT("%s")
			TEXT("static const struct\r\n")
			TEXT("{\r\n")
			TEXT("%s")
			TEXT("} %s = %s;\r\n")
		TEXT("#endif\r\n"),
		Name,
		Name,
		Name,
			*Decl.ConstantBufferMembers,
			*Decl.ResourceMembers,
			*Decl.StructMembers,
			Name,
			*Decl.Initializer,
		Name
		);
}

	return FString(TEXT("\n"));
}

FString CreateUniformBufferShaderDeclaration(const TCHAR* Name,const FUniformBufferStruct& UniformBufferStruct,EShaderPlatform Platform)
{
	switch(Platform)
	{
		case SP_OPENGL_ES3_1_ANDROID:
		case SP_OPENGL_ES31_EXT:
		case SP_OPENGL_SM4:
		case SP_OPENGL_SM5:
		case SP_SWITCH:
			return CreateHLSLUniformBufferDeclaration(Name, UniformBufferStruct, false);
		case SP_PCD3D_SM5:
		default:
			return CreateHLSLUniformBufferDeclaration(Name, UniformBufferStruct, true);
	}
}

void CacheUniformBufferIncludes(TMap<const TCHAR*,FCachedUniformBufferDeclaration>& Cache, EShaderPlatform Platform)
{
	for (TMap<const TCHAR*,FCachedUniformBufferDeclaration>::TIterator It(Cache); It; ++It)
	{
		FCachedUniformBufferDeclaration& BufferDeclaration = It.Value();
		check(BufferDeclaration.Declaration[Platform].Len() == 0);

		for (TLinkedList<FUniformBufferStruct*>::TIterator StructIt(FUniformBufferStruct::GetStructList()); StructIt; StructIt.Next())
		{
			if (It.Key() == StructIt->GetShaderVariableName())
			{
				BufferDeclaration.Declaration[Platform] = CreateUniformBufferShaderDeclaration(StructIt->GetShaderVariableName(), **StructIt, Platform);
				break;
			}
		}
	}
}

void FShaderType::AddReferencedUniformBufferIncludes(FShaderCompilerEnvironment& OutEnvironment, FString& OutSourceFilePrefix, EShaderPlatform Platform)
{
	// Cache uniform buffer struct declarations referenced by this shader type's files
	if (!bCachedUniformBufferStructDeclarations[Platform])
	{
		CacheUniformBufferIncludes(ReferencedUniformBufferStructsCache, Platform);
		bCachedUniformBufferStructDeclarations[Platform] = true;
	}

	FString UniformBufferIncludes;

	for (TMap<const TCHAR*,FCachedUniformBufferDeclaration>::TConstIterator It(ReferencedUniformBufferStructsCache); It; ++It)
	{
		check(It.Value().Declaration[Platform].Len() > 0);
		UniformBufferIncludes += FString::Printf(TEXT("#include \"/Engine/Generated/UniformBuffers/%s.ush\"") LINE_TERMINATOR, It.Key());
		FString Declaration = It.Value().Declaration[Platform];
		OutEnvironment.IncludeVirtualPathToContentsMap.Add(
			*FString::Printf(TEXT("/Engine/Generated/UniformBuffers/%s.ush"),It.Key()),
			StringToArray<ANSICHAR>(*Declaration, Declaration.Len() + 1)
			);

		for (TLinkedList<FUniformBufferStruct*>::TIterator StructIt(FUniformBufferStruct::GetStructList()); StructIt; StructIt.Next())
		{
			if (It.Key() == StructIt->GetShaderVariableName())
			{
				StructIt->AddResourceTableEntries(OutEnvironment.ResourceTableMap, OutEnvironment.ResourceTableLayoutHashes);
			}
		}
	}

	TArray<ANSICHAR>& GeneratedUniformBuffersInclude = OutEnvironment.IncludeVirtualPathToContentsMap.FindOrAdd("/Engine/Generated/GeneratedUniformBuffers.ush");
	if (GeneratedUniformBuffersInclude.Num() > 0)
	{
		GeneratedUniformBuffersInclude.RemoveAt(GeneratedUniformBuffersInclude.Num() - 1);
	}
	GeneratedUniformBuffersInclude.Append(StringToArray<ANSICHAR>(*UniformBufferIncludes, UniformBufferIncludes.Len() + 1));
}

void FVertexFactoryType::AddReferencedUniformBufferIncludes(FShaderCompilerEnvironment& OutEnvironment, FString& OutSourceFilePrefix, EShaderPlatform Platform)
{
	// Cache uniform buffer struct declarations referenced by this shader type's files
	if (!bCachedUniformBufferStructDeclarations[Platform])
	{
		CacheUniformBufferIncludes(ReferencedUniformBufferStructsCache, Platform);
		bCachedUniformBufferStructDeclarations[Platform] = true;
	}

	FString UniformBufferIncludes;

	for (TMap<const TCHAR*,FCachedUniformBufferDeclaration>::TConstIterator It(ReferencedUniformBufferStructsCache); It; ++It)
	{
		check(It.Value().Declaration[Platform].Len() > 0);
		UniformBufferIncludes += FString::Printf(TEXT("#include \"/Engine/Generated/UniformBuffers/%s.ush\"") LINE_TERMINATOR, It.Key());
		FString Declaration = It.Value().Declaration[Platform];
		OutEnvironment.IncludeVirtualPathToContentsMap.Add(
			*FString::Printf(TEXT("/Engine/Generated/UniformBuffers/%s.ush"),It.Key()),
			StringToArray<ANSICHAR>(*Declaration, Declaration.Len() + 1)
		);

		for (TLinkedList<FUniformBufferStruct*>::TIterator StructIt(FUniformBufferStruct::GetStructList()); StructIt; StructIt.Next())
		{
			if (It.Key() == StructIt->GetShaderVariableName())
			{
				StructIt->AddResourceTableEntries(OutEnvironment.ResourceTableMap, OutEnvironment.ResourceTableLayoutHashes);
			}
		}
	}

	TArray<ANSICHAR>& GeneratedUniformBuffersInclude = OutEnvironment.IncludeVirtualPathToContentsMap.FindOrAdd("/Engine/Generated/GeneratedUniformBuffers.ush");
	if (GeneratedUniformBuffersInclude.Num() > 0)
	{
		GeneratedUniformBuffersInclude.RemoveAt(GeneratedUniformBuffersInclude.Num() - 1);
	}
	GeneratedUniformBuffersInclude.Append(StringToArray<ANSICHAR>(*UniformBufferIncludes, UniformBufferIncludes.Len() + 1));
}
