using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace UnrealBuildTool
{
	/// <summary>
	/// 
	/// </summary>
	public class UEBuildFramework
	{
		/// <summary>
		/// 
		/// </summary>
		/// <param name="InFrameworkName"></param>
		public UEBuildFramework(string InFrameworkName)
		{
			FrameworkName = InFrameworkName;
		}

		/// <summary>
		/// 
		/// </summary>
		/// <param name="InFrameworkName"></param>
		/// <param name="InFrameworkZipPath"></param>
		public UEBuildFramework(string InFrameworkName, string InFrameworkZipPath)
		{
			FrameworkName = InFrameworkName;
			FrameworkZipPath = InFrameworkZipPath;
		}

		/// <summary>
		/// 
		/// </summary>
		/// <param name="InFrameworkName"></param>
		/// <param name="InFrameworkZipPath"></param>
		/// <param name="InCopyBundledAssets"></param>
		public UEBuildFramework(string InFrameworkName, string InFrameworkZipPath, string InCopyBundledAssets)
		{
			FrameworkName = InFrameworkName;
			FrameworkZipPath = InFrameworkZipPath;
			CopyBundledAssets = InCopyBundledAssets;
		}

		internal UEBuildModule OwningModule = null;

		/// <summary>
		/// 
		/// </summary>
		public string FrameworkName = null;

		/// <summary>
		/// 
		/// </summary>
		public string FrameworkZipPath = null;

		/// <summary>
		/// 
		/// </summary>
		public string CopyBundledAssets = null;
	}
}
