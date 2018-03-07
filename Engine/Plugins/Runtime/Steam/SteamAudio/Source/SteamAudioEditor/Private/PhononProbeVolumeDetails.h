//
// Copyright (C) Valve Corporation. All rights reserved.
//

#pragma once

#include "IDetailCustomization.h"
#include "Input/Reply.h"

class IDetailLayoutBuilder;
class IDetailChildrenBuilder;
class IPropertyHandle;
class APhononProbeVolume;

namespace SteamAudio
{
	class FPhononProbeVolumeDetails : public IDetailCustomization
	{
	public:
		static TSharedRef<IDetailCustomization> MakeInstance();

	private:
		virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;

		void OnGenerateBakedDataInfo(TSharedRef<IPropertyHandle> PropertyHandle, int32 ArrayIndex, IDetailChildrenBuilder& ChildrenBuilder);
		void OnClearBakedDataClicked(const int32 ArrayIndex);
		FText GetTotalDataSize();

		FReply OnGenerateProbes();
		TWeakObjectPtr<APhononProbeVolume> PhononProbeVolume;
	};
}