Unreal Engine HTML5 SDK. 

  - Download emscripten SDK from:
    o https://kripken.github.io/emscripten-site/docs/getting_started/downloads.html
      > scroll down to "Windows"
        o and click on "Portable Emscripten SDK for Windows" download link
        o this zip file contains a bin/elevate.exe file

      > scroll down to "Linux and Mac OS X"
        o and click on "Portable Emscripten SDK for Linux and OS X" download link
        o this tar file contains emsdk_manifest.json file (not available in the windows zip)

    o unpack the compressed tar.gz archive

    o the two interested files are:
      > the (python script file) emsdk (for double checking the s3.amazonaws.com path below)
      > and emsdk_manifest.json


  - scan emsdk_manifest.json and look for the interested emscripten version (at the end of the file)
    o for example, at the time this was written, emscripten 1.35.0 was latest
    o and the required tools for Win64 looked like this:
      > ["clang-e1.35.0-64bit", "node-4.1.1-64bit", "python-2.7.5.3-64bit", "emscripten-1.35.0"]

    o scan the json file again for the respective tool "id-version-bitness"
    o and download the tools using the following base path:
      > https://s3.amazonaws.com/mozilla-games/emscripten/packages/<url of tool to fetch>


    o for example, Win64:
      > wget https://s3.amazonaws.com/mozilla-games/emscripten/packages/emscripten-clang_e1.35.0.zip
      > wget https://s3.amazonaws.com/mozilla-games/emscripten/packages/node_4.1.1_64bit.zip
      > wget https://s3.amazonaws.com/mozilla-games/emscripten/packages/python_2.7.5.3_64bit.zip

      > wget https://s3.amazonaws.com/mozilla-games/emscripten/packages/emscripten-1.35.0.zip


    o for example, Mac OSX:
      > wget https://s3.amazonaws.com/mozilla-games/emscripten/packages/emscripten-clang_e1.35.0.tar.gz
      > wget https://s3.amazonaws.com/mozilla-games/emscripten/packages/node-v4.1.1-darwin-x64.tar.gz

      > wget https://s3.amazonaws.com/mozilla-games/emscripten/packages/emscripten-1.35.0.tar.gz
        * NOTE: this last one will be the same as the last one in the Win64 example above...


  - Copy folders for each platforms to their respective folders


  - Update version strings in .../Engine/Source/Programs/UnrealBuildTool/HTML5/HTML5SDKInfo.cs
    o SDKVersion
    o NODE_JS_WIN
    o NODE_JS_MAC
  - NOTE: build flags may need to be updated to get rid of terse warning messages
    o those are done here: .../Engine/Source/Programs/UnrealBuildTool/HTML5/HTML5ToolChain.cs


  - Perforce: Delete older SDK, Checkin new SDK.




------------------------------------------------------------
the following is left here for reference...
------------------------------------------------------------

Unreal Engine HTML5 SDK. 
 
  - Download emscripten SDK at a seperate location. 
  - Replicate current directory structure.
  - Delete some unsused clang binaries to save space. 
	clang-cl
	clang-check
	llvm-c-test
	llvm-objdump
	llvm-mv
	llvm-rtdyld
	diagtool
	clang-format
  - Delete emscripten/tests folder from the emscripten directory to save space. 
  - Copy folders for each platforms to this folder, 
  - Update version string in HTML5SDKInfo.cs
  - Perforce: Delete older SDK, Checkin new SDK. 

   


