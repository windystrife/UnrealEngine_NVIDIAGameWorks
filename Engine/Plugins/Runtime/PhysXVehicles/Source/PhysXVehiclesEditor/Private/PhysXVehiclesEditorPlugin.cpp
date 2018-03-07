// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "UObject/Class.h"
#include "WheeledVehicleMovementComponent4WDetails.h"
#include "PropertyEditorModule.h"
#include "VehicleTransmissionDataCustomization.h"
#include "Modules/ModuleManager.h"
#include "IPhysXVehiclesEditorPlugin.h"
#include "AssetData.h"
#include "AssetRegistryModule.h"
#include "Vehicles/TireType.h"
#include "TireConfig.h"
#include "VehicleWheel.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "HAL/IConsoleManager.h"
#include "Engine/Blueprint.h"

class FPhysXVehiclesEditorPlugin : public IPhysXVehiclesEditorPlugin
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.RegisterCustomClassLayout("WheeledVehicleMovementComponent4W", FOnGetDetailCustomizationInstance::CreateStatic(&FWheeledVehicleMovementComponent4WDetails::MakeInstance));
		PropertyModule.RegisterCustomPropertyTypeLayout("VehicleTransmissionData", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FVehicleTransmissionDataCustomization::MakeInstance));
		PropertyModule.NotifyCustomizationModuleChanged();
	}


	virtual void ShutdownModule() override
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomClassLayout("WheeledVehicleMovementComponent4W");
		PropertyModule.UnregisterCustomPropertyTypeLayout("VehicleTransmissionData");
		PropertyModule.NotifyCustomizationModuleChanged();
	}
};

IMPLEMENT_MODULE(FPhysXVehiclesEditorPlugin, PhysXVehiclesEditor)

// CONVERT TIRE TYPES UTIL
static const FString EngineDir = TEXT("/Engine/");

static void ConvertTireTypes()
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	// First iterate over TireTypes to create new TireConfig's for each
	TMap<UTireType*, UTireConfig*> TireTypeToConfigMap;

	TArray<FAssetData> AssetDatas;
	AssetRegistryModule.Get().GetAssetsByClass(UTireType::StaticClass()->GetFName(), AssetDatas);
	for (FAssetData AssetData : AssetDatas)
	{
		UTireType* TireType = Cast<UTireType>(AssetData.GetAsset());


		if (TireType != nullptr && !TireType->GetPathName().Contains(EngineDir)) // Don't modify engine content
		{
			// Get location to create new TireConfig (next to TireType)
			FString TirePath = TireType->GetPathName(); 
			int32 SlashPos = INDEX_NONE;
			if (TirePath.FindLastChar('/', SlashPos))
			{
				TirePath = TirePath.Left(SlashPos);

				FString TireTypeName = TireType->GetName();
				FString TireConfigName = FString::Printf(TEXT("%s_TireConfig"), *TireTypeName);

				FString TireConfigPackageName = FString::Printf(TEXT("%s/%s"), *TirePath, *TireConfigName);

				UPackage* TireConfigPackage = CreatePackage(NULL, *TireConfigPackageName);
				if (ensure(TireConfigPackage != nullptr))
				{
					UTireConfig* TireConfig = NewObject<UTireConfig>(TireConfigPackage, FName(*TireConfigName), RF_Public | RF_Standalone | RF_Transactional);

					// Notify the asset registry
					FAssetRegistryModule::AssetCreated(TireConfig);

					// Copy tire friction
					TireConfig->SetFrictionScale(TireType->FrictionScale);

					// Mark the package dirty...
					TireConfigPackage->MarkPackageDirty();

					// Add to map
					TireTypeToConfigMap.Add(TireType, TireConfig);
				}
			}
		}
	}

	// Now iterate over PhysicalMaterials, to fill in maps on TireTypes
	AssetDatas.Empty();
	AssetRegistryModule.Get().GetAssetsByClass(UPhysicalMaterial::StaticClass()->GetFName(), AssetDatas);
	for (FAssetData AssetData : AssetDatas)
	{
		UPhysicalMaterial* PhysMat = Cast<UPhysicalMaterial>(AssetData.GetAsset());
		if (PhysMat != nullptr && !PhysMat->GetPathName().Contains(EngineDir)) // Don't modify engine content
		{
			// Find friction scale for this phys mat against each tire config
			for (auto It = TireTypeToConfigMap.CreateIterator(); It; ++It)
			{
				UTireType* TireType = It.Key();

				float Scale = PhysMat->TireFrictionScale;
				for (FTireFrictionScalePair& Pair : PhysMat->TireFrictionScales)
				{
					if (Pair.TireType == TireType)
					{
						Scale *= Pair.FrictionScale;
						break;
					}
				}

				// If it's not 1.0, add entry on TireConfig for this material
				if (!FMath::IsNearlyEqual(Scale, 1.f))
				{
					UTireConfig* TireConfig = It.Value();
					TireConfig->SetPerMaterialFrictionScale(PhysMat, Scale);
				}
			}

			// Clear out old friction scales
			PhysMat->TireFrictionScales.Empty();
		}
	}

	// Iterate over VehicleWheel Blueprints and change to point to TireConfig instead of TireType
	AssetDatas.Empty();
	AssetRegistryModule.Get().GetAssetsByClass(UBlueprint::StaticClass()->GetFName(), AssetDatas);
	for (FAssetData AssetData : AssetDatas)
	{
		static const FName NativeParentClassTag(TEXT("NativeParentClass"));
		FString VehicleWheelClassName = FString::Printf(TEXT("%s'%s'"), *UClass::StaticClass()->GetName(), *UVehicleWheel::StaticClass()->GetPathName());

		if (AssetData.GetTagValueRef<FString>(NativeParentClassTag) == VehicleWheelClassName)
		{
			UBlueprint* Blueprint = Cast<UBlueprint>(AssetData.GetAsset());
			if (Blueprint != nullptr)
			{
				UClass* WheelClass = Blueprint->GeneratedClass;
				UVehicleWheel* WheelCDO = WheelClass->GetDefaultObject<UVehicleWheel>();

				UTireType* WheelTireType = WheelCDO->TireType;
				UTireConfig** TireConfigPtr = TireTypeToConfigMap.Find(WheelTireType);
				if (TireConfigPtr)
				{
					Blueprint->Modify();
					WheelCDO->TireConfig = *TireConfigPtr;
				}

				// Clear old TireType pointer
				WheelCDO->TireType = nullptr;
			}
		}
	}
}

FAutoConsoleCommand ConvertTireTypesEditorCommand(
	TEXT("ConvertTireTypes"),
	TEXT("Convert legacy TireTypes to new TireConfigs"),
	FConsoleCommandDelegate::CreateStatic(&ConvertTireTypes)
);
