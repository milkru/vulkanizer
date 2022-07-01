#include "common.h"
#include "pipeline.h"

VkShaderModule createShaderModule(
	VkDevice _device,
	size_t _spvCodeSize,
	uint32_t* _pSpvCode)
{
	VkShaderModuleCreateInfo shaderModuleCreateInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	shaderModuleCreateInfo.codeSize = _spvCodeSize;
	shaderModuleCreateInfo.pCode = _pSpvCode;

	VkShaderModule shaderModule;
	VK_CALL(vkCreateShaderModule(_device, &shaderModuleCreateInfo, nullptr, &shaderModule));

	return shaderModule;
}

VkPipelineLayout createPipelineLayout(
	VkDevice _device,
	std::vector<VkDescriptorSetLayout> _descriptorSetLayouts,
	std::vector<VkPushConstantRange> _pushConstantRanges)
{
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	pipelineLayoutCreateInfo.setLayoutCount = _descriptorSetLayouts.size();
	pipelineLayoutCreateInfo.pSetLayouts = _descriptorSetLayouts.data();
	pipelineLayoutCreateInfo.pushConstantRangeCount = _pushConstantRanges.size();
	pipelineLayoutCreateInfo.pPushConstantRanges = _pushConstantRanges.data();

	VkPipelineLayout pipelineLayout;
	VK_CALL(vkCreatePipelineLayout(_device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));

	return pipelineLayout;
}
