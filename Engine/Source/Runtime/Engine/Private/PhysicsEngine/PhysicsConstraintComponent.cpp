// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Texture2D.h"
#include "Logging/TokenizedMessage.h"
#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/ConstraintUtils.h"
#include "Components/BillboardComponent.h"

#define LOCTEXT_NAMESPACE "ConstraintComponent"

UPhysicsConstraintComponent::UPhysicsConstraintComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bWantsInitializeComponent = true;
#if WITH_EDITORONLY_DATA
	bVisualizeComponent = true;
#endif
}



UPrimitiveComponent* UPhysicsConstraintComponent::GetComponentInternal(EConstraintFrame::Type Frame) const
{
	UPrimitiveComponent* PrimComp = NULL;

	FName ComponentName = NAME_None;
	AActor* Actor = NULL;

	// Frame 1
	if(Frame == EConstraintFrame::Frame1)
	{
		// Use override component if specified
		if(OverrideComponent1.IsValid())
		{
			return OverrideComponent1.Get();
		}

		ComponentName = ComponentName1.ComponentName;
		Actor = ConstraintActor1;
	}
	// Frame 2
	else
	{
		// Use override component if specified
		if(OverrideComponent2.IsValid())
		{
			return OverrideComponent2.Get();
		}

		ComponentName = ComponentName2.ComponentName;
		Actor = ConstraintActor2;
	}

	// If neither actor nor component name specified, joint to 'world'
	if(Actor != NULL || ComponentName != NAME_None)
	{
		// If no actor specified, but component name is - use Owner
		if(Actor == NULL)
		{
			Actor = GetOwner();
		}

		// If we now have an Actor, lets find a component
		if(Actor != NULL)
		{
			// No name specified, use the root component
			if(ComponentName == NAME_None)
			{
				PrimComp = Cast<UPrimitiveComponent>(Actor->GetRootComponent());
			}
			// Name specified, see if we can find that component..
			else
			{
				for(UActorComponent* Comp : Actor->GetComponents())
				{
					if(Comp->GetFName() == ComponentName)
					{
						if (UChildActorComponent* ChildActorComp = Cast<UChildActorComponent>(Comp))
						{
							if (AActor* ChildActor = ChildActorComp->GetChildActor())
							{
								PrimComp = Cast<UPrimitiveComponent>(ChildActor->GetRootComponent());
							}
						}
						else
						{
							PrimComp = Cast<UPrimitiveComponent>(Comp);
						}
						break;
					}
				}
			}
		}	
	}

	return PrimComp;
}

//Helps to find bone index if bone specified, otherwise find bone index of root body
int32 GetBoneIndexHelper(FName InBoneName, const USkeletalMeshComponent& SkelComp, int32* BodyIndex = nullptr)
{
	FName BoneName = InBoneName;
	const UPhysicsAsset* PhysAsset = SkelComp.GetPhysicsAsset();
	
	if(BoneName == NAME_None)
	{
		//Didn't specify bone name so just use root body
		if (PhysAsset)
		{			
			const int32 RootBodyIndex = SkelComp.FindRootBodyIndex();
			if (PhysAsset->SkeletalBodySetups.IsValidIndex(RootBodyIndex))
			{
				BoneName = PhysAsset->SkeletalBodySetups[RootBodyIndex]->BoneName;
			}
		}
	}

	if(BodyIndex)
	{
		*BodyIndex = PhysAsset ? PhysAsset->FindBodyIndex(BoneName) : INDEX_NONE;
	}

	return SkelComp.GetBoneIndex(BoneName);
}

FTransform UPhysicsConstraintComponent::GetBodyTransformInternal(EConstraintFrame::Type Frame, FName InBoneName) const
{
	UPrimitiveComponent* PrimComp = GetComponentInternal(Frame);
	if(!PrimComp)
	{
		return FTransform::Identity;
	}
	  
	//Use GetComponentTransform() by default for all components
	FTransform ResultTM = PrimComp->GetComponentTransform();
		
	// Skeletal case
	if(const USkeletalMeshComponent* SkelComp = Cast<USkeletalMeshComponent>(PrimComp))
	{
		const int32 BoneIndex = GetBoneIndexHelper(InBoneName, *SkelComp);
		if (BoneIndex != INDEX_NONE)
		{
			ResultTM = SkelComp->GetBoneTransform(BoneIndex);
		}
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		else
		{
			FMessageLog("PIE").Warning(FText::Format(LOCTEXT("BadBoneNameToConstraint", "Couldn't find bone {0} for ConstraintComponent {1}."),
				FText::FromName(InBoneName), FText::FromString(GetPathNameSafe(this))));
		}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	}

	return ResultTM;
}

FBox UPhysicsConstraintComponent::GetBodyBoxInternal(EConstraintFrame::Type Frame, FName InBoneName) const
{
	FBox ResultBox(ForceInit);

	UPrimitiveComponent* PrimComp  = GetComponentInternal(Frame);

	// Skeletal case
	if(const USkeletalMeshComponent* SkelComp = Cast<USkeletalMeshComponent>(PrimComp))
	{
		if (const UPhysicsAsset* PhysicsAsset = SkelComp->GetPhysicsAsset())
		{
			int32 BodyIndex;
			const int32 BoneIndex = GetBoneIndexHelper(InBoneName, *SkelComp, &BodyIndex);
			if(BoneIndex != INDEX_NONE && BodyIndex != INDEX_NONE)
			{	
				const FTransform BoneTransform = SkelComp->GetBoneTransform(BoneIndex);
				ResultBox = PhysicsAsset->SkeletalBodySetups[BodyIndex]->AggGeom.CalcAABB(BoneTransform);
			}
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			else
			{
				FMessageLog("PIE").Warning(FText::Format(LOCTEXT("BadBoneNameToConstraint2", "Couldn't find bone {0} for ConstraintComponent {1}."),
					FText::FromName(InBoneName), FText::FromString(GetPathNameSafe(this))));
			}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		}
	}
	else if(PrimComp != NULL)
	{
		ResultBox = PrimComp->Bounds.GetBox();
	}

	return ResultBox;
}

FTransform UPhysicsConstraintComponent::GetBodyTransform(EConstraintFrame::Type Frame) const
{
	if(Frame == EConstraintFrame::Frame1)
	{
		return GetBodyTransformInternal(Frame, ConstraintInstance.ConstraintBone1);
	}
	else
	{
		return GetBodyTransformInternal(Frame, ConstraintInstance.ConstraintBone2);
	}
}

FBox UPhysicsConstraintComponent::GetBodyBox(EConstraintFrame::Type Frame) const
{
	if(Frame == EConstraintFrame::Frame1)
	{
		return GetBodyBoxInternal(Frame, ConstraintInstance.ConstraintBone1);
	}
	else
	{
		return GetBodyBoxInternal(Frame, ConstraintInstance.ConstraintBone2);
	}
}

FBodyInstance* UPhysicsConstraintComponent::GetBodyInstance(EConstraintFrame::Type Frame) const
{
	FBodyInstance* Instance = NULL;
	UPrimitiveComponent* PrimComp = GetComponentInternal(Frame);
	if(PrimComp != NULL)
	{
		if(Frame == EConstraintFrame::Frame1)
		{
			Instance = PrimComp->GetBodyInstance(ConstraintInstance.ConstraintBone1);
		}
		else
		{
			Instance = PrimComp->GetBodyInstance(ConstraintInstance.ConstraintBone2);
		}
	}
	return Instance;
}


/** Wrapper that calls our constraint broken delegate */
void UPhysicsConstraintComponent::OnConstraintBrokenWrapper(int32 ConstraintIndex)
{
	OnConstraintBroken.Broadcast(ConstraintIndex);
}

void UPhysicsConstraintComponent::InitComponentConstraint()
{
	// First we convert world space position of constraint into local space frames
	UpdateConstraintFrames();

	// Then we init the constraint
	FBodyInstance* Body1 = GetBodyInstance(EConstraintFrame::Frame1);
	FBodyInstance* Body2 = GetBodyInstance(EConstraintFrame::Frame2);

	if (Body1 != nullptr || Body2 != nullptr)
	{
		ConstraintInstance.InitConstraint(Body1, Body2, GetConstraintScale(), this, FOnConstraintBroken::CreateUObject(this, &UPhysicsConstraintComponent::OnConstraintBrokenWrapper));
	}
}

void UPhysicsConstraintComponent::TermComponentConstraint()
{
	ConstraintInstance.TermConstraint();
}

void UPhysicsConstraintComponent::OnConstraintBrokenHandler(FConstraintInstance* BrokenConstraint)
{
	OnConstraintBroken.Broadcast(BrokenConstraint->ConstraintIndex);
}

float UPhysicsConstraintComponent::GetConstraintScale() const
{
	return GetComponentScale().GetAbsMin();
}

void UPhysicsConstraintComponent::SetConstrainedComponents(UPrimitiveComponent* Component1, FName BoneName1, UPrimitiveComponent* Component2, FName BoneName2)
{
	if(Component1 != NULL)
	{
		this->ComponentName1.ComponentName = Component1->GetFName();
		OverrideComponent1 = Component1;
		ConstraintInstance.ConstraintBone1 = BoneName1;
	}

	if(Component2 != NULL)
	{
		this->ComponentName2.ComponentName = Component2->GetFName();
		OverrideComponent2 = Component2;
		ConstraintInstance.ConstraintBone2 = BoneName2;
	}

	InitComponentConstraint();
}

void UPhysicsConstraintComponent::BreakConstraint()
{
	ConstraintInstance.TermConstraint();
}


void UPhysicsConstraintComponent::InitializeComponent()
{
	Super::InitializeComponent();
	InitComponentConstraint();
}

#if WITH_EDITOR
void UPhysicsConstraintComponent::OnRegister()
{
	Super::OnRegister();

	if (SpriteComponent)
	{
		UpdateSpriteTexture();
		SpriteComponent->SpriteInfo.Category = TEXT("Physics");
		SpriteComponent->SpriteInfo.DisplayName = NSLOCTEXT( "SpriteCategory", "Physics", "Physics" );
	}
}
#endif

void UPhysicsConstraintComponent::OnUnregister()
{
	Super::OnUnregister();

	// Slight hack - there isn't an EndPlayComponent, so we see if we are unregistered and we have an owner but its gone, and if so, we shut down constraint
	if(GetOwner() && GetOwner()->IsPendingKillPending())
	{
		TermComponentConstraint();
	}
}

void UPhysicsConstraintComponent::BeginDestroy()
{
	Super::BeginDestroy();
	TermComponentConstraint();
}

void UPhysicsConstraintComponent::PostLoad()
{
	Super::PostLoad();

	// Fix old content that used a ConstraintSetup
	if ( GetLinkerUE4Version() < VER_UE4_ALL_PROPS_TO_CONSTRAINTINSTANCE && (ConstraintSetup_DEPRECATED != NULL) )
	{
		// Will have copied from setup into DefaultIntance inside
		ConstraintInstance.CopyConstraintParamsFrom(&ConstraintSetup_DEPRECATED->DefaultInstance);
		ConstraintSetup_DEPRECATED = NULL;
	}

	if (GetLinkerUE4Version() < VER_UE4_SOFT_CONSTRAINTS_USE_MASS)
	{
		//In previous versions the mass was placed into the spring constant. This is correct because you use different springs for different mass - however, this makes tuning hard
		//We now multiply mass into the spring constant. To fix old data we use CalculateMass which is not perfect but close (within 0.1kg)
		//We also use the primitive body instance directly for determining if simulated - this is potentially wrong for fixed bones in skeletal mesh, but it's much more likely right (in skeletal case we don't have access to bodies to check)
		
		UPrimitiveComponent * Primitive1 = GetComponentInternal(EConstraintFrame::Frame1);
		UPrimitiveComponent * Primitive2 = GetComponentInternal(EConstraintFrame::Frame2);
		
		int NumDynamic = 0;
		float TotalMass = 0.f;

		if (Primitive1 && Primitive1->BodyInstance.bSimulatePhysics)
		{
			FName BoneName = ConstraintInstance.ConstraintBone1;
			++NumDynamic;
			TotalMass += Primitive1->CalculateMass(BoneName);
		}

		if (Primitive2 && Primitive2->BodyInstance.bSimulatePhysics)
		{
			FName BoneName = ConstraintInstance.ConstraintBone2;
			++NumDynamic;
			TotalMass += Primitive2->CalculateMass(BoneName);
		}

		if ( (NumDynamic > 0) && (TotalMass > 0) )	//we don't support cases where both constrained bodies are static or NULL, but add this anyway to avoid crash
		{
			float AverageMass = TotalMass / NumDynamic;

#if WITH_EDITORONLY_DATA
			ConstraintInstance.ProfileInstance.LinearLimit.Stiffness /= AverageMass;
			ConstraintInstance.SwingLimitStiffness_DEPRECATED /= AverageMass;
			ConstraintInstance.TwistLimitStiffness_DEPRECATED /= AverageMass;
			ConstraintInstance.LinearLimitDamping_DEPRECATED /= AverageMass;
			ConstraintInstance.SwingLimitDamping_DEPRECATED /= AverageMass;
			ConstraintInstance.TwistLimitDamping_DEPRECATED /= AverageMass;
			
#endif
		}

	}
}

#if WITH_EDITOR
void UPhysicsConstraintComponent::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	ConstraintInstance.ProfileInstance.SyncChangedConstraintProperties(PropertyChangedEvent);
	Super::PostEditChangeChainProperty(PropertyChangedEvent);
}

void UPhysicsConstraintComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	UpdateConstraintFrames();
	UpdateSpriteTexture();
}

void UPhysicsConstraintComponent::PostEditComponentMove(bool bFinished)
{
	Super::PostEditComponentMove(bFinished);

	// Update frames
	UpdateConstraintFrames();
}

void UPhysicsConstraintComponent::CheckForErrors()
{
	Super::CheckForErrors();

	UPrimitiveComponent* PrimComp1 = GetComponentInternal(EConstraintFrame::Frame1);
	UPrimitiveComponent* PrimComp2 = GetComponentInternal(EConstraintFrame::Frame2);

	// Check we have something to joint
	if( PrimComp1 == NULL && PrimComp2 == NULL )
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("OwnerName"), FText::FromString(GetNameSafe(GetOwner())));
		FMessageLog("MapCheck").Warning()
			->AddToken(FUObjectToken::Create(this))
			->AddToken(FTextToken::Create(FText::Format( LOCTEXT("NoComponentsFound","{OwnerName} : No components found to joint."), Arguments ) ));
	}
	// Make sure constraint components are not both static.
	else if ( PrimComp1 != NULL && PrimComp2 != NULL )
	{
		if ( PrimComp1->Mobility != EComponentMobility::Movable && PrimComp2->Mobility != EComponentMobility::Movable )
		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("OwnerName"), FText::FromString(GetNameSafe(GetOwner())));
			FMessageLog("MapCheck").Warning()
				->AddToken(FUObjectToken::Create(this))
				->AddToken(FTextToken::Create(FText::Format( LOCTEXT("BothComponentsStatic","{OwnerName} : Both components are static."), Arguments ) ));
		}
	}
	else
	{
		// At this point, we know one constraint component is NULL and the other is non-NULL.
		// Check that the non-NULL constraint component is dynamic.
		if ( ( PrimComp1 == NULL && PrimComp2->Mobility != EComponentMobility::Movable ) ||
			( PrimComp2 == NULL && PrimComp1->Mobility != EComponentMobility::Movable ) )
		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("OwnerName"), FText::FromString(GetNameSafe(GetOwner())));
			FMessageLog("MapCheck").Warning()
				->AddToken(FUObjectToken::Create(this))
				->AddToken(FTextToken::Create(FText::Format( LOCTEXT("SingleStaticComponent","{OwnerName} : Connected to single static component."), Arguments ) ));
		}
	}
}

#endif // WITH_EDITOR

void UPhysicsConstraintComponent::UpdateConstraintFrames()
{
	FTransform A1Transform = GetBodyTransform(EConstraintFrame::Frame1);
	A1Transform.RemoveScaling();

	FTransform A2Transform = GetBodyTransform(EConstraintFrame::Frame2);
	A2Transform.RemoveScaling();

	// World ref frame
	const FVector WPos = GetComponentLocation();
	const FVector WPri = GetComponentTransform().GetUnitAxis( EAxis::X );
	const FVector WOrth = GetComponentTransform().GetUnitAxis( EAxis::Y );

	ConstraintInstance.Pos1 = A1Transform.InverseTransformPosition(WPos);
	ConstraintInstance.PriAxis1 = A1Transform.InverseTransformVectorNoScale(WPri);
	ConstraintInstance.SecAxis1 = A1Transform.InverseTransformVectorNoScale(WOrth);

	const FVector RotatedX = ConstraintInstance.AngularRotationOffset.RotateVector(FVector(1,0,0));
	const FVector RotatedY = ConstraintInstance.AngularRotationOffset.RotateVector(FVector(0,1,0));
	const FVector WPri2 = GetComponentTransform().TransformVectorNoScale(RotatedX);
	const FVector WOrth2 = GetComponentTransform().TransformVectorNoScale(RotatedY);
	
	
	ConstraintInstance.Pos2 = A2Transform.InverseTransformPosition(WPos);
	ConstraintInstance.PriAxis2 = A2Transform.InverseTransformVectorNoScale(WPri2);
	ConstraintInstance.SecAxis2 = A2Transform.InverseTransformVectorNoScale(WOrth2);

	//Constraint instance is given our reference frame scale and uses it to scale position.
	//Note that the scale passed in is also used for limits, so we first undo the position scale so that it's consistent.

	//Note that in the case where there is no body instance, the position is given in world space and there is no scaling.
	const float RefScale = FMath::Max(GetConstraintScale(), 0.01f);
	if(GetBodyInstance(EConstraintFrame::Frame1))
	{
		ConstraintInstance.Pos1 /= RefScale;
	}

	if (GetBodyInstance(EConstraintFrame::Frame2))
	{
		ConstraintInstance.Pos2 /= RefScale;
	}
}

void UPhysicsConstraintComponent::SetConstraintReferenceFrame(EConstraintFrame::Type Frame, const FTransform& RefFrame)
{
	ConstraintInstance.SetRefFrame(Frame, RefFrame);
}

void UPhysicsConstraintComponent::SetConstraintReferencePosition(EConstraintFrame::Type Frame, const FVector& RefPosition)
{
	ConstraintInstance.SetRefPosition(Frame, RefPosition);
}

void UPhysicsConstraintComponent::SetConstraintReferenceOrientation(EConstraintFrame::Type Frame, const FVector& PriAxis, const FVector& SecAxis)
{
	ConstraintInstance.SetRefOrientation(Frame, PriAxis, SecAxis);
}

void UPhysicsConstraintComponent::GetConstraintForce(FVector& OutLinearForce, FVector& OutAngularForce)
{
	ConstraintInstance.GetConstraintForce(OutLinearForce, OutAngularForce);
}

bool UPhysicsConstraintComponent::IsBroken()
{
	return ConstraintInstance.IsBroken();
}

void UPhysicsConstraintComponent::SetDisableCollision(bool bDisableCollision)
{
	ConstraintInstance.SetDisableCollision(bDisableCollision);
}


#if WITH_EDITOR
void UPhysicsConstraintComponent::UpdateSpriteTexture()
{
	if (SpriteComponent)
	{
		if (ConstraintUtils::IsHinge(ConstraintInstance))
		{
			SpriteComponent->SetSprite(LoadObject<UTexture2D>(NULL, TEXT("/Engine/EditorResources/S_KHinge.S_KHinge")));
		}
		else if (ConstraintUtils::IsPrismatic(ConstraintInstance))
		{
			SpriteComponent->SetSprite(LoadObject<UTexture2D>(NULL, TEXT("/Engine/EditorResources/S_KPrismatic.S_KPrismatic")));
		}
		else
		{
			SpriteComponent->SetSprite(LoadObject<UTexture2D>(NULL, TEXT("/Engine/EditorResources/S_KBSJoint.S_KBSJoint")));
		}
	}
}
#endif // WITH_EDITOR

void UPhysicsConstraintComponent::SetLinearPositionDrive( bool bEnableDriveX, bool bEnableDriveY, bool bEnableDriveZ )
{
	ConstraintInstance.SetLinearPositionDrive(bEnableDriveX, bEnableDriveY, bEnableDriveZ);
}

void UPhysicsConstraintComponent::SetLinearVelocityDrive( bool bEnableDriveX, bool bEnableDriveY, bool bEnableDriveZ )
{
	ConstraintInstance.SetLinearVelocityDrive(bEnableDriveX, bEnableDriveY, bEnableDriveZ);
}

void UPhysicsConstraintComponent::SetOrientationDriveTwistAndSwing( bool bEnableTwistDrive, bool bEnableSwingDrive)
{
	ConstraintInstance.SetOrientationDriveTwistAndSwing(bEnableTwistDrive, bEnableSwingDrive);
}

void UPhysicsConstraintComponent::SetOrientationDriveSLERP(bool bEnableSLERP)
{
	ConstraintInstance.SetOrientationDriveSLERP(bEnableSLERP);
}


void UPhysicsConstraintComponent::SetAngularDriveMode(EAngularDriveMode::Type DriveMode)
{
	ConstraintInstance.SetAngularDriveMode(DriveMode);
}

void UPhysicsConstraintComponent::SetAngularVelocityDriveTwistAndSwing( bool bEnableTwistDrive, bool bEnableSwingDrive)
{
	ConstraintInstance.SetAngularVelocityDriveTwistAndSwing(bEnableTwistDrive, bEnableSwingDrive);
}

void UPhysicsConstraintComponent::SetAngularVelocityDriveSLERP(bool bEnableSLERP)
{
	ConstraintInstance.SetAngularVelocityDriveSLERP(bEnableSLERP);
}


void UPhysicsConstraintComponent::SetLinearPositionTarget( const FVector& InPosTarget )
{
	ConstraintInstance.SetLinearPositionTarget(InPosTarget);
}

void UPhysicsConstraintComponent::SetLinearVelocityTarget( const FVector& InVelTarget )
{
	ConstraintInstance.SetLinearVelocityTarget(InVelTarget);
}

void UPhysicsConstraintComponent::SetLinearDriveParams( float PositionStrength, float VelocityStrength, float InForceLimit )
{
	ConstraintInstance.SetLinearDriveParams(PositionStrength, VelocityStrength, InForceLimit);
}

void UPhysicsConstraintComponent::SetAngularOrientationTarget( const FRotator& InPosTarget )
{
	ConstraintInstance.SetAngularOrientationTarget(InPosTarget.Quaternion());
}

void UPhysicsConstraintComponent::SetAngularVelocityTarget( const FVector& InVelTarget )
{
	ConstraintInstance.SetAngularVelocityTarget(InVelTarget);
}

void UPhysicsConstraintComponent::SetAngularDriveParams( float PositionStrength, float VelocityStrength, float InForceLimit )
{
	ConstraintInstance.SetAngularDriveParams(PositionStrength, VelocityStrength, InForceLimit);
}

void UPhysicsConstraintComponent::SetLinearXLimit(ELinearConstraintMotion Motion, float LinearLimit)
{
	ConstraintInstance.SetLinearXLimit(Motion, LinearLimit);
}

void UPhysicsConstraintComponent::SetLinearYLimit(ELinearConstraintMotion Motion, float LinearLimit)
{
	ConstraintInstance.SetLinearYLimit(Motion, LinearLimit);
}

void UPhysicsConstraintComponent::SetLinearZLimit(ELinearConstraintMotion Motion, float LinearLimit)
{
	ConstraintInstance.SetLinearZLimit(Motion, LinearLimit);
}

void UPhysicsConstraintComponent::SetAngularSwing1Limit(EAngularConstraintMotion Motion, float Swing1LimitAngle)
{
	ConstraintInstance.SetAngularSwing1Limit(Motion, Swing1LimitAngle);
}

void UPhysicsConstraintComponent::SetAngularSwing2Limit(EAngularConstraintMotion Motion, float Swing2LimitAngle)
{
	ConstraintInstance.SetAngularSwing2Limit(Motion, Swing2LimitAngle);
}

void UPhysicsConstraintComponent::SetAngularTwistLimit(EAngularConstraintMotion Motion, float TwistLimitAngle)
{
	ConstraintInstance.SetAngularTwistLimit(Motion, TwistLimitAngle);
}

void UPhysicsConstraintComponent::SetLinearBreakable(bool bLinearBreakable, float LinearBreakThreshold)
{
	ConstraintInstance.SetLinearBreakable(bLinearBreakable, LinearBreakThreshold);
}

void UPhysicsConstraintComponent::SetAngularBreakable(bool bAngularBreakable, float AngularBreakThreshold)
{
	ConstraintInstance.SetAngularBreakable(bAngularBreakable, AngularBreakThreshold);
}

float UPhysicsConstraintComponent::GetCurrentTwist() const
{
	const float CurrentTwistRads = ConstraintInstance.GetCurrentTwist();
	return FMath::RadiansToDegrees(CurrentTwistRads);
}

float UPhysicsConstraintComponent::GetCurrentSwing1() const
{
	const float CurrentSwing1Rads = ConstraintInstance.GetCurrentSwing1();
	return FMath::RadiansToDegrees(CurrentSwing1Rads);
}

float UPhysicsConstraintComponent::GetCurrentSwing2() const
{
	const float CurrentSwing2Rads = ConstraintInstance.GetCurrentSwing2();
	return FMath::RadiansToDegrees(CurrentSwing2Rads);
}

#undef LOCTEXT_NAMESPACE
