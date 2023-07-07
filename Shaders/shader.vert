#version 450

//#extension GL_KHR_vulkan_glsl : enable

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;

layout(binding = 0) uniform UboViewProjection {
	mat4 view;
	mat4 projection;
} uboViewProjection;

layout(binding = 1) uniform UboModel {
	mat4 model;
} uboModel;

layout(location = 0) out vec3 outColor;

void main() {
	gl_Position = uboViewProjection.projection * uboViewProjection.view * uboModel.model * vec4(position, 1.);
	outColor = color;
}