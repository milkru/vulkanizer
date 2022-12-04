#pragma once

struct Shader
{
	VkShaderModule resource = VK_NULL_HANDLE;
	VkShaderStageFlags stage = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
	const char* pEntry = "main";
	std::vector<VkDescriptorSetLayoutBinding> layoutBindings{};
	VkPushConstantRange pushConstants{};
};

struct ShaderDesc
{
	const char* pPath = "";		  // Relative shader file path.
	const char* pEntry = "main";  // Shader entry point.
};

typedef std::initializer_list<Shader> Shaders;

Shader createShader(
	Device& _rDevice,
	ShaderDesc _desc);

void destroyShader(
	Device& _rDevice,
	Shader& _rShader);
