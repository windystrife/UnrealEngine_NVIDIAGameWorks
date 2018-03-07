using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace UnrealBuildTool
{
	/// <summary>
	/// Attribute which can be applied to a TargetRules-dervied class to indicate which platforms it supports
	/// </summary>
	[AttributeUsage(AttributeTargets.Class, AllowMultiple = true)]
	public class SupportedPlatformsAttribute : Attribute
	{
		/// <summary>
		/// Array of supported platforms
		/// </summary>
		public readonly UnrealTargetPlatform[] Platforms;

		/// <summary>
		/// Initialize the attribute with a list of platforms
		/// </summary>
		/// <param name="Platforms">Variable-length array of platform arguments</param>
		public SupportedPlatformsAttribute(params UnrealTargetPlatform[] Platforms)
		{
			this.Platforms = Platforms;
		}

		/// <summary>
		/// Initialize the attribute with all the platforms in a given category
		/// </summary>
		/// <param name="Category">Category of platforms to add</param>
		public SupportedPlatformsAttribute(UnrealPlatformClass Category)
		{
			this.Platforms = Utils.GetPlatformsInClass(Category);
		}
	}
}
