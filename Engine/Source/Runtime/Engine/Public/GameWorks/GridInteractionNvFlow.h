#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

#include "GridInteractionNvFlow.generated.h"

// NvFlow begin

UENUM(BlueprintType)
enum EInteractionChannelNvFlow
{
	EIC_Channel1 UMETA(DisplayName = "Channel1"),
	EIC_Channel2 UMETA(DisplayName = "Channel2"),
	EIC_Channel3 UMETA(DisplayName = "Channel3"),
	EIC_Channel4 UMETA(DisplayName = "Channel4"),
	EIC_Channel5 UMETA(DisplayName = "Channel5"),
	EIC_Channel6 UMETA(DisplayName = "Channel6"),
	EIC_Channel7 UMETA(DisplayName = "Channel7"),
	EIC_Channel8 UMETA(DisplayName = "Channel8"),

	EIC_MAX,
};

UENUM(BlueprintType)
enum EInteractionResponseNvFlow
{
	EIR_Ignore UMETA(DisplayName = "Ignore"),
	EIR_Receive UMETA(DisplayName = "Receive"),
	EIR_Produce UMETA(DisplayName = "Produce"),
	EIR_TwoWay UMETA(DisplayName = "Two way"),
	EIR_MAX,
};

USTRUCT(BlueprintType)
struct ENGINE_API FInteractionResponseContainerNvFlow
{
	GENERATED_USTRUCT_BODY()

#if !CPP      //noexport property
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = CollisionResponseContainer, meta = (DisplayName = "Channel1"))
	TEnumAsByte<enum EInteractionResponseNvFlow> Channel1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = CollisionResponseContainer, meta = (DisplayName = "Channel2"))
	TEnumAsByte<enum EInteractionResponseNvFlow> Channel2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = CollisionResponseContainer, meta = (DisplayName = "Channel3"))
	TEnumAsByte<enum EInteractionResponseNvFlow> Channel3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = CollisionResponseContainer, meta = (DisplayName = "Channel4"))
	TEnumAsByte<enum EInteractionResponseNvFlow> Channel4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = CollisionResponseContainer, meta = (DisplayName = "Channel5"))
	TEnumAsByte<enum EInteractionResponseNvFlow> Channel5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = CollisionResponseContainer, meta = (DisplayName = "Channel6"))
	TEnumAsByte<enum EInteractionResponseNvFlow> Channel6;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = CollisionResponseContainer, meta = (DisplayName = "Channel7"))
	TEnumAsByte<enum EInteractionResponseNvFlow> Channel7;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = CollisionResponseContainer, meta = (DisplayName = "Channel8"))
	TEnumAsByte<enum EInteractionResponseNvFlow> Channel8;
#endif

	union
	{
		struct
		{
			uint8 Channel1;			// 0
			uint8 Channel2;			// 1
			uint8 Channel3;			// 2
			uint8 Channel4;			// 3
			uint8 Channel5;			// 4
			uint8 Channel6;			// 5
			uint8 Channel7;			// 6
			uint8 Channel8;			// 7
		};
		uint8 EnumArray[8];
	};

	FInteractionResponseContainerNvFlow()
		: Channel1(EIR_TwoWay)
		, Channel2(EIR_Ignore)
		, Channel3(EIR_Ignore)
		, Channel4(EIR_Ignore)
		, Channel5(EIR_Ignore)
		, Channel6(EIR_Ignore)
		, Channel7(EIR_Ignore)
		, Channel8(EIR_Ignore)
	{
	}

	FORCEINLINE_DEBUGGABLE EInteractionResponseNvFlow GetResponse(EInteractionChannelNvFlow Channel) const
	{
		return (EInteractionResponseNvFlow)EnumArray[Channel];
	}

	void SetResponse(EInteractionChannelNvFlow Channel, EInteractionResponseNvFlow NewResponse)
	{
		if (Channel < ARRAY_COUNT(EnumArray))
		{
			EnumArray[Channel] = NewResponse;
		}
	}

};

// NvFlow end
