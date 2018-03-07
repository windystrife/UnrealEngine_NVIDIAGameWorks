using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// Public Linux functions exposed to UAT
	/// </summary>
	public static class WindowsExports
	{
		/// <summary>
		/// 
		/// </summary>
		/// <param name="ProjectFile"></param>
		/// <param name="ProjectName"></param>
		/// <param name="ProjectDirectory"></param>
		/// <param name="InTargetConfigurations"></param>
		/// <param name="InExecutablePaths"></param>
		/// <param name="EngineDirectory"></param>
		/// <returns></returns>
		public static bool PrepForUATPackageOrDeploy(FileReference ProjectFile, string ProjectName, DirectoryReference ProjectDirectory, List<UnrealTargetConfiguration> InTargetConfigurations, List<FileReference> InExecutablePaths, DirectoryReference EngineDirectory)
		{
	        BaseWindowsDeploy Deploy = new BaseWindowsDeploy();
            return Deploy.PrepForUATPackageOrDeploy(ProjectFile, ProjectName, ProjectDirectory.FullName, InTargetConfigurations, InExecutablePaths.Select(x => x.FullName).ToList(), EngineDirectory.FullName);
		}

		/// <summary>
		/// Tries to get the directory for an installed Visual Studio version
		/// </summary>
		/// <param name="Compiler">The compiler version</param>
		/// <param name="InstallDir">Receives the install directory on success</param>
		/// <returns>True if successful</returns>
		public static bool TryGetVSInstallDir(WindowsCompiler Compiler, out DirectoryReference InstallDir)
		{
			return WindowsPlatform.TryGetVSInstallDir(Compiler, out InstallDir);
		}

		/// <summary>
		/// Gets the path to MSBuild.exe
		/// </summary>
		/// <returns>Path to MSBuild.exe</returns>
		public static string GetMSBuildToolPath()
		{
			return VCEnvironment.GetMSBuildToolPath();
		}
	}
}
