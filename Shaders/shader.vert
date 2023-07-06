#version 450

//#extension GL_KHR_vulkan_glsl : enable

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;

layout(binding = 0) uniform MVP {
	mat4 model;
	mat4 view;
	mat4 projection;
} mvp;

layout(location = 0) out vec3 outColor;

void main() {
	gl_Position = mvp.projection * mvp.view * mvp.model * vec4(position, 1.);
	outColor = color;
}