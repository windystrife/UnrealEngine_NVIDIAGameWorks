// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieSceneMaterialParameterCollectionTemplate.h"
#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"
#include "Tracks/MovieSceneMaterialParameterCollectionTrack.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "IMovieScenePlayer.h"
#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"
#include "MovieSceneSequence.h"

struct FPreAnimatedMPCScalarToken : IMovieScenePreAnimatedToken
{
	FName ParameterName;
	float Value;

	FPreAnimatedMPCScalarToken(FName InParameterName, float InValue)
		: ParameterName(InParameterName)
		, Value(InValue)
	{}

	virtual void RestoreState(UObject& Object, IMovieScenePlayer& Player) override
	{
		UMaterialParameterCollectionInstance* Instance = CastChecked<UMaterialParameterCollectionInstance>(&Object);
		Instance->SetScalarParameterValue(ParameterName, Value);
	}
};

struct FPreAnimatedMPCScalarTokenProducer : IMovieScenePreAnimatedTokenProducer
{
	FName ParameterName;

	FPreAnimatedMPCScalarTokenProducer(FName InParameterName)
		: ParameterName(InParameterName)
	{}

	virtual IMovieScenePreAnimatedTokenPtr CacheExistingState(UObject& Object) const override
	{
		UMaterialParameterCollectionInstance* Instance = CastChecked<UMaterialParameterCollectionInstance>(&Object);
		float Value = 0.f;
		if (Instance->GetScalarParameterValue(ParameterName, Value))
		{
			return FPreAnimatedMPCScalarToken(ParameterName, Value);
		}
		return IMovieScenePreAnimatedTokenPtr();
	}
};

struct FPreAnimatedMPCVectorToken : IMovieScenePreAnimatedToken
{
	FName ParameterName;
	FLinearColor Value;

	FPreAnimatedMPCVectorToken(FName InParameterName, FLinearColor InValue)
		: ParameterName(InParameterName)
		, Value(InValue)
	{}

	virtual void RestoreState(UObject& Object, IMovieScenePlayer& Player) override
	{
		UMaterialParameterCollectionInstance* Instance = CastChecked<UMaterialParameterCollectionInstance>(&Object);
		Instance->SetVectorParameterValue(ParameterName, Value);
	}
};

struct FPreAnimatedMPCVectorTokenProducer : IMovieScenePreAnimatedTokenProducer
{
	FName ParameterName;

	FPreAnimatedMPCVectorTokenProducer(FName InParameterName)
		: ParameterName(InParameterName)
	{}

	virtual IMovieScenePreAnimatedTokenPtr CacheExistingState(UObject& Object) const override
	{
		UMaterialParameterCollectionInstance* Instance = CastChecked<UMaterialParameterCollectionInstance>(&Object);
		FLinearColor Value;
		if (Instance->GetVectorParameterValue(ParameterName, Value))
		{
			return FPreAnimatedMPCVectorToken(ParameterName, Value);
		}
		return IMovieScenePreAnimatedTokenPtr();
	}
};

struct FMaterialParameterCollectionExecutionToken : IMovieSceneExecutionToken
{
	FMaterialParameterCollectionExecutionToken(UMaterialParameterCollection* InCollection) : Collection(InCollection) {}

#if PLATFORM_COMPILER_HAS_DEFAULTED_FUNCTIONS
	FMaterialParameterCollectionExecutionToken(FMaterialParameterCollectionExecutionToken&&) = default;
	FMaterialParameterCollectionExecutionToken& operator=(FMaterialParameterCollectionExecutionToken&&) = default;
#else
	FMaterialParameterCollectionExecutionToken(FMaterialParameterCollectionExecutionToken&& RHS)
		: Values(MoveTemp(RHS.Values))
	{
	}
	FMaterialParameterCollectionExecutionToken& operator=(FMaterialParameterCollectionExecutionToken&& RHS)
	{
		Values = MoveTemp(RHS.Values);
		return *this;
	}
#endif

	virtual void Execute(const FMovieSceneContext& Context, const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player)
	{
		UObject* WorldContextObject = Player.GetPlaybackContext();
		UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);

		if (!World)
		{
			return;
		}

		static TMovieSceneAnimTypeIDContainer<FName> AnimTypeIDsByName;
		UMaterialParameterCollectionInstance* Instance = World->GetParameterCollectionInstance(Collection);

		TArray<FString> InvalidParameterNames;

		for ( const FScalarParameterNameAndValue& ScalarNameAndValue : Values.ScalarValues )
		{
			FName Name = ScalarNameAndValue.ParameterName;

			Player.SavePreAnimatedState(*Instance, AnimTypeIDsByName.GetAnimTypeID(Name), FPreAnimatedMPCScalarTokenProducer(Name));
			
			if (!Instance->SetScalarParameterValue(Name, ScalarNameAndValue.Value))
			{
				InvalidParameterNames.Add(Name.ToString());
			}
		}

		// MPCs use vector and color terminology interchangeably
		for ( const FColorParameterNameAndValue& ColorNameAndValue : Values.ColorValues )
		{
			FName Name = ColorNameAndValue.ParameterName;

			Player.SavePreAnimatedState(*Instance, AnimTypeIDsByName.GetAnimTypeID(Name), FPreAnimatedMPCVectorTokenProducer(Name));
			
			if (!Instance->SetVectorParameterValue(Name, ColorNameAndValue.Value))
			{
				InvalidParameterNames.Add(Name.ToString());
			}
		}

		if (InvalidParameterNames.Num() && !Instance->bLoggedMissingParameterWarning)
		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("ParamNames"), FText::FromString(FString::Join(InvalidParameterNames, TEXT(", "))));
			FMessageLog("PIE").Warning()
				->AddToken(FTextToken::Create(NSLOCTEXT("MaterialParameterCollectionTrack", "InvalidParameterText", "Invalid parameter name or type applied in sequence")))
				->AddToken(FUObjectToken::Create(Player.GetEvaluationTemplate().GetSequence(MovieSceneSequenceID::Root)))
				->AddToken(FTextToken::Create(NSLOCTEXT("MaterialParameterCollectionTrack", "OnText", "on")))
				->AddToken(FUObjectToken::Create(Collection))
				->AddToken(FTextToken::Create(FText::Format(NSLOCTEXT("MaterialParameterCollectionTrack", "InvalidParameterFormatText", "with the following invalid parameters: {ParamNames}."), Arguments)));
			Instance->bLoggedMissingParameterWarning = true;
		}
	}
	
	UMaterialParameterCollection* Collection;
	FEvaluatedParameterSectionValues Values;
};

FMovieSceneMaterialParameterCollectionTemplate::FMovieSceneMaterialParameterCollectionTemplate(const UMovieSceneParameterSection& Section, const UMovieSceneMaterialParameterCollectionTrack& Track)
	: FMovieSceneParameterSectionTemplate(Section)
	, MPC(Track.MPC)
{
}

void FMovieSceneMaterialParameterCollectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	FMaterialParameterCollectionExecutionToken ExecutionToken(MPC);
	EvaluateCurves(Context, ExecutionToken.Values);
	ExecutionTokens.Add(MoveTemp(ExecutionToken));
}
