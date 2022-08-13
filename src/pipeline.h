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

union DescriptorInfo
{
	DescriptorInfo(
		VkBuffer _buffer,
		VkDeviceSize _offset = 0,
		VkDeviceSize _range = VK_WHOLE_SIZE)
	{
		bufferInfo.buffer = _buffer;
		bufferInfo.offset = _offset;
		bufferInfo.range = _range;
	}

	VkDescriptorImageInfo imageInfo;
	VkDescriptorBufferInfo bufferInfo;
};

VkDescriptorSetLayout createDescriptorSetLayout(
	VkDevice _device,
	std::initializer_list<Shader> _shaders);

VkPipelineLayout createPipelineLayout(
	VkDevice _device,
	std::vector<VkDescriptorSetLayout> _descriptorSetLayouts,
	std::vector<VkPushConstantRange> _pushConstantRanges);

VkDescriptorUpdateTemplate createDescriptorUpdateTemplate(
	VkDevice _device,
	VkDescriptorSetLayout _descriptorSetLayout,
	VkPipelineLayout _pipelineLayout,
	VkPipelineBindPoint _pipelineBindPoint,
	std::initializer_list<Shader> _shaders);
