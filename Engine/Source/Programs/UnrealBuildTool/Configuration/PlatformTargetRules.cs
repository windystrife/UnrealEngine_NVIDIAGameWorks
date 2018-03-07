using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading.Tasks;

namespace UnrealBuildTool
{
	/// <summary>
	/// Stub partial class for XboxOne-specific target settings. 
	/// This class is not in a restricted location to simplify code paths in UBT. It is visible to all UE4 users, without NDA, and will appear
	/// empty to those without the XboxOne implementation code.
	/// </summary>
	public partial class XboxOneTargetRules
	{
	}

	/// <summary>
	/// Stub read-only wrapper for XboxOne-specific target settings.
	/// This class is not in a restricted location to simplify code paths in UBT. It is visible to all UE4 users, without NDA, and will appear
	/// empty to those without the XboxOne implementation code.
	/// </summary>
	public partial class ReadOnlyXboxOneTargetRules
	{
		/// <summary>
		/// The private mutable settings object
		/// </summary>
		private XboxOneTargetRules Inner;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Inner">The settings object to wrap</param>
		public ReadOnlyXboxOneTargetRules(XboxOneTargetRules Inner)
		{
			this.Inner = Inner;
		}
	}

	/// <summary>
	/// Stub partial class for PS4-specific target settings. 
	/// This class is not in a restricted location to simplify code paths in UBT. It is visible to all UE4 users, without NDA, and will appear
	/// empty to those without the PS4 implementation code.
	/// </summary>
	public partial class PS4TargetRules
	{
	}

	/// <summary>
	/// Stub read-only wrapper for XboxOne-specific target settings.
	/// This class is not in a restricted location to simplify code paths in UBT. It is visible to all UE4 users, without NDA, and will appear
	/// empty to those without the XboxOne implementation code.
	/// </summary>
	public partial class ReadOnlyPS4TargetRules
	{
		/// <summary>
		/// The private mutable settings object
		/// </summary>
		private PS4TargetRules Inner;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Inner">The settings object to wrap</param>
		public ReadOnlyPS4TargetRules(PS4TargetRules Inner)
		{
			this.Inner = Inner;
		}
	}
}
