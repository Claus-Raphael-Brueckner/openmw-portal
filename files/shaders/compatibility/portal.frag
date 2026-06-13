#version 120

uniform sampler2D portalTex;
// 0 = full quad (default), 1 = arch (semicircle on top + rectangle below)
uniform int   portalMaskType;
// halfHeight / halfWidth — needed to convert UV distances to world-space circle
uniform float portalAspect;

varying vec4 v_clipPos;
varying vec2 v_texCoord;

void main()
{
    if (portalMaskType == 1)
    {
        // Circle radius = halfWidth; circle top aligned with portal top.
        // y_center in UV: 1 - 0.5/aspect  (aspect = halfHeight/halfWidth)
        float yc = 1.0 - 0.5 / portalAspect;
        float du = v_texCoord.x - 0.5;
        float dv = v_texCoord.y - yc;
        // World-space circle test: du² + (dv*aspect)² ≤ 0.25
        bool in_circle = (du * du + dv * dv * portalAspect * portalAspect) <= 0.25;
        // Rectangle: from v=0 up to the circle center, full width
        bool in_rect = v_texCoord.y <= yc;
        if (!in_circle && !in_rect) discard;
    }

    vec2 uv = v_clipPos.xy / v_clipPos.w * 0.5 + 0.5;
    gl_FragData[0] = texture2D(portalTex, uv);
    gl_FragData[0].a = 1.0; // RTT alpha is unreliable when sky renders with GL_BLEND; portal quad is always opaque
}
