#version 450

layout(input_attachment_index = 0, binding = 0) uniform subpassInput inputColor;
layout(input_attachment_index = 1, binding = 1) uniform subpassInput inputDepth;

layout(location = 0) out vec4 outColor;

void main() {
	int xHalf = 1200/2;
	if(gl_FragCoord.x > xHalf)
	{
		float lowerBound = 0.98;
		float higherBound = 1.;

		float depth = subpassLoad(inputDepth).r;
		float scaledDepth = 1. - ((depth - lowerBound)/(higherBound - lowerBound));
		outColor = vec4(scaledDepth, 0., scaledDepth, 1.);
	}
	else{
		outColor = subpassLoad(inputColor).rgba;
	}
}