#version 450

//#extension GL_KHR_vulkan_glsl : enable

// Triangle vertex positions (will put in vertex buffer later)
vec3 positions[3] = vec3[](
	vec3(0.0, -0.4, 0.0),
	vec3(0.4, 0.4, 0.0),
	vec3(-0.4, 0.4, 0.0)
);

// Triangle vertex colors
vec3 colors[3] = vec3[](
	vec3(1.0, 0.0, 0.0),
	vec3(0.0, 1.0, 0.0),
	vec3(0.0, 0.0, 1.0)
);

layout(location = 0) out vec3 fragColor;

void main() {
	gl_Position = vec4(positions[gl_VertexIndex], 1.0f);
	fragColor = colors[gl_VertexIndex];
}