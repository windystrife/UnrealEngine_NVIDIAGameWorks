Visual Studio Snippets for Unreal Engine C++ Projects
===========

How to install snippets?
===========

**Method One**
Paste .snippet files into: C:\Users\$user$\Documents\Visual Studio 2013\Code Snippets\Visual C++\My Code Snippets. Then restart VS.

**Method Two**
Open Visual Studio, navigate to TOOLS -> Code Snippets Manager… -> Import…

How to use snippets?
===========

**Method One**
Just start typing ue4... snippet list should be loaded in a form of combo box. Then use arrows to select snippet. Hit ENTER or TAB to insert snippet.

**Method Two**
Type all snippet name and hit TAB. You don't have to wait for VS to show snippet list.

To navigate between highlighted fields you can use TAB and SHIFT + TAB. After you enter all names, hit ENTER.

Snippets
===========

*ue4classa* – Blueprintable class that derives from an AActor. Parameters are: comment, class name and base class name.

*ue4classu* – Blueprintable class that derives from an UObject. Parameters are: comment, class name, base class name.

*ue4struct* – Simple structure. Parameters are: comment and name.

*ue4interface* – Simple ue4 interface. Parameters are: comment and name.

*ue4bpevent* – This function can be used as an event in blueprint. Parameters are: comment, UI category, virtual and const modifiers, function name and arguments.

*ue4bpfunc* – This function is available for blueprint logic. Parameters are: comment (parameters and return value), UI category, virtual and const modifiers, function name and arguments.

*ue4prop* – This read/write property is available everywhere (blueprint, instance and archetype details). Parameters are: comment, category, type and name.

*ue4enum* – Simple enum. Parameters are: comment, enum name, first member name and it’s comment.

*ue4enumdisplay* – Enum that can be used with blueprints. Parameters are: comment, enum name, first member name, it’s display name and comment.

*ue4log* – Simplest log line. Parameters are category, verbosity and message.

*ue4logdeclare* – Declaration of log category. Place this in main header of your project to allow logging. Parameters are: category, default verbosity and compile time verbosity.

*ue4logdefine* – Definition of log category. Place this in main code file. Parameter is category name.

*ue4logfloat* – Log line that can be used to print float value. Parameters are: category, verbosity and variable name.

*ue4logint* – This log line can be used to log an integer value. Parameters are: category, verbosity and variable name.

*ue4loguobj* – This log line is designed to log from inside of the objects. By default, square brackets contains a name of an object that writes the log. Parameters are: category, verbosity, message and name of a pointer to the object.

*ue4mark* – Can be used to mark changes in engine classes. Parameters are: Company symbol, task/ticket number, name and surname of a developer and short description of modification.

*ue4eve* - 9 snippets for each params combination. Can be used to create event. Parameters are: owning type and event type name.

*ue4del* - 9 snippets for each params combination. Can be used to create delegate. Parameters are: delegate type name and param type names.

*ue4delmul* - 9 snippets for each params combination. Can be used to create multicast delegate. Parameters are: delegate type name and param type names.

*ue4deldyn* - 9 snippets for each params combination. Can be used to create dynamic delegate. Parameters are: delegate type name, param type names and display values.

*ue4deldynmul* - 9 snippets for each params combination. Can be used to create dynamic multicast delegate. Parameters are: delegate type name, param type names and display values`.