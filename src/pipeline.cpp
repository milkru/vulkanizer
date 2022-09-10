#include "common.h"
#include "device.h"
#include "pipeline.h"
#include "utils.h"

#include <spirv_reflect.h>

#ifndef SPV_REFLECT_CALL
#define SPV_REFLECT_CALL(_call) \
	do { \
		SpvReflectResult result_ = _call; \
		assert(result_ == SPV_REFLECT_RESULT_SUCCESS); \
	} \
	while (0)
#endif // SPV_REFLECT_CALL

static VkShaderStageFlagBits getShaderStage(SpvReflectShaderStageFlagBits _reflectShaderStage)
{
	switch (_reflectShaderStage)
	{
	case SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT:
		return VK_SHADER_STAGE_COMPUTE_BIT;

	case SPV_REFLECT_SHADER_STAGE_TASK_BIT_NV:
		return VK_SHADER_STAGE_TASK_BIT_NV;

	case SPV_REFLECT_SHADER_STAGE_MESH_BIT_NV:
		return VK_SHADER_STAGE_MESH_BIT_NV;

	case SPV_REFLECT_SHADER_STAGE_VERTEX_BIT:
		return VK_SHADER_STAGE_VERTEX_BIT;

	case SPV_REFLECT_SHADER_STAGE_FRAGMENT_BIT:
		return VK_SHADER_STAGE_FRAGMENT_BIT;

	default:
		assert(!"Unsupported SpvReflectShaderStageFlagBits.");
		return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
	}
}

static VkDescriptorType getDescriptorType(const SpvReflectDescriptorType _reflectDescriptorType)
{
	switch (_reflectDescriptorType)
	{
	case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:
		return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

	case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
		return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

	default:
		assert(!"Unsupported SpvReflectDescriptorType.");
		return VK_DESCRIPTOR_TYPE_MAX_ENUM;
	}
}

static bool tryUpdateBindingShaderStage(
	std::vector<VkDescriptorSetLayoutBinding>& _bindings,
	VkDescriptorSetLayoutBinding _binding)
{
	for (VkDescriptorSetLayoutBinding& shaderBinding : _bindings)
	{
		if (shaderBinding.binding == _binding.binding)
		{
			shaderBinding.stageFlags |= _binding.stageFlags;
			return true;
		}
	}

	return false;
}

static std::vector<VkDescriptorSetLayoutBinding> mergeDescriptorSetLayoutBindings(
	std::initializer_list<Shader> _shaders)
{
	std::vector<VkDescriptorSetLayoutBinding> bindings;
	for (const Shader& shader : _shaders)
	{
		for (const VkDescriptorSetLayoutBinding& shaderBinding : shader.bindings)
		{
			if (tryUpdateBindingShaderStage(bindings, shaderBinding))
			{
				continue;
			}

			bindings.push_back(shaderBinding);
		}
	}

	return bindings;
}

Shader createShader(
	Device _device,
	const char* _pFilePath)
{
	Shader shader{};

	std::vector<uint8_t> shaderCode = readFile(_pFilePath);

	VkShaderModuleCreateInfo shaderModuleCreateInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	shaderModuleCreateInfo.codeSize = uint32_t(shaderCode.size());
	shaderModuleCreateInfo.pCode = (uint32_t*)shaderCode.data();

	VK_CALL(vkCreateShaderModule(_device.deviceVk, &shaderModuleCreateInfo, nullptr, &shader.module));

	SpvReflectShaderModule spvModule;
	SPV_REFLECT_CALL(spvReflectCreateShaderModule(uint32_t(shaderCode.size()), (uint32_t*)shaderCode.data(), &spvModule));
	assert(spvModule.entry_point_count == 1);

	shader.entry = spvModule.entry_point_name;
	shader.stage = getShaderStage(spvModule.shader_stage);

	uint32_t bindingCount = 0;
	SPV_REFLECT_CALL(spvReflectEnumerateDescriptorBindings(&spvModule, &bindingCount, nullptr));

	std::vector<SpvReflectDescriptorBinding*> spvBindings(bindingCount);
	SPV_REFLECT_CALL(spvReflectEnumerateDescriptorBindings(&spvModule, &bindingCount, spvBindings.data()));

	shader.bindings.reserve(bindingCount);
	for (uint32_t bindingIndex = 0; bindingIndex < bindingCount; ++bindingIndex)
	{
		VkDescriptorSetLayoutBinding descriptorSetLayoutBinding{};
		descriptorSetLayoutBinding.binding = spvBindings[bindingIndex]->binding;
		descriptorSetLayoutBinding.descriptorCount = 1;
		descriptorSetLayoutBinding.descriptorType = getDescriptorType(spvBindings[bindingIndex]->descriptor_type);
		descriptorSetLayoutBinding.stageFlags = shader.stage;
		descriptorSetLayoutBinding.pImmutableSamplers = nullptr;

		shader.bindings.push_back(descriptorSetLayoutBinding);
	}

	spvReflectDestroyShaderModule(&spvModule);

	return shader;
}

void destroyShader(
	Device _device,
	Shader& _rShader)
{
	vkDestroyShaderModule(_device.deviceVk, _rShader.module, nullptr);
	_rShader = {};
}

VkDescriptorSetLayout createDescriptorSetLayout(
	VkDevice _device,
	std::initializer_list<Shader> _shaders)
{
	std::vector<VkDescriptorSetLayoutBinding> bindings = mergeDescriptorSetLayoutBindings(_shaders);

	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	descriptorSetLayoutCreateInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
	descriptorSetLayoutCreateInfo.bindingCount = uint32_t(bindings.size());
	descriptorSetLayoutCreateInfo.pBindings = bindings.data();

	VkDescriptorSetLayout descriptorSetLayout;
	VK_CALL(vkCreateDescriptorSetLayout(_device, &descriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayout));

	return descriptorSetLayout;
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

VkDescriptorUpdateTemplate createDescriptorUpdateTemplate(
	VkDevice _device,
	VkDescriptorSetLayout _descriptorSetLayout,
	VkPipelineLayout _pipelineLayout,
	VkPipelineBindPoint _pipelineBindPoint,
	std::initializer_list<Shader> _shaders)
{
	std::vector<VkDescriptorSetLayoutBinding> bindings = mergeDescriptorSetLayoutBindings(_shaders);

	std::vector<VkDescriptorUpdateTemplateEntry> entries(bindings.size());
	for (uint32_t descriptorIndex = 0; descriptorIndex < bindings.size(); ++descriptorIndex)
	{
		VkDescriptorSetLayoutBinding binding = bindings[descriptorIndex];
		VkDescriptorUpdateTemplateEntry& entry = entries[descriptorIndex];

		entry.dstBinding = binding.binding;
		entry.dstArrayElement = 0;
		entry.descriptorCount = 1;
		entry.descriptorType = binding.descriptorType;
		entry.offset = descriptorIndex * sizeof(DescriptorInfo);
		entry.stride = sizeof(DescriptorInfo);
	}

	VkDescriptorUpdateTemplateCreateInfo descriptorUpdateTemplateCreateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO };
	descriptorUpdateTemplateCreateInfo.descriptorUpdateEntryCount = uint32_t(entries.size());
	descriptorUpdateTemplateCreateInfo.pDescriptorUpdateEntries = entries.data();
	descriptorUpdateTemplateCreateInfo.templateType = VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_PUSH_DESCRIPTORS_KHR;
	descriptorUpdateTemplateCreateInfo.descriptorSetLayout = _descriptorSetLayout;
	descriptorUpdateTemplateCreateInfo.pipelineBindPoint = _pipelineBindPoint;
	descriptorUpdateTemplateCreateInfo.pipelineLayout = _pipelineLayout;

	VkDescriptorUpdateTemplate descriptorUpdateTemplate;
	VK_CALL(vkCreateDescriptorUpdateTemplate(_device, &descriptorUpdateTemplateCreateInfo, nullptr, &descriptorUpdateTemplate));

	return descriptorUpdateTemplate;
}
