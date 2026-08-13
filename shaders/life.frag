#version 330 core
in vec2 vUV;
in vec4 vColor;

out vec4 FragColor;

uniform sampler2D mask;
uniform int mode;

void main() {
    if (mode == 0){
        FragColor = texture(mask, vUV);
    } 
    else {
        FragColor = vColor;
    }
}
