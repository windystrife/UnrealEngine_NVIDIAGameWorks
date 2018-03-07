using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace UnrealBuildTool
{
	/// <summary>
	/// Attribute which can be applied to a TargetRules-dervied class to indicate which configurations it supports
	/// </summary>
	[AttributeUsage(AttributeTargets.Class, AllowMultiple = true)]
	public class SupportedConfigurationsAttribute : Attribute
	{
		/// <summary>
		/// Array of supported platforms
		/// </summary>
		public readonly UnrealTargetConfiguration[] Configurations;

		/// <summary>
		/// Initialize the attribute with a list of configurations
		/// </summary>
		/// <param name="Configurations">Variable-length array of configuration arguments</param>
		public SupportedConfigurationsAttribute(params UnrealTargetConfiguration[] Configurations)
		{
			this.Configurations = Configurations;
		}
	}
}
