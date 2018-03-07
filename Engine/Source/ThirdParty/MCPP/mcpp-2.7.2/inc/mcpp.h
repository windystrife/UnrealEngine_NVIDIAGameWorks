/** mcpp.h: Interface for external applications. */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Callback for retrieving the contents of an include file.
 * If contents is NULL, mcpp is just querying to see if the file exists.
 * If contents is non-NULL, it should point to the contents of the file.
 * Note that MCPP will not manage the memory for you. You must free contents yourself.
 * Note that contents_size is the size of the buffer including the NULL terminator!
 */
typedef int (*get_file_contents_func)(void* user_data, const char* filename, const char** out_contents, size_t* out_contents_size);

/** File loader callback interface. */
struct file_loader
{
	get_file_contents_func get_file_contents;
	void* user_data;
};
typedef struct file_loader file_loader;

/** External interface for preprocessing a file with MCPP. */
extern int mcpp_run(
	const char* options,
	const char* filename,
	char** outfile,
	char** outerrors,
	file_loader in_file_loader
	);

#ifdef __cplusplus
}
#endif
