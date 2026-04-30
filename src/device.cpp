#include "device.h"
#include "vk_types.h"
#include "instance.h"
#include <cmath>
#include <vector>
#include <set>
#include <stdexcept>
#include <vulkan/vulkan_core.h>
#include <iostream>
static const std::vector<const char*> kDeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

static const std::vector<const char*> kValidationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

//Helped to determine the device we use
//From vk tutorial we are just looking for graphics and presentation
//and also swapchain capabilities
QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    QueueFamilyIndices indices;

    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());

    int i = 0;
    for (const auto& q : families) {
        if (q.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }

        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
        if (presentSupport) {
            indices.presentFamily = i;
        }

        if (indices.isComplete()) break;
         ++i;
    }

    return indices;
}

SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    SwapChainSupportDetails details{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    uint32_t count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &count, nullptr);
    if (count) 
  {
        details.formats.resize(count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &count, details.formats.data());
    }

    count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &count, nullptr);
    if (count) 
    {
        details.presentModes.resize(count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &count, details.presentModes.data());
    }
    return details;
}

static bool checkDeviceExtensionSupport(VkPhysicalDevice device)
{
    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> available(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, available.data());

    std::set<std::string> required(kDeviceExtensions.begin(), kDeviceExtensions.end());
    for (const auto& e : available) required.erase(e.extensionName);
    return required.empty();
}

static bool isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    QueueFamilyIndices indices = findQueueFamilies(device, surface);

    bool extensionsSupported = checkDeviceExtensionSupport(device);

    bool swapchainValid = false;
    if (extensionsSupported) 
    {
        auto swapchainSupport = querySwapChainSupport(device, surface);
        swapchainValid = !swapchainSupport.formats.empty() && !swapchainSupport.presentModes.empty();
    }

    return indices.isComplete() && extensionsSupported && swapchainValid;
}

void VulkanDevice::create(VulkanInstance& instance, VkSurfaceKHR surface)
{
  VkInstance vkinst = instance.getInstance();
  //Pick physical device 
  uint32_t deviceCount = 0;
  vkEnumeratePhysicalDevices(vkinst, &deviceCount, nullptr);
  if (deviceCount == 0)
  {
    throw std::runtime_error("Failed to find GPU's with Vulkan Support");
  }

  std::vector<VkPhysicalDevice> devices(deviceCount);
  vkEnumeratePhysicalDevices(vkinst, &deviceCount, devices.data());

  for (auto device : devices) 
  {
    if (isDeviceSuitable(device, surface)) 
    {
         physicalDevice = device;
         break;
    }
  }

  if (physicalDevice == VK_NULL_HANDLE) 
  {
    throw std::runtime_error("failed to find a suitable GPU!");
  }

  VkPhysicalDeviceProperties properties{};
  vkGetPhysicalDeviceProperties(physicalDevice, &properties);
  std::cout << "Selected Device: " << properties.deviceName << " "
            << VK_VERSION_MAJOR(properties.apiVersion) << "."
            << VK_VERSION_MINOR(properties.apiVersion) << "."
            << VK_VERSION_PATCH(properties.apiVersion)
            << std::endl;


  //Create a logical device once we have picked our device
  QueueFamilyIndices indices = findQueueFamilies(physicalDevice, surface);
  //store queue family indices to be used in swap chain 
  queueFI = indices;
  std::set<uint32_t> uniqueQueueFamilies = {
        indices.graphicsFamily.value(),
        indices.presentFamily.value()
    };

  std::vector<VkDeviceQueueCreateInfo> queueInfos;
  queueInfos.reserve(uniqueQueueFamilies.size());

  float queuePriority = 1.0f;
  for (uint32_t family : uniqueQueueFamilies) 
  {
      VkDeviceQueueCreateInfo queueCreateInfo{};
      queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
      queueCreateInfo.queueFamilyIndex = family;
      queueCreateInfo.queueCount = 1;
      queueCreateInfo.pQueuePriorities = &queuePriority;
      queueInfos.push_back(queueCreateInfo);
  }
  
  VkPhysicalDeviceFeatures features{};

  VkDeviceCreateInfo deviceCreateInfo{};
  deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  deviceCreateInfo.queueCreateInfoCount= static_cast<uint32_t>(queueInfos.size());
  deviceCreateInfo.pQueueCreateInfos = queueInfos.data();
  deviceCreateInfo.pEnabledFeatures = &features;
  deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(kDeviceExtensions.size());
  deviceCreateInfo.ppEnabledExtensionNames = kDeviceExtensions.data();

  if (instance.validationEnabled()) 
  {
    deviceCreateInfo.enabledLayerCount   = static_cast<uint32_t>(kValidationLayers.size());
    deviceCreateInfo.ppEnabledLayerNames = kValidationLayers.data();
  } 
  else 
  {
    deviceCreateInfo.enabledLayerCount = 0;
  }

  if (vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device) != VK_SUCCESS) 
  {
    throw std::runtime_error("failed to create logical device!");
  }
  
  vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
  vkGetDeviceQueue(device, indices.presentFamily.value(),  0, &presentQueue);

  std::cout << "Logical Device Created\n";
}

void VulkanDevice::destroy()
{
  if (device) 
  {
    vkDestroyDevice(device, nullptr);
    device = VK_NULL_HANDLE;
  }
  std::cerr << "Vulkan Device Destroyed\n";
}
