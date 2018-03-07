using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace UnrealBuildTool
{
	/// <summary>
	/// Factory class for registering platforms at startup
	/// </summary>
	abstract class UEBuildPlatformFactory
	{
		/// <summary>
		/// Attempt to register a build platform, checking whether it is a valid platform in installed builds
		/// </summary>
		public void TryRegisterBuildPlatforms(SDKOutputLevel OutputLevel, bool bValidatingPlatforms)
		{
			// We need all platforms to be registered when we run -validateplatform command to check SDK status of each
			if (bValidatingPlatforms || InstalledPlatformInfo.IsValidPlatform(TargetPlatform))
			{
				RegisterBuildPlatforms(OutputLevel);
			}
		}

		/// <summary>
		/// Gets the target platform for an individual factory
		/// </summary>
		protected abstract UnrealTargetPlatform TargetPlatform
		{
			get;
		}

		/// <summary>
		/// Register the platform with the UEBuildPlatform class
		/// </summary>
		protected abstract void RegisterBuildPlatforms(SDKOutputLevel OutputLevel);
	}
}
