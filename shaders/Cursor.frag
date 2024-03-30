#version 450

layout(set = 0, binding = 1) uniform sampler2D texSampler;

layout(location = 0) in vec3 fragColor;

layout(location = 0) out vec4 outColor;

void main() {
    if (fragColor.r != 1.0) {
        outColor = vec4(fragColor, 0.0);
    } else {
        outColor = vec4(fragColor, 1.0);
    }
}