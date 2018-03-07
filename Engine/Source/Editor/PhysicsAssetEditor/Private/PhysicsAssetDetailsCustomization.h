// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "SlateEnums.h"

class FPhysicsAssetEditor;
class IDetailLayoutBuilder;
class FUICommandInfo;
class SWidget;
class IPropertyHandle;
class FUICommandList;

class FPhysicsAssetDetailsCustomization : public IDetailCustomization
{
public:
	// Makes a new instance of this detail layout class for a specific detail view requesting it
	static TSharedRef<IDetailCustomization> MakeInstance(TWeakPtr<FPhysicsAssetEditor> InPhysicsAssetEditor);

	FPhysicsAssetDetailsCustomization(TWeakPtr<FPhysicsAssetEditor> InPhysicsAssetEditor)
		: PhysicsAssetEditorPtr(InPhysicsAssetEditor)
	{}

	// IDetailCustomization interface
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;

private:
	void BindCommands();

	TSharedRef<SWidget> MakePhysicalAnimationProfilesWidget();
	
	TSharedRef<SWidget> MakeConstraintProfilesWidget();

	TSharedRef<SWidget> CreateProfileButton(const FText& InGlyph, TSharedPtr<FUICommandInfo> InCommand);

	void HandlePhysicalAnimationProfileNameCommitted(const FText& InText, ETextCommit::Type InCommitType);

	void HandleConstraintProfileNameCommitted(const FText& InText, ETextCommit::Type InCommitType);

	void ApplyPhysicalAnimationProfile(FName InName);

	void NewPhysicalAnimationProfile();

	bool CanCreateNewPhysicalAnimationProfile() const;

	void DuplicatePhysicalAnimationProfile();

	bool CanDuplicatePhysicalAnimationProfile() const;

	void DeleteCurrentPhysicalAnimationProfile();

	bool CanDeleteCurrentPhysicalAnimationProfile() const;

	void AddBodyToPhysicalAnimationProfile();

	bool CanAddBodyToPhysicalAnimationProfile() const;

	void RemoveBodyFromPhysicalAnimationProfile();

	bool CanRemoveBodyFromPhysicalAnimationProfile() const;

	void ApplyConstraintProfile(FName InName);

	bool ConstraintProfileExistsForAny() const;

	void NewConstraintProfile();

	bool CanCreateNewConstraintProfile() const;

	void DuplicateConstraintProfile();

	bool CanDuplicateConstraintProfile() const;

	void DeleteCurrentConstraintProfile();

	bool CanDeleteCurrentConstraintProfile() const;

	void AddConstraintToCurrentConstraintProfile();

	bool CanAddConstraintToCurrentConstraintProfile() const;

	void RemoveConstraintFromCurrentConstraintProfile();

	bool CanRemoveConstraintFromCurrentConstraintProfile() const;

private:
	TWeakPtr<FPhysicsAssetEditor> PhysicsAssetEditorPtr;

	TSharedPtr<IPropertyHandle> PhysicalAnimationProfilesHandle;

	TSharedPtr<IPropertyHandle> ConstraintProfilesHandle;

	TSharedPtr<FUICommandList> CommandList;
};