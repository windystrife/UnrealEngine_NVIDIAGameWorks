// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MaterialShared.cpp: Shared material implementation.
=============================================================================*/

#include "MaterialShared.h"
#include "Stats/StatsMisc.h"
#include "UObject/CoreObjectVersion.h"
#include "Misc/App.h"
#include "UObject/UObjectHash.h"
#include "LocalVertexFactory.h"
#include "Materials/MaterialInterface.h"
#include "MaterialExpressionIO.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceBasePropertyOverrides.h"
#include "Materials/MaterialInstance.h"
#include "UObject/UObjectIterator.h"
#include "ComponentReregisterContext.h"
#include "Materials/MaterialExpressionBreakMaterialAttributes.h"
#include "Materials/MaterialExpressionReroute.h"
#include "ShaderCompiler.h"
#include "MaterialCompiler.h"
#include "MeshMaterialShaderType.h"
#include "RendererInterface.h"
#include "Materials/HLSLMaterialTranslator.h"
#include "ComponentRecreateRenderStateContext.h"
#include "EngineModule.h"
#include "Engine/Texture.h"
#include "SceneView.h"
#include "ShaderPlatformQualitySettings.h"
#include "MaterialShaderQualitySettings.h"
#include "DecalRenderingCommon.h"
#include "ExternalTexture.h"

DEFINE_LOG_CATEGORY(LogMaterial);

int32 GDeferUniformExpressionCaching = 1;
FAutoConsoleVariableRef CVarDeferUniformExpressionCaching(
	TEXT("r.DeferUniformExpressionCaching"),
	GDeferUniformExpressionCaching,
	TEXT("Whether to defer caching of uniform expressions until a rendering command needs them up to date.  Deferring updates is more efficient because multiple SetVectorParameterValue calls in a frame will only result in one update."),
	ECVF_RenderThreadSafe
	);

FName MaterialQualityLevelNames[] = 
{
	FName(TEXT("Low")),
	FName(TEXT("High")),
	FName(TEXT("Medium")),
	FName(TEXT("Num"))
};

static_assert(ARRAY_COUNT(MaterialQualityLevelNames) == EMaterialQualityLevel::Num + 1, "Missing entry from material quality level names.");

void GetMaterialQualityLevelName(EMaterialQualityLevel::Type InQualityLevel, FString& OutName)
{
	check(InQualityLevel < ARRAY_COUNT(MaterialQualityLevelNames));
	MaterialQualityLevelNames[(int32)InQualityLevel].ToString(OutName);
}

static inline SIZE_T AddShaderSize(FShader* Shader, TSet<FShaderResourceId>& UniqueShaderResourceIds)
{
	SIZE_T ResourceSize = 0;
	FShaderResourceId ResourceId = Shader->GetResourceId();
	bool bCountedResource = false;
	UniqueShaderResourceIds.Add(ResourceId, &bCountedResource);
	if (!bCountedResource)
	{
		ResourceSize += Shader->GetResourceSizeBytes();
	}
	ResourceSize += Shader->GetSizeBytes();
	return ResourceSize;
}

int32 FMaterialCompiler::Errorf(const TCHAR* Format,...)
{
	TCHAR	ErrorText[2048];
	GET_VARARGS( ErrorText, ARRAY_COUNT(ErrorText), ARRAY_COUNT(ErrorText)-1, Format, Format );
	return Error(ErrorText);
}

IMPLEMENT_STRUCT(ExpressionInput);
IMPLEMENT_STRUCT(ColorMaterialInput);
IMPLEMENT_STRUCT(ScalarMaterialInput);
IMPLEMENT_STRUCT(VectorMaterialInput);
IMPLEMENT_STRUCT(Vector2MaterialInput);
IMPLEMENT_STRUCT(MaterialAttributesInput);

#if WITH_EDITOR
int32 FExpressionInput::Compile(class FMaterialCompiler* Compiler)
{
	if(Expression)
	{
		Expression->ValidateState();
		
		int32 ExpressionResult = Compiler->CallExpression(FMaterialExpressionKey(Expression, OutputIndex, Compiler->GetMaterialAttribute(), Compiler->IsCurrentlyCompilingForPreviousFrame()),Compiler);

		if(Mask && ExpressionResult != INDEX_NONE)
		{
			return Compiler->ComponentMask(
				ExpressionResult,
				!!MaskR,!!MaskG,!!MaskB,!!MaskA
				);
		}
		else
		{
			return ExpressionResult;
		}
	}
	else
		return INDEX_NONE;
}

void FExpressionInput::Connect( int32 InOutputIndex, class UMaterialExpression* InExpression )
{
	OutputIndex = InOutputIndex;
	Expression = InExpression;

	TArray<FExpressionOutput> Outputs;
	Outputs = Expression->GetOutputs();
	FExpressionOutput* Output = &Outputs[ OutputIndex ];
	Mask = Output->Mask;
	MaskR = Output->MaskR;
	MaskG = Output->MaskG;
	MaskB = Output->MaskB;
	MaskA = Output->MaskA;
}
#endif // WITH_EDITOR

FExpressionInput FExpressionInput::GetTracedInput() const
{
#if WITH_EDITORONLY_DATA
	if (Expression != nullptr && Expression->IsA(UMaterialExpressionReroute::StaticClass()))
	{
		UMaterialExpressionReroute* Reroute = CastChecked<UMaterialExpressionReroute>(Expression);
		return Reroute->TraceInputsToRealInput();
	}
#endif
	return *this;
}

/** Native serialize for FMaterialExpression struct */
static bool SerializeExpressionInput(FArchive& Ar, FExpressionInput& Input)
{
	Ar.UsingCustomVersion(FCoreObjectVersion::GUID);

	if (Ar.CustomVer(FCoreObjectVersion::GUID) < FCoreObjectVersion::MaterialInputNativeSerialize)
	{
		return false;
	}

#if WITH_EDITORONLY_DATA
	if (!Ar.IsFilterEditorOnly())
	{
		Ar << Input.Expression;
	}
#endif
	Ar << Input.OutputIndex;
	Ar << Input.InputName;
	Ar << Input.Mask;
	Ar << Input.MaskR;
	Ar << Input.MaskG;
	Ar << Input.MaskB;
	Ar << Input.MaskA;

	// Some expressions may have been stripped when cooking and Expression can be null after loading
	// so make sure we keep the information about the connected node in cooked packages
	if ( Ar.IsFilterEditorOnly() )
	{
#if WITH_EDITORONLY_DATA
		if (Ar.IsSaving())
		{
			Input.ExpressionName = Input.Expression ? Input.Expression->GetFName() : NAME_None;
		}
#endif // WITH_EDITORONLY_DATA
		Ar << Input.ExpressionName;
	}

	return true;
}

template <typename InputType>
static bool SerializeMaterialInput(FArchive& Ar, FMaterialInput<InputType>& Input)
{
	if (SerializeExpressionInput(Ar, Input))
	{
		bool bUseConstantValue = Input.UseConstant;
		Ar << bUseConstantValue;
		Input.UseConstant = bUseConstantValue;
		Ar << Input.Constant;
		return true;
	}
	else
	{
		return false;
	}
}

bool FExpressionInput::Serialize(FArchive& Ar)
{
	return SerializeExpressionInput(Ar, *this);
}

bool FColorMaterialInput::Serialize(FArchive& Ar)
{
	return SerializeMaterialInput<FColor>(Ar, *this);
}

bool FScalarMaterialInput::Serialize(FArchive& Ar)
{
	return SerializeMaterialInput<float>(Ar, *this);
}

bool FVectorMaterialInput::Serialize(FArchive& Ar)
{
	return SerializeMaterialInput<FVector>(Ar, *this);
}

bool FVector2MaterialInput::Serialize(FArchive& Ar)
{
	return SerializeMaterialInput<FVector2D>(Ar, *this);
}

bool FMaterialAttributesInput::Serialize(FArchive& Ar)
{
	return SerializeExpressionInput(Ar, *this);
}

#if WITH_EDITOR
int32 FColorMaterialInput::CompileWithDefault(class FMaterialCompiler* Compiler, EMaterialProperty Property)
{
	if (UseConstant)
	{
		FLinearColor LinearColor(Constant);
		return Compiler->Constant3(LinearColor.R, LinearColor.G, LinearColor.B);
	}
	else if (Expression)
	{
		int32 ResultIndex = FExpressionInput::Compile(Compiler);
		if (ResultIndex != INDEX_NONE)
		{
			return ResultIndex;
		}
	}

	return Compiler->ForceCast(FMaterialAttributeDefinitionMap::CompileDefaultExpression(Compiler, Property), MCT_Float3);
}

int32 FScalarMaterialInput::CompileWithDefault(class FMaterialCompiler* Compiler, EMaterialProperty Property)
{
	if (UseConstant)
	{
		return Compiler->Constant(Constant);
	}
	else if (Expression)
	{
		int32 ResultIndex = FExpressionInput::Compile(Compiler);
		if (ResultIndex != INDEX_NONE)
		{
			return ResultIndex;
		}
	}
	
	return Compiler->ForceCast(FMaterialAttributeDefinitionMap::CompileDefaultExpression(Compiler, Property), MCT_Float1);
}

int32 FVectorMaterialInput::CompileWithDefault(class FMaterialCompiler* Compiler, EMaterialProperty Property)
{
	if (UseConstant)
	{
		return Compiler->Constant3(Constant.X, Constant.Y, Constant.Z);
	}
	else if(Expression)
	{
		int32 ResultIndex = FExpressionInput::Compile(Compiler);
		if (ResultIndex != INDEX_NONE)
		{
			return ResultIndex;
		}
	}
	return Compiler->ForceCast(FMaterialAttributeDefinitionMap::CompileDefaultExpression(Compiler, Property), MCT_Float3);
}

int32 FVector2MaterialInput::CompileWithDefault(class FMaterialCompiler* Compiler, EMaterialProperty Property)
{
	if (UseConstant)
	{
		return Compiler->Constant2(Constant.X, Constant.Y);
	}
	else if (Expression)
	{
		int32 ResultIndex = FExpressionInput::Compile(Compiler);
		if (ResultIndex != INDEX_NONE)
		{
			return ResultIndex;
		}
	}

	return Compiler->ForceCast(FMaterialAttributeDefinitionMap::CompileDefaultExpression(Compiler, Property), MCT_Float2);
}

int32 FMaterialAttributesInput::CompileWithDefault(class FMaterialCompiler* Compiler, const FGuid& AttributeID)
{
	int32 Ret = INDEX_NONE;
	if(Expression)
	{
		FScopedMaterialCompilerAttribute ScopedMaterialCompilerAttribute(Compiler, AttributeID);
		Ret = FExpressionInput::Compile(Compiler);

		if (Ret != INDEX_NONE && !Expression->IsResultMaterialAttributes(OutputIndex))
		{
			Compiler->Error(TEXT("Cannot connect a non MaterialAttributes node to a MaterialAttributes pin."));
		}
	}

	EMaterialProperty Property = FMaterialAttributeDefinitionMap::GetProperty(AttributeID);

	SetConnectedProperty(Property, Ret != INDEX_NONE);

	if( Ret == INDEX_NONE )
	{
		Ret = FMaterialAttributeDefinitionMap::CompileDefaultExpression(Compiler, AttributeID);
	}

	return Ret;
}
#endif  // WITH_EDITOR

void FMaterialCompilationOutput::Serialize(FArchive& Ar)
{
	UniformExpressionSet.Serialize(Ar);

	Ar << bRequiresSceneColorCopy;
	Ar << bNeedsSceneTextures;
	Ar << bUsesEyeAdaptation;
	Ar << bModifiesMeshPosition;
	Ar << bUsesWorldPositionOffset;
	Ar << bNeedsGBuffer;
	Ar << bUsesGlobalDistanceField;
	Ar << bUsesPixelDepthOffset;
	Ar << bUsesSceneDepthLookup;
}

void FMaterial::GetShaderMapId(EShaderPlatform Platform, FMaterialShaderMapId& OutId) const
{ 
	if (bLoadedCookedShaderMapId)
	{
		OutId = CookedShaderMapId;
	}
	else
	{
		TArray<FShaderType*> ShaderTypes;
		TArray<FVertexFactoryType*> VFTypes;
		TArray<const FShaderPipelineType*> ShaderPipelineTypes;

		GetDependentShaderAndVFTypes(Platform, ShaderTypes, ShaderPipelineTypes, VFTypes);

		OutId.Usage = GetShaderMapUsage();
		OutId.BaseMaterialId = GetMaterialId();
		OutId.QualityLevel = GetQualityLevelForShaderMapId();
		OutId.FeatureLevel = GetFeatureLevel();
		OutId.SetShaderDependencies(ShaderTypes, ShaderPipelineTypes, VFTypes);
		GetReferencedTexturesHash(Platform, OutId.TextureReferencesHash);
	}
}

EMaterialTessellationMode FMaterial::GetTessellationMode() const 
{ 
	return MTM_NoTessellation; 
}

ERefractionMode FMaterial::GetRefractionMode() const 
{ 
	return RM_IndexOfRefraction; 
}

void FMaterial::GetShaderMapIDsWithUnfinishedCompilation(TArray<int32>& ShaderMapIds)
{
	// Build an array of the shader map Id's are not finished compiling.
	if (GameThreadShaderMap && !GameThreadShaderMap->IsCompilationFinalized())
	{
		ShaderMapIds.Add(GameThreadShaderMap->GetCompilingId());
	}
	else if (OutstandingCompileShaderMapIds.Num() != 0 )
	{
		ShaderMapIds.Append(OutstandingCompileShaderMapIds);
	}
}

bool FMaterial::IsCompilationFinished() const
{
	// Build an array of the shader map Id's are not finished compiling.
	if (GameThreadShaderMap && !GameThreadShaderMap->IsCompilationFinalized())
	{
		return false;
	}
	else if (OutstandingCompileShaderMapIds.Num() != 0 )
	{
		return false;
	}
	return true;
}

bool FMaterial::HasValidGameThreadShaderMap() const
{
	if(!GameThreadShaderMap || !GameThreadShaderMap->IsCompilationFinalized())
	{
		return false;
	}
	return true;
}

void FMaterial::CancelCompilation()
{
	TArray<int32> ShaderMapIdsToCancel;
	GetShaderMapIDsWithUnfinishedCompilation(ShaderMapIdsToCancel);

	if (ShaderMapIdsToCancel.Num() > 0)
	{
		// Cancel all compile jobs for these shader maps.
		GShaderCompilingManager->CancelCompilation(*GetFriendlyName(), ShaderMapIdsToCancel);
	}
}

void FMaterial::FinishCompilation()
{
	TArray<int32> ShaderMapIdsToFinish;
	GetShaderMapIDsWithUnfinishedCompilation(ShaderMapIdsToFinish);

	if (ShaderMapIdsToFinish.Num() > 0)
	{
		// Block until the shader maps that we will save have finished being compiled
		GShaderCompilingManager->FinishCompilation(*GetFriendlyName(), ShaderMapIdsToFinish);
	}
}

const FMaterialShaderMap* FMaterial::GetShaderMapToUse() const 
{ 
	const FMaterialShaderMap* ShaderMapToUse = NULL;

	if (IsInGameThread())
	{
		// If we are accessing uniform texture expressions on the game thread, use results from a shader map whose compile is in flight that matches this material
		// This allows querying what textures a material uses even when it is being asynchronously compiled
		ShaderMapToUse = GetGameThreadShaderMap() ? GetGameThreadShaderMap() : FMaterialShaderMap::GetShaderMapBeingCompiled(this);

		checkf(!ShaderMapToUse || ShaderMapToUse->GetNumRefs() > 0, TEXT("NumRefs %i, GameThreadShaderMap 0x%08x"), ShaderMapToUse->GetNumRefs(), GetGameThreadShaderMap());
	}
	else 
	{
		check(IsInRenderingThread());
		ShaderMapToUse = GetRenderingThreadShaderMap();
	}

	return ShaderMapToUse;
}

const TArray<TRefCountPtr<FMaterialUniformExpressionTexture> >& FMaterial::GetUniform2DTextureExpressions() const 
{ 
	const FMaterialShaderMap* ShaderMapToUse = GetShaderMapToUse();

	if (ShaderMapToUse)
	{
		return ShaderMapToUse->GetUniformExpressionSet().Uniform2DTextureExpressions; 
	}
	
	static const TArray<TRefCountPtr<FMaterialUniformExpressionTexture> > EmptyExpressions;
	return EmptyExpressions;
}

const TArray<TRefCountPtr<FMaterialUniformExpressionTexture> >& FMaterial::GetUniformCubeTextureExpressions() const 
{ 
	const FMaterialShaderMap* ShaderMapToUse = GetShaderMapToUse();

	if (ShaderMapToUse)
	{
		return ShaderMapToUse->GetUniformExpressionSet().UniformCubeTextureExpressions; 
	}

	static const TArray<TRefCountPtr<FMaterialUniformExpressionTexture> > EmptyExpressions;
	return EmptyExpressions;
}

const TArray<TRefCountPtr<FMaterialUniformExpression> >& FMaterial::GetUniformVectorParameterExpressions() const 
{ 
	const FMaterialShaderMap* ShaderMapToUse = GetShaderMapToUse();

	if (ShaderMapToUse)
	{
		return ShaderMapToUse->GetUniformExpressionSet().UniformVectorExpressions; 
	}

	static const TArray<TRefCountPtr<FMaterialUniformExpression> > EmptyExpressions;
	return EmptyExpressions;
}

const TArray<TRefCountPtr<FMaterialUniformExpression> >& FMaterial::GetUniformScalarParameterExpressions() const 
{ 
	const FMaterialShaderMap* ShaderMapToUse = GetShaderMapToUse();

	if (ShaderMapToUse)
	{
		return ShaderMapToUse->GetUniformExpressionSet().UniformScalarExpressions; 
	}

	static const TArray<TRefCountPtr<FMaterialUniformExpression> > EmptyExpressions;
	return EmptyExpressions;
}

bool FMaterial::RequiresSceneColorCopy_GameThread() const
{
	return GameThreadShaderMap.GetReference() ? GameThreadShaderMap->RequiresSceneColorCopy() : false; 
}

bool FMaterial::RequiresSceneColorCopy_RenderThread() const
{
	check(IsInParallelRenderingThread());
	if (RenderingThreadShaderMap)
	{
		return RenderingThreadShaderMap->RequiresSceneColorCopy();
	}
	return false;
}

bool FMaterial::NeedsSceneTextures() const 
{
	check(IsInParallelRenderingThread());

	if (RenderingThreadShaderMap)
	{
		return RenderingThreadShaderMap->NeedsSceneTextures();
	}
	
	return false;
}

bool FMaterial::NeedsGBuffer() const
{
	check(IsInParallelRenderingThread());

	if (IsOpenGLPlatform(GMaxRHIShaderPlatform) // @todo: TTP #341211
		&& !IsMobilePlatform(GMaxRHIShaderPlatform)) 
	{
		return true;
	}

	if (RenderingThreadShaderMap)
	{
		return RenderingThreadShaderMap->NeedsGBuffer();
	}

	return false;
}


bool FMaterial::UsesEyeAdaptation() const 
{
	check(IsInParallelRenderingThread());

	if (RenderingThreadShaderMap)
	{
		return RenderingThreadShaderMap->UsesEyeAdaptation();
	}

	return false;
}

bool FMaterial::UsesGlobalDistanceField_GameThread() const 
{ 
	return GameThreadShaderMap.GetReference() ? GameThreadShaderMap->UsesGlobalDistanceField() : false; 
}

bool FMaterial::UsesWorldPositionOffset_GameThread() const 
{ 
	return GameThreadShaderMap.GetReference() ? GameThreadShaderMap->UsesWorldPositionOffset() : false; 
}

bool FMaterial::MaterialModifiesMeshPosition_RenderThread() const
{ 
	check(IsInParallelRenderingThread());
	bool bUsesWPO = RenderingThreadShaderMap ? RenderingThreadShaderMap->ModifiesMeshPosition() : false;

	return bUsesWPO || GetTessellationMode() != MTM_NoTessellation;
}

bool FMaterial::MaterialModifiesMeshPosition_GameThread() const
{
	check(IsInGameThread());
	FMaterialShaderMap* ShaderMap = GameThreadShaderMap.GetReference();
	bool bUsesWPO = ShaderMap ? ShaderMap->ModifiesMeshPosition() : false;

	return bUsesWPO || GetTessellationMode() != MTM_NoTessellation;
}

bool FMaterial::MaterialMayModifyMeshPosition() const
{
	// Conservative estimate when called before material translation has occurred. 
	// This function is only intended for use in deciding whether or not shader permutations are required.
	return HasVertexPositionOffsetConnected() || HasPixelDepthOffsetConnected() || HasMaterialAttributesConnected() || GetTessellationMode() != MTM_NoTessellation
		|| (GetMaterialDomain() == MD_DeferredDecal && GetDecalBlendMode() == DBM_Volumetric_DistanceFunction);
}

bool FMaterial::MaterialUsesPixelDepthOffset() const
{
	check(IsInParallelRenderingThread());
	return RenderingThreadShaderMap ? RenderingThreadShaderMap->UsesPixelDepthOffset() : false;
}

bool FMaterial::MaterialUsesSceneDepthLookup_RenderThread() const
{
	check(IsInParallelRenderingThread());
	return RenderingThreadShaderMap ? RenderingThreadShaderMap->UsesSceneDepthLookup() : false;
}

bool FMaterial::MaterialUsesSceneDepthLookup_GameThread() const
{
	return GameThreadShaderMap.GetReference() ? GameThreadShaderMap->UsesSceneDepthLookup() : false;
}

FMaterialShaderMap* FMaterial::GetRenderingThreadShaderMap() const 
{ 
	check(IsInParallelRenderingThread());
	return RenderingThreadShaderMap; 
}

void FMaterial::SetRenderingThreadShaderMap(FMaterialShaderMap* InMaterialShaderMap)
{
	check(IsInRenderingThread());
	RenderingThreadShaderMap = InMaterialShaderMap;
}

void FMaterial::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObjects(ErrorExpressions);
}

struct FLegacyTextureLookup
{
	void Serialize(FArchive& Ar)
	{
		Ar << TexCoordIndex;
		Ar << TextureIndex;
		Ar << UScale;
		Ar << VScale;
	}

	int32 TexCoordIndex;
	int32 TextureIndex;	

	float UScale;
	float VScale;
};

FArchive& operator<<(FArchive& Ar, FLegacyTextureLookup& Ref)
{
	Ref.Serialize( Ar );
	return Ar;
}

void FMaterial::LegacySerialize(FArchive& Ar)
{
	if (Ar.UE4Ver() < VER_UE4_PURGED_FMATERIAL_COMPILE_OUTPUTS)
	{
		TArray<FString> LegacyStrings;
		Ar << LegacyStrings;

		TMap<UMaterialExpression*,int32> LegacyMap;
		Ar << LegacyMap;
		int32 LegacyInt;
		Ar << LegacyInt;

		FeatureLevel = ERHIFeatureLevel::SM4;
		QualityLevel = EMaterialQualityLevel::High;
		Ar << Id_DEPRECATED;

		TArray<UTexture*> LegacyTextures;
		Ar << LegacyTextures;

		bool bTemp2;
		Ar << bTemp2;

		bool bTemp;
		Ar << bTemp;

		TArray<FLegacyTextureLookup> LegacyLookups;
		Ar << LegacyLookups;

		uint32 DummyDroppedFallbackComponents = 0;
		Ar << DummyDroppedFallbackComponents;
	}

	SerializeInlineShaderMap(Ar);
}

void FMaterial::SerializeInlineShaderMap(FArchive& Ar)
{
	bool bCooked = Ar.IsCooking();
	Ar << bCooked;

	if (FPlatformProperties::RequiresCookedData() && !bCooked && Ar.IsLoading())
	{
		UE_LOG(LogShaders, Fatal, TEXT("This platform requires cooked packages, and shaders were not cooked into this material %s."), *GetFriendlyName());
	}

	if (bCooked)
	{
		if (Ar.IsCooking())
		{
			FinishCompilation();

			bool bValid = GameThreadShaderMap != nullptr && GameThreadShaderMap->CompiledSuccessfully();
			
			Ar << bValid;

			if (bValid)
			{
				GameThreadShaderMap->Serialize(Ar);
			}
		}
		else
		{
			bool bValid = false;
			Ar << bValid;

			if (bValid)
			{
				TRefCountPtr<FMaterialShaderMap> LoadedShaderMap = new FMaterialShaderMap();
				LoadedShaderMap->Serialize(Ar);

				// Toss the loaded shader data if this is a server only instance
				//@todo - don't cook it in the first place
				if (FApp::CanEverRender())
				{
					GameThreadShaderMap = RenderingThreadShaderMap = LoadedShaderMap;
				}
				else
				{
					GameThreadShaderMap = LoadedShaderMap;
				}
			}
		}
	}
}

void FMaterial::RegisterInlineShaderMap()
{
	if (GameThreadShaderMap)
	{
		GameThreadShaderMap->RegisterSerializedShaders();
	}
}

void FMaterialResource::LegacySerialize(FArchive& Ar)
{
	FMaterial::LegacySerialize(Ar);

	if (Ar.UE4Ver() < VER_UE4_PURGED_FMATERIAL_COMPILE_OUTPUTS)
	{
		int32 BlendModeOverrideValueTemp = 0;
		Ar << BlendModeOverrideValueTemp;
		bool bDummyBool = false;
		Ar << bDummyBool;
		Ar << bDummyBool;
	}
}

const TArray<UTexture*>& FMaterialResource::GetReferencedTextures() const
{
	return Material ? Material->ExpressionTextureReferences : UMaterial::GetDefaultMaterial(MD_Surface)->ExpressionTextureReferences;
}

void FMaterialResource::AddReferencedObjects(FReferenceCollector& Collector)
{
	FMaterial::AddReferencedObjects(Collector);

	Collector.AddReferencedObject(Material);
	Collector.AddReferencedObject(MaterialInstance);
}

bool FMaterialResource::GetAllowDevelopmentShaderCompile()const
{
	return Material->bAllowDevelopmentShaderCompile;
}

void FMaterial::ReleaseShaderMap()
{
	if (GameThreadShaderMap)
	{
		GameThreadShaderMap = nullptr;
		
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			ReleaseShaderMap,
			FMaterial*,Material,this,
		{
			Material->SetRenderingThreadShaderMap(nullptr);
		});
	}
}

void FMaterial::DiscardShaderMap()
{
	check(RenderingThreadShaderMap == nullptr);
	if (GameThreadShaderMap)
	{
		GameThreadShaderMap->DiscardSerializedShaders();
		GameThreadShaderMap = nullptr;
	}
}

EMaterialDomain FMaterialResource::GetMaterialDomain() const { return Material->MaterialDomain; }
bool FMaterialResource::IsTangentSpaceNormal() const { return Material->bTangentSpaceNormal || (!Material->Normal.IsConnected() && !Material->bUseMaterialAttributes); }
bool FMaterialResource::ShouldInjectEmissiveIntoLPV() const { return Material->bUseEmissiveForDynamicAreaLighting; }
bool FMaterialResource::ShouldBlockGI() const { return Material->bBlockGI; }
bool FMaterialResource::ShouldGenerateSphericalParticleNormals() const { return Material->bGenerateSphericalParticleNormals; }
bool FMaterialResource::ShouldDisableDepthTest() const { return Material->bDisableDepthTest; }
bool FMaterialResource::ShouldEnableResponsiveAA() const { return Material->bEnableResponsiveAA; }
bool FMaterialResource::ShouldDoSSR() const { return Material->bScreenSpaceReflections; }
bool FMaterialResource::IsWireframe() const { return Material->Wireframe; }
bool FMaterialResource::IsUIMaterial() const { return Material->MaterialDomain == MD_UI; }
bool FMaterialResource::IsLightFunction() const { return Material->MaterialDomain == MD_LightFunction; }
bool FMaterialResource::IsUsedWithEditorCompositing() const { return Material->bUsedWithEditorCompositing; }
bool FMaterialResource::IsDeferredDecal() const { return Material->MaterialDomain == MD_DeferredDecal; }
bool FMaterialResource::IsVolumetricPrimitive() const { return Material->MaterialDomain == MD_Volume; }
bool FMaterialResource::IsSpecialEngineMaterial() const { return Material->bUsedAsSpecialEngineMaterial; }
bool FMaterialResource::HasVertexPositionOffsetConnected() const { return HasMaterialAttributesConnected() || (!Material->bUseMaterialAttributes && Material->WorldPositionOffset.IsConnected()); }
bool FMaterialResource::HasPixelDepthOffsetConnected() const { return HasMaterialAttributesConnected() || (!Material->bUseMaterialAttributes && Material->PixelDepthOffset.IsConnected()); }
bool FMaterialResource::HasMaterialAttributesConnected() const { return Material->bUseMaterialAttributes && Material->MaterialAttributes.IsConnected(); }
FString FMaterialResource::GetBaseMaterialPathName() const { return Material->GetPathName(); }

bool FMaterialResource::IsUsedWithSkeletalMesh() const
{
	return Material->bUsedWithSkeletalMesh;
}

bool FMaterialResource::IsUsedWithLandscape() const
{
	return false;
}

bool FMaterialResource::IsUsedWithParticleSystem() const
{
	return Material->bUsedWithParticleSprites || Material->bUsedWithBeamTrails;
}

bool FMaterialResource::IsUsedWithParticleSprites() const
{
	return Material->bUsedWithParticleSprites;
}

bool FMaterialResource::IsUsedWithBeamTrails() const
{
	return Material->bUsedWithBeamTrails;
}

bool FMaterialResource::IsUsedWithMeshParticles() const
{
	return Material->bUsedWithMeshParticles;
}

bool FMaterialResource::IsUsedWithNiagaraSprites() const
{
	return Material->bUsedWithNiagaraSprites;
}

bool FMaterialResource::IsUsedWithNiagaraRibbons() const
{
	return Material->bUsedWithNiagaraRibbons;
}

bool FMaterialResource::IsUsedWithNiagaraMeshParticles() const
{
	return Material->bUsedWithNiagaraMeshParticles;
}

bool FMaterialResource::IsUsedWithStaticLighting() const
{
	return Material->bUsedWithStaticLighting;
}

bool FMaterialResource::IsUsedWithFlexFluidSurfaces() const
{
	return Material->bUsedWithFlexFluidSurfaces;
}

bool FMaterialResource::IsUsedWithMorphTargets() const
{
	return Material->bUsedWithMorphTargets;
}

bool FMaterialResource::IsUsedWithSplineMeshes() const
{
	return Material->bUsedWithSplineMeshes;
}

bool FMaterialResource::IsUsedWithFlexMeshes() const
{
	return Material->bUsedWithFlexMeshes;
}

bool FMaterialResource::IsUsedWithInstancedStaticMeshes() const
{
	return Material->bUsedWithInstancedStaticMeshes;
}

bool FMaterialResource::IsUsedWithAPEXCloth() const
{
	return Material->bUsedWithClothing;
}

// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI

FVxgiMaterialProperties FMaterialResource::GetVxgiMaterialProperties() const
{
	return Material->GetVxgiMaterialProperties();
}

bool FMaterialResource::IsPreviewMaterial() const
{
	return Material->bIsPreviewMaterial;
}

bool FMaterialResource::HasEmissiveColorConnected() const
{
	return Material->EmissiveColor.IsConnected();
}

#endif
// NVCHANGE_END: Add VXGI

EMaterialTessellationMode FMaterialResource::GetTessellationMode() const 
{ 
	return (EMaterialTessellationMode)Material->D3D11TessellationMode; 
}

bool FMaterialResource::IsCrackFreeDisplacementEnabled() const 
{ 
	return Material->bEnableCrackFreeDisplacement;
}

bool FMaterialResource::IsTranslucencyAfterDOFEnabled() const 
{ 
	return Material->bEnableSeparateTranslucency && !IsUIMaterial() && !IsDeferredDecal();
}

bool FMaterialResource::IsMobileSeparateTranslucencyEnabled() const
{
	return Material->bEnableMobileSeparateTranslucency && !IsUIMaterial() && !IsDeferredDecal();
}

bool FMaterialResource::IsAdaptiveTessellationEnabled() const
{
	return Material->bEnableAdaptiveTessellation;
}

bool FMaterialResource::IsFullyRough() const
{
	return Material->bFullyRough;
}

bool FMaterialResource::UseNormalCurvatureToRoughness() const
{
	return Material->bNormalCurvatureToRoughness;
}

bool FMaterialResource::IsUsingFullPrecision() const
{
	return Material->bUseFullPrecision;
}

bool FMaterialResource::IsUsingHQForwardReflections() const
{
	return Material->bUseHQForwardReflections;
}

bool FMaterialResource::IsUsingPlanarForwardReflections() const
{
	return Material->bUsePlanarForwardReflections;
}

bool FMaterialResource::OutputsVelocityOnBasePass() const
{
	return Material->bOutputVelocityOnBasePass && !IsUIMaterial();
}

bool FMaterialResource::IsNonmetal() const
{
	return !Material->bUseMaterialAttributes ?
			(!Material->Metallic.IsConnected() && !Material->Specular.IsConnected()) :
			!(Material->MaterialAttributes.IsConnected(MP_Specular) || Material->MaterialAttributes.IsConnected(MP_Metallic));
}

bool FMaterialResource::UseLmDirectionality() const
{
	return Material->bUseLightmapDirectionality;
}

/**
 * Should shaders compiled for this material be saved to disk?
 */
bool FMaterialResource::IsPersistent() const { return true; }

FGuid FMaterialResource::GetMaterialId() const
{
	return Material->StateId;
}

ETranslucencyLightingMode FMaterialResource::GetTranslucencyLightingMode() const { return (ETranslucencyLightingMode)Material->TranslucencyLightingMode; }

float FMaterialResource::GetOpacityMaskClipValue() const 
{
	return MaterialInstance ? MaterialInstance->GetOpacityMaskClipValue() : Material->GetOpacityMaskClipValue();
}

bool FMaterialResource::GetCastDynamicShadowAsMasked() const
{
	return MaterialInstance ? MaterialInstance->GetCastDynamicShadowAsMasked() : Material->GetCastDynamicShadowAsMasked();
}

EBlendMode FMaterialResource::GetBlendMode() const 
{
	return MaterialInstance ? MaterialInstance->GetBlendMode() : Material->GetBlendMode();
}

ERefractionMode FMaterialResource::GetRefractionMode() const
{
	return Material->RefractionMode;
}

EMaterialShadingModel FMaterialResource::GetShadingModel() const 
{
	return MaterialInstance ? MaterialInstance->GetShadingModel() : Material->GetShadingModel();
}

bool FMaterialResource::IsTwoSided() const 
{
	return MaterialInstance ? MaterialInstance->IsTwoSided() : Material->IsTwoSided();
}

bool FMaterialResource::IsDitheredLODTransition() const 
{
	return MaterialInstance ? MaterialInstance->IsDitheredLODTransition() : Material->IsDitheredLODTransition();
}

bool FMaterialResource::IsTranslucencyWritingCustomDepth() const
{
	return Material->IsTranslucencyWritingCustomDepth();
}

bool FMaterialResource::IsMasked() const 
{
	return MaterialInstance ? MaterialInstance->IsMasked() : Material->IsMasked();
}

bool FMaterialResource::IsDitherMasked() const 
{
	return Material->DitherOpacityMask;
}

bool FMaterialResource::AllowNegativeEmissiveColor() const 
{
	return Material->bAllowNegativeEmissiveColor;
}

bool FMaterialResource::IsDistorted() const { return Material->bUsesDistortion && IsTranslucentBlendMode(GetBlendMode()); }
float FMaterialResource::GetTranslucencyDirectionalLightingIntensity() const { return Material->TranslucencyDirectionalLightingIntensity; }
float FMaterialResource::GetTranslucentShadowDensityScale() const { return Material->TranslucentShadowDensityScale; }
float FMaterialResource::GetTranslucentSelfShadowDensityScale() const { return Material->TranslucentSelfShadowDensityScale; }
float FMaterialResource::GetTranslucentSelfShadowSecondDensityScale() const { return Material->TranslucentSelfShadowSecondDensityScale; }
float FMaterialResource::GetTranslucentSelfShadowSecondOpacity() const { return Material->TranslucentSelfShadowSecondOpacity; }
float FMaterialResource::GetTranslucentBackscatteringExponent() const { return Material->TranslucentBackscatteringExponent; }
FLinearColor FMaterialResource::GetTranslucentMultipleScatteringExtinction() const { return Material->TranslucentMultipleScatteringExtinction; }
float FMaterialResource::GetTranslucentShadowStartOffset() const { return Material->TranslucentShadowStartOffset; }
float FMaterialResource::GetRefractionDepthBiasValue() const { return Material->RefractionDepthBias; }
float FMaterialResource::GetMaxDisplacement() const { return Material->MaxDisplacement; }
bool FMaterialResource::ShouldApplyFogging() const {return Material->bUseTranslucencyVertexFog;}
bool FMaterialResource::ComputeFogPerPixel() const {return Material->bComputeFogPerPixel;}
FString FMaterialResource::GetFriendlyName() const { return *GetNameSafe(Material); }

uint32 FMaterialResource::GetDecalBlendMode() const
{
	return Material->GetDecalBlendMode();
}

uint32 FMaterialResource::GetMaterialDecalResponse() const
{
	return Material->GetMaterialDecalResponse();
}

bool FMaterialResource::HasNormalConnected() const
{
	return HasMaterialAttributesConnected() || Material->HasNormalConnected();
}

bool FMaterialResource::RequiresSynchronousCompilation() const
{
	return Material->IsDefaultMaterial();
}

bool FMaterialResource::IsDefaultMaterial() const
{
	return Material->IsDefaultMaterial();
}

int32 FMaterialResource::GetNumCustomizedUVs() const
{
	return Material->NumCustomizedUVs;
}

int32 FMaterialResource::GetBlendableLocation() const
{
	return Material->BlendableLocation;
}

bool FMaterialResource::GetBlendableOutputAlpha() const
{
	return Material->BlendableOutputAlpha;
}

UMaterialInterface* FMaterialResource::GetMaterialInterface() const 
{ 
	return MaterialInstance ? (UMaterialInterface*)MaterialInstance : (UMaterialInterface*)Material;
}

void FMaterialResource::NotifyCompilationFinished()
{
	UMaterial::NotifyCompilationFinished(MaterialInstance ? (UMaterialInterface*)MaterialInstance : (UMaterialInterface*)Material);
}

/**
 * Gets instruction counts that best represent the likely usage of this material based on shading model and other factors.
 * @param Descriptions - an array of descriptions to be populated
 * @param InstructionCounts - an array of instruction counts matching the Descriptions.  
 *		The dimensions of these arrays are guaranteed to be identical and all values are valid.
 */
void FMaterialResource::GetRepresentativeInstructionCounts(TArray<FString> &Descriptions, TArray<int32> &InstructionCounts) const
{
	TMap<FName, FString> ShaderTypeNamesAndDescriptions;

	//when adding a shader type here be sure to update FPreviewMaterial::ShouldCache()
	//so the shader type will get compiled with preview materials
	const FMaterialShaderMap* MaterialShaderMap = GetGameThreadShaderMap();
	if (MaterialShaderMap && MaterialShaderMap->IsCompilationFinalized())
	{
		GetRepresentativeShaderTypesAndDescriptions(ShaderTypeNamesAndDescriptions);

		if( IsUIMaterial() )
		{
			for (const TPair<FName, FString>& ShaderTypePair : ShaderTypeNamesAndDescriptions)
			{
				FShaderType* ShaderType = FindShaderTypeByName(ShaderTypePair.Key);
				int32 NumInstructions = MaterialShaderMap->GetMaxNumInstructionsForShader(ShaderType);
				if (NumInstructions > 0)
				{
					//if the shader was found, add it to the output arrays
					InstructionCounts.Push(NumInstructions);
					Descriptions.Push(ShaderTypePair.Value);
				}
			}
		}
		else
		{
			const FMeshMaterialShaderMap* MeshShaderMap = MaterialShaderMap->GetMeshShaderMap(&FLocalVertexFactory::StaticType);
			if (MeshShaderMap)
			{
				Descriptions.Empty();
				InstructionCounts.Empty();

				for (const TPair<FName, FString>& ShaderTypePair : ShaderTypeNamesAndDescriptions)
				{
					FShaderType* ShaderType = FindShaderTypeByName(ShaderTypePair.Key);
					if (ShaderType)
					{
						int32 NumInstructions = MeshShaderMap->GetMaxNumInstructionsForShader(ShaderType);
						if (NumInstructions > 0)
						{
							//if the shader was found, add it to the output arrays
							InstructionCounts.Push(NumInstructions);
							Descriptions.Push(ShaderTypePair.Value);
						}
					}
				}
			}
		}
	}

	check(Descriptions.Num() == InstructionCounts.Num());
}

void FMaterialResource::GetRepresentativeShaderTypesAndDescriptions(TMap<FName, FString>& ShaderTypeNamesAndDescriptions) const
{
	static auto* MobileHDR = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MobileHDR"));
	bool bMobileHDR = MobileHDR && MobileHDR->GetValueOnAnyThread() == 1;

	if( IsUIMaterial() )
	{
		static FName TSlateMaterialShaderPSDefaultfalseName = TEXT("TSlateMaterialShaderPSDefaultfalse");
		ShaderTypeNamesAndDescriptions.Add(TSlateMaterialShaderPSDefaultfalseName, TEXT("Default UI Pixel Shader"));

		static FName TSlateMaterialShaderVSfalseName = TEXT("TSlateMaterialShaderVSfalse");
		ShaderTypeNamesAndDescriptions.Add(TSlateMaterialShaderVSfalseName, TEXT("Default UI Vertex Shader"));

		static FName TSlateMaterialShaderVStrueName = TEXT("TSlateMaterialShaderVStrue");
		ShaderTypeNamesAndDescriptions.Add(TSlateMaterialShaderVStrueName, TEXT("Instanced UI Vertex Shader"));
	}
	else if (GetFeatureLevel() >= ERHIFeatureLevel::SM4)
	{
		if (GetShadingModel() == MSM_Unlit)
		{
			//unlit materials are never lightmapped
			static FName TBasePassPSFNoLightMapPolicyName = TEXT("TBasePassPSFNoLightMapPolicy");
			ShaderTypeNamesAndDescriptions.Add(TBasePassPSFNoLightMapPolicyName, TEXT("Base pass shader without light map"));
		}
		else
		{
			//also show a dynamically lit shader
			static FName TBasePassPSFNoLightMapPolicyName = TEXT("TBasePassPSFNoLightMapPolicy");
			ShaderTypeNamesAndDescriptions.Add(TBasePassPSFNoLightMapPolicyName, TEXT("Base pass shader"));

			static auto* CVarAllowStaticLighting = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.AllowStaticLighting"));
			const bool bAllowStaticLighting = CVarAllowStaticLighting->GetValueOnAnyThread() != 0;

			if (bAllowStaticLighting)
			{
				if (IsUsedWithStaticLighting())
				{
					static FName TBasePassPSTLightMapPolicyName = TEXT("TBasePassPSTDistanceFieldShadowsAndLightMapPolicyHQ");
					ShaderTypeNamesAndDescriptions.Add(TBasePassPSTLightMapPolicyName, TEXT("Base pass shader with Surface Lightmap"));
		}

				static FName TBasePassPSFPrecomputedVolumetricLightmapLightingPolicyName = TEXT("TBasePassPSFPrecomputedVolumetricLightmapLightingPolicy");
				ShaderTypeNamesAndDescriptions.Add(TBasePassPSFPrecomputedVolumetricLightmapLightingPolicyName, TEXT("Base pass shader with Volumetric Lightmap"));
			}
		}

		static FName TBasePassVSFNoLightMapPolicyName = TEXT("TBasePassVSFNoLightMapPolicy");
		ShaderTypeNamesAndDescriptions.Add(TBasePassVSFNoLightMapPolicyName, TEXT("Vertex shader"));
	}
	else
	{
		const TCHAR* DescSuffix = bMobileHDR ? TEXT(" (HDR)") : TEXT(" (LDR)");

		if (GetShadingModel() == MSM_Unlit)
		{
			//unlit materials are never lightmapped
			static FName Name_HDRLinear64 = TEXT("TBasePassForForwardShadingPSFNoLightMapPolicy0HDRLinear64");
			static FName Name_LDRGamma32 = TEXT("TBasePassForForwardShadingPSFNoLightMapPolicy0LDRGamma32");
			const FName TBasePassForForwardShadingPSFNoLightMapPolicy0Name = bMobileHDR ? Name_HDRLinear64 : Name_LDRGamma32;
			ShaderTypeNamesAndDescriptions.Add(TBasePassForForwardShadingPSFNoLightMapPolicy0Name, FString::Printf(TEXT("Mobile base pass shader without light map%s"), DescSuffix));
		}
		else
		{
			if (IsUsedWithStaticLighting())
			{
				//lit materials are usually lightmapped
				{
					static FName Name_HDRLinear64 = TEXT("TBasePassForForwardShadingPSTLightMapPolicy0LQHDRLinear64");
					static FName Name_LDRGamma32 = TEXT("TBasePassForForwardShadingPSTLightMapPolicy0LQLDRGamma32");
					static FName TSlateMaterialShaderVStrueName = bMobileHDR ? Name_HDRLinear64 : Name_LDRGamma32;
					ShaderTypeNamesAndDescriptions.Add(TSlateMaterialShaderVStrueName, FString::Printf(TEXT("Mobile base pass shader with static lighting%s"), DescSuffix));
				}

				// + distance field shadows
				{
					static FName Name_HDRLinear64 = TEXT("TBasePassForForwardShadingPSTDistanceFieldShadowsAndLightMapPolicy0LQHDRLinear64");
					static FName Name_LDRGamma32 = TEXT("TBasePassForForwardShadingPSTDistanceFieldShadowsAndLightMapPolicy0LQLDRGamma32");
					static FName TBasePassForForwardShadingPSTDistanceFieldShadowsAndLightMapPolicy0LQName = bMobileHDR ? Name_HDRLinear64 : Name_LDRGamma32;
					ShaderTypeNamesAndDescriptions.Add(TBasePassForForwardShadingPSTDistanceFieldShadowsAndLightMapPolicy0LQName, FString::Printf(TEXT("Mobile base pass shader with distance field shadows%s"), DescSuffix));
				}
			}

			//also show a dynamically lit shader
			static FName Name_HDRLinear64 = TEXT("TBasePassForForwardShadingPSFSimpleDirectionalLightAndSHIndirectPolicy0HDRLinear64");
			static FName Name_LDRGamma32 = TEXT("TBasePassForForwardShadingPSFSimpleDirectionalLightAndSHIndirectPolicy0LDRGamma32");
			const FName TBasePassForForwardShadingPSFSimpleDirectionalLightAndSHIndirectPolicy0Name = bMobileHDR ? Name_HDRLinear64 : Name_LDRGamma32;
			ShaderTypeNamesAndDescriptions.Add(TBasePassForForwardShadingPSFSimpleDirectionalLightAndSHIndirectPolicy0Name, FString::Printf(TEXT("Mobile base pass shader with only dynamic lighting%s"), DescSuffix));
		}

		{
			static FName Name_HDRLinear64 = TEXT("TBasePassForForwardShadingVSFNoLightMapPolicyHDRLinear64");
			static FName Name_LDRGamma32 = TEXT("TBasePassForForwardShadingVSFNoLightMapPolicyLDRGamma32");
			const FName TBasePassForForwardShadingVSFNoLightMapPolicyName = bMobileHDR ? Name_HDRLinear64 : Name_LDRGamma32;
			ShaderTypeNamesAndDescriptions.Add(TBasePassForForwardShadingVSFNoLightMapPolicyName, FString::Printf(TEXT("Mobile base pass vertex shader%s"), DescSuffix));
		}
	}
}

SIZE_T FMaterialResource::GetResourceSizeInclusive()
{
	FResourceSizeEx ResSize = FResourceSizeEx(EResourceSizeMode::Inclusive);
	GetResourceSizeEx(ResSize);
	return ResSize.GetTotalMemoryBytes();
}

void FMaterialResource::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	if (CumulativeResourceSize.GetResourceSizeMode() == EResourceSizeMode::Inclusive)
	{
		TSet<const FMaterialShaderMap*> UniqueShaderMaps;
		TMap<FShaderId, FShader*> UniqueShaders;
		TArray<FShaderPipeline*> ShaderPipelines;
		TSet<FShaderResourceId> UniqueShaderResourceIds;

		CumulativeResourceSize.AddDedicatedSystemMemoryBytes(sizeof(FMaterialResource));
		UniqueShaderMaps.Add(GetGameThreadShaderMap());

		for (TSet<const FMaterialShaderMap*>::TConstIterator It(UniqueShaderMaps); It; ++It)
		{
			const FMaterialShaderMap* MaterialShaderMap = *It;
			if (MaterialShaderMap)
			{
				CumulativeResourceSize.AddDedicatedSystemMemoryBytes(MaterialShaderMap->GetSizeBytes());
				MaterialShaderMap->GetShaderList(UniqueShaders);
				MaterialShaderMap->GetShaderPipelineList(ShaderPipelines);
			}
		}

		for (auto& KeyValue : UniqueShaders)
		{
			auto* Shader = KeyValue.Value;
			if (Shader)
			{
				CumulativeResourceSize.AddDedicatedSystemMemoryBytes(AddShaderSize(Shader, UniqueShaderResourceIds));
			}
		}

		for (FShaderPipeline* Pipeline : ShaderPipelines)
		{
			if (Pipeline)
			{
				for (FShader* Shader : Pipeline->GetShaders())
				{
					if (Shader)
					{
						CumulativeResourceSize.AddDedicatedSystemMemoryBytes(AddShaderSize(Shader, UniqueShaderResourceIds));
					}
				}
				CumulativeResourceSize.AddDedicatedSystemMemoryBytes(Pipeline->GetSizeBytes());
			}
		}
	}
}

/**
 * Destructor
 */
FMaterial::~FMaterial()
{
	if (GIsEditor)
	{
		const FSetElementId FoundId = EditorLoadedMaterialResources.FindId(this);
		if (FoundId.IsValidId())
		{
			// Remove the material from EditorLoadedMaterialResources if found
			EditorLoadedMaterialResources.Remove(FoundId);
		}
	}

	FMaterialShaderMap::RemovePendingMaterial(this);

	// If the material becomes invalid, then the debug view material will also be invalid
	void ClearAllDebugViewMaterials();
	ClearAllDebugViewMaterials();
}

// could be more to a more central DBuffer file
// @return e.g. 1+2+4 means DBufferA(1) + DBufferB(2) + DBufferC(4) is used
static uint8 ComputeDBufferMRTMask(EDecalBlendMode DecalBlendMode)
{
	switch(DecalBlendMode)
	{
		case DBM_DBuffer_ColorNormalRoughness:
			return 1 + 2 + 4;
		case DBM_DBuffer_Color:
			return 1;
		case DBM_DBuffer_ColorNormal:
			return 1 + 2;
		case DBM_DBuffer_ColorRoughness:
			return 1 + 4;
		case DBM_DBuffer_Normal:
			return 2;
		case DBM_DBuffer_NormalRoughness:
			return 2 + 4;
		case DBM_DBuffer_Roughness:
			return 4;
	}

	return 0;
}

/** Populates OutEnvironment with defines needed to compile shaders for this material. */
void FMaterial::SetupMaterialEnvironment(
	EShaderPlatform Platform,
	const FUniformExpressionSet& InUniformExpressionSet,
	FShaderCompilerEnvironment& OutEnvironment
	) const
{
	// Add the material uniform buffer definition.
	FShaderUniformBufferParameter::ModifyCompilationEnvironment(TEXT("Material"),InUniformExpressionSet.GetUniformBufferStruct(),Platform,OutEnvironment);

	if ((RHISupportsTessellation(Platform) == false) || (GetTessellationMode() == MTM_NoTessellation))
	{
		OutEnvironment.SetDefine(TEXT("USING_TESSELLATION"),TEXT("0"));
	}
	else
	{
		OutEnvironment.SetDefine(TEXT("USING_TESSELLATION"),TEXT("1"));
		if (GetTessellationMode() == MTM_FlatTessellation)
		{
			OutEnvironment.SetDefine(TEXT("TESSELLATION_TYPE_FLAT"),TEXT("1"));
		}
		else if (GetTessellationMode() == MTM_PNTriangles)
		{
			OutEnvironment.SetDefine(TEXT("TESSELLATION_TYPE_PNTRIANGLES"),TEXT("1"));
		}

		// This is dominant vertex/edge information.  Note, mesh must have preprocessed neighbors IB of material will fallback to default.
		//  PN triangles needs preprocessed buffers regardless of c
		if (IsCrackFreeDisplacementEnabled())
		{
			OutEnvironment.SetDefine(TEXT("DISPLACEMENT_ANTICRACK"),TEXT("1"));
		}
		else
		{
			OutEnvironment.SetDefine(TEXT("DISPLACEMENT_ANTICRACK"),TEXT("0"));
		}

		// Set whether to enable the adaptive tessellation, which tries to maintain a uniform number of pixels per triangle.
		if (IsAdaptiveTessellationEnabled())
		{
			OutEnvironment.SetDefine(TEXT("USE_ADAPTIVE_TESSELLATION_FACTOR"),TEXT("1"));
		}
		else
		{
			OutEnvironment.SetDefine(TEXT("USE_ADAPTIVE_TESSELLATION_FACTOR"),TEXT("0"));
		}
		
	}

	switch(GetBlendMode())
	{
	case BLEND_Opaque:
	case BLEND_Masked:
		{
			// Only set MATERIALBLENDING_MASKED if the material is truly masked
			//@todo - this may cause mismatches with what the shader compiles and what the renderer thinks the shader needs
			// For example IsTranslucentBlendMode doesn't check IsMasked
			if(!WritesEveryPixel())
			{
				OutEnvironment.SetDefine(TEXT("MATERIALBLENDING_MASKED"),TEXT("1"));
			}
			else
			{
				OutEnvironment.SetDefine(TEXT("MATERIALBLENDING_SOLID"),TEXT("1"));
			}
			break;
		}
	case BLEND_AlphaComposite: 
		{
			// Fall through the case, blend mode will reuse MATERIALBLENDING_TRANSLUCENT
			OutEnvironment.SetDefine(TEXT("MATERIALBLENDING_ALPHACOMPOSITE"), TEXT("1"));
		}
	case BLEND_Translucent: OutEnvironment.SetDefine(TEXT("MATERIALBLENDING_TRANSLUCENT"),TEXT("1")); break;
	case BLEND_Additive: OutEnvironment.SetDefine(TEXT("MATERIALBLENDING_ADDITIVE"),TEXT("1")); break;
	case BLEND_Modulate: OutEnvironment.SetDefine(TEXT("MATERIALBLENDING_MODULATE"),TEXT("1")); break;
	default: 
		UE_LOG(LogMaterial, Warning, TEXT("Unknown material blend mode: %u  Setting to BLEND_Opaque"),(int32)GetBlendMode());
		OutEnvironment.SetDefine(TEXT("MATERIALBLENDING_SOLID"),TEXT("1"));
	}

	{
		EMaterialDecalResponse MaterialDecalResponse = (EMaterialDecalResponse)GetMaterialDecalResponse();

		// bit 0:color/1:normal/2:roughness to enable/disable parts of the DBuffer decal effect
		int32 MaterialDecalResponseMask = 0;

		switch(MaterialDecalResponse)
		{
			case MDR_None:					MaterialDecalResponseMask = 0; break;
			case MDR_ColorNormalRoughness:	MaterialDecalResponseMask = 1 + 2 + 4; break;
			case MDR_Color:					MaterialDecalResponseMask = 1; break;
			case MDR_ColorNormal:			MaterialDecalResponseMask = 1 + 2; break;
			case MDR_ColorRoughness:		MaterialDecalResponseMask = 1 + 4; break;
			case MDR_Normal:				MaterialDecalResponseMask = 2; break;
			case MDR_NormalRoughness:		MaterialDecalResponseMask = 2 + 4; break;
			case MDR_Roughness:				MaterialDecalResponseMask = 4; break;
			default:
				check(0);
		}

		OutEnvironment.SetDefine(TEXT("MATERIALDECALRESPONSEMASK"), MaterialDecalResponseMask);
	}

	switch(GetRefractionMode())
	{
	case RM_IndexOfRefraction: OutEnvironment.SetDefine(TEXT("REFRACTION_USE_INDEX_OF_REFRACTION"),TEXT("1")); break;
	case RM_PixelNormalOffset: OutEnvironment.SetDefine(TEXT("REFRACTION_USE_PIXEL_NORMAL_OFFSET"),TEXT("1")); break;
	default: 
		UE_LOG(LogMaterial, Warning, TEXT("Unknown material refraction mode: %u  Setting to RM_IndexOfRefraction"),(int32)GetRefractionMode());
		OutEnvironment.SetDefine(TEXT("REFRACTION_USE_INDEX_OF_REFRACTION"),TEXT("1"));
	}

	OutEnvironment.SetDefine(TEXT("USE_DITHERED_LOD_TRANSITION_FROM_MATERIAL"), IsDitheredLODTransition());
	OutEnvironment.SetDefine(TEXT("MATERIAL_TWOSIDED"), IsTwoSided());
	OutEnvironment.SetDefine(TEXT("MATERIAL_TANGENTSPACENORMAL"), IsTangentSpaceNormal());
	OutEnvironment.SetDefine(TEXT("GENERATE_SPHERICAL_PARTICLE_NORMALS"),ShouldGenerateSphericalParticleNormals());
	OutEnvironment.SetDefine(TEXT("MATERIAL_USES_SCENE_COLOR_COPY"), RequiresSceneColorCopy_GameThread());
	OutEnvironment.SetDefine(TEXT("MATERIAL_FULLY_ROUGH"), IsFullyRough());
	OutEnvironment.SetDefine(TEXT("MATERIAL_HQ_FORWARD_REFLECTIONS"), IsUsingHQForwardReflections());
	OutEnvironment.SetDefine(TEXT("MATERIAL_PLANAR_FORWARD_REFLECTIONS"), IsUsingPlanarForwardReflections());
	OutEnvironment.SetDefine(TEXT("MATERIAL_NONMETAL"), IsNonmetal());
	OutEnvironment.SetDefine(TEXT("MATERIAL_USE_LM_DIRECTIONALITY"), UseLmDirectionality());
	OutEnvironment.SetDefine(TEXT("MATERIAL_INJECT_EMISSIVE_INTO_LPV"), ShouldInjectEmissiveIntoLPV());
	OutEnvironment.SetDefine(TEXT("MATERIAL_SSR"), ShouldDoSSR());
	OutEnvironment.SetDefine(TEXT("MATERIAL_BLOCK_GI"), ShouldBlockGI());
	OutEnvironment.SetDefine(TEXT("MATERIAL_DITHER_OPACITY_MASK"), IsDitherMasked());
	OutEnvironment.SetDefine(TEXT("MATERIAL_NORMAL_CURVATURE_TO_ROUGHNESS"), UseNormalCurvatureToRoughness() ? TEXT("1") : TEXT("0"));
	OutEnvironment.SetDefine(TEXT("MATERIAL_ALLOW_NEGATIVE_EMISSIVECOLOR"), AllowNegativeEmissiveColor());
	OutEnvironment.SetDefine(TEXT("MATERIAL_OUTPUT_OPACITY_AS_ALPHA"), GetBlendableOutputAlpha());
	OutEnvironment.SetDefine(TEXT("TRANSLUCENT_SHADOW_WITH_MASKED_OPACITY"), GetCastDynamicShadowAsMasked());

	if (IsUsingFullPrecision())
	{
		OutEnvironment.CompilerFlags.Add(CFLAG_UseFullPrecisionInPS);
	}

	{
		auto DecalBlendMode = (EDecalBlendMode)GetDecalBlendMode();

		uint8 bDBufferMask = ComputeDBufferMRTMask(DecalBlendMode);

		OutEnvironment.SetDefine(TEXT("MATERIAL_DBUFFERA"), (bDBufferMask & 0x1) != 0);
		OutEnvironment.SetDefine(TEXT("MATERIAL_DBUFFERB"), (bDBufferMask & 0x2) != 0);
		OutEnvironment.SetDefine(TEXT("MATERIAL_DBUFFERC"), (bDBufferMask & 0x4) != 0);
	}

	if(GetMaterialDomain() == MD_DeferredDecal)
	{
		bool bHasNormalConnected = HasNormalConnected();
		EDecalBlendMode DecalBlendMode = FDecalRenderingCommon::ComputeFinalDecalBlendMode(Platform, (EDecalBlendMode)GetDecalBlendMode(), bHasNormalConnected);
		FDecalRenderingCommon::ERenderTargetMode RenderTargetMode = FDecalRenderingCommon::ComputeRenderTargetMode(Platform, DecalBlendMode, bHasNormalConnected);
		uint32 RenderTargetCount = FDecalRenderingCommon::ComputeRenderTargetCount(Platform, RenderTargetMode);

		uint32 BindTarget1 = (RenderTargetMode == FDecalRenderingCommon::RTM_SceneColorAndGBufferNoNormal || RenderTargetMode == FDecalRenderingCommon::RTM_SceneColorAndGBufferDepthWriteNoNormal) ? 0 : 1;
		OutEnvironment.SetDefine(TEXT("BIND_RENDERTARGET1"), BindTarget1);

		// avoid using the index directly, better use DECALBLENDMODEID_VOLUMETRIC, DECALBLENDMODEID_STAIN, ...
		OutEnvironment.SetDefine(TEXT("DECAL_BLEND_MODE"), (uint32)DecalBlendMode);
		OutEnvironment.SetDefine(TEXT("DECAL_PROJECTION"), 1);
		OutEnvironment.SetDefine(TEXT("DECAL_RENDERTARGET_COUNT"), RenderTargetCount);
		OutEnvironment.SetDefine(TEXT("DECAL_RENDERSTAGE"), (uint32)FDecalRenderingCommon::ComputeRenderStage(Platform, DecalBlendMode));

		// to compare against DECAL_BLEND_MODE, we can expose more if needed
		OutEnvironment.SetDefine(TEXT("DECALBLENDMODEID_VOLUMETRIC"), (uint32)DBM_Volumetric_DistanceFunction);
		OutEnvironment.SetDefine(TEXT("DECALBLENDMODEID_STAIN"), (uint32)DBM_Stain);
		OutEnvironment.SetDefine(TEXT("DECALBLENDMODEID_NORMAL"), (uint32)DBM_Normal);
		OutEnvironment.SetDefine(TEXT("DECALBLENDMODEID_EMISSIVE"), (uint32)DBM_Emissive);
		OutEnvironment.SetDefine(TEXT("DECALBLENDMODEID_TRANSLUCENT"), (uint32)DBM_Translucent);
	}

	switch(GetMaterialDomain())
	{
		case MD_Surface:			OutEnvironment.SetDefine(TEXT("MATERIAL_DOMAIN_SURFACE"),				TEXT("1")); break;
		case MD_DeferredDecal:		OutEnvironment.SetDefine(TEXT("MATERIAL_DOMAIN_DEFERREDDECAL"),				TEXT("1")); break;
		case MD_LightFunction:		OutEnvironment.SetDefine(TEXT("MATERIAL_DOMAIN_LIGHTFUNCTION"),				TEXT("1")); break;
		case MD_Volume:				OutEnvironment.SetDefine(TEXT("MATERIAL_DOMAIN_VOLUME"),				TEXT("1")); break;
		case MD_PostProcess:		OutEnvironment.SetDefine(TEXT("MATERIAL_DOMAIN_POSTPROCESS"),				TEXT("1")); break;
		case MD_UI:					OutEnvironment.SetDefine(TEXT("MATERIAL_DOMAIN_UI"),				TEXT("1")); break;
		default:
			UE_LOG(LogMaterial, Warning, TEXT("Unknown material domain: %u  Setting to MD_Surface"),(int32)GetMaterialDomain());
			OutEnvironment.SetDefine(TEXT("MATERIAL_DOMAIN_SURFACE"),TEXT("1"));
	};

	switch(GetShadingModel())
	{
		case MSM_Unlit:				OutEnvironment.SetDefine(TEXT("MATERIAL_SHADINGMODEL_UNLIT"),				TEXT("1")); break;
		case MSM_DefaultLit:		OutEnvironment.SetDefine(TEXT("MATERIAL_SHADINGMODEL_DEFAULT_LIT"),			TEXT("1")); break;
		case MSM_Subsurface:		OutEnvironment.SetDefine(TEXT("MATERIAL_SHADINGMODEL_SUBSURFACE"),			TEXT("1")); break;
		case MSM_PreintegratedSkin: OutEnvironment.SetDefine(TEXT("MATERIAL_SHADINGMODEL_PREINTEGRATED_SKIN"),	TEXT("1")); break;
		case MSM_SubsurfaceProfile: OutEnvironment.SetDefine(TEXT("MATERIAL_SHADINGMODEL_SUBSURFACE_PROFILE"),	TEXT("1")); break;
		case MSM_ClearCoat:			OutEnvironment.SetDefine(TEXT("MATERIAL_SHADINGMODEL_CLEAR_COAT"),			TEXT("1")); break;
		case MSM_TwoSidedFoliage:	OutEnvironment.SetDefine(TEXT("MATERIAL_SHADINGMODEL_TWOSIDED_FOLIAGE"),	TEXT("1")); break;
		case MSM_Hair:				OutEnvironment.SetDefine(TEXT("MATERIAL_SHADINGMODEL_HAIR"),				TEXT("1")); break;
		case MSM_Cloth:				OutEnvironment.SetDefine(TEXT("MATERIAL_SHADINGMODEL_CLOTH"),				TEXT("1")); break;
		case MSM_Eye:				OutEnvironment.SetDefine(TEXT("MATERIAL_SHADINGMODEL_EYE"),					TEXT("1")); break;
		default:
			UE_LOG(LogMaterial, Warning, TEXT("Unknown material shading model: %u  Setting to MSM_DefaultLit"),(int32)GetShadingModel());
			OutEnvironment.SetDefine(TEXT("MATERIAL_SHADINGMODEL_DEFAULT_LIT"),TEXT("1"));
	};

	if (IsTranslucentBlendMode(GetBlendMode()))
	{
		switch(GetTranslucencyLightingMode())
		{
		case TLM_VolumetricNonDirectional: OutEnvironment.SetDefine(TEXT("TRANSLUCENCY_LIGHTING_VOLUMETRIC_NONDIRECTIONAL"),TEXT("1")); break;
		case TLM_VolumetricDirectional: OutEnvironment.SetDefine(TEXT("TRANSLUCENCY_LIGHTING_VOLUMETRIC_DIRECTIONAL"),TEXT("1")); break;
		case TLM_VolumetricPerVertexNonDirectional: OutEnvironment.SetDefine(TEXT("TRANSLUCENCY_LIGHTING_VOLUMETRIC_PERVERTEX_NONDIRECTIONAL"),TEXT("1")); break;
		case TLM_VolumetricPerVertexDirectional: OutEnvironment.SetDefine(TEXT("TRANSLUCENCY_LIGHTING_VOLUMETRIC_PERVERTEX_DIRECTIONAL"),TEXT("1")); break;
		case TLM_Surface: OutEnvironment.SetDefine(TEXT("TRANSLUCENCY_LIGHTING_SURFACE_LIGHTINGVOLUME"),TEXT("1")); break;
		case TLM_SurfacePerPixelLighting: OutEnvironment.SetDefine(TEXT("TRANSLUCENCY_LIGHTING_SURFACE_FORWARDSHADING"),TEXT("1")); break;

		default: 
			UE_LOG(LogMaterial, Warning, TEXT("Unknown lighting mode: %u"),(int32)GetTranslucencyLightingMode());
			OutEnvironment.SetDefine(TEXT("TRANSLUCENCY_LIGHTING_VOLUMETRIC_NONDIRECTIONAL"),TEXT("1")); break;
		};
	}

	if( IsUsedWithEditorCompositing() )
	{
		OutEnvironment.SetDefine(TEXT("EDITOR_PRIMITIVE_MATERIAL"),TEXT("1"));
	}

	{	
		static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.StencilForLODDither"));
		OutEnvironment.SetDefine(TEXT("USE_STENCIL_LOD_DITHER_DEFAULT"), CVar->GetValueOnAnyThread() != 0 ? 1 : 0);
	}

	{
		switch (GetMaterialDomain())
		{
			case MD_Surface:		OutEnvironment.SetDefine(TEXT("MATERIALDOMAIN_SURFACE"), 1u); break;
			case MD_DeferredDecal:	OutEnvironment.SetDefine(TEXT("MATERIALDOMAIN_DEFERREDDECAL"), 1u); break;
			case MD_LightFunction:	OutEnvironment.SetDefine(TEXT("MATERIALDOMAIN_LIGHTFUNCTION"), 1u); break;
			case MD_PostProcess:	OutEnvironment.SetDefine(TEXT("MATERIALDOMAIN_POSTPROCESS"), 1u); break;
			case MD_UI:				OutEnvironment.SetDefine(TEXT("MATERIALDOMAIN_UI"), 1u); break;
		}
	}
}

/**
 * Caches the material shaders for this material with no static parameters on the given platform.
 * This is used by material resources of UMaterials.
 */
bool FMaterial::CacheShaders(EShaderPlatform Platform, bool bApplyCompletedShaderMapForRendering)
{
	FMaterialShaderMapId NoStaticParametersId;
	GetShaderMapId(Platform, NoStaticParametersId);
	return CacheShaders(NoStaticParametersId, Platform, bApplyCompletedShaderMapForRendering);
}

/**
 * Caches the material shaders for the given static parameter set and platform.
 * This is used by material resources of UMaterialInstances.
 */
bool FMaterial::CacheShaders(const FMaterialShaderMapId& ShaderMapId, EShaderPlatform Platform, bool bApplyCompletedShaderMapForRendering)
{
	bool bSucceeded = false;

	check(ShaderMapId.BaseMaterialId.IsValid());

	// If we loaded this material with inline shaders, use what was loaded (GameThreadShaderMap) instead of looking in the DDC
	if (bContainsInlineShaders)
	{
		FMaterialShaderMap* ExistingShaderMap = nullptr;
		
		if (GameThreadShaderMap)
		{
			// Note: in the case of an inlined shader map, the shadermap Id will not be valid because we stripped some editor-only data needed to create it
			// Get the shadermap Id from the shadermap that was inlined into the package, if it exists
			ExistingShaderMap = FMaterialShaderMap::FindId(GameThreadShaderMap->GetShaderMapId(), Platform);
		}

		// Re-use an identical shader map in memory if possible, removing the reference to the inlined shader map
		if (ExistingShaderMap)
		{
			GameThreadShaderMap = ExistingShaderMap;
		}
		else if (GameThreadShaderMap)
		{
			// We are going to use the inlined shader map, register it so it can be re-used by other materials
			GameThreadShaderMap->Register(Platform);
		}
	}
	else
	{
		// Find the material's cached shader map.
		GameThreadShaderMap = FMaterialShaderMap::FindId(ShaderMapId, Platform);

		// Attempt to load from the derived data cache if we are uncooked
		if ((!GameThreadShaderMap || !GameThreadShaderMap->IsComplete(this, true)) && !FPlatformProperties::RequiresCookedData())
		{
			FMaterialShaderMap::LoadFromDerivedDataCache(this, ShaderMapId, Platform, GameThreadShaderMap);
		}
	}

	UMaterialInterface* MaterialInterface = GetMaterialInterface();
	const bool bMaterialInstance = MaterialInterface && MaterialInterface->IsA(UMaterialInstance::StaticClass());
	const bool bSpecialEngineMaterial = !bMaterialInstance && IsSpecialEngineMaterial();

	// Log which shader, pipeline or factory is missing when about to have a fatal error
	const bool bLogShaderMapFailInfo = bSpecialEngineMaterial && (bContainsInlineShaders || FPlatformProperties::RequiresCookedData());


	bool bAssumeShaderMapIsComplete = false;
#if UE_BUILD_SHIPPING || UE_BUILD_TEST
	bAssumeShaderMapIsComplete = (bContainsInlineShaders || FPlatformProperties::RequiresCookedData()) 
		&& !bLogShaderMapFailInfo; // if it is the special engine material, we will check it
#endif

	if (GameThreadShaderMap && GameThreadShaderMap->TryToAddToExistingCompilationTask(this))
	{
		//FMaterialShaderMap::ShaderMapsBeingCompiled.Find(GameThreadShaderMap);
#if DEBUG_INFINITESHADERCOMPILE
		UE_LOG(LogTemp, Display, TEXT("Found existing compiling shader for material %s, linking to other GameThreadShaderMap 0x%08X%08X"), *GetFriendlyName(), (int)((int64)(GameThreadShaderMap.GetReference()) >> 32), (int)((int64)(GameThreadShaderMap.GetReference())) );
#endif
		OutstandingCompileShaderMapIds.AddUnique(GameThreadShaderMap->GetCompilingId());
		// Reset the shader map so the default material will be used until the compile finishes.
		GameThreadShaderMap = nullptr;
		bSucceeded = true;
	}
	else if (!GameThreadShaderMap || !(bAssumeShaderMapIsComplete || GameThreadShaderMap->IsComplete(this, !bLogShaderMapFailInfo)))
	{
		if (bContainsInlineShaders || FPlatformProperties::RequiresCookedData())
		{
			if (bSpecialEngineMaterial)
			{
				UMaterialInterface* Interface = GetMaterialInterface();
				FString Instance;
				if (Interface)
				{
					Instance = Interface->GetPathName();
				}

				//assert if the default material's shader map was not found, since it will cause problems later
				UE_LOG(LogMaterial, Fatal,TEXT("Failed to find shader map for default material %s(%s)! Please make sure cooking was successful (%s inline shaders, %s GTSM%s)"),
					*GetFriendlyName(),
					*Instance,
					bContainsInlineShaders ? TEXT("Contains") : TEXT("No"),
					GameThreadShaderMap ? TEXT("has") : TEXT("null"),
					bAssumeShaderMapIsComplete ? TEXT(" assumes map complete") : TEXT("")
				);
			}
			else
			{
				UE_LOG(LogMaterial, Log, TEXT("Can't compile %s with cooked content, will use default material instead"), *GetFriendlyName());
			}

			// Reset the shader map so the default material will be used.
			GameThreadShaderMap = nullptr;
		}
		else
		{
			const TCHAR* ShaderMapCondition;
			if (GameThreadShaderMap)
			{
				ShaderMapCondition = TEXT("Incomplete");
			}
			else
			{
				ShaderMapCondition = TEXT("Missing");
			}
			UE_LOG(LogMaterial, Log, TEXT("%s cached shader map for material %s, compiling. %s"),ShaderMapCondition,*GetFriendlyName(), IsSpecialEngineMaterial() ? TEXT("Is special engine material.") : TEXT("") );

			// If there's no cached shader map for this material, compile a new one.
			// This is just kicking off the async compile, GameThreadShaderMap will not be complete yet
			bSucceeded = BeginCompileShaderMap(ShaderMapId, Platform, GameThreadShaderMap, bApplyCompletedShaderMapForRendering);

			if (!bSucceeded)
			{
				// If it failed to compile the material, reset the shader map so the material isn't used.
				GameThreadShaderMap = nullptr;

				if (IsDefaultMaterial())
				{
					for (int32 ErrorIndex = 0; ErrorIndex < CompileErrors.Num(); ErrorIndex++)
					{
						// Always log material errors in an unsuppressed category
						UE_LOG(LogMaterial, Warning, TEXT("	%s"), *CompileErrors[ErrorIndex]);
					}

					// Assert if the default material could not be compiled, since there will be nothing for other failed materials to fall back on.
					UE_LOG(LogMaterial, Fatal,TEXT("Failed to compile default material %s!"), *GetFriendlyName());
				}
			}
		}
	}
	else
	{
		bSucceeded = true;
	}

	// Note: this is safe to set from the game thread because we should be between the RenderFences of an FMaterialUpdateContext
	RenderingThreadShaderMap = GameThreadShaderMap;

	return bSucceeded;
}

/**
* Compiles this material for Platform, storing the result in OutShaderMap
*
* @param ShaderMapId - the set of static parameters to compile
* @param Platform - the platform to compile for
* @param OutShaderMap - the shader map to compile
* @return - true if compile succeeded or was not necessary (shader map for ShaderMapId was found and was complete)
*/
bool FMaterial::BeginCompileShaderMap(
	const FMaterialShaderMapId& ShaderMapId, 
	EShaderPlatform Platform, 
	TRefCountPtr<FMaterialShaderMap>& OutShaderMap, 
	bool bApplyCompletedShaderMapForRendering)
{
#if WITH_EDITORONLY_DATA
	bool bSuccess = false;

	STAT(double MaterialCompileTime = 0);

	TRefCountPtr<FMaterialShaderMap> NewShaderMap = new FMaterialShaderMap();

	SCOPE_SECONDS_COUNTER(MaterialCompileTime);

	// Generate the material shader code.
	FMaterialCompilationOutput NewCompilationOutput;
	FHLSLMaterialTranslator MaterialTranslator(this,NewCompilationOutput,ShaderMapId.ParameterSet,Platform,GetQualityLevel(),ShaderMapId.FeatureLevel);
	bSuccess = MaterialTranslator.Translate();

	if(bSuccess)
	{
		// Create a shader compiler environment for the material that will be shared by all jobs from this material
		TRefCountPtr<FShaderCompilerEnvironment> MaterialEnvironment = new FShaderCompilerEnvironment();

		MaterialTranslator.GetMaterialEnvironment(Platform, *MaterialEnvironment);
		const FString MaterialShaderCode = MaterialTranslator.GetMaterialShaderCode();
		const bool bSynchronousCompile = RequiresSynchronousCompilation() || !GShaderCompilingManager->AllowAsynchronousShaderCompiling();

		MaterialEnvironment->IncludeVirtualPathToContentsMap.Add(TEXT("/Engine/Generated/Material.ush"), StringToArray<ANSICHAR>(*MaterialShaderCode, MaterialShaderCode.Len() + 1));

		// Compile the shaders for the material.
		NewShaderMap->Compile(this, ShaderMapId, MaterialEnvironment, NewCompilationOutput, Platform, bSynchronousCompile, bApplyCompletedShaderMapForRendering);

		if (bSynchronousCompile)
		{
			// If this is a synchronous compile, assign the compile result to the output
			OutShaderMap = NewShaderMap->CompiledSuccessfully() ? NewShaderMap : nullptr;
		}
		else
		{
#if DEBUG_INFINITESHADERCOMPILE
			UE_LOG(LogTemp, Display, TEXT("Kicking off shader compilation for %s, GameThreadShaderMap 0x%08X%08X"), *GetFriendlyName(), (int)((int64)(NewShaderMap.GetReference()) >> 32), (int)((int64)(NewShaderMap.GetReference())));
#endif
			OutstandingCompileShaderMapIds.AddUnique( NewShaderMap->GetCompilingId() );
			// Async compile, use NULL so that rendering will fall back to the default material.
			OutShaderMap = nullptr;
		}
	}

	INC_FLOAT_STAT_BY(STAT_ShaderCompiling_MaterialCompiling,(float)MaterialCompileTime);
	INC_FLOAT_STAT_BY(STAT_ShaderCompiling_MaterialShaders,(float)MaterialCompileTime);

	return bSuccess;
#else
	UE_LOG(LogMaterial, Fatal,TEXT("Not supported."));
	return false;
#endif
}

/**
 * Should the shader for this material with the given platform, shader type and vertex 
 * factory type combination be compiled
 *
 * @param Platform		The platform currently being compiled for
 * @param ShaderType	Which shader is being compiled
 * @param VertexFactory	Which vertex factory is being compiled (can be NULL)
 *
 * @return true if the shader should be compiled
 */
bool FMaterial::ShouldCache(EShaderPlatform Platform, const FShaderType* ShaderType, const FVertexFactoryType* VertexFactoryType) const
{
	return true;
}

//
// FColoredMaterialRenderProxy implementation.
//

const FMaterial* FColoredMaterialRenderProxy::GetMaterial(ERHIFeatureLevel::Type InFeatureLevel) const
{
	return Parent->GetMaterial(InFeatureLevel);
}

/**
 * Finds the shader matching the template type and the passed in vertex factory, asserts if not found.
 */
FShader* FMaterial::GetShader(FMeshMaterialShaderType* ShaderType, FVertexFactoryType* VertexFactoryType) const
{
#if WITH_EDITOR && DO_CHECK
	// Attempt to get some more info for a rare crash (UE-35937)
	FMaterialShaderMap* GameThreadShaderMapPtr = GameThreadShaderMap;
	checkf( RenderingThreadShaderMap, TEXT("RenderingThreadShaderMap was NULL (GameThreadShaderMap is %p). This may relate to bug UE-35937"), GameThreadShaderMapPtr );
#endif
	const FMeshMaterialShaderMap* MeshShaderMap = RenderingThreadShaderMap->GetMeshShaderMap(VertexFactoryType);
	FShader* Shader = MeshShaderMap ? MeshShaderMap->GetShader(ShaderType) : nullptr;
	if (!Shader)
	{
		// we don't care about thread safety because we are about to crash 
		const auto CachedGameThreadShaderMap = GameThreadShaderMap;
		const auto CachedGameMeshShaderMap = CachedGameThreadShaderMap ? CachedGameThreadShaderMap->GetMeshShaderMap(VertexFactoryType) : nullptr;
		bool bShaderWasFoundInGameShaderMap = CachedGameMeshShaderMap && CachedGameMeshShaderMap->GetShader(ShaderType) != nullptr;

		// Get the ShouldCache results that determine whether the shader should be compiled
		auto ShaderPlatform = GShaderPlatformForFeatureLevel[GetFeatureLevel()];
		bool bMaterialShouldCache = ShouldCache(ShaderPlatform, ShaderType, VertexFactoryType);
		bool bVFShouldCache = VertexFactoryType->ShouldCache(ShaderPlatform, this, ShaderType);
		bool bShaderShouldCache = ShaderType->ShouldCache(ShaderPlatform, this, VertexFactoryType);
		FString MaterialUsage = GetMaterialUsageDescription();

		int BreakPoint = 0;

		// Assert with detailed information if the shader wasn't found for rendering.  
		// This is usually the result of an incorrect ShouldCache function.
		UE_LOG(LogMaterial, Fatal,
			TEXT("Couldn't find Shader %s for Material Resource %s!\n")
			TEXT("		RenderMeshShaderMap %d, RenderThreadShaderMap %d\n")
			TEXT("		GameMeshShaderMap %d, GameThreadShaderMap %d, bShaderWasFoundInGameShaderMap %d\n")
			TEXT("		With VF=%s, Platform=%s\n")
			TEXT("		ShouldCache: Mat=%u, VF=%u, Shader=%u \n")
			TEXT("		MaterialUsageDesc: %s"),
			ShaderType->GetName(),  *GetFriendlyName(),
			MeshShaderMap != nullptr, RenderingThreadShaderMap != nullptr,
			CachedGameMeshShaderMap != nullptr, CachedGameThreadShaderMap != nullptr, bShaderWasFoundInGameShaderMap,
			VertexFactoryType->GetName(), *LegacyShaderPlatformToShaderFormat(ShaderPlatform).ToString(),
			bMaterialShouldCache, bVFShouldCache, bShaderShouldCache,
			*MaterialUsage
			);
	}

	return Shader;
}

FShaderPipeline* FMaterial::GetShaderPipeline(class FShaderPipelineType* ShaderPipelineType, FVertexFactoryType* VertexFactoryType, bool bFatalIfNotFound) const
{
	const FMeshMaterialShaderMap* MeshShaderMap = RenderingThreadShaderMap->GetMeshShaderMap(VertexFactoryType);
	FShaderPipeline* ShaderPipeline = MeshShaderMap ? MeshShaderMap->GetShaderPipeline(ShaderPipelineType) : nullptr;
	if (!ShaderPipeline && bFatalIfNotFound)
	{
		// Get the ShouldCache results that determine whether the shader should be compiled
		auto ShaderPlatform = GShaderPlatformForFeatureLevel[GetFeatureLevel()];
		FString MaterialUsage = GetMaterialUsageDescription();

		UE_LOG(LogMaterial, Error,
			TEXT("Couldn't find ShaderPipeline %s for Material Resource %s!"), ShaderPipelineType->GetName(), *GetFriendlyName());

		for (auto* ShaderType : ShaderPipelineType->GetStages())
		{
			FShader* Shader = MeshShaderMap ? MeshShaderMap->GetShader((FShaderType*)ShaderType) : RenderingThreadShaderMap->GetShader((FShaderType*)ShaderType);
			if (!Shader)
			{
				UE_LOG(LogMaterial, Error, TEXT("Missing %s shader %s!"), GetShaderFrequencyString(ShaderType->GetFrequency(), false), ShaderType->GetName());
			}
			else if (ShaderType->GetMeshMaterialShaderType())
			{
				bool bMaterialShouldCache = ShouldCache(ShaderPlatform, ShaderType->GetMeshMaterialShaderType(), VertexFactoryType);
				bool bVFShouldCache = VertexFactoryType->ShouldCache(ShaderPlatform, this, ShaderType->GetMeshMaterialShaderType());
				bool bShaderShouldCache = ShaderType->GetMeshMaterialShaderType()->ShouldCache(ShaderPlatform, this, VertexFactoryType);

				UE_LOG(LogMaterial, Error, TEXT("%s %s ShouldCache: Mat=%u, VF=%u, Shader=%u"),
					GetShaderFrequencyString(ShaderType->GetFrequency(), false), ShaderType->GetName(), bMaterialShouldCache, bVFShouldCache, bShaderShouldCache);
			}
			else if (ShaderType->GetMaterialShaderType())
			{
				bool bMaterialShouldCache = ShouldCache(ShaderPlatform, ShaderType->GetMaterialShaderType(), VertexFactoryType);
				bool bShaderShouldCache = ShaderType->GetMaterialShaderType()->ShouldCache(ShaderPlatform, this);

				UE_LOG(LogMaterial, Error, TEXT("%s %s ShouldCache: Mat=%u, NO VF, Shader=%u"),
					GetShaderFrequencyString(ShaderType->GetFrequency(), false), ShaderType->GetName(), bMaterialShouldCache, bShaderShouldCache);
			}
		}

		int BreakPoint = 0;

		// Assert with detailed information if the shader wasn't found for rendering.  
		// This is usually the result of an incorrect ShouldCache function.
		UE_LOG(LogMaterial, Fatal,
			TEXT("		With VF=%s, Platform=%s\n")
			TEXT("		MaterialUsageDesc: %s"),
			ShaderPipelineType->GetName(), *GetFriendlyName(),
			VertexFactoryType->GetName(), *LegacyShaderPlatformToShaderFormat(ShaderPlatform).ToString(),
			*MaterialUsage
			);
	}

	return ShaderPipeline;
}

/** Returns the index to the Expression in the Expressions array, or -1 if not found. */
int32 FMaterial::FindExpression( const TArray<TRefCountPtr<FMaterialUniformExpressionTexture> >&Expressions, const FMaterialUniformExpressionTexture &Expression )
{
	for (int32 ExpressionIndex = 0; ExpressionIndex < Expressions.Num(); ++ExpressionIndex)
	{
		if ( Expressions[ExpressionIndex]->IsIdentical(&Expression) )
		{
			return ExpressionIndex;
		}
	}
	return -1;
}

TSet<FMaterial*> FMaterial::EditorLoadedMaterialResources;

/*-----------------------------------------------------------------------------
	FMaterialRenderContext
-----------------------------------------------------------------------------*/

/** 
 * Constructor
 */
FMaterialRenderContext::FMaterialRenderContext(
	const FMaterialRenderProxy* InMaterialRenderProxy,
	const FMaterial& InMaterial,
	const FSceneView* InView)
		: MaterialRenderProxy(InMaterialRenderProxy)
		, Material(InMaterial)
		, Time(0.0f)
		, RealTime(0.0f)
{
	bShowSelection = GIsEditor && InView && InView->Family->EngineShowFlags.Selection;

	// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
	// Normally, selected dynamic objects get a color added to their EmissiveColor, but we don't want that for voxelization
	bShowSelection = bShowSelection && !InView->bIsVxgiVoxelization;
#endif
	// NVCHANGE_END: Add VXGI
}

/*-----------------------------------------------------------------------------
	FMaterialRenderProxy
-----------------------------------------------------------------------------*/

void FMaterialRenderProxy::EvaluateUniformExpressions(FUniformExpressionCache& OutUniformExpressionCache, const FMaterialRenderContext& Context, FRHICommandList* CommandListIfLocalMode) const
{
	check(IsInParallelRenderingThread());

	SCOPE_CYCLE_COUNTER(STAT_CacheUniformExpressions);
	
	// Retrieve the material's uniform expression set.
	const FUniformExpressionSet& UniformExpressionSet = Context.Material.GetRenderingThreadShaderMap()->GetUniformExpressionSet();

	OutUniformExpressionCache.CachedUniformExpressionShaderMap = Context.Material.GetRenderingThreadShaderMap();

	// Create and cache the material's uniform buffer.
	OutUniformExpressionCache.UniformBuffer = UniformExpressionSet.CreateUniformBuffer(Context, CommandListIfLocalMode, &OutUniformExpressionCache.LocalUniformBuffer);

	OutUniformExpressionCache.ParameterCollections = UniformExpressionSet.ParameterCollections;

	OutUniformExpressionCache.bUpToDate = true;
}

void FMaterialRenderProxy::CacheUniformExpressions()
{
	// Register the render proxy's as a render resource so it can receive notifications to free the uniform buffer.
	InitResource();

	bool bUsingNewLoader = EVENT_DRIVEN_ASYNC_LOAD_ACTIVE_AT_RUNTIME && GEventDrivenLoaderEnabled;

	check((bUsingNewLoader && GIsInitialLoad) || // The EDL at boot time maybe not load the default materials first; we need to intialize materials before the default materials are done
		UMaterial::GetDefaultMaterial(MD_Surface));

	DeferredUniformExpressionCacheRequests.Add(this);

	UMaterialInterface::IterateOverActiveFeatureLevels([&](ERHIFeatureLevel::Type InFeatureLevel)
	{
		InvalidateUniformExpressionCache();
	});

	if (!GDeferUniformExpressionCaching)
	{
		FMaterialRenderProxy::UpdateDeferredCachedUniformExpressions();
	}
}

void FMaterialRenderProxy::CacheUniformExpressions_GameThread()
{
	if (FApp::CanEverRender())
	{
		FMaterialRenderProxy* RenderProxy = this;
		ENQUEUE_RENDER_COMMAND(FCacheUniformExpressionsCommand)(
			[RenderProxy](FRHICommandListImmediate& RHICmdList)
			{
				RenderProxy->CacheUniformExpressions();
			});
	}
}

void FMaterialRenderProxy::InvalidateUniformExpressionCache()
{
	check(IsInRenderingThread());
	for (int32 i = 0; i < ERHIFeatureLevel::Num; ++i)
	{
		UniformExpressionCache[i].bUpToDate = false;
		UniformExpressionCache[i].UniformBuffer.SafeRelease();
		UniformExpressionCache[i].CachedUniformExpressionShaderMap = nullptr;
	}
}

FMaterialRenderProxy::FMaterialRenderProxy()
	: bSelected(false)
	, bHovered(false)
	, SubsurfaceProfileRT(0)
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	, DeletedFlag(0)
	, bIsStaticDrawListReferenced(0)
#endif
{
}

FMaterialRenderProxy::FMaterialRenderProxy(bool bInSelected, bool bInHovered)
	: bSelected(bInSelected)
	, bHovered(bInHovered)
	, SubsurfaceProfileRT(0)
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	, DeletedFlag(0)
	, bIsStaticDrawListReferenced(0)
#endif
{
}

FMaterialRenderProxy::~FMaterialRenderProxy()
{
	// Removed for now to work around UE-31636. Re-enable when the underlying bug is fixed!
	//check(!IsReferencedInDrawList());

	if(IsInitialized())
	{
		check(IsInRenderingThread());
		ReleaseResource();
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	DeletedFlag = 1;
#endif
}

void FMaterialRenderProxy::InitDynamicRHI()
{
	// MaterialRenderProxyMap is only used by shader compiling
	if (!FPlatformProperties::RequiresCookedData())
	{
		FMaterialRenderProxy::MaterialRenderProxyMap.Add(this);
	}
}

void FMaterialRenderProxy::ReleaseDynamicRHI()
{
	if (!FPlatformProperties::RequiresCookedData())
	{
		FMaterialRenderProxy::MaterialRenderProxyMap.Remove(this);
	}

	DeferredUniformExpressionCacheRequests.Remove(this);

	InvalidateUniformExpressionCache();

	FExternalTextureRegistry::Get().RemoveMaterialRenderProxyReference(this);
}

void FMaterialRenderProxy::UpdateDeferredCachedUniformExpressions()
{
	check(IsInRenderingThread());

	for (TSet<FMaterialRenderProxy*>::TConstIterator It(DeferredUniformExpressionCacheRequests); It; ++It)
	{
		FMaterialRenderProxy* MaterialProxy = *It;

		UMaterialInterface::IterateOverActiveFeatureLevels([&](ERHIFeatureLevel::Type InFeatureLevel)
		{
			const FMaterial* MaterialNoFallback = MaterialProxy->GetMaterialNoFallback(InFeatureLevel);

			if (MaterialNoFallback && MaterialNoFallback->GetRenderingThreadShaderMap())
			{
				const FMaterial* Material = MaterialProxy->GetMaterial(InFeatureLevel);

				// Do not cache uniform expressions for fallback materials. This step could
				// be skipped where we don't allow for asynchronous shader compiling.
				bool bIsFallbackMaterial = (Material != MaterialNoFallback);

				if (!bIsFallbackMaterial)
				{
					FMaterialRenderContext MaterialRenderContext(MaterialProxy, *Material, nullptr);
					MaterialRenderContext.bShowSelection = GIsEditor;
					MaterialProxy->EvaluateUniformExpressions(MaterialProxy->UniformExpressionCache[(int32)InFeatureLevel], MaterialRenderContext);
				}
			}
		});
	}

	DeferredUniformExpressionCacheRequests.Reset();
}

TSet<FMaterialRenderProxy*> FMaterialRenderProxy::MaterialRenderProxyMap;
TSet<FMaterialRenderProxy*> FMaterialRenderProxy::DeferredUniformExpressionCacheRequests;

/*-----------------------------------------------------------------------------
	FColoredMaterialRenderProxy
-----------------------------------------------------------------------------*/

bool FColoredMaterialRenderProxy::GetVectorValue(const FName ParameterName, FLinearColor* OutValue, const FMaterialRenderContext& Context) const
{
	if(ParameterName == ColorParamName)
	{
		*OutValue = Color;
		return true;
	}
	else
	{
		return Parent->GetVectorValue(ParameterName, OutValue, Context);
	}
}

bool FColoredMaterialRenderProxy::GetScalarValue(const FName ParameterName, float* OutValue, const FMaterialRenderContext& Context) const
{
	return Parent->GetScalarValue(ParameterName, OutValue, Context);
}

bool FColoredMaterialRenderProxy::GetTextureValue(const FName ParameterName,const UTexture** OutValue, const FMaterialRenderContext& Context) const
{
	return Parent->GetTextureValue(ParameterName,OutValue,Context);
}

/*-----------------------------------------------------------------------------
	FLightingDensityMaterialRenderProxy
-----------------------------------------------------------------------------*/
static FName NAME_LightmapRes = FName(TEXT("LightmapRes"));

bool FLightingDensityMaterialRenderProxy::GetVectorValue(const FName ParameterName, FLinearColor* OutValue, const FMaterialRenderContext& Context) const
{
	if (ParameterName == NAME_LightmapRes)
	{
		*OutValue = FLinearColor(LightmapResolution.X, LightmapResolution.Y, 0.0f, 0.0f);
		return true;
	}
	return FColoredMaterialRenderProxy::GetVectorValue(ParameterName, OutValue, Context);
}

/*-----------------------------------------------------------------------------
	FOverrideSelectionColorMaterialRenderProxy
-----------------------------------------------------------------------------*/

const FMaterial* FOverrideSelectionColorMaterialRenderProxy::GetMaterial(ERHIFeatureLevel::Type InFeatureLevel) const
{
	return Parent->GetMaterial(InFeatureLevel);
}

bool FOverrideSelectionColorMaterialRenderProxy::GetVectorValue(const FName ParameterName, FLinearColor* OutValue, const FMaterialRenderContext& Context) const
{
	if( ParameterName == NAME_SelectionColor )
	{
		*OutValue = SelectionColor;
		return true;
	}
	else
	{
		return Parent->GetVectorValue(ParameterName, OutValue, Context);
	}
}

bool FOverrideSelectionColorMaterialRenderProxy::GetScalarValue(const FName ParameterName, float* OutValue, const FMaterialRenderContext& Context) const
{
	return Parent->GetScalarValue(ParameterName,OutValue,Context);
}

bool FOverrideSelectionColorMaterialRenderProxy::GetTextureValue(const FName ParameterName,const UTexture** OutValue, const FMaterialRenderContext& Context) const
{
	return Parent->GetTextureValue(ParameterName,OutValue,Context);
}

/** Returns the number of samplers used in this material, or -1 if the material does not have a valid shader map (compile error or still compiling). */
int32 FMaterialResource::GetSamplerUsage() const
{
	if (GetGameThreadShaderMap())
	{
		return GetGameThreadShaderMap()->GetMaxTextureSamplers();
	}

	return -1;
}

void FMaterialResource::GetUserInterpolatorUsage(uint32& NumUsedUVScalars, uint32& NumUsedCustomInterpolatorScalars) const
{
	NumUsedUVScalars = NumUsedCustomInterpolatorScalars = 0;

	if (const FMaterialShaderMap* ShaderMap = GetGameThreadShaderMap())
	{
		NumUsedUVScalars = ShaderMap->GetNumUsedUVScalars();
		NumUsedCustomInterpolatorScalars = ShaderMap->GetNumUsedCustomInterpolatorScalars();
	}
}

FString FMaterialResource::GetMaterialUsageDescription() const
{
	check(Material);
	FString BaseDescription = FString::Printf(
		TEXT("LightingModel=%s, BlendMode=%s, "),
		*GetShadingModelString(GetShadingModel()), *GetBlendModeString(GetBlendMode()));

	// this changed from ",SpecialEngine, TwoSided" to ",SpecialEngine=1, TwoSided=1, TSNormal=0, ..." to be more readable
	BaseDescription += FString::Printf(
		TEXT("SpecialEngine=%d, TwoSided=%d, TSNormal=%d, Masked=%d, Distorted=%d, WritesEveryPixel=%d, ModifiesMeshPosition=%d")
		TEXT(", Usage={"),
		(int32)IsSpecialEngineMaterial(), (int32)IsTwoSided(), (int32)IsTangentSpaceNormal(), (int32)IsMasked(), (int32)IsDistorted(), (int32)WritesEveryPixel(), (int32)MaterialMayModifyMeshPosition()
		);

	bool bFirst = true;
	for (int32 MaterialUsageIndex = 0; MaterialUsageIndex < MATUSAGE_MAX; MaterialUsageIndex++)
	{
		if (Material->GetUsageByFlag((EMaterialUsage)MaterialUsageIndex))
		{
			if (!bFirst)
			{
				BaseDescription += FString(TEXT(","));
			}
			BaseDescription += Material->GetUsageName((EMaterialUsage)MaterialUsageIndex);
			bFirst = false;
		}
	}
	BaseDescription += FString(TEXT("}"));

	return BaseDescription;
}

void FMaterial::GetDependentShaderAndVFTypes(EShaderPlatform Platform, TArray<FShaderType*>& OutShaderTypes, TArray<const FShaderPipelineType*>& OutShaderPipelineTypes, TArray<FVertexFactoryType*>& OutVFTypes) const
{
	const bool bHasTessellation = GetTessellationMode() != MTM_NoTessellation;

	// Iterate over all vertex factory types.
	for (TLinkedList<FVertexFactoryType*>::TIterator VertexFactoryTypeIt(FVertexFactoryType::GetTypeList()); VertexFactoryTypeIt; VertexFactoryTypeIt.Next())
	{
		FVertexFactoryType* VertexFactoryType = *VertexFactoryTypeIt;
		check(VertexFactoryType);

		if (VertexFactoryType->IsUsedWithMaterials())
		{
			bool bAddedTypeFromThisVF = false;

			// Iterate over all mesh material shader types.
			for(TLinkedList<FShaderType*>::TIterator ShaderTypeIt(FShaderType::GetTypeList());ShaderTypeIt;ShaderTypeIt.Next())
			{
				FMeshMaterialShaderType* ShaderType = ShaderTypeIt->GetMeshMaterialShaderType();

				if (ShaderType && 
					ShaderType->ShouldCache(Platform, this, VertexFactoryType) && 
					ShouldCache(Platform, ShaderType, VertexFactoryType) &&
					VertexFactoryType->ShouldCache(Platform, this, ShaderType)
					)
				{
					bAddedTypeFromThisVF = true;
					OutShaderTypes.AddUnique(ShaderType);
				}
			}

			for (TLinkedList<FShaderPipelineType*>::TIterator PipelineTypeIt(FShaderPipelineType::GetTypeList()); PipelineTypeIt; PipelineTypeIt.Next())
			{
				auto* PipelineType = *PipelineTypeIt;
				if (PipelineType->IsMeshMaterialTypePipeline() && PipelineType->HasTessellation() == bHasTessellation)
				{
					int32 NumShouldCache = 0;
					auto& ShaderStages = PipelineType->GetStages();
					for (const FShaderType* Type : ShaderStages)
					{
						const FMeshMaterialShaderType* ShaderType = Type->GetMeshMaterialShaderType();
						if (ShaderType->ShouldCache(Platform, this, VertexFactoryType) &&
							ShouldCache(Platform, ShaderType, VertexFactoryType) &&
							VertexFactoryType->ShouldCache(Platform, this, ShaderType)
							)
						{
							++NumShouldCache;
						}
					}

					if (NumShouldCache == ShaderStages.Num())
					{
						bAddedTypeFromThisVF = true;
						OutShaderPipelineTypes.AddUnique(PipelineType);
						for (const FShaderType* Type : ShaderStages)
						{
							OutShaderTypes.AddUnique((FShaderType*)Type);
						}
					}
				}
			}

			if (bAddedTypeFromThisVF)
			{
				OutVFTypes.Add(VertexFactoryType);
			}
		}
	}

	// Iterate over all material shader types.
	for (TLinkedList<FShaderType*>::TIterator ShaderTypeIt(FShaderType::GetTypeList()); ShaderTypeIt; ShaderTypeIt.Next())
	{
		FMaterialShaderType* ShaderType = ShaderTypeIt->GetMaterialShaderType();

		if (ShaderType && 
			ShaderType->ShouldCache(Platform, this) && 
			ShouldCache(Platform, ShaderType, nullptr)
			)
		{
			OutShaderTypes.Add(ShaderType);
		}
	}

	for (TLinkedList<FShaderPipelineType*>::TIterator PipelineTypeIt(FShaderPipelineType::GetTypeList()); PipelineTypeIt; PipelineTypeIt.Next())
	{
		auto* PipelineType = *PipelineTypeIt;
		if (PipelineType->IsMaterialTypePipeline() && PipelineType->HasTessellation() == bHasTessellation)
		{
			int32 NumShouldCache = 0;
			auto& ShaderStages = PipelineType->GetStages();
			for (const FShaderType* Type : ShaderStages)
			{
				const FMaterialShaderType* ShaderType = Type->GetMaterialShaderType();
				if (ShaderType &&
					ShaderType->ShouldCache(Platform, this) &&
					ShouldCache(Platform, ShaderType, nullptr)
					)
				{
					++NumShouldCache;
				}
			}

			if (NumShouldCache == ShaderStages.Num())
			{
				for (const FShaderType* Type : ShaderStages)
				{
					const FMaterialShaderType* ShaderType = Type->GetMaterialShaderType();
					OutShaderPipelineTypes.AddUnique(PipelineType);
					OutShaderTypes.AddUnique((FShaderType*)Type);
				}
			}
		}
	}

	// Sort by name so that we get deterministic keys
	OutShaderTypes.Sort(FCompareShaderTypes());
	OutVFTypes.Sort(FCompareVertexFactoryTypes());
	OutShaderPipelineTypes.Sort(FCompareShaderPipelineNameTypes());
}

void FMaterial::GetReferencedTexturesHash(EShaderPlatform Platform, FSHAHash& OutHash) const
{
	FSHA1 HashState;

	const TArray<UTexture*>& ReferencedTextures = GetReferencedTextures();
	// Hash the names of the uniform expression textures to capture changes in their order or values resulting from material compiler code changes
	for (int32 TextureIndex = 0; TextureIndex < ReferencedTextures.Num(); TextureIndex++)
	{
		FString TextureName;

		if (ReferencedTextures[TextureIndex])
		{
			TextureName = ReferencedTextures[TextureIndex]->GetName();
		}

		HashState.UpdateWithString(*TextureName, TextureName.Len());
	}

	UMaterialShaderQualitySettings* MaterialShaderQualitySettings = UMaterialShaderQualitySettings::Get();
	if(MaterialShaderQualitySettings->HasPlatformQualitySettings(Platform, QualityLevel))
	{
		MaterialShaderQualitySettings->GetShaderPlatformQualitySettings(Platform)->AppendToHashState(QualityLevel, HashState);
	}

	HashState.Final();
	HashState.GetHash(&OutHash.Hash[0]);
}

/**
 * Get user source code for the material, with a list of code snippets to highlight representing the code for each MaterialExpression
 * @param OutSource - generated source code
 * @param OutHighlightMap - source code highlight list
 * @return - true on Success
 */
bool FMaterial::GetMaterialExpressionSource( FString& OutSource )
{
#if WITH_EDITORONLY_DATA
	class FViewSourceMaterialTranslator : public FHLSLMaterialTranslator
	{
	public:
		FViewSourceMaterialTranslator(FMaterial* InMaterial,FMaterialCompilationOutput& InMaterialCompilationOutput,const FStaticParameterSet& StaticParameters,EShaderPlatform InPlatform,EMaterialQualityLevel::Type InQualityLevel,ERHIFeatureLevel::Type InFeatureLevel)
		:	FHLSLMaterialTranslator(InMaterial,InMaterialCompilationOutput,StaticParameters,InPlatform,InQualityLevel,InFeatureLevel)
		{}
	};

	FMaterialCompilationOutput TempOutput;
	FViewSourceMaterialTranslator MaterialTranslator(this, TempOutput, FStaticParameterSet(), GMaxRHIShaderPlatform, GetQualityLevel(), GetFeatureLevel());
	bool bSuccess = MaterialTranslator.Translate();

	if( bSuccess )
	{
		// Generate the HLSL
		OutSource = MaterialTranslator.GetMaterialShaderCode();
	}
	return bSuccess;
#else
	UE_LOG(LogMaterial, Fatal,TEXT("Not supported."));
	return false;
#endif
}

/** Recompiles any materials in the EditorLoadedMaterialResources list if they are not complete. */
void FMaterial::UpdateEditorLoadedMaterialResources(EShaderPlatform InShaderPlatform)
{
	for (TSet<FMaterial*>::TIterator It(EditorLoadedMaterialResources); It; ++It)
	{
		FMaterial* CurrentMaterial = *It;
		if (!CurrentMaterial->GetGameThreadShaderMap() || !CurrentMaterial->GetGameThreadShaderMap()->IsComplete(CurrentMaterial, true))
		{
			CurrentMaterial->CacheShaders(InShaderPlatform, true);
		}
	}
}

void FMaterial::BackupEditorLoadedMaterialShadersToMemory(TMap<FMaterialShaderMap*, TUniquePtr<TArray<uint8> > >& ShaderMapToSerializedShaderData)
{
	for (TSet<FMaterial*>::TIterator It(EditorLoadedMaterialResources); It; ++It)
	{
		FMaterial* CurrentMaterial = *It;
		FMaterialShaderMap* ShaderMap = CurrentMaterial->GetGameThreadShaderMap();

		if (ShaderMap && !ShaderMapToSerializedShaderData.Contains(ShaderMap))
		{
			TArray<uint8>* ShaderData = ShaderMap->BackupShadersToMemory();
			ShaderMapToSerializedShaderData.Emplace(ShaderMap, ShaderData);
		}
	}
}

void FMaterial::RestoreEditorLoadedMaterialShadersFromMemory(const TMap<FMaterialShaderMap*, TUniquePtr<TArray<uint8> > >& ShaderMapToSerializedShaderData)
{
	for (TSet<FMaterial*>::TIterator It(EditorLoadedMaterialResources); It; ++It)
	{
		FMaterial* CurrentMaterial = *It;
		FMaterialShaderMap* ShaderMap = CurrentMaterial->GetGameThreadShaderMap();

		if (ShaderMap)
		{
			const TUniquePtr<TArray<uint8> >* ShaderData = ShaderMapToSerializedShaderData.Find(ShaderMap);

			if (ShaderData)
			{
				ShaderMap->RestoreShadersFromMemory(**ShaderData);
			}
		}
	}
}

FMaterialUpdateContext::FMaterialUpdateContext(uint32 Options, EShaderPlatform InShaderPlatform)
{
	bool bReregisterComponents = (Options & EOptions::ReregisterComponents) != 0;
	bool bRecreateRenderStates = (Options & EOptions::RecreateRenderStates) != 0;

	bSyncWithRenderingThread = (Options & EOptions::SyncWithRenderingThread) != 0;
	if (bReregisterComponents)
	{
		ComponentReregisterContext = MakeUnique<FGlobalComponentReregisterContext>();
	}
	else if (bRecreateRenderStates)
	{
		ComponentRecreateRenderStateContext = MakeUnique<FGlobalComponentRecreateRenderStateContext>();
	}
	if (bSyncWithRenderingThread)
	{
		FlushRenderingCommands();
	}
	ShaderPlatform = InShaderPlatform;
}

void FMaterialUpdateContext::AddMaterial(UMaterial* Material)
{
	UpdatedMaterials.Add(Material);
	UpdatedMaterialInterfaces.Add(Material);
}

void FMaterialUpdateContext::AddMaterialInstance(UMaterialInstance* Instance)
{
	UpdatedMaterials.Add(Instance->GetMaterial());
	UpdatedMaterialInterfaces.Add(Instance);
}

void FMaterialUpdateContext::AddMaterialInterface(UMaterialInterface* Interface)
{
	UpdatedMaterials.Add(Interface->GetMaterial());
	UpdatedMaterialInterfaces.Add(Interface);
}

FMaterialUpdateContext::~FMaterialUpdateContext()
{
	double StartTime = FPlatformTime::Seconds();
	bool bProcess = false;

	// if the shader platform that was processed is not the currently rendering shader platform, 
	// there's no reason to update all of the runtime components
	UMaterialInterface::IterateOverActiveFeatureLevels([&](ERHIFeatureLevel::Type InFeatureLevel)
	{
		if (ShaderPlatform == GShaderPlatformForFeatureLevel[InFeatureLevel])
		{
			bProcess = true;
		}
	});

	if (!bProcess)
	{
		return;
	}

	// Flush rendering commands even though we already did so in the constructor.
	// Anything may have happened since the constructor has run. The flush is
	// done once here to avoid calling it once per static permutation we update.
	if (bSyncWithRenderingThread)
	{
		FlushRenderingCommands();
	}

	TArray<const FMaterial*> MaterialResourcesToUpdate;
	TArray<UMaterialInstance*> InstancesToUpdate;

	bool bUpdateStaticDrawLists = !ComponentReregisterContext && !ComponentRecreateRenderStateContext;

	// If static draw lists must be updated, gather material resources from all updated materials.
	if (bUpdateStaticDrawLists)
	{
		for (TSet<UMaterial*>::TConstIterator It(UpdatedMaterials); It; ++It)
		{
			UMaterial* Material = *It;

			for (int32 QualityLevelIndex = 0; QualityLevelIndex < EMaterialQualityLevel::Num; QualityLevelIndex++)
			{
				for (int32 FeatureLevelIndex = 0; FeatureLevelIndex < ERHIFeatureLevel::Num; FeatureLevelIndex++)
				{
					FMaterialResource* CurrentResource = Material->MaterialResources[QualityLevelIndex][FeatureLevelIndex];
					MaterialResourcesToUpdate.Add(CurrentResource);
				}
			}
		}
	}

	// Go through all loaded material instances and recompile their static permutation resources if needed
	// This is necessary since the parent UMaterial stores information about how it should be rendered, (eg bUsesDistortion)
	// but the child can have its own shader map which may not contain all the shaders that the parent's settings indicate that it should.
	for (TObjectIterator<UMaterialInstance> It; It; ++It)
	{
		UMaterialInstance* CurrentMaterialInstance = *It;
		UMaterial* BaseMaterial = CurrentMaterialInstance->GetMaterial();

		if (UpdatedMaterials.Contains(BaseMaterial))
		{
			// Check to see if this instance is dependent on any of the material interfaces we directly updated.
			for (auto InterfaceIt = UpdatedMaterialInterfaces.CreateConstIterator(); InterfaceIt; ++InterfaceIt)
			{
				if (CurrentMaterialInstance->IsDependent(*InterfaceIt))
				{
					InstancesToUpdate.Add(CurrentMaterialInstance);
					break;
				}
			}
		}
	}

	// Material instances that use this base material must have their uniform expressions recached 
	// However, some material instances that use this base material may also depend on another MI with static parameters
	// So we must traverse upwards and ensure all parent instances that need updating are recached first.
	int32 NumInstancesWithStaticPermutations = 0;

	TFunction<void(UMaterialInstance* MI)> UpdateInstance = [&](UMaterialInstance* MI)
	{
		if (MI->Parent && InstancesToUpdate.Contains(MI->Parent))
		{
			if (UMaterialInstance* ParentInst = Cast<UMaterialInstance>(MI->Parent))
			{
				UpdateInstance(ParentInst);
			}
		}

		MI->InitStaticPermutation();//bHasStaticPermutation can change.
		if (MI->bHasStaticPermutationResource)
		{
			NumInstancesWithStaticPermutations++;
			// Collect FMaterial's that have been recompiled
			if (bUpdateStaticDrawLists)
			{
				for (int32 QualityLevelIndex = 0; QualityLevelIndex < EMaterialQualityLevel::Num; QualityLevelIndex++)
				{
					for (int32 FeatureLevelIndex = 0; FeatureLevelIndex < ERHIFeatureLevel::Num; FeatureLevelIndex++)
					{
						FMaterialResource* CurrentResource = MI->StaticPermutationMaterialResources[QualityLevelIndex][FeatureLevelIndex];
						MaterialResourcesToUpdate.Add(CurrentResource);
					}
				}
			}
		}
		InstancesToUpdate.Remove(MI);
	};

	while (InstancesToUpdate.Num() > 0)
	{
		UpdateInstance(InstancesToUpdate.Last());
	}

	if (bUpdateStaticDrawLists)
	{
		// Update static draw lists affected by any FMaterials that were recompiled
		// This is only needed if we aren't reregistering components which is not always
		// safe, e.g. while a component is being registered.
		GetRendererModule().UpdateStaticDrawListsForMaterials(MaterialResourcesToUpdate);
	}
	else if (ComponentReregisterContext)
	{
		ComponentReregisterContext.Reset();
	}
	else if (ComponentRecreateRenderStateContext)
	{
		ComponentRecreateRenderStateContext.Reset();
	}

	double EndTime = FPlatformTime::Seconds();

	if (UpdatedMaterials.Num() > 0)
	{
		UE_LOG(LogMaterial, Verbose,
			   TEXT("%.2f seconds spent updating %d materials, %d interfaces, %d instances, %d with static permutations."),
			   (float)(EndTime - StartTime),
			   UpdatedMaterials.Num(),
			   UpdatedMaterialInterfaces.Num(),
			   InstancesToUpdate.Num(),
			   NumInstancesWithStaticPermutations
			);
	}
}

bool UMaterialInterface::IsPropertyActive(EMaterialProperty InProperty)const
{
	//TODO: Disable properties in instances based on the currently set overrides and other material settings?
	//For now just allow all properties in instances. 
	//This had to be refactored into the instance as some override properties alter the properties that are active.
	return false;
}

#if WITH_EDITOR
int32 UMaterialInterface::CompilePropertyEx( class FMaterialCompiler* Compiler, const FGuid& AttributeID )
{
	return INDEX_NONE;
}

int32 UMaterialInterface::CompileProperty(FMaterialCompiler* Compiler, EMaterialProperty Property, uint32 ForceCastFlags)
{
	int32 Result = INDEX_NONE;

	if (IsPropertyActive(Property))
	{
		Result = CompilePropertyEx(Compiler, FMaterialAttributeDefinitionMap::GetID(Property));
	}
	else
	{
		Result = FMaterialAttributeDefinitionMap::CompileDefaultExpression(Compiler, Property);
	}

	if (ForceCastFlags & MFCF_ForceCast)
	{
		Result = Compiler->ForceCast(Result, FMaterialAttributeDefinitionMap::GetValueType(Property), ForceCastFlags);
	}

	return Result;
}
#endif // WITH_EDITOR

void UMaterialInterface::AnalyzeMaterialProperty(EMaterialProperty InProperty, int32& OutNumTextureCoordinates, bool& bOutRequiresVertexData)
{
#if WITH_EDITORONLY_DATA
	// FHLSLMaterialTranslator collects all required information during translation, but these data are protected. Needs to
	// derive own class from it to get access to these data.
	class FMaterialAnalyzer : public FHLSLMaterialTranslator
	{
	public:
		FMaterialAnalyzer(FMaterial* InMaterial, FMaterialCompilationOutput& InMaterialCompilationOutput, const FStaticParameterSet& StaticParameters, EShaderPlatform InPlatform, EMaterialQualityLevel::Type InQualityLevel, ERHIFeatureLevel::Type InFeatureLevel)
			: FHLSLMaterialTranslator(InMaterial, InMaterialCompilationOutput, StaticParameters, InPlatform, InQualityLevel, InFeatureLevel)
		{}
		int32 GetTextureCoordsCount() const
		{
			return NumUserTexCoords;
		}
		bool UsesVertexColor() const
		{
			return bUsesVertexColor;
		}

		bool UsesTransformVector() const
		{
			return bUsesTransformVector;
		}

		bool UsesWorldPositionExcludingShaderOffsets() const
		{
			return bNeedsWorldPositionExcludingShaderOffsets;
		}

		bool UsesPrecomputedAOMask() const
		{
			return bUsesAOMaterialMask;
		}

		bool UsesVertexPosition() const 
		{
			return bUsesVertexPosition;
		}
	};

	FMaterialCompilationOutput TempOutput;
	FMaterialResource* MaterialResource = GetMaterialResource(GMaxRHIFeatureLevel);
	FMaterialShaderMapId ShaderMapID;
	MaterialResource->GetShaderMapId(GMaxRHIShaderPlatform, ShaderMapID);
	FMaterialAnalyzer MaterialTranslator(MaterialResource, TempOutput, ShaderMapID.ParameterSet, GMaxRHIShaderPlatform, MaterialResource->GetQualityLevel(), GMaxRHIFeatureLevel);	
	
	static_cast<FMaterialCompiler*>(&MaterialTranslator)->SetMaterialProperty(InProperty); // FHLSLMaterialTranslator hides this interface, so cast to parent
	CompileProperty(&MaterialTranslator, InProperty);
	// Request data from translator
	OutNumTextureCoordinates = MaterialTranslator.GetTextureCoordsCount();
	bOutRequiresVertexData = MaterialTranslator.UsesVertexColor() || MaterialTranslator.UsesTransformVector() || MaterialTranslator.UsesWorldPositionExcludingShaderOffsets() || MaterialTranslator.UsesPrecomputedAOMask() || MaterialTranslator.UsesVertexPosition();
#endif
}

#if WITH_EDITORONLY_DATA
//Reorder the output index for any FExpressionInput connected to a UMaterialExpressionBreakMaterialAttributes.
//If the order of pins in the material results or the make/break attributes nodes changes 
//then the OutputIndex stored in any FExpressionInput coming from UMaterialExpressionBreakMaterialAttributes will be wrong and needs reordering.
void DoMaterialAttributeReorder(FExpressionInput* Input, int32 UE4Ver)
{
	if( Input && Input->Expression && Input->Expression->IsA(UMaterialExpressionBreakMaterialAttributes::StaticClass()) )
	{
		if( UE4Ver < VER_UE4_MATERIAL_ATTRIBUTES_REORDERING )
		{
			switch(Input->OutputIndex)
			{
			case 4: Input->OutputIndex = 7; break;
			case 5: Input->OutputIndex = 4; break;
			case 6: Input->OutputIndex = 5; break;
			case 7: Input->OutputIndex = 6; break;
			}
		}
		
		if( UE4Ver < VER_UE4_FIX_REFRACTION_INPUT_MASKING && Input->OutputIndex == 13 )
		{
			Input->Mask = 1;
			Input->MaskR = 1;
			Input->MaskG = 1;
			Input->MaskB = 1;
			Input->MaskA = 0;
		}

		// closest version to the clear coat change
		if( UE4Ver < VER_UE4_ADD_ROOTCOMPONENT_TO_FOLIAGEACTOR && Input->OutputIndex >= 12 )
		{
			Input->OutputIndex += 2;
		}
	}
}
#endif // WITH_EDITORONLY_DATA
//////////////////////////////////////////////////////////////////////////

FMaterialInstanceBasePropertyOverrides::FMaterialInstanceBasePropertyOverrides()
	:bOverride_OpacityMaskClipValue(false)
	,bOverride_BlendMode(false)
	,bOverride_ShadingModel(false)
	,bOverride_DitheredLODTransition(false)
	,bOverride_TwoSided(false)
	// NVCHANGE_BEGIN: Add VXGI
	, bOverride_VxgiConeTracingEnabled(false)
	, bOverride_UsedWithVxgiVoxelization(false)
	, bOverride_VxgiAllowTesselationDuringVoxelization(false)
	, bOverride_VxgiOmniDirectional(false)
	, bOverride_VxgiProportionalEmittance(false)
	, bOverride_VxgiCoverageSupersampling(false)
	, bOverride_VxgiMaterialSamplingRate(false)
	, bOverride_VxgiOpacityNoiseScaleBias(false)
	, bOverride_VxgiVoxelizationThickness(false)
	// NVCHANGE_END: Add VXGI
	,OpacityMaskClipValue(.333333f)
	,BlendMode(BLEND_Opaque)
	,ShadingModel(MSM_DefaultLit)
	,TwoSided(0)
	,DitheredLODTransition(0)
	// NVCHANGE_BEGIN: Add VXGI
	, bVxgiConeTracingEnabled(false)
	, bUsedWithVxgiVoxelization(true)
	, bVxgiAllowTesselationDuringVoxelization(false)
	, bVxgiOmniDirectional(false)
	, bVxgiProportionalEmittance(false)
	, bVxgiCoverageSupersampling(false)
	, VxgiMaterialSamplingRate(VXGIMSR_FixedDefault)
	, VxgiOpacityNoiseScaleBias(0, 0)
	, VxgiVoxelizationThickness(1.0f)
	// NVCHANGE_END: Add VXGI
{

}

bool FMaterialInstanceBasePropertyOverrides::operator==(const FMaterialInstanceBasePropertyOverrides& Other)const
{
	return	bOverride_OpacityMaskClipValue == Other.bOverride_OpacityMaskClipValue &&
			bOverride_BlendMode == Other.bOverride_BlendMode &&
			bOverride_ShadingModel == Other.bOverride_ShadingModel &&
			bOverride_TwoSided == Other.bOverride_TwoSided &&
			bOverride_DitheredLODTransition == Other.bOverride_DitheredLODTransition &&
			OpacityMaskClipValue == Other.OpacityMaskClipValue &&
			BlendMode == Other.BlendMode &&
			ShadingModel == Other.ShadingModel &&
			TwoSided == Other.TwoSided &&
			// NVCHANGE_BEGIN: Add VXGI
			DitheredLODTransition == Other.DitheredLODTransition &&
			bOverride_VxgiConeTracingEnabled == Other.bOverride_VxgiConeTracingEnabled &&
			bOverride_UsedWithVxgiVoxelization == Other.bOverride_UsedWithVxgiVoxelization &&
			bOverride_VxgiAllowTesselationDuringVoxelization == Other.bOverride_VxgiAllowTesselationDuringVoxelization &&
			bOverride_VxgiOmniDirectional == Other.bOverride_VxgiOmniDirectional &&
			bOverride_VxgiProportionalEmittance == Other.bOverride_VxgiProportionalEmittance &&
			bOverride_VxgiCoverageSupersampling == Other.bOverride_VxgiCoverageSupersampling &&
			bOverride_VxgiMaterialSamplingRate == Other.bOverride_VxgiMaterialSamplingRate &&
			bOverride_VxgiOpacityNoiseScaleBias == Other.bOverride_VxgiOpacityNoiseScaleBias &&
			bOverride_VxgiVoxelizationThickness == Other.bOverride_VxgiVoxelizationThickness &&
			bVxgiConeTracingEnabled == Other.bVxgiConeTracingEnabled &&
			bUsedWithVxgiVoxelization == Other.bUsedWithVxgiVoxelization &&
			bVxgiAllowTesselationDuringVoxelization == Other.bVxgiAllowTesselationDuringVoxelization &&
			bVxgiOmniDirectional == Other.bVxgiOmniDirectional &&
			bVxgiProportionalEmittance == Other.bVxgiProportionalEmittance &&
			bVxgiCoverageSupersampling == Other.bVxgiCoverageSupersampling &&
			VxgiMaterialSamplingRate == Other.VxgiMaterialSamplingRate &&
			VxgiOpacityNoiseScaleBias == Other.VxgiOpacityNoiseScaleBias &&
			VxgiVoxelizationThickness == Other.VxgiVoxelizationThickness &&
			// NVCHANGE_END: Add VXGI
			DitheredLODTransition == Other.DitheredLODTransition;
}

bool FMaterialInstanceBasePropertyOverrides::operator!=(const FMaterialInstanceBasePropertyOverrides& Other)const
{
	return !(*this == Other);
}

//////////////////////////////////////////////////////////////////////////

bool FMaterialShaderMapId::ContainsShaderType(const FShaderType* ShaderType) const
{
	for (int32 TypeIndex = 0; TypeIndex < ShaderTypeDependencies.Num(); TypeIndex++)
	{
		if (ShaderTypeDependencies[TypeIndex].ShaderType == ShaderType)
		{
			return true;
		}
	}

	return false;
}

bool FMaterialShaderMapId::ContainsShaderPipelineType(const FShaderPipelineType* ShaderPipelineType) const
{
	for (int32 TypeIndex = 0; TypeIndex < ShaderPipelineTypeDependencies.Num(); TypeIndex++)
	{
		if (ShaderPipelineTypeDependencies[TypeIndex].ShaderPipelineType == ShaderPipelineType)
		{
			return true;
		}
	}

	return false;
}

bool FMaterialShaderMapId::ContainsVertexFactoryType(const FVertexFactoryType* VFType) const
{
	for (int32 TypeIndex = 0; TypeIndex < VertexFactoryTypeDependencies.Num(); TypeIndex++)
	{
		if (VertexFactoryTypeDependencies[TypeIndex].VertexFactoryType == VFType)
		{
			return true;
		}
	}

	return false;
}//////////////////////////////////////////////////////////////////////////

FMaterialAttributeDefintion::FMaterialAttributeDefintion(
		const FGuid& InAttributeID, const FString& InDisplayName, EMaterialProperty InProperty,
		EMaterialValueType InValueType, const FVector4& InDefaultValue, EShaderFrequency InShaderFrequency,
		int32 InTexCoordIndex /*= INDEX_NONE*/, bool bInIsHidden /*= false*/, MaterialAttributeBlendFunction InBlendFunction /*= nullptr*/)
	: AttributeID(InAttributeID)
	, DisplayName(InDisplayName)
	, Property(InProperty)
	, ValueType(InValueType)
	, DefaultValue(InDefaultValue)
	, ShaderFrequency(InShaderFrequency)
	, TexCoordIndex(InTexCoordIndex)
	, BlendFunction(InBlendFunction)
	, bIsHidden(bInIsHidden)
{
	checkf(ValueType & MCT_Float, TEXT("Unsupported type, only Float1 through Float4 allowed."));
}

int32 FMaterialAttributeDefintion::CompileDefaultValue(FMaterialCompiler* Compiler)
{
	int32 Ret;

	// TODO: Temporarily preserving hack from 4.13 to change default value for two-sided foliage model 
	if (Property == MP_SubsurfaceColor && Compiler->GetMaterialShadingModel() == MSM_TwoSidedFoliage)
	{
		check(ValueType == MCT_Float3);
		return Compiler->Constant3(0, 0, 0);
	}

	if (TexCoordIndex == INDEX_NONE)
	{
		// Standard value type
		switch (ValueType)
		{
		case MCT_Float:
		case MCT_Float1: Ret = Compiler->Constant(DefaultValue.X); break;
		case MCT_Float2: Ret = Compiler->Constant2(DefaultValue.X, DefaultValue.Y); break;
		case MCT_Float3: Ret = Compiler->Constant3(DefaultValue.X, DefaultValue.Y, DefaultValue.Z); break;
		default: Ret = Compiler->Constant4(DefaultValue.X, DefaultValue.Y, DefaultValue.Z, DefaultValue.W);
		}
	}
	else
	{
		// Texture coordinates allow pass through for default	
		Ret = Compiler->TextureCoordinate(TexCoordIndex, false, false);
	}

	return Ret;
}

//////////////////////////////////////////////////////////////////////////

FMaterialCustomOutputAttributeDefintion::FMaterialCustomOutputAttributeDefintion(
		const FGuid& InAttributeID, const FString& InDisplayName, const FString& InFunctionName, EMaterialProperty InProperty,
		EMaterialValueType InValueType, const FVector4& InDefaultValue, EShaderFrequency InShaderFrequency, MaterialAttributeBlendFunction InBlendFunction /*= nullptr*/)
	: FMaterialAttributeDefintion(InAttributeID, InDisplayName, InProperty, InValueType, InDefaultValue, InShaderFrequency, INDEX_NONE, false, InBlendFunction)
	, FunctionName(InFunctionName)
{
}

//////////////////////////////////////////////////////////////////////////
FMaterialAttributeDefinitionMap FMaterialAttributeDefinitionMap::GMaterialPropertyAttributesMap;

void FMaterialAttributeDefinitionMap::InitializeAttributeMap()
{
	check(!bIsInitialized);
	bIsInitialized = true;
	const bool bHideAttribute = true;

	// All types plus default/missing attribute
	AttributeMap.Empty(MP_MAX + 1);

	// Basic attributes
	Add(FGuid(0x69B8D336, 0x16ED4D49, 0x9AA49729, 0x2F050F7A), TEXT("BaseColor"),		MP_BaseColor,		MCT_Float3,	FVector4(0,0,0,0), SF_Pixel);
	Add(FGuid(0xB769B54D, 0xD08D4440, 0xABC21BA6, 0xCD27D0E2), TEXT("EmissiveColor"),	MP_EmissiveColor,	MCT_Float3,	FVector4(0,0,0,0), SF_Pixel);
	Add(FGuid(0x0FA2821A, 0x200F4A4A, 0xB719B789, 0xC1259C64), TEXT("Normal"),			MP_Normal,			MCT_Float3,	FVector4(0,0,1,0), SF_Pixel);
	Add(FGuid(0x57C3A161, 0x7F064296, 0xB00B24A5, 0xA496F34C), TEXT("Metallic"),		MP_Metallic,		MCT_Float,	FVector4(0,0,0,0), SF_Pixel);
	Add(FGuid(0x9FDAB399, 0x25564CC9, 0x8CD2D572, 0xC12C8FED), TEXT("Specular"),		MP_Specular,		MCT_Float,	FVector4(.5,0,0,0), SF_Pixel);
	Add(FGuid(0xD1DD967C, 0x4CAD47D3, 0x9E6346FB, 0x08ECF210), TEXT("Roughness"),		MP_Roughness,		MCT_Float,	FVector4(.5,0,0,0), SF_Pixel);
	Add(FGuid(0xB8F50FBA, 0x2A754EC1, 0x9EF672CF, 0xEB27BF51), TEXT("Opacity"),			MP_Opacity,			MCT_Float,	FVector4(1,0,0,0), SF_Pixel);
	Add(FGuid(0x679FFB17, 0x2BB5422C, 0xAD520483, 0x166E0C75), TEXT("OpacityMask"),		MP_OpacityMask,		MCT_Float,	FVector4(1,0,0,0), SF_Pixel);

	// Advanced attributes
	Add(FGuid(0x5B8FC679, 0x51CE4082, 0x9D777BEE, 0xF4F72C44), TEXT("SubsurfaceColor"),			MP_SubsurfaceColor,			MCT_Float3,	FVector4(1,1,1,0), SF_Pixel);
	Add(FGuid(0xD0B0FA03, 0x14D74455, 0xA851BAC5, 0x81A0788B), TEXT("Refraction"),				MP_Refraction,				MCT_Float2,	FVector4(1,0,0,0), SF_Pixel);
	Add(FGuid(0xE8EBD0AD, 0xB1654CBE, 0xB079C3A8, 0xB39B9F15), TEXT("AmbientOcclusion"),		MP_AmbientOcclusion,		MCT_Float,	FVector4(1,0,0,0), SF_Pixel);
	Add(FGuid(0x0AC97EC3, 0xE3D047BA, 0xB610167D, 0xC4D919FF), TEXT("PixelDepthOffset"),		MP_PixelDepthOffset,		MCT_Float,	FVector4(0,0,0,0), SF_Pixel);
	Add(FGuid(0xA0119D44, 0xC456450D, 0x9C39C933, 0x1F72D8D1), TEXT("TessellationMultiplier"),	MP_TessellationMultiplier,	MCT_Float,	FVector4(1,0,0,0), SF_Hull);
	Add(FGuid(0x2091ECA2, 0xB59248EE, 0x8E2CD578, 0xD371926D), TEXT("WorldDisplacement"),		MP_WorldDisplacement,		MCT_Float3,	FVector4(0,0,0,0), SF_Domain);
	Add(FGuid(0xF905F895, 0xD5814314, 0x916D2434, 0x8C40CE9E), TEXT("WorldPositionOffset"),		MP_WorldPositionOffset,		MCT_Float3,	FVector4(0,0,0,0), SF_Vertex);

	// Texture coordinates
	Add(FGuid(0xD30EC284, 0xE13A4160, 0x87BB5230, 0x2ED115DC), TEXT("CustomizedUV0"), MP_CustomizedUVs0, MCT_Float2, FVector4(0,0,0,0), SF_Vertex, 0);
	Add(FGuid(0xC67B093C, 0x2A5249AA, 0xABC97ADE, 0x4A1F49C5), TEXT("CustomizedUV1"), MP_CustomizedUVs1, MCT_Float2, FVector4(0,0,0,0), SF_Vertex, 1);
	Add(FGuid(0x85C15B24, 0xF3E047CA, 0x85856872, 0x01AE0F4F), TEXT("CustomizedUV2"), MP_CustomizedUVs2, MCT_Float2, FVector4(0,0,0,0), SF_Vertex, 2);
	Add(FGuid(0x777819DC, 0x31AE4676, 0xB864EF77, 0xB807E873), TEXT("CustomizedUV3"), MP_CustomizedUVs3, MCT_Float2, FVector4(0,0,0,0), SF_Vertex, 3);
	Add(FGuid(0xDA63B233, 0xDDF44CAD, 0xB93D867B, 0x8DAFDBCC), TEXT("CustomizedUV4"), MP_CustomizedUVs4, MCT_Float2, FVector4(0,0,0,0), SF_Vertex, 4);
	Add(FGuid(0xC2F52B76, 0x4A034388, 0x89119528, 0x2071B190), TEXT("CustomizedUV5"), MP_CustomizedUVs5, MCT_Float2, FVector4(0,0,0,0), SF_Vertex, 5);
	Add(FGuid(0x8214A8CA, 0x0CB944CF, 0x9DFD78DB, 0xE48BB55F), TEXT("CustomizedUV6"), MP_CustomizedUVs6, MCT_Float2, FVector4(0,0,0,0), SF_Vertex, 6);
	Add(FGuid(0xD8F8D01F, 0xC6F74715, 0xA3CFB4FF, 0x9EF51FAC), TEXT("CustomizedUV7"), MP_CustomizedUVs7, MCT_Float2, FVector4(0,0,0,0), SF_Vertex, 7);

	// Custom attributes
	Add(FGuid(0x9E502E69, 0x3C8F48FA, 0x94645CFD, 0x28E5428D), TEXT("ClearCoat"),			MP_CustomData0, MCT_Float, FVector4(1,0,0,0), SF_Pixel);
	Add(FGuid(0xBE4F2FFD, 0x12FC4296, 0xB0124EEA, 0x12C28D92), TEXT("ClearCoatRoughness"),	MP_CustomData1, MCT_Float, FVector4(.1,0,0,0), SF_Pixel);

	// Lightmass attributes	
	Add(FGuid(0x68934E1B, 0x70EB411B, 0x86DF5AA5, 0xDF2F626C), TEXT("DiffuseColor"),	MP_DiffuseColor,	MCT_Float3, FVector4(0,0,0,0), SF_Pixel, INDEX_NONE, bHideAttribute);
	Add(FGuid(0xE89CBD84, 0x62EA48BE, 0x80F88521, 0x2B0C403C), TEXT("SpecularColor"),	MP_SpecularColor,	MCT_Float3, FVector4(0,0,0,0), SF_Pixel, INDEX_NONE, bHideAttribute);

	// Debug attributes
	Add(FGuid(0x5BF6BA94, 0xA3264629, 0xA253A05B, 0x0EABBB86), TEXT("Missing"), MP_MAX, MCT_Float, FVector4(0,0,0,0), SF_Pixel, INDEX_NONE, bHideAttribute);
}

void FMaterialAttributeDefinitionMap::Add(const FGuid& AttributeID, const FString& DisplayName, EMaterialProperty Property,
	EMaterialValueType ValueType, const FVector4& DefaultValue, EShaderFrequency ShaderFrequency,
	int32 TexCoordIndex /*= INDEX_NONE*/, bool bIsHidden /*= false*/, MaterialAttributeBlendFunction BlendFunction /*= nullptr*/)
{
	checkf(!AttributeMap.Contains(Property), TEXT("Tried to add duplicate material property."));
	AttributeMap.Add(Property, FMaterialAttributeDefintion(AttributeID, DisplayName, Property, ValueType, DefaultValue, ShaderFrequency, TexCoordIndex, bIsHidden, BlendFunction));
}

FMaterialAttributeDefintion* FMaterialAttributeDefinitionMap::Find(const FGuid& AttributeID)
{
	for (auto& Attribute : CustomAttributes)
	{
		if (Attribute.AttributeID == AttributeID)
		{
			return &Attribute;
		}
	}

	for (auto& Attribute : AttributeMap)
	{
		if (Attribute.Value.AttributeID == AttributeID)
		{
			return &Attribute.Value;
		}
	}
	
	UE_LOG(LogMaterial, Warning, TEXT("Failed to find material attribute, AttributeID: %s."), *AttributeID.ToString(EGuidFormats::Digits));
	return Find(MP_MAX);
}

FMaterialAttributeDefintion* FMaterialAttributeDefinitionMap::Find(EMaterialProperty Property)
{
	if (FMaterialAttributeDefintion* Attribute = AttributeMap.Find(Property))
	{
		return Attribute;
	}

	UE_LOG(LogMaterial, Warning, TEXT("Failed to find material attribute, PropertyType: %i."), (uint32)Property);
	return Find(MP_MAX);
}

void FMaterialAttributeDefinitionMap::AppendDDCKeyString(FString& String)
{
	FString& DDCString = GMaterialPropertyAttributesMap.AttributeDDCString;

	if (DDCString.Len() == 0)
	{
		FString AttributeIDs;

		for (const auto& Attribute : GMaterialPropertyAttributesMap.AttributeMap)
		{
			AttributeIDs += Attribute.Value.AttributeID.ToString(EGuidFormats::Digits);
		}

		for (const auto& Attribute : GMaterialPropertyAttributesMap.CustomAttributes)
		{
			AttributeIDs += Attribute.AttributeID.ToString(EGuidFormats::Digits);
		}

		FSHA1 HashState;
		HashState.UpdateWithString(*AttributeIDs, AttributeIDs.Len());
		HashState.Final();

		FSHAHash Hash;
		HashState.GetHash(&Hash.Hash[0]);
		DDCString = Hash.ToString();
	}
	else
	{
		// TODO: In debug force re-generate DDC string and compare to catch invalid runtime changes
	}

	String.Append(DDCString);
}

void FMaterialAttributeDefinitionMap::AddCustomAttribute(const FGuid& AttributeID, const FString& DisplayName, const FString& FunctionName, EMaterialValueType ValueType, const FVector4& DefaultValue, MaterialAttributeBlendFunction BlendFunction /*= nullptr*/)
{
	FMaterialCustomOutputAttributeDefintion UserAttribute(AttributeID, DisplayName, FunctionName, MP_CustomOutput, ValueType, DefaultValue, SF_Pixel, BlendFunction);
#if DO_CHECK
	for (auto& Attribute : GMaterialPropertyAttributesMap.AttributeMap)
	{
		checkf(Attribute.Value.AttributeID != AttributeID, TEXT("Tried to add duplicate custom output attribute (%s) already in base attributes (%s)."), *DisplayName, *(Attribute.Value.DisplayName));
	}
	checkf(!GMaterialPropertyAttributesMap.CustomAttributes.Contains(UserAttribute), TEXT("Tried to add duplicate custom output attribute (%s)."), *DisplayName);
#endif
	GMaterialPropertyAttributesMap.CustomAttributes.Add(UserAttribute);
}

void FMaterialAttributeDefinitionMap::GetCustomAttributeList(TArray<FMaterialCustomOutputAttributeDefintion>& CustomAttributeList)
{
	CustomAttributeList.Empty(GMaterialPropertyAttributesMap.CustomAttributes.Num());
	for (auto& Attribute : GMaterialPropertyAttributesMap.CustomAttributes)
	{
		CustomAttributeList.Add(Attribute);
	}
}

void FMaterialAttributeDefinitionMap::GetDisplayNameToIDList(TArray<TPair<FString, FGuid>>& NameToIDList)
{
	NameToIDList.Empty(GMaterialPropertyAttributesMap.AttributeMap.Num() + GMaterialPropertyAttributesMap.CustomAttributes.Num());
	for (const auto& Attribute : GMaterialPropertyAttributesMap.AttributeMap)
	{
		if (!Attribute.Value.bIsHidden)
		{
			NameToIDList.Emplace(Attribute.Value.DisplayName, Attribute.Value.AttributeID);
		}
	}

	for (auto& Attribute : GMaterialPropertyAttributesMap.CustomAttributes)
	{
		NameToIDList.Emplace(Attribute.DisplayName, Attribute.AttributeID);
	}
}
