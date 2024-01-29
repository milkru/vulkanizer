#version 460

#extension GL_EXT_shader_8bit_storage: require
#extension GL_EXT_shader_16bit_storage: require

#include "shader_common.h"

layout(binding = 0) readonly buffer Vertices { Vertex vertices[]; };
layout(binding = 1) readonly buffer PerDrawDataVector { PerDrawData perDrawDataVector[]; };
layout(binding = 2) readonly buffer DrawCommands { DrawCommand drawCommands[]; };

layout(location = 0) out vec3 outColor;

layout (push_constant) uniform block
{
    PerFrameData perFrameData;
};

void main()
{
	uint drawIndex = drawCommands[gl_DrawID].drawIndex;
	PerDrawData perDrawData = perDrawDataVector[drawIndex];

	vec3 position = vec3(
		vertices[gl_VertexIndex].position[0],
		vertices[gl_VertexIndex].position[1],
		vertices[gl_VertexIndex].position[2]);
		
	vec4 worldPosition = perDrawData.model * vec4(position, 1.0);

	vec3 normal = vec3(
		int(vertices[gl_VertexIndex].normal[0]),
		int(vertices[gl_VertexIndex].normal[1]),
		int(vertices[gl_VertexIndex].normal[2])) / 127.0 - 1.0;
		
	normal = mat3(perDrawData.model) * normalize(normal);

	vec2 texCoord = vec2(
		vertices[gl_VertexIndex].texCoord[0],
		vertices[gl_VertexIndex].texCoord[1]);
		
    gl_Position = perFrameData.projection * perFrameData.view * worldPosition;
	
	float shade = dot(normal, normalize(perFrameData.cameraPosition - worldPosition.xyz));
    outColor = shade * (0.5 + 0.5 * normal);

	outColor = shade * vec3(
		vertices[gl_VertexIndex].color[0],
		vertices[gl_VertexIndex].color[1],
		vertices[gl_VertexIndex].color[2]);
}
