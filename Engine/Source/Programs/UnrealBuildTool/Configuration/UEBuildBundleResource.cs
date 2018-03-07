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
	public class UEBuildBundleResource
	{
		/// <summary>
		/// 
		/// </summary>
		/// <param name="InResourcePath"></param>
		/// <param name="InBundleContentsSubdir"></param>
		/// <param name="bInShouldLog"></param>
		public UEBuildBundleResource(string InResourcePath, string InBundleContentsSubdir = "Resources", bool bInShouldLog = true)
		{
			ResourcePath = InResourcePath;
			BundleContentsSubdir = InBundleContentsSubdir;
			bShouldLog = bInShouldLog;
		}

		/// <summary>
		/// 
		/// </summary>
		public string ResourcePath = null;

		/// <summary>
		/// 
		/// </summary>
		public string BundleContentsSubdir = null;

		/// <summary>
		/// 
		/// </summary>
		public bool bShouldLog = true;
	}
}
