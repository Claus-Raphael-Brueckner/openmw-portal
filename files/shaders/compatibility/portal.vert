#version 120

uniform mat4 projectionMatrix;
varying vec2 v_uv;

void main()
{
    gl_Position = projectionMatrix * gl_ModelViewMatrix * gl_Vertex;
    v_uv = gl_MultiTexCoord0.xy;
}
