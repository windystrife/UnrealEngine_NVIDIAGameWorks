INCLUDE TOOL
============

The IncludeTool utility can be used to convert existing C++ projects into an include-what-you-use style. It does so by attempting to form self-contained
translation units out of each header file.

While it can save a lot of time, it is based on a heuristic, and is highly unlikely to produce perfect output - rather, it may produce a 
starting point from which you can make manual edits to complete the transition. Even then, it operates by brute force (so is agnonizingly slow; in the 
order of taking several days to complete for large projects), is painful to iterate on, may require project-specific modifications, 
and can be obtuse to debug.

It has been used to convert a number of internal Epic projects and the engine itself, and while we provide it as a courtesy, the level of 
support we can provide for it will be limited. 

OPERATION
---------

IncludeTool operates in several phases:

* UnrealBuildTool is invoked with a target to generate a list of build steps. The project's editor target is recommended, since it typically includes more 
  code paths than any other target type. We also recommend compiling for Linux using the cross-compile toolchain from windows, so as to use the Clang toolchain.
  Using Clang is important, because Visual C++ doesn't do two phase name lookup. That fails to find dependencies from template classes on dependent classes 
  until those templates are instantiated, which can make a real mess of the output.  

* The source files are partially preprocessed using an internal preprocessor. This does not transform the tokenized source file itself, but determines which blocks of 
  code are active and inactive for the current target, and splits each file into a series of *fragments*. Each fragment defines a range of lines in the source file 
  between #include directives, meaning that a single-file version of any source file can be assembled at any point by recursively following #include directives and 
  concatenating the list of fragments encountered.
  
  This step is important, because it is the list of fragments which IncludeTool is going to optimize. There are several restrictions that IncludeTool validates during 
  this phase and issues warnings about - some appear pedantic and stylistic, but are required to ensure that the output is valid. Particularly notable are:
  
  * There should be no circular includes between header files
  * Included source file must be preprocessed in the same way by every translation unit (ie. any macro must be defined the same way in every context 
    that it is included).

* Each fragment is written to the working folder. To preserve line numbers, each source file is output in its original layout, but with lines belonging to other 
  fragments commented out.

* Source files are tokenized and searched for patterns that look like symbols that can be forward declared. (eg. "class Foo { ...")

* Each source file is brute-force compiled to determine which fragments it depends on to compile successfully. This is the most labor intensive part of the transformation,
  but the results from this analysis are stored in the working directory, and should be reusable if source files do not change. The tool also supports a "sharded" mode
  for using multiple PCs to compute the dependency data (see below).
  
  The search is structured as follows:

  * An input translation unit is expanded to be represented by a sequence of fragments (1...n).
  * A set of the required fragments (r) is initialized to only those fragments that were in the input file (ie. rather than being included by the input file).
  * A binary search is performed to find the longest sequence of fragments required for the source file to still successfully compile (1...m, with r). 
  * The last fragment in the sequence (m), is added to the required set (r). If 'm' has already been optimized, its dependencies are also added to r.
  * The binary search is repeated for the sequence of fragments (1...m - 1).
	  
* Each output file is written with a minimal set of includes for it to compile in isolation. The heuristic used attempts to directly include the header for any 
  dependency containing a symbol that is explicitly referenced by name, and only include other dependencies if they are not included recursively.

EXAMPLE USAGE
-------------

To just do the fragment analysis and output diagnostic information:

    -Mode=Scan -Target=FooEditor -Platform=Linux -Configuration=Development -WorkingDir=D:\Working

To optimize a codebase:

    -Mode=Optimize -Target=FooEditor -Platform=Linux -Configuration=Development -WorkingDir=D:\Working -OutputDir=D:\Output -SourceFiles=-/Engine/... -OptimizeFiles=-/Engine/... -OutputFiles=-/Engine/...

Several filter arguments may be passed to limit source files iteration (-SourceFiles, -OptimizeFiles, -OutputFiles). Each can take a semicolon list of P4-style 
wildcards to include (/Engine/...) or exclude (-/Engine/Foo.cpp) files, or may specify a response file with rules, one per line (@D:\Filter.txt). In general,
you probably don't want to re-optimize engine code, so it makes sense to exclude all of those from analysis.

Since the program can take so long to run, it does have a facility for running in "sharded" mode. If you have several PCs all synced up to the same source tree,
you can use the -Shard=N and -NumShards=M to configure a machine to only consider a portion of the input set. The working directories should be in the same location
on each machine, and the resulting working directories can be copied together and used for a final run on a single machine to generate output files.

See comments in Program.cs for other modes (the ToolMode enum) and command-line options (the CommandLineOptions class).
