using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// Interface to allow exposing public methods from the toolchain to other assemblies
	/// </summary>
	public interface IAndroidToolChain
	{
		/// <summary>
		/// Finds the list of supported architectures
		/// </summary>
		/// <returns>The targeted architectures</returns>
		List<string> GetAllArchitectures();

		/// <summary>
		/// Finds the list of supported GPU architectures
		/// </summary>
		/// <returns>The targeted GPU architectures</returns>
		List<string> GetAllGPUArchitectures();
	}

	/// <summary>
	/// Interface to allow exposing public methods from the Android deployment context to other assemblies
	/// </summary>
	public interface IAndroidDeploy
	{
		/// <summary>
		/// 
		/// </summary>
		/// <param name="bDisallowPackagingDataInApk"></param>
		/// <returns></returns>
		bool PackageDataInsideApk(bool bDisallowPackagingDataInApk);

		/// <summary>
		/// 
		/// </summary>
		/// <param name="Architectures"></param>
		/// <param name="inPluginExtraData"></param>
		void SetAndroidPluginData(List<string> Architectures, List<string> inPluginExtraData);

		/// <summary>
		/// 
		/// </summary>
		/// <param name="ProjectFile"></param>
		/// <param name="ProjectName"></param>
		/// <param name="ProjectDirectory"></param>
		/// <param name="ExecutablePath"></param>
		/// <param name="EngineDirectory"></param>
		/// <param name="bForDistribution"></param>
		/// <param name="CookFlavor"></param>
		/// <param name="bIsDataDeploy"></param>
		/// <returns></returns>
		bool PrepForUATPackageOrDeploy(FileReference ProjectFile, string ProjectName, DirectoryReference ProjectDirectory, string ExecutablePath, string EngineDirectory, bool bForDistribution, string CookFlavor, bool bIsDataDeploy);
	}

	/// <summary>
	/// Public Android functions exposed to UAT
	/// </summary>
	public static class AndroidExports
	{
		/// <summary>
		/// 
		/// </summary>
		/// <param name="ProjectFile"></param>
		/// <returns></returns>
		public static IAndroidToolChain CreateToolChain(FileReference ProjectFile)
		{
			return new AndroidToolChain(ProjectFile, false, null, null);
		}

		/// <summary>
		/// 
		/// </summary>
		/// <param name="ProjectFile"></param>
		/// <returns></returns>
		public static IAndroidDeploy CreateDeploymentHandler(FileReference ProjectFile)
		{
			return new UEDeployAndroid(ProjectFile);
		}

		/// <summary>
		/// 
		/// </summary>
		/// <returns></returns>
		public static bool ShouldMakeSeparateApks()
		{
			return UEDeployAndroid.ShouldMakeSeparateApks();
		}

		/// <summary>
		/// 
		/// </summary>
		/// <param name="NDKArch"></param>
		/// <returns></returns>
		public static string GetUE4Arch(string NDKArch)
		{
			return UEDeployAndroid.GetUE4Arch(NDKArch);
		}

		/// <summary>
		/// 
		/// </summary>
		/// <param name="SourceFile"></param>
		/// <param name="TargetFile"></param>
		public static void StripSymbols(FileReference SourceFile, FileReference TargetFile)
		{
			AndroidToolChain ToolChain = new AndroidToolChain(null, false, null, null);
			ToolChain.StripSymbols(SourceFile, TargetFile);
		}
	}
}
