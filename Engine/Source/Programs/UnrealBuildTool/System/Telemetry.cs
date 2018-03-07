// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text;

namespace UnrealBuildTool
{
	/// <summary>
	/// Telemetry service to collect data about execution.
	/// </summary>
	/// <remarks>
	/// You must provide an instance of a class that implements <see cref="Telemetry.IProvider"/> and has the <see cref="Telemetry.ProviderAttribute"/> attribute.
	/// Epic provides one internally, but external studios will need to provide one themselves.
	/// </remarks>
	public static class Telemetry
	{
		/// <summary>
		/// Provides an interface for a telemetry provider.
		/// </summary>
		public interface IProvider : IDisposable
		{
			/// <summary>
			/// Sends an event to the telemetry provider
			/// </summary>
			/// <param name="EventName">Name of the event</param>
			/// <param name="Attributes">list of attributes in key,value format.</param>
			void SendEvent(string EventName, IEnumerable<Tuple<string, string>> Attributes);
		}

		/// <summary>
		/// Attribute to designate a telemetry provider
		/// </summary>
		[AttributeUsage(AttributeTargets.Class, AllowMultiple = false)]
		public class ProviderAttribute : Attribute
		{
			/// <summary>
			/// If true, this provider is only created for Epic internal builds.
			/// </summary>
			public bool bEpicInternalOnly;
		}

		/// <summary>
		/// Get all assemblies in this domain which reference the passed-in assembly
		/// </summary>
		/// <param name="domain">The domain to search</param>
		/// <param name="target">The assembly to search for references to</param>
		/// <returns>All Assemblies that reference target.</returns>
		private static IEnumerable<Assembly> GetReferencingAssemblies(this AppDomain domain, Assembly target)
		{
			// return the target assembly and any assemblies in the domain that reference the target assembly.
			return domain.GetAssemblies().Where(asm => asm.GetReferencedAssemblies().Any(refd => refd.FullName == target.FullName)).Union(new[] { target });
		}


		static IProvider Provider;

		/// <summary>
		/// Called at app startup to initialize the telemetry provider.
		/// </summary>
		public static void Initialize()
		{
			var TelemetryInitStartTime = DateTime.UtcNow;

			// only collect telemetry on Epic builds
			bool bIsEpicInternal = File.Exists(@"..\Build\NotForLicensees\EpicInternal.txt");

			// reflect over the assembly and find the first eligible provider class.
			Provider =
				(from reflectionAssembly in AppDomain.CurrentDomain.GetReferencingAssemblies(Assembly.GetExecutingAssembly())
				 from T in reflectionAssembly.GetTypes()
				 // type must implement interface, contain the attribute, be concrete and default constructible.
				 where T.IsDefined(typeof(ProviderAttribute), true) && T.GetInterfaces().Contains(typeof(IProvider)) && T.IsClass && !T.IsAbstract && T.GetConstructor(Type.EmptyTypes) != null
				 // if it's an epic internal build, it's ok to use epic internal providers.
				 where bIsEpicInternal || !T.GetCustomAttribute<ProviderAttribute>().bEpicInternalOnly
				 // create the instance
				 select (IProvider)Activator.CreateInstance(T)).FirstOrDefault();

			if (UnrealBuildTool.bPrintPerformanceInfo)
			{
				var TelemetryInitTime = (DateTime.UtcNow - TelemetryInitStartTime).TotalSeconds;
				Log.TraceInformation("Telemetry initialization took " + TelemetryInitTime + "s. Provider Type = " + (Provider == null ? "null" : Provider.GetType().ToString()));
			}
		}

		/// <summary>
		/// Checks if the Telemetry systeme is available. Use to skip work to prepare telemetry events.
		/// </summary>
		/// <remarks>If unavailable, SendEvent will not crash, but will ignore the event.</remarks>
		/// <returns></returns>
		public static bool IsAvailable()
		{
			return Provider != null;
		}

		/// <summary>
		/// Helper to allow one to pass a series of key/value pairs as a single string list of the form: key1,value1,key2,value2,...
		/// Odd lengths are truncated. Pass an empty value using and empty string or null.
		/// </summary>
		/// <param name="EventName"></param>
		/// <param name="Attributes"></param>
		public static void SendEvent(string EventName, params string[] Attributes)
		{
			if (Provider == null) return;
			// zip even values as attribute names, odd values as attribute values.
			SendEvent(EventName, Attributes.Where((value, index) => (index % 2) == 0).Zip(Attributes.Where((value, index) => (index % 2) == 1), (key, value) => Tuple.Create(key, value)));
		}

		/// <summary>
		/// 
		/// </summary>
		/// <param name="EventName"></param>
		/// <param name="Attributes"></param>
		public static void SendEvent(string EventName, IEnumerable<Tuple<string, string>> Attributes)
		{
			if (Provider == null) return;

			Provider.SendEvent(EventName, Attributes);
		}

		/// <summary>
		/// Called at app shutdown to clean up the telemetry provider.
		/// </summary>
		public static void Shutdown()
		{
			if (Provider != null)
			{
				Provider.Dispose();
				Provider = null;
			}
		}
	}
}
