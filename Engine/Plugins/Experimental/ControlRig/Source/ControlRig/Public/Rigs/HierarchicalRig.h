// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ControlRig.h"
#include "AnimationHierarchy.h"
#include "BonePose.h"
#include "NodeChain.h"
#include "ControlManipulator.h"
#include "Animation/NodeMappingProviderInterface.h"
#include "Animation/NodeMappingContainer.h"
#include "HierarchicalRig.generated.h"

class USkeletalMesh;

/** ControlRig that handles hierarchical (i.e. node based) data, constraints etc. */
UCLASS(BlueprintType, editinlinenew, meta=(DisplayName="Hierarchical ControlRig"))
class CONTROLRIG_API UHierarchicalRig : public UControlRig, public INodeMappingProviderInterface
{
	GENERATED_BODY()

public:
	UHierarchicalRig();

	/** The skeletal mesh component we process the transforms of */
	UPROPERTY(transient)
	TWeakObjectPtr<USkeletalMeshComponent> SkeletalMeshComponent;

protected:
	/** Constraints to apply */
	UPROPERTY(EditAnywhere, Category = "Constraints")
	TArray<FTransformConstraint> Constraints;

public:
	/** Manipulators used to move inputs in the scene */
	UPROPERTY(EditDefaultsOnly, Instanced, Category = "Manipulators")
	TArray<UControlManipulator*> Manipulators;

	/** Get the local transform of a node */
	UFUNCTION(BlueprintPure, Category = "Hierarchy|Transforms", meta=(AnimationInput, AnimationOutput))
	virtual FTransform GetLocalTransform(FName NodeName) const;

	/** Get the local location of a node */
	UFUNCTION(BlueprintPure, Category = "Hierarchy|Transforms", meta = (AnimationInput, AnimationOutput))
	virtual FVector GetLocalLocation(FName NodeName) const;

	/** Get the local rotation of a node */
	UFUNCTION(BlueprintPure, Category = "Hierarchy|Transforms", meta = (AnimationInput, AnimationOutput))
	virtual FRotator GetLocalRotation(FName NodeName) const;

	/** Get the local scale of a node */
	UFUNCTION(BlueprintPure, Category = "Hierarchy|Transforms", meta = (AnimationInput, AnimationOutput))
	virtual FVector GetLocalScale(FName NodeName) const;

	/** Get the global transform of a node */
	UFUNCTION(BlueprintPure, Category = "Hierarchy|Transforms", meta = (AnimationInput, AnimationOutput))
	virtual FTransform GetGlobalTransform(FName NodeName) const;

	/** Get the global location of a node */
	UFUNCTION(BlueprintPure, Category = "Hierarchy|Transforms", meta = (AnimationInput, AnimationOutput))
	virtual FVector GetGlobalLocation(FName NodeName) const;

	/** Get the global rotation of a node */
	UFUNCTION(BlueprintPure, Category = "Hierarchy|Transforms", meta = (AnimationInput, AnimationOutput))
	virtual FRotator GetGlobalRotation(FName NodeName) const;

	/** Get the global scale of a node */
	UFUNCTION(BlueprintPure, Category = "Hierarchy|Transforms", meta = (AnimationInput, AnimationOutput))
	virtual FVector GetGlobalScale(FName NodeName) const;

	/** Set the local transform of a node */
	UFUNCTION(BlueprintCallable, Category = "Hierarchy|Transforms", meta = (AnimationInput, AnimationOutput))
	virtual void SetLocalTransform(FName NodeName, const FTransform& Transform);

	/** Set the global transform of a node */
	UFUNCTION(BlueprintCallable, Category = "Hierarchy|Transforms", meta = (AnimationInput, AnimationOutput))
	virtual void SetGlobalTransform(FName NodeName, const FTransform& Transform);

	/** Set the global transform of a node */
	UFUNCTION(BlueprintCallable, Category = "Hierarchy|Transforms", meta = (AnimationInput, AnimationOutput))
	virtual void SetMappedGlobalTransform(FName NodeName, const FTransform& Transform);

	/** Get the global transform of a node */
	UFUNCTION(BlueprintPure, Category = "Hierarchy|Transforms", meta = (AnimationInput, AnimationOutput))
	virtual FTransform GetMappedGlobalTransform(FName NodeName) const;

	/** Set the local transform of a node */
	UFUNCTION(BlueprintCallable, Category = "Hierarchy|Transforms", meta = (AnimationInput, AnimationOutput))
	virtual void SetMappedLocalTransform(FName NodeName, const FTransform& Transform);

	/** Get the local transform of a node */
	UFUNCTION(BlueprintPure, Category = "Hierarchy|Transforms", meta = (AnimationInput, AnimationOutput))
	virtual FTransform GetMappedLocalTransform(FName NodeName) const;

public: 
	/** Get the hierarchy */
	FAnimationHierarchy& GetHierarchy() { return Hierarchy; }

	/** Get the hierarchy */
	const FAnimationHierarchy& GetHierarchy() const { return Hierarchy; }

	/** Finds a manipulator by name */
	UControlManipulator* FindManipulator(const FName& Name);

	/** INodeMappingInterface implementation */
	virtual void GetMappableNodeData(TArray<FName>& OutNames, TArray<FTransform>& OutTransforms) const override;
	// @todo : go away once we serialize hierarchy
	virtual void Setup() override;

	/** Check whether this manipulator is enabled */
	virtual bool IsManipulatorEnabled(UControlManipulator* InManipulator) const { return true; }

	/** Find a counterpart to this manipulator, if any */
	virtual UControlManipulator* FindCounterpartManipulator(UControlManipulator* InManipulator) const { return nullptr; }

	/** Find the main node that is driven by the node in question, if any */
	virtual FName FindNodeDrivenByNode(FName InNodeName) const { return NAME_None; }

	/** Rename current node to new node name */
	virtual bool RenameNode(const FName& CurrentNodeName, const FName& NewNodeName);

	UPROPERTY(BlueprintReadWrite, transient, Category = "Mapping")
	UNodeMappingContainer* NodeMappingContainer;

//#if WITH_EDITOR
	// @todo shouldn't all manipulator to move to editor only?
	// but the manipulator property is what's being animated, so this might still be needed
	void UpdateManipulatorToNode(bool bNotifyListeners);
	//#endif
public:
	// UControlRig interface
	virtual void Initialize() override;
	virtual AActor* GetHostingActor() const override;
	virtual void BindToObject(UObject* InObject) override;
	virtual void UnbindFromObject() override;
	virtual bool IsBoundToObject(UObject* InObject) const override;
	virtual	UObject* GetBoundObject() const;
#if WITH_EDITOR
	virtual FText GetCategory() const override;
	virtual FText GetTooltipText() const override;
#endif
	virtual void GetTickDependencies(TArray<FTickPrerequisite, TInlineAllocator<1>>& OutTickPrerequisites) override;

	// IControlRigInterface interface
	virtual void PreEvaluate() override;
	virtual void Evaluate() override;
	virtual void PostEvaluate() override;
	//virtual void Initialize() override;

	// sort the nodes
	void Sort();
	void UpdateNodes();

private:
	/** Create the topologically-sorted list of nodes to evaluate */
	void CreateSortedNodes();

	/** Evaluate a single node's constraints */
	void EvaluateNode(const FName& NodeName);

	// test to see if Dependent is dependent on Target
	FTransform ResolveConstraints(const FTransform& LocalTransform, const FTransform& ParentTransform, const FConstraintNodeData& NodeData);
	void AddDependenciesRecursive(int32 OriginalNodeIndex, int32 NodeIndex);
	void ApplyConstraint(const FName& NodeName);

protected:
	// for child to add more dependency list
	virtual void GetDependentArray(const FName& NodeName, TArray<FName>& OutList) const;
private:
	/** Internal hierarchy data */
	UPROPERTY()
	FAnimationHierarchy Hierarchy;

	/** Node names sorted so that a node's dependent is before it, for evaluation 
	- @todo, we'd like to remove this but rearrange node itself? */
	TArray<FName> SortedNodes;

	/** Array of arrays giving us the nodes that need to be updated when one updates */
	TArray<TArray<int32>> DependencyGraph;

	/**  Apply Mapping Data Transform functions */
	void ApplyMappingTransform(FName NodeName, FTransform& InOutTransform) const;
	void ApplyInverseMappingTransform(FName NodeName, FTransform& InOutTransform) const;

public:
	/** Add a simple constraint */
	void AddConstraint(const FTransformConstraint& TransformConstraint);

#if WITH_EDITOR
	// anything that changes hierarchy will be editor only for now
	// BEGIN: editor only - nothing forces it but we have to make sure this works. 
	/** Add a node to the hierarchy */
	void AddNode(FName NodeName, FName ParentName, const FTransform& GlobalTransform, FName LinkedNode = NAME_None);
	void SetParent(FName NodeName, FName NewParentName);
	FNodeChain MakeNodeChain(FName RootNode, FName EndNode);

	/** Setter for Constraints */
	void SetConstraints(const TArray<FTransformConstraint>& InConstraints);
	/** Setup this hierarchy ControlRig from a skeletal mesh */
	void BuildHierarchyFromSkeletalMesh(USkeletalMesh* SkeletalMesh);
	void DeleteNode(FName NodeName);
	void DeleteConstraint(FName NodeName, FName TargetNode);
	void UpdateConstraints();
	UControlManipulator* AddManipulator(TSubclassOf<UControlManipulator> ManipulatorClass, FText DisplayName, FName NodeName, FName PropertyToManipulate, EIKSpaceMode KinematicSpace = EIKSpaceMode::IKMode, bool bUsesTranslation = true, bool bUsesRotation = true, bool bUsesScale = false, bool bInLocalSpace = false);

	// END: editor only - nothing forces it but we have to make sure this works. 
#endif // WITH_EDITOR
};