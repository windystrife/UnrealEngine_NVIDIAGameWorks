// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "StaticMeshEditorTools.h"
#include "Framework/Commands/UIAction.h"
#include "Textures/SlateIcon.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "EngineDefines.h"
#include "EditorStyleSet.h"
#include "PropertyHandle.h"
#include "IDetailChildrenBuilder.h"
#include "Misc/MessageDialog.h"
#include "Misc/FeedbackContext.h"
#include "Modules/ModuleManager.h"
#include "SlateOptMacros.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Materials/Material.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "RawMesh.h"
#include "MeshUtilities.h"
#include "StaticMeshResources.h"
#include "StaticMeshEditor.h"
#include "PropertyCustomizationHelpers.h"
#include "PhysicsEngine/BodySetup.h"
#include "FbxMeshUtils.h"
#include "Widgets/Input/SVectorInputBox.h"

#include "Runtime/Analytics/Analytics/Public/Interfaces/IAnalyticsProvider.h"
#include "EngineAnalytics.h"
#include "Widgets/Input/STextComboBox.h"
#include "ScopedTransaction.h"
#include "JsonObjectConverter.h"
#include "Engine/SkeletalMesh.h"
#include "IMeshReductionManagerModule.h"
#include "HAL/PlatformApplicationMisc.h"

const float MaxHullAccuracy = 1.f;
const float MinHullAccuracy = 0.f;
const float DefaultHullAccuracy = 0.5f;
const float HullAccuracyDelta = 0.01f;

const int32 MaxVertsPerHullCount = 32;
const int32 MinVertsPerHullCount = 6;
const int32 DefaultVertsPerHull = 16;

#define LOCTEXT_NAMESPACE "StaticMeshEditor"
DEFINE_LOG_CATEGORY_STATIC(LogStaticMeshEditorTools,Log,All);

FStaticMeshDetails::FStaticMeshDetails( class FStaticMeshEditor& InStaticMeshEditor )
	: StaticMeshEditor( InStaticMeshEditor )
{}

FStaticMeshDetails::~FStaticMeshDetails()
{
}

void FStaticMeshDetails::CustomizeDetails( class IDetailLayoutBuilder& DetailBuilder )
{
	IDetailCategoryBuilder& LODSettingsCategory = DetailBuilder.EditCategory( "LodSettings", LOCTEXT("LodSettingsCategory", "LOD Settings") );
	IDetailCategoryBuilder& StaticMeshCategory = DetailBuilder.EditCategory( "StaticMesh", LOCTEXT("StaticMeshGeneralSettings", "General Settings") );
	IDetailCategoryBuilder& CollisionCategory = DetailBuilder.EditCategory( "Collision", LOCTEXT("CollisionCategory", "Collision") );
	IDetailCategoryBuilder& ImportSettingsCategory = DetailBuilder.EditCategory("ImportSettings");

	// Hide the ability to change the import settings object
	TSharedRef<IPropertyHandle> ImportSettings = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UStaticMesh, AssetImportData));
	IDetailPropertyRow& Row = ImportSettingsCategory.AddProperty(ImportSettings);
	Row.CustomWidget(true)
		.NameContent()
		[
			ImportSettings->CreatePropertyNameWidget()
		];
	
	DetailBuilder.EditCategory( "Navigation", FText::GetEmpty(), ECategoryPriority::Uncommon );

	LevelOfDetailSettings = MakeShareable( new FLevelOfDetailSettingsLayout( StaticMeshEditor ) );

	LevelOfDetailSettings->AddToDetailsPanel( DetailBuilder );

	
	TSharedRef<IPropertyHandle> BodyProp = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UStaticMesh,BodySetup));
	BodyProp->MarkHiddenByCustomization();

	static TArray<FName> HiddenBodyInstanceProps;

	if( HiddenBodyInstanceProps.Num() == 0 )
	{
		//HiddenBodyInstanceProps.Add("DefaultInstance");
		HiddenBodyInstanceProps.Add("BoneName");
		HiddenBodyInstanceProps.Add("PhysicsType");
		HiddenBodyInstanceProps.Add("bConsiderForBounds");
		HiddenBodyInstanceProps.Add("CollisionReponse");
	}

	uint32 NumChildren = 0;
	BodyProp->GetNumChildren( NumChildren );

	TSharedPtr<IPropertyHandle> BodyPropObject;

	if( NumChildren == 1 )
	{
		// This is an edit inline new property so the first child is the object instance for the edit inline new.  The instance contains the child we want to display
		BodyPropObject = BodyProp->GetChildHandle( 0 );

		NumChildren = 0;
		BodyPropObject->GetNumChildren( NumChildren );

		for( uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex )
		{
			TSharedPtr<IPropertyHandle> ChildProp = BodyPropObject->GetChildHandle(ChildIndex);
			if( ChildProp.IsValid() && ChildProp->GetProperty() && !HiddenBodyInstanceProps.Contains(ChildProp->GetProperty()->GetFName()) )
			{
				CollisionCategory.AddProperty( ChildProp );
			}
		}
	}
}

void SConvexDecomposition::Construct(const FArguments& InArgs)
{
	StaticMeshEditorPtr = InArgs._StaticMeshEditorPtr;

	CurrentHullAccuracy = DefaultHullAccuracy;
	CurrentMaxVertsPerHullCount = DefaultVertsPerHull;

	this->ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.0f, 16.0f, 0.0f, 8.0f)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text( LOCTEXT("HullAccuracy_ConvexDecomp", "Accuracy") )
			]

			+SHorizontalBox::Slot()
			.FillWidth(3.0f)
			[
				SAssignNew(HullAccuracy, SSpinBox<float>)
				.MinValue(MinHullAccuracy)
				.MaxValue(MaxHullAccuracy)
				.Delta(HullAccuracyDelta)
				.Value( this, &SConvexDecomposition::GetHullAccuracy )
				.OnValueCommitted( this, &SConvexDecomposition::OnHullAccuracyCommitted )
				.OnValueChanged( this, &SConvexDecomposition::OnHullAccuracyChanged )
			]
		]

		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.0f, 8.0f, 0.0f, 16.0f)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text( LOCTEXT("MaxHullVerts_ConvexDecomp", "Max Hull Verts") )
			]

			+SHorizontalBox::Slot()
				.FillWidth(3.0f)
				[
					SAssignNew(MaxVertsPerHull, SSpinBox<int32>)
					.MinValue(MinVertsPerHullCount)
					.MaxValue(MaxVertsPerHullCount)
					.Value( this, &SConvexDecomposition::GetVertsPerHullCount )
					.OnValueCommitted( this, &SConvexDecomposition::OnVertsPerHullCountCommitted )
					.OnValueChanged( this, &SConvexDecomposition::OnVertsPerHullCountChanged )
				]
		]

		+SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(8.0f, 0.0f, 8.0f, 0.0f)
			[
				SNew(SButton)
				.Text( LOCTEXT("Apply_ConvexDecomp", "Apply") )
				.OnClicked(this, &SConvexDecomposition::OnApplyDecomp)
			]

			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(8.0f, 0.0f, 8.0f, 0.0f)
			[
				SNew(SButton)
				.Text( LOCTEXT("Defaults_ConvexDecomp", "Defaults") )
				.OnClicked(this, &SConvexDecomposition::OnDefaults)
			]
		]
	];
}

bool FStaticMeshDetails::IsApplyNeeded() const
{
	return LevelOfDetailSettings.IsValid() && LevelOfDetailSettings->IsApplyNeeded();
}

void FStaticMeshDetails::ApplyChanges()
{
	if( LevelOfDetailSettings.IsValid() )
	{
		LevelOfDetailSettings->ApplyChanges();
	}
}

SConvexDecomposition::~SConvexDecomposition()
{

}

FReply SConvexDecomposition::OnApplyDecomp()
{
	StaticMeshEditorPtr.Pin()->DoDecomp(CurrentHullAccuracy, CurrentMaxVertsPerHullCount);

	return FReply::Handled();
}

FReply SConvexDecomposition::OnDefaults()
{
	CurrentHullAccuracy = DefaultHullAccuracy;
	CurrentMaxVertsPerHullCount = DefaultVertsPerHull;

	return FReply::Handled();
}

void SConvexDecomposition::OnHullAccuracyCommitted(float InNewValue, ETextCommit::Type CommitInfo)
{
	OnHullAccuracyChanged(InNewValue);
}

void SConvexDecomposition::OnHullAccuracyChanged(float InNewValue)
{
	CurrentHullAccuracy = InNewValue;
}

float SConvexDecomposition::GetHullAccuracy() const
{
	return CurrentHullAccuracy;
}
void SConvexDecomposition::OnVertsPerHullCountCommitted(int32 InNewValue,  ETextCommit::Type CommitInfo)
{
	OnVertsPerHullCountChanged(InNewValue);
}

void SConvexDecomposition::OnVertsPerHullCountChanged(int32 InNewValue)
{
	CurrentMaxVertsPerHullCount = InNewValue;
}

int32 SConvexDecomposition::GetVertsPerHullCount() const
{
	return CurrentMaxVertsPerHullCount;
}

static UEnum& GetFeatureImportanceEnum()
{
	static FName FeatureImportanceName(TEXT("EMeshFeatureImportance::Off"));
	static UEnum* FeatureImportanceEnum = NULL;
	if (FeatureImportanceEnum == NULL)
	{
		UEnum::LookupEnumName(FeatureImportanceName, &FeatureImportanceEnum);
		check(FeatureImportanceEnum);
	}
	return *FeatureImportanceEnum;
}

static void FillEnumOptions(TArray<TSharedPtr<FString> >& OutStrings, UEnum& InEnum)
{
	for (int32 EnumIndex = 0; EnumIndex < InEnum.NumEnums() - 1; ++EnumIndex)
	{
		OutStrings.Add(MakeShareable(new FString(InEnum.GetNameStringByIndex(EnumIndex))));
	}
}

FMeshBuildSettingsLayout::FMeshBuildSettingsLayout( TSharedRef<FLevelOfDetailSettingsLayout> InParentLODSettings )
	: ParentLODSettings( InParentLODSettings )
{

}

FMeshBuildSettingsLayout::~FMeshBuildSettingsLayout()
{
}

void FMeshBuildSettingsLayout::GenerateHeaderRowContent( FDetailWidgetRow& NodeRow )
{
	NodeRow.NameContent()
	[
		SNew( STextBlock )
		.Text( LOCTEXT("MeshBuildSettings", "Build Settings") )
		.Font( IDetailLayoutBuilder::GetDetailFont() )
	];
}

FString FMeshBuildSettingsLayout::GetCurrentDistanceFieldReplacementMeshPath() const
{
	return BuildSettings.DistanceFieldReplacementMesh ? BuildSettings.DistanceFieldReplacementMesh->GetPathName() : FString("");
}

void FMeshBuildSettingsLayout::OnDistanceFieldReplacementMeshSelected(const FAssetData& AssetData)
{
	BuildSettings.DistanceFieldReplacementMesh = Cast<UStaticMesh>(AssetData.GetAsset());
}

void FMeshBuildSettingsLayout::GenerateChildContent( IDetailChildrenBuilder& ChildrenBuilder )
{
	{
		ChildrenBuilder.AddCustomRow( LOCTEXT("RecomputeNormals", "Recompute Normals") )
		.NameContent()
		[
			SNew(STextBlock)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text(LOCTEXT("RecomputeNormals", "Recompute Normals"))
		
		]
		.ValueContent()
		[
			SNew(SCheckBox)
			.IsChecked(this, &FMeshBuildSettingsLayout::ShouldRecomputeNormals)
			.OnCheckStateChanged(this, &FMeshBuildSettingsLayout::OnRecomputeNormalsChanged)
		];
	}

	{
		ChildrenBuilder.AddCustomRow( LOCTEXT("RecomputeTangents", "Recompute Tangents") )
		.NameContent()
		[
			SNew(STextBlock)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text(LOCTEXT("RecomputeTangents", "Recompute Tangents"))
		]
		.ValueContent()
		[
			SNew(SCheckBox)
			.IsChecked(this, &FMeshBuildSettingsLayout::ShouldRecomputeTangents)
			.OnCheckStateChanged(this, &FMeshBuildSettingsLayout::OnRecomputeTangentsChanged)
		];
	}

	{
		ChildrenBuilder.AddCustomRow( LOCTEXT("UseMikkTSpace", "Use MikkTSpace Tangent Space") )
		.NameContent()
		[
			SNew(STextBlock)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text(LOCTEXT("UseMikkTSpace", "Use MikkTSpace Tangent Space"))
		]
		.ValueContent()
		[
			SNew(SCheckBox)
			.IsChecked(this, &FMeshBuildSettingsLayout::ShouldUseMikkTSpace)
			.OnCheckStateChanged(this, &FMeshBuildSettingsLayout::OnUseMikkTSpaceChanged)
		];
	}

	{
		ChildrenBuilder.AddCustomRow( LOCTEXT("RemoveDegenerates", "Remove Degenerates") )
		.NameContent()
		[
			SNew(STextBlock)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text(LOCTEXT("RemoveDegenerates", "Remove Degenerates"))
		]
		.ValueContent()
		[
			SNew(SCheckBox)
			.IsChecked(this, &FMeshBuildSettingsLayout::ShouldRemoveDegenerates)
			.OnCheckStateChanged(this, &FMeshBuildSettingsLayout::OnRemoveDegeneratesChanged)
		];
	}

	{
		ChildrenBuilder.AddCustomRow( LOCTEXT("BuildAdjacencyBuffer", "Build Adjacency Buffer") )
		.NameContent()
		[
			SNew(STextBlock)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text(LOCTEXT("BuildAdjacencyBuffer", "Build Adjacency Buffer"))
		]
		.ValueContent()
		[
			SNew(SCheckBox)
			.IsChecked(this, &FMeshBuildSettingsLayout::ShouldBuildAdjacencyBuffer)
			.OnCheckStateChanged(this, &FMeshBuildSettingsLayout::OnBuildAdjacencyBufferChanged)
		];
	}

	{
		ChildrenBuilder.AddCustomRow( LOCTEXT("BuildReversedIndexBuffer", "Build Reversed Index Buffer") )
		.NameContent()
		[
			SNew(STextBlock)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text(LOCTEXT("BuildReversedIndexBuffer", "Build Reversed Index Buffer"))
		]
		.ValueContent()
		[
			SNew(SCheckBox)
			.IsChecked(this, &FMeshBuildSettingsLayout::ShouldBuildReversedIndexBuffer)
			.OnCheckStateChanged(this, &FMeshBuildSettingsLayout::OnBuildReversedIndexBufferChanged)
		];
	}

	{
		ChildrenBuilder.AddCustomRow(LOCTEXT("UseHighPrecisionTangentBasis", "Use High Precision Tangent Basis"))
		.NameContent()
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(LOCTEXT("UseHighPrecisionTangentBasis", "Use High Precision Tangent Basis"))
		]
		.ValueContent()
		[
			SNew(SCheckBox)
			.IsChecked(this, &FMeshBuildSettingsLayout::ShouldUseHighPrecisionTangentBasis)
			.OnCheckStateChanged(this, &FMeshBuildSettingsLayout::OnUseHighPrecisionTangentBasisChanged)
		];
	}

	{
		ChildrenBuilder.AddCustomRow( LOCTEXT("UseFullPrecisionUVs", "Use Full Precision UVs") )
		.NameContent()
		[
			SNew(STextBlock)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text(LOCTEXT("UseFullPrecisionUVs", "Use Full Precision UVs"))
		]
		.ValueContent()
		[
			SNew(SCheckBox)
			.IsChecked(this, &FMeshBuildSettingsLayout::ShouldUseFullPrecisionUVs)
			.OnCheckStateChanged(this, &FMeshBuildSettingsLayout::OnUseFullPrecisionUVsChanged)
		];
	}

	{
		ChildrenBuilder.AddCustomRow( LOCTEXT("GenerateLightmapUVs", "Generate Lightmap UVs") )
		.NameContent()
		[
			SNew(STextBlock)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text(LOCTEXT("GenerateLightmapUVs", "Generate Lightmap UVs"))
		]
		.ValueContent()
		[
			SNew(SCheckBox)
			.IsChecked(this, &FMeshBuildSettingsLayout::ShouldGenerateLightmapUVs)
			.OnCheckStateChanged(this, &FMeshBuildSettingsLayout::OnGenerateLightmapUVsChanged)
		];
	}

	{
		ChildrenBuilder.AddCustomRow( LOCTEXT("MinLightmapResolution", "Min Lightmap Resolution") )
		.NameContent()
		[
			SNew(STextBlock)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text(LOCTEXT("MinLightmapResolution", "Min Lightmap Resolution"))
		]
		.ValueContent()
		[
			SNew(SSpinBox<int32>)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.MinValue(1)
			.MaxValue(2048)
			.Value(this, &FMeshBuildSettingsLayout::GetMinLightmapResolution)
			.OnValueChanged(this, &FMeshBuildSettingsLayout::OnMinLightmapResolutionChanged)
		];
	}

	{
		ChildrenBuilder.AddCustomRow( LOCTEXT("SourceLightmapIndex", "Source Lightmap Index") )
		.NameContent()
		[
			SNew(STextBlock)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text(LOCTEXT("SourceLightmapIndex", "Source Lightmap Index"))
		]
		.ValueContent()
		[
			SNew(SSpinBox<int32>)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.MinValue(0)
			.MaxValue(7)
			.Value(this, &FMeshBuildSettingsLayout::GetSrcLightmapIndex)
			.OnValueChanged(this, &FMeshBuildSettingsLayout::OnSrcLightmapIndexChanged)
		];
	}

	{
		ChildrenBuilder.AddCustomRow( LOCTEXT("DestinationLightmapIndex", "Destination Lightmap Index") )
		.NameContent()
		[
			SNew(STextBlock)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text(LOCTEXT("DestinationLightmapIndex", "Destination Lightmap Index"))
		]
		.ValueContent()
		[
			SNew(SSpinBox<int32>)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.MinValue(0)
			.MaxValue(7)
			.Value(this, &FMeshBuildSettingsLayout::GetDstLightmapIndex)
			.OnValueChanged(this, &FMeshBuildSettingsLayout::OnDstLightmapIndexChanged)
		];
	}

	{
		ChildrenBuilder.AddCustomRow(LOCTEXT("BuildScale", "Build Scale"))
		.NameContent()
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(LOCTEXT("BuildScale", "Build Scale"))
			.ToolTipText( LOCTEXT("BuildScale_ToolTip", "The local scale applied when building the mesh") )
		]
		.ValueContent()
		.MinDesiredWidth(125.0f * 3.0f)
		.MaxDesiredWidth(125.0f * 3.0f)
		[
			SNew(SVectorInputBox)
			.X(this, &FMeshBuildSettingsLayout::GetBuildScaleX)
			.Y(this, &FMeshBuildSettingsLayout::GetBuildScaleY)
			.Z(this, &FMeshBuildSettingsLayout::GetBuildScaleZ)
			.bColorAxisLabels(false)
			.AllowResponsiveLayout(true)
			.OnXCommitted(this, &FMeshBuildSettingsLayout::OnBuildScaleXChanged)
			.OnYCommitted(this, &FMeshBuildSettingsLayout::OnBuildScaleYChanged)
			.OnZCommitted(this, &FMeshBuildSettingsLayout::OnBuildScaleZChanged)
			.Font(IDetailLayoutBuilder::GetDetailFont())
		];
	}

	{
		ChildrenBuilder.AddCustomRow( LOCTEXT("DistanceFieldResolutionScale", "Distance Field Resolution Scale") )
		.NameContent()
		[
			SNew(STextBlock)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text(LOCTEXT("DistanceFieldResolutionScale", "Distance Field Resolution Scale"))
		]
		.ValueContent()
		[
			SNew(SSpinBox<float>)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.MinValue(0.0f)
			.MaxValue(100.0f)
			.Value(this, &FMeshBuildSettingsLayout::GetDistanceFieldResolutionScale)
			.OnValueChanged(this, &FMeshBuildSettingsLayout::OnDistanceFieldResolutionScaleChanged)
			.OnValueCommitted(this, &FMeshBuildSettingsLayout::OnDistanceFieldResolutionScaleCommitted)
		];
	}
		
	{
		ChildrenBuilder.AddCustomRow( LOCTEXT("GenerateDistanceFieldAsIfTwoSided", "Two-Sided Distance Field Generation") )
		.NameContent()
		[
			SNew(STextBlock)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text(LOCTEXT("GenerateDistanceFieldAsIfTwoSided", "Two-Sided Distance Field Generation"))
		]
		.ValueContent()
		[
			SNew(SCheckBox)
			.IsChecked(this, &FMeshBuildSettingsLayout::ShouldGenerateDistanceFieldAsIfTwoSided)
			.OnCheckStateChanged(this, &FMeshBuildSettingsLayout::OnGenerateDistanceFieldAsIfTwoSidedChanged)
		];
	}

	{
		TSharedRef<SWidget> PropWidget = SNew(SObjectPropertyEntryBox)
			.AllowedClass(UStaticMesh::StaticClass())
			.AllowClear(true)
			.ObjectPath(this, &FMeshBuildSettingsLayout::GetCurrentDistanceFieldReplacementMeshPath)
			.OnObjectChanged(this, &FMeshBuildSettingsLayout::OnDistanceFieldReplacementMeshSelected);

		ChildrenBuilder.AddCustomRow( LOCTEXT("DistanceFieldReplacementMesh", "Distance Field Replacement Mesh") )
		.NameContent()
		[
			SNew(STextBlock)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text(LOCTEXT("DistanceFieldReplacementMesh", "Distance Field Replacement Mesh"))
		]
		.ValueContent()
		[
			PropWidget
		];
	}

	{
		ChildrenBuilder.AddCustomRow( LOCTEXT("ApplyChanges", "Apply Changes") )
		.ValueContent()
		.HAlign(HAlign_Left)
		[
			SNew(SButton)
			.OnClicked(this, &FMeshBuildSettingsLayout::OnApplyChanges)
			.IsEnabled(ParentLODSettings.Pin().ToSharedRef(), &FLevelOfDetailSettingsLayout::IsApplyNeeded)
			[
				SNew( STextBlock )
				.Text(LOCTEXT("ApplyChanges", "Apply Changes"))
				.Font( IDetailLayoutBuilder::GetDetailFont() )
			]
		];
	}
}

void FMeshBuildSettingsLayout::UpdateSettings(const FMeshBuildSettings& InSettings)
{
	BuildSettings = InSettings;
}

FReply FMeshBuildSettingsLayout::OnApplyChanges()
{
	if( ParentLODSettings.IsValid() )
	{
		ParentLODSettings.Pin()->ApplyChanges();
	}
	return FReply::Handled();
}

ECheckBoxState FMeshBuildSettingsLayout::ShouldRecomputeNormals() const
{
	return BuildSettings.bRecomputeNormals ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

ECheckBoxState FMeshBuildSettingsLayout::ShouldRecomputeTangents() const
{
	return BuildSettings.bRecomputeTangents ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

ECheckBoxState FMeshBuildSettingsLayout::ShouldUseMikkTSpace() const
{
	return BuildSettings.bUseMikkTSpace ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

ECheckBoxState FMeshBuildSettingsLayout::ShouldRemoveDegenerates() const
{
	return BuildSettings.bRemoveDegenerates ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

ECheckBoxState FMeshBuildSettingsLayout::ShouldBuildAdjacencyBuffer() const
{
	return BuildSettings.bBuildAdjacencyBuffer ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

ECheckBoxState FMeshBuildSettingsLayout::ShouldBuildReversedIndexBuffer() const
{
	return BuildSettings.bBuildReversedIndexBuffer ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

ECheckBoxState FMeshBuildSettingsLayout::ShouldUseHighPrecisionTangentBasis() const
{
	return BuildSettings.bUseHighPrecisionTangentBasis ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

ECheckBoxState FMeshBuildSettingsLayout::ShouldUseFullPrecisionUVs() const
{
	return BuildSettings.bUseFullPrecisionUVs ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

ECheckBoxState FMeshBuildSettingsLayout::ShouldGenerateLightmapUVs() const
{
	return BuildSettings.bGenerateLightmapUVs ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

ECheckBoxState FMeshBuildSettingsLayout::ShouldGenerateDistanceFieldAsIfTwoSided() const
{
	return BuildSettings.bGenerateDistanceFieldAsIfTwoSided ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

int32 FMeshBuildSettingsLayout::GetMinLightmapResolution() const
{
	return BuildSettings.MinLightmapResolution;
}

int32 FMeshBuildSettingsLayout::GetSrcLightmapIndex() const
{
	return BuildSettings.SrcLightmapIndex;
}

int32 FMeshBuildSettingsLayout::GetDstLightmapIndex() const
{
	return BuildSettings.DstLightmapIndex;
}

TOptional<float> FMeshBuildSettingsLayout::GetBuildScaleX() const
{
	return BuildSettings.BuildScale3D.X;
}

TOptional<float> FMeshBuildSettingsLayout::GetBuildScaleY() const
{
	return BuildSettings.BuildScale3D.Y;
}

TOptional<float> FMeshBuildSettingsLayout::GetBuildScaleZ() const
{
	return BuildSettings.BuildScale3D.Z;
}

float FMeshBuildSettingsLayout::GetDistanceFieldResolutionScale() const
{
	return BuildSettings.DistanceFieldResolutionScale;
}

void FMeshBuildSettingsLayout::OnRecomputeNormalsChanged(ECheckBoxState NewState)
{
	const bool bRecomputeNormals = (NewState == ECheckBoxState::Checked) ? true : false;
	if (BuildSettings.bRecomputeNormals != bRecomputeNormals)
	{
		if (FEngineAnalytics::IsAvailable())
		{
			FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.BuildSettings"), TEXT("bRecomputeNormals"), bRecomputeNormals ? TEXT("True") : TEXT("False"));
		}
		BuildSettings.bRecomputeNormals = bRecomputeNormals;
	}
}

void FMeshBuildSettingsLayout::OnRecomputeTangentsChanged(ECheckBoxState NewState)
{
	const bool bRecomputeTangents = (NewState == ECheckBoxState::Checked) ? true : false;
	if (BuildSettings.bRecomputeTangents != bRecomputeTangents)
	{
		if (FEngineAnalytics::IsAvailable())
		{
			FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.BuildSettings"), TEXT("bRecomputeTangents"), bRecomputeTangents ? TEXT("True") : TEXT("False"));
		}
		BuildSettings.bRecomputeTangents = bRecomputeTangents;
	}
}

void FMeshBuildSettingsLayout::OnUseMikkTSpaceChanged(ECheckBoxState NewState)
{
	const bool bUseMikkTSpace = (NewState == ECheckBoxState::Checked) ? true : false;
	if (BuildSettings.bUseMikkTSpace != bUseMikkTSpace)
	{
		BuildSettings.bUseMikkTSpace = bUseMikkTSpace;
	}
}

void FMeshBuildSettingsLayout::OnRemoveDegeneratesChanged(ECheckBoxState NewState)
{
	const bool bRemoveDegenerates = (NewState == ECheckBoxState::Checked) ? true : false;
	if (BuildSettings.bRemoveDegenerates != bRemoveDegenerates)
	{
		if (FEngineAnalytics::IsAvailable())
		{
			FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.BuildSettings"), TEXT("bRemoveDegenerates"), bRemoveDegenerates ? TEXT("True") : TEXT("False"));
		}
		BuildSettings.bRemoveDegenerates = bRemoveDegenerates;
	}
}

void FMeshBuildSettingsLayout::OnBuildAdjacencyBufferChanged(ECheckBoxState NewState)
{
	const bool bBuildAdjacencyBuffer = (NewState == ECheckBoxState::Checked) ? true : false;
	if (BuildSettings.bBuildAdjacencyBuffer != bBuildAdjacencyBuffer)
	{
		if (FEngineAnalytics::IsAvailable())
		{
			FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.BuildSettings"), TEXT("bBuildAdjacencyBuffer"), bBuildAdjacencyBuffer ? TEXT("True") : TEXT("False"));
		}
		BuildSettings.bBuildAdjacencyBuffer = bBuildAdjacencyBuffer;
	}
}

void FMeshBuildSettingsLayout::OnBuildReversedIndexBufferChanged(ECheckBoxState NewState)
{
	const bool bBuildReversedIndexBuffer = (NewState == ECheckBoxState::Checked) ? true : false;
	if (BuildSettings.bBuildReversedIndexBuffer != bBuildReversedIndexBuffer)
	{
		if (FEngineAnalytics::IsAvailable())
		{
			FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.BuildSettings"), TEXT("bBuildReversedIndexBuffer"), bBuildReversedIndexBuffer ? TEXT("True") : TEXT("False"));
		}
		BuildSettings.bBuildReversedIndexBuffer = bBuildReversedIndexBuffer;
	}
}

void FMeshBuildSettingsLayout::OnUseHighPrecisionTangentBasisChanged(ECheckBoxState NewState)
{
	const bool bUseHighPrecisionTangents = (NewState == ECheckBoxState::Checked) ? true : false;
	if (BuildSettings.bUseHighPrecisionTangentBasis != bUseHighPrecisionTangents)
	{
		if (FEngineAnalytics::IsAvailable())
		{
			FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.BuildSettings"), TEXT("bUseHighPrecisionTangentBasis"), bUseHighPrecisionTangents ? TEXT("True") : TEXT("False"));
		}
		BuildSettings.bUseHighPrecisionTangentBasis = bUseHighPrecisionTangents;
	}
}

void FMeshBuildSettingsLayout::OnUseFullPrecisionUVsChanged(ECheckBoxState NewState)
{
	const bool bUseFullPrecisionUVs = (NewState == ECheckBoxState::Checked) ? true : false;
	if (BuildSettings.bUseFullPrecisionUVs != bUseFullPrecisionUVs)
	{
		if (FEngineAnalytics::IsAvailable())
		{
			FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.BuildSettings"), TEXT("bUseFullPrecisionUVs"), bUseFullPrecisionUVs ? TEXT("True") : TEXT("False"));
		}
		BuildSettings.bUseFullPrecisionUVs = bUseFullPrecisionUVs;
	}
}

void FMeshBuildSettingsLayout::OnGenerateLightmapUVsChanged(ECheckBoxState NewState)
{
	const bool bGenerateLightmapUVs = (NewState == ECheckBoxState::Checked) ? true : false;
	if (BuildSettings.bGenerateLightmapUVs != bGenerateLightmapUVs)
	{
		if (FEngineAnalytics::IsAvailable())
		{
			FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.BuildSettings"), TEXT("bGenerateLightmapUVs"), bGenerateLightmapUVs ? TEXT("True") : TEXT("False"));
		}
		BuildSettings.bGenerateLightmapUVs = bGenerateLightmapUVs;
	}
}

void FMeshBuildSettingsLayout::OnGenerateDistanceFieldAsIfTwoSidedChanged(ECheckBoxState NewState)
{
	const bool bGenerateDistanceFieldAsIfTwoSided = (NewState == ECheckBoxState::Checked) ? true : false;
	if (BuildSettings.bGenerateDistanceFieldAsIfTwoSided != bGenerateDistanceFieldAsIfTwoSided)
	{
		if (FEngineAnalytics::IsAvailable())
		{
			FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.BuildSettings"), TEXT("bGenerateDistanceFieldAsIfTwoSided"), bGenerateDistanceFieldAsIfTwoSided ? TEXT("True") : TEXT("False"));
		}
		BuildSettings.bGenerateDistanceFieldAsIfTwoSided = bGenerateDistanceFieldAsIfTwoSided;
	}
}

void FMeshBuildSettingsLayout::OnMinLightmapResolutionChanged( int32 NewValue )
{
	if (BuildSettings.MinLightmapResolution != NewValue)
	{
		if (FEngineAnalytics::IsAvailable())
		{
			FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.BuildSettings"), TEXT("MinLightmapResolution"), FString::Printf(TEXT("%i"), NewValue));
		}
		BuildSettings.MinLightmapResolution = NewValue;
	}
}

void FMeshBuildSettingsLayout::OnSrcLightmapIndexChanged( int32 NewValue )
{
	if (BuildSettings.SrcLightmapIndex != NewValue)
	{
		if (FEngineAnalytics::IsAvailable())
		{
			FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.BuildSettings"), TEXT("SrcLightmapIndex"), FString::Printf(TEXT("%i"), NewValue));
		}
		BuildSettings.SrcLightmapIndex = NewValue;
	}
}

void FMeshBuildSettingsLayout::OnDstLightmapIndexChanged( int32 NewValue )
{
	if (BuildSettings.DstLightmapIndex != NewValue)
	{
		if (FEngineAnalytics::IsAvailable())
		{
			FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.BuildSettings"), TEXT("DstLightmapIndex"), FString::Printf(TEXT("%i"), NewValue));
		}
		BuildSettings.DstLightmapIndex = NewValue;
	}
}

void FMeshBuildSettingsLayout::OnBuildScaleXChanged( float NewScaleX, ETextCommit::Type TextCommitType )
{
	if (!FMath::IsNearlyEqual(NewScaleX, 0.0f) && BuildSettings.BuildScale3D.X != NewScaleX)
	{
		if (FEngineAnalytics::IsAvailable())
		{
			FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.BuildSettings"), TEXT("BuildScale3D.X"), FString::Printf(TEXT("%.3f"), NewScaleX));
		}
		BuildSettings.BuildScale3D.X = NewScaleX;
	}
}

void FMeshBuildSettingsLayout::OnBuildScaleYChanged( float NewScaleY, ETextCommit::Type TextCommitType )
{
	if (!FMath::IsNearlyEqual(NewScaleY, 0.0f) && BuildSettings.BuildScale3D.Y != NewScaleY)
	{
		if (FEngineAnalytics::IsAvailable())
		{
			FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.BuildSettings"), TEXT("BuildScale3D.Y"), FString::Printf(TEXT("%.3f"), NewScaleY));
		}
		BuildSettings.BuildScale3D.Y = NewScaleY;
	}
}

void FMeshBuildSettingsLayout::OnBuildScaleZChanged( float NewScaleZ, ETextCommit::Type TextCommitType )
{
	if (!FMath::IsNearlyEqual(NewScaleZ, 0.0f) && BuildSettings.BuildScale3D.Z != NewScaleZ)
	{
		if (FEngineAnalytics::IsAvailable())
		{
			FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.BuildSettings"), TEXT("BuildScale3D.Z"), FString::Printf(TEXT("%.3f"), NewScaleZ));
		}
		BuildSettings.BuildScale3D.Z = NewScaleZ;
	}
}

void FMeshBuildSettingsLayout::OnDistanceFieldResolutionScaleChanged(float NewValue)
{
	BuildSettings.DistanceFieldResolutionScale = NewValue;
}

void FMeshBuildSettingsLayout::OnDistanceFieldResolutionScaleCommitted(float NewValue, ETextCommit::Type TextCommitType)
{
	if (FEngineAnalytics::IsAvailable())
	{
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.BuildSettings"), TEXT("DistanceFieldResolutionScale"), FString::Printf(TEXT("%.3f"), NewValue));
	}
	OnDistanceFieldResolutionScaleChanged(NewValue);
}

FMeshReductionSettingsLayout::FMeshReductionSettingsLayout( TSharedRef<FLevelOfDetailSettingsLayout> InParentLODSettings )
	: ParentLODSettings( InParentLODSettings )
{
	FillEnumOptions(ImportanceOptions, GetFeatureImportanceEnum());
}

FMeshReductionSettingsLayout::~FMeshReductionSettingsLayout()
{
}

void FMeshReductionSettingsLayout::GenerateHeaderRowContent( FDetailWidgetRow& NodeRow  )
{
	NodeRow.NameContent()
	[
		SNew( STextBlock )
		.Text( LOCTEXT("MeshReductionSettings", "Reduction Settings") )
		.Font( IDetailLayoutBuilder::GetDetailFont() )
	];
}


BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void FMeshReductionSettingsLayout::GenerateChildContent( IDetailChildrenBuilder& ChildrenBuilder )
{
	{
		ChildrenBuilder.AddCustomRow( LOCTEXT("PercentTriangles", "Percent Triangles") )
		.NameContent()
		[
			SNew(STextBlock)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text(LOCTEXT("PercentTriangles", "Percent Triangles"))
		]
		.ValueContent()
		[
			SNew(SSpinBox<float>)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.MinValue(0.0f)
			.MaxValue(100.0f)
			.Value(this, &FMeshReductionSettingsLayout::GetPercentTriangles)
			.OnValueChanged(this, &FMeshReductionSettingsLayout::OnPercentTrianglesChanged)
			.OnValueCommitted(this, &FMeshReductionSettingsLayout::OnPercentTrianglesCommitted)
		];

	}

	{
		ChildrenBuilder.AddCustomRow( LOCTEXT("MaxDeviation", "Max Deviation") )
		.NameContent()
		[
			SNew(STextBlock)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text(LOCTEXT("MaxDeviation", "Max Deviation"))
		]
		.ValueContent()
		[
			SNew(SSpinBox<float>)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.MinValue(0.0f)
			.MaxValue(1000.0f)
			.Value(this, &FMeshReductionSettingsLayout::GetMaxDeviation)
			.OnValueChanged(this, &FMeshReductionSettingsLayout::OnMaxDeviationChanged)
			.OnValueCommitted(this, &FMeshReductionSettingsLayout::OnMaxDeviationCommitted)
		];

	}

	{
		ChildrenBuilder.AddCustomRow( LOCTEXT("PixelError", "Pixel Error") )
		.NameContent()
		[
			SNew(STextBlock)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text(LOCTEXT("PixelError", "Pixel Error"))
		]
		.ValueContent()
		[
			SNew(SSpinBox<float>)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.MinValue(0.0f)
			.MaxValue(40.0f)
			.Value(this, &FMeshReductionSettingsLayout::GetPixelError)
			.OnValueChanged(this, &FMeshReductionSettingsLayout::OnPixelErrorChanged)
			.OnValueCommitted(this, &FMeshReductionSettingsLayout::OnPixelErrorCommitted)
		];

	}

	{
		ChildrenBuilder.AddCustomRow( LOCTEXT("Silhouette_MeshSimplification", "Silhouette") )
		.NameContent()
		[
			SNew( STextBlock )
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text( LOCTEXT("Silhouette_MeshSimplification", "Silhouette") )
		]
		.ValueContent()
		[
			SAssignNew(SilhouetteCombo, STextComboBox)
			//.Font( IDetailLayoutBuilder::GetDetailFont() )
			.ContentPadding(0)
			.OptionsSource(&ImportanceOptions)
			.InitiallySelectedItem(ImportanceOptions[ReductionSettings.SilhouetteImportance])
			.OnSelectionChanged(this, &FMeshReductionSettingsLayout::OnSilhouetteImportanceChanged)
		];

	}

	{
		ChildrenBuilder.AddCustomRow( LOCTEXT("Texture_MeshSimplification", "Texture") )
		.NameContent()
		[
			SNew( STextBlock )
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text( LOCTEXT("Texture_MeshSimplification", "Texture") )
		]
		.ValueContent()
		[
			SAssignNew( TextureCombo, STextComboBox )
			//.Font( IDetailLayoutBuilder::GetDetailFont() )
			.ContentPadding(0)
			.OptionsSource( &ImportanceOptions )
			.InitiallySelectedItem(ImportanceOptions[ReductionSettings.TextureImportance])
			.OnSelectionChanged(this, &FMeshReductionSettingsLayout::OnTextureImportanceChanged)
		];

	}

	{
		ChildrenBuilder.AddCustomRow( LOCTEXT("Shading_MeshSimplification", "Shading") )
		.NameContent()
		[
			SNew( STextBlock )
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text( LOCTEXT("Shading_MeshSimplification", "Shading") )
		]
		.ValueContent()
		[
			SAssignNew( ShadingCombo, STextComboBox )
			//.Font( IDetailLayoutBuilder::GetDetailFont() )
			.ContentPadding(0)
			.OptionsSource( &ImportanceOptions )
			.InitiallySelectedItem(ImportanceOptions[ReductionSettings.ShadingImportance])
			.OnSelectionChanged(this, &FMeshReductionSettingsLayout::OnShadingImportanceChanged)
		];

	}

	{
		ChildrenBuilder.AddCustomRow( LOCTEXT("WeldingThreshold", "Welding Threshold") )
		.NameContent()
		[
			SNew(STextBlock)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text(LOCTEXT("WeldingThreshold", "Welding Threshold"))
		]
		.ValueContent()
		[
			SNew(SSpinBox<float>)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.MinValue(0.0f)
			.MaxValue(10.0f)
			.Value(this, &FMeshReductionSettingsLayout::GetWeldingThreshold)
			.OnValueChanged(this, &FMeshReductionSettingsLayout::OnWeldingThresholdChanged)
			.OnValueCommitted(this, &FMeshReductionSettingsLayout::OnWeldingThresholdCommitted)
		];

	}

	{
		ChildrenBuilder.AddCustomRow( LOCTEXT("RecomputeNormals", "Recompute Normals") )
		.NameContent()
		[
			SNew(STextBlock)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text(LOCTEXT("RecomputeNormals", "Recompute Normals"))

		]
		.ValueContent()
		[
			SNew(SCheckBox)
			.IsChecked(this, &FMeshReductionSettingsLayout::ShouldRecalculateNormals)
			.OnCheckStateChanged(this, &FMeshReductionSettingsLayout::OnRecalculateNormalsChanged)
		];
	}

	{
		ChildrenBuilder.AddCustomRow( LOCTEXT("HardEdgeAngle", "Hard Edge Angle") )
		.NameContent()
		[
			SNew(STextBlock)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text(LOCTEXT("HardEdgeAngle", "Hard Edge Angle"))
		]
		.ValueContent()
		[
			SNew(SSpinBox<float>)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.MinValue(0.0f)
			.MaxValue(180.0f)
			.Value(this, &FMeshReductionSettingsLayout::GetHardAngleThreshold)
			.OnValueChanged(this, &FMeshReductionSettingsLayout::OnHardAngleThresholdChanged)
			.OnValueCommitted(this, &FMeshReductionSettingsLayout::OnHardAngleThresholdCommitted)
		];

	}

	{
		ChildrenBuilder.AddCustomRow( LOCTEXT("ApplyChanges", "Apply Changes") )
			.ValueContent()
			.HAlign(HAlign_Left)
			[
				SNew(SButton)
				.OnClicked(this, &FMeshReductionSettingsLayout::OnApplyChanges)
				.IsEnabled(ParentLODSettings.Pin().ToSharedRef(), &FLevelOfDetailSettingsLayout::IsApplyNeeded)
				[
					SNew( STextBlock )
					.Text(LOCTEXT("ApplyChanges", "Apply Changes"))
					.Font( IDetailLayoutBuilder::GetDetailFont() )
				]
			];
	}

	SilhouetteCombo->SetSelectedItem(ImportanceOptions[ReductionSettings.SilhouetteImportance]);
	TextureCombo->SetSelectedItem(ImportanceOptions[ReductionSettings.TextureImportance]);
	ShadingCombo->SetSelectedItem(ImportanceOptions[ReductionSettings.ShadingImportance]);
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

const FMeshReductionSettings& FMeshReductionSettingsLayout::GetSettings() const
{
	return ReductionSettings;
}

void FMeshReductionSettingsLayout::UpdateSettings(const FMeshReductionSettings& InSettings)
{
	ReductionSettings = InSettings;
}

FReply FMeshReductionSettingsLayout::OnApplyChanges()
{
	if( ParentLODSettings.IsValid() )
	{
		ParentLODSettings.Pin()->ApplyChanges();
	}
	return FReply::Handled();
}

float FMeshReductionSettingsLayout::GetPercentTriangles() const
{
	return ReductionSettings.PercentTriangles * 100.0f; // Display fraction as percentage.
}

float FMeshReductionSettingsLayout::GetMaxDeviation() const
{
	return ReductionSettings.MaxDeviation;
}

float FMeshReductionSettingsLayout::GetPixelError() const
{
	return ReductionSettings.PixelError;
}

float FMeshReductionSettingsLayout::GetWeldingThreshold() const
{
	return ReductionSettings.WeldingThreshold;
}

ECheckBoxState FMeshReductionSettingsLayout::ShouldRecalculateNormals() const
{
	return ReductionSettings.bRecalculateNormals ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

float FMeshReductionSettingsLayout::GetHardAngleThreshold() const
{
	return ReductionSettings.HardAngleThreshold;
}

void FMeshReductionSettingsLayout::OnPercentTrianglesChanged(float NewValue)
{
	// Percentage -> fraction.
	ReductionSettings.PercentTriangles = NewValue * 0.01f;
}

void FMeshReductionSettingsLayout::OnPercentTrianglesCommitted(float NewValue, ETextCommit::Type TextCommitType)
{
	if (FEngineAnalytics::IsAvailable())
	{
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.ReductionSettings"), TEXT("PercentTriangles"), FString::Printf(TEXT("%.1f"), NewValue));
	}
	OnPercentTrianglesChanged(NewValue);
}

void FMeshReductionSettingsLayout::OnMaxDeviationChanged(float NewValue)
{
	ReductionSettings.MaxDeviation = NewValue;
}

void FMeshReductionSettingsLayout::OnMaxDeviationCommitted(float NewValue, ETextCommit::Type TextCommitType)
{
	if (FEngineAnalytics::IsAvailable())
	{
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.ReductionSettings"), TEXT("MaxDeviation"), FString::Printf(TEXT("%.1f"), NewValue));
	}
	OnMaxDeviationChanged(NewValue);
}

void FMeshReductionSettingsLayout::OnPixelErrorChanged(float NewValue)
{
	ReductionSettings.PixelError = NewValue;
}

void FMeshReductionSettingsLayout::OnPixelErrorCommitted(float NewValue, ETextCommit::Type TextCommitType)
{
	if (FEngineAnalytics::IsAvailable())
	{
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.ReductionSettings"), TEXT("PixelError"), FString::Printf(TEXT("%.1f"), NewValue));
	}
	OnPixelErrorChanged(NewValue);
}

void FMeshReductionSettingsLayout::OnWeldingThresholdChanged(float NewValue)
{
	ReductionSettings.WeldingThreshold = NewValue;
}

void FMeshReductionSettingsLayout::OnWeldingThresholdCommitted(float NewValue, ETextCommit::Type TextCommitType)
{
	if (FEngineAnalytics::IsAvailable())
	{
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.ReductionSettings"), TEXT("WeldingThreshold"), FString::Printf(TEXT("%.2f"), NewValue));
	}
	OnWeldingThresholdChanged(NewValue);
}

void FMeshReductionSettingsLayout::OnRecalculateNormalsChanged(ECheckBoxState NewValue)
{
	const bool bRecalculateNormals = NewValue == ECheckBoxState::Checked;
	if (ReductionSettings.bRecalculateNormals != bRecalculateNormals)
	{
		if (FEngineAnalytics::IsAvailable())
		{
			FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.ReductionSettings"), TEXT("bRecalculateNormals"), bRecalculateNormals ? TEXT("True") : TEXT("False"));
		}
		ReductionSettings.bRecalculateNormals = bRecalculateNormals;
	}
}

void FMeshReductionSettingsLayout::OnHardAngleThresholdChanged(float NewValue)
{
	ReductionSettings.HardAngleThreshold = NewValue;
}

void FMeshReductionSettingsLayout::OnHardAngleThresholdCommitted(float NewValue, ETextCommit::Type TextCommitType)
{
	if (FEngineAnalytics::IsAvailable())
	{
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.ReductionSettings"), TEXT("HardAngleThreshold"), FString::Printf(TEXT("%.3f"), NewValue));
	}
	OnHardAngleThresholdChanged(NewValue);
}

void FMeshReductionSettingsLayout::OnSilhouetteImportanceChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo)
{
	const EMeshFeatureImportance::Type SilhouetteImportance = (EMeshFeatureImportance::Type)ImportanceOptions.Find(NewValue);
	if (ReductionSettings.SilhouetteImportance != SilhouetteImportance)
	{
		if (FEngineAnalytics::IsAvailable())
		{
			FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.ReductionSettings"), TEXT("SilhouetteImportance"), *NewValue.Get());
		}
		ReductionSettings.SilhouetteImportance = SilhouetteImportance;
	}
}

void FMeshReductionSettingsLayout::OnTextureImportanceChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo)
{
	const EMeshFeatureImportance::Type TextureImportance = (EMeshFeatureImportance::Type)ImportanceOptions.Find(NewValue);
	if (ReductionSettings.TextureImportance != TextureImportance)
	{
		if (FEngineAnalytics::IsAvailable())
		{
			FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.ReductionSettings"), TEXT("TextureImportance"), *NewValue.Get());
		}
		ReductionSettings.TextureImportance = TextureImportance;
	}
}

void FMeshReductionSettingsLayout::OnShadingImportanceChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo)
{
	const EMeshFeatureImportance::Type ShadingImportance = (EMeshFeatureImportance::Type)ImportanceOptions.Find(NewValue);
	if (ReductionSettings.ShadingImportance != ShadingImportance)
	{
		if (FEngineAnalytics::IsAvailable())
		{
			FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.ReductionSettings"), TEXT("ShadingImportance"), *NewValue.Get());
		}
		ReductionSettings.ShadingImportance = ShadingImportance;
	}
}

FMeshSectionSettingsLayout::~FMeshSectionSettingsLayout()
{
}

UStaticMesh& FMeshSectionSettingsLayout::GetStaticMesh() const
{
	UStaticMesh* StaticMesh = StaticMeshEditor.GetStaticMesh();
	check(StaticMesh);
	return *StaticMesh;
}

void FMeshSectionSettingsLayout::AddToCategory( IDetailCategoryBuilder& CategoryBuilder )
{
	FSectionListDelegates SectionListDelegates;

	SectionListDelegates.OnGetSections.BindSP(this, &FMeshSectionSettingsLayout::OnGetSectionsForView, LODIndex);
	SectionListDelegates.OnSectionChanged.BindSP(this, &FMeshSectionSettingsLayout::OnSectionChanged);
	SectionListDelegates.OnGenerateCustomNameWidgets.BindSP(this, &FMeshSectionSettingsLayout::OnGenerateCustomNameWidgetsForSection);
	SectionListDelegates.OnGenerateCustomSectionWidgets.BindSP(this, &FMeshSectionSettingsLayout::OnGenerateCustomSectionWidgetsForSection);
	SectionListDelegates.OnGenerateLodComboBox.BindSP(this, &FMeshSectionSettingsLayout::OnGenerateLodComboBoxForSectionList);

	SectionListDelegates.OnCopySectionList.BindSP(this, &FMeshSectionSettingsLayout::OnCopySectionList, LODIndex);
	SectionListDelegates.OnCanCopySectionList.BindSP(this, &FMeshSectionSettingsLayout::OnCanCopySectionList, LODIndex);
	SectionListDelegates.OnPasteSectionList.BindSP(this, &FMeshSectionSettingsLayout::OnPasteSectionList, LODIndex);
	SectionListDelegates.OnCopySectionItem.BindSP(this, &FMeshSectionSettingsLayout::OnCopySectionItem);
	SectionListDelegates.OnCanCopySectionItem.BindSP(this, &FMeshSectionSettingsLayout::OnCanCopySectionItem);
	SectionListDelegates.OnPasteSectionItem.BindSP(this, &FMeshSectionSettingsLayout::OnPasteSectionItem);

	CategoryBuilder.AddCustomBuilder(MakeShareable(new FSectionList(CategoryBuilder.GetParentLayout(), SectionListDelegates, false, 64, LODIndex)));

	StaticMeshEditor.RegisterOnSelectedLODChanged(FOnSelectedLODChanged::CreateSP(this, &FMeshSectionSettingsLayout::UpdateLODCategoryVisibility), true);
}

void FMeshSectionSettingsLayout::OnCopySectionList(int32 CurrentLODIndex)
{
	TSharedRef<FJsonObject> RootJsonObject = MakeShareable(new FJsonObject());

	UStaticMesh& StaticMesh = GetStaticMesh();
	FStaticMeshRenderData* RenderData = StaticMesh.RenderData.Get();

	if (RenderData != nullptr && RenderData->LODResources.IsValidIndex(CurrentLODIndex))
	{
		FStaticMeshLODResources& LOD = RenderData->LODResources[CurrentLODIndex];

		for (int32 SectionIndex = 0; SectionIndex < LOD.Sections.Num(); ++SectionIndex)
		{
			const FStaticMeshSection& Section = LOD.Sections[SectionIndex];

			TSharedPtr<FJsonObject> JSonSection = MakeShareable(new FJsonObject);

			JSonSection->SetNumberField(TEXT("MaterialIndex"), Section.MaterialIndex);
			JSonSection->SetBoolField(TEXT("EnableCollision"), Section.bEnableCollision);
			JSonSection->SetBoolField(TEXT("CastShadow"), Section.bCastShadow);

			RootJsonObject->SetObjectField(FString::Printf(TEXT("Section_%d"), SectionIndex), JSonSection);
		}
	}

	typedef TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>> FStringWriter;
	typedef TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>> FStringWriterFactory;

	FString CopyStr;
	TSharedRef<FStringWriter> Writer = FStringWriterFactory::Create(&CopyStr);
	FJsonSerializer::Serialize(RootJsonObject, Writer);

	if (!CopyStr.IsEmpty())
	{
		FPlatformApplicationMisc::ClipboardCopy(*CopyStr);
	}
}

bool FMeshSectionSettingsLayout::OnCanCopySectionList(int32 CurrentLODIndex) const
{
	UStaticMesh& StaticMesh = GetStaticMesh();
	FStaticMeshRenderData* RenderData = StaticMesh.RenderData.Get();

	if (RenderData != nullptr && RenderData->LODResources.IsValidIndex(CurrentLODIndex))
	{
		FStaticMeshLODResources& LOD = RenderData->LODResources[CurrentLODIndex];

		return LOD.Sections.Num() > 0;
	}

	return false;
}

void FMeshSectionSettingsLayout::OnPasteSectionList(int32 CurrentLODIndex)
{
	FString PastedText;
	FPlatformApplicationMisc::ClipboardPaste(PastedText);

	TSharedPtr<FJsonObject> RootJsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(PastedText);
	bool bResult = FJsonSerializer::Deserialize(Reader, RootJsonObject);

	if (RootJsonObject.IsValid())
	{
		UStaticMesh& StaticMesh = GetStaticMesh();
		FStaticMeshRenderData* RenderData = StaticMesh.RenderData.Get();

		if (RenderData != nullptr && RenderData->LODResources.IsValidIndex(CurrentLODIndex))
		{
			UProperty* Property = UStaticMesh::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_STRING_CHECKED(UStaticMesh, SectionInfoMap));

			GetStaticMesh().PreEditChange(Property);

			FScopedTransaction Transaction(LOCTEXT("StaticMeshToolChangedPasteSectionList", "Staticmesh editor: Pasted section list"));
			GetStaticMesh().Modify();

			FStaticMeshLODResources& LOD = RenderData->LODResources[CurrentLODIndex];

			for (int32 SectionIndex = 0; SectionIndex < LOD.Sections.Num(); ++SectionIndex)
			{
				FStaticMeshSection& Section = LOD.Sections[SectionIndex];

				const TSharedPtr<FJsonObject>* JSonSection = nullptr;

				if (RootJsonObject->TryGetObjectField(FString::Printf(TEXT("Section_%d"), SectionIndex), JSonSection))
				{
					(*JSonSection)->TryGetNumberField(TEXT("MaterialIndex"), Section.MaterialIndex);
					(*JSonSection)->TryGetBoolField(TEXT("EnableCollision"), Section.bEnableCollision);
					(*JSonSection)->TryGetBoolField(TEXT("CastShadow"), Section.bCastShadow);

					// Update the section info
					FMeshSectionInfo Info = StaticMesh.SectionInfoMap.Get(LODIndex, SectionIndex);
					Info.MaterialIndex = Section.MaterialIndex;
					Info.bCastShadow = Section.bCastShadow;
					Info.bEnableCollision = Section.bEnableCollision;

					StaticMesh.SectionInfoMap.Set(LODIndex, SectionIndex, Info);
				}
			}

			CallPostEditChange(Property);
		}
	}
}

void FMeshSectionSettingsLayout::OnCopySectionItem(int32 CurrentLODIndex, int32 SectionIndex)
{
	TSharedRef<FJsonObject> RootJsonObject = MakeShareable(new FJsonObject());

	UStaticMesh& StaticMesh = GetStaticMesh();
	FStaticMeshRenderData* RenderData = StaticMesh.RenderData.Get();

	if (RenderData != nullptr && RenderData->LODResources.IsValidIndex(CurrentLODIndex))
	{
		FStaticMeshLODResources& LOD = RenderData->LODResources[CurrentLODIndex];

		if (LOD.Sections.IsValidIndex(SectionIndex))
		{
			const FStaticMeshSection& Section = LOD.Sections[SectionIndex];

			RootJsonObject->SetNumberField(TEXT("MaterialIndex"), Section.MaterialIndex);
			RootJsonObject->SetBoolField(TEXT("EnableCollision"), Section.bEnableCollision);
			RootJsonObject->SetBoolField(TEXT("CastShadow"), Section.bCastShadow);
		}
	}

	typedef TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>> FStringWriter;
	typedef TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>> FStringWriterFactory;

	FString CopyStr;
	TSharedRef<FStringWriter> Writer = FStringWriterFactory::Create(&CopyStr);
	FJsonSerializer::Serialize(RootJsonObject, Writer);

	if (!CopyStr.IsEmpty())
	{
		FPlatformApplicationMisc::ClipboardCopy(*CopyStr);
	}
}

bool FMeshSectionSettingsLayout::OnCanCopySectionItem(int32 CurrentLODIndex, int32 SectionIndex) const
{
	UStaticMesh& StaticMesh = GetStaticMesh();
	FStaticMeshRenderData* RenderData = StaticMesh.RenderData.Get();

	if (RenderData != nullptr && RenderData->LODResources.IsValidIndex(CurrentLODIndex))
	{
		FStaticMeshLODResources& LOD = RenderData->LODResources[CurrentLODIndex];

		return LOD.Sections.IsValidIndex(SectionIndex);
	}

	return false;
}

void FMeshSectionSettingsLayout::OnPasteSectionItem(int32 CurrentLODIndex, int32 SectionIndex)
{
	FString PastedText;
	FPlatformApplicationMisc::ClipboardPaste(PastedText);

	TSharedPtr<FJsonObject> RootJsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(PastedText);
	bool bResult = FJsonSerializer::Deserialize(Reader, RootJsonObject);

	if (RootJsonObject.IsValid())
	{
		UStaticMesh& StaticMesh = GetStaticMesh();
		FStaticMeshRenderData* RenderData = StaticMesh.RenderData.Get();

		if (RenderData != nullptr && RenderData->LODResources.IsValidIndex(CurrentLODIndex))
		{
			UProperty* Property = UStaticMesh::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_STRING_CHECKED(UStaticMesh, SectionInfoMap));

			GetStaticMesh().PreEditChange(Property);

			FScopedTransaction Transaction(LOCTEXT("StaticMeshToolChangedPasteSectionItem", "Staticmesh editor: Pasted section item"));
			GetStaticMesh().Modify();

			FStaticMeshLODResources& LOD = RenderData->LODResources[CurrentLODIndex];

			if (LOD.Sections.IsValidIndex(SectionIndex))
			{
				FStaticMeshSection& Section = LOD.Sections[SectionIndex];

				RootJsonObject->TryGetNumberField(TEXT("MaterialIndex"), Section.MaterialIndex);
				RootJsonObject->TryGetBoolField(TEXT("EnableCollision"), Section.bEnableCollision);
				RootJsonObject->TryGetBoolField(TEXT("CastShadow"), Section.bCastShadow);

				// Update the section info
				FMeshSectionInfo Info = StaticMesh.SectionInfoMap.Get(LODIndex, SectionIndex);
				Info.MaterialIndex = Section.MaterialIndex;
				Info.bCastShadow = Section.bCastShadow;
				Info.bEnableCollision = Section.bEnableCollision;

				StaticMesh.SectionInfoMap.Set(LODIndex, SectionIndex, Info);
			}

			CallPostEditChange(Property);
		}
	}
}

void FMeshSectionSettingsLayout::OnGetSectionsForView(ISectionListBuilder& OutSections, int32 ForLODIndex)
{
	check(LODIndex == ForLODIndex);
	UStaticMesh& StaticMesh = GetStaticMesh();
	FStaticMeshRenderData* RenderData = StaticMesh.RenderData.Get();
	if (RenderData && RenderData->LODResources.IsValidIndex(LODIndex))
	{
		FStaticMeshLODResources& LOD = RenderData->LODResources[LODIndex];
		int32 NumSections = LOD.Sections.Num();
		
		for (int32 SectionIndex = 0; SectionIndex < NumSections; ++SectionIndex)
		{
			FMeshSectionInfo Info = StaticMesh.SectionInfoMap.Get(LODIndex, SectionIndex);
			int32 MaterialIndex = Info.MaterialIndex;
			if (StaticMesh.StaticMaterials.IsValidIndex(MaterialIndex))
			{
				FName CurrentSectionMaterialSlotName = StaticMesh.StaticMaterials[MaterialIndex].MaterialSlotName;
				FName CurrentSectionOriginalImportedMaterialName = StaticMesh.StaticMaterials[MaterialIndex].ImportedMaterialSlotName;
				TMap<int32, FName> AvailableSectionName;
				int32 CurrentIterMaterialIndex = 0;
				for (const FStaticMaterial &SkeletalMaterial : StaticMesh.StaticMaterials)
				{
					if (MaterialIndex != CurrentIterMaterialIndex)
						AvailableSectionName.Add(CurrentIterMaterialIndex, SkeletalMaterial.MaterialSlotName);
					CurrentIterMaterialIndex++;
				}
				UMaterialInterface* SectionMaterial = StaticMesh.StaticMaterials[MaterialIndex].MaterialInterface;
				if (SectionMaterial == NULL)
				{
					SectionMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
				}
				OutSections.AddSection(LODIndex, SectionIndex, CurrentSectionMaterialSlotName, MaterialIndex, CurrentSectionOriginalImportedMaterialName, AvailableSectionName, StaticMesh.StaticMaterials[MaterialIndex].MaterialInterface, false);
			}
		}
	}
}

void FMeshSectionSettingsLayout::OnSectionChanged(int32 ForLODIndex, int32 SectionIndex, int32 NewMaterialSlotIndex, FName NewMaterialSlotName)
{
	check(LODIndex == ForLODIndex);
	UStaticMesh& StaticMesh = GetStaticMesh();

	check(StaticMesh.StaticMaterials.IsValidIndex(NewMaterialSlotIndex));

	int32 NewStaticMaterialIndex = INDEX_NONE;
	for (int StaticMaterialIndex = 0; StaticMaterialIndex < StaticMesh.StaticMaterials.Num(); ++StaticMaterialIndex)
	{
		if (NewMaterialSlotIndex == StaticMaterialIndex && StaticMesh.StaticMaterials[StaticMaterialIndex].MaterialSlotName == NewMaterialSlotName)
		{
			NewStaticMaterialIndex = StaticMaterialIndex;
			break;
		}
	}
	check(NewStaticMaterialIndex != INDEX_NONE);
	check(StaticMesh.RenderData);
	FStaticMeshRenderData* RenderData = StaticMesh.RenderData.Get();
	if (RenderData && RenderData->LODResources.IsValidIndex(LODIndex))
	{
		FStaticMeshLODResources& LOD = RenderData->LODResources[LODIndex];
		if (LOD.Sections.IsValidIndex(SectionIndex))
		{
			UProperty* Property = UStaticMesh::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_STRING_CHECKED(UStaticMesh, SectionInfoMap));

			GetStaticMesh().PreEditChange(Property);

			FScopedTransaction Transaction(LOCTEXT("StaticMeshOnSectionChangedTransaction", "Staticmesh editor: Section material slot changed"));
			GetStaticMesh().Modify();
			FMeshSectionInfo Info = StaticMesh.SectionInfoMap.Get(LODIndex, SectionIndex);
			Info.MaterialIndex = NewStaticMaterialIndex;
			StaticMesh.SectionInfoMap.Set(LODIndex, SectionIndex, Info);
			CallPostEditChange();
		}
	}
}

TSharedRef<SWidget> FMeshSectionSettingsLayout::OnGenerateCustomNameWidgetsForSection(int32 ForLODIndex, int32 SectionIndex)
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SCheckBox)
			.IsChecked(this, &FMeshSectionSettingsLayout::IsSectionHighlighted, SectionIndex)
			.OnCheckStateChanged(this, &FMeshSectionSettingsLayout::OnSectionHighlightedChanged, SectionIndex)
			.ToolTipText(LOCTEXT("Highlight_ToolTip", "Highlights this section in the viewport"))
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.ColorAndOpacity( FLinearColor( 0.4f, 0.4f, 0.4f, 1.0f) )
				.Text(LOCTEXT("Highlight", "Highlight"))

			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 2, 0, 0)
		[
			SNew(SCheckBox)
			.IsChecked(this, &FMeshSectionSettingsLayout::IsSectionIsolatedEnabled, SectionIndex)
			.OnCheckStateChanged(this, &FMeshSectionSettingsLayout::OnSectionIsolatedChanged, SectionIndex)
			.ToolTipText(LOCTEXT("Isolate_ToolTip", "Isolates this section in the viewport"))
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.ColorAndOpacity(FLinearColor(0.4f, 0.4f, 0.4f, 1.0f))
				.Text(LOCTEXT("Isolate", "Isolate"))

			]
		];
}

TSharedRef<SWidget> FMeshSectionSettingsLayout::OnGenerateCustomSectionWidgetsForSection(int32 ForLODIndex, int32 SectionIndex)
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2, 0, 2, 0)
		[
			SNew(SCheckBox)
				.IsChecked(this, &FMeshSectionSettingsLayout::DoesSectionCastShadow, SectionIndex)
				.OnCheckStateChanged(this, &FMeshSectionSettingsLayout::OnSectionCastShadowChanged, SectionIndex)
			[
				SNew(STextBlock)
					.Font(FEditorStyle::GetFontStyle("StaticMeshEditor.NormalFont"))
					.Text(LOCTEXT("CastShadow", "Cast Shadow"))
			]
		]
		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2,0,2,0)
		[
			SNew(SCheckBox)
				.IsEnabled(this, &FMeshSectionSettingsLayout::SectionCollisionEnabled)
				.ToolTipText(this, &FMeshSectionSettingsLayout::GetCollisionEnabledToolTip)
				.IsChecked(this, &FMeshSectionSettingsLayout::DoesSectionCollide, SectionIndex)
				.OnCheckStateChanged(this, &FMeshSectionSettingsLayout::OnSectionCollisionChanged, SectionIndex)
			[
				SNew(STextBlock)
					.Font(FEditorStyle::GetFontStyle("StaticMeshEditor.NormalFont"))
					.Text(LOCTEXT("EnableCollision", "Enable Collision"))
			]
		];
}

ECheckBoxState FMeshSectionSettingsLayout::DoesSectionCastShadow(int32 SectionIndex) const
{
	UStaticMesh& StaticMesh = GetStaticMesh();
	FMeshSectionInfo Info = StaticMesh.SectionInfoMap.Get(LODIndex, SectionIndex);
	return Info.bCastShadow ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void FMeshSectionSettingsLayout::OnSectionCastShadowChanged(ECheckBoxState NewState, int32 SectionIndex)
{
	UStaticMesh& StaticMesh = GetStaticMesh();

	FText TransactionTest = LOCTEXT("StaticMeshEditorSetShadowCastingSectionFlag", "Staticmesh editor: Set Shadow Casting For section");
	if (NewState == ECheckBoxState::Unchecked)
	{
		TransactionTest = LOCTEXT("StaticMeshEditorClearShadowCastingSectionFlag", "Staticmesh editor: Clear Shadow Casting For section");
	}
	FScopedTransaction Transaction(TransactionTest);

	UProperty* Property = UStaticMesh::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_STRING_CHECKED(UStaticMesh, SectionInfoMap));
	StaticMesh.PreEditChange(Property);
	StaticMesh.Modify();
	
	FMeshSectionInfo Info = StaticMesh.SectionInfoMap.Get(LODIndex, SectionIndex);
	Info.bCastShadow = (NewState == ECheckBoxState::Checked) ? true : false;
	StaticMesh.SectionInfoMap.Set(LODIndex, SectionIndex, Info);
	CallPostEditChange();
}

bool FMeshSectionSettingsLayout::SectionCollisionEnabled() const
{
	UStaticMesh& StaticMesh = GetStaticMesh();
	// Only enable 'Enable Collision' check box if this LOD is used for collision
	return (StaticMesh.LODForCollision == LODIndex);
}

FText FMeshSectionSettingsLayout::GetCollisionEnabledToolTip() const
{
	UStaticMesh& StaticMesh = GetStaticMesh();
	
	// If using a different LOD for collision, disable the check box
	if (StaticMesh.LODForCollision != LODIndex)
	{
		return LOCTEXT("EnableCollisionToolTipDisabled", "This LOD is not used for collision, see the LODForCollision setting.");
	}
	// This LOD is used for collision, give info on what flag does
	else
	{
		return LOCTEXT("EnableCollisionToolTipEnabled", "Controls whether this section ever has per-poly collision. Disabling this where possible will lower memory usage for this mesh.");
	}
}

ECheckBoxState FMeshSectionSettingsLayout::DoesSectionCollide(int32 SectionIndex) const
{
	UStaticMesh& StaticMesh = GetStaticMesh();
	FMeshSectionInfo Info = StaticMesh.SectionInfoMap.Get(LODIndex, SectionIndex);
	return Info.bEnableCollision ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void FMeshSectionSettingsLayout::OnSectionCollisionChanged(ECheckBoxState NewState, int32 SectionIndex)
{
	UStaticMesh& StaticMesh = GetStaticMesh();

	FText TransactionTest = LOCTEXT("StaticMeshEditorSetCollisionSectionFlag", "Staticmesh editor: Set Collision For section");
	if (NewState == ECheckBoxState::Unchecked)
	{
		TransactionTest = LOCTEXT("StaticMeshEditorClearCollisionSectionFlag", "Staticmesh editor: Clear Collision For section");
	}
	FScopedTransaction Transaction(TransactionTest);

	UProperty* Property = UStaticMesh::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_STRING_CHECKED(UStaticMesh, SectionInfoMap));
	StaticMesh.PreEditChange(Property);
	StaticMesh.Modify();

	FMeshSectionInfo Info = StaticMesh.SectionInfoMap.Get(LODIndex, SectionIndex);
	Info.bEnableCollision = (NewState == ECheckBoxState::Checked) ? true : false;
	StaticMesh.SectionInfoMap.Set(LODIndex, SectionIndex, Info);
	CallPostEditChange();
}

ECheckBoxState FMeshSectionSettingsLayout::IsSectionHighlighted(int32 SectionIndex) const
{
	ECheckBoxState State = ECheckBoxState::Unchecked;
	UStaticMeshComponent* Component = StaticMeshEditor.GetStaticMeshComponent();
	if (Component)
	{
		State = Component->SelectedEditorSection == SectionIndex ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}
	return State;
}

void FMeshSectionSettingsLayout::OnSectionHighlightedChanged(ECheckBoxState NewState, int32 SectionIndex)
{
	UStaticMeshComponent* Component = StaticMeshEditor.GetStaticMeshComponent();
	if (Component)
	{
		if (NewState == ECheckBoxState::Checked)
		{
			Component->SelectedEditorSection = SectionIndex;
			if (Component->SectionIndexPreview != SectionIndex)
			{
				// Unhide all mesh sections
				Component->SetSectionPreview(INDEX_NONE);
			}
			Component->SetMaterialPreview(INDEX_NONE);
			Component->SelectedEditorMaterial = INDEX_NONE;
		}
		else if (NewState == ECheckBoxState::Unchecked)
		{
			Component->SelectedEditorSection = INDEX_NONE;
		}
		Component->MarkRenderStateDirty();
		StaticMeshEditor.RefreshViewport();
	}
}

ECheckBoxState FMeshSectionSettingsLayout::IsSectionIsolatedEnabled(int32 SectionIndex) const
{
	ECheckBoxState State = ECheckBoxState::Unchecked;
	const UStaticMeshComponent* Component = StaticMeshEditor.GetStaticMeshComponent();
	if (Component)
	{
		State = Component->SectionIndexPreview == SectionIndex ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}
	return State;
}

void FMeshSectionSettingsLayout::OnSectionIsolatedChanged(ECheckBoxState NewState, int32 SectionIndex)
{
	UStaticMeshComponent* Component = StaticMeshEditor.GetStaticMeshComponent();
	if (Component)
	{
		if (NewState == ECheckBoxState::Checked)
		{
			Component->SetSectionPreview(SectionIndex);
			if (Component->SelectedEditorSection != SectionIndex)
			{
				Component->SelectedEditorSection = INDEX_NONE;
			}
			Component->SetMaterialPreview(INDEX_NONE);
			Component->SelectedEditorMaterial = INDEX_NONE;
		}
		else if (NewState == ECheckBoxState::Unchecked)
		{
			Component->SetSectionPreview(INDEX_NONE);
		}
		Component->MarkRenderStateDirty();
		StaticMeshEditor.RefreshViewport();
	}
}

void FMeshSectionSettingsLayout::CallPostEditChange(UProperty* PropertyChanged/*=nullptr*/)
{
	UStaticMesh& StaticMesh = GetStaticMesh();
	if( PropertyChanged )
	{
		FPropertyChangedEvent PropertyUpdateStruct(PropertyChanged);
		StaticMesh.PostEditChangeProperty(PropertyUpdateStruct);
	}
	else
	{
		StaticMesh.Modify();
		StaticMesh.PostEditChange();
	}
	if(StaticMesh.BodySetup)
	{
		StaticMesh.BodySetup->CreatePhysicsMeshes();
	}
	StaticMeshEditor.RefreshViewport();
}

void FMeshSectionSettingsLayout::SetCurrentLOD(int32 NewLodIndex)
{
	if (StaticMeshEditor.GetStaticMeshComponent() == nullptr || LodCategoriesPtr == nullptr)
	{
		return;
	}
	int32 CurrentDisplayLOD = StaticMeshEditor.GetStaticMeshComponent()->ForcedLodModel;
	int32 RealCurrentDisplayLOD = CurrentDisplayLOD == 0 ? 0 : CurrentDisplayLOD - 1;
	int32 RealNewLOD = NewLodIndex == 0 ? 0 : NewLodIndex - 1;
	
	if (CurrentDisplayLOD == NewLodIndex || !LodCategoriesPtr->IsValidIndex(RealCurrentDisplayLOD) || !LodCategoriesPtr->IsValidIndex(RealNewLOD))
	{
		return;
	}

	StaticMeshEditor.GetStaticMeshComponent()->SetForcedLodModel(NewLodIndex);

	//Reset the preview section since we do not edit the same LOD
	StaticMeshEditor.GetStaticMeshComponent()->SetSectionPreview(INDEX_NONE);
	StaticMeshEditor.GetStaticMeshComponent()->SelectedEditorSection = INDEX_NONE;
}

void FMeshSectionSettingsLayout::UpdateLODCategoryVisibility()
{
	if (CustomLODEditModePtr != nullptr && *CustomLODEditModePtr == true)
	{
		//Do not change the Category visibility if we are in custom mode
		return;
	}
	bool bAutoLod = false;
	if (StaticMeshEditor.GetStaticMeshComponent() != nullptr)
	{
		bAutoLod = StaticMeshEditor.GetStaticMeshComponent()->ForcedLodModel == 0;
	}
	int32 CurrentDisplayLOD = bAutoLod ? 0 : StaticMeshEditor.GetStaticMeshComponent()->ForcedLodModel - 1;

	if (LodCategoriesPtr != nullptr && LodCategoriesPtr->IsValidIndex(CurrentDisplayLOD) && StaticMeshEditor.GetStaticMesh())
	{
		int32 StaticMeshLodNumber = StaticMeshEditor.GetStaticMesh()->GetNumLODs();
		for (int32 LodCategoryIndex = 0; LodCategoryIndex < StaticMeshLodNumber; ++LodCategoryIndex)
		{
			if (!LodCategoriesPtr->IsValidIndex(LodCategoryIndex))
			{
				break;
			}
			(*LodCategoriesPtr)[LodCategoryIndex]->SetCategoryVisibility(CurrentDisplayLOD == LodCategoryIndex);
		}
		//Reset the preview section since we do not edit the same LOD
		StaticMeshEditor.GetStaticMeshComponent()->SetSectionPreview(INDEX_NONE);
		StaticMeshEditor.GetStaticMeshComponent()->SelectedEditorSection = INDEX_NONE;
	}
}

FText FMeshSectionSettingsLayout::GetCurrentLodName() const
{
	bool bAutoLod = false;
	if (StaticMeshEditor.GetStaticMeshComponent() != nullptr)
	{
		bAutoLod = StaticMeshEditor.GetStaticMeshComponent()->ForcedLodModel == 0;
	}
	int32 CurrentDisplayLOD = bAutoLod ? 0 : StaticMeshEditor.GetStaticMeshComponent()->ForcedLodModel - 1;
	return FText::FromString(bAutoLod ? FString(TEXT("Auto (LOD0)")) : (FString(TEXT("LOD")) + FString::FromInt(CurrentDisplayLOD)));
}

FText FMeshSectionSettingsLayout::GetCurrentLodTooltip() const
{
	if (StaticMeshEditor.GetStaticMeshComponent() != nullptr && StaticMeshEditor.GetStaticMeshComponent()->ForcedLodModel == 0)
	{
		return FText::FromString(TEXT("LOD0 is edit when selecting Auto LOD"));
	}
	return FText::GetEmpty();
}

TSharedRef<SWidget> FMeshSectionSettingsLayout::OnGenerateLodComboBoxForSectionList(int32 LodIndex)
{
	return SNew(SComboButton)
		.Visibility(this, &FMeshSectionSettingsLayout::LodComboBoxVisibilityForSectionList, LodIndex)
		.OnGetMenuContent(this, &FMeshSectionSettingsLayout::OnGenerateLodMenuForSectionList, LodIndex)
		.VAlign(VAlign_Center)
		.ContentPadding(2)
		.ButtonContent()
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(this, &FMeshSectionSettingsLayout::GetCurrentLodName)
			.ToolTipText(this, &FMeshSectionSettingsLayout::GetCurrentLodTooltip)
		];
}

EVisibility FMeshSectionSettingsLayout::LodComboBoxVisibilityForSectionList(int32 LodIndex) const
{
	//No combo box when in Custom mode
	if (CustomLODEditModePtr != nullptr && *CustomLODEditModePtr == true)
	{
		return EVisibility::Hidden;
	}
	return EVisibility::All;
}

TSharedRef<SWidget> FMeshSectionSettingsLayout::OnGenerateLodMenuForSectionList(int32 LodIndex)
{
	UStaticMesh* StaticMesh = StaticMeshEditor.GetStaticMesh();

	if (StaticMesh == nullptr)
	{
		return SNullWidget::NullWidget;
	}

	bool bAutoLod = false;
	if (StaticMeshEditor.GetStaticMeshComponent() != nullptr)
	{
		bAutoLod = StaticMeshEditor.GetStaticMeshComponent()->ForcedLodModel == 0;
	}
	const int32 StaticMeshLODCount = StaticMesh->GetNumLODs();
	if (StaticMeshLODCount < 2)
	{
		return SNullWidget::NullWidget;
	}
	FMenuBuilder MenuBuilder(true, NULL);

	FText AutoLodText = FText::FromString((TEXT("Auto LOD")));
	FUIAction AutoLodAction(FExecuteAction::CreateSP(this, &FMeshSectionSettingsLayout::SetCurrentLOD, 0));
	MenuBuilder.AddMenuEntry(AutoLodText, LOCTEXT("OnGenerateLodMenuForSectionList_Auto_ToolTip", "LOD0 is edit when selecting Auto LOD"), FSlateIcon(), AutoLodAction);
	// Add a menu item for each texture.  Clicking on the texture will display it in the content browser
	for (int32 AllLodIndex = 0; AllLodIndex < StaticMeshLODCount; ++AllLodIndex)
	{
		FText LODLevelString = FText::FromString((TEXT("LOD ") + FString::FromInt(AllLodIndex)));
		FUIAction Action(FExecuteAction::CreateSP(this, &FMeshSectionSettingsLayout::SetCurrentLOD, AllLodIndex + 1));
		MenuBuilder.AddMenuEntry(LODLevelString, FText::GetEmpty(), FSlateIcon(), Action);
	}

	return MenuBuilder.MakeWidget();
}

//////////////////////////////////////////////////////////////////////////
// FMeshMaterialLayout
//////////////////////////////////////////////////////////////////////////

FMeshMaterialsLayout::~FMeshMaterialsLayout()
{
}

UStaticMesh& FMeshMaterialsLayout::GetStaticMesh() const
{
	UStaticMesh* StaticMesh = StaticMeshEditor.GetStaticMesh();
	check(StaticMesh);
	return *StaticMesh;
}

void FMeshMaterialsLayout::AddToCategory(IDetailCategoryBuilder& CategoryBuilder)
{
	CategoryBuilder.AddCustomRow(LOCTEXT("AddLODLevelCategories_MaterialArrayOperationAdd", "Add Material Slot"))
		.CopyAction(FUIAction(FExecuteAction::CreateSP(this, &FMeshMaterialsLayout::OnCopyMaterialList), FCanExecuteAction::CreateSP(this, &FMeshMaterialsLayout::OnCanCopyMaterialList)))
		.PasteAction(FUIAction(FExecuteAction::CreateSP(this, &FMeshMaterialsLayout::OnPasteMaterialList)))
		.NameContent()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(LOCTEXT("AddLODLevelCategories_MaterialArrayOperations", "Material Slots"))
		]
		.ValueContent()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(this, &FMeshMaterialsLayout::GetMaterialArrayText)
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Padding(2.0f, 1.0f)
				[
					SNew(SButton)
					.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
					.Text(LOCTEXT("AddLODLevelCategories_MaterialArrayOpAdd", "Add Material Slot"))
					.ToolTipText(LOCTEXT("AddLODLevelCategories_MaterialArrayOpAdd_Tooltip", "Add Material Slot at the end of the Material slot array. Those Material slots can be used to override a LODs section, (not the base LOD)"))
					.ContentPadding(4.0f)
					.ForegroundColor(FSlateColor::UseForeground())
					.OnClicked(this, &FMeshMaterialsLayout::AddMaterialSlot)
					.IsEnabled(true)
					.IsFocusable(false)
					[
						SNew(SImage)
						.Image(FEditorStyle::GetBrush("PropertyWindow.Button_AddToArray"))
						.ColorAndOpacity(FSlateColor::UseForeground())
					]
				]
			]
		];

	FMaterialListDelegates MaterialListDelegates;
	MaterialListDelegates.OnGetMaterials.BindSP(this, &FMeshMaterialsLayout::GetMaterials);
	MaterialListDelegates.OnMaterialChanged.BindSP(this, &FMeshMaterialsLayout::OnMaterialChanged);
	MaterialListDelegates.OnGenerateCustomMaterialWidgets.BindSP(this, &FMeshMaterialsLayout::OnGenerateWidgetsForMaterial);
	MaterialListDelegates.OnGenerateCustomNameWidgets.BindSP(this, &FMeshMaterialsLayout::OnGenerateNameWidgetsForMaterial);
	MaterialListDelegates.OnMaterialListDirty.BindSP(this, &FMeshMaterialsLayout::OnMaterialListDirty);
	MaterialListDelegates.OnResetMaterialToDefaultClicked.BindSP(this, &FMeshMaterialsLayout::OnResetMaterialToDefaultClicked);

	MaterialListDelegates.OnCopyMaterialItem.BindSP(this, &FMeshMaterialsLayout::OnCopyMaterialItem);
	MaterialListDelegates.OnCanCopyMaterialItem.BindSP(this, &FMeshMaterialsLayout::OnCanCopyMaterialItem);
	MaterialListDelegates.OnPasteMaterialItem.BindSP(this, &FMeshMaterialsLayout::OnPasteMaterialItem);

	CategoryBuilder.AddCustomBuilder(MakeShareable(new FMaterialList(CategoryBuilder.GetParentLayout(), MaterialListDelegates, false, true, true)));
}

void FMeshMaterialsLayout::OnCopyMaterialList()
{
	UProperty* Property = UStaticMesh::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_STRING_CHECKED(UStaticMesh, StaticMaterials));
	check(Property != nullptr);

	auto JsonValue = FJsonObjectConverter::UPropertyToJsonValue(Property, &GetStaticMesh().StaticMaterials, 0, 0);

	typedef TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>> FStringWriter;
	typedef TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>> FStringWriterFactory;

	FString CopyStr;
	TSharedRef<FStringWriter> Writer = FStringWriterFactory::Create(&CopyStr);
	FJsonSerializer::Serialize(JsonValue.ToSharedRef(), TEXT(""), Writer);

	if (!CopyStr.IsEmpty())
	{
		FPlatformApplicationMisc::ClipboardCopy(*CopyStr);
	}
}

bool FMeshMaterialsLayout::OnCanCopyMaterialList() const
{
	return GetStaticMesh().StaticMaterials.Num() > 0;
}

void FMeshMaterialsLayout::OnPasteMaterialList()
{
	FString PastedText;
	FPlatformApplicationMisc::ClipboardPaste(PastedText);

	TSharedPtr<FJsonValue> RootJsonValue;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(PastedText);
	FJsonSerializer::Deserialize(Reader, RootJsonValue);

	if (RootJsonValue.IsValid())
	{
		UProperty* Property = UStaticMesh::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_STRING_CHECKED(UStaticMesh, StaticMaterials));
		check(Property != nullptr);

		GetStaticMesh().PreEditChange(Property);
		FScopedTransaction Transaction(LOCTEXT("StaticMeshToolChangedPasteMaterialList", "Staticmesh editor: Pasted material list"));
		GetStaticMesh().Modify();

		TArray<FStaticMaterial> TempMaterials;
		FJsonObjectConverter::JsonValueToUProperty(RootJsonValue, Property, &TempMaterials, 0, 0);
		//Do not change the number of material in the array
		for (int32 MaterialIndex = 0; MaterialIndex < TempMaterials.Num(); ++MaterialIndex)
		{
			if (GetStaticMesh().StaticMaterials.IsValidIndex(MaterialIndex))
			{
				GetStaticMesh().StaticMaterials[MaterialIndex].MaterialInterface = TempMaterials[MaterialIndex].MaterialInterface;
			}
		}

		CallPostEditChange(Property);
	}
}

void FMeshMaterialsLayout::OnCopyMaterialItem(int32 CurrentSlot)
{
	TSharedRef<FJsonObject> RootJsonObject = MakeShareable(new FJsonObject());

	if (GetStaticMesh().StaticMaterials.IsValidIndex(CurrentSlot))
	{
		const FStaticMaterial &Material = GetStaticMesh().StaticMaterials[CurrentSlot];

		FJsonObjectConverter::UStructToJsonObject(FStaticMaterial::StaticStruct(), &Material, RootJsonObject, 0, 0);
	}

	typedef TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>> FStringWriter;
	typedef TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>> FStringWriterFactory;

	FString CopyStr;
	TSharedRef<FStringWriter> Writer = FStringWriterFactory::Create(&CopyStr);
	FJsonSerializer::Serialize(RootJsonObject, Writer);

	if (!CopyStr.IsEmpty())
	{
		FPlatformApplicationMisc::ClipboardCopy(*CopyStr);
	}
}

bool FMeshMaterialsLayout::OnCanCopyMaterialItem(int32 CurrentSlot) const
{
	return GetStaticMesh().StaticMaterials.IsValidIndex(CurrentSlot);
}

void FMeshMaterialsLayout::OnPasteMaterialItem(int32 CurrentSlot)
{
	FString PastedText;
	FPlatformApplicationMisc::ClipboardPaste(PastedText);

	TSharedPtr<FJsonObject> RootJsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(PastedText);
	FJsonSerializer::Deserialize(Reader, RootJsonObject);

	if (RootJsonObject.IsValid())
	{
		UProperty* Property = UStaticMesh::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_STRING_CHECKED(UStaticMesh, StaticMaterials));
		check(Property != nullptr);

		GetStaticMesh().PreEditChange(Property);

		FScopedTransaction Transaction(LOCTEXT("StaticMeshToolChangedPasteMaterialItem", "Staticmesh editor: Pasted material item"));
		GetStaticMesh().Modify();

		if (GetStaticMesh().StaticMaterials.IsValidIndex(CurrentSlot))
		{
			FStaticMaterial TmpStaticMaterial;
			FJsonObjectConverter::JsonObjectToUStruct(RootJsonObject.ToSharedRef(), FStaticMaterial::StaticStruct(), &TmpStaticMaterial, 0, 0);
			GetStaticMesh().StaticMaterials[CurrentSlot].MaterialInterface = TmpStaticMaterial.MaterialInterface;
		}

		CallPostEditChange(Property);
	}
}

FReply FMeshMaterialsLayout::AddMaterialSlot()
{
	UStaticMesh& StaticMesh = GetStaticMesh();

	FScopedTransaction Transaction(LOCTEXT("FMeshMaterialsLayout_AddMaterialSlot", "Staticmesh editor: Add material slot"));
	StaticMesh.Modify();
	StaticMesh.StaticMaterials.Add(FStaticMaterial());

	StaticMesh.PostEditChange();

	return FReply::Handled();
}

FText FMeshMaterialsLayout::GetMaterialArrayText() const
{
	UStaticMesh& StaticMesh = GetStaticMesh();

	FString MaterialArrayText = TEXT(" Material Slots");
	int32 SlotNumber = 0;
	SlotNumber = StaticMesh.StaticMaterials.Num();
	MaterialArrayText = FString::FromInt(SlotNumber) + MaterialArrayText;
	return FText::FromString(MaterialArrayText);
}

void FMeshMaterialsLayout::GetMaterials(IMaterialListBuilder& ListBuilder)
{
	UStaticMesh& StaticMesh = GetStaticMesh();
	for (int32 MaterialIndex = 0; MaterialIndex < StaticMesh.StaticMaterials.Num(); ++MaterialIndex)
	{
		UMaterialInterface* Material = StaticMesh.GetMaterial(MaterialIndex);
		if (Material == NULL)
		{
			Material = UMaterial::GetDefaultMaterial(MD_Surface);
		}
		ListBuilder.AddMaterial(MaterialIndex, Material, /*bCanBeReplaced=*/ true);
	}
}

void FMeshMaterialsLayout::OnMaterialChanged(UMaterialInterface* NewMaterial, UMaterialInterface* PrevMaterial, int32 MaterialIndex, bool bReplaceAll)
{
	UStaticMesh& StaticMesh = GetStaticMesh();
	FScopedTransaction ScopeTransaction(LOCTEXT("StaticMeshEditorMaterialChanged", "Staticmesh editor: Material changed"));

	// flag the property (Materials) we're modifying so that not all of the object is rebuilt.
	UProperty* ChangedProperty = NULL;
	ChangedProperty = FindField<UProperty>(UStaticMesh::StaticClass(), "StaticMaterials");
	check(ChangedProperty);
	StaticMesh.PreEditChange(ChangedProperty);

	if (StaticMesh.StaticMaterials.IsValidIndex(MaterialIndex))
	{
		StaticMesh.StaticMaterials[MaterialIndex].MaterialInterface = NewMaterial;
		if (NewMaterial != nullptr)
		{
			//Set the Material slot name to a good default one
			if (StaticMesh.StaticMaterials[MaterialIndex].MaterialSlotName == NAME_None)
			{
				StaticMesh.StaticMaterials[MaterialIndex].MaterialSlotName = NewMaterial->GetFName();
			}
			//Set the original fbx material name so we can re-import correctly
			if (StaticMesh.StaticMaterials[MaterialIndex].ImportedMaterialSlotName == NAME_None)
			{
				StaticMesh.StaticMaterials[MaterialIndex].ImportedMaterialSlotName = NewMaterial->GetFName();
			}
		}
	}

	CallPostEditChange(ChangedProperty);
}

TSharedRef<SWidget> FMeshMaterialsLayout::OnGenerateWidgetsForMaterial(UMaterialInterface* Material, int32 SlotIndex)
{
	UStaticMesh& StaticMesh = GetStaticMesh();
	bool bMaterialIsUsed = false;
	if(MaterialUsedMap.Contains(SlotIndex))
	{
		bMaterialIsUsed = MaterialUsedMap.Find(SlotIndex)->Num() > 0;
	}

	return 
		SNew(SMaterialSlotWidget, SlotIndex, bMaterialIsUsed)
		.MaterialName(this, &FMeshMaterialsLayout::GetMaterialNameText, SlotIndex)
		.OnMaterialNameCommitted(this, &FMeshMaterialsLayout::OnMaterialNameCommitted, SlotIndex)
		.CanDeleteMaterialSlot(this, &FMeshMaterialsLayout::CanDeleteMaterialSlot, SlotIndex)
		.OnDeleteMaterialSlot(this, &FMeshMaterialsLayout::OnDeleteMaterialSlot, SlotIndex)
		.ToolTipText(this, &FMeshMaterialsLayout::GetOriginalImportMaterialNameText, SlotIndex);

#if 0 // HACK!!! Temporary disabled
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0,2,0,0)
		[
			SNew(SCheckBox)
				.Visibility(this, &FMeshMaterialsLayout::GetOverrideUVDensityVisibililty)
				.IsChecked(this, &FMeshMaterialsLayout::IsUVDensityOverridden, SlotIndex)
				.OnCheckStateChanged(this, &FMeshMaterialsLayout::OnOverrideUVDensityChanged, SlotIndex)
			[
				SNew(STextBlock)
					.Font(FEditorStyle::GetFontStyle("StaticMeshEditor.NormalFont"))
					.Text(LOCTEXT("OverrideUVDensity", "Override UV Density"))
			]
		]
		+ GetUVDensitySlot(SlotIndex, 0)
		+ GetUVDensitySlot(SlotIndex, 1)
		+ GetUVDensitySlot(SlotIndex, 2)
		+ GetUVDensitySlot(SlotIndex, 3);
#endif
}

TSharedRef<SWidget> FMeshMaterialsLayout::OnGenerateNameWidgetsForMaterial(UMaterialInterface* Material, int32 SlotIndex)
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SCheckBox)
			.IsChecked(this, &FMeshMaterialsLayout::IsMaterialHighlighted, SlotIndex)
			.OnCheckStateChanged(this, &FMeshMaterialsLayout::OnMaterialHighlightedChanged, SlotIndex)
			.ToolTipText(LOCTEXT("Highlight_CustomMaterialName_ToolTip", "Highlights this material in the viewport"))
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.ColorAndOpacity(FLinearColor(0.4f, 0.4f, 0.4f, 1.0f))
				.Text(LOCTEXT("Highlight", "Highlight"))
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 2, 0, 0)
		[
			SNew(SCheckBox)
			.IsChecked(this, &FMeshMaterialsLayout::IsMaterialIsolatedEnabled, SlotIndex)
			.OnCheckStateChanged(this, &FMeshMaterialsLayout::OnMaterialIsolatedChanged, SlotIndex)
			.ToolTipText(LOCTEXT("Isolate_CustomMaterialName_ToolTip", "Isolates this material in the viewport"))
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.ColorAndOpacity(FLinearColor(0.4f, 0.4f, 0.4f, 1.0f))
				.Text(LOCTEXT("Isolate", "Isolate"))
			]
		];
}

ECheckBoxState FMeshMaterialsLayout::IsMaterialHighlighted(int32 SlotIndex) const
{
	ECheckBoxState State = ECheckBoxState::Unchecked;
	UStaticMeshComponent* Component = StaticMeshEditor.GetStaticMeshComponent();
	if (Component)
	{
		State = Component->SelectedEditorMaterial == SlotIndex ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}
	return State;
}

void FMeshMaterialsLayout::OnMaterialHighlightedChanged(ECheckBoxState NewState, int32 SlotIndex)
{
	UStaticMeshComponent* Component = StaticMeshEditor.GetStaticMeshComponent();
	if (Component)
	{
		if (NewState == ECheckBoxState::Checked)
		{
			Component->SelectedEditorMaterial = SlotIndex;
			if (Component->MaterialIndexPreview != SlotIndex)
			{
				Component->SetMaterialPreview(INDEX_NONE);
			}
			Component->SetSectionPreview(INDEX_NONE);
			Component->SelectedEditorSection = INDEX_NONE;
		}
		else if (NewState == ECheckBoxState::Unchecked)
		{
			Component->SelectedEditorMaterial = INDEX_NONE;
		}
		Component->MarkRenderStateDirty();
		StaticMeshEditor.RefreshViewport();
	}
}

ECheckBoxState FMeshMaterialsLayout::IsMaterialIsolatedEnabled(int32 SlotIndex) const
{
	ECheckBoxState State = ECheckBoxState::Unchecked;
	const UStaticMeshComponent* Component = StaticMeshEditor.GetStaticMeshComponent();
	if (Component)
	{
		State = Component->MaterialIndexPreview == SlotIndex ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}
	return State;
}

void FMeshMaterialsLayout::OnMaterialIsolatedChanged(ECheckBoxState NewState, int32 SlotIndex)
{
	UStaticMeshComponent* Component = StaticMeshEditor.GetStaticMeshComponent();
	if (Component)
	{
		if (NewState == ECheckBoxState::Checked)
		{
			Component->SetMaterialPreview(SlotIndex);
			if (Component->SelectedEditorMaterial != SlotIndex)
			{
				Component->SelectedEditorMaterial = INDEX_NONE;
			}
			Component->SetSectionPreview(INDEX_NONE);
			Component->SelectedEditorSection = INDEX_NONE;
		}
		else if (NewState == ECheckBoxState::Unchecked)
		{
			Component->SetMaterialPreview(INDEX_NONE);
		}
		Component->MarkRenderStateDirty();
		StaticMeshEditor.RefreshViewport();
	}
}

void FMeshMaterialsLayout::OnResetMaterialToDefaultClicked(UMaterialInterface* Material, int32 MaterialIndex)
{
	UStaticMesh& StaticMesh = GetStaticMesh();
	check(StaticMesh.StaticMaterials.IsValidIndex(MaterialIndex));
	StaticMesh.StaticMaterials[MaterialIndex].MaterialInterface = UMaterial::GetDefaultMaterial(MD_Surface);
	CallPostEditChange();
}

FText FMeshMaterialsLayout::GetOriginalImportMaterialNameText(int32 MaterialIndex) const
{
	UStaticMesh& StaticMesh = GetStaticMesh();
	if (StaticMesh.StaticMaterials.IsValidIndex(MaterialIndex))
	{
		FString OriginalImportMaterialName;
		StaticMesh.StaticMaterials[MaterialIndex].ImportedMaterialSlotName.ToString(OriginalImportMaterialName);
		OriginalImportMaterialName = TEXT("Original Imported Material Name: ") + OriginalImportMaterialName;
		return FText::FromString(OriginalImportMaterialName);
	}
	return FText::FromName(NAME_None);
}

FText FMeshMaterialsLayout::GetMaterialNameText(int32 MaterialIndex) const
{
	UStaticMesh& StaticMesh = GetStaticMesh();
	if (StaticMesh.StaticMaterials.IsValidIndex(MaterialIndex))
	{
		return FText::FromName(StaticMesh.StaticMaterials[MaterialIndex].MaterialSlotName);
	}
	return FText::FromName(NAME_None);
}

void FMeshMaterialsLayout::OnMaterialNameCommitted(const FText& InValue, ETextCommit::Type CommitType, int32 MaterialIndex)
{
	UStaticMesh& StaticMesh = GetStaticMesh();
	FName InValueName = FName(*(InValue.ToString()));
	if (StaticMesh.StaticMaterials.IsValidIndex(MaterialIndex) && StaticMesh.StaticMaterials[MaterialIndex].MaterialSlotName != InValueName)
	{
		FScopedTransaction ScopeTransaction(LOCTEXT("StaticMeshEditorMaterialSlotNameChanged", "Staticmesh editor: Material slot name change"));

		UProperty* ChangedProperty = NULL;
		ChangedProperty = FindField<UProperty>(UStaticMesh::StaticClass(), "StaticMaterials");
		check(ChangedProperty);
		StaticMesh.PreEditChange(ChangedProperty);

		StaticMesh.StaticMaterials[MaterialIndex].MaterialSlotName = InValueName;
		
		FPropertyChangedEvent PropertyUpdateStruct(ChangedProperty);
		StaticMesh.PostEditChangeProperty(PropertyUpdateStruct);
	}
}

bool FMeshMaterialsLayout::CanDeleteMaterialSlot(int32 MaterialIndex) const
{
	UStaticMesh& StaticMesh = GetStaticMesh();
	return (MaterialIndex + 1) == StaticMesh.StaticMaterials.Num();
}

void FMeshMaterialsLayout::OnDeleteMaterialSlot(int32 MaterialIndex)
{
	UStaticMesh& StaticMesh = GetStaticMesh();
	if (CanDeleteMaterialSlot(MaterialIndex))
	{
		FScopedTransaction Transaction(LOCTEXT("StaticMeshEditorDeletedMaterialSlot", "Staticmesh editor: Deleted material slot"));

		StaticMesh.Modify();
		StaticMesh.StaticMaterials.RemoveAt(MaterialIndex);

		StaticMesh.PostEditChange();
	}

}

TSharedRef<SWidget> FMeshMaterialsLayout::OnGetMaterialSlotUsedByMenuContent(int32 MaterialIndex)
{
	UStaticMesh& StaticMesh = GetStaticMesh();
	FMenuBuilder MenuBuilder(true, NULL);
	TArray<FSectionLocalizer> *SectionLocalizers;
	if (MaterialUsedMap.Contains(MaterialIndex))
	{
		SectionLocalizers = MaterialUsedMap.Find(MaterialIndex);
		FUIAction Action;
		FText EmptyTooltip;
		// Add a menu item for each texture.  Clicking on the texture will display it in the content browser
		for (const FSectionLocalizer& SectionUsingMaterial : (*SectionLocalizers))
		{
			FString ArrayItemName = TEXT("Lod ") + FString::FromInt(SectionUsingMaterial.LODIndex) + TEXT("  Index ") + FString::FromInt(SectionUsingMaterial.SectionIndex);
			MenuBuilder.AddMenuEntry(FText::FromString(ArrayItemName), EmptyTooltip, FSlateIcon(), Action);
		}
	}
	return MenuBuilder.MakeWidget();
}

FText FMeshMaterialsLayout::GetFirstMaterialSlotUsedBySection(int32 MaterialIndex) const
{
	UStaticMesh& StaticMesh = GetStaticMesh();
	if (MaterialUsedMap.Contains(MaterialIndex))
	{
		const TArray<FSectionLocalizer> *SectionLocalizers = MaterialUsedMap.Find(MaterialIndex);
		if (SectionLocalizers->Num() > 0)
		{
			FString ArrayItemName = FString::FromInt(SectionLocalizers->Num()) + TEXT(" Sections");
			return FText::FromString(ArrayItemName);
		}
	}
	return FText();
}

bool FMeshMaterialsLayout::OnMaterialListDirty()
{
	UStaticMesh& StaticMesh = GetStaticMesh();
	bool ForceMaterialListRefresh = false;
	TMap<int32, TArray<FSectionLocalizer>> TempMaterialUsedMap;
	for (int32 MaterialIndex = 0; MaterialIndex < StaticMesh.StaticMaterials.Num(); ++MaterialIndex)
	{
		TArray<FSectionLocalizer> SectionLocalizers;
		for (int32 LODIndex = 0; LODIndex < StaticMesh.GetNumLODs(); ++LODIndex)
		{
			for (int32 SectionIndex = 0; SectionIndex < StaticMesh.GetNumSections(LODIndex); ++SectionIndex)
			{
				FMeshSectionInfo Info = StaticMesh.SectionInfoMap.Get(LODIndex, SectionIndex);

				if (Info.MaterialIndex == MaterialIndex)
				{
					SectionLocalizers.Add(FSectionLocalizer(LODIndex, SectionIndex));
				}
			}
		}
		TempMaterialUsedMap.Add(MaterialIndex, SectionLocalizers);
	}
	if (TempMaterialUsedMap.Num() != MaterialUsedMap.Num())
	{
		ForceMaterialListRefresh = true;
	}
	else if (!ForceMaterialListRefresh)
	{
		for (auto KvpOld : MaterialUsedMap)
		{
			if (!TempMaterialUsedMap.Contains(KvpOld.Key))
			{
				ForceMaterialListRefresh = true;
				break;
			}
			const TArray<FSectionLocalizer> &TempSectionLocalizers = (*(TempMaterialUsedMap.Find(KvpOld.Key)));
			const TArray<FSectionLocalizer> &OldSectionLocalizers = KvpOld.Value;
			if (TempSectionLocalizers.Num() != OldSectionLocalizers.Num())
			{
				ForceMaterialListRefresh = true;
				break;
			}
			for (int32 SectionLocalizerIndex = 0; SectionLocalizerIndex < OldSectionLocalizers.Num(); ++SectionLocalizerIndex)
			{
				if (OldSectionLocalizers[SectionLocalizerIndex] != TempSectionLocalizers[SectionLocalizerIndex])
				{
					ForceMaterialListRefresh = true;
					break;
				}
			}
			if (ForceMaterialListRefresh)
			{
				break;
			}
		}
	}
	MaterialUsedMap = TempMaterialUsedMap;

	return ForceMaterialListRefresh;
}

ECheckBoxState FMeshMaterialsLayout::IsShadowCastingEnabled(int32 SlotIndex) const
{
	bool FirstEvalDone = false;
	bool ShadowCastingValue = false;
	UStaticMesh& StaticMesh = GetStaticMesh();
	for (int32 LODIndex = 0; LODIndex < StaticMesh.GetNumLODs(); ++LODIndex)
	{
		for (int32 SectionIndex = 0; SectionIndex < StaticMesh.GetNumSections(LODIndex); ++SectionIndex)
		{
			FMeshSectionInfo Info = StaticMesh.SectionInfoMap.Get(LODIndex, SectionIndex);
			if (Info.MaterialIndex == SlotIndex)
			{
				if (!FirstEvalDone)
				{
					ShadowCastingValue = Info.bCastShadow;
					FirstEvalDone = true;
				}
				else if (ShadowCastingValue != Info.bCastShadow)
				{
					return ECheckBoxState::Undetermined;
				}
			}
		}
	}
	if (FirstEvalDone)
	{
		return ShadowCastingValue ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}
	return ECheckBoxState::Undetermined;
}

void FMeshMaterialsLayout::OnShadowCastingChanged(ECheckBoxState NewState, int32 SlotIndex)
{
	UStaticMesh& StaticMesh = GetStaticMesh();
	
	if (NewState == ECheckBoxState::Undetermined)
		return;
	
	bool CastShadow = (NewState == ECheckBoxState::Checked) ? true : false;
	bool SomethingChange = false;
	for (int32 LODIndex = 0; LODIndex < StaticMesh.GetNumLODs(); ++LODIndex)
	{
		for (int32 SectionIndex = 0; SectionIndex < StaticMesh.GetNumSections(LODIndex); ++SectionIndex)
		{
			FMeshSectionInfo Info = StaticMesh.SectionInfoMap.Get(LODIndex, SectionIndex);
			if (Info.MaterialIndex == SlotIndex)
			{
				Info.bCastShadow = CastShadow;
				StaticMesh.SectionInfoMap.Set(LODIndex, SectionIndex, Info);
				SomethingChange = true;
			}
		}
	}

	if (SomethingChange)
	{
		CallPostEditChange();
	}
}

EVisibility FMeshMaterialsLayout::GetOverrideUVDensityVisibililty() const
{
	if (StaticMeshEditor.GetViewMode() == VMI_MeshUVDensityAccuracy)
	{
		return EVisibility::SelfHitTestInvisible;
	}
	else
	{
		return EVisibility::Collapsed;
	}
}

ECheckBoxState FMeshMaterialsLayout::IsUVDensityOverridden(int32 SlotIndex) const
{
	const UStaticMesh& StaticMesh = GetStaticMesh();
	if (!StaticMesh.StaticMaterials.IsValidIndex(SlotIndex))
	{
		return ECheckBoxState::Undetermined;
	}
	else if (StaticMesh.StaticMaterials[SlotIndex].UVChannelData.bOverrideDensities)
	{
		return ECheckBoxState::Checked;
	}
	else
	{
		return ECheckBoxState::Unchecked;
	}
}


void FMeshMaterialsLayout::OnOverrideUVDensityChanged(ECheckBoxState NewState, int32 SlotIndex)
{
	UStaticMesh& StaticMesh = GetStaticMesh();
	if (NewState != ECheckBoxState::Undetermined && StaticMesh.StaticMaterials.IsValidIndex(SlotIndex))
	{
		StaticMesh.StaticMaterials[SlotIndex].UVChannelData.bOverrideDensities = (NewState == ECheckBoxState::Checked);
		StaticMesh.UpdateUVChannelData(true);
	}
}

EVisibility FMeshMaterialsLayout::GetUVDensityVisibility(int32 SlotIndex, int32 UVChannelIndex) const
{
	UStaticMesh& StaticMesh = GetStaticMesh();
	if (StaticMeshEditor.GetViewMode() == VMI_MeshUVDensityAccuracy && IsUVDensityOverridden(SlotIndex) == ECheckBoxState::Checked && UVChannelIndex < StaticMeshEditor.GetNumUVChannels())
	{
		return EVisibility::SelfHitTestInvisible;
	}
	else
	{
		return EVisibility::Collapsed;
	}
}

TOptional<float> FMeshMaterialsLayout::GetUVDensityValue(int32 SlotIndex, int32 UVChannelIndex) const
{
	const UStaticMesh& StaticMesh = GetStaticMesh();
	if (StaticMesh.StaticMaterials.IsValidIndex(SlotIndex))
	{
		float Value = StaticMesh.StaticMaterials[SlotIndex].UVChannelData.LocalUVDensities[UVChannelIndex];
		return FMath::RoundToFloat(Value * 4.f) * .25f;
	}
	return TOptional<float>();
}

void FMeshMaterialsLayout::SetUVDensityValue(float InDensity, ETextCommit::Type CommitType, int32 SlotIndex, int32 UVChannelIndex)
{
	UStaticMesh& StaticMesh = GetStaticMesh();
	if (StaticMesh.StaticMaterials.IsValidIndex(SlotIndex))
	{
		StaticMesh.StaticMaterials[SlotIndex].UVChannelData.LocalUVDensities[UVChannelIndex] = FMath::Max<float>(0, InDensity);
		StaticMesh.UpdateUVChannelData(true);
	}
}

void FMeshMaterialsLayout::CallPostEditChange(UProperty* PropertyChanged/*=nullptr*/)
{
	UStaticMesh& StaticMesh = GetStaticMesh();
	if (PropertyChanged)
	{
		FPropertyChangedEvent PropertyUpdateStruct(PropertyChanged);
		StaticMesh.PostEditChangeProperty(PropertyUpdateStruct);
	}
	else
	{
		StaticMesh.Modify();
		StaticMesh.PostEditChange();
	}
	if (StaticMesh.BodySetup)
	{
		StaticMesh.BodySetup->CreatePhysicsMeshes();
	}
	StaticMeshEditor.RefreshViewport();
}


/////////////////////////////////
// FLevelOfDetailSettingsLayout
/////////////////////////////////

FLevelOfDetailSettingsLayout::FLevelOfDetailSettingsLayout( FStaticMeshEditor& InStaticMeshEditor )
	: StaticMeshEditor( InStaticMeshEditor )
{
	LODGroupNames.Reset();
	UStaticMesh::GetLODGroups(LODGroupNames);
	for (int32 GroupIndex = 0; GroupIndex < LODGroupNames.Num(); ++GroupIndex)
	{
		LODGroupOptions.Add(MakeShareable(new FString(LODGroupNames[GroupIndex].GetPlainNameString())));
	}

	for (int32 i = 0; i < MAX_STATIC_MESH_LODS; ++i)
	{
		bBuildSettingsExpanded[i] = false;
		bReductionSettingsExpanded[i] = false;
		bSectionSettingsExpanded[i] = (i == 0);

		LODScreenSizes[i] = 0.0f;
	}

	LODCount = StaticMeshEditor.GetStaticMesh()->GetNumLODs();

	UpdateLODNames();
}

/** Returns true if automatic mesh reduction is available. */
static bool IsAutoMeshReductionAvailable()
{
	bool bAutoMeshReductionAvailable = FModuleManager::Get().LoadModuleChecked<IMeshReductionManagerModule>("MeshReductionInterface").GetStaticMeshReductionInterface() != NULL;
	return bAutoMeshReductionAvailable;
}

void FLevelOfDetailSettingsLayout::AddToDetailsPanel( IDetailLayoutBuilder& DetailBuilder )
{
	UStaticMesh* StaticMesh = StaticMeshEditor.GetStaticMesh();

	IDetailCategoryBuilder& LODSettingsCategory =
		DetailBuilder.EditCategory( "LodSettings", LOCTEXT("LodSettingsCategory", "LOD Settings") );

	int32 LODGroupIndex = LODGroupNames.Find(StaticMesh->LODGroup);
	check(LODGroupIndex == INDEX_NONE || LODGroupIndex < LODGroupOptions.Num());


	IDetailPropertyRow& LODGroupRow = LODSettingsCategory.AddProperty(GET_MEMBER_NAME_CHECKED(UStaticMesh, LODGroup));

	LODGroupRow.CustomWidget()
	.NameContent()
	[
		SNew(STextBlock)
		.Font( IDetailLayoutBuilder::GetDetailFont() )
		.Text(LOCTEXT("LODGroup", "LOD Group"))
	]
	.ValueContent()
	[
		SAssignNew(LODGroupComboBox, STextComboBox)
		.Font(IDetailLayoutBuilder::GetDetailFont())
		.OptionsSource(&LODGroupOptions)
		.InitiallySelectedItem(LODGroupOptions[(LODGroupIndex == INDEX_NONE) ? 0 : LODGroupIndex])
		.OnSelectionChanged(this, &FLevelOfDetailSettingsLayout::OnLODGroupChanged)
	];
	
	LODSettingsCategory.AddCustomRow( LOCTEXT("LODImport", "LOD Import") )
		.NameContent()
		[
			SNew(STextBlock)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text(LOCTEXT("LODImport", "LOD Import"))
		]
	.ValueContent()
		[
			SNew(STextComboBox)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.OptionsSource(&LODNames)
			.InitiallySelectedItem(LODNames[0])
			.OnSelectionChanged(this, &FLevelOfDetailSettingsLayout::OnImportLOD)
		];

	LODSettingsCategory.AddCustomRow( LOCTEXT("MinLOD", "Minimum LOD") )
	.NameContent()
	[
		SNew(STextBlock)
		.Font( IDetailLayoutBuilder::GetDetailFont() )
		.Text(LOCTEXT("MinLOD", "Minimum LOD"))
	]
	.ValueContent()
	[
		SNew(SSpinBox<int32>)
		.Font( IDetailLayoutBuilder::GetDetailFont() )
		.Value(this, &FLevelOfDetailSettingsLayout::GetMinLOD)
		.OnValueChanged(this, &FLevelOfDetailSettingsLayout::OnMinLODChanged)
		.OnValueCommitted(this, &FLevelOfDetailSettingsLayout::OnMinLODCommitted)
		.MinValue(0)
		.MaxValue(MAX_STATIC_MESH_LODS)
		.ToolTipText(this, &FLevelOfDetailSettingsLayout::GetMinLODTooltip)
		.IsEnabled(FLevelOfDetailSettingsLayout::GetLODCount() > 1)
	];

	// Add Number of LODs slider.
	const int32 MinAllowedLOD = 1;
	LODSettingsCategory.AddCustomRow( LOCTEXT("NumberOfLODs", "Number of LODs") )
	.NameContent()
	[
		SNew(STextBlock)
		.Font( IDetailLayoutBuilder::GetDetailFont() )
		.Text(LOCTEXT("NumberOfLODs", "Number of LODs"))
	]
	.ValueContent()
	[
		SNew(SSpinBox<int32>)
		.Font( IDetailLayoutBuilder::GetDetailFont() )
		.Value(this, &FLevelOfDetailSettingsLayout::GetLODCount)
		.OnValueChanged(this, &FLevelOfDetailSettingsLayout::OnLODCountChanged)
		.OnValueCommitted(this, &FLevelOfDetailSettingsLayout::OnLODCountCommitted)
		.MinValue(MinAllowedLOD)
		.MaxValue(MAX_STATIC_MESH_LODS)
		.ToolTipText(this, &FLevelOfDetailSettingsLayout::GetLODCountTooltip)
		.IsEnabled(IsAutoMeshReductionAvailable())
	];

	// Auto LOD distance check box.
	LODSettingsCategory.AddCustomRow( LOCTEXT("AutoComputeLOD", "Auto Compute LOD Distances") )
	.NameContent()
	[
		SNew(STextBlock)
		.Font( IDetailLayoutBuilder::GetDetailFont() )
		.Text(LOCTEXT("AutoComputeLOD", "Auto Compute LOD Distances"))
	]
	.ValueContent()
	[
		SNew(SCheckBox)
		.IsChecked(this, &FLevelOfDetailSettingsLayout::IsAutoLODChecked)
		.OnCheckStateChanged(this, &FLevelOfDetailSettingsLayout::OnAutoLODChanged)
	];

	LODSettingsCategory.AddCustomRow( LOCTEXT("ApplyChanges", "Apply Changes") )
	.ValueContent()
	.HAlign(HAlign_Left)
	[
		SNew(SButton)
		.OnClicked(this, &FLevelOfDetailSettingsLayout::OnApply)
		.IsEnabled(this, &FLevelOfDetailSettingsLayout::IsApplyNeeded)
		[
			SNew( STextBlock )
			.Text(LOCTEXT("ApplyChanges", "Apply Changes"))
			.Font( DetailBuilder.GetDetailFont() )
		]
	];

	AddLODLevelCategories( DetailBuilder );
}

bool FLevelOfDetailSettingsLayout::CanRemoveLOD(int32 LODIndex) const
{
	UStaticMesh* StaticMesh = StaticMeshEditor.GetStaticMesh();

	if (StaticMesh != nullptr)
	{
		const int32 NumLODs = StaticMesh->GetNumLODs();
		
		// LOD0 should never be removed
		return (NumLODs > 1 && LODIndex > 0 && LODIndex < NumLODs);
	}

	return false;
}

FReply FLevelOfDetailSettingsLayout::OnRemoveLOD(int32 LODIndex)
{
	UStaticMesh* StaticMesh = StaticMeshEditor.GetStaticMesh();

	if (StaticMesh != nullptr)
	{
		const int32 NumLODs = StaticMesh->GetNumLODs();

		if (NumLODs > 1 && LODIndex > 0 && LODIndex < NumLODs)
		{
			FText RemoveLODText = FText::Format( LOCTEXT("ConfirmRemoveLOD", "Are you sure you want to remove LOD {0} from {1}?"), LODIndex, FText::FromString(StaticMesh->GetName()) );

			if (FMessageDialog::Open(EAppMsgType::YesNo, RemoveLODText) == EAppReturnType::Yes)
			{
				FText TransactionDescription = FText::Format( LOCTEXT("OnRemoveLOD", "Staticmesh editor: Remove LOD {0}"), LODIndex);
				FScopedTransaction Transaction( TEXT(""), TransactionDescription, StaticMesh );

				StaticMesh->Modify();
				StaticMesh->SourceModels.RemoveAt(LODIndex);
				--LODCount;
				StaticMesh->PostEditChange();

				StaticMeshEditor.RefreshTool();
			}
		}
	}

	return FReply::Handled();
}

void FLevelOfDetailSettingsLayout::AddLODLevelCategories( IDetailLayoutBuilder& DetailBuilder )
{
	UStaticMesh* StaticMesh = StaticMeshEditor.GetStaticMesh();
	
	if( StaticMesh )
	{
		const int32 StaticMeshLODCount = StaticMesh->GetNumLODs();
		FStaticMeshRenderData* RenderData = StaticMesh->RenderData.Get();

		//Add the Materials array
		{
			FString CategoryName = FString(TEXT("StaticMeshMaterials"));

			IDetailCategoryBuilder& MaterialsCategory = DetailBuilder.EditCategory(*CategoryName, LOCTEXT("StaticMeshMaterialsLabel", "Material Slots"), ECategoryPriority::Important);

			MaterialsLayoutWidget = MakeShareable(new FMeshMaterialsLayout(StaticMeshEditor));
			MaterialsLayoutWidget->AddToCategory(MaterialsCategory);
		}

		int32 CurrentLodIndex = 0;
		
		if (StaticMeshEditor.GetStaticMeshComponent() != nullptr)
		{
			CurrentLodIndex = StaticMeshEditor.GetStaticMeshComponent()->ForcedLodModel;
		}
		LodCategories.Empty(StaticMeshLODCount);

		FString LODControllerCategoryName = FString(TEXT("LODCustomMode"));
		FText LODControllerString = LOCTEXT("LODCustomModeCategoryName", "LOD Picker");

		IDetailCategoryBuilder& LODCustomModeCategory = DetailBuilder.EditCategory( *LODControllerCategoryName, LODControllerString, ECategoryPriority::Important );


		LODCustomModeCategory.AddCustomRow((LOCTEXT("LODCustomModeFirstRowName", "LODCustomMode")))
		.NameContent()
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(this, &FLevelOfDetailSettingsLayout::GetLODCustomModeNameContent, (int32)INDEX_NONE)
			.ToolTipText(LOCTEXT("LODCustomModeFirstRowTooltip", "Custom Mode allow editing multiple LOD in same time."))
		]
		.ValueContent()
		[
			SNew(SCheckBox)
			.IsChecked(this, &FLevelOfDetailSettingsLayout::IsLODCustomModeCheck, (int32)INDEX_NONE)
			.OnCheckStateChanged(this, &FLevelOfDetailSettingsLayout::SetLODCustomModeCheck, (int32)INDEX_NONE)
			.ToolTipText(LOCTEXT("LODCustomModeFirstRowTooltip", "Custom Mode allow editing multiple LOD in same time."))
		];
		//Set the custom mode to false
		CustomLODEditMode = false;
		// Create information panel for each LOD level.
		for(int32 LODIndex = 0; LODIndex < StaticMeshLODCount; ++LODIndex)
		{
			//Show the viewport LOD at start
			bool IsViewportLOD = (CurrentLodIndex == 0 ? 0 : CurrentLodIndex - 1) == LODIndex;
			DetailDisplayLODs[LODIndex] = true; //enable all LOD in custom mode
			LODCustomModeCategory.AddCustomRow((LOCTEXT("LODCustomModeRowName", "LODCheckBoxRowName")))
			.NameContent()
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.Text(this, &FLevelOfDetailSettingsLayout::GetLODCustomModeNameContent, LODIndex)
				.IsEnabled(this, &FLevelOfDetailSettingsLayout::IsLODCustomModeEnable, LODIndex)
			]
			.ValueContent()
			[
				SNew(SCheckBox)
				.IsChecked(this, &FLevelOfDetailSettingsLayout::IsLODCustomModeCheck, LODIndex)
				.OnCheckStateChanged(this, &FLevelOfDetailSettingsLayout::SetLODCustomModeCheck, LODIndex)
				.IsEnabled(this, &FLevelOfDetailSettingsLayout::IsLODCustomModeEnable, LODIndex)
			];

			if (IsAutoMeshReductionAvailable())
			{
				ReductionSettingsWidgets[LODIndex] = MakeShareable( new FMeshReductionSettingsLayout( AsShared() ) );
			}

			if (LODIndex < StaticMesh->SourceModels.Num())
			{
				FStaticMeshSourceModel& SrcModel = StaticMesh->SourceModels[LODIndex];
				if (ReductionSettingsWidgets[LODIndex].IsValid())
				{
					ReductionSettingsWidgets[LODIndex]->UpdateSettings(SrcModel.ReductionSettings);
				}

				if (SrcModel.RawMeshBulkData->IsEmpty() == false)
				{
					BuildSettingsWidgets[LODIndex] = MakeShareable( new FMeshBuildSettingsLayout( AsShared() ) );
					BuildSettingsWidgets[LODIndex]->UpdateSettings(SrcModel.BuildSettings);
				}

				LODScreenSizes[LODIndex] = SrcModel.ScreenSize;
			}
			else if (LODIndex > 0)
			{
				if (ReductionSettingsWidgets[LODIndex].IsValid() && ReductionSettingsWidgets[LODIndex-1].IsValid())
				{
					FMeshReductionSettings ReductionSettings = ReductionSettingsWidgets[LODIndex-1]->GetSettings();
					// By default create LODs with half the triangles of the previous LOD.
					ReductionSettings.PercentTriangles *= 0.5f;
					ReductionSettingsWidgets[LODIndex]->UpdateSettings(ReductionSettings);
				}

				if(LODScreenSizes[LODIndex] >= LODScreenSizes[LODIndex-1])
				{
					const float DefaultScreenSizeDifference = 0.01f;
					LODScreenSizes[LODIndex] = LODScreenSizes[LODIndex-1] - DefaultScreenSizeDifference;
				}
			}

			FString CategoryName = FString(TEXT("LOD"));
			CategoryName.AppendInt( LODIndex );

			FText LODLevelString = FText::FromString(FString(TEXT("LOD ")) + FString::FromInt(LODIndex) );

			IDetailCategoryBuilder& LODCategory = DetailBuilder.EditCategory( *CategoryName, LODLevelString, ECategoryPriority::Important );
			LodCategories.Add(&LODCategory);

			LODCategory.HeaderContent
			(
				SNew( SBox )
				.HAlign( HAlign_Right )
				[
					SNew( SHorizontalBox )
					+ SHorizontalBox::Slot()
					.Padding(FMargin(5.0f, 0.0f))
					.AutoWidth()
					[
						SNew(STextBlock)
						.Font(FEditorStyle::GetFontStyle("StaticMeshEditor.NormalFont"))
						.Text(this, &FLevelOfDetailSettingsLayout::GetLODScreenSizeTitle, LODIndex)
						.Visibility( LODIndex > 0 ? EVisibility::Visible : EVisibility::Collapsed )
					]
					+ SHorizontalBox::Slot()
					.Padding( FMargin( 5.0f, 0.0f ) )
					.AutoWidth()
					[
						SNew(STextBlock)
						.Font(FEditorStyle::GetFontStyle("StaticMeshEditor.NormalFont"))
						.Text( FText::Format( LOCTEXT("Triangles_MeshSimplification", "Triangles: {0}"), FText::AsNumber( StaticMeshEditor.GetNumTriangles(LODIndex) ) ) )
					]
					+ SHorizontalBox::Slot()
					.Padding( FMargin( 5.0f, 0.0f ) )
					.AutoWidth()
					[
						SNew(STextBlock)
						.Font(FEditorStyle::GetFontStyle("StaticMeshEditor.NormalFont"))
						.Text( FText::Format( LOCTEXT("Vertices_MeshSimplification", "Vertices: {0}"), FText::AsNumber( StaticMeshEditor.GetNumVertices(LODIndex) ) ) )
					]
				]
			);
			
			
			SectionSettingsWidgets[ LODIndex ] = MakeShareable( new FMeshSectionSettingsLayout( StaticMeshEditor, LODIndex, LodCategories, &CustomLODEditMode) );
			SectionSettingsWidgets[ LODIndex ]->AddToCategory( LODCategory );

			LODCategory.AddCustomRow(( LOCTEXT("ScreenSizeRow", "ScreenSize")))
			.NameContent()
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.Text(LOCTEXT("ScreenSizeName", "Screen Size"))
			]
			.ValueContent()
			[
				SNew(SSpinBox<float>)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.MinValue(0.0f)
				.MaxValue(WORLD_MAX)
				.SliderExponent(2.0f)
				.Value(this, &FLevelOfDetailSettingsLayout::GetLODScreenSize, LODIndex)
				.OnValueChanged(this, &FLevelOfDetailSettingsLayout::OnLODScreenSizeChanged, LODIndex)
				.OnValueCommitted(this, &FLevelOfDetailSettingsLayout::OnLODScreenSizeCommitted, LODIndex)
				.IsEnabled(this, &FLevelOfDetailSettingsLayout::CanChangeLODScreenSize)
			];

			if (BuildSettingsWidgets[LODIndex].IsValid())
			{
				LODCategory.AddCustomBuilder( BuildSettingsWidgets[LODIndex].ToSharedRef() );
			}

			if( ReductionSettingsWidgets[LODIndex].IsValid() )
			{
				LODCategory.AddCustomBuilder( ReductionSettingsWidgets[LODIndex].ToSharedRef() );
			}

			if (LODIndex != 0)
			{
				LODCategory.AddCustomRow( LOCTEXT("RemoveLOD", "Remove LOD") )
				.ValueContent()
				.HAlign(HAlign_Left)
				[
					SNew(SButton)
					.OnClicked(this, &FLevelOfDetailSettingsLayout::OnRemoveLOD, LODIndex)
					.IsEnabled(this, &FLevelOfDetailSettingsLayout::CanRemoveLOD, LODIndex)
					.ToolTipText( LOCTEXT("RemoveLOD_ToolTip", "Removes this LOD from the Static Mesh") )
					[
						SNew(STextBlock)
						.Text( LOCTEXT("RemoveLOD", "Remove LOD") )
						.Font( DetailBuilder.GetDetailFont() )
					]
				];
			}
			LODCategory.SetCategoryVisibility(IsViewportLOD);
		}

		//Show the LOD custom category 
		LODCustomModeCategory.SetCategoryVisibility(StaticMeshLODCount > 1);
	}
}


FLevelOfDetailSettingsLayout::~FLevelOfDetailSettingsLayout()
{
}

int32 FLevelOfDetailSettingsLayout::GetLODCount() const
{
	return LODCount;
}

float FLevelOfDetailSettingsLayout::GetLODScreenSize( int32 LODIndex ) const
{
	check(LODIndex < MAX_STATIC_MESH_LODS);
	UStaticMesh* Mesh = StaticMeshEditor.GetStaticMesh();
	float ScreenSize = LODScreenSizes[FMath::Clamp(LODIndex, 0, MAX_STATIC_MESH_LODS-1)];
	if(Mesh->bAutoComputeLODScreenSize)
	{
		ScreenSize = Mesh->RenderData->ScreenSize[LODIndex];
	}
	else if(Mesh->SourceModels.IsValidIndex(LODIndex))
	{
		ScreenSize = Mesh->SourceModels[LODIndex].ScreenSize;
	}
	return ScreenSize;
}

FText FLevelOfDetailSettingsLayout::GetLODScreenSizeTitle( int32 LODIndex ) const
{
	return FText::Format( LOCTEXT("ScreenSize_MeshSimplification", "Screen Size: {0}"), FText::AsNumber(GetLODScreenSize(LODIndex)));
}

bool FLevelOfDetailSettingsLayout::CanChangeLODScreenSize() const
{
	return !IsAutoLODEnabled();
}

void FLevelOfDetailSettingsLayout::OnLODScreenSizeChanged( float NewValue, int32 LODIndex )
{
	check(LODIndex < MAX_STATIC_MESH_LODS);
	UStaticMesh* StaticMesh = StaticMeshEditor.GetStaticMesh();
	if (!StaticMesh->bAutoComputeLODScreenSize)
	{
		// First propagate any changes from the source models to our local scratch.
		for (int32 i = 0; i < StaticMesh->SourceModels.Num(); ++i)
		{
			LODScreenSizes[i] = StaticMesh->SourceModels[i].ScreenSize;
		}

		// Update Display factors for further LODs
		const float MinimumDifferenceInScreenSize = KINDA_SMALL_NUMBER;
		LODScreenSizes[LODIndex] = NewValue;
		// Make sure we aren't trying to ovelap or have more than one LOD for a value
		for (int32 i = 1; i < MAX_STATIC_MESH_LODS; ++i)
		{
			float MaxValue = FMath::Max(LODScreenSizes[i-1] - MinimumDifferenceInScreenSize, 0.0f);
			LODScreenSizes[i] = FMath::Min(LODScreenSizes[i], MaxValue);
		}

		// Push changes immediately.
		for (int32 i = 0; i < MAX_STATIC_MESH_LODS; ++i)
		{
			if (StaticMesh->SourceModels.IsValidIndex(i))
			{
				StaticMesh->SourceModels[i].ScreenSize = LODScreenSizes[i];
			}
			if (StaticMesh->RenderData
				&& StaticMesh->RenderData->LODResources.IsValidIndex(i))
			{
				StaticMesh->RenderData->ScreenSize[i] = LODScreenSizes[i];
			}
		}

		// Reregister static mesh components using this mesh.
		{
			FStaticMeshComponentRecreateRenderStateContext ReregisterContext(StaticMesh,false);
			StaticMesh->Modify();
		}

		StaticMeshEditor.RefreshViewport();
	}
}

void FLevelOfDetailSettingsLayout::OnLODScreenSizeCommitted( float NewValue, ETextCommit::Type CommitType, int32 LODIndex )
{
	OnLODScreenSizeChanged(NewValue, LODIndex);
}

void FLevelOfDetailSettingsLayout::UpdateLODNames()
{
	LODNames.Empty();
	LODNames.Add( MakeShareable( new FString( LOCTEXT("BaseLOD", "Base LOD").ToString() ) ) );
	for(int32 LODLevelID = 1; LODLevelID < LODCount; ++LODLevelID)
	{
		LODNames.Add( MakeShareable( new FString( FText::Format( NSLOCTEXT("LODSettingsLayout", "LODLevel_Reimport", "Reimport LOD Level {0}"), FText::AsNumber( LODLevelID ) ).ToString() ) ) );
	}
	LODNames.Add( MakeShareable( new FString( FText::Format( NSLOCTEXT("LODSettingsLayout", "LODLevel_Import", "Import LOD Level {0}"), FText::AsNumber( LODCount ) ).ToString() ) ) );
}

void FLevelOfDetailSettingsLayout::OnBuildSettingsExpanded(bool bIsExpanded, int32 LODIndex)
{
	check(LODIndex >= 0 && LODIndex < MAX_STATIC_MESH_LODS);
	bBuildSettingsExpanded[LODIndex] = bIsExpanded;
}

void FLevelOfDetailSettingsLayout::OnReductionSettingsExpanded(bool bIsExpanded, int32 LODIndex)
{
	check(LODIndex >= 0 && LODIndex < MAX_STATIC_MESH_LODS);
	bReductionSettingsExpanded[LODIndex] = bIsExpanded;
}

void FLevelOfDetailSettingsLayout::OnSectionSettingsExpanded(bool bIsExpanded, int32 LODIndex)
{
	check(LODIndex >= 0 && LODIndex < MAX_STATIC_MESH_LODS);
	bSectionSettingsExpanded[LODIndex] = bIsExpanded;
}

void FLevelOfDetailSettingsLayout::OnLODGroupChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo)
{
	UStaticMesh* StaticMesh = StaticMeshEditor.GetStaticMesh();
	check(StaticMesh);
	int32 GroupIndex = LODGroupOptions.Find(NewValue);
	FName NewGroup = LODGroupNames[GroupIndex];
	if (StaticMesh->LODGroup != NewGroup)
	{
		EAppReturnType::Type DialogResult = FMessageDialog::Open(
			EAppMsgType::YesNo,
			FText::Format( LOCTEXT("ApplyDefaultLODSettings", "Changing LOD group will overwrite the current settings with the defaults from LOD group '{0}'. Do you wish to continue?"), FText::FromString( **NewValue ) )
			);
		if (DialogResult == EAppReturnType::Yes)
		{
			StaticMesh->SetLODGroup(NewGroup);
			// update the internal count
			LODCount = StaticMesh->SourceModels.Num();
			StaticMeshEditor.RefreshTool();
		}
		else
		{
			// Overriding the selection; ensure that the widget correctly reflects the property value
			int32 Index = LODGroupNames.Find(StaticMesh->LODGroup);
			check(Index != INDEX_NONE);
			LODGroupComboBox->SetSelectedItem(LODGroupOptions[Index]);
		}
	}
}

bool FLevelOfDetailSettingsLayout::IsAutoLODEnabled() const
{
	UStaticMesh* StaticMesh = StaticMeshEditor.GetStaticMesh();
	check(StaticMesh);
	return StaticMesh->bAutoComputeLODScreenSize;
}

ECheckBoxState FLevelOfDetailSettingsLayout::IsAutoLODChecked() const
{
	return IsAutoLODEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void FLevelOfDetailSettingsLayout::OnAutoLODChanged(ECheckBoxState NewState)
{
	UStaticMesh* StaticMesh = StaticMeshEditor.GetStaticMesh();
	check(StaticMesh);
	StaticMesh->Modify();
	StaticMesh->bAutoComputeLODScreenSize = (NewState == ECheckBoxState::Checked) ? true : false;
	if (!StaticMesh->bAutoComputeLODScreenSize)
	{
		if (StaticMesh->SourceModels.IsValidIndex(0))
		{
			StaticMesh->SourceModels[0].ScreenSize = 1.0f;
		}
		for (int32 LODIndex = 1; LODIndex < StaticMesh->SourceModels.Num(); ++LODIndex)
		{
			StaticMesh->SourceModels[LODIndex].ScreenSize = StaticMesh->RenderData->ScreenSize[LODIndex];
		}
	}
	StaticMesh->PostEditChange();
	StaticMeshEditor.RefreshTool();
}

void FLevelOfDetailSettingsLayout::OnImportLOD(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo)
{
	int32 LODIndex = 0;
	if( LODNames.Find(NewValue, LODIndex) && LODIndex > 0 )
	{
		UStaticMesh* StaticMesh = StaticMeshEditor.GetStaticMesh();
		check(StaticMesh);
		FbxMeshUtils::ImportMeshLODDialog(StaticMesh,LODIndex);
		StaticMesh->PostEditChange();
		StaticMeshEditor.RefreshTool();
	}

}

bool FLevelOfDetailSettingsLayout::IsApplyNeeded() const
{
	UStaticMesh* StaticMesh = StaticMeshEditor.GetStaticMesh();
	check(StaticMesh);

	if (StaticMesh->SourceModels.Num() != LODCount)
	{
		return true;
	}

	for (int32 LODIndex = 0; LODIndex < LODCount; ++LODIndex)
	{
		FStaticMeshSourceModel& SrcModel = StaticMesh->SourceModels[LODIndex];
		if (BuildSettingsWidgets[LODIndex].IsValid()
			&& SrcModel.BuildSettings != BuildSettingsWidgets[LODIndex]->GetSettings())
		{
			return true;
		}
		if (ReductionSettingsWidgets[LODIndex].IsValid()
			&& SrcModel.ReductionSettings != ReductionSettingsWidgets[LODIndex]->GetSettings())
		{
			return true;
		}
	}

	return false;
}

void FLevelOfDetailSettingsLayout::ApplyChanges()
{
	UStaticMesh* StaticMesh = StaticMeshEditor.GetStaticMesh();
	check(StaticMesh);

	// Calling Begin and EndSlowTask are rather dangerous because they tick
	// Slate. Call them here and flush rendering commands to be sure!.

	FFormatNamedArguments Args;
	Args.Add( TEXT("StaticMeshName"), FText::FromString( StaticMesh->GetName() ) );
	GWarn->BeginSlowTask( FText::Format( LOCTEXT("ApplyLODChanges", "Applying changes to {StaticMeshName}..."), Args ), true );
	FlushRenderingCommands();

	StaticMesh->Modify();
	if (StaticMesh->SourceModels.Num() > LODCount)
	{
		int32 NumToRemove = StaticMesh->SourceModels.Num() - LODCount;
		StaticMesh->SourceModels.RemoveAt(LODCount, NumToRemove);
	}
	while (StaticMesh->SourceModels.Num() < LODCount)
	{
		new(StaticMesh->SourceModels) FStaticMeshSourceModel();
	}
	check(StaticMesh->SourceModels.Num() == LODCount);

	for (int32 LODIndex = 0; LODIndex < LODCount; ++LODIndex)
	{
		FStaticMeshSourceModel& SrcModel = StaticMesh->SourceModels[LODIndex];
		if (BuildSettingsWidgets[LODIndex].IsValid())
		{
			SrcModel.BuildSettings = BuildSettingsWidgets[LODIndex]->GetSettings();
		}
		if (ReductionSettingsWidgets[LODIndex].IsValid())
		{
			SrcModel.ReductionSettings = ReductionSettingsWidgets[LODIndex]->GetSettings();
		}

		if (LODIndex == 0)
		{
			SrcModel.ScreenSize = 1.0f;
		}
		else
		{
			SrcModel.ScreenSize = LODScreenSizes[LODIndex];
			FStaticMeshSourceModel& PrevModel = StaticMesh->SourceModels[LODIndex-1];
			if(SrcModel.ScreenSize >= PrevModel.ScreenSize)
			{
				const float DefaultScreenSizeDifference = 0.01f;
				LODScreenSizes[LODIndex] = LODScreenSizes[LODIndex-1] - DefaultScreenSizeDifference;

				// Make sure there are no incorrectly overlapping values
				SrcModel.ScreenSize = 1.0f - 0.01f * LODIndex;
			}
		}
	}
	StaticMesh->PostEditChange();

	GWarn->EndSlowTask();

	StaticMeshEditor.RefreshTool();
}

FReply FLevelOfDetailSettingsLayout::OnApply()
{
	ApplyChanges();
	return FReply::Handled();
}

void FLevelOfDetailSettingsLayout::OnLODCountChanged(int32 NewValue)
{
	LODCount = FMath::Clamp<int32>(NewValue, 1, MAX_STATIC_MESH_LODS);

	UpdateLODNames();
}

void FLevelOfDetailSettingsLayout::OnLODCountCommitted(int32 InValue, ETextCommit::Type CommitInfo)
{
	OnLODCountChanged(InValue);
}

FText FLevelOfDetailSettingsLayout::GetLODCountTooltip() const
{
	if(IsAutoMeshReductionAvailable())
	{
		return LOCTEXT("LODCountTooltip", "The number of LODs for this static mesh. If auto mesh reduction is available, setting this number will determine the number of LOD levels to auto generate.");
	}

	return LOCTEXT("LODCountTooltip_Disabled", "Auto mesh reduction is unavailable! Please provide a mesh reduction interface such as Simplygon to use this feature or manually import LOD levels.");
}

int32 FLevelOfDetailSettingsLayout::GetMinLOD() const
{
	UStaticMesh* StaticMesh = StaticMeshEditor.GetStaticMesh();
	check(StaticMesh);

	return StaticMesh->MinLOD;
}

void FLevelOfDetailSettingsLayout::OnMinLODChanged(int32 NewValue)
{
	UStaticMesh* StaticMesh = StaticMeshEditor.GetStaticMesh();
	check(StaticMesh);

	{
		FStaticMeshComponentRecreateRenderStateContext ReregisterContext(StaticMesh,false);
		StaticMesh->MinLOD = FMath::Clamp<int32>(NewValue, 0, MAX_STATIC_MESH_LODS - 1);
		StaticMesh->Modify();
	}
	StaticMeshEditor.RefreshViewport();
}

void FLevelOfDetailSettingsLayout::OnMinLODCommitted(int32 InValue, ETextCommit::Type CommitInfo)
{
	OnMinLODChanged(InValue);
}

FText FLevelOfDetailSettingsLayout::GetMinLODTooltip() const
{
	return LOCTEXT("MinLODTooltip", "The minimum LOD to use for rendering.  This can be overridden in components.");
}

FText FLevelOfDetailSettingsLayout::GetLODCustomModeNameContent(int32 LODIndex) const
{
	int32 CurrentLodIndex = 0;
	if (StaticMeshEditor.GetStaticMeshComponent() != nullptr)
	{
		CurrentLodIndex = StaticMeshEditor.GetStaticMeshComponent()->ForcedLodModel;
	}
	int32 RealCurrentLODIndex = (CurrentLodIndex == 0 ? 0 : CurrentLodIndex - 1);
	if (LODIndex == INDEX_NONE)
	{
		return LOCTEXT("GetLODCustomModeNameContent", "Custom");
	}
	return FText::Format(LOCTEXT("GetLODModeNameContent", "LOD{0}"), LODIndex);
}

ECheckBoxState FLevelOfDetailSettingsLayout::IsLODCustomModeCheck(int32 LODIndex) const
{
	int32 CurrentLodIndex = 0;
	if (StaticMeshEditor.GetStaticMeshComponent() != nullptr)
	{
		CurrentLodIndex = StaticMeshEditor.GetStaticMeshComponent()->ForcedLodModel;
	}
	if (LODIndex == INDEX_NONE)
	{
		return CustomLODEditMode ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}
	return DetailDisplayLODs[LODIndex] ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void FLevelOfDetailSettingsLayout::SetLODCustomModeCheck(ECheckBoxState NewState, int32 LODIndex)
{
	int32 CurrentLodIndex = 0;
	if (StaticMeshEditor.GetStaticMeshComponent() != nullptr)
	{
		CurrentLodIndex = StaticMeshEditor.GetStaticMeshComponent()->ForcedLodModel;
	}
	if (LODIndex == INDEX_NONE)
	{
		if (NewState == ECheckBoxState::Unchecked)
		{
			CustomLODEditMode = false;
			SectionSettingsWidgets[0]->SetCurrentLOD(CurrentLodIndex);
			for (int32 DetailLODIndex = 0; DetailLODIndex < MAX_STATIC_MESH_LODS; ++DetailLODIndex)
			{
				if (!LodCategories.IsValidIndex(DetailLODIndex))
				{
					break;
				}
				LodCategories[DetailLODIndex]->SetCategoryVisibility(DetailLODIndex == (CurrentLodIndex == 0 ? 0 : CurrentLodIndex-1));
			}
		}
		else
		{
			CustomLODEditMode = true;
			SectionSettingsWidgets[0]->SetCurrentLOD(0);
		}
	}
	else if(CustomLODEditMode)
	{
		DetailDisplayLODs[LODIndex] = NewState == ECheckBoxState::Checked;
	}

	if (CustomLODEditMode)
	{
		for (int32 DetailLODIndex = 0; DetailLODIndex < MAX_STATIC_MESH_LODS; ++DetailLODIndex)
		{
			if (!LodCategories.IsValidIndex(DetailLODIndex))
			{
				break;
			}
			LodCategories[DetailLODIndex]->SetCategoryVisibility(DetailDisplayLODs[DetailLODIndex]);
		}
	}
}

bool FLevelOfDetailSettingsLayout::IsLODCustomModeEnable(int32 LODIndex) const
{
	if (LODIndex == INDEX_NONE)
	{
		// Custom checkbox is always enable
		return true;
	}
	return CustomLODEditMode;
}

#undef LOCTEXT_NAMESPACE
