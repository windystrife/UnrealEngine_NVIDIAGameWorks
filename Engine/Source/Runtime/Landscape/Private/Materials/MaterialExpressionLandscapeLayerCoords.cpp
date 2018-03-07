// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Materials/MaterialExpressionLandscapeLayerCoords.h"
#include "LandscapePrivate.h"
#include "MaterialCompiler.h"


#define LOCTEXT_NAMESPACE "Landscape"


///////////////////////////////////////////////////////////////////////////////
// UMaterialExpressionLandscapeLayerCoords
///////////////////////////////////////////////////////////////////////////////

UMaterialExpressionLandscapeLayerCoords::UMaterialExpressionLandscapeLayerCoords(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Landscape;
		FConstructorStatics()
			: NAME_Landscape(LOCTEXT("Landscape", "Landscape"))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Landscape);
#endif

	bCollapsed = false;
}

#if WITH_EDITOR
int32 UMaterialExpressionLandscapeLayerCoords::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	switch (CustomUVType)
	{
	case LCCT_CustomUV0:
		return Compiler->TextureCoordinate(0, false, false);
	case LCCT_CustomUV1:
		return Compiler->TextureCoordinate(1, false, false);
	case LCCT_CustomUV2:
		return Compiler->TextureCoordinate(2, false, false);
	case LCCT_WeightMapUV:
		return Compiler->TextureCoordinate(3, false, false);
	default:
		break;
	}

	int32 BaseUV;

	switch (MappingType)
	{
	case TCMT_Auto:
	case TCMT_XY: BaseUV = Compiler->TextureCoordinate(0, false, false); break;
	case TCMT_XZ: BaseUV = Compiler->TextureCoordinate(1, false, false); break;
	case TCMT_YZ: BaseUV = Compiler->TextureCoordinate(2, false, false); break;
	default: UE_LOG(LogLandscape, Fatal, TEXT("Invalid mapping type %u"), (uint8)MappingType); return INDEX_NONE;
	};

	float Scale = (MappingScale == 0.0f) ? 1.0f : 1.0f / MappingScale;
	int32 RealScale = Compiler->Constant(Scale);

	float Cos = FMath::Cos(MappingRotation * PI / 180.0);
	float Sin = FMath::Sin(MappingRotation * PI / 180.0);

	int32 ResultUV = INDEX_NONE;
	int32 TransformedUV = Compiler->Add(
		Compiler->Mul(RealScale,
		Compiler->AppendVector(
		Compiler->Dot(BaseUV, Compiler->Constant2(+Cos, +Sin)),
		Compiler->Dot(BaseUV, Compiler->Constant2(-Sin, +Cos)))
		),
		Compiler->Constant2(MappingPanU, MappingPanV)
		);

	if (Compiler->GetFeatureLevel() != ERHIFeatureLevel::ES2) // No need to localize UV
	{
		ResultUV = TransformedUV;
	}
	else
	{
		int32 Offset = Compiler->TextureCoordinateOffset();
		int32 TransformedOffset =
			Compiler->Floor(
			Compiler->Mul(RealScale,
			Compiler->AppendVector(
			Compiler->Dot(Offset, Compiler->Constant2(+Cos, +Sin)),
			Compiler->Dot(Offset, Compiler->Constant2(-Sin, +Cos)))
			));

		ResultUV = Compiler->Sub(TransformedUV, TransformedOffset);
	}

	return ResultUV;
}


void UMaterialExpressionLandscapeLayerCoords::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(FString(TEXT("LandscapeCoords")));
}
#endif // WITH_EDITOR

bool UMaterialExpressionLandscapeLayerCoords::NeedsLoadForClient() const
{
	return true;
}

#undef LOCTEXT_NAMESPACE
