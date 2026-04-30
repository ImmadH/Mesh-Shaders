#include "pipeline.h"
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <array>
#include <iostream>

static std::vector<char> readFile(const std::string& filename)
{
  std::ifstream file(filename, std::ios::ate | std::ios::binary);
  if (!file.is_open())
    throw std::runtime_error("failed to open file: " + filename);

  size_t size = (size_t)file.tellg();
  std::vector<char> buffer(size);
  file.seekg(0);
  file.read(buffer.data(), size);
  return buffer;
}

VkShaderModule VulkanPipeline::createShaderModule(const VulkanDevice& device, const std::vector<char>& code) const
{
  VkShaderModuleCreateInfo info{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
  info.codeSize = code.size();
  info.pCode    = reinterpret_cast<const uint32_t*>(code.data());

  VkShaderModule module;
  if (vkCreateShaderModule(device.getDevice(), &info, nullptr, &module) != VK_SUCCESS)
    throw std::runtime_error("failed to create shader module!");
  return module;
}


void VulkanPipeline::createGraphicsPipeline(const VulkanDevice& device,
                                            const VulkanSwapchain& swapchain,
                                            const VulkanRenderPass& renderPass,
                                            const char* vertSpvPath,
                                            const char* fragSpvPath)
{
  // shader modules
  auto vertCode = readFile(vertSpvPath);
  auto fragCode = readFile(fragSpvPath);

  VkShaderModule vertModule = createShaderModule(device, vertCode);
  VkShaderModule fragModule = createShaderModule(device, fragCode);

  VkPipelineShaderStageCreateInfo vertStage{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
  vertStage.stage  = VK_SHADER_STAGE_VERTEX_BIT;
  vertStage.module = vertModule;
  vertStage.pName  = "main";

  VkPipelineShaderStageCreateInfo fragStage{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
  fragStage.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
  fragStage.module = fragModule;
  fragStage.pName  = "main";

  VkPipelineShaderStageCreateInfo stages[] = { vertStage, fragStage };

  // fixed functions 
  VkVertexInputBindingDescription bindingDesc{};
  bindingDesc.binding   = 0;
  bindingDesc.stride    = sizeof(Vertex);
  bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  std::array<VkVertexInputAttributeDescription, 2> attrDescs{};
  attrDescs[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos)};
  attrDescs[1] = {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)};

  VkPipelineVertexInputStateCreateInfo vertexInput{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
  vertexInput.vertexBindingDescriptionCount   = 1;
  vertexInput.pVertexBindingDescriptions      = &bindingDesc;
  vertexInput.vertexAttributeDescriptionCount = (uint32_t)attrDescs.size();
  vertexInput.pVertexAttributeDescriptions    = attrDescs.data();

  VkPipelineInputAssemblyStateCreateInfo inputAssemblerInfo{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
  inputAssemblerInfo.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  inputAssemblerInfo.primitiveRestartEnable = VK_FALSE;

  VkPipelineViewportStateCreateInfo vpState{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
  vpState.viewportCount = 1;
  vpState.scissorCount  = 1;

  VkPipelineRasterizationStateCreateInfo rasterizer{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
  rasterizer.depthClampEnable        = VK_FALSE;
  rasterizer.rasterizerDiscardEnable = VK_FALSE;
  rasterizer.polygonMode             = VK_POLYGON_MODE_FILL;
  rasterizer.lineWidth               = 1.0f;
  rasterizer.cullMode                = VK_CULL_MODE_NONE;
  rasterizer.frontFace               = VK_FRONT_FACE_CLOCKWISE; 
  rasterizer.depthBiasEnable         = VK_FALSE;

  VkPipelineMultisampleStateCreateInfo multiSampling{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
  multiSampling.rasterizationSamples   = VK_SAMPLE_COUNT_1_BIT;
  multiSampling.sampleShadingEnable    = VK_FALSE;

  VkPipelineColorBlendAttachmentState cbAtt{};
  cbAtt.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                         VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  cbAtt.blendEnable    = VK_FALSE;

  VkPipelineColorBlendStateCreateInfo cblend{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
  cblend.logicOpEnable   = VK_FALSE;
  cblend.attachmentCount = 1;
  cblend.pAttachments    = &cbAtt;

  VkDynamicState dynamics[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
  VkPipelineDynamicStateCreateInfo dyn{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
  dyn.dynamicStateCount = 2;
  dyn.pDynamicStates    = dynamics;

  VkPipelineDepthStencilStateCreateInfo depthStencil{ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
  depthStencil.depthTestEnable       = VK_TRUE;
  depthStencil.depthWriteEnable      = VK_TRUE;
  depthStencil.depthCompareOp        = VK_COMPARE_OP_LESS;
  depthStencil.depthBoundsTestEnable = VK_FALSE;
  depthStencil.stencilTestEnable     = VK_FALSE;

  VkPushConstantRange pcRange{};
  pcRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  pcRange.offset     = 0;
  pcRange.size       = sizeof(glm::mat4);

  VkPipelineLayoutCreateInfo pl{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
  pl.pushConstantRangeCount = 1;
  pl.pPushConstantRanges    = &pcRange;
  if (vkCreatePipelineLayout(device.getDevice(), &pl, nullptr, &pipelineLayout) != VK_SUCCESS)
    throw std::runtime_error("failed to create pipeline layout!");

  //  graphics pipeline
  VkGraphicsPipelineCreateInfo pipelineCreateInfo{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
  pipelineCreateInfo.stageCount = 2;
  pipelineCreateInfo.pStages = stages;
  pipelineCreateInfo.pVertexInputState  = &vertexInput;
  pipelineCreateInfo.pInputAssemblyState = &inputAssemblerInfo;
  pipelineCreateInfo.pViewportState = &vpState;
  pipelineCreateInfo.pRasterizationState = &rasterizer;
  pipelineCreateInfo.pMultisampleState = &multiSampling;
  pipelineCreateInfo.pColorBlendState = &cblend;
  pipelineCreateInfo.pDynamicState      = &dyn;
  pipelineCreateInfo.pDepthStencilState = &depthStencil;
  pipelineCreateInfo.layout             = pipelineLayout;
  pipelineCreateInfo.renderPass         = renderPass.getRenderPass();
  pipelineCreateInfo.subpass            = 0;

  if (vkCreateGraphicsPipelines(device.getDevice(), VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &graphicsPipeline) != VK_SUCCESS)
    throw std::runtime_error("failed to create graphics pipeline!");

  // shader modules go after pipeline creation
  vkDestroyShaderModule(device.getDevice(), fragModule, nullptr);
  vkDestroyShaderModule(device.getDevice(), vertModule, nullptr);

  std::cout << "Created Graphics Pipeline\n";
}

void VulkanPipeline::destroy(const VulkanDevice& device)
{
  if (graphicsPipeline) 
  {
    vkDestroyPipeline(device.getDevice(), graphicsPipeline, nullptr);
    graphicsPipeline = VK_NULL_HANDLE;
  }
  if (pipelineLayout) 
  {
    vkDestroyPipelineLayout(device.getDevice(), pipelineLayout, nullptr);
    pipelineLayout = VK_NULL_HANDLE;
  }
  std::cerr << "Graphics Pipeline Destroyed\n";
}

