#version 450

#extension GL_EXT_shader_8bit_storage: require

struct Vertex
{
	float position[2];
	float uv[2];
	uint8_t color[4];
};

layout(binding = 0) readonly buffer Vertices { Vertex vertices[]; };

layout (push_constant) uniform PushConstants
{
	vec2 scale;
	vec2 translate;
} pushConstants;

layout (location = 0) out vec2 outUV;
layout (location = 1) out vec4 outColor;

void main() 
{
	outUV = vec2(
		vertices[gl_VertexIndex].uv[0],
		vertices[gl_VertexIndex].uv[1]);

	outColor = vec4(
		uint(vertices[gl_VertexIndex].color[0]),
		uint(vertices[gl_VertexIndex].color[1]),
		uint(vertices[gl_VertexIndex].color[2]),
		uint(vertices[gl_VertexIndex].color[3])) / 256.0;

	gl_Position = vec4(vec2(
			vertices[gl_VertexIndex].position[0],
			vertices[gl_VertexIndex].position[1]) *
		pushConstants.scale + pushConstants.translate, 0.0, 1.0);
}
