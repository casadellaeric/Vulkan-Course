#version 450

layout(location = 0) in vec3 color;
layout(location = 1) in vec2 texCoords;

layout(set = 1, binding = 0) uniform sampler2D textureSampler;

layout(location = 0) out vec4 outColor;

void main() {
	outColor = texture(textureSampler, texCoords);
}