// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Net;
using System.Net.NetworkInformation;
using System.Reflection;
using System.Runtime.Remoting;
using System.Runtime.Remoting.Channels;
using System.Runtime.Remoting.Channels.Ipc;
using System.Runtime.Remoting.Channels.Tcp;
using System.Runtime.Remoting.Lifetime;
using System.Threading;
using System.Windows.Forms;
using System.Net.Sockets;
using System.Text;
using DotNETUtilities;

namespace RPCUtility
{
	public class RPCUtilityApplication
	{

		[STAThread]
		static Int32 Main( string[] args )
		{
			Int64 ExitCode = 0;

			// No parameters means run as a server
			if( args.Length == 0 )
			{
				Console.WriteLine("[{0} {1}]", DateTime.Now.ToLongDateString(), DateTime.Now.ToLongTimeString());
				Console.WriteLine("RPCUtility no longer runs as a server. Use UnrealRemoteTool instead.");
			}
			else
			{
				Debug.WriteLine( "Client starting up" );
				try
				{
					// Extract the command line parameters
					if( args.Length < 2 || 
						(args[1] == "RPCBatchUpload" && args.Length != 3) ||
						(args[1] == "RPCSoakTest" && args.Length != 2) ||
						((args[1] == "RPCUpload" || args[1] == "RPCDownload") && args.Length != 4))
					{
						throw new Exception( "\n\nWrong number of arguments!\n" + 
							"Usage: RPCUtility.exe MachineName RemoteWorkingDirectory CommandName [CommandArg...]\n" +
							"       RPCUtility.exe MachineName RPCBatchUpload CommandFile\n" +
							"       RPCUtility.exe MachineName RPCUpload LocalFilePath RemoteFilePath\n" +
							"       RPCUtility.exe MachineName RPCDownload RemoteFilePath LocalFilePath\n" +
							"       RPCUtility.exe MachineName RPCSoakTest\n");
					}

					string MachineName = args[0];

					// optional strings
					string WorkingDirectory = "";
					string CommandName = "";
					string CommandArgs = "";
					string SourceFilename = "";
					string DestFilename = "";

					if (args[1] == "RPCBatchUpload" || args[1] == "RPCGetFileInfo")
					{
						CommandName = args[1];
						SourceFilename = args[2];
					}
					else if (args[1] == "RPCUpload" || args[1] == "RPCDownload")
					{
						CommandName = args[1];
						SourceFilename = args[2];
						DestFilename = args[3];
					}
					else if (args[1] == "RPCSoakTest")
					{
						CommandName = args[1];
					}
					else
					{
						WorkingDirectory = args[1];
						CommandName = args[2];
						if (args.Length > 3)
						{
							CommandArgs = String.Join(" ", args, 3, args.Length - 3);
						}

						Debug.WriteLine("    Machine name      : " + MachineName);
						Debug.WriteLine("    Working directory : " + WorkingDirectory);
						Debug.WriteLine("    Command name      : " + CommandName);
						Debug.WriteLine("    Command args      : " + CommandArgs);
					}


					// Make sure we can ping the remote machine before we continue
					if( RPCCommandHelper.PingRemoteHost( MachineName ) )
					{
						Socket ClientSocket = RPCCommandHelper.ConnectToUnrealRemoteTool(MachineName);

// 						Hashtable Command = new Hashtable();
// 						Command["WorkingDirectory"] = WorkingDirectory;
// 						Command["CommandName"] = CommandName;
// 						Command["CommandArgs"] = CommandArgs;
// 
// 						Hashtable Reply = SendMessageWithReply(ClientSocket, Command);


						Random Generator = new Random();
						Int32 RetriesRemaining = 2;
						while( RetriesRemaining >= 0 )
						{
							try
							{
								Hashtable CommandResult = null;
								// Handle special copy command on the local side
								if (CommandName == "RPCBatchUpload")
								{
									// one line per local/remote file pair
									string[] FilePairs = File.ReadAllLines(SourceFilename);

									RPCCommandHelper.RPCBatchUpload(ClientSocket, FilePairs);
								}
								else if (CommandName == "RPCUpload")
								{
									CommandResult = RPCCommandHelper.RPCUpload(ClientSocket, SourceFilename, DestFilename);
								}
								else if (CommandName == "RPCDownload")
								{
									CommandResult = RPCCommandHelper.RPCDownload(ClientSocket, SourceFilename, DestFilename);
								}
								else if (CommandName == "RPCGetFileInfo")
								{
									DateTime A;
									long B;
									RPCCommandHelper.GetFileInfo(ClientSocket, SourceFilename, DateTime.UtcNow, out A, out B);
									Console.WriteLine("File info for {0}: Size = {1}, ModTime = {2}", SourceFilename, B, A);
								}
								else if (CommandName == "RPCSoakTest")
								{
									RPCCommandHelper.RPCSoakTest(ClientSocket);
								}
								else
								{
									CommandResult = RPCCommandHelper.RPCCommand(ClientSocket, WorkingDirectory, CommandName, CommandArgs, null);
								}

								if (CommandResult != null)
								{
									if (CommandResult["ExitCode"] != null)
									{
										ExitCode = (Int64)CommandResult["ExitCode"];
									}
									Console.WriteLine(CommandResult["CommandOutput"] as string);
								}
								break;
							}
							catch( Exception Ex3 )
							{
								if( RetriesRemaining > 0 )
								{
									// Generate a random retry timeout of 3-5 seconds
									Int32 RetryTimeoutMS = 3000 + (Int32)( 2000 * Generator.NextDouble() );
									Console.WriteLine( "Retrying command after sleeping for " + RetryTimeoutMS + " milliseconds. Command is:" + CommandName + " " + CommandArgs );
									Thread.Sleep( RetryTimeoutMS );
								}
								else
								{
									// We've tried enough times, just throw the error
									throw new Exception( "Deep Exception, retries exhausted... ", Ex3 );
								}
								RetriesRemaining--;
							}
						}
					}
					else
					{
						Console.WriteLine( "Failed to ping remote host: " + MachineName );
						ExitCode = -3;
					}
				}
				catch( Exception Ex )
				{
					Console.WriteLine( "Outer Exception: " + Ex.ToString() );
					ExitCode = -1;
				}

				Debug.WriteLine( "Client shutting down" );
			}

			return (Int32)ExitCode;
		}
	}
}
