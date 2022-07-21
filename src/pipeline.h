#pragma once

struct Shader
{
	VkShaderModule module = VK_NULL_HANDLE;
	VkShaderStageFlagBits stage = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
	std::string entry = "main";
	std::vector<VkDescriptorSetLayoutBinding> bindings;
};

Shader createShader(
	Device _device,
	const char* _pFilePath);

void destroyShader(
	Device _device,
	Shader& _rShader);

VkPipelineLayout createPipelineLayout(
	VkDevice _device,
	std::vector<VkDescriptorSetLayout> _descriptorSetLayouts,
	std::vector<VkPushConstantRange> _pushConstantRanges);
