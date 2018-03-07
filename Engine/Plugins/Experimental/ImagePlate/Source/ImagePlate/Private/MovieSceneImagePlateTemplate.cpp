// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneImagePlateTemplate.h"

#include "Package.h"
#include "UObject/GCObject.h"

#include "MovieSceneImagePlateTrack.h"
#include "MovieSceneImagePlateSection.h"
#include "ImagePlateFileSequence.h"

#include "Engine/Texture2D.h"
#include "Engine/Texture2DDynamic.h"
#include "Engine/TextureRenderTarget2D.h"

struct FImagePlateSequenceData : PropertyTemplate::FSectionData
{
	TOptional<FImagePlateAsyncCache> AsyncCache;
};

// This pre-animated token is only created if we create a new render target and assign it to the property
struct FRenderTexturePropertyPreAnimatedToken : IMovieScenePreAnimatedToken
{
	/** Producer that can create these tokens */
	struct FProducer : IMovieScenePreAnimatedTokenProducer
	{
		FProducer(const TSharedPtr<FTrackInstancePropertyBindings>& InBindings)
			: Bindings(InBindings)
		{}

		virtual void InitializeObjectForAnimation(UObject& Object) const
		{
			UTexture2DDynamic* DynamicRenderTexture = NewObject<UTexture2DDynamic>(&Object, FName(), RF_Transient|RF_DuplicateTransient);
			DynamicRenderTexture->Init(256, 256, PF_R8G8B8A8);
			Bindings->SetCurrentValue<UTexture*>(Object, DynamicRenderTexture);
		}

		virtual IMovieScenePreAnimatedTokenPtr CacheExistingState(UObject& Object) const override
		{
			return FRenderTexturePropertyPreAnimatedToken(Bindings, Object);
		}

		TSharedPtr<FTrackInstancePropertyBindings> Bindings;
	};

	FRenderTexturePropertyPreAnimatedToken(const TSharedPtr<FTrackInstancePropertyBindings>& InBindings, UObject& Object)
		: PropertyBindings(InBindings)
	{
		OldTexture = InBindings->GetCurrentValue<UTexture*>(Object);
	}

	virtual void RestoreState(UObject& RestoreObject, IMovieScenePlayer& Player)
	{
		PropertyBindings->CallFunction<UTexture*>(RestoreObject, OldTexture.Get());
	}

private:

	TWeakObjectPtr<UTexture> OldTexture;

	/** Property bindings that allow us to set the property when we've finished evaluating */
	TSharedPtr<FTrackInstancePropertyBindings> PropertyBindings;
};

struct FImagePlatePreRollExecutionToken : IMovieSceneExecutionToken
{
	float ImageSequenceTime;
	FImagePlatePreRollExecutionToken(float InImageSequenceTime) : ImageSequenceTime(InImageSequenceTime) {}

	virtual void Execute(const FMovieSceneContext& Context, const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) override
	{
		FImagePlateSequenceData& SectionData = PersistentData.GetSectionData<FImagePlateSequenceData>();

		// Cache 10 frames in the play direction
		const int32 LeadingPrecacheFrames = Context.GetDirection() == EPlayDirection::Forwards ? 10 : 0;
		const int32 TrailingPrecacheFrames = Context.GetDirection() == EPlayDirection::Forwards ? 0 : 10;

		SectionData.AsyncCache->RequestFrame(ImageSequenceTime, LeadingPrecacheFrames, TrailingPrecacheFrames);
	}
};

struct FImagePlateExecutionToken : IMovieSceneExecutionToken
{
	float ImageSequenceTime;
	bool bReuseExistingTexture;
	FImagePlateExecutionToken(float InImageSequenceTime, bool bInReuseExistingTexture) : ImageSequenceTime(InImageSequenceTime), bReuseExistingTexture(bInReuseExistingTexture) {}

	virtual void Execute(const FMovieSceneContext& Context, const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) override
	{
		FImagePlateSequenceData& SectionData = PersistentData.GetSectionData<FImagePlateSequenceData>();

		for (TWeakObjectPtr<> WeakObject : Player.FindBoundObjects(Operand))
		{
			UObject* Object = WeakObject.Get();
			if (!Object)
			{
				continue;
			}

			UTexture* RenderTexture = SectionData.PropertyBindings->GetCurrentValue<UTexture*>(*Object);
			UObjectProperty* Property = Cast<UObjectProperty>(SectionData.PropertyBindings->GetProperty(*Object));

			bool bCreateNewTexture = ( Property && UTexture2DDynamic::StaticClass()->IsChildOf(Property->PropertyClass) ) && ( !RenderTexture || !bReuseExistingTexture);
			if (bCreateNewTexture)
			{
				// Save the render target assignment with the track
				Player.SavePreAnimatedState(*Object, SectionData.PropertyID, FRenderTexturePropertyPreAnimatedToken::FProducer(SectionData.PropertyBindings), PersistentData.GetTrackKey());
				RenderTexture = SectionData.PropertyBindings->GetCurrentValue<UTexture*>(*Object);
			}

			if (!RenderTexture)
			{
				continue;
			}

			// Cache 10 frames in the play direction
			int32 LeadingPrecacheFrames = Context.GetDirection() == EPlayDirection::Forwards ? 10 : 0;
			int32 TrailingPrecacheFrames = Context.GetDirection() == EPlayDirection::Forwards ? 0 : 10;

			// When scrubbing, we don't have a good understanding of the direction, so cache 5 frames each way
			if (Context.GetStatus() == EMovieScenePlayerStatus::Scrubbing)
			{
				LeadingPrecacheFrames = TrailingPrecacheFrames = 5;
			}

			// Get the source frame data
			TSharedFuture<FImagePlateSourceFrame> FrameData = SectionData.AsyncCache->RequestFrame(ImageSequenceTime, LeadingPrecacheFrames, TrailingPrecacheFrames);

			// If we're not scrubbing (or the frame data is already available), resolve it to the texture
			if (Context.GetStatus() != EMovieScenePlayerStatus::Scrubbing || FrameData.WaitFor(0))
			{
				TFuture<void> TextureCopied = FrameData.Get().CopyTo(RenderTexture);
				// @todo: do we need to block here? It'll get picked up on the render thread anyway before our frame is out
				//TextureCopied.Wait();
			}
		}
	}
};

FMovieSceneImagePlateSectionTemplate::FMovieSceneImagePlateSectionTemplate(const UMovieSceneImagePlateSection& InSection, const UMovieSceneImagePlateTrack& InTrack)
	: PropertyData(InTrack.GetPropertyName(), InTrack.GetPropertyPath(), NAME_None, "OnRenderTextureChanged")
{
	Params.FileSequence = InSection.FileSequence;
	Params.SectionStartTime = InSection.GetStartTime();
	Params.bReuseExistingTexture = InSection.bReuseExistingTexture;
}

void FMovieSceneImagePlateSectionTemplate::Setup(FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const
{
	if (Params.FileSequence)
	{
		PropertyData.SetupTrack<FImagePlateSequenceData>(PersistentData);
		PersistentData.GetSectionData<FImagePlateSequenceData>().AsyncCache = Params.FileSequence->GetAsyncCache();
	}
}

void FMovieSceneImagePlateSectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	if (!Params.FileSequence || Context.IsPostRoll())
	{
		return;
	}

	if (Context.IsPreRoll())
	{
		const float ImageSequenceTime = (Context.HasPreRollEndTime() ? Context.GetPreRollEndTime() - Params.SectionStartTime : 0.f);
		ExecutionTokens.Add(FImagePlatePreRollExecutionToken(ImageSequenceTime));
	}
	else
	{
		const float ImageSequenceTime = Context.GetTime() - Params.SectionStartTime;
		ExecutionTokens.Add(FImagePlateExecutionToken(ImageSequenceTime, Params.bReuseExistingTexture));
	}
}
