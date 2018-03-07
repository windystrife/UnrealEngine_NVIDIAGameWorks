SQLite database support requires the 'amalgamated' source code for sqlite,
obtainable at http://www.sqlite.org/download.html. It also requires you to
compile the engine from Github source code rather than using the precompiled
binary builds from the Unreal Launcher.

Download and extract the Unreal source as per the instructions on Github, then
extract the contents (*.cpp, *.h) from the sqlite amalgamated zip into
Engine/Source/ThirdParty/sqlite/sqlite and build it using VisualStudio.

If you are already building the engine from source it is necessary to rebuild
it once you've added the SQLite files.
