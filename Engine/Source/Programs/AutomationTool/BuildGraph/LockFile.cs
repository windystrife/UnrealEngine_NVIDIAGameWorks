// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Text;
using System.Threading;
using Tools.DotNETCommon;
using UnrealBuildTool;

namespace AutomationTool
{
    /// <summary>
    /// Utility class which creates a file and obtains an exclusive lock on it. Used as a mutex between processes on different machines through a network share.
    /// </summary>
    static class LockFile
    {
        public static void TakeLock(DirectoryReference LockDirectory, TimeSpan Timeout, System.Action Callback)
        {
            string LockFilePath = Path.Combine(LockDirectory.FullName, ".lock");

            FileStream Stream = null;
            DateTime StartTime = DateTime.Now;
            DateTime Deadline = StartTime.Add(Timeout);

            try
            {
                DirectoryReference.CreateDirectory(LockDirectory);

                for (int Iterations = 0; ; ++Iterations)
                {
                    // Attempt to create the lock file. Ignore any IO exceptions. Stream will be null if this fails.
                    try { Stream = new FileStream(LockFilePath, FileMode.Create, FileAccess.Write, FileShare.Read, 4096, FileOptions.DeleteOnClose); }
                    catch (IOException) { }

                    if (Stream != null)
                    {
                        // If we have a stream, we've taken the lock.
                        try
                        {
                            // Write the machine name to the file.
                            Stream.Write(Encoding.UTF8.GetBytes(Environment.MachineName));
                            Stream.Flush();
                            break;
                        }
                        catch
                        {
                            throw new AutomationException("Failed to write to the lock file '{0}'.", LockFilePath);
                        }
                    }

                    // We've failed to take the lock. Throw an exception if the timeout has elapsed.
                    // Otherwise print a log message and retry.
                    var CurrentTime = DateTime.Now;
                    if (CurrentTime >= Deadline)
                    {
                        throw new AutomationException("Couldn't create lock file '{0}' after {1} seconds.", LockFilePath, CurrentTime.Subtract(StartTime).TotalSeconds);
                    }

                    if (Iterations == 0)
                    {
                        CommandUtils.Log("Waiting for lock file '{0}' to be removed...", LockFilePath);
                    }
                    else if ((Iterations % 30) == 0)
                    {
                        CommandUtils.LogWarning("Still waiting for lock file '{0}' after {1} seconds.", LockFilePath, CurrentTime.Subtract(StartTime).TotalSeconds);
                    }

                    // Wait for a while before retrying.
                    Thread.Sleep(1000);
                }

                // Invoke the user callback now that we own the lock.
                Callback();
            }
            finally
            {
                // Always dispose the lock file stream if we took the lock.
                // The file will delete on close.
                if (Stream != null)
                {
                    Stream.Dispose();
                    Stream = null;
                }
            }
        }
    }
}
