#version 450
#extension GL_ARB_separate_shader_objects : enable

out gl_PerVertex {
	vec4 gl_Position;
};

//shader inputs
layout(location = 0) in vec2 positions;
layout(location = 1) in vec3 inColor;

//shader output to fragment shader. only vertex shaders can take vertex attributes.
layout(location = 0) out vec3 fragColor;

void main() {
	gl_Position = vec4(positions, 0.0, 1.0);
	fragColor = inColor;

}