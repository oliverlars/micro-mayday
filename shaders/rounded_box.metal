#include "sdf.metal"

// Pick the radius for the corner P falls into. P is in centre-relative pixels and
// y is down (framebuffer space), so P.x>0 is the right half and P.y>0 the bottom.
// Radii are ordered clockwise from top-left: (TopLeft, TopRight, BottomRight, BottomLeft).
float SelectCornerRadius(float2 P, float4 Radii)
{
	float2 R = (P.x > 0.0) ? float2(Radii.y, Radii.z)  // right: TopRight, BottomRight
	                       : float2(Radii.x, Radii.w); // left:  TopLeft,  BottomLeft
	return (P.y > 0.0) ? R.y : R.x;                    // bottom : top
}

// https://iquilezles.org/articles/distfunctions2d/
float DistanceToRoundedBox(float2 P, float2 HalfSize, float4 Radii)
{
	float Radius = SelectCornerRadius(P, Radii);
	float ClampedRadius = min(Radius, min(HalfSize.x, HalfSize.y));
	float2 Q = abs(P) - HalfSize + ClampedRadius;
	return min(max(Q.x, Q.y), 0.0) + length(max(Q, 0.0)) - ClampedRadius;
}

fragment float4 FragmentMain(vertex_output Input [[stage_in]],
                             constant uniforms &Uniforms [[buffer(1)]])
{
	float2 P = Input.LocalPos - Input.Centre;
	float Dist = DistanceToRoundedBox(P, Input.HalfSize, Input.CornerRadii);
	if (Input.ShadowBlur > 0.0) return ShadeShadow(Dist, Input.ShadowBlur, Input.Colour);
	return ShadeSDF(Dist, Input.LocalPos, Input.HalfSize,
	                Input.Colour, Input.OutlineColour, Input.OutlineThickness,
	                Input.Extrusion, 10.0, Uniforms);
}
