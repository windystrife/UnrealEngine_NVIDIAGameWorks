// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace PlatformInfo
{
	/** The target type of the given platform */
	enum class EPlatformType : uint8
	{
		/** The platform targets cooked monolithic game executables */
		Game,

		/** The platform targets uncooked modular editor executables and DLLs */
		Editor,

		/** The platform targets cooked monolithic game client executables (but no server code) */
		Client,

		/** The platform targets cooked monolithic game server executables (but no client code) */
		Server,
	};

	/** Available icon sizes (see FPlatformIconPaths) */
	enum class EPlatformIconSize : uint8
	{
		/** Normal sized icon (24x24) */
		Normal,

		/** Large sized icon (64x64) */
		Large,

		/** Extra large sized icon (128x128) */
		XLarge,
	};

	/** Flavor types used when filtering the platforms based upon their flags */
	enum class EPlatformFilter : uint8
	{
		/** Include all platform types */
		All,

		/** Include only build flavors */
		BuildFlavor,

		/** Include only cook flavors */
		CookFlavor,
	};

	/** Flags describing platform variants */
	namespace EPlatformFlags
	{
		typedef uint8 Flags;
		enum Flag
		{
			/** Nothing of interest */
			None = 0,

			/** The flavor generates different output when building (eg, 32 or 64-bit) */
			BuildFlavor = 1<<0,

			/** The flavor generates different output when cooking (eg, ATC or PVRTC texture format) */
			CookFlavor = 1<<1,
		};
	}

	enum class EPlatformSDKStatus : uint8
	{
		/** SDK status is unknown */
		Unknown,

		/** SDK is installed */
		Installed,

		/** SDK is not installed */
		NotInstalled,
	};

	/** Information about where to find the platform icons (for use by FEditorStyle) */
	struct FPlatformIconPaths
	{
		FPlatformIconPaths()
		{
		}

		FPlatformIconPaths(const FString& InIconPath)
			: NormalPath(InIconPath)
			, LargePath(InIconPath)
			, XLargePath(InIconPath)
		{
		}

		FPlatformIconPaths(const FString& InNormalPath, const FString& InLargePath)
			: NormalPath(InNormalPath)
			, LargePath(InLargePath)
			, XLargePath(InLargePath)
		{
		}

		FPlatformIconPaths(const FString& InNormalPath, const FString& InLargePath, const FString& InXLargePath)
			: NormalPath(InNormalPath)
			, LargePath(InLargePath)
			, XLargePath(InXLargePath)
		{
		}

		FName NormalStyleName;
		FString NormalPath;

		FName LargeStyleName;
		FString LargePath;

		FName XLargeStyleName;
		FString XLargePath;
	};

	/** Information about a given platform */
	struct FPlatformInfo
	{
		/** Name used to identify this platform, eg "Android_ATC" */
		FName PlatformInfoName;

		/** Name used to find the corresponding ITargetPlatform for this platform (also used by UAT) */
		FName TargetPlatformName;

		/** Vanilla name for this platform, eg "Android" for "Android_ATC" */
		FName VanillaPlatformName;

		/** Platform flavor, eg "ATC" for "Android_ATC" */
		FName PlatformFlavor;

		/** The friendly (and localized) display name of this platform */
		FText DisplayName;

		/** Type of this platform */
		EPlatformType PlatformType;

		/** Flags for this platform */
		EPlatformFlags::Flags PlatformFlags;

		/** Information about where to find the platform icons (for use by FEditorStyle) */
		FPlatformIconPaths IconPaths;

		/** Additional argument string data to append to UAT commands relating to this platform */
		FString UATCommandLine;

		/** Path under CarefullyRedist for the SDK.  FString so case sensitive platforms don't get messed up by a pre-existing FName of a different casing. */
		FString AutoSDKPath;

		/** Whether or not this platform SDK has been properly installed */
		EPlatformSDKStatus SDKStatus;

		/** Tutorial path for tutorial to install SDK */
		FString SDKTutorial;

		/** Name of sub-folder where binaries will be placed */
		FString BinaryFolderName;

		/** Name of this platform when loading INI files */
		FString IniPlatformName;

		/** Enabled for use */
		bool bEnabledForUse;

		/** Whether code projects for this platform require the host platform compiler to be installed. Host platforms typically have a SDK status of valid, but they can't necessarily build. */
		bool bUsesHostCompiler;

		/** Whether UAT closes immediately after launching on this platform, or if it sticks around to read output from the running process */
		bool bUATClosesAfterLaunch;

		/** Whether or not the platform is confidential in nature */
		bool bIsConfidential;

		/** An identifier that corresponds to UBT's UnrealTargetPlatform enum (and by proxy, FGenericPlatformMisc::GetUBTPlatform()) */
		FName UBTTargetId;

		/** Returns true if this platform is vanilla */
		FORCEINLINE bool IsVanilla() const
		{
			return PlatformFlavor.IsNone();
		}

		/** Returns true if this platform is a flavor */
		FORCEINLINE bool IsFlavor() const
		{
			return !PlatformFlavor.IsNone();
		}

		/** Get the icon name (for FEditorStyle) used by the given icon type for this platform */
		FName GetIconStyleName(const EPlatformIconSize InIconSize) const
		{
			switch(InIconSize)
			{
			case EPlatformIconSize::Normal:
				return IconPaths.NormalStyleName;
			case EPlatformIconSize::Large:
				return IconPaths.LargeStyleName;
			case EPlatformIconSize::XLarge:
				return IconPaths.XLargeStyleName;
			default:
				break;
			}
			return NAME_None;
		}

		/** Get the path to the icon on disk (for FEditorStyle) for the given icon type for this platform */
		const FString& GetIconPath(const EPlatformIconSize InIconSize) const
		{
			switch(InIconSize)
			{
			case EPlatformIconSize::Normal:
				return IconPaths.NormalPath;
			case EPlatformIconSize::Large:
				return IconPaths.LargePath;
			case EPlatformIconSize::XLarge:
				return IconPaths.XLargePath;
			default:
				break;
			}
			static const FString EmptyString = TEXT("");
			return EmptyString;
		}
	};

	/** Vanilla platform that may contain a set of flavors */
	struct FVanillaPlatformEntry
	{
		FVanillaPlatformEntry()
			: PlatformInfo(nullptr)
		{
		}

		FVanillaPlatformEntry(const FPlatformInfo* const InPlatformInfo)
			: PlatformInfo(InPlatformInfo)
		{
		}

		/** Information for this platform */
		const FPlatformInfo* PlatformInfo;

		/** Any flavors for this platform */
		TArray<const FPlatformInfo*> PlatformFlavors;
	};

	/** Simple wrapper class to allow range-based-for enumeration from a call to EnumeratePlatforms */
	class FPlatformEnumerator
	{
	public:
		FPlatformEnumerator(const FPlatformInfo* InPlatformsArray, const int32 InNumPlatforms)
			: PlatformsArray(InPlatformsArray)
			, CurrentPlatform(InPlatformsArray)
			, NumPlatforms(InNumPlatforms)
		{
		}

		FORCEINLINE const FPlatformInfo* begin() const
		{
			return PlatformsArray;
		}

		FORCEINLINE const FPlatformInfo* end() const
		{
			return PlatformsArray + NumPlatforms;
		}

		FORCEINLINE const FPlatformInfo* operator->() const
		{
			return CurrentPlatform;
		}

		FORCEINLINE const FPlatformInfo& operator*() const
		{
			return *CurrentPlatform;
		}

		FORCEINLINE FPlatformEnumerator& operator++()
		{
			++CurrentPlatform;
			return *this;
		}

		FORCEINLINE FPlatformEnumerator operator++(int)
		{
			FPlatformEnumerator Copy(*this);
			++CurrentPlatform;
			return Copy;
		}

		FORCEINLINE operator bool() const
		{
			return CurrentPlatform < end();
		}

	private:
		const FPlatformInfo* PlatformsArray;
		const FPlatformInfo* CurrentPlatform;
		int32 NumPlatforms;
	};

	/**
	 * Try and find the information for the given platform
	 * @param InPlatformName - The name of the platform to find
	 * @return The platform info if the platform was found, null otherwise
	 */
	DESKTOPPLATFORM_API const FPlatformInfo* FindPlatformInfo(const FName& InPlatformName);

	/**
	 * Try and find the vanilla information for the given platform
	 * @param InPlatformName - The name of the platform to find (can be a flavor, but you'll still get back the vanilla platform)
	 * @return The platform info if the platform was found, null otherwise
	 */
	DESKTOPPLATFORM_API const FPlatformInfo* FindVanillaPlatformInfo(const FName& InPlatformName);

	/**
	 * Get an array of all the platforms we know about
	 * @param OutNumPlatforms - The number of platforms in the array
	 * @return The pointer to the start of the platform array
	 */
	DESKTOPPLATFORM_API const FPlatformInfo* GetPlatformInfoArray(int32& OutNumPlatforms);

	/**
	 * Convenience function to enumerate all the platforms we know about (compatible with range-based-for)
	 *
	 * @param bAccessiblePlatformsOnly	If true, only the accessible platforms(installed, or could be installed) will be returned
	 * @return An enumerator for the platforms (see FPlatformEnumerator)
	 */
	DESKTOPPLATFORM_API FPlatformEnumerator EnumeratePlatformInfoArray(bool bAccessiblePlatformsOnly = true);

	/**
	 * Build a hierarchy mapping vanilla platforms to their flavors
	 * @param InFilter - Flags to control which kinds of flavors you want to include
	 * @return An array of vanilla platforms, potentially containing flavors
	 */
	DESKTOPPLATFORM_API TArray<FVanillaPlatformEntry> BuildPlatformHierarchy(const EPlatformFilter InFilter);

	/**
	* Build a hierarchy mapping for specified vanilla platform to it flavors
	* @param InPlatformName - Platform name to build hierarchy for, could be vanilla or flavor name
	* @param InFilter - Flags to control which kinds of flavors you want to include
	* @return Vanilla platform potentially containing flavors
	*/
	DESKTOPPLATFORM_API FVanillaPlatformEntry BuildPlatformHierarchy(const FName& InPlatformName, const EPlatformFilter InFilter);

	/**
	 * Get an array of all the platforms we know about
	 * @param OutNumPlatforms - The number of platforms in the array
	 * @return The pointer to the start of the platform array
	 */
	DESKTOPPLATFORM_API void UpdatePlatformSDKStatus(FString InPlatformName, EPlatformSDKStatus InStatus);

	/**
	* Update the display name for a platform
	* @param InPlatformName - The platform to update
	* @param InDisplayName - The new display name
	*/
	DESKTOPPLATFORM_API void UpdatePlatformDisplayName(FString InPlatformName, FText InDisplayName);

	/**
	* Returns an EPlatformType value from a string representation.
	* @param PlatformTypeName The string to get the EPlatformType for.
	* @return An EPlatformType value.
	*/
	DESKTOPPLATFORM_API EPlatformType EPlatformTypeFromString(const FString& PlatformTypeName);
}

namespace Lex
{
	DESKTOPPLATFORM_API FString ToString(const PlatformInfo::EPlatformType Value);

	inline void FromString(PlatformInfo::EPlatformType& OutValue, const TCHAR* Buffer)
	{
		OutValue = PlatformInfo::EPlatformTypeFromString(FString(Buffer));
	}
}