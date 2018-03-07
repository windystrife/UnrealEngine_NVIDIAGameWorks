using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// Public TVOS functions exposed to UAT
	/// </summary>
	public static class TVOSExports
	{
		/// <summary>
		/// 
		/// </summary>
		/// <param name="InProject"></param>
		/// <param name="Distribution"></param>
		/// <param name="MobileProvision"></param>
		/// <param name="SigningCertificate"></param>
		/// <param name="TeamUUID"></param>
		/// <param name="bAutomaticSigning"></param>
		public static void GetProvisioningData(FileReference InProject, bool Distribution, out string MobileProvision, out string SigningCertificate, out string TeamUUID, out bool bAutomaticSigning)
		{
			IOSProjectSettings ProjectSettings = ((TVOSPlatform)UEBuildPlatform.GetBuildPlatform(UnrealTargetPlatform.TVOS)).ReadProjectSettings(InProject);

			IOSProvisioningData Data = ((IOSPlatform)UEBuildPlatform.GetBuildPlatform(UnrealTargetPlatform.TVOS)).ReadProvisioningData(ProjectSettings, Distribution);
			if(Data == null)
			{
				MobileProvision = null;
				SigningCertificate = null;
				TeamUUID = null;
				bAutomaticSigning = false;
			}
			else
			{
				MobileProvision = Data.MobileProvision;
				SigningCertificate = Data.SigningCertificate;
				TeamUUID = Data.TeamUUID;
				bAutomaticSigning = ProjectSettings.bAutomaticSigning;
			}
		}

		/// <summary>
		/// 
		/// </summary>
		/// <param name="Config"></param>
		/// <param name="ProjectFile"></param>
		/// <param name="InProjectName"></param>
		/// <param name="InProjectDirectory"></param>
		/// <param name="InExecutablePath"></param>
		/// <param name="InEngineDir"></param>
		/// <param name="bForDistribution"></param>
		/// <param name="CookFlavor"></param>
		/// <param name="bIsDataDeploy"></param>
		/// <param name="bCreateStubIPA"></param>
		/// <returns></returns>
		public static bool PrepForUATPackageOrDeploy(UnrealTargetConfiguration Config, FileReference ProjectFile, string InProjectName, DirectoryReference InProjectDirectory, string InExecutablePath, DirectoryReference InEngineDir, bool bForDistribution, string CookFlavor, bool bIsDataDeploy, bool bCreateStubIPA)
		{
			return new UEDeployTVOS().PrepForUATPackageOrDeploy(Config, ProjectFile, InProjectName, InProjectDirectory.FullName, InExecutablePath, InEngineDir.FullName, bForDistribution, CookFlavor, bIsDataDeploy, bCreateStubIPA);
		}

        /// <summary>
        /// 
        /// </summary>
        /// <param name="ProjectFile"></param>
        /// <param name="Config"></param>
        /// <param name="ProjectDirectory"></param>
        /// <param name="bIsUE4Game"></param>
        /// <param name="GameName"></param>
        /// <param name="ProjectName"></param>
        /// <param name="InEngineDir"></param>
        /// <param name="AppDirectory"></param>
        /// <param name="bSupportsPortrait"></param>
        /// <param name="bSupportsLandscape"></param>
        /// <param name="bSkipIcons"></param>
        /// <returns></returns>
        public static bool GeneratePList(FileReference ProjectFile, UnrealTargetConfiguration Config, DirectoryReference ProjectDirectory, bool bIsUE4Game, string GameName, string ProjectName, DirectoryReference InEngineDir, DirectoryReference AppDirectory, out bool bSupportsPortrait, out bool bSupportsLandscape, out bool bSkipIcons)
		{
			return new UEDeployTVOS().GeneratePList(ProjectFile, Config, ProjectDirectory.FullName, bIsUE4Game, GameName, ProjectName, InEngineDir.FullName, AppDirectory.FullName, out bSupportsPortrait, out bSupportsLandscape, out bSkipIcons);
		}
	}
}
