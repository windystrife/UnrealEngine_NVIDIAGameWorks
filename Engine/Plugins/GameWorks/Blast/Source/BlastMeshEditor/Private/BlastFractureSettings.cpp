// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BlastFractureSettings.h"
#include "BlastFracture.h"
#include "BlastMesh.h"
#include "Engine/SkeletalMesh.h"
#include "BlastMeshEditor.h"

//#include "Materials/Material.h"
#include "DetailLayoutBuilder.h"
#include "PropertyHandle.h"
#include "DetailWidgetRow.h"
#include "DetailCategoryBuilder.h"
#include "IDetailCustomization.h"
#include "IDetailsView.h"
#include "IDetailCustomNodeBuilder.h"
#include "IDetailPropertyRow.h"
#include "Widgets/Input/SComboButton.h"
#include "SButton.h"
#include "MultiBox/MultiBoxBuilder.h"
#include "Commands/UIAction.h"

#define LOCTEXT_NAMESPACE "BlastMeshEditor"

//////////////////////////////////////////////////////////////////////////
// FBlastVectorCustomization
//////////////////////////////////////////////////////////////////////////

FBlastVector::FOnVisualModificationDelegate FBlastVector::OnVisualModification;

TOptional<float> FBlastVectorCustomization::OnGetValue(int axis) const
{
	TArray<void*> RawData;
	PropertyHandle->AccessRawData(RawData);

	if (RawData.Num() == 1)
	{
		FBlastVector* Value = reinterpret_cast<FBlastVector*>(RawData[0]);
		if (Value != NULL)
		{
			return Value->V[axis];
		}
	}
	return TOptional<float>();
}

void FBlastVectorCustomization::OnValueCommitted(float NewValue, ETextCommit::Type CommitType, int axis)
{
	TArray<void*> RawData;
	PropertyHandle->AccessRawData(RawData);

	if (RawData.Num() == 1)
	{
		FBlastVector* Value = reinterpret_cast<FBlastVector*>(RawData[0]);
		if (Value != NULL)
		{
			Value->V[axis] = NewValue;
		}
	}
}

//void OnValueChanged(float NewValue, uint32_t axis);
//{
//	if (bIsUsingSlider)
//	{
//		ProxyValue->Set(NewValue);
//		FlushValues(WeakHandlePtr);
//	}
//}

//void FBlastVectorCustomization::OnBeginSliderMovement()
//{
//	bIsUsingSlider = true;
//}
//
//template<typename ProxyType, typename NumericType>
//void FBlastVectorCustomization::OnEndSliderMovement(NumericType NewValue, TWeakPtr<IPropertyHandle> WeakHandlePtr, TSharedRef< TProxyProperty<ProxyType, NumericType> > ProxyValue)
//{
//	bIsUsingSlider = false;
//
//	ProxyValue->Set(NewValue);
//	FlushValues(WeakHandlePtr);
//}

TSharedRef<IPropertyTypeCustomization> FBlastVectorCustomization::MakeInstance()
{
	return MakeShareable(new FBlastVectorCustomization());
}

void FBlastVectorCustomization::CustomizeHeader(TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	PropertyHandle = StructPropertyHandle;
	//uint32 NumChildren;
	//StructPropertyHandle->GetNumChildren(NumChildren);

	//for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
	//{
	//	const TSharedRef< IPropertyHandle > ChildHandle = StructPropertyHandle->GetChildHandle(ChildIndex).ToSharedRef();

	//	if (ChildHandle->GetProperty()->GetName() == TEXT("V"))
	//	{
	//		PropertyHandle = ChildHandle;
	//	}
	//}

	//check(PropertyHandle.IsValid());

	HeaderRow.NameContent()
		[
			StructPropertyHandle->CreatePropertyNameWidget(StructPropertyHandle->GetProperty()->GetDisplayNameText(), StructPropertyHandle->GetProperty()->GetToolTipText(), true)
		]
		.ValueContent()
		.MinDesiredWidth(500)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1)
			.Padding(FMargin(0.0f, 2.0f, 3.0f, 2.0f))
			[
				SNew(SVectorInputBox)
				.bColorAxisLabels(true)
				.AllowResponsiveLayout(true)
				.AllowSpin(false)
				.X(this, &FBlastVectorCustomization::OnGetValue, 0)
				.Y(this, &FBlastVectorCustomization::OnGetValue, 1)
				.Z(this, &FBlastVectorCustomization::OnGetValue, 2)
				.OnXCommitted(this, &FBlastVectorCustomization::OnValueCommitted, 0)
				.OnYCommitted(this, &FBlastVectorCustomization::OnValueCommitted, 1)
				.OnZCommitted(this, &FBlastVectorCustomization::OnValueCommitted, 2)
			]
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			.Padding(2.0f)
			[
				SAssignNew(Button, SButton)
				.Text(LOCTEXT("Pick vector", "Pick vector"))
				.ToolTipText(LOCTEXT("Pick vector", "Pick vector"))
				.OnClicked(this, &FBlastVectorCustomization::OnClicked)
				.Content()
				[
					SNew(SImage)
					.Image(this, &FBlastVectorCustomization::GetVisibilityBrush)
				]
			]			
		];
}

FReply FBlastVectorCustomization::OnClicked()
{
	TArray<void*> RawData;
	PropertyHandle->AccessRawData(RawData);

	if (RawData.Num() == 1)
	{
		FBlastVector* Value = reinterpret_cast<FBlastVector*>(RawData[0]);
		if (Value != NULL)
		{
			Value->Activate();
		}
	}

	return FReply::Handled();
}

const FSlateBrush* FBlastVectorCustomization::GetVisibilityBrush() const
{
	TArray<void*> RawData;
	PropertyHandle->AccessRawData(RawData);

	if (RawData.Num() == 1)
	{
		FBlastVector* Value = reinterpret_cast<FBlastVector*>(RawData[0]);
		if (Value != NULL && Value->IsActive)
		{
			Button->SetColorAndOpacity(FLinearColor::Blue);
		}
		else
		{
			Button->SetColorAndOpacity(FLinearColor::White);
		}
	}
	return FBlastMeshEditorStyle::Get()->GetBrush("BlastMeshEditor.Adjust");
}

void FBlastVectorCustomization::CustomizeChildren(TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	//Create further customization here
}

//////////////////////////////////////////////////////////////////////////
// UBlastFixChunkHierarchyProperties
//////////////////////////////////////////////////////////////////////////

UBlastFixChunkHierarchyProperties::UBlastFixChunkHierarchyProperties(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

//////////////////////////////////////////////////////////////////////////
// UBlastRebuildCollisionMeshProperties
//////////////////////////////////////////////////////////////////////////

UBlastRebuildCollisionMeshProperties::UBlastRebuildCollisionMeshProperties(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

//////////////////////////////////////////////////////////////////////////
// UBlastStaticMeshHolder
//////////////////////////////////////////////////////////////////////////

UBlastStaticMeshHolder::UBlastStaticMeshHolder(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UBlastStaticMeshHolder::PostEditChangeProperty(struct FPropertyChangedEvent& e)
{
	Super::PostEditChangeProperty(e);
	FName PropertyName = (e.Property != NULL) ? e.Property->GetFName() : NAME_None;
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UBlastStaticMeshHolder, StaticMesh) && StaticMesh != nullptr)
	{
		OnStaticMeshSelected.ExecuteIfBound();
	}
}

//////////////////////////////////////////////////////////////////////////
// FFractureMaterial
//////////////////////////////////////////////////////////////////////////


/*void FFractureMaterial::FillNxFractureMaterialDesc(apex::FractureMaterialDesc& PFractureMaterialDesc)
{
	PFractureMaterialDesc.uvScale = PxVec2(UVScale.X, UVScale.Y);
	PFractureMaterialDesc.uvOffset = PxVec2(UVOffset.X, UVOffset.Y);
	PFractureMaterialDesc.tangent = U2PVector(Tangent);
	PFractureMaterialDesc.uAngle = UAngle;
	PFractureMaterialDesc.interiorSubmeshIndex = InteriorElementIndex >= 0 ? (PxU32)InteriorElementIndex : 0xFFFFFFFF;	// We'll use this value to indicate we should create a new element
}*/

//////////////////////////////////////////////////////////////////////////
// FBlastFractureSettingsComponentDetails
//////////////////////////////////////////////////////////////////////////

TSharedRef<IDetailCustomization> FBlastFractureSettingsComponentDetails::MakeInstance()
{
	return MakeShareable(new FBlastFractureSettingsComponentDetails);
}

FReply FBlastFractureSettingsComponentDetails::ExecuteToolCommand(IDetailLayoutBuilder* DetailBuilder, UFunction* MethodToExecute)
{
	TArray <TWeakObjectPtr <UObject>> ObjectBeingCustomized;
	DetailBuilder->GetObjectsBeingCustomized(ObjectBeingCustomized);

	for (auto WeakObject : ObjectBeingCustomized)
	{
		if (UObject* Instance = WeakObject.Get())
		{
			Instance->CallFunctionByNameWithArguments(*MethodToExecute->GetName(), *GLog, nullptr, true);
		}
	}

	return FReply::Handled();
}


class FInteriorMaterialSlotDropdownBuilder : public IDetailCustomNodeBuilder, public TSharedFromThis<FInteriorMaterialSlotDropdownBuilder>
{
	TSharedRef<IPropertyHandle> PropertyHandle;
	FBlastMeshEditor* MeshEditor;
public:
	FInteriorMaterialSlotDropdownBuilder(TSharedRef<IPropertyHandle> InPropertyHandle, FBlastMeshEditor* InMeshEditor) : PropertyHandle(InPropertyHandle), MeshEditor(InMeshEditor)
	{

	}

	virtual void SetOnRebuildChildren(FSimpleDelegate InOnRegenerateChildren) override {}

	void SetCurrentSelection(FName Value)
	{
		PropertyHandle->SetValue(Value);
	}

	FText GetCurrentSelection() const
	{
		FName CurValue;
		if (PropertyHandle->GetValue(CurValue) == FPropertyAccess::Success)
		{
			return CurValue.IsNone() ? LOCTEXT("NewMaterialSlot", "Create New Material Slot") : FText::FromName(CurValue);
		}
		return LOCTEXT("NewMaterialSlotUnknown", "<Unknown Value>");
	}

	TSharedRef<SWidget> PopulateMenu()
	{
		FMenuBuilder MenuBuilder(true, NULL);
		UBlastMesh* CurrentMesh = MeshEditor->GetBlastMesh();
		if (CurrentMesh)
		{
			for (const FSkeletalMaterial& SkelMat : CurrentMesh->Mesh->Materials)
			{
				FName NameToUse = SkelMat.ImportedMaterialSlotName.IsNone() ? SkelMat.MaterialSlotName : SkelMat.ImportedMaterialSlotName;
				FExecuteAction SelectAction = FExecuteAction::CreateSP(this, &FInteriorMaterialSlotDropdownBuilder::SetCurrentSelection, NameToUse);
				MenuBuilder.AddMenuEntry(FText::FromName(NameToUse), FText(), FSlateIcon(), FUIAction(SelectAction));
			}
			MenuBuilder.AddMenuSeparator();
		}

		FExecuteAction SelectAction = FExecuteAction::CreateSP(this, &FInteriorMaterialSlotDropdownBuilder::SetCurrentSelection, FName(NAME_None));
		MenuBuilder.AddMenuEntry(LOCTEXT("NewMaterialSlot", "Create New Material Slot"), FText(), FSlateIcon(), FUIAction(SelectAction));
		
		return MenuBuilder.MakeWidget();
	}


	virtual void GenerateHeaderRowContent(FDetailWidgetRow& NodeRow) override
	{
		if (PropertyHandle->GetProperty() != nullptr)
		{
			NodeRow.NameContent()
			[
				PropertyHandle->CreatePropertyNameWidget()
			];

			// Otherwise, use the default property widget
			NodeRow.ValueContent()
			[
				SNew(SComboButton)
				.ToolTipText(PropertyHandle->GetToolTipText())
				.OnGetMenuContent(FOnGetContent::CreateSP(this, &FInteriorMaterialSlotDropdownBuilder::PopulateMenu))
				.ButtonContent()
				[
					SNew(STextBlock)
					.Text(this, &FInteriorMaterialSlotDropdownBuilder::GetCurrentSelection)
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
			];
		}
	}

	virtual void GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder) override {}
	virtual void Tick(float DeltaTime) override {}
	virtual bool RequiresTick() const override { return false; }
	virtual bool InitiallyCollapsed() const override { return false; }

	virtual FName GetName() const override 
	{
		if (PropertyHandle->GetProperty() != nullptr)
		{
			return PropertyHandle->GetProperty()->GetFName();
		}
		return NAME_None;
	}

};

void FBlastFractureSettingsComponentDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TSet <UClass*> Classes;

	TArray <TWeakObjectPtr <UObject>> ObjectBeingCustomized;
	DetailBuilder.GetObjectsBeingCustomized(ObjectBeingCustomized);

	FBlastMeshEditor* MeshEditor = nullptr;
	for (auto WeakObject : ObjectBeingCustomized)
	{
		if (UObject* Instance = WeakObject.Get())
		{
			if (UBlastFractureSettings* FS = Cast<UBlastFractureSettings>(Instance))
			{
				if (FS->BlastMeshEditor)
				{
					MeshEditor = FS->BlastMeshEditor;
				}
			}
			Classes.Add(Instance->GetClass());
		}
	}

	//Create category
	IDetailCategoryBuilder& Category = DetailBuilder.EditCategory("General");

	TSharedRef<IPropertyHandle> InteriorMaterialSlotNameProp = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UBlastFractureSettings, InteriorMaterialSlotName));
	TSharedRef<IPropertyHandle> InteriorMaterialProp = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UBlastFractureSettings, InteriorMaterial));
	if (MeshEditor)
	{
		DetailBuilder.HideProperty(InteriorMaterialSlotNameProp);
		Category.AddCustomBuilder(MakeShared<FInteriorMaterialSlotDropdownBuilder>(InteriorMaterialSlotNameProp, MeshEditor));
		Category.AddProperty(InteriorMaterialProp).EditCondition(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateLambda([InteriorMaterialSlotNameProp]()
		{
			FName CurValue;
			InteriorMaterialSlotNameProp->GetValue(CurValue);
			return CurValue.IsNone();
		})), nullptr);
	}
	


	//Create a button for each element
	for (UClass* Class : Classes)
	{
		for (TFieldIterator <UFunction> FuncIt(Class); FuncIt; ++FuncIt)
		{
			UFunction* Function = *FuncIt;
			if (Function->HasMetaData(TEXT("FractureSettingsDefaults")) && (Function->NumParms == 0))
			{
				const FString FunctionName = Function->GetName();
				FText ButtonCaption;
				FText ToolTip;
				if (FunctionName.Equals("LoadDefault"))
				{
					ButtonCaption = FText::FromString("Load Default");
					ToolTip = FText::FromString("Load default fracture settings");
				}
				else if (FunctionName.Equals("SaveAsDefault"))
				{
					ButtonCaption = FText::FromString("Save As Default");
					ToolTip = FText::FromString("Save current fracture settings as default");
				}
				else
				{
					ButtonCaption = FText::FromString(FunctionName);
				}
				//const FString FilterString = FunctionName;
				Category.AddCustomRow(ButtonCaption, true)
					.ValueContent()
					[
						SNew(SButton)
						.ToolTipText(ToolTip)
						.ButtonStyle(FEditorStyle::Get(), "FlatButton.Dark")
						.OnClicked(FOnClicked::CreateStatic(&FBlastFractureSettingsComponentDetails::ExecuteToolCommand, &DetailBuilder, Function))
						[	
							SNew(STextBlock)
							.ColorAndOpacity(FLinearColor::White)
							.Text(ButtonCaption)
						]
					];
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// UBlastractureSettings{Custom}
//////////////////////////////////////////////////////////////////////////

UBlastFractureSettingsNoise::UBlastFractureSettingsNoise(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UBlastFractureSettingsNoise::Setup(float InAmplitude, float InFrequency, int32 InOctaveNumber, int32 InSurfaceResolution)
{
	Amplitude = InAmplitude;
	Frequency = InFrequency;
	OctaveNumber = InOctaveNumber;
	SurfaceResolution = InSurfaceResolution;
}

void UBlastFractureSettingsNoise::Setup(const UBlastFractureSettingsNoise& Other)
{
	Setup(Other.Amplitude, Other.Frequency, Other.OctaveNumber, Other.SurfaceResolution);
}

UBlastFractureSettingsVoronoi::UBlastFractureSettingsVoronoi(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UBlastFractureSettingsVoronoi::Setup(bool InForceReset, const FVector& InAnisotropy, const FQuat& InRotation)
{
	ForceReset = InForceReset;
	CellAnisotropy = InAnisotropy;
	CellRotation = InRotation;
}

void UBlastFractureSettingsVoronoi::Setup(const UBlastFractureSettingsVoronoi& Other)
{
	Setup(Other.ForceReset, Other.CellAnisotropy, Other.CellRotation);
}

UBlastFractureSettingsVoronoiUniform::UBlastFractureSettingsVoronoiUniform(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UBlastFractureSettingsVoronoiClustered::UBlastFractureSettingsVoronoiClustered(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UBlastFractureSettingsRadial::UBlastFractureSettingsRadial(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Origin.DefaultBlastVectorActivation = &Origin;
	Normal.DefaultBlastVectorActivation = &Origin;
}

UBlastFractureSettingsInSphere::UBlastFractureSettingsInSphere(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Origin.DefaultBlastVectorActivation = &Origin;
}

UBlastFractureSettingsRemoveInSphere::UBlastFractureSettingsRemoveInSphere(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Origin.DefaultBlastVectorActivation = &Origin;
}

UBlastFractureSettingsUniformSlicing::UBlastFractureSettingsUniformSlicing(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UBlastFractureSettingsCutout::UBlastFractureSettingsCutout(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Origin.DefaultBlastVectorActivation = &Origin;
	Normal.DefaultBlastVectorActivation = &Origin;
}

UBlastFractureSettingsCut::UBlastFractureSettingsCut(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Point.DefaultBlastVectorActivation = &Point;
	Normal.DefaultBlastVectorActivation = &Point;
}

//////////////////////////////////////////////////////////////////////////
// UBlastractureSettings
//////////////////////////////////////////////////////////////////////////

UBlastFractureSettings::UBlastFractureSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	VoronoiUniformFracture = NewObject<UBlastFractureSettingsVoronoiUniform>(this, GET_MEMBER_NAME_CHECKED(UBlastFractureSettings, VoronoiUniformFracture));
	VoronoiClusteredFracture = NewObject<UBlastFractureSettingsVoronoiClustered>(this, GET_MEMBER_NAME_CHECKED(UBlastFractureSettings, VoronoiClusteredFracture));
	RadialFracture = NewObject<UBlastFractureSettingsRadial>(this, GET_MEMBER_NAME_CHECKED(UBlastFractureSettings, RadialFracture));
	InSphereFracture = NewObject<UBlastFractureSettingsInSphere>(this, GET_MEMBER_NAME_CHECKED(UBlastFractureSettings, InSphereFracture));
	RemoveInSphere = NewObject<UBlastFractureSettingsRemoveInSphere>(this, GET_MEMBER_NAME_CHECKED(UBlastFractureSettings, RemoveInSphere));
	UniformSlicingFracture = NewObject<UBlastFractureSettingsUniformSlicing>(this, GET_MEMBER_NAME_CHECKED(UBlastFractureSettings, UniformSlicingFracture));
	CutoutFracture = NewObject<UBlastFractureSettingsCutout>(this, GET_MEMBER_NAME_CHECKED(UBlastFractureSettings, CutoutFracture));
	CutFracture = NewObject<UBlastFractureSettingsCut>(this, GET_MEMBER_NAME_CHECKED(UBlastFractureSettings, CutFracture));
	
	LoadDefault();
}

void UBlastFractureSettings::LoadDefault()
{
	auto Config = FBlastFracture::GetInstance()->GetConfig();
	if (Config)
	{
		VoronoiUniformFracture->CellCount = Config->VoronoiUniformCellCount;
		VoronoiUniformFracture->Setup(Config->VoronoiForceReset, Config->VoronoiCellAnisotropy, Config->VoronoiCellRotation);

		VoronoiClusteredFracture->CellCount = Config->VoronoiClusteredCellCount;
		VoronoiClusteredFracture->ClusterCount = Config->VoronoiClusteredClusterCount;
		VoronoiClusteredFracture->ClusterRadius = Config->VoronoiClusteredClusterRadius;
		VoronoiClusteredFracture->Setup(Config->VoronoiForceReset, Config->VoronoiCellAnisotropy, Config->VoronoiCellRotation);

		//RadialFracture->Origin = Config->RadialOrigin;
		//RadialFracture->Normal = Config->RadialNormal;
		RadialFracture->Radius = Config->RadialRadius;
		RadialFracture->AngularSteps = Config->RadialAngularSteps;
		RadialFracture->RadialSteps = Config->RadialRadialSteps;
		RadialFracture->AngleOffset = Config->RadialAngleOffset;
		RadialFracture->Variability = Config->RadialVariability;
		RadialFracture->Setup(Config->VoronoiForceReset, Config->VoronoiCellAnisotropy, Config->VoronoiCellRotation);

		InSphereFracture->CellCount = Config->InSphereCellCount;
		InSphereFracture->Radius = Config->InSphereRadius;
		InSphereFracture->Setup(Config->VoronoiForceReset, Config->VoronoiCellAnisotropy, Config->VoronoiCellRotation);

		RemoveInSphere->Radius = Config->RemoveInSphereRadius;
		RemoveInSphere->Probability = Config->RemoveInSphereProbability;
		RemoveInSphere->Setup(Config->VoronoiForceReset, Config->VoronoiCellAnisotropy, Config->VoronoiCellRotation);

		UniformSlicingFracture->SlicesCount = Config->UniformSlicingSlicesCount;
		UniformSlicingFracture->AngleVariation = Config->UniformSlicingAngleVariation;
		UniformSlicingFracture->OffsetVariation = Config->UniformSlicingOffsetVariation;
		UniformSlicingFracture->Setup(Config->NoiseAmplitude, Config->NoiseFrequency, Config->NoiseOctaveNumber, Config->NoiseSurfaceResolution);
		
		CutoutFracture->Size = Config->CutoutSize;
		CutoutFracture->RotationZ = Config->CutoutRotationZ;
		CutoutFracture->bPeriodic = Config->bCutoutPeriodic;
		CutoutFracture->bFillGaps = Config->bCutoutFillGaps;
		CutoutFracture->Setup(Config->NoiseAmplitude, Config->NoiseFrequency, Config->NoiseOctaveNumber, Config->NoiseSurfaceResolution);

		CutFracture->Setup(Config->NoiseAmplitude, Config->NoiseFrequency, Config->NoiseOctaveNumber, Config->NoiseSurfaceResolution);

		bRemoveIslands = Config->bRemoveIslands;
		bReplaceFracturedChunk = Config->bReplaceFracturedChunk;
		bUseFractureSeed = Config->RandomSeed >= 0;
		FractureSeed = Config->RandomSeed < 0 ? 0 : Config->RandomSeed;
		bDefaultSupportDepth = Config->DefaultSupportDepth >= 0;
		DefaultSupportDepth = Config->DefaultSupportDepth < 0 ? 0 : Config->DefaultSupportDepth;
	}
}

void UBlastFractureSettings::SaveAsDefault()
{
	auto Config = FBlastFracture::GetInstance()->GetConfig();
	if (Config)
	{
		Config->VoronoiForceReset = VoronoiUniformFracture->ForceReset;
		Config->VoronoiCellAnisotropy = VoronoiUniformFracture->CellAnisotropy;
		Config->VoronoiCellRotation = VoronoiUniformFracture->CellRotation;

		Config->VoronoiUniformCellCount = VoronoiUniformFracture->CellCount;

		Config->VoronoiClusteredCellCount = VoronoiClusteredFracture->CellCount;
		Config->VoronoiClusteredClusterCount = VoronoiClusteredFracture->ClusterCount;
		Config->VoronoiClusteredClusterRadius = VoronoiClusteredFracture->ClusterRadius;

		//Config->RadialOrigin = RadialFracture->Origin;
		//Config->RadialNormal = RadialFracture->Normal;
		Config->RadialRadius = RadialFracture->Radius;
		Config->RadialAngularSteps = RadialFracture->AngularSteps;
		Config->RadialRadialSteps = RadialFracture->RadialSteps;
		Config->RadialAngleOffset = RadialFracture->AngleOffset;
		Config->RadialVariability = RadialFracture->Variability;

		Config->InSphereCellCount = InSphereFracture->CellCount;
		Config->InSphereRadius = InSphereFracture->Radius;

		Config->RemoveInSphereRadius = RemoveInSphere->Radius;
		Config->RemoveInSphereProbability = RemoveInSphere->Probability;

		Config->UniformSlicingSlicesCount = UniformSlicingFracture->SlicesCount;
		Config->UniformSlicingAngleVariation = UniformSlicingFracture->AngleVariation;
		Config->UniformSlicingOffsetVariation = UniformSlicingFracture->OffsetVariation;

		Config->CutoutSize = CutoutFracture->Size;
		Config->CutoutRotationZ = CutoutFracture->RotationZ;
		Config->bCutoutPeriodic = CutoutFracture->bPeriodic;
		Config->bCutoutFillGaps = CutoutFracture->bFillGaps;

		Config->NoiseAmplitude = UniformSlicingFracture->Amplitude;
		Config->NoiseFrequency = UniformSlicingFracture->Frequency;
		Config->NoiseOctaveNumber = UniformSlicingFracture->OctaveNumber;
		Config->NoiseSurfaceResolution = UniformSlicingFracture->SurfaceResolution;

		Config->bRemoveIslands = bRemoveIslands;
		Config->bReplaceFracturedChunk = bReplaceFracturedChunk;
		Config->RandomSeed = bUseFractureSeed ? FractureSeed : -1;
		Config->DefaultSupportDepth = bDefaultSupportDepth ? DefaultSupportDepth : -1;

		Config->SaveConfig();
	}
}

void UBlastFractureSettings::Reset()
{
	FractureSession.Reset();
	//FractureHistory.Empty();
}

void UBlastFractureSettings::PostEditChangeProperty(struct FPropertyChangedEvent& e)
{
	Super::PostEditChangeProperty(e);
	FName PropertyName = (e.Property != NULL) ? e.Property->GetFName() : NAME_None;
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UBlastFractureSettings, InteriorMaterial) && InteriorMaterial != nullptr)
	{
		OnMaterialSelected.ExecuteIfBound();
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UBlastFractureSettings, FractureMethod))
	{
		OnFractureMethodChanged.ExecuteIfBound();
	}
}

//////////////////////////////////////////////////////////////////////////
// UBlastractureSettingsConfig
//////////////////////////////////////////////////////////////////////////

UBlastFractureSettingsConfig::UBlastFractureSettingsConfig(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	VoronoiCellAnisotropy = FVector(1.f);
	VoronoiCellRotation = FQuat::Identity;
	VoronoiUniformCellCount = 10;
	VoronoiClusteredCellCount = 10;
	VoronoiClusteredClusterCount = 5;
	VoronoiClusteredClusterRadius = 100.f;
	//RadialOrigin = FVector(0.f);
	//RadialNormal = FVector(0.f, 0.f, 1.f);
	RadialRadius = 100.f;
	RadialAngularSteps = 6;
	RadialRadialSteps = 5;
	RadialAngleOffset = 0.f;
	RadialVariability = 0.f;
	InSphereCellCount = 10;
	InSphereRadius = 20.f;
	RemoveInSphereRadius = 20.f;
	RemoveInSphereProbability = 1.f;
	UniformSlicingSlicesCount = FIntVector(2, 2, 2);
	UniformSlicingAngleVariation = 0.f;
	UniformSlicingOffsetVariation = 0.f;
	CutoutSize = FVector2D(100, 100);
	CutoutRotationZ = 0.f;
	bCutoutPeriodic = false;
	bCutoutFillGaps = true;
	NoiseAmplitude = 0.f;
	NoiseFrequency = 1.f;
	NoiseOctaveNumber = 1;
	NoiseSurfaceResolution = 1;
	RandomSeed = -1;
	DefaultSupportDepth = -1;
	bRemoveIslands = true;
	bReplaceFracturedChunk = false;
}

#undef LOCTEXT_NAMESPACE
