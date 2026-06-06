#version 120

uniform sampler2D portalTex;
varying vec4 v_clipPos;

void main()
{
    vec2 uv = v_clipPos.xy / v_clipPos.w * 0.5 + 0.5;
    gl_FragData[0] = texture2D(portalTex, uv);
    gl_FragData[0].a = 1.0; // RTT alpha is unreliable when sky renders with GL_BLEND; portal quad is always opaque
}
