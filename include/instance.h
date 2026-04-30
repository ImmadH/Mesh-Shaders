#pragma once 
#include <vulkan/vulkan_core.h>
#include <vector>
//not going to use constructors/destructors for now will just manually init & clean up :0 
//
#ifdef NDEBUG
const bool enableValidation = false;
#else
const bool enableValidation = true;
#endif

class VulkanInstance
{
public:
  void create(bool enableValidationLayers);
  void destroy();
  VkDebugUtilsMessengerEXT getDebugMessenger() const { return debugMessenger; }
  bool validationEnabled() const { return m_validationEnabled; }
  VkInstance getInstance() const {return instance;}

private:
  VkInstance instance = VK_NULL_HANDLE;
  VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
  bool m_validationEnabled;

};
