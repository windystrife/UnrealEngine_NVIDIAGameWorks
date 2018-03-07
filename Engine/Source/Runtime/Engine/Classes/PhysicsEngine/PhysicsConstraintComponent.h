// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "Components/SceneComponent.h"
#include "PhysicsEngine/ConstraintInstance.h"
#include "PhysicsConstraintComponent.generated.h"

class AActor;
class UPrimitiveComponent;
struct FBodyInstance;

/**
 *	This is effectively a joint that allows you to connect 2 rigid bodies together. You can create different types of joints using the various parameters of this component.
 */
UCLASS(ClassGroup=Physics, meta=(BlueprintSpawnableComponent), HideCategories=(Activation,"Components|Activation", Physics, Mobility), ShowCategories=("Physics|Components|PhysicsConstraint"))
class ENGINE_API UPhysicsConstraintComponent : public USceneComponent
{
	GENERATED_UCLASS_BODY()

	/** Pointer to first Actor to constrain.  */
	UPROPERTY(EditInstanceOnly, Category=Constraint)
	AActor* ConstraintActor1;

	/** 
	 *	Name of first component property to constrain. If Actor1 is NULL, will look within Owner.
	 *	If this is NULL, will use RootComponent of Actor1
	 */
	UPROPERTY(EditAnywhere, Category=Constraint)
	FConstrainComponentPropName ComponentName1;


	/** Pointer to second Actor to constrain. */
	UPROPERTY(EditInstanceOnly, Category=Constraint)
	AActor* ConstraintActor2;

	/** 
	 *	Name of second component property to constrain. If Actor2 is NULL, will look within Owner. 
	 *	If this is NULL, will use RootComponent of Actor2
	 */
	UPROPERTY(EditAnywhere, Category=Constraint)
	FConstrainComponentPropName ComponentName2;


	/** Allows direct setting of first component to constraint. */
	TWeakObjectPtr<UPrimitiveComponent> OverrideComponent1;

	/** Allows direct setting of second component to constraint. */
	TWeakObjectPtr<UPrimitiveComponent> OverrideComponent2;


	UPROPERTY(instanced)
	class UPhysicsConstraintTemplate* ConstraintSetup_DEPRECATED;

	/** Notification when constraint is broken. */
	UPROPERTY(BlueprintAssignable)
	FConstraintBrokenSignature OnConstraintBroken;

public:
	/** All constraint settings */
	UPROPERTY(EditAnywhere, Category=ConstraintComponent, meta=(ShowOnlyInnerProperties))
	FConstraintInstance			ConstraintInstance;

	//Begin UObject Interface
	virtual void BeginDestroy() override;
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	//End UObject interface

	//Begin ActorComponent interface
#if WITH_EDITOR
	virtual void CheckForErrors() override;
	virtual void OnRegister() override;
#endif // WITH_EDITOR
	virtual void OnUnregister() override;
	virtual void InitializeComponent() override;
	//End ActorComponent interface

	//~ Begin SceneComponent Interface
#if WITH_EDITOR
	virtual void PostEditComponentMove(bool bFinished) override;
#endif // WITH_EDITOR
	//~ End SceneComponent Interface

	/** Get the body frame. Works without constraint being created */
	FTransform GetBodyTransform(EConstraintFrame::Type Frame) const;
	
	/** Get body bounding box. Works without constraint being created */
	FBox GetBodyBox(EConstraintFrame::Type Frame) const;

	/** Initialize the frames and creates constraint */
	void InitComponentConstraint();

	/** Break the constraint */
	void TermComponentConstraint();

	/** Directly specify component to connect. Will update frames based on current position. */
	UFUNCTION(BlueprintCallable, Category="Physics|Components|PhysicsConstraint")
	void SetConstrainedComponents(UPrimitiveComponent* Component1, FName BoneName1, UPrimitiveComponent* Component2, FName BoneName2);

	/** Break this constraint */
	UFUNCTION(BlueprintCallable, Category="Physics|Components|PhysicsConstraint")
	void BreakConstraint();

	/** Enables/Disables linear position drive 
	 *	
	 *	@param bEnableDriveX	Indicates whether the drive for the X-Axis should be enabled
	 *	@param bEnableDriveY	Indicates whether the drive for the Y-Axis should be enabled
	 *	@param bEnableDriveZ	Indicates whether the drive for the Z-Axis should be enabled
	 */
	UFUNCTION(BlueprintCallable, Category="Physics|Components|PhysicsConstraint")
	void SetLinearPositionDrive(bool bEnableDriveX, bool bEnableDriveY, bool bEnableDriveZ);

	/** Enables/Disables linear position drive 
	 *	
	 *	@param bEnableDriveX	Indicates whether the drive for the X-Axis should be enabled
	 *	@param bEnableDriveY	Indicates whether the drive for the Y-Axis should be enabled
	 *	@param bEnableDriveZ	Indicates whether the drive for the Z-Axis should be enabled
	 */
	UFUNCTION(BlueprintCallable, Category="Physics|Components|PhysicsConstraint")
	void SetLinearVelocityDrive(bool bEnableDriveX, bool bEnableDriveY, bool bEnableDriveZ);

	/** Enables/Disables angular orientation drive. Only relevant if the AngularDriveMode is set to Twist and Swing 
	 *	
	 *	@param bEnableSwingDrive	Indicates whether the drive for the swing axis should be enabled. Only relevant if the AngularDriveMode is set to Twist and Swing
	 *	@param bEnableTwistDrive	Indicates whether the drive for the twist axis should be enabled. Only relevant if the AngularDriveMode is set to Twist and Swing
	 */
	UFUNCTION(BlueprintCallable, Category="Physics|Components|PhysicsConstraint", meta=(DeprecatedFunction, DeprecationMessage = "Use SetOrientationDriveTwistAndSwing instead."))
	void SetAngularOrientationDrive(bool bEnableSwingDrive, bool bEnableTwistDrive)
	{
		SetOrientationDriveTwistAndSwing(bEnableTwistDrive, bEnableSwingDrive);
	}
	/** Enables/Disables angular orientation drive. Only relevant if the AngularDriveMode is set to Twist and Swing
	*
	*	@param bEnableSwingDrive	Indicates whether the drive for the swing axis should be enabled. Only relevant if the AngularDriveMode is set to Twist and Swing
	*	@param bEnableTwistDrive	Indicates whether the drive for the twist axis should be enabled. Only relevant if the AngularDriveMode is set to Twist and Swing
	*/
	UFUNCTION(BlueprintCallable, Category = "Physics|Components|PhysicsConstraint")
	void SetOrientationDriveTwistAndSwing(bool bEnableTwistDrive, bool bEnableSwingDrive);

	/** Enables/Disables the angular orientation slerp drive. Only relevant if the AngularDriveMode is set to SLERP
	*
	*	@param bEnableSLERP     	Indicates whether the SLERP drive should be enabled. Only relevant if the AngularDriveMode is set to SLERP
	*/
	UFUNCTION(BlueprintCallable, Category = "Physics|Components|PhysicsConstraint")
	void SetOrientationDriveSLERP(bool bEnableSLERP);

	UFUNCTION(BlueprintCallable, Category="Physics|Components|PhysicsConstraint", meta = (DeprecatedFunction, DeprecationMessage = "Use SetAngularVelocityDriveTwistAndSwing instead."))
	void SetAngularVelocityDrive(bool bEnableSwingDrive, bool bEnableTwistDrive)
	{
		SetAngularVelocityDriveTwistAndSwing(bEnableTwistDrive, bEnableSwingDrive);
	}

	/** Enables/Disables angular velocity twist and swing drive. Only relevant if the AngularDriveMode is set to Twist and Swing
	*
	*	@param bEnableSwingDrive	Indicates whether the drive for the swing axis should be enabled. Only relevant if the AngularDriveMode is set to Twist and Swing
	*	@param bEnableTwistDrive	Indicates whether the drive for the twist axis should be enabled. Only relevant if the AngularDriveMode is set to Twist and Swing
	*/
	UFUNCTION(BlueprintCallable, Category = "Physics|Components|PhysicsConstraint")
	void SetAngularVelocityDriveTwistAndSwing(bool bEnableTwistDrive, bool bEnableSwingDrive);

	/** Enables/Disables the angular velocity slerp drive. Only relevant if the AngularDriveMode is set to SLERP
	*
	*	@param bEnableSLERP     	Indicates whether the SLERP drive should be enabled. Only relevant if the AngularDriveMode is set to SLERP
	*/
	UFUNCTION(BlueprintCallable, Category = "Physics|Components|PhysicsConstraint")
	void SetAngularVelocityDriveSLERP(bool bEnableSLERP);

	/** Switches the angular drive mode between SLERP and Twist And Swing
	*
	*	@param DriveMode	The angular drive mode to use. SLERP uses shortest spherical path, but will not work if any angular constraints are locked. Twist and Swing decomposes the path into the different angular degrees of freedom but may experience gimbal lock
	*/
	UFUNCTION(BlueprintCallable, Category = "Physics|Components|PhysicsConstraint")
	void SetAngularDriveMode(EAngularDriveMode::Type DriveMode);

	/** Sets the target position for the linear drive. 
	 *	@param InPosTarget		Target position
	 */
	UFUNCTION(BlueprintCallable, Category="Physics|Components|PhysicsConstraint")
	void SetLinearPositionTarget(const FVector& InPosTarget);
	
	/** Sets the target velocity for the linear drive. 
	 *	@param InVelTarget		Target velocity
	 */
	UFUNCTION(BlueprintCallable, Category="Physics|Components|PhysicsConstraint")
	void SetLinearVelocityTarget(const FVector& InVelTarget);

	/** Sets the drive params for the linear drive. 
	 *	@param PositionStrength		Positional strength for the drive (stiffness)
	 *	@param VelocityStrength 	Velocity strength of the drive (damping)
	 *	@param InForceLimit	Max force applied by the drive
	 */
	UFUNCTION(BlueprintCallable, Category="Physics|Components|PhysicsConstraint")
	void SetLinearDriveParams(float PositionStrength, float VelocityStrength, float InForceLimit);

	/** Sets the target orientation for the angular drive. 
	 *	@param InPosTarget		Target orientation
	 */
	UFUNCTION(BlueprintCallable, Category="Physics|Components|PhysicsConstraint")
	void SetAngularOrientationTarget(const FRotator& InPosTarget);

	
	/** Sets the target velocity for the angular drive. 
	 *	@param InVelTarget		Target velocity
	 */
	UFUNCTION(BlueprintCallable, Category="Physics|Components|PhysicsConstraint")
	void SetAngularVelocityTarget(const FVector& InVelTarget);

	/** Sets the drive params for the angular drive. 
	 *	@param PositionStrength		Positional strength for the drive (stiffness)
	 *	@param VelocityStrength 	Velocity strength of the drive (damping)
	 *	@param InForceLimit	Max force applied by the drive
	 */
	UFUNCTION(BlueprintCallable, Category="Physics|Components|PhysicsConstraint")
	void SetAngularDriveParams(float PositionStrength, float VelocityStrength, float InForceLimit);


	/** Sets the LinearX Motion Type
	*	@param ConstraintType	New Constraint Type
	*	@param LimitSize		Size of limit
	*/
	UFUNCTION(BlueprintCallable, Category = "Physics|Components|PhysicsConstraint")
	void SetLinearXLimit(ELinearConstraintMotion ConstraintType, float LimitSize);

	/** Sets the LinearY Motion Type
	*	@param ConstraintType	New Constraint Type
	*	@param LimitSize		Size of limit
	*/
	UFUNCTION(BlueprintCallable, Category = "Physics|Components|PhysicsConstraint")
	void SetLinearYLimit(ELinearConstraintMotion ConstraintType, float LimitSize);

	/** Sets the LinearZ Motion Type
	*	@param ConstraintType	New Constraint Type
	*	@param LimitSize		Size of limit
	*/
	UFUNCTION(BlueprintCallable, Category = "Physics|Components|PhysicsConstraint")
	void SetLinearZLimit(ELinearConstraintMotion ConstraintType, float LimitSize);

	/** Sets the Angular Swing1 Motion Type
	*	@param ConstraintType	New Constraint Type
	*	@param Swing1LimitAngle	Size of limit in degrees
	*/
	UFUNCTION(BlueprintCallable, Category = "Physics|Components|PhysicsConstraint")
	void SetAngularSwing1Limit(EAngularConstraintMotion MotionType, float Swing1LimitAngle);

	/** Sets the Angular Swing2 Motion Type
	*	@param ConstraintType	New Constraint Type
	*	@param Swing2LimitAngle	Size of limit in degrees
	*/
	UFUNCTION(BlueprintCallable, Category = "Physics|Components|PhysicsConstraint")
	void SetAngularSwing2Limit(EAngularConstraintMotion MotionType, float Swing2LimitAngle);

	/** Sets the Angular Twist Motion Type
	*	@param ConstraintType	New Constraint Type
	*	@param TwistLimitAngle	Size of limit in degrees
	*/
	UFUNCTION(BlueprintCallable, Category = "Physics|Components|PhysicsConstraint")
	void SetAngularTwistLimit(EAngularConstraintMotion ConstraintType, float TwistLimitAngle);

	/** Sets the Linear Breakable properties
	*	@param bLinearBreakable		Whether it is possible to break the joint with linear force
	*	@param LinearBreakThreshold	Force needed to break the joint
	*/
	UFUNCTION(BlueprintCallable, Category = "Physics|Components|PhysicsConstraint")
	void SetLinearBreakable(bool bLinearBreakable, float LinearBreakThreshold);

	/** Sets the Angular Breakable properties
	*	@param bAngularBreakable		Whether it is possible to break the joint with angular force
	*	@param AngularBreakThreshold	Torque needed to break the joint
	*/
	UFUNCTION(BlueprintCallable, Category = "Physics|Components|PhysicsConstraint")
	void SetAngularBreakable(bool bAngularBreakable, float AngularBreakThreshold);

	/** Gets the current Angular Twist of the constraint */
	UFUNCTION(BlueprintCallable, Category = "Physics|Components|PhysicsConstraint")
	float GetCurrentTwist() const;

	/** Gets the current Swing1 of the constraint */
	UFUNCTION(BlueprintCallable, Category = "Physics|Components|PhysicsConstraint")
	float GetCurrentSwing1() const;

	/** Gets the current Swing2 of the constraint */
	UFUNCTION(BlueprintCallable, Category = "Physics|Components|PhysicsConstraint")
	float GetCurrentSwing2() const;

	/** 
	 *	Update the reference frames held inside the constraint that indicate the joint location in the reference frame 
	 *	of the two connected bodies. You should call this whenever the constraint or either Component moves, or if you change
	 *	the connected Components. This function does nothing though once the joint has been initialized.
	 */
	void UpdateConstraintFrames();

	// Pass in reference frame in. If the constraint is currently active, this will set its active local pose. Otherwise the change will take affect in InitConstraint. 
	UFUNCTION(BlueprintCallable, Category = "Physics|Components|PhysicsConstraint")
	void SetConstraintReferenceFrame(EConstraintFrame::Type Frame, const FTransform& RefFrame);
	
	// Pass in reference position in (maintains reference orientation). If the constraint is currently active, this will set its active local pose. Otherwise the change will take affect in InitConstraint.
	UFUNCTION(BlueprintCallable, Category = "Physics|Components|PhysicsConstraint")
	void SetConstraintReferencePosition(EConstraintFrame::Type Frame, const FVector& RefPosition);
	
	// Pass in reference orientation in (maintains reference position). If the constraint is currently active, this will set its active local pose. Otherwise the change will take affect in InitConstraint.
	UFUNCTION(BlueprintCallable, Category = "Physics|Components|PhysicsConstraint")
	void SetConstraintReferenceOrientation(EConstraintFrame::Type Frame, const FVector& PriAxis, const FVector& SecAxis);

	// If true, the collision between the two rigid bodies of the constraint will be disabled.
	UFUNCTION(BlueprintCallable, Category = "Physics|Components|PhysicsConstraint")
	void SetDisableCollision(bool bDisableCollision);
	
	// Retrieve the constraint force most recently applied to maintain this constraint. Returns 0 forces if the constraint is not initialized or broken.
	UFUNCTION(BlueprintCallable, Category = "Physics|Components|PhysicsConstraint")
	void GetConstraintForce(FVector& OutLinearForce, FVector& OutAngularForce);

	// Retrieve the status of constraint being broken.
	UFUNCTION(BlueprintCallable, Category = "Physics|Components|PhysicsConstraint")
	bool IsBroken();

#if WITH_EDITOR
	void UpdateSpriteTexture();
#endif

protected:

	friend class FConstraintComponentVisualizer;

	/** Get the body instance that we want to constrain to */
	FBodyInstance* GetBodyInstance(EConstraintFrame::Type Frame) const;

	/** Internal util to get body transform from actor/component name/bone name information */
	FTransform GetBodyTransformInternal(EConstraintFrame::Type Frame, FName InBoneName) const;
	/** Internal util to get body box from actor/component name/bone name information */
	FBox GetBodyBoxInternal(EConstraintFrame::Type Frame, FName InBoneName) const;
	/** Internal util to get component from actor/component name */
	UPrimitiveComponent* GetComponentInternal(EConstraintFrame::Type Frame) const;

	/** Routes the FConstraint callback to the dynamic delegate */
	void OnConstraintBrokenHandler(FConstraintInstance* BrokenConstraint);

	/** Returns the scale of the constraint as it will be passed into the ConstraintInstance*/
	float GetConstraintScale() const;

private:
	/** Wrapper that calls our constraint broken delegate */
	void OnConstraintBrokenWrapper(int32 ConstraintIndex);
};

