#version 450

#extension GL_EXT_shader_8bit_storage: require
#extension GL_EXT_shader_16bit_storage: require

// A structure has a scalar alignment equal to the largest scalar alignment of any of its members.
// https://www.khronos.org/registry/vulkan/specs/1.3-extensions/html/chap15.html#interfaces-resources-layout
struct Vertex
{
	float16_t position[3];
	uint8_t normal[2];
	float16_t texCoord[2];
};

layout(binding = 0) readonly buffer Vertices { Vertex vertices[]; };

layout(location = 0) out vec3 fragColor;

void main()
{
	vec3 position = vec3(
		vertices[gl_VertexIndex].position[0],
		vertices[gl_VertexIndex].position[1],
		vertices[gl_VertexIndex].position[2]);

	vec2 normal_xy = vec2(
		int(vertices[gl_VertexIndex].normal[0]),
		int(vertices[gl_VertexIndex].normal[1])) / 127.0 - 1.0;

	// Reconstruct normal vector Z component.
	// https://aras-p.info/texts/CompactNormalStorage.html
	float normal_z = sqrt(1.0 - dot(normal_xy, normal_xy));
	
	vec3 normal = vec3(normal_xy, normal_z);

	vec2 texcoord = vec2(
		vertices[gl_VertexIndex].texCoord[0],
		vertices[gl_VertexIndex].texCoord[1]);

    gl_Position = vec4(position.xy, 0.0, 1.0);
    fragColor = 0.5 + 0.5 * normal;
}
