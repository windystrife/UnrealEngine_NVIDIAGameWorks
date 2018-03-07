#include "../../SDL_internal.h"

#if SDL_VIDEO_DRIVER_X11

#include "SDL_x11video.h"
#include "SDL_assert.h"
#include "SDL_x11vulkan.h"

#define DEFAULT_VULKAN  "libvulkan.so.1"
#define VULKAN_REQUIRES_DLOPEN
#if defined(VULKAN_REQUIRES_DLOPEN) && defined(SDL_LOADSO_DLOPEN)
#include <dlfcn.h>
#define VK_LoadObject(X)    dlopen(X, (RTLD_NOW|RTLD_GLOBAL))
#define VK_LoadFunction     dlsym
#define VK_UnloadObject     dlclose
#else
#define VK_LoadObject   SDL_LoadObject
#define VK_LoadFunction SDL_LoadFunction
#define VK_UnloadObject SDL_UnloadObject
#endif

int
X11_VK_LoadLibrary(_THIS, const char *path) {
    void *handle;

    if (_this->vk_data) {
        return SDL_SetError("OpenGL context already created");
    }

    /* Load the OpenGL library */
    if (path == NULL) {
        path = SDL_getenv("SDL_VULKAN_LIBRARY");
    }
    if (path == NULL) {
        path = DEFAULT_VULKAN;
    }
    _this->vk_config.dll_handle = VK_LoadObject(path);
    if (!_this->vk_config.dll_handle) {
#if defined(VULKAN_REQUIRES_DLOPEN) && defined(SDL_LOADSO_DLOPEN)
        SDL_SetError("Failed loading %s: %s", path, dlerror());
#endif
        return -1;
    }
    SDL_strlcpy(_this->vk_config.driver_path, path,
                SDL_arraysize(_this->vk_config.driver_path));

    /* Allocate OpenGL memory */
    _this->vk_data = (struct SDL_VKDriverData *) SDL_calloc(1,sizeof(struct SDL_VKDriverData));
    if (!_this->vk_data) {
        return SDL_OutOfMemory();
    }

    /* Load function pointers */
    handle = _this->vk_config.dll_handle;
    _this->vk_data->vkCreateXlibSurfaceKHR =
        (VkResult (*)(VkInstance, const VkXlibSurfaceCreateInfoKHR*, const VkAllocationCallbacks*, VkSurfaceKHR*))
        VK_LoadFunction(handle, "vkCreateXlibSurfaceKHR");

    if (!_this->vk_data->vkCreateXlibSurfaceKHR) {
        return SDL_SetError("Could not retrieve Vulkan functions");
    }

    return 0;
}

void
X11_VK_UnloadLibrary(_THIS) {

#if 0
    VK_UnloadObject(_this->vk_config.dll_handle);
    _this->vk_config.dll_handle = NULL;
#endif

    /* Free Vulkan memory */
    SDL_free(_this->vk_data);
    _this->vk_data = NULL;
}


SDL_bool
FindExtension(VkInstanceCreateInfo* createInfo, const char* extension) {
    int i = 0;
    for(i = 0; i < createInfo->enabledExtensionCount; i++) {
        if(strcmp(createInfo->ppEnabledExtensionNames[i], extension) == 0) {
            return SDL_TRUE;
        }
    }
    return SDL_FALSE;
}

char**
X11_VK_GetRequiredInstanceExtensions(_THIS, unsigned int* count) {
    /** If we didn't allocated memory for the strings, let's do it now. */
    if(NULL == _this->vk_config.required_instance_extensions) {
        size_t length;
        _this->vk_config.required_instance_extensions = (char**)malloc(sizeof(char*) * 2);

        length = SDL_strlen(VK_KHR_SURFACE_EXTENSION_NAME) + 1;
        _this->vk_config.required_instance_extensions[0] = (char*)malloc(sizeof(char) * length );
        SDL_strlcpy(_this->vk_config.required_instance_extensions[0], VK_KHR_SURFACE_EXTENSION_NAME, length);

        length = SDL_strlen(VK_KHR_XLIB_SURFACE_EXTENSION_NAME) + 1;
        _this->vk_config.required_instance_extensions[1] = (char*)malloc(sizeof(char) * length );
        SDL_strlcpy(_this->vk_config.required_instance_extensions[1], VK_KHR_XLIB_SURFACE_EXTENSION_NAME, length);
    }
    *count = 2;
    return _this->vk_config.required_instance_extensions;
}

SDL_bool
X11_VK_CreateSurface(_THIS, SDL_Window* window, SDL_VkInstance instance, SDL_VkSurface* surface) {
    VkResult result;
    VkInstance inst = (VkInstance)instance;
    VkSurfaceKHR srf;
    SDL_WindowData *data = NULL;
    Display *display = NULL;
    VkXlibSurfaceCreateInfoKHR createInfo;

    if (!window) {
        SDL_SetError("'window' is null");
        return SDL_FALSE;
    }
    data = (SDL_WindowData *) window->driverdata;
    display = data->videodata->display;

    if (inst == VK_NULL_HANDLE) {
        SDL_SetError("'instance' is null");
        return SDL_FALSE;
    }

    createInfo.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
    createInfo.pNext = NULL;
    createInfo.flags = 0;
    createInfo.dpy = display;
    createInfo.window = data->xwindow;

    result = _this->vk_data->vkCreateXlibSurfaceKHR(inst, &createInfo, NULL, &srf);
//            result = vkCreateXlibSurfaceKHR(inst, &createInfo, NULL, &srf);
    if (result != VK_SUCCESS) {
        SDL_SetError("vkCreateXlibSurfaceKHR failed: %i", (int)result);
        return SDL_FALSE;
    }
    *surface = srf;
    return SDL_TRUE;
}

#endif /* SDL_VIDEO_DRIVER_X11 */
