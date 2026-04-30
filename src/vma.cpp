#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
    #define VK_USE_PLATFORM_WIN32_KHR
#endif
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
