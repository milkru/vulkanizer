#include "device.h"
#include "shader.h"
#include "buffer.h"
#include "pipeline.h"
#include "texture.h"
#include "pass.h"

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

static std::vector<VkDescriptorSetLayoutBinding> mergeSetLayoutBindings(
	Shaders& _rShaders)
{
	std::vector<VkDescriptorSetLayoutBinding> mergedLayoutBindings;

	for (const Shader& rShader : _rShaders)
	{
		for (const VkDescriptorSetLayoutBinding& rLayoutBinding : rShader.layoutBindings)
		{
			if (tryUpdateBindingShaderStage(mergedLayoutBindings, rLayoutBinding))
			{
				continue;
			}

			mergedLayoutBindings.push_back(rLayoutBinding);
		}
	}

	return mergedLayoutBindings;
}

static VkPushConstantRange mergePushConstants(
	Shaders& _rShaders)
{
	VkPushConstantRange mergedPushConstants{};

	for (const Shader& rShader : _rShaders)
	{
		if (rShader.pushConstants.stageFlags != 0)
		{
			if (mergedPushConstants.stageFlags == 0)
			{
				mergedPushConstants = {
					.offset = rShader.pushConstants.offset,
					.size = rShader.pushConstants.size };
			}

			assert(mergedPushConstants.offset == rShader.pushConstants.offset);
			assert(mergedPushConstants.size == rShader.pushConstants.size);

			mergedPushConstants.stageFlags |= rShader.pushConstants.stageFlags;
		}
	}

	return mergedPushConstants;
}

static VkDescriptorSetLayout createDescriptorSetLayout(
	VkDevice _device,
	std::vector<VkDescriptorSetLayoutBinding> _bindings)
{
	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	descriptorSetLayoutCreateInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
	descriptorSetLayoutCreateInfo.bindingCount = u32(_bindings.size());
	descriptorSetLayoutCreateInfo.pBindings = _bindings.data();

	VkDescriptorSetLayout descriptorSetLayout;
	VK_CALL(vkCreateDescriptorSetLayout(_device, &descriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayout));

	return descriptorSetLayout;
}

static VkPipelineLayout createPipelineLayout(
	VkDevice _device,
	VkDescriptorSetLayout _descriptorSetLayout,
	VkPushConstantRange _pushConstants)
{
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	pipelineLayoutCreateInfo.setLayoutCount = 1;
	pipelineLayoutCreateInfo.pSetLayouts = &_descriptorSetLayout;
	pipelineLayoutCreateInfo.pushConstantRangeCount = _pushConstants.stageFlags != 0 ? 1 : 0;
	pipelineLayoutCreateInfo.pPushConstantRanges = _pushConstants.stageFlags != 0 ? &_pushConstants : nullptr;

	VkPipelineLayout pipelineLayout;
	VK_CALL(vkCreatePipelineLayout(_device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));

	return pipelineLayout;
}

static VkDescriptorUpdateTemplate createDescriptorUpdateTemplate(
	VkDevice _device,
	VkDescriptorSetLayout _descriptorSetLayout,
	VkPipelineLayout _pipelineLayout,
	VkPipelineBindPoint _pipelineBindPoint,
	std::vector<VkDescriptorSetLayoutBinding> _bindings)
{
	std::vector<VkDescriptorUpdateTemplateEntry> entries(_bindings.size());
	for (u32 descriptorIndex = 0; descriptorIndex < _bindings.size(); ++descriptorIndex)
	{
		VkDescriptorSetLayoutBinding binding = _bindings[descriptorIndex];
		VkDescriptorUpdateTemplateEntry& entry = entries[descriptorIndex];

		entry.dstBinding = binding.binding;
		entry.dstArrayElement = 0;
		entry.descriptorCount = 1;
		entry.descriptorType = binding.descriptorType;
		entry.offset = descriptorIndex * sizeof(Binding);
		entry.stride = sizeof(Binding);
	}

	VkDescriptorUpdateTemplateCreateInfo descriptorUpdateTemplateCreateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO };
	descriptorUpdateTemplateCreateInfo.descriptorUpdateEntryCount = u32(entries.size());
	descriptorUpdateTemplateCreateInfo.pDescriptorUpdateEntries = entries.data();
	descriptorUpdateTemplateCreateInfo.templateType = VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_PUSH_DESCRIPTORS_KHR;
	descriptorUpdateTemplateCreateInfo.descriptorSetLayout = _descriptorSetLayout;
	descriptorUpdateTemplateCreateInfo.pipelineBindPoint = _pipelineBindPoint;
	descriptorUpdateTemplateCreateInfo.pipelineLayout = _pipelineLayout;

	VkDescriptorUpdateTemplate descriptorUpdateTemplate;
	VK_CALL(vkCreateDescriptorUpdateTemplate(_device, &descriptorUpdateTemplateCreateInfo, nullptr, &descriptorUpdateTemplate));

	return descriptorUpdateTemplate;
}

static VkPipeline createGraphicsPipeline(
	VkDevice _device,
	VkPipelineLayout _pipelineLayout,
	GraphicsPipelineDesc _desc)
{
	std::vector<VkPipelineShaderStageCreateInfo> shaderStageCreateInfos;
	shaderStageCreateInfos.reserve(_desc.shaders.size());

	for (const Shader& rShader : _desc.shaders)
	{
		VkPipelineShaderStageCreateInfo shaderStageCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
		shaderStageCreateInfo.stage = VkShaderStageFlagBits(rShader.stage);
		shaderStageCreateInfo.module = rShader.resource;
		shaderStageCreateInfo.pName = rShader.pEntry;

		shaderStageCreateInfos.push_back(shaderStageCreateInfo);
	}

	VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
	inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

	VkPipelineViewportStateCreateInfo viewportStateCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
	viewportStateCreateInfo.viewportCount = 1;
	viewportStateCreateInfo.scissorCount = 1;

	VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
	rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
	rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
	rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizationStateCreateInfo.lineWidth = 1.0f;
	rasterizationStateCreateInfo.cullMode = _desc.rasterization.cullMode;
	rasterizationStateCreateInfo.frontFace = _desc.rasterization.frontFace;
	rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;

	VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
	multisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;
	multisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
	depthStencilStateCreateInfo.depthTestEnable = _desc.depthStencil.bDepthTestEnable;
	depthStencilStateCreateInfo.depthWriteEnable = _desc.depthStencil.bDepthWriteEnable;
	depthStencilStateCreateInfo.depthCompareOp = _desc.depthStencil.depthCompareOp;
	depthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
	depthStencilStateCreateInfo.stencilTestEnable = VK_FALSE;

	std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments;
	colorBlendAttachments.reserve(_desc.attachmentLayout.colorAttachments.size());

	for (ColorAttachmentDesc colorAttachmentState : _desc.attachmentLayout.colorAttachments)
	{
		colorBlendAttachments.push_back({
			.blendEnable = colorAttachmentState.bBlendEnable,
			.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
			.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
			.colorBlendOp = VK_BLEND_OP_ADD,
			.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
			.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
			.alphaBlendOp = VK_BLEND_OP_ADD,
			.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT });
	}

	VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
	colorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
	colorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_COPY;
	colorBlendStateCreateInfo.attachmentCount = u32(colorBlendAttachments.size());
	colorBlendStateCreateInfo.pAttachments = colorBlendAttachments.data();

	VkDynamicState dynamicStates[] =
	{
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
	dynamicStateCreateInfo.dynamicStateCount = ARRAY_SIZE(dynamicStates);
	dynamicStateCreateInfo.pDynamicStates = dynamicStates;

	std::vector<VkFormat> colorFormats;
	colorFormats.reserve(_desc.attachmentLayout.colorAttachments.size());

	for (ColorAttachmentDesc colorAttachmentState : _desc.attachmentLayout.colorAttachments)
	{
		colorFormats.push_back(colorAttachmentState.format);
	}

	VkPipelineRenderingCreateInfoKHR pipelineRenderingCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR };
	pipelineRenderingCreateInfo.colorAttachmentCount = u32(colorFormats.size());
	pipelineRenderingCreateInfo.pColorAttachmentFormats = colorFormats.data();
	pipelineRenderingCreateInfo.depthAttachmentFormat = _desc.attachmentLayout.depthStencilFormat;

	VkGraphicsPipelineCreateInfo pipelineCreateInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
	pipelineCreateInfo.stageCount = u32(shaderStageCreateInfos.size());
	pipelineCreateInfo.pStages = shaderStageCreateInfos.data();
	pipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo;
	pipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
	pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
	pipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
	pipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
	pipelineCreateInfo.pDepthStencilState = &depthStencilStateCreateInfo;
	pipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
	pipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
	pipelineCreateInfo.layout = _pipelineLayout;
	pipelineCreateInfo.renderPass = VK_NULL_HANDLE;
	pipelineCreateInfo.pNext = &pipelineRenderingCreateInfo;
	pipelineCreateInfo.subpass = 0;
	pipelineCreateInfo.basePipelineIndex = -1;
	pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;

	VkPipeline graphicsPipeline;
	VK_CALL(vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &graphicsPipeline));

	return graphicsPipeline;
}

static VkPipeline createComputePipeline(
	VkDevice _device,
	VkPipelineLayout _pipelineLayout,
	Shader& _rComputeShader)
{
	assert(_rComputeShader.stage == VK_SHADER_STAGE_COMPUTE_BIT);

	VkPipelineShaderStageCreateInfo shaderStageCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
	shaderStageCreateInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	shaderStageCreateInfo.module = _rComputeShader.resource;
	shaderStageCreateInfo.pName = _rComputeShader.pEntry;

	VkComputePipelineCreateInfo computePipelineCreateInfo = { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
	computePipelineCreateInfo.stage = shaderStageCreateInfo;
	computePipelineCreateInfo.layout = _pipelineLayout;

	VkPipeline computePipeline;
	VK_CALL(vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &computePipeline));

	return computePipeline;
}

static Pipeline createPipeline(
	Device& _rDevice,
	VkPipelineBindPoint _bindPoint,
	Shaders _shaders,
	LAMBDA(Pipeline&) _callback)
{
	std::vector<VkDescriptorSetLayoutBinding> layoutBindings = mergeSetLayoutBindings(_shaders);
	VkPushConstantRange pushConstants = mergePushConstants(_shaders);

	Pipeline pipeline = {
		.type = _bindPoint,
		.pushConstants = pushConstants };

	pipeline.setLayout = createDescriptorSetLayout(
		_rDevice.device, layoutBindings);

	pipeline.pipelineLayout = createPipelineLayout(
		_rDevice.device, pipeline.setLayout, pushConstants);

	pipeline.updateTemplate = createDescriptorUpdateTemplate(
		_rDevice.device, pipeline.setLayout, pipeline.pipelineLayout, pipeline.type, layoutBindings);

	_callback(pipeline);

	return pipeline;
}

Pipeline createGraphicsPipeline(
	Device& _rDevice,
	GraphicsPipelineDesc _desc)
{
	return createPipeline(_rDevice, VK_PIPELINE_BIND_POINT_GRAPHICS, _desc.shaders,
		[&](Pipeline& _pipeline)
		{
			_pipeline.pipeline = createGraphicsPipeline(_rDevice.device, _pipeline.pipelineLayout, _desc);
		});
}

Pipeline createComputePipeline(
	Device& _rDevice,
	Shader& _rShader)
{
	return createPipeline(_rDevice, VK_PIPELINE_BIND_POINT_COMPUTE, { _rShader },
		[&](Pipeline& _pipeline)
		{
			_pipeline.pipeline = createComputePipeline(_rDevice.device, _pipeline.pipelineLayout, { _rShader });
		});
}

void destroyPipeline(
	Device& _rDevice,
	Pipeline& _rPipeline)
{
	vkDestroyPipeline(_rDevice.device, _rPipeline.pipeline, nullptr);
	vkDestroyDescriptorUpdateTemplate(_rDevice.device, _rPipeline.updateTemplate, nullptr);
	vkDestroyPipelineLayout(_rDevice.device, _rPipeline.pipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(_rDevice.device, _rPipeline.setLayout, nullptr);
}
