#include "FlowGridActor.h"

#include "NvFlowCommon.h"
#include "FlowGridAsset.h"
#include "FlowGridComponent.h"

/*=============================================================================
	FlowGridActor.cpp: AFlowGridActor methods.
=============================================================================*/

// NvFlow begin

#include "UObject/ConstructorHelpers.h"

#include "Engine/Texture2D.h"
#include "Components/BillboardComponent.h"
#include "PhysicsEngine/PhysXSupport.h"

AFlowGridActor::AFlowGridActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	FlowGridComponent = ObjectInitializer.CreateAbstractDefaultSubobject<UFlowGridComponent>(this, TEXT("FlowGridComponent0"));
	RootComponent = FlowGridComponent;

#if WITH_EDITORONLY_DATA
	SpriteComponent = ObjectInitializer.CreateEditorOnlyDefaultSubobject<UBillboardComponent>(this, TEXT("Sprite"));

	if (!IsRunningCommandlet() && (SpriteComponent != nullptr))
	{
		// Structure to hold one-time initialization
		struct FConstructorStatics
		{
			ConstructorHelpers::FObjectFinderOptional<UTexture2D> EffectsTextureObject;
			FName ID_Effects;
			FText NAME_Effects;
			FConstructorStatics()
				: EffectsTextureObject(TEXT("/Engine/EditorResources/S_VectorFieldVol"))
				, ID_Effects(TEXT("Effects"))
				, NAME_Effects(NSLOCTEXT("SpriteCategory", "Effects", "Effects"))
			{
			}
		};
		static FConstructorStatics ConstructorStatics;

		SpriteComponent->Sprite = ConstructorStatics.EffectsTextureObject.Get();
		SpriteComponent->bIsScreenSizeScaled = true;
		SpriteComponent->SpriteInfo.Category = ConstructorStatics.ID_Effects;
		SpriteComponent->SpriteInfo.DisplayName = ConstructorStatics.NAME_Effects;
		SpriteComponent->bAbsoluteScale = true;
		//SpriteComponent->AttachParent = FlowGridComponent;
		SpriteComponent->SetupAttachment(FlowGridComponent);
		SpriteComponent->bReceivesDecals = false;
	}
#endif // WITH_EDITORONLY_DATA
}

// NvFlow end
