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
        // Arch: circle (top) + rectangle (bottom).
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
    else if (portalMaskType == 2)
    {
        // Round: circle centered on the quad, radius = halfWidth.
        float du = v_texCoord.x - 0.5;
        float dv = v_texCoord.y - 0.5;
        // World-space circle test: du² + (dv*aspect)² ≤ 0.25
        if (du * du + dv * dv * portalAspect * portalAspect > 0.25) discard;
    }
    else if (portalMaskType == 3)
    {
        // Upper arc: circle top-aligned with the quad (same position as the arch in type 1),
        // but without the rectangle below — only the circular portion remains.
        // y_center in UV: 1 - 0.5/aspect  (aspect = halfHeight/halfWidth)
        float yc = 1.0 - 0.5 / portalAspect;
        float du = v_texCoord.x - 0.5;
        float dv = v_texCoord.y - yc;
        if (du * du + dv * dv * portalAspect * portalAspect > 0.25) discard;
    }
	else if (portalMaskType == 4)
    {
        // Abgerundete obere Ecken. Radius = Breite / 4 (0.25 in UV-Koordinaten).
        float r = 0.25;
        // yc ist die y-Koordinate, ab der die obere Rundung beginnt.
        float yc = 1.0 - r / portalAspect;

        if (v_texCoord.y > yc)
        {
            if (v_texCoord.x < r)
            {
                // Test für Kreis oben links
                float du = v_texCoord.x - r;
                float dv = v_texCoord.y - yc;
                if (du * du + dv * dv * portalAspect * portalAspect > r * r) discard;
            }
            else if (v_texCoord.x > 1.0 - r)
            {
                // Test für Kreis oben rechts
                float du = v_texCoord.x - (1.0 - r);
                float dv = v_texCoord.y - yc;
                if (du * du + dv * dv * portalAspect * portalAspect > r * r) discard;
            }
            // Pixel im mittleren oberen Bereich (Rect 1) passieren hier automatisch.
        }
        // Pixel im unteren Bereich (Rect 2, y <= yc) passieren ebenfalls automatisch.
    }

    vec2 uv = v_clipPos.xy / v_clipPos.w * 0.5 + 0.5;
    gl_FragData[0] = texture2D(portalTex, uv);
    gl_FragData[0].a = 1.0; // RTT alpha is unreliable when sky renders with GL_BLEND; portal quad is always opaque
}
