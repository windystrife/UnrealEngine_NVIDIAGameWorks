// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneMediaTemplate.h"

#include "MediaSource.h"
#include "MediaPlayer.h"
#include "MediaTexture.h"
#include "Package.h"
#include "UObject/GCObject.h"

#include "MovieSceneMediaSection.h"
#include "MovieSceneMediaTrack.h"


/* Local helpers
 *****************************************************************************/

/**
 * Simple struct that tries to keep an object rooted for this object's lifetime.
 * Copying is prohibited. Moving passes ownership of the root to the LHS.
 * Cannot not guarantee rooting outside of its control.
 */
template<typename T>
struct TScopedRootObject
{
	/** Construct and roo the specified object */
	TScopedRootObject(T* InObject)
		: Object(InObject), bAddedToRoot(false)
	{
		if (InObject && !InObject->IsRooted())
		{
			Object->AddToRoot();
		}
	}

	/** Copying is disabled */
	TScopedRootObject(const TScopedRootObject&) = delete;
	TScopedRootObject& operator=(const TScopedRootObject&) = delete;

	/** Move construction/assignment passes ownership of the root to the LHS */
	TScopedRootObject(TScopedRootObject&& In)
		: Object(In.Object)
		, bAddedToRoot(In.bAddedToRoot)
	{
		In.Object = nullptr;
		In.bAddedToRoot = false;
	}

	TScopedRootObject& operator=(TScopedRootObject&& In)
	{
		Object = In.Object;
		bAddedToRoot = In.bAddedToRoot;
		
		In.Object = nullptr;
		In.bAddedToRoot = false;

		return *this;
	}

	/** Destructor frees the root, if necessary */
	~TScopedRootObject()
	{
		if (Object && bAddedToRoot)
		{
			Object->RemoveFromRoot();
		}
	}

	T* GetObject() const
	{
		return Object;
	}

private:

	/** The object that should be rooted */
	T* Object;

	/** Whether we added the object to the root (false if it's null, or was already rooted) */
	bool bAddedToRoot;
};


// This pre-animated token is only created if we create a new mediaplayer and assign it to the property
struct FMediaPlayerPropertyPreAnimatedToken
	: IMovieScenePreAnimatedToken
{
	FMediaPlayerPropertyPreAnimatedToken(UMediaPlayer* InPreviousPropertyValue, const TSharedPtr<FTrackInstancePropertyBindings>& InBindings)
		: PreviousPropertyValue(InPreviousPropertyValue), PropertyBindings(InBindings)
	{ }

	/** Manual defaulted functions while we need to support compilers that can't generate/default them for us */
#if PLATFORM_COMPILER_HAS_DEFAULTED_FUNCTIONS
	FMediaPlayerPropertyPreAnimatedToken(FMediaPlayerPropertyPreAnimatedToken&& In) = default;
	FMediaPlayerPropertyPreAnimatedToken& operator=(FMediaPlayerPropertyPreAnimatedToken&& In) = default;

#else
	FMediaPlayerPropertyPreAnimatedToken(FMediaPlayerPropertyPreAnimatedToken&& In)
		: PreviousPropertyValue(MoveTemp(In.PreviousPropertyValue))
		, PropertyBindings(MoveTemp(In.PropertyBindings))
	{ }

	FMediaPlayerPropertyPreAnimatedToken& operator=(FMediaPlayerPropertyPreAnimatedToken&& In)
	{
		PreviousPropertyValue = MoveTemp(In.PreviousPropertyValue);
		PropertyBindings = MoveTemp(In.PropertyBindings);

		return *this;
	}
#endif

	/** Virtual destructor. */
	virtual ~FMediaPlayerPropertyPreAnimatedToken() { }

public:

	virtual void RestoreState(UObject& RestoreObject, IMovieScenePlayer& Player)
	{
		UMediaPlayer* PreviousPlayer = PreviousPropertyValue.GetObject();
		PropertyBindings->CallFunction<UMediaPlayer*>(RestoreObject, PreviousPlayer);
	}

private:

	/** Keep the previous player alive so we can definitely restore it later on */
	TScopedRootObject<UMediaPlayer> PreviousPropertyValue;

	/** Property bindings that allow us to set the property when we've finished evaluating */
	TSharedPtr<FTrackInstancePropertyBindings> PropertyBindings;

public:

	/** Producer that can create these tokens */
	struct FProducer
		: IMovieScenePreAnimatedTokenProducer
	{
		FProducer(const TSharedPtr<FTrackInstancePropertyBindings>& InBindings)
			: Bindings(InBindings)
		{ }

		virtual ~FProducer() { }

		virtual IMovieScenePreAnimatedTokenPtr CacheExistingState(UObject& Object) const override
		{
			return FMediaPlayerPropertyPreAnimatedToken(Bindings->GetCurrentValue<UMediaPlayer*>(Object), Bindings);
		}

		TSharedPtr<FTrackInstancePropertyBindings> Bindings;
	};
};


/**
 * This pre-animated token is only created if we create a new
 * media texture and assign it to an existing media player.
 */
struct FMediaPlayerTexturePreAnimatedToken
	: IMovieScenePreAnimatedToken
{
	/** Virtual destructor. */
	virtual ~FMediaPlayerTexturePreAnimatedToken() { }

	virtual void RestoreState(UObject& RestoreObject, IMovieScenePlayer& Player)
	{
		UMediaPlayer* MediaPlayer = CastChecked<UMediaPlayer>(&RestoreObject);
		// @todo gmp: fix me media framework 3.0
		/*
		MediaPlayer->SetVideoTexture(nullptr);
		*/
	}

	static FMovieSceneAnimTypeID GetAnimTypeID() { return TMovieSceneAnimTypeID<FMediaPlayerTexturePreAnimatedToken>(); }

	struct FProducer : IMovieScenePreAnimatedTokenProducer
	{
		virtual IMovieScenePreAnimatedTokenPtr CacheExistingState(UObject& Object) const override
		{
			return FMediaPlayerTexturePreAnimatedToken();
		}
	};
};


/**
 * Persistent data that's stored for each currently evaluating section.
 */
struct FMediaSectionData
	: PropertyTemplate::FSectionData, FGCObject
{
	FMediaSectionData()
		: TemporaryMediaPlayer(nullptr)
	{ }

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		Collector.AddReferencedObject(TemporaryMediaPlayer);
	}

	UMediaPlayer* GetTemporaryMediaPlayer()
	{
		if (TemporaryMediaPlayer == nullptr)
		{
			TemporaryMediaPlayer = NewObject<UMediaPlayer>(GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), UMediaPlayer::StaticClass()));
		}

		return TemporaryMediaPlayer;
	}

	UMediaPlayer* GetOrUpdateMediaPlayerFromProperty(UObject& InObject, IMovieScenePlayer& Player)
	{
		UMediaPlayer* MediaPlayer = PropertyBindings->GetCurrentValue<UMediaPlayer*>(InObject);

		// assign the temporary player to the property if it's null or not the temporary player
		const bool bNeedToAssignTemporaryPlayer = !MediaPlayer || (TemporaryMediaPlayer && MediaPlayer != TemporaryMediaPlayer);

		if (bNeedToAssignTemporaryPlayer)
		{
			GetTemporaryMediaPlayer();
			MediaPlayer = TemporaryMediaPlayer;
		}

		check(MediaPlayer);

		// Ensure we have a video texture on the media player (transient ones will always
		// have a texture already, but media players specified on external properties may not)

		// @todo gmp: fix me Media Framework 3.0
		/*
		if (!MediaPlayer->GetVideoTexture())
		{
			UMediaTexture* NewTexture = NewObject<UMediaTexture>(GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), UMediaTexture::StaticClass()));
			NewTexture->UpdateResource();

			// Save the previous video texture so we don't end up trying to save one that's in a transient package
			Player.SavePreAnimatedState(*MediaPlayer, FMediaPlayerTexturePreAnimatedToken::GetAnimTypeID(), FMediaPlayerTexturePreAnimatedToken::FProducer());

			MediaPlayer->SetVideoTexture(NewTexture);
		}
		*/

		// always assign the property last, so that anything responding to the update
		// can have access to the media player and its video texture if needs be

		if (bNeedToAssignTemporaryPlayer)
		{
			// save the previous property value using a unique anim type ID to ensure we don't leave it set after evaluation
			Player.SavePreAnimatedState(InObject, PropertyID, FMediaPlayerPropertyPreAnimatedToken::FProducer(PropertyBindings));

			// assign the property
			PropertyBindings->CallFunction<UMediaPlayer*>(InObject, MediaPlayer);
		}

		return MediaPlayer;
	}

private:

	UMediaPlayer* TemporaryMediaPlayer;
};


struct FMediaPlayerPreAnimatedTokenProducer
	: IMovieScenePreAnimatedTokenProducer
{
	virtual IMovieScenePreAnimatedTokenPtr CacheExistingState(UObject& Object) const override
	{
		struct FToken : IMovieScenePreAnimatedToken
		{
			FToken(float InRate, const FString& InOldUrl, FTimespan InSeekPosition)
				: Rate(InRate), OldUrl(InOldUrl), SeekPosition(InSeekPosition)
			{}

			virtual void RestoreState(UObject& RestoreObject, IMovieScenePlayer& Player)
			{
				UMediaPlayer* LocalMediaPlayer = CastChecked<UMediaPlayer>(&RestoreObject);

				if (!OldUrl.IsEmpty())
				{
					LocalMediaPlayer->OpenUrl(OldUrl);
				}
				else
				{
					LocalMediaPlayer->Close();
				}

				LocalMediaPlayer->SetRate(Rate);
				LocalMediaPlayer->Seek(SeekPosition);
			}

			float Rate;
			FString OldUrl;
			FTimespan SeekPosition;
		};

		UMediaPlayer* MediaPlayer = CastChecked<UMediaPlayer>(&Object);
		return FToken{ MediaPlayer->GetRate(), MediaPlayer->GetUrl(), MediaPlayer->GetTime() };
	}
};


struct FMediaSectionPreRollExecutionToken
	: IMovieSceneExecutionToken
{
	FMediaSectionPreRollExecutionToken(UMediaSource* InSource, FTimespan InMediaTime)
		: Source(InSource)
		, MediaTime(InMediaTime)
	{
	}

	virtual void Execute(const FMovieSceneContext& Context, const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) override
	{
		using namespace PropertyTemplate;

		const bool bForceTemporaryPlayer = ForceTemporaryPlayer();

		FMediaSectionData& SectionData = PersistentData.GetSectionData<FMediaSectionData>();
		for (TWeakObjectPtr<> WeakObject : Player.FindBoundObjects(Operand))
		{
			UObject* Object = WeakObject.Get();
			if (!Object)
			{
				continue;
			}

			UMediaPlayer* MediaPlayer = bForceTemporaryPlayer ? SectionData.GetTemporaryMediaPlayer() : SectionData.GetOrUpdateMediaPlayerFromProperty(*Object, Player);
			if (!ensure(MediaPlayer))
			{
				continue;
			}

			// Save the previous media player state
			Player.SavePreAnimatedState(*MediaPlayer, TMovieSceneAnimTypeID<FMediaSectionPreRollExecutionToken>(), FMediaPlayerPreAnimatedTokenProducer());

			// Open the media source if necessary
			if (MediaPlayer->GetUrl() != Source->GetUrl())
			{
				if (MediaPlayer->CanPlaySource(Source))
				{
					MediaPlayer->OpenSource(Source);
				}
				else
				{
					// @todo: log warning/error
				}
			}

			OnUpdateFrame(MediaPlayer, Context);
		}
	}

protected:

	virtual bool ForceTemporaryPlayer() const
	{
		return true;
	}

	virtual void OnUpdateFrame(UMediaPlayer* MediaPlayer, const FMovieSceneContext& Context)
	{
		if (MediaPlayer->GetRate() != 0.f)
		{
			MediaPlayer->SetRate(0.f);
		}

		// @todo: Set this on the section itself
		if (MediaPlayer->GetTime() != MediaTime)
		{
			MediaPlayer->Seek(MediaTime);
		}
	}

	UMediaSource* Source;
	FTimespan MediaTime;
};


struct FMediaSectionExecutionToken
	: FMediaSectionPreRollExecutionToken
{
	FMediaSectionExecutionToken(UMediaSource* InSource, FTimespan InMediaTime)
		: FMediaSectionPreRollExecutionToken(InSource, InMediaTime)
		, PlaybackRate(1.f)
	{
	}

	virtual bool ForceTemporaryPlayer() const
	{
		return false;
	}

	virtual void OnUpdateFrame(UMediaPlayer* MediaPlayer, const FMovieSceneContext& Context) override
	{
		// Play and update the media player
		if (Context.GetStatus() == EMovieScenePlayerStatus::Playing)
		{
			if (MediaPlayer->GetRate() != PlaybackRate)
			{
				MediaPlayer->SetRate(PlaybackRate);
				MediaPlayer->Seek(MediaTime);
			}
		}
		else
		{
			MediaPlayer->SetRate(0.0f);
			MediaPlayer->Seek(MediaTime);
		}
	}

	float PlaybackRate;
};


/* FMovieSceneMediaSectionTemplate structors
 *****************************************************************************/

FMovieSceneMediaSectionTemplate::FMovieSceneMediaSectionTemplate(const UMovieSceneMediaSection& InSection, const UMovieSceneMediaTrack& InTrack)
	: PropertyData(InTrack.GetPropertyName(), InTrack.GetPropertyPath(), NAME_None, "OnMediaPlayerChanged")
{
	Params.Source = InSection.GetMediaSource();
	Params.SectionStartTime = InSection.GetStartTime();
}


/* FMovieSceneEvalTemplate interface
 *****************************************************************************/

void FMovieSceneMediaSectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	if (!Params.Source)
	{
		return;
	}

	if (Context.IsPreRoll())
	{
		// Use the end of the preroll range if we're in shot preroll, otherwise start at frame 0
		// @todo: account for start offset and video time dilation if/when these are added
		float FirstMediaFrameTime = Context.HasPreRollEndTime() ? Context.GetPreRollEndTime() - Params.SectionStartTime : 0.f;
		ExecutionTokens.Add(FMediaSectionPreRollExecutionToken(Params.Source, FTimespan(int64(ETimespan::TicksPerSecond * FirstMediaFrameTime))));
	}
	else if (!Context.IsPostRoll())
	{
		// Only update the video if we're not postrolling (which only should happen when playing in reverse through the preroll range)

		// @todo: account for start offset and video time dilation if/when these are added
		float MediaTime = Context.GetTime() - Params.SectionStartTime;
		ExecutionTokens.Add(FMediaSectionExecutionToken(Params.Source, FTimespan(int64(ETimespan::TicksPerSecond * MediaTime))));
	}
}


UScriptStruct& FMovieSceneMediaSectionTemplate::GetScriptStructImpl() const
{
	return *StaticStruct();
}


void FMovieSceneMediaSectionTemplate::Initialize(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const
{
	// Null out any prerolling video textures right at the start of the frame, before any other video sections update through their execution tokens.
	if (!Context.IsPreRoll())
	{
		return;
	}

	FMediaSectionData& SectionData = PersistentData.GetSectionData<FMediaSectionData>();
	UMediaPlayer* TemporaryMediaPlayer = SectionData.GetTemporaryMediaPlayer();

	for (TWeakObjectPtr<> WeakObject : Player.FindBoundObjects(Operand))
	{
		if (UObject* Object = WeakObject.Get())
		{
			UMediaPlayer* MediaPlayerFromProperty = SectionData.PropertyBindings->GetCurrentValue<UMediaPlayer*>(*Object);

			// Ensure the temporary (prerolling) player has no video texture. It will get
			// created when the video is actually evaluated for real. This will only be
			// set if we've scrubbed backwards out of a video section into its pre-roll
			
			// @todo gmp: fix me media framework 3.0
			/*
			TemporaryMediaPlayer->SetVideoTexture(nullptr);
			*/

			if (MediaPlayerFromProperty == TemporaryMediaPlayer)
			{
				// Ensure the property is not set if we're prerolling (so we don't display preroll frames)
				FMovieSceneAnimTypeID PropertyID = SectionData.PropertyID;
				Player.RestorePreAnimatedState(*Object, [=](FMovieSceneAnimTypeID In){ return In == PropertyID; });
			}
		}
	}
}


void FMovieSceneMediaSectionTemplate::Setup(FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const
{
	PropertyData.SetupTrack<FMediaSectionData>(PersistentData);
}


void FMovieSceneMediaSectionTemplate::SetupOverrides()
{
	EnableOverrides(RequiresSetupFlag | RequiresInitializeFlag);
}
