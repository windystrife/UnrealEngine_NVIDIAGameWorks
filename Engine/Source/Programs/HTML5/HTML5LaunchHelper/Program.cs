// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Net;
using System.Net.Sockets;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Web;

namespace HTML5LaunchHelper
{
	class HttpServer
	{
#region extension to MIME type list
		// some basic mime types, not really important but for completeness sake.
		private static IDictionary<string, string> MimeTypeMapping = new Dictionary<string, string>(StringComparer.InvariantCultureIgnoreCase)
		{
			{".bin", "application/octet-stream"},
			{".css", "text/css"},
			{".dll", "application/octet-stream"},
			{".dmg", "application/octet-stream"},
			{".ear", "application/java-archive"},
			{".eot", "application/octet-stream"},
			{".exe", "application/octet-stream"},
			{".flv", "video/x-flv"},
			{".gif", "image/gif"},
			{".hqx", "application/mac-binhex40"},
			{".htc", "text/x-component"},
			{".htm", "text/html"},
			{".html", "text/html"},
			{".ico", "image/x-icon"},
			{".img", "application/octet-stream"},
			{".iso", "application/octet-stream"},
			{".jar", "application/java-archive"},
			{".jardiff", "application/x-java-archive-diff"},
			{".jng", "image/x-jng"},
			{".jnlp", "application/x-java-jnlp-file"},
			{".jpeg", "image/jpeg"},
			{".jpg", "image/jpeg"},
			{".js", "application/x-javascript"},
			{".mml", "text/mathml"},
			{".mng", "video/x-mng"},
			{".mov", "video/quicktime"},
			{".mp3", "audio/mpeg"},
			{".mpeg", "video/mpeg"},
			{".mpg", "video/mpeg"},
			{".msi", "application/octet-stream"},
			{".msm", "application/octet-stream"},
			{".msp", "application/octet-stream"},
			{".pdb", "application/x-pilot"},
			{".pdf", "application/pdf"},
			{".pem", "application/x-x509-ca-cert"},
			{".pl", "application/x-perl"},
			{".pm", "application/x-perl"},
			{".png", "image/png"},
			{".prc", "application/x-pilot"},
			{".ra", "audio/x-realaudio"},
			{".rar", "application/x-rar-compressed"},
			{".rpm", "application/x-redhat-package-manager"},
			{".rss", "text/xml"},
			{".run", "application/x-makeself"},
			{".sea", "application/x-sea"},
			{".shtml", "text/html"},
			{".sit", "application/x-stuffit"},
			{".swf", "application/x-shockwave-flash"},
			{".tcl", "application/x-tcl"},
			{".tk", "application/x-tcl"},
			{".txt", "text/plain"},
			{".war", "application/java-archive"},
			{".wbmp", "image/vnd.wap.wbmp"},
			{".wmv", "video/x-ms-wmv"},
			{".xml", "text/xml"},
			{".xpi", "application/x-xpinstall"},
			{".zip", "application/zip"},
	};
#endregion

		private HttpListener WebServer = new HttpListener();
		private string Root;

		public HttpServer(int Port, string ServerRoot, bool UseAllPrefixes)
		{
			Root = ServerRoot;
			WebServer.Prefixes.Add(string.Format("http://localhost:{0}/", Port.ToString()));

			if (UseAllPrefixes)
			{
				WebServer.Prefixes.Add (string.Format ("http://127.0.0.1:{0}/", Port.ToString ()));
				WebServer.Prefixes.Add (string.Format ("http://{0}:{1}/", Environment.MachineName, Port.ToString ()));
				IPHostEntry host = Dns.GetHostEntry (Dns.GetHostName ());
				foreach (IPAddress ip in host.AddressList) {
					if (ip.AddressFamily == AddressFamily.InterNetwork) {
						WebServer.Prefixes.Add (string.Format ("http://{0}:{1}/", ip.ToString (), Port.ToString ()));
					}
				}
			}
		}

		public bool Run()
		{
			System.Console.WriteLine("Starting Server at " + WebServer.Prefixes.First().ToString());
			try
			{
				WebServer.Start();
			}
			catch (HttpListenerException)
			{
				System.Console.WriteLine("WARNING: Port already in use... Exiting");
				return false;
			}

			Task.Factory.StartNew(()
				=>
				{
					while( WebServer.IsListening)
					{
						// Handle requests in threaded mode.
						Task.Factory.StartNew((Ctx)
							=>
							{
								var Context = Ctx as HttpListenerContext;
								try
								{
									RequestHandler(Context);
								}
								catch { }
								finally
								{
									Context.Response.Close();
								}

							}, WebServer.GetContext());
					}
				}
			);
			return true;
		}

		private void RequestHandler(HttpListenerContext Context)
		{
			if (Directory.Exists(Root + Context.Request.Url.LocalPath))
			{
				// Process the list of files found in the directory.
				string[] fileEntries = Directory.GetFileSystemEntries(Root + Context.Request.Url.LocalPath);
				string Response =   "<html>\n" +
									"<body>\n" +
									"<h2>Unreal WebServer</h2>\n" +
									"<h3>Directory listing for " + Context.Request.Url.LocalPath + "</h3>\n" +
									"<hr>\n";

				Response += "<table>\n";

				Response +=  "<tr>\n" +
								"\t<th>Filename</th>\n"+
								"\t<th>TimeStamp</th>\n"+
							 "</tr>\n";

				foreach (string fileName in fileEntries)
				{
					string Slash = Directory.Exists(fileName) ? "/" : "" ;
					string Url = fileName.Replace(Root, WebServer.Prefixes.First().ToString());
					Response += "<tr>\n\t<td><a href=\"" + Url + "\">" + Path.GetFileName(fileName) + Slash + "</a></td>\n\t<td> " + File.GetLastAccessTime(fileName).ToString() + "</td>\n</tr>\n";
				}
				Response  += "</table></html>";
				byte[] buf = Encoding.UTF8.GetBytes(Response);
				Context.Response.AddHeader("Access-Control-Allow-Origin", "*");
				Context.Response.ContentLength64 = buf.Length;
				Context.Response.OutputStream.Write(buf, 0, buf.Length);
				Context.Response.Close();
			}
			else if (File.Exists(Root + Context.Request.Url.LocalPath))
			{
				string RequestedFile = Root + Context.Request.Url.LocalPath;
				string RequestedFileCompressed = Root + Context.Request.Url.LocalPath + "gz";
				if (File.Exists(RequestedFileCompressed))
				{
					RequestedFile = RequestedFileCompressed;
				}
				System.Console.WriteLine("Serving " + RequestedFile);
				using (Stream source = File.OpenRead(RequestedFile))
				{
					string Extention = Path.GetExtension(RequestedFile);
					string MimeType = "text/html";

					if (MimeTypeMapping.ContainsKey(Extention))
					{
						MimeType = MimeTypeMapping[Extention];
					}
					// This is the crux of serving pre-compressed files.
					if (Extention.EndsWith("gz"))
					{
						Context.Response.AddHeader("Content-Encoding", "gzip");
					}

					byte[] buffer = new byte[source.Length];
					source.Read(buffer, 0, buffer.Length);
					Context.Response.AddHeader("Access-Control-Allow-Origin", "*");
					Context.Response.ContentType = MimeType;
					Context.Response.ContentLength64 = buffer.Length;
					Context.Response.OutputStream.Write(buffer, 0, buffer.Length);
					Context.Response.Close();
				}
			}
			else
			{
				string RequestedFile = Root + Context.Request.Url.LocalPath;
				System.Console.WriteLine("Not Serving " + RequestedFile);
				string Response = "<html>\n" +
									"<body>\n" +
									"<h2>404 Not Found</h2>\n" +
									"</html>\n";

				byte[] buf = Encoding.UTF8.GetBytes(Response);
				Context.Response.ContentLength64 = buf.Length;
				Context.Response.StatusCode = (int)HttpStatusCode.NotFound;
				Context.Response.ContentType = "text/html";
				Context.Response.OutputStream.Write(buf, 0, buf.Length);
				Context.Response.Close();
			}
		}

		public void Stop()
		{
			WebServer.Stop();
			WebServer.Close();
		}
	}

	class ArgumentName : Attribute
	{
		public string Name;
		public ArgumentName(string _Name)
		{
			Name = _Name;
		}
	}

	class DefaultArgument: Attribute
	{
		public string Value;
		public DefaultArgument(string _Value)
		{
			Value = _Value;
		}
	}

	// Various command line options supported.
	class Arguments
	{
		// if this is set -  This browser is spawned and the web server blocks till the browser quits.
		// if this not set - Just the server starts up and waits for key to quit.
		[ArgumentName("-Browser="), DefaultArgument("")]
		public string Browser
		{
			get;
			set;
		}

		[ArgumentName("-ServerRoot="), DefaultArgument("./")]
		public string ServerRoot
		{
			get;
			set;
		}

		[ArgumentName("-ServerPort="), DefaultArgument("8000")]
		public string ServerPort
		{
			get;
			set;
		}

		[ArgumentName("-BrowserCommandLine="), DefaultArgument("")]
		public string BrowserCommandLine
		{
			get;
			set;
		}

		[ArgumentName("-UseAllPrefixes="), DefaultArgument("FALSE")]
		public string UseAllPrefixes
		{
			get;
			set;
		}


		public Arguments()
		{}

		public bool Parse(string[] args)
		{
			PropertyInfo[] Infos = typeof(Arguments).GetProperties();
			foreach( var Info in Infos )
			{
				object[] Attributes = Info.GetCustomAttributes(false);

				string Name = null;
				string DefaultValue = null;

				foreach( var Att in Attributes)
				{
					if (Att.GetType() == typeof(ArgumentName))
					{
						Name = (Att as ArgumentName).Name;
					}
					if (Att.GetType() == typeof(DefaultArgument))
					{
						DefaultValue = (Att as DefaultArgument).Value;
					}
				}

				bool found = false;
				foreach(var arg in args)
				{
					if (arg.StartsWith(Name))
					{
						string val = arg.Replace(Name, "");
						Info.SetValue(this, val);
						found = true;
						break;
					}
				}

				if (!found && DefaultValue != null)
				{
					Info.SetValue(this, DefaultValue);
				}
				else if ( !found && DefaultValue == null)
				{
					return false;
				}
			}
			return true;
		}

		public void ShowParsedValues()
		{
			PropertyInfo[] Infos = typeof(Arguments).GetProperties();
			foreach (var Info in Infos)
			{
				object[] Attributes = Info.GetCustomAttributes(false);
				string Name = null;
				foreach (var Att in Attributes)
				{
					if (Att.GetType() == typeof(ArgumentName))
					{
						Name = (Att as ArgumentName).Name;
					}
				}
				if (Info.GetValue(this) != null)
				{
					System.Console.WriteLine("Name: " + Name + " " + Info.GetValue(this).ToString());
				}
			}
		}

		public void ShowAllOptions()
		{
			PropertyInfo[] Infos = typeof(Arguments).GetProperties();
			foreach (var Info in Infos)
			{
				object[] Attributes = Info.GetCustomAttributes(false);

				string Name = null;
				string DefaultValue = null;

				foreach (var Att in Attributes)
				{
					if (Att.GetType() == typeof(ArgumentName))
					{
						Name = (Att as ArgumentName).Name;
					}
					if (Att.GetType() == typeof(DefaultArgument))
					{
						DefaultValue = (Att as DefaultArgument).Value;
					}
				}
				System.Console.WriteLine("Option: {0}, Default Value {1}", Name, DefaultValue == null ? " None, this option is required " : DefaultValue);
			}
		}
	}

	class Program
	{
		static private List<Process> ProcessesToKill = new List<Process>();
		static private List<Process> ProcessesToWatch = new List<Process>();

		static bool IsRunningOnMac()
		{
			PlatformID Platform = Environment.OSVersion.Platform;
			switch (Platform)
			{
				case PlatformID.Unix:
					return System.IO.File.Exists("/System/Library/CoreServices/SystemVersion.plist");
				case PlatformID.MacOSX:
					return true;
			}
			return false;
		}

		static Process SpawnBrowserProcess(string bpath, string args)
		{
			var bIsSafari = bpath.Contains("Safari");

			var Result = new Process();
			if (IsRunningOnMac())
			{
				string BrowserArgs = bIsSafari ? "" : args;
				Result.StartInfo.FileName = "/usr/bin/open";
				Result.StartInfo.UseShellExecute = false;
				Result.StartInfo.RedirectStandardOutput = true;
				Result.StartInfo.RedirectStandardInput = true;
				Result.StartInfo.Arguments = String.Format("-nW \"{0}\" --args {1}", bpath, BrowserArgs);
				Result.EnableRaisingEvents = true;
			}
			else
			{
				Result.StartInfo.FileName = bpath;
				Result.StartInfo.UseShellExecute = false;
				Result.StartInfo.RedirectStandardOutput = true;
				Result.StartInfo.RedirectStandardInput = true;
				Result.StartInfo.Arguments = args;
				Result.EnableRaisingEvents = true;
			}

			Result.Start();

			if (bIsSafari)
			{
				// Give Safari time to open...
				System.Threading.Thread.Sleep(1500);
				var Proc = new Process();
				Proc.StartInfo.FileName = "/usr/bin/osascript";
				Proc.StartInfo.UseShellExecute = false;
				Proc.StartInfo.RedirectStandardOutput = true;
				Proc.StartInfo.RedirectStandardInput = true;
				Proc.StartInfo.Arguments = String.Format("-e 'tell application \"Safari\" to open location \"{0}\"'",  args);
				Proc.EnableRaisingEvents = true;
				Proc.Start();
				Proc.WaitForExit();
			}

			System.Console.WriteLine("Spawning Browser Process {0} with args {1}\n", bpath, args);
			return Result;

		}

		static void SpawnBrowserAndBlock(Arguments Args)
		{
			// Browsers can be multiprocess programs (Chrome, basically)
			bool bMultiprocessBrowser = Args.Browser.Contains("chrome");
			// so we need to catch spawning of other child processes. The trick is
			// they aren't really child-processes at all. There appears to be no real binding between the two,
			// So we kind of fudged it a bit here.
			var PrevProcesses = Process.GetProcesses();
			var FirstProcess = SpawnBrowserProcess(Args.Browser, Args.BrowserCommandLine);
			ProcessesToWatch.Add(FirstProcess);
			var ProcName = FirstProcess.ProcessName;

			// We should now have a list of processes to watch to exit.
			// Loop over the calling WaitForExit() until the list is empty.
			while (ProcessesToWatch.Count() > 0)
			{
				for (var i=0; i<ProcessesToWatch.Count(); ++i)
				{
					if (ProcessesToWatch[i].HasExited)
					{
						ProcessesToWatch.RemoveAt(i);
					}
				}

				if (bMultiprocessBrowser && FirstProcess != null && FirstProcess.HasExited)
				{
					var CurrentProcesses = Process.GetProcesses();
					foreach (var item in CurrentProcesses)
					{
						var bWasAlive = false;
						foreach (var pitem in PrevProcesses)
						{
							if (pitem.Id == item.Id)
							{
								bWasAlive = true;
							}
						}
						if (!bWasAlive)
						{
							try
							{
								if (!item.HasExited && item.ProcessName.StartsWith(ProcName))
								{
									var PID = item.Id;
									System.Console.WriteLine("Found Process {0} with PID {1} which started at {2}. Waiting on that process to end.", item.ProcessName, item.Id, item.StartTime.ToString());
									ProcessesToWatch.Add(item);
								}
							}
							catch { }
						}
					}
					FirstProcess = null;
				}

				// It is considered an error if any service processes have died.
				foreach (var proc in ProcessesToKill)
				{
					if (proc.HasExited)
					{
						System.Console.WriteLine("A spawned thread has died. Do you have a python server instance running?");
						HardShutdown();
					}
				}

				System.Threading.Thread.Sleep(250);
			}
			System.Console.WriteLine("All processes being watched have exited.\n");

			// All processes we cared about have finished, So it is time to clean up the services we spawned for them.
			foreach (var proc in ProcessesToKill)
			{
				if (!proc.HasExited)
				{
					System.Console.WriteLine("Killing spawned process {0}.\n", proc.Id);
					proc.Kill();
					proc.WaitForExit();
				}
			}
		}

		static void HardShutdown()
		{
			foreach (var proc in ProcessesToKill)
			{
				if (!proc.HasExited)
				{
					proc.Kill();
					proc.WaitForExit();
				}
			}

			foreach (var proc in ProcessesToWatch)
			{
				if (!proc.HasExited)
				{
					proc.Kill();
					proc.WaitForExit();
				}
			}
		}

		static int Main(string[] args)
		{
			System.Console.WriteLine("Version: 20170906"); // date: YYYYMMDD - needed to help figure out what version QA is running...
			var Args = new Arguments();
			if (Args.Parse(args))
			{
				string cwd = Directory.GetCurrentDirectory();
				if ( Args.ServerRoot.Equals("./") && cwd.Equals("/") ) // UE-45302
				{
					string path = System.IO.Path.GetDirectoryName( System.Reflection.Assembly.GetExecutingAssembly().GetName().CodeBase ).Replace("file:","");
					Directory.SetCurrentDirectory(path);
				}
				Args.ShowParsedValues();
			}
			else
			{
				System.Console.WriteLine("Incorrect Command line Options.. Exiting");
				Args.ShowAllOptions();
				return 0;
			}

			var Server = new HttpServer(Convert.ToInt32(Args.ServerPort),Args.ServerRoot, Args.UseAllPrefixes == "FALSE" ? false : true );
			if (!Server.Run())
			{
				return 0;
			}

			if ( Args.Browser != "" )
			{
				if ((!File.Exists(Args.Browser) && !IsRunningOnMac()) || (!Directory.Exists(Args.Browser) && IsRunningOnMac()))
				{
					System.Console.WriteLine("Browser Not found, Please check -Browser= option");
					return 0;
				}
				SpawnBrowserAndBlock(Args);
			}
			else
			{
				System.Console.WriteLine("Press Any key Quit Server");
				System.Console.ReadKey();
			}

			Server.Stop();
			return 0;
		}
	}
}
