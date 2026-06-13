#version 120

uniform mat4 projectionMatrix;
varying vec4 v_clipPos;
varying vec2 v_texCoord;

void main()
{
    gl_Position = projectionMatrix * gl_ModelViewMatrix * gl_Vertex;
    v_clipPos   = gl_Position;
    v_texCoord  = gl_MultiTexCoord0.xy;
}
