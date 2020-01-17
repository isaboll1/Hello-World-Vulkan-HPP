#version 450
#extension GL_ARB_separate_shader_objects : enable

//accept vertex attribute input for use in fragment shader.
layout(location = 0) in vec3 fragColor;

//output color based on vertex attriute input.
layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(fragColor, 1.0);
}