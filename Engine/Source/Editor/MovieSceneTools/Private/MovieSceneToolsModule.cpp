// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Curves/RichCurve.h"
#include "ISequencerModule.h"
#include "MovieSceneToolsProjectSettingsCustomization.h"

#include "TrackEditors/PropertyTrackEditors/BoolPropertyTrackEditor.h"
#include "TrackEditors/PropertyTrackEditors/BytePropertyTrackEditor.h"
#include "TrackEditors/PropertyTrackEditors/ColorPropertyTrackEditor.h"
#include "TrackEditors/PropertyTrackEditors/FloatPropertyTrackEditor.h"
#include "TrackEditors/PropertyTrackEditors/IntegerPropertyTrackEditor.h"
#include "TrackEditors/PropertyTrackEditors/VectorPropertyTrackEditor.h"
#include "TrackEditors/PropertyTrackEditors/TransformPropertyTrackEditor.h"
#include "TrackEditors/PropertyTrackEditors/VisibilityPropertyTrackEditor.h"
#include "TrackEditors/PropertyTrackEditors/ActorReferencePropertyTrackEditor.h"
#include "TrackEditors/PropertyTrackEditors/StringPropertyTrackEditor.h"

#include "TrackEditors/TransformTrackEditor.h"
#include "TrackEditors/CameraCutTrackEditor.h"
#include "TrackEditors/CinematicShotTrackEditor.h"
#include "TrackEditors/SlomoTrackEditor.h"
#include "TrackEditors/SubTrackEditor.h"
#include "TrackEditors/AudioTrackEditor.h"
#include "TrackEditors/SkeletalAnimationTrackEditor.h"
#include "TrackEditors/ParticleTrackEditor.h"
#include "TrackEditors/ParticleParameterTrackEditor.h"
#include "TrackEditors/AttachTrackEditor.h"
#include "TrackEditors/EventTrackEditor.h"
#include "TrackEditors/PathTrackEditor.h"
#include "TrackEditors/MaterialTrackEditor.h"
#include "TrackEditors/FadeTrackEditor.h"
#include "TrackEditors/SpawnTrackEditor.h"
#include "TrackEditors/LevelVisibilityTrackEditor.h"
#include "TrackEditors/CameraAnimTrackEditor.h"
#include "TrackEditors/CameraShakeTrackEditor.h"
#include "TrackEditors/MaterialParameterCollectionTrackEditor.h"

#include "MovieSceneBuiltInEasingFunctionCustomization.h"
#include "MovieSceneObjectBindingIDCustomization.h"
#include "SequencerClipboardReconciler.h"
#include "ClipboardTypes.h"
#include "ISettingsModule.h"
#include "PropertyEditorModule.h"
#include "IMovieSceneTools.h"
#include "MovieSceneToolsProjectSettings.h"


#define LOCTEXT_NAMESPACE "FMovieSceneToolsModule"


/**
 * Implements the MovieSceneTools module.
 */
class FMovieSceneToolsModule
	: public IMovieSceneTools
{
public:

	// IModuleInterface interface

	virtual void StartupModule() override
	{
		if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
		{
			SettingsModule->RegisterSettings("Project", "Editor", "Level Sequences",
				LOCTEXT("RuntimeSettingsName", "Level Sequences"),
				LOCTEXT("RuntimeSettingsDescription", "Configure project settings relating to Level Sequences"),
				GetMutableDefault<UMovieSceneToolsProjectSettings>()
			);
		}

		ISequencerModule& SequencerModule = FModuleManager::Get().LoadModuleChecked<ISequencerModule>( "Sequencer" );

		// register property track editors
		BoolPropertyTrackCreateEditorHandle = SequencerModule.RegisterPropertyTrackEditor<FBoolPropertyTrackEditor>();
		BytePropertyTrackCreateEditorHandle = SequencerModule.RegisterPropertyTrackEditor<FBytePropertyTrackEditor>();
		ColorPropertyTrackCreateEditorHandle = SequencerModule.RegisterPropertyTrackEditor<FColorPropertyTrackEditor>();
		FloatPropertyTrackCreateEditorHandle = SequencerModule.RegisterPropertyTrackEditor<FFloatPropertyTrackEditor>();
		IntegerPropertyTrackCreateEditorHandle = SequencerModule.RegisterPropertyTrackEditor<FIntegerPropertyTrackEditor>();
		VectorPropertyTrackCreateEditorHandle = SequencerModule.RegisterPropertyTrackEditor<FVectorPropertyTrackEditor>();
		TransformPropertyTrackCreateEditorHandle = SequencerModule.RegisterPropertyTrackEditor<FTransformPropertyTrackEditor>();
		VisibilityPropertyTrackCreateEditorHandle = SequencerModule.RegisterPropertyTrackEditor<FVisibilityPropertyTrackEditor>();
		ActorReferencePropertyTrackCreateEditorHandle = SequencerModule.RegisterPropertyTrackEditor<FActorReferencePropertyTrackEditor>();
		StringPropertyTrackCreateEditorHandle = SequencerModule.RegisterPropertyTrackEditor<FStringPropertyTrackEditor>();

		// register specialty track editors
		AnimationTrackCreateEditorHandle = SequencerModule.RegisterTrackEditor( FOnCreateTrackEditor::CreateStatic( &FSkeletalAnimationTrackEditor::CreateTrackEditor ) );
		AttachTrackCreateEditorHandle = SequencerModule.RegisterTrackEditor( FOnCreateTrackEditor::CreateStatic( &F3DAttachTrackEditor::CreateTrackEditor ) );
		AudioTrackCreateEditorHandle = SequencerModule.RegisterTrackEditor( FOnCreateTrackEditor::CreateStatic( &FAudioTrackEditor::CreateTrackEditor ) );
		EventTrackCreateEditorHandle = SequencerModule.RegisterTrackEditor( FOnCreateTrackEditor::CreateStatic( &FEventTrackEditor::CreateTrackEditor ) );
		ParticleTrackCreateEditorHandle = SequencerModule.RegisterTrackEditor( FOnCreateTrackEditor::CreateStatic( &FParticleTrackEditor::CreateTrackEditor ) );
		ParticleParameterTrackCreateEditorHandle = SequencerModule.RegisterTrackEditor( FOnCreateTrackEditor::CreateStatic( &FParticleParameterTrackEditor::CreateTrackEditor ) );
		PathTrackCreateEditorHandle = SequencerModule.RegisterTrackEditor( FOnCreateTrackEditor::CreateStatic( &F3DPathTrackEditor::CreateTrackEditor ) );
		CameraCutTrackCreateEditorHandle = SequencerModule.RegisterTrackEditor( FOnCreateTrackEditor::CreateStatic( &FCameraCutTrackEditor::CreateTrackEditor ) );
		CinematicShotTrackCreateEditorHandle = SequencerModule.RegisterTrackEditor( FOnCreateTrackEditor::CreateStatic( &FCinematicShotTrackEditor::CreateTrackEditor ) );
		SlomoTrackCreateEditorHandle = SequencerModule.RegisterTrackEditor( FOnCreateTrackEditor::CreateStatic( &FSlomoTrackEditor::CreateTrackEditor ) );
		SubTrackCreateEditorHandle = SequencerModule.RegisterTrackEditor( FOnCreateTrackEditor::CreateStatic( &FSubTrackEditor::CreateTrackEditor ) );
		TransformTrackCreateEditorHandle = SequencerModule.RegisterTrackEditor( FOnCreateTrackEditor::CreateStatic( &F3DTransformTrackEditor::CreateTrackEditor ) );
		ComponentMaterialTrackCreateEditorHandle = SequencerModule.RegisterTrackEditor( FOnCreateTrackEditor::CreateStatic( &FComponentMaterialTrackEditor::CreateTrackEditor ) );
		FadeTrackCreateEditorHandle = SequencerModule.RegisterTrackEditor( FOnCreateTrackEditor::CreateStatic( &FFadeTrackEditor::CreateTrackEditor ) );
		SpawnTrackCreateEditorHandle = SequencerModule.RegisterTrackEditor( FOnCreateTrackEditor::CreateStatic( &FSpawnTrackEditor::CreateTrackEditor ) );
		LevelVisibilityTrackCreateEditorHandle = SequencerModule.RegisterTrackEditor( FOnCreateTrackEditor::CreateStatic( &FLevelVisibilityTrackEditor::CreateTrackEditor ) );
		CameraAnimTrackCreateEditorHandle = SequencerModule.RegisterTrackEditor(FOnCreateTrackEditor::CreateStatic(&FCameraAnimTrackEditor::CreateTrackEditor));
		CameraShakeTrackCreateEditorHandle = SequencerModule.RegisterTrackEditor(FOnCreateTrackEditor::CreateStatic(&FCameraShakeTrackEditor::CreateTrackEditor));
		MPCTrackCreateEditorHandle = SequencerModule.RegisterTrackEditor(FOnCreateTrackEditor::CreateStatic(&FMaterialParameterCollectionTrackEditor::CreateTrackEditor));

		RegisterClipboardConversions();

		// register details customization
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.RegisterCustomClassLayout("MovieSceneToolsProjectSettings", FOnGetDetailCustomizationInstance::CreateStatic(&FMovieSceneToolsProjectSettingsCustomization::MakeInstance));
		PropertyModule.RegisterCustomClassLayout("MovieSceneBuiltInEasingFunction", FOnGetDetailCustomizationInstance::CreateLambda(&MakeShared<FMovieSceneBuiltInEasingFunctionCustomization>));
		PropertyModule.RegisterCustomPropertyTypeLayout("MovieSceneObjectBindingID", FOnGetPropertyTypeCustomizationInstance::CreateLambda(&MakeShared<FMovieSceneObjectBindingIDCustomization>));
	}

	virtual void ShutdownModule() override
	{
		if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
		{
			SettingsModule->UnregisterSettings("Project", "Editor", "Level Sequences");
		}

		if (!FModuleManager::Get().IsModuleLoaded("Sequencer"))
		{
			return;
		}

		ISequencerModule& SequencerModule = FModuleManager::Get().GetModuleChecked<ISequencerModule>( "Sequencer" );

		// unregister property track editors
		SequencerModule.UnRegisterTrackEditor( BoolPropertyTrackCreateEditorHandle );
		SequencerModule.UnRegisterTrackEditor( BytePropertyTrackCreateEditorHandle );
		SequencerModule.UnRegisterTrackEditor( ColorPropertyTrackCreateEditorHandle );
		SequencerModule.UnRegisterTrackEditor( FloatPropertyTrackCreateEditorHandle );
		SequencerModule.UnRegisterTrackEditor( IntegerPropertyTrackCreateEditorHandle );
		SequencerModule.UnRegisterTrackEditor( VectorPropertyTrackCreateEditorHandle );
		SequencerModule.UnRegisterTrackEditor( VisibilityPropertyTrackCreateEditorHandle );
		SequencerModule.UnRegisterTrackEditor( ActorReferencePropertyTrackCreateEditorHandle );
		SequencerModule.UnRegisterTrackEditor( StringPropertyTrackCreateEditorHandle );

		// unregister specialty track editors
		SequencerModule.UnRegisterTrackEditor( AnimationTrackCreateEditorHandle );
		SequencerModule.UnRegisterTrackEditor( AttachTrackCreateEditorHandle );
		SequencerModule.UnRegisterTrackEditor( AudioTrackCreateEditorHandle );
		SequencerModule.UnRegisterTrackEditor( EventTrackCreateEditorHandle );
		SequencerModule.UnRegisterTrackEditor( ParticleTrackCreateEditorHandle );
		SequencerModule.UnRegisterTrackEditor( ParticleParameterTrackCreateEditorHandle );
		SequencerModule.UnRegisterTrackEditor( PathTrackCreateEditorHandle );
		SequencerModule.UnRegisterTrackEditor( CameraCutTrackCreateEditorHandle );
		SequencerModule.UnRegisterTrackEditor( CinematicShotTrackCreateEditorHandle );
		SequencerModule.UnRegisterTrackEditor( SlomoTrackCreateEditorHandle );
		SequencerModule.UnRegisterTrackEditor( SubTrackCreateEditorHandle );
		SequencerModule.UnRegisterTrackEditor( TransformTrackCreateEditorHandle );
		SequencerModule.UnRegisterTrackEditor( ComponentMaterialTrackCreateEditorHandle );
		SequencerModule.UnRegisterTrackEditor( FadeTrackCreateEditorHandle );
		SequencerModule.UnRegisterTrackEditor( SpawnTrackCreateEditorHandle );
		SequencerModule.UnRegisterTrackEditor( CameraAnimTrackCreateEditorHandle );
		SequencerModule.UnRegisterTrackEditor( CameraShakeTrackCreateEditorHandle );
		SequencerModule.UnRegisterTrackEditor( MPCTrackCreateEditorHandle );

		if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
		{	
			FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
			PropertyModule.UnregisterCustomClassLayout("MovieSceneToolsProjectSettings");
			PropertyModule.UnregisterCustomClassLayout("MovieSceneBuiltInEasingFunction");
			PropertyModule.UnregisterCustomPropertyTypeLayout("MovieSceneObjectBindingID");
		}
	}

	void RegisterClipboardConversions()
	{
		using namespace MovieSceneClipboard;

		DefineImplicitConversion<int32, uint8>();
		DefineImplicitConversion<int32, bool>();

		DefineImplicitConversion<uint8, int32>();
		DefineImplicitConversion<uint8, bool>();

		DefineExplicitConversion<int32, FRichCurveKey>([](const int32& In) -> FRichCurveKey { return FRichCurveKey(0.f, In);	});
		DefineExplicitConversion<uint8, FRichCurveKey>([](const uint8& In) -> FRichCurveKey { return FRichCurveKey(0.f, In);	});
		DefineExplicitConversion<FRichCurveKey, int32>([](const FRichCurveKey& In) -> int32 { return In.Value; 					});
		DefineExplicitConversion<FRichCurveKey, uint8>([](const FRichCurveKey& In) -> uint8 { return In.Value; 					});
		DefineExplicitConversion<FRichCurveKey, bool>([](const FRichCurveKey& In) -> bool	{ return !!In.Value; 				});

		FSequencerClipboardReconciler::AddTrackAlias("Location.X", "R");
		FSequencerClipboardReconciler::AddTrackAlias("Location.Y", "G");
		FSequencerClipboardReconciler::AddTrackAlias("Location.Z", "B");

		FSequencerClipboardReconciler::AddTrackAlias("Rotation.X", "R");
		FSequencerClipboardReconciler::AddTrackAlias("Rotation.Y", "G");
		FSequencerClipboardReconciler::AddTrackAlias("Rotation.Z", "B");

		FSequencerClipboardReconciler::AddTrackAlias("Scale.X", "R");
		FSequencerClipboardReconciler::AddTrackAlias("Scale.Y", "G");
		FSequencerClipboardReconciler::AddTrackAlias("Scale.Z", "B");

		FSequencerClipboardReconciler::AddTrackAlias("X", "R");
		FSequencerClipboardReconciler::AddTrackAlias("Y", "G");
		FSequencerClipboardReconciler::AddTrackAlias("Z", "B");
		FSequencerClipboardReconciler::AddTrackAlias("W", "A");
	}

private:

	/** Registered delegate handles */
	FDelegateHandle BoolPropertyTrackCreateEditorHandle;
	FDelegateHandle BytePropertyTrackCreateEditorHandle;
	FDelegateHandle ColorPropertyTrackCreateEditorHandle;
	FDelegateHandle FloatPropertyTrackCreateEditorHandle;
	FDelegateHandle IntegerPropertyTrackCreateEditorHandle;
	FDelegateHandle VectorPropertyTrackCreateEditorHandle;
	FDelegateHandle TransformPropertyTrackCreateEditorHandle;
	FDelegateHandle VisibilityPropertyTrackCreateEditorHandle;
	FDelegateHandle ActorReferencePropertyTrackCreateEditorHandle;
	FDelegateHandle StringPropertyTrackCreateEditorHandle;

	FDelegateHandle AnimationTrackCreateEditorHandle;
	FDelegateHandle AttachTrackCreateEditorHandle;
	FDelegateHandle AudioTrackCreateEditorHandle;
	FDelegateHandle EventTrackCreateEditorHandle;
	FDelegateHandle ParticleTrackCreateEditorHandle;
	FDelegateHandle ParticleParameterTrackCreateEditorHandle;
	FDelegateHandle PathTrackCreateEditorHandle;
	FDelegateHandle CameraCutTrackCreateEditorHandle;
	FDelegateHandle CinematicShotTrackCreateEditorHandle;
	FDelegateHandle SlomoTrackCreateEditorHandle;
	FDelegateHandle SubTrackCreateEditorHandle;
	FDelegateHandle TransformTrackCreateEditorHandle;
	FDelegateHandle ComponentMaterialTrackCreateEditorHandle;
	FDelegateHandle FadeTrackCreateEditorHandle;
	FDelegateHandle SpawnTrackCreateEditorHandle;
	FDelegateHandle LevelVisibilityTrackCreateEditorHandle;
	FDelegateHandle CameraAnimTrackCreateEditorHandle;
	FDelegateHandle CameraShakeTrackCreateEditorHandle;
	FDelegateHandle MPCTrackCreateEditorHandle;
};


IMPLEMENT_MODULE( FMovieSceneToolsModule, MovieSceneTools );

#undef LOCTEXT_NAMESPACE

