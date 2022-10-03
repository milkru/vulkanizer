#include "common.h"
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

static VkShaderStageFlags getShaderStage(SpvReflectShaderStageFlagBits _reflectShaderStage)
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

Shader createShader(
	Device _device,
	ShaderDesc _desc)
{
	std::vector<uint8_t> shaderCode = readFile(_desc.pPath);

	SpvReflectShaderModule spvModule;
	SPV_REFLECT_CALL(spvReflectCreateShaderModule(uint32_t(shaderCode.size()), (uint32_t*)shaderCode.data(), &spvModule));
	
	assert(spvModule.entry_point_count == 1);
	assert(spvModule.descriptor_set_count <= 1);
	assert(spvModule.push_constant_block_count <= 1);

	Shader shader = {
		.stage = getShaderStage(spvModule.shader_stage),
		.pEntry = _desc.pEntry };

	VkShaderModuleCreateInfo shaderModuleCreateInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	shaderModuleCreateInfo.codeSize = uint32_t(shaderCode.size());
	shaderModuleCreateInfo.pCode = (uint32_t*)shaderCode.data();

	VK_CALL(vkCreateShaderModule(_device.device, &shaderModuleCreateInfo, nullptr, &shader.resource));

	if (spvModule.push_constant_block_count > 0 && spvModule.push_constant_blocks != nullptr)
	{
		shader.pushConstants = {
			.stageFlags = shader.stage,
			.offset = spvModule.push_constant_blocks->offset,
			.size = spvModule.push_constant_blocks->size };
	}

	uint32_t spvBindingCount = 0;
	SPV_REFLECT_CALL(spvReflectEnumerateDescriptorBindings(&spvModule, &spvBindingCount, nullptr));

	std::vector<SpvReflectDescriptorBinding*> spvBindings(spvBindingCount);
	SPV_REFLECT_CALL(spvReflectEnumerateDescriptorBindings(&spvModule, &spvBindingCount, spvBindings.data()));

	shader.layoutBindings.reserve(spvBindingCount);
	for (uint32_t layoutBindingIndex = 0; layoutBindingIndex < spvBindingCount; ++layoutBindingIndex)
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
	Device _device,
	Shader& _rShader)
{
	vkDestroyShaderModule(_device.device, _rShader.resource, nullptr);
	_rShader = {};
}
