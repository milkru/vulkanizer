#version 450

#extension GL_EXT_shader_8bit_storage: require
#extension GL_EXT_shader_16bit_storage: require

#include "shader_common.h"

layout(binding = 0) readonly buffer Vertices { Vertex vertices[]; };

layout(location = 0) out vec3 outColor;

layout (push_constant) uniform block
{
    PerFrameData perFrameData;
};

void main()
{
	vec3 position = vec3(
		vertices[gl_VertexIndex].position[0],
		vertices[gl_VertexIndex].position[1],
		vertices[gl_VertexIndex].position[2]);

	vec2 normalXY = vec2(
		int(vertices[gl_VertexIndex].normal[0]),
		int(vertices[gl_VertexIndex].normal[1])) / 127.0 - 1.0;

	// Reconstruct normal vector Z component.
	// https://aras-p.info/texts/CompactNormalStorage.html
	float normalX = sqrt(1.0 - dot(normalXY, normalXY));
	
	vec3 normal = vec3(normalXY, normalX);

	vec2 texCoord = vec2(
		vertices[gl_VertexIndex].texCoord[0],
		vertices[gl_VertexIndex].texCoord[1]);

    gl_Position =
		perFrameData.viewProjection *
		perFrameData.model *
		vec4(position, 1.0);

    outColor = 0.5 + 0.5 * normal;
}
