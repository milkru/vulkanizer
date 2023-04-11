#include "core/device.h"
#include "core/buffer.h"

#include "draw.h"

DrawBuffers createDrawBuffers(
	Device& _rDevice,
	u32 _meshCount,
	u32 _maxDrawCount,
	u32 _spawnCubeSize)
{
	EASY_BLOCK("InitializeDraws");
	
	std::vector<PerDrawData> perDrawDataVector(_maxDrawCount);
	for (u32 drawIndex = 0; drawIndex < _maxDrawCount; ++drawIndex)
	{
		PerDrawData perDrawData = { .meshIndex = drawIndex % _meshCount };

		auto randomFloat = []()
		{
			return f32(rand()) / RAND_MAX;
		};

		// TODO-MILKRU: Multiply meshlet/mesh bounding spheres by scale.
		perDrawData.model = glm::scale(m4(1.0f), v3(1.0f));

		perDrawData.model = glm::rotate(perDrawData.model,
			glm::radians(360.0f * randomFloat()), v3(0.0, 1.0, 0.0));

		perDrawData.model = glm::translate(perDrawData.model, {
			_spawnCubeSize * (randomFloat() - 0.5f),
			_spawnCubeSize * (randomFloat() - 0.5f),
			_spawnCubeSize * (randomFloat() - 0.5f) });

		perDrawDataVector[drawIndex] = perDrawData;
	}
	
	DrawBuffers drawBuffers = {
		.drawsBuffer = createBuffer(_rDevice, {
			.byteSize = sizeof(PerDrawData) * perDrawDataVector.size(),
			.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			.pContents = perDrawDataVector.data() }),

		.drawCommandsBuffer = createBuffer(_rDevice, {
			.byteSize = sizeof(DrawCommand) * _maxDrawCount,
			.usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT }),

		.drawCountBuffer = createBuffer(_rDevice, {
			.byteSize = sizeof(u32),
			.usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT }),

		.visibilityBuffer = createBuffer(_rDevice, {
			.byteSize = sizeof(i32) * perDrawDataVector.size(),
			.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT }) };

	immediateSubmit(_rDevice, [&](VkCommandBuffer _commandBuffer)
		{
			fillBuffer(_commandBuffer, _rDevice, drawBuffers.drawCountBuffer, 0u,
				VK_ACCESS_NONE, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);

			fillBuffer(_commandBuffer, _rDevice, drawBuffers.visibilityBuffer, 0u,
				VK_ACCESS_NONE, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
		});

	return drawBuffers;
}
