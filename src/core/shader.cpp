#include "device.h"
#include "shader.h"
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

static VkShaderStageFlags getShaderStage(
	SpvReflectShaderStageFlagBits _reflectShaderStage)
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
		assert(!"Unsupported SpvReflectShaderStageFlagBits!");
		return {};
	}
}

static VkDescriptorType getDescriptorType(
	SpvReflectDescriptorType _reflectDescriptorType)
{
	switch (_reflectDescriptorType)
	{
	case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:
		return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

	case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
		return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

	case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE:
		return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

	default:
		assert(!"Unsupported SpvReflectDescriptorType!");
		return {};
	}
}

Shader createShader(
	Device& _rDevice,
	ShaderDesc _desc)
{
	std::vector<char> spvSource = readFile(_desc.pPath);

	SpvReflectShaderModule spvModule;
	SPV_REFLECT_CALL(spvReflectCreateShaderModule(u32(spvSource.size()), spvSource.data(), &spvModule));

	assert(spvModule.entry_point_count == 1);
	assert(spvModule.descriptor_set_count <= 1);
	assert(spvModule.push_constant_block_count <= 1);

	Shader shader = {
		.stage = getShaderStage(spvModule.shader_stage),
		.pEntry = _desc.pEntry };

	VkShaderModuleCreateInfo shaderModuleCreateInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	shaderModuleCreateInfo.codeSize = u32(spvSource.size());
	shaderModuleCreateInfo.pCode = (u32*)spvSource.data();

	VK_CALL(vkCreateShaderModule(_rDevice.device, &shaderModuleCreateInfo, nullptr, &shader.resource));

	if (spvModule.push_constant_block_count > 0 && spvModule.push_constant_blocks != nullptr)
	{
		shader.pushConstants = {
			.stageFlags = shader.stage,
			.offset = spvModule.push_constant_blocks->offset,
			.size = spvModule.push_constant_blocks->size };
	}

	u32 spvBindingCount = 0;
	SPV_REFLECT_CALL(spvReflectEnumerateDescriptorBindings(&spvModule, &spvBindingCount, nullptr));

	std::vector<SpvReflectDescriptorBinding*> spvBindings(spvBindingCount);
	SPV_REFLECT_CALL(spvReflectEnumerateDescriptorBindings(&spvModule, &spvBindingCount, spvBindings.data()));

	shader.layoutBindings.reserve(spvBindingCount);
	for (u32 layoutBindingIndex = 0; layoutBindingIndex < spvBindingCount; ++layoutBindingIndex)
	{
		VkDescriptorSetLayoutBinding descriptorSetLayoutBinding{};
		descriptorSetLayoutBinding.binding = spvBindings[layoutBindingIndex]->binding;
		descriptorSetLayoutBinding.descriptorCount = 1;
		descriptorSetLayoutBinding.descriptorType = getDescriptorType(spvBindings[layoutBindingIndex]->descriptor_type);
		descriptorSetLayoutBinding.stageFlags = shader.stage;
		descriptorSetLayoutBinding.pImmutableSamplers = nullptr;

		shader.layoutBindings.push_back(descriptorSetLayoutBinding);
	}

	spvReflectDestroyShaderModule(&spvModule);

	return shader;
}

void destroyShader(
	Device&_rDevice,
	Shader& _rShader)
{
	vkDestroyShaderModule(_rDevice.device, _rShader.resource, nullptr);
}
