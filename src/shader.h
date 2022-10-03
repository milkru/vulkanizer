#pragma once

struct ShaderDesc
{
	const char* pPath = "";
	const char* pEntry = "main";
};

struct Shader
{
	VkShaderModule resource = VK_NULL_HANDLE;
	VkShaderStageFlags stage = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
	const char* pEntry = "main";
	std::vector<VkDescriptorSetLayoutBinding> layoutBindings{};
	VkPushConstantRange pushConstants{};
};

typedef std::initializer_list<Shader> Shaders;

Shader createShader(
	Device _device,
	ShaderDesc _desc);

void destroyShader(
	Device _device,
	Shader& _rShader);
