// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MaterialShared.cpp: Shared material implementation.
=============================================================================*/

#include "Materials/MaterialUniformExpressions.h"
#include "SceneManagement.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceSupport.h"
#include "Materials/MaterialParameterCollection.h"
#include "ExternalTexture.h"
#include "MessageLog.h"
#include "UObjectToken.h"

TLinkedList<FMaterialUniformExpressionType*>*& FMaterialUniformExpressionType::GetTypeList()
{
	static TLinkedList<FMaterialUniformExpressionType*>* TypeList = NULL;
	return TypeList;
}

TMap<FName,FMaterialUniformExpressionType*>& FMaterialUniformExpressionType::GetTypeMap()
{
	static TMap<FName,FMaterialUniformExpressionType*> TypeMap;

	// Move types from the type list to the type map.
	TLinkedList<FMaterialUniformExpressionType*>* TypeListLink = GetTypeList();
	while(TypeListLink)
	{
		TLinkedList<FMaterialUniformExpressionType*>* NextLink = TypeListLink->Next();
		FMaterialUniformExpressionType* Type = **TypeListLink;

		TypeMap.Add(FName(Type->Name),Type);
		TypeListLink->Unlink();
		delete TypeListLink;

		TypeListLink = NextLink;
	}

	return TypeMap;
}

FMaterialUniformExpressionType::FMaterialUniformExpressionType(
	const TCHAR* InName,
	SerializationConstructorType InSerializationConstructor
	):
	Name(InName),
	SerializationConstructor(InSerializationConstructor)
{
	// Put the type in the type list until the name subsystem/type map are initialized.
	(new TLinkedList<FMaterialUniformExpressionType*>(this))->LinkHead(GetTypeList());
}

FArchive& operator<<(FArchive& Ar,FMaterialUniformExpression*& Ref)
{
	// Serialize the expression type.
	if(Ar.IsSaving())
	{
		// Write the type name.
		check(Ref);
		FName TypeName(Ref->GetType()->Name);
		Ar << TypeName;
	}
	else if(Ar.IsLoading())
	{
		// Read the type name.
		FName TypeName = NAME_None;
		Ar << TypeName;

		// Find the expression type with a matching name.
		FMaterialUniformExpressionType* Type = FMaterialUniformExpressionType::GetTypeMap().FindRef(TypeName);
		checkf(Type, TEXT("Unable to find FMaterialUniformExpressionType for TypeName '%s'"), *TypeName.ToString());

		// Construct a new instance of the expression type.
		Ref = (*Type->SerializationConstructor)();
	}

	// Serialize the expression.
	Ref->Serialize(Ar);

	return Ar;
}

FArchive& operator<<(FArchive& Ar,FMaterialUniformExpressionTexture*& Ref)
{
	Ar << (FMaterialUniformExpression*&)Ref;
	return Ar;
}

void FUniformExpressionSet::Serialize(FArchive& Ar)
{
	Ar << UniformVectorExpressions;
	Ar << UniformScalarExpressions;
	Ar << Uniform2DTextureExpressions;
	Ar << UniformCubeTextureExpressions;
	Ar << UniformExternalTextureExpressions;

	Ar << ParameterCollections;

	Ar << PerFrameUniformScalarExpressions;
	Ar << PerFrameUniformVectorExpressions;
	Ar << PerFramePrevUniformScalarExpressions;
	Ar << PerFramePrevUniformVectorExpressions;

	// Recreate the uniform buffer struct after loading.
	if(Ar.IsLoading())
	{
		CreateBufferStruct();
	}
}

bool FUniformExpressionSet::IsEmpty() const
{
	return UniformVectorExpressions.Num() == 0
		&& UniformScalarExpressions.Num() == 0
		&& Uniform2DTextureExpressions.Num() == 0
		&& UniformCubeTextureExpressions.Num() == 0
		&& UniformExternalTextureExpressions.Num() == 0
		&& PerFrameUniformScalarExpressions.Num() == 0
		&& PerFrameUniformVectorExpressions.Num() == 0
		&& PerFramePrevUniformScalarExpressions.Num() == 0
		&& PerFramePrevUniformVectorExpressions.Num() == 0
		&& ParameterCollections.Num() == 0;
}

bool FUniformExpressionSet::operator==(const FUniformExpressionSet& ReferenceSet) const
{
	if (UniformVectorExpressions.Num() != ReferenceSet.UniformVectorExpressions.Num()
		|| UniformScalarExpressions.Num() != ReferenceSet.UniformScalarExpressions.Num()
		|| Uniform2DTextureExpressions.Num() != ReferenceSet.Uniform2DTextureExpressions.Num()
		|| UniformCubeTextureExpressions.Num() != ReferenceSet.UniformCubeTextureExpressions.Num()
		|| UniformExternalTextureExpressions.Num() != ReferenceSet.UniformExternalTextureExpressions.Num()
		|| PerFrameUniformScalarExpressions.Num() != ReferenceSet.PerFrameUniformScalarExpressions.Num()
		|| PerFrameUniformVectorExpressions.Num() != ReferenceSet.PerFrameUniformVectorExpressions.Num()
		|| PerFramePrevUniformScalarExpressions.Num() != ReferenceSet.PerFramePrevUniformScalarExpressions.Num()
		|| PerFramePrevUniformVectorExpressions.Num() != ReferenceSet.PerFramePrevUniformVectorExpressions.Num()
		|| ParameterCollections.Num() != ReferenceSet.ParameterCollections.Num())
	{
		return false;
	}

	for (int32 i = 0; i < UniformVectorExpressions.Num(); i++)
	{
		if (!UniformVectorExpressions[i]->IsIdentical(ReferenceSet.UniformVectorExpressions[i]))
		{
			return false;
		}
	}

	for (int32 i = 0; i < UniformScalarExpressions.Num(); i++)
	{
		if (!UniformScalarExpressions[i]->IsIdentical(ReferenceSet.UniformScalarExpressions[i]))
		{
			return false;
		}
	}

	for (int32 i = 0; i < Uniform2DTextureExpressions.Num(); i++)
	{
		if (!Uniform2DTextureExpressions[i]->IsIdentical(ReferenceSet.Uniform2DTextureExpressions[i]))
		{
			return false;
		}
	}

	for (int32 i = 0; i < UniformCubeTextureExpressions.Num(); i++)
	{
		if (!UniformCubeTextureExpressions[i]->IsIdentical(ReferenceSet.UniformCubeTextureExpressions[i]))
		{
			return false;
		}
	}

	for (int32 i = 0; i < UniformExternalTextureExpressions.Num(); i++)
	{
		if (!UniformExternalTextureExpressions[i]->IsIdentical(ReferenceSet.UniformExternalTextureExpressions[i]))
		{
			return false;
		}
	}

	for (int32 i = 0; i < PerFrameUniformScalarExpressions.Num(); i++)
	{
		if (!PerFrameUniformScalarExpressions[i]->IsIdentical(ReferenceSet.PerFrameUniformScalarExpressions[i]))
		{
			return false;
		}
	}

	for (int32 i = 0; i < PerFrameUniformVectorExpressions.Num(); i++)
	{
		if (!PerFrameUniformVectorExpressions[i]->IsIdentical(ReferenceSet.PerFrameUniformVectorExpressions[i]))
		{
			return false;
		}
	}

	for (int32 i = 0; i < PerFramePrevUniformScalarExpressions.Num(); i++)
	{
		if (!PerFramePrevUniformScalarExpressions[i]->IsIdentical(ReferenceSet.PerFramePrevUniformScalarExpressions[i]))
		{
			return false;
		}
	}

	for (int32 i = 0; i < PerFramePrevUniformVectorExpressions.Num(); i++)
	{
		if (!PerFramePrevUniformVectorExpressions[i]->IsIdentical(ReferenceSet.PerFramePrevUniformVectorExpressions[i]))
		{
			return false;
		}
	}

	for (int32 i = 0; i < ParameterCollections.Num(); i++)
	{
		if (ParameterCollections[i] != ReferenceSet.ParameterCollections[i])
		{
			return false;
		}
	}

	return true;
}

FString FUniformExpressionSet::GetSummaryString() const
{
	return FString::Printf(TEXT("(%u vectors, %u scalars, %u 2d tex, %u cube tex, %u external tex, %u scalars/frame, %u vectors/frame, %u collections)"),
		UniformVectorExpressions.Num(), 
		UniformScalarExpressions.Num(),
		Uniform2DTextureExpressions.Num(),
		UniformCubeTextureExpressions.Num(),
		UniformExternalTextureExpressions.Num(),
		PerFrameUniformScalarExpressions.Num(),
		PerFrameUniformVectorExpressions.Num(),
		ParameterCollections.Num()
		);
}

void FUniformExpressionSet::SetParameterCollections(const TArray<UMaterialParameterCollection*>& InCollections)
{
	ParameterCollections.Empty(InCollections.Num());

	for (int32 CollectionIndex = 0; CollectionIndex < InCollections.Num(); CollectionIndex++)
	{
		ParameterCollections.Add(InCollections[CollectionIndex]->StateId);
	}
}

static FShaderUniformBufferParameter* ConstructMaterialUniformBufferParameter()
{
	return nullptr;
}


static FName MaterialLayoutName(TEXT("Material"));

void FUniformExpressionSet::CreateBufferStruct()
{
	// Make sure FUniformExpressionSet::CreateDebugLayout() is in sync
	TArray<FUniformBufferStruct::FMember> Members;
	uint32 NextMemberOffset = 0;

	if (UniformVectorExpressions.Num())
	{
		new(Members) FUniformBufferStruct::FMember(TEXT("VectorExpressions"),TEXT(""),NextMemberOffset,UBMT_FLOAT32,EShaderPrecisionModifier::Half,1,4,UniformVectorExpressions.Num(),NULL);
		const uint32 VectorArraySize = UniformVectorExpressions.Num() * sizeof(FVector4);
		NextMemberOffset += VectorArraySize;
	}

	if (UniformScalarExpressions.Num())
	{
		new(Members) FUniformBufferStruct::FMember(TEXT("ScalarExpressions"),TEXT(""),NextMemberOffset,UBMT_FLOAT32,EShaderPrecisionModifier::Half,1,4,(UniformScalarExpressions.Num() + 3) / 4,NULL);
		const uint32 ScalarArraySize = (UniformScalarExpressions.Num() + 3) / 4 * sizeof(FVector4);
		NextMemberOffset += ScalarArraySize;
	}

	static FString Texture2DNames[128];
	static FString Texture2DSamplerNames[128];
	static FString TextureCubeNames[128];
	static FString TextureCubeSamplerNames[128];
	static FString ExternalTextureNames[128];
	static FString MediaTextureSamplerNames[128];
	static bool bInitializedTextureNames = false;
	if (!bInitializedTextureNames)
	{
		bInitializedTextureNames = true;
		for (int32 i = 0; i < 128; ++i)
		{
			Texture2DNames[i] = FString::Printf(TEXT("Texture2D_%d"), i);
			Texture2DSamplerNames[i] = FString::Printf(TEXT("Texture2D_%dSampler"), i);
			TextureCubeNames[i] = FString::Printf(TEXT("TextureCube_%d"), i);
			TextureCubeSamplerNames[i] = FString::Printf(TEXT("TextureCube_%dSampler"), i);
			ExternalTextureNames[i] = FString::Printf(TEXT("ExternalTexture_%d"), i);
			MediaTextureSamplerNames[i] = FString::Printf(TEXT("ExternalTexture_%dSampler"), i);
		}
	}

	check(Uniform2DTextureExpressions.Num() <= 128);
	check(UniformCubeTextureExpressions.Num() <= 128);

	for (int32 i = 0; i < Uniform2DTextureExpressions.Num(); ++i)
	{
		check((NextMemberOffset & 0x7) == 0);
		new(Members) FUniformBufferStruct::FMember(*Texture2DNames[i],TEXT("Texture2D"),NextMemberOffset,UBMT_TEXTURE,EShaderPrecisionModifier::Float,1,1,1,NULL);
		NextMemberOffset += 8;
		new(Members) FUniformBufferStruct::FMember(*Texture2DSamplerNames[i],TEXT("SamplerState"),NextMemberOffset,UBMT_SAMPLER,EShaderPrecisionModifier::Float,1,1,1,NULL);
		NextMemberOffset += 8;
	}

	for (int32 i = 0; i < UniformCubeTextureExpressions.Num(); ++i)
	{
		check((NextMemberOffset & 0x7) == 0);
		new(Members) FUniformBufferStruct::FMember(*TextureCubeNames[i],TEXT("TextureCube"),NextMemberOffset,UBMT_TEXTURE,EShaderPrecisionModifier::Float,1,1,1,NULL);
		NextMemberOffset += 8;
		new(Members) FUniformBufferStruct::FMember(*TextureCubeSamplerNames[i],TEXT("SamplerState"),NextMemberOffset,UBMT_SAMPLER,EShaderPrecisionModifier::Float,1,1,1,NULL);
		NextMemberOffset += 8;
	}

	for (int32 i = 0; i < UniformExternalTextureExpressions.Num(); ++i)
	{
		check((NextMemberOffset & 0x7) == 0);
		new(Members) FUniformBufferStruct::FMember(*ExternalTextureNames[i], TEXT("TextureExternal"), NextMemberOffset, UBMT_TEXTURE, EShaderPrecisionModifier::Float, 1, 1, 1, NULL);
		NextMemberOffset += 8;
		new(Members) FUniformBufferStruct::FMember(*MediaTextureSamplerNames[i], TEXT("SamplerState"), NextMemberOffset, UBMT_SAMPLER, EShaderPrecisionModifier::Float, 1, 1, 1, NULL);
		NextMemberOffset += 8;
	}


	new(Members) FUniformBufferStruct::FMember(TEXT("Wrap_WorldGroupSettings"),TEXT("SamplerState"),NextMemberOffset,UBMT_SAMPLER,EShaderPrecisionModifier::Float,1,1,1,NULL);
	NextMemberOffset += 8;

	new(Members) FUniformBufferStruct::FMember(TEXT("Clamp_WorldGroupSettings"),TEXT("SamplerState"),NextMemberOffset,UBMT_SAMPLER,EShaderPrecisionModifier::Float,1,1,1,NULL);
	NextMemberOffset += 8;

	const uint32 StructSize = Align(NextMemberOffset,UNIFORM_BUFFER_STRUCT_ALIGNMENT);
	UniformBufferStruct.Emplace(
		MaterialLayoutName,
		TEXT("MaterialUniforms"),
		TEXT("Material"),
		ConstructMaterialUniformBufferParameter,
		StructSize,
		Members,
		false
		);
}

const FUniformBufferStruct& FUniformExpressionSet::GetUniformBufferStruct() const
{
	return UniformBufferStruct.GetValue();
}

FUniformBufferRHIRef FUniformExpressionSet::CreateUniformBuffer(const FMaterialRenderContext& MaterialRenderContext, FRHICommandList* CommandListIfLocalMode, struct FLocalUniformBuffer* OutLocalUniformBuffer) const
{
	check(UniformBufferStruct);
	check(IsInParallelRenderingThread());
	
	FUniformBufferRHIRef UniformBuffer;

	if (UniformBufferStruct->GetSize() > 0)
	{
		FMemMark Mark(FMemStack::Get());
		void* const TempBuffer = FMemStack::Get().PushBytes(UniformBufferStruct->GetSize(),UNIFORM_BUFFER_STRUCT_ALIGNMENT);

		FLinearColor* TempVectorBuffer = (FLinearColor*)TempBuffer;
		for(int32 VectorIndex = 0;VectorIndex < UniformVectorExpressions.Num();++VectorIndex)
		{
			TempVectorBuffer[VectorIndex] = FLinearColor(0,0,0,0);
			UniformVectorExpressions[VectorIndex]->GetNumberValue(MaterialRenderContext,TempVectorBuffer[VectorIndex]);
		}

		float* TempScalarBuffer = (float*)(TempVectorBuffer + UniformVectorExpressions.Num());
		for(int32 ScalarIndex = 0;ScalarIndex < UniformScalarExpressions.Num();++ScalarIndex)
		{
			FLinearColor VectorValue(0,0,0,0);
			UniformScalarExpressions[ScalarIndex]->GetNumberValue(MaterialRenderContext,VectorValue);
			TempScalarBuffer[ScalarIndex] = VectorValue.R;
		}

		void** ResourceTable = (void**)((uint8*)TempBuffer + UniformBufferStruct->GetLayout().ResourceOffset);
		check(((UPTRINT)ResourceTable & 0x7) == 0);

		check(UniformBufferStruct->GetLayout().Resources.Num() == Uniform2DTextureExpressions.Num() * 2 + UniformCubeTextureExpressions.Num() * 2 + UniformExternalTextureExpressions.Num() * 2 + 2);

		// Cache 2D texture uniform expressions.
		for(int32 ExpressionIndex = 0;ExpressionIndex < Uniform2DTextureExpressions.Num();ExpressionIndex++)
		{
			const UTexture* Value;
			ESamplerSourceMode SourceMode;
			Uniform2DTextureExpressions[ExpressionIndex]->GetTextureValue(MaterialRenderContext,MaterialRenderContext.Material,Value,SourceMode);

			if (Value)
			{
				// Pre-application validity checks (explicit ensures to avoid needless string allocation)
				const FMaterialUniformExpressionTextureParameter* TextureParameter = (Uniform2DTextureExpressions[ExpressionIndex]->GetType() == &FMaterialUniformExpressionTextureParameter::StaticType) ?
					&static_cast<const FMaterialUniformExpressionTextureParameter&>(*Uniform2DTextureExpressions[ExpressionIndex]) : nullptr;

				// gmartin: Trying to locate UE-23902
				if (!Value->IsValidLowLevel())
				{
					ensureMsgf(false, TEXT("Texture not valid! UE-23902! Parameter (%s)"), TextureParameter ? *TextureParameter->GetParameterName().ToString() : TEXT("non-parameter"));
				}

				// Do not allow external textures to be applied to normal texture samplers
				if (Value->GetMaterialType() == MCT_TextureExternal)
				{
					FText MessageText = FText::Format(
						NSLOCTEXT("MaterialExpressions", "IncompatibleExternalTexture", " applied to a non-external Texture2D sampler. This may work by chance on some platforms but is not portable. Please change sampler type to 'External'. Parameter '{0}' (slot {1}) in material '{2}'"),
						FText::FromName(TextureParameter ? TextureParameter->GetParameterName() : FName()),
						ExpressionIndex,
						FText::FromString(*MaterialRenderContext.Material.GetFriendlyName()));

					FMessageLog("PIE").Warning()
						->AddToken(FUObjectToken::Create(Value))
						->AddToken(FTextToken::Create(MessageText));
				}
			}

			if (Value && Value->Resource)
			{
				//@todo-rco: Help track down a invalid values
				checkf(Value->IsA(UTexture::StaticClass()), TEXT("Expecting a UTexture! Value='%s' class='%s'"), *Value->GetName(), *Value->GetClass()->GetName());

				// UMaterial / UMaterialInstance should have caused all dependent textures to be PostLoaded, which initializes their rendering resource
				checkf(Value->TextureReference.TextureReferenceRHI, TEXT("Texture %s of class %s had invalid texture reference. Material %s with texture expression in slot %i"),
					*Value->GetName(), *Value->GetClass()->GetName(),
					*MaterialRenderContext.Material.GetFriendlyName(), ExpressionIndex);

				*ResourceTable++ = Value->TextureReference.TextureReferenceRHI;
				FSamplerStateRHIRef* SamplerSource = &Value->Resource->SamplerStateRHI;

				if (SourceMode == SSM_Wrap_WorldGroupSettings)
				{
					SamplerSource = &Wrap_WorldGroupSettings->SamplerStateRHI;
				}
				else if (SourceMode == SSM_Clamp_WorldGroupSettings)
				{
					SamplerSource = &Clamp_WorldGroupSettings->SamplerStateRHI;
				}

				checkf(*SamplerSource, TEXT("Texture %s of class %s had invalid sampler source. Material %s with texture expression in slot %i"),
					*Value->GetName(), *Value->GetClass()->GetName(),
					*MaterialRenderContext.Material.GetFriendlyName(), ExpressionIndex);
				*ResourceTable++ = *SamplerSource;
			}
			else
			{
				check(GWhiteTexture->TextureRHI);
				*ResourceTable++ = GWhiteTexture->TextureRHI;
				check(GWhiteTexture->SamplerStateRHI);
				*ResourceTable++ = GWhiteTexture->SamplerStateRHI;
			}
		}

		// Cache cube texture uniform expressions.
		for(int32 ExpressionIndex = 0;ExpressionIndex < UniformCubeTextureExpressions.Num();ExpressionIndex++)
		{
			const UTexture* Value;
			ESamplerSourceMode SourceMode;
			UniformCubeTextureExpressions[ExpressionIndex]->GetTextureValue(MaterialRenderContext,MaterialRenderContext.Material,Value,SourceMode);
			if(Value && Value->Resource)
			{
				check(Value->TextureReference.TextureReferenceRHI);
				*ResourceTable++ = Value->TextureReference.TextureReferenceRHI;
				FSamplerStateRHIRef* SamplerSource = &Value->Resource->SamplerStateRHI;

				if (SourceMode == SSM_Wrap_WorldGroupSettings)
				{
					SamplerSource = &Wrap_WorldGroupSettings->SamplerStateRHI;
				}
				else if (SourceMode == SSM_Clamp_WorldGroupSettings)
				{
					SamplerSource = &Clamp_WorldGroupSettings->SamplerStateRHI;
				}

				check(*SamplerSource);
				*ResourceTable++ = *SamplerSource;
			}
			else
			{
				check(GWhiteTexture->TextureRHI);
				*ResourceTable++ = GWhiteTexture->TextureRHI;
				check(GWhiteTexture->SamplerStateRHI);
				*ResourceTable++ = GWhiteTexture->SamplerStateRHI;
			}
		}

		// Cache external texture uniform expressions.
		for (int32 ExpressionIndex = 0; ExpressionIndex < UniformExternalTextureExpressions.Num(); ExpressionIndex++)
		{
			FTextureRHIRef TextureRHI;
			FSamplerStateRHIRef SamplerStateRHI;
			if (UniformExternalTextureExpressions[ExpressionIndex]->GetExternalTexture(MaterialRenderContext, TextureRHI, SamplerStateRHI))
			{
				*ResourceTable++ = TextureRHI;
				*ResourceTable++ = SamplerStateRHI;
			}
			else
			{
				check(GBlackTexture->TextureRHI);
				*ResourceTable++ = GBlackTexture->TextureRHI;
				check(GBlackTexture->SamplerStateRHI);
				*ResourceTable++ = GBlackTexture->SamplerStateRHI;
			}
		}

		check(Wrap_WorldGroupSettings->SamplerStateRHI);
		*ResourceTable++ = Wrap_WorldGroupSettings->SamplerStateRHI;
		check(Clamp_WorldGroupSettings->SamplerStateRHI);
		*ResourceTable++ = Clamp_WorldGroupSettings->SamplerStateRHI;

		if (CommandListIfLocalMode)
		{
			check(OutLocalUniformBuffer);
			*OutLocalUniformBuffer = CommandListIfLocalMode->BuildLocalUniformBuffer(TempBuffer, UniformBufferStruct->GetSize(), UniformBufferStruct->GetLayout());
			check(OutLocalUniformBuffer->IsValid());
		}
		else
		{
			UniformBuffer = RHICreateUniformBuffer(TempBuffer, UniformBufferStruct->GetLayout(), UniformBuffer_MultiFrame);
			check(!OutLocalUniformBuffer->IsValid());
		}
	}

	return UniformBuffer;
}

FMaterialUniformExpressionTexture::FMaterialUniformExpressionTexture() :
	TextureIndex(INDEX_NONE),
	SamplerSource(SSM_FromTextureAsset),
	TransientOverrideValue_GameThread(NULL),
	TransientOverrideValue_RenderThread(NULL)
{}

FMaterialUniformExpressionTexture::FMaterialUniformExpressionTexture(int32 InTextureIndex, ESamplerSourceMode InSamplerSource) :
	TextureIndex(InTextureIndex),
	SamplerSource(InSamplerSource),
	TransientOverrideValue_GameThread(NULL),
	TransientOverrideValue_RenderThread(NULL)
{
}

void FMaterialUniformExpressionTexture::Serialize(FArchive& Ar)
{
	int32 SamplerSourceInt = (int32)SamplerSource;
	Ar << TextureIndex << SamplerSourceInt;
	SamplerSource = (ESamplerSourceMode)SamplerSourceInt;
}

void FMaterialUniformExpressionTexture::SetTransientOverrideTextureValue( UTexture* InOverrideTexture )
{
	TransientOverrideValue_GameThread = InOverrideTexture;
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(SetTransientOverrideTextureValueCommand,
											   FMaterialUniformExpressionTexture*, ExpressionTexture, this,
											   UTexture*, InOverrideTexture, InOverrideTexture,
											   {
												   ExpressionTexture->TransientOverrideValue_RenderThread = InOverrideTexture;
											   });
}

void FMaterialUniformExpressionTexture::GetTextureValue(const FMaterialRenderContext& Context,const FMaterial& Material,const UTexture*& OutValue,ESamplerSourceMode& OutSamplerSource) const
{
	check(IsInParallelRenderingThread());
	OutSamplerSource = SamplerSource;
	if( TransientOverrideValue_RenderThread != NULL )
	{
		OutValue = TransientOverrideValue_RenderThread;
	}
	else
	{
		OutValue = GetIndexedTexture(Material, TextureIndex);
	}
}

void FMaterialUniformExpressionTexture::GetGameThreadTextureValue(const UMaterialInterface* MaterialInterface,const FMaterial& Material,UTexture*& OutValue,bool bAllowOverride) const
{
	check(IsInGameThread());
	if (bAllowOverride && TransientOverrideValue_GameThread)
	{
		OutValue = TransientOverrideValue_GameThread;
	}
	else
	{
		OutValue = GetIndexedTexture(Material, TextureIndex);
	}
}

bool FMaterialUniformExpressionTexture::IsIdentical(const FMaterialUniformExpression* OtherExpression) const
{
	if (GetType() != OtherExpression->GetType())
	{
		return false;
	}
	FMaterialUniformExpressionTexture* OtherTextureExpression = (FMaterialUniformExpressionTexture*)OtherExpression;

	return TextureIndex == OtherTextureExpression->TextureIndex;
}

FMaterialUniformExpressionExternalTextureBase::FMaterialUniformExpressionExternalTextureBase(int32 InSourceTextureIndex)
	: SourceTextureIndex(InSourceTextureIndex)
{}

FMaterialUniformExpressionExternalTextureBase::FMaterialUniformExpressionExternalTextureBase(const FGuid& InGuid)
	: SourceTextureIndex(INDEX_NONE)
	, ExternalTextureGuid(InGuid)
{
}

void FMaterialUniformExpressionExternalTextureBase::Serialize(FArchive& Ar)
{
	Ar << SourceTextureIndex;
	Ar << ExternalTextureGuid;
}

bool FMaterialUniformExpressionExternalTextureBase::IsIdentical(const FMaterialUniformExpression* OtherExpression) const
{
	if (GetType() != OtherExpression->GetType())
	{
		return false;
	}

	const auto* Other = static_cast<const FMaterialUniformExpressionExternalTextureBase*>(OtherExpression);
	return SourceTextureIndex == Other->SourceTextureIndex && ExternalTextureGuid == Other->ExternalTextureGuid;
}

FGuid FMaterialUniformExpressionExternalTextureBase::ResolveExternalTextureGUID(const FMaterialRenderContext& Context, TOptional<FName> ParameterName) const
{
	// Use the compile-time GUID if it is set
	if (ExternalTextureGuid.IsValid())
	{
		return ExternalTextureGuid;
	}

	const UTexture* TextureParameterObject = nullptr;
	if (ParameterName.IsSet() && Context.MaterialRenderProxy && Context.MaterialRenderProxy->GetTextureValue(ParameterName.GetValue(), &TextureParameterObject, Context) && TextureParameterObject)
	{
		return TextureParameterObject->GetExternalTextureGuid();
	}

	// Otherwise attempt to use the texture index in the material, if it's valid
	const UTexture* TextureObject = SourceTextureIndex != INDEX_NONE ? GetIndexedTexture(Context.Material, SourceTextureIndex) : nullptr;
	if (TextureObject)
	{
		return TextureObject->GetExternalTextureGuid();
	}

	return FGuid();
}

bool FMaterialUniformExpressionExternalTexture::GetExternalTexture(const FMaterialRenderContext& Context, FTextureRHIRef& OutTextureRHI, FSamplerStateRHIRef& OutSamplerStateRHI) const
{
	check(IsInParallelRenderingThread());

	FGuid GuidToLookup = ResolveExternalTextureGUID(Context);
	return FExternalTextureRegistry::Get().GetExternalTexture(Context.MaterialRenderProxy, GuidToLookup, OutTextureRHI, OutSamplerStateRHI);
}

FMaterialUniformExpressionExternalTextureParameter::FMaterialUniformExpressionExternalTextureParameter()
{}

FMaterialUniformExpressionExternalTextureParameter::FMaterialUniformExpressionExternalTextureParameter(FName InParameterName, int32 InTextureIndex)
	: Super(InTextureIndex)
	, ParameterName(InParameterName)
{}

void FMaterialUniformExpressionExternalTextureParameter::Serialize(FArchive& Ar)
{
	Ar << ParameterName;
	Super::Serialize(Ar);
}

bool FMaterialUniformExpressionExternalTextureParameter::GetExternalTexture(const FMaterialRenderContext& Context, FTextureRHIRef& OutTextureRHI, FSamplerStateRHIRef& OutSamplerStateRHI) const
{
	check(IsInParallelRenderingThread());

	FGuid GuidToLookup = ResolveExternalTextureGUID(Context, ParameterName);
	return FExternalTextureRegistry::Get().GetExternalTexture(Context.MaterialRenderProxy, GuidToLookup, OutTextureRHI, OutSamplerStateRHI);
}

bool FMaterialUniformExpressionExternalTextureParameter::IsIdentical(const FMaterialUniformExpression* OtherExpression) const
{
	if (GetType() != OtherExpression->GetType())
	{
		return false;
	}

	auto* Other = static_cast<const FMaterialUniformExpressionExternalTextureParameter*>(OtherExpression);
	return ParameterName == Other->ParameterName && Super::IsIdentical(OtherExpression);
}

void FMaterialUniformExpressionVectorParameter::GetGameThreadNumberValue(const UMaterialInterface* SourceMaterialToCopyFrom, FLinearColor& OutValue) const
{
	check(IsInGameThread());
	checkSlow(SourceMaterialToCopyFrom);

	const UMaterialInterface* It = SourceMaterialToCopyFrom;

	for (;;)
	{
		const UMaterialInstance* MatInst = Cast<UMaterialInstance>(It);

		if (MatInst)
		{
			const FVectorParameterValue* ParameterValue = GameThread_FindParameterByName(MatInst->VectorParameterValues, ParameterName);
			if(ParameterValue)
			{
				OutValue = ParameterValue->ParameterValue;
				break;
			}

			// go up the hierarchy
			It = MatInst->Parent;
		}
		else
		{
			// we reached the base material
			// get the copy form the base material
			GetDefaultValue(OutValue);
			break;
		}
	}
}

void FMaterialUniformExpressionScalarParameter::GetGameThreadNumberValue(const UMaterialInterface* SourceMaterialToCopyFrom, float& OutValue) const
{
	check(IsInGameThread());
	checkSlow(SourceMaterialToCopyFrom);

	const UMaterialInterface* It = SourceMaterialToCopyFrom;

	for (;;)
	{
		const UMaterialInstance* MatInst = Cast<UMaterialInstance>(It);

		if (MatInst)
		{
			const FScalarParameterValue* ParameterValue = GameThread_FindParameterByName(MatInst->ScalarParameterValues, ParameterName);
			if(ParameterValue)
			{
				OutValue = ParameterValue->ParameterValue;
				break;
			}

			// go up the hierarchy
			It = MatInst->Parent;
		}
		else
		{
			// we reached the base material
			// get the copy form the base material
			GetDefaultValue(OutValue);
			break;
		}
	}
}

namespace
{
	void SerializeOptional(FArchive& Ar, TOptional<FName>& OptionalName)
	{
		bool bIsSet = OptionalName.IsSet();
		Ar << bIsSet;

		if (bIsSet)
		{
			if (!OptionalName.IsSet())
			{
				OptionalName.Emplace();
			}

			Ar << OptionalName.GetValue();
		}
	}
}

void FMaterialUniformExpressionExternalTextureCoordinateScaleRotation::Serialize(FArchive& Ar)
{
	// Write out the optional parameter name
	SerializeOptional(Ar, ParameterName);

	Super::Serialize(Ar);
}

bool FMaterialUniformExpressionExternalTextureCoordinateScaleRotation::IsIdentical(const FMaterialUniformExpression* OtherExpression) const
{
	if (GetType() != OtherExpression->GetType() || !Super::IsIdentical(OtherExpression))
	{
		return false;
	}

	const auto* Other = static_cast<const FMaterialUniformExpressionExternalTextureCoordinateScaleRotation*>(OtherExpression);
	return ParameterName == Other->ParameterName;
}

void FMaterialUniformExpressionExternalTextureCoordinateScaleRotation::GetNumberValue(const FMaterialRenderContext& Context, FLinearColor& OutValue) const
{
	FGuid GuidToLookup = ResolveExternalTextureGUID(Context, ParameterName);
	if (!GuidToLookup.IsValid() || !FExternalTextureRegistry::Get().GetExternalTextureCoordinateScaleRotation(GuidToLookup, OutValue))
	{
		OutValue = FLinearColor(1.f, 0.f, 0.f, 1.f);
	}
}

void FMaterialUniformExpressionExternalTextureCoordinateOffset::Serialize(FArchive& Ar)
{
	// Write out the optional parameter name
	SerializeOptional(Ar, ParameterName);

	Super::Serialize(Ar);
}

bool FMaterialUniformExpressionExternalTextureCoordinateOffset::IsIdentical(const FMaterialUniformExpression* OtherExpression) const
{
	if (GetType() != OtherExpression->GetType() || !Super::IsIdentical(OtherExpression))
	{
		return false;
	}

	const auto* Other = static_cast<const FMaterialUniformExpressionExternalTextureCoordinateOffset*>(OtherExpression);
	return ParameterName == Other->ParameterName;
}

void FMaterialUniformExpressionExternalTextureCoordinateOffset::GetNumberValue(const FMaterialRenderContext& Context, FLinearColor& OutValue) const
{
	FGuid GuidToLookup = ResolveExternalTextureGUID(Context, ParameterName);
	if (!GuidToLookup.IsValid() || !FExternalTextureRegistry::Get().GetExternalTextureCoordinateOffset(GuidToLookup, OutValue))
	{
		OutValue.R = OutValue.G = 0;
		OutValue.B = OutValue.A = 0;
	}
}


IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionTexture);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionConstant);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionTime);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionRealTime);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionVectorParameter);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionScalarParameter);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionTextureParameter);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionExternalTextureBase);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionExternalTexture);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionExternalTextureParameter);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionExternalTextureCoordinateScaleRotation);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionExternalTextureCoordinateOffset);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionFlipBookTextureParameter);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionSine);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionSquareRoot);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionLength);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionLogarithm2);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionLogarithm10);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionFoldedMath);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionPeriodic);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionAppendVector);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionMin);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionMax);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionClamp);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionSaturate);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionComponentSwizzle);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionFloor);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionCeil);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionFrac);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionFmod);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionAbs);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionTextureProperty);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionTrigMath);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionRound);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionTruncate);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionSign);
