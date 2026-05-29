#version 120

uniform sampler2D portalTex;
varying vec2 v_uv;

void main()
{
    gl_FragData[0] = texture2D(portalTex, v_uv);
}
