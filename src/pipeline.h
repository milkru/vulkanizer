#pragma once

VkShaderModule createShaderModule(
	VkDevice _device,
	size_t _spvCodeSize,
	uint32_t* _pSpvCode);

VkPipelineLayout createPipelineLayout(
	VkDevice _device,
	std::vector<VkDescriptorSetLayout> _descriptorSetLayouts,
	std::vector<VkPushConstantRange> _pushConstantRanges);
