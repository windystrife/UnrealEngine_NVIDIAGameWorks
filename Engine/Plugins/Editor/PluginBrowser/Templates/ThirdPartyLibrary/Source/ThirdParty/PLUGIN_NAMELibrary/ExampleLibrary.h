#if defined _WIN32 || defined _WIN64
#define DLLIMPORT __declspec(dllimport)
#else
#define DLLIMPORT
#endif

DLLIMPORT void ExampleLibraryFunction();