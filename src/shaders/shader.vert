#version 450

// TODO-MILKRU: Since storageInputOutput16 is not supported,
// wait for PVF implementation to be done in order to use vertex attributes with float16_t and uint8_t,
// since storageBuffer16BitAccess is supported.
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texCoord;

layout(location = 0) out vec3 fragColor;

void main()
{
    gl_Position = vec4(position.xy, 0.0, 1.0);
    fragColor = 0.5 + 0.5 * normal;
}
