#version 450

//#extension GL_KHR_vulkan_glsl : enable

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;
layout(location = 2) in vec2 texCoords;

layout(set = 0, binding = 0) uniform UboViewProjection {
	mat4 view;
	mat4 projection;
} uboViewProjection;

// Not in use, left for reference
layout(set = 0, binding = 1) uniform UboModel {
	mat4 model;
} uboModel;

layout(push_constant) uniform PushModel{
	mat4 model;
} pushModel;

layout(location = 0) out vec3 outColor;
layout(location = 1) out vec2 outTexCoords;

void main() {
	gl_Position = uboViewProjection.projection * uboViewProjection.view * pushModel.model * vec4(position, 1.);
	outColor = color;
	outTexCoords = texCoords;
}