#version 400
uniform mat4 MVP;
uniform mat4 M;
uniform vec3 Eye = vec3(1);
in vec3 vPosition;
in vec3 vNormal;
out vec3 color;
out vec3 normal;
out vec3 vert2eye;
void main() {
    gl_Position = MVP * vec4(vPosition, 1.0);
    color = vec3(0.8f,0.8f,0.8f);
    normal = vNormal;
    vert2eye = Eye - (vec4(vPosition,1.0)*M).xyz;
}
