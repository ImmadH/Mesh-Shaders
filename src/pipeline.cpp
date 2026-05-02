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
                                            const VulkanRenderPass& renderPass,
                                            const char* taskSpvPath,
                                            const char* meshSpvPath,
                                            const char* fragSpvPath)
{
  auto taskCode = readFile(taskSpvPath);
  auto meshCode = readFile(meshSpvPath);
  auto fragCode = readFile(fragSpvPath);

  VkShaderModule taskModule = createShaderModule(device, taskCode);
  VkShaderModule meshModule = createShaderModule(device, meshCode);
  VkShaderModule fragModule = createShaderModule(device, fragCode);

  VkPipelineShaderStageCreateInfo taskStage{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
  taskStage.stage  = VK_SHADER_STAGE_TASK_BIT_EXT;
  taskStage.module = taskModule;
  taskStage.pName  = "main";

  VkPipelineShaderStageCreateInfo meshStage{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
  meshStage.stage  = VK_SHADER_STAGE_MESH_BIT_EXT;
  meshStage.module = meshModule;
  meshStage.pName  = "main";

  VkPipelineShaderStageCreateInfo fragStage{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
  fragStage.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
  fragStage.module = fragModule;
  fragStage.pName  = "main";

  VkPipelineShaderStageCreateInfo stages[] = { taskStage, meshStage, fragStage };

  // 6 storage buffer bindings: meshlet descs, meshlet verts, meshlet tris, vertex data, instances, lod groups
  VkDescriptorSetLayoutBinding bindings[6] = {};
  for (int i = 0; i < 6; i++) {
      bindings[i].binding         = (uint32_t)i;
      bindings[i].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
      bindings[i].descriptorCount = 1;
      bindings[i].stageFlags      = VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT;
  }

  VkDescriptorSetLayoutCreateInfo dslInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
  dslInfo.bindingCount = 6;
  dslInfo.pBindings    = bindings;
  if (vkCreateDescriptorSetLayout(device.getDevice(), &dslInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS)
    throw std::runtime_error("failed to create descriptor set layout!");

  VkPushConstantRange pcRange{};
  pcRange.stageFlags = VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT;
  pcRange.offset     = 0;
  pcRange.size       = sizeof(MeshPushConstants);

  VkPipelineLayoutCreateInfo pl{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
  pl.setLayoutCount         = 1;
  pl.pSetLayouts            = &descriptorSetLayout;
  pl.pushConstantRangeCount = 1;
  pl.pPushConstantRanges    = &pcRange;
  if (vkCreatePipelineLayout(device.getDevice(), &pl, nullptr, &pipelineLayout) != VK_SUCCESS)
    throw std::runtime_error("failed to create pipeline layout!");

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
  multiSampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  multiSampling.sampleShadingEnable  = VK_FALSE;

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

  // mesh shader pipeline: no vertex input state, no input assembly state
  VkGraphicsPipelineCreateInfo pipelineCreateInfo{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
  pipelineCreateInfo.stageCount          = 3;
  pipelineCreateInfo.pStages             = stages;
  pipelineCreateInfo.pVertexInputState   = nullptr;
  pipelineCreateInfo.pInputAssemblyState = nullptr;
  pipelineCreateInfo.pViewportState      = &vpState;
  pipelineCreateInfo.pRasterizationState = &rasterizer;
  pipelineCreateInfo.pMultisampleState   = &multiSampling;
  pipelineCreateInfo.pColorBlendState    = &cblend;
  pipelineCreateInfo.pDynamicState       = &dyn;
  pipelineCreateInfo.pDepthStencilState  = &depthStencil;
  pipelineCreateInfo.layout              = pipelineLayout;
  pipelineCreateInfo.renderPass          = renderPass.getRenderPass();
  pipelineCreateInfo.subpass             = 0;

  if (vkCreateGraphicsPipelines(device.getDevice(), VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &graphicsPipeline) != VK_SUCCESS)
    throw std::runtime_error("failed to create graphics pipeline!");

  vkDestroyShaderModule(device.getDevice(), fragModule, nullptr);
  vkDestroyShaderModule(device.getDevice(), meshModule, nullptr);
  vkDestroyShaderModule(device.getDevice(), taskModule, nullptr);

  std::cout << "Created Mesh Shader Pipeline\n";
}

void VulkanPipeline::destroy(const VulkanDevice& device)
{
  if (graphicsPipeline) {
    vkDestroyPipeline(device.getDevice(), graphicsPipeline, nullptr);
    graphicsPipeline = VK_NULL_HANDLE;
  }
  if (pipelineLayout) {
    vkDestroyPipelineLayout(device.getDevice(), pipelineLayout, nullptr);
    pipelineLayout = VK_NULL_HANDLE;
  }
  if (descriptorSetLayout) {
    vkDestroyDescriptorSetLayout(device.getDevice(), descriptorSetLayout, nullptr);
    descriptorSetLayout = VK_NULL_HANDLE;
  }
  std::cerr << "Graphics Pipeline Destroyed\n";
}

