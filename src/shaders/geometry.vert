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

	vec2 normalXY = vec2(
		int(vertices[gl_VertexIndex].normal[0]),
		int(vertices[gl_VertexIndex].normal[1])) / 127.0 - 1.0;

	// Reconstruct normal vector Z component.
	// https://aras-p.info/texts/CompactNormalStorage.html
	float normalZ = max(/*epsilon*/ 1e-06, sqrt(1.0 - dot(normalXY, normalXY)));
	vec3 normal = mat3(perDrawData.model) * normalize(vec3(normalXY, normalZ));

	vec2 texCoord = vec2(
		vertices[gl_VertexIndex].texCoord[0],
		vertices[gl_VertexIndex].texCoord[1]);
		
    gl_Position = perFrameData.viewProjection * perDrawData.model * vec4(position, 1.0);
	
    outColor = 0.5 + 0.5 * normal;
}
