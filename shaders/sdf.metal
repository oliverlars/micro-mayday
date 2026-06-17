#include <metal_stdlib>

using namespace metal;

struct uniforms
{
	float4x4 PVMMatrix;
	float4   LightPosition;   // xyz = world-space position of point light, w = falloff radius (same units)
	float4   LightColour;
	float    LightIntensity;
	float    Pad[3];
};

// Field order follows enum bit order in r_vertex_attributes (see
// VERTEX_ATTRIBUTE_FIELDS in render_common.h) so the struct layout matches the
// CPU-packed vertex buffer. Param is the shape-specific scalar (corner radius
// for the rounded box, exponent for the squircle).
struct vertex_input
{
	float4 Position;
	float4 Colour;
	float  Param;
	float2 Centre;
	float2 HalfSize;
	float4 OutlineColour;
	float  OutlineThickness;
	float  Extrusion;
	float4 CornerRadii;
	float  ShadowBlur;
};

struct vertex_output
{
	float4 Position [[position]];
	float2 LocalPos;
	float4 Colour;
	float2 Centre;
	float2 HalfSize;
	float  Param;
	float4 OutlineColour;
	float  OutlineThickness;
	float  Extrusion;
	float4 CornerRadii;
	float  ShadowBlur;
};

vertex vertex_output VertexMain(constant vertex_input *Vertices [[buffer(0)]],
                                constant uniforms &Uniforms [[buffer(1)]],
                                uint VertexID [[vertex_id]])
{
	vertex_output Output;
	Output.Position         = Uniforms.PVMMatrix * Vertices[VertexID].Position;
	Output.LocalPos         = Vertices[VertexID].Position.xy;
	Output.Colour           = Vertices[VertexID].Colour;
	Output.Centre           = Vertices[VertexID].Centre;
	Output.HalfSize         = Vertices[VertexID].HalfSize;
	Output.Param            = Vertices[VertexID].Param;
	Output.OutlineColour    = Vertices[VertexID].OutlineColour;
	Output.OutlineThickness = Vertices[VertexID].OutlineThickness;
	Output.Extrusion        = Vertices[VertexID].Extrusion;
	Output.CornerRadii      = Vertices[VertexID].CornerRadii;
	Output.ShadowBlur       = Vertices[VertexID].ShadowBlur;
	return Output;
}

float4 ShadeShadow(float Dist, float Blur, float4 ShadowColour)
{
	float Coverage = 1.0 - smoothstep(-Blur, Blur, Dist);
	float Alpha = ShadowColour.a * Coverage;
	if (Alpha <= 0.0) discard_fragment();
	return float4(ShadowColour.rgb, Alpha);
}

#define AMBIENT 0.8

struct point_light
{
	float3 Position;
	float3 Colour;
	float Intensity;
	float Radius;
};

// Plain Blinn-Phong on a position + normal. Radius is the distance at which the
// non-ambient contribution falls to half — falloff is 1 / (1 + (d/R)^2) so the
// units of Radius must match Position / LightPos.
float3 ApplyLighting(
	float3 BaseColour,
	float3 Position,
	float3 Normal,
	float  SpecularAmount, 
	point_light PointLights[],
	int PointLightCount)
{
	float3 Contribution = float3(0.0);

	for(int Index = 0; Index < PointLightCount; Index++)
	{
		point_light Light = PointLights[Index];
		float3 L = normalize(Light.Position - Position);
		float3 V = float3(0.0, 0.0, 1.0);
		float3 H = normalize(L + V);

		float D = distance(Light.Position, Position) / max(Light.Radius, 1e-5);
		float Attenuation = 1.0 / (1.0 + D * D);

		float Lambert  = max(0.0, dot(Normal, L));
		float Specular = pow(max(0.0, dot(Normal, H)), SpecularAmount);
		Contribution += ((1.0 - AMBIENT) * Lambert * BaseColour + BaseColour * Specular) * Light.Colour * Light.Intensity * Attenuation;
	}
	return BaseColour * AMBIENT + Contribution;
}

// Turns a 2D signed distance field (pixels, negative inside) into the final
// blended + optionally bevel-lit colour. The only shape-specific input is Dist;
// every SDF shape shares the coverage, outline and fake-3D bevel logic here.
// BevelMax caps the bevel band width in pixels.
float4 ShadeSDF(float Dist, float2 LocalPos, float2 HalfSize,
                float4 Colour, float4 OutlineColour, float OutlineThickness,
                float Extrusion, float BevelMax, constant uniforms &Uniforms)
{
	float FillAlpha = clamp(0.5 - Dist / max(fwidth(Dist), 1e-5), 0.0, 1.0);
	if (FillAlpha <= 0.0) discard_fragment();

	// Fake-3D normal from the 2D field: treat the band Dist in [-Bevel, 0] as a
	// quarter-circle rolling from the flat top (normal +z) down to the silhouette
	// (normal horizontal). Extrusion scales the tilt and signs the roll direction
	// (+ raised lip, - recessed wall). At Extrusion ≈ 0 the tilt collapses to
	// (0,0,1) and Height to 0, leaving a flat surface still lit by the point light.
	float Bevel = min(BevelMax, min(HalfSize.x, HalfSize.y) * 0.5);

	// Gradient points outward (toward increasing distance / the edge). dfdy is
	// negated to undo the framebuffer's y-down orientation, so the tilt is in the
	// same y-up space as the light.
	float2 Grad    = float2(dfdx(Dist), -dfdy(Dist));
	float2 GradDir = Grad / max(length(Grad), 1e-5);

	float T       = clamp(-Dist / Bevel, 0.0, 1.0); // 0 at silhouette, 1 on flat top
	float Angle   = T * (3.14159265 * 0.5);
	float SlopeXY = cos(Angle);
	float SlopeZ  = sin(Angle);

	float3 Tilted = float3(sign(Extrusion) * GradDir * SlopeXY, SlopeZ);
	float3 Normal = normalize(mix(float3(0.0, 0.0, 1.0), Tilted, abs(Extrusion)));
	float  Height = Extrusion * SlopeZ * Bevel;

	float3 Position = float3(LocalPos, Height);
	point_light PointLights[3] = 
	{
		{Uniforms.LightPosition.xyz, Uniforms.LightColour.rgb, Uniforms.LightIntensity, Uniforms.LightPosition.w},
		{float3(Position.xy + 200, 10), float3(1, 1, 1), 1.0, 500},
		{float3(Position.xy - 200, 10), float3(1, 1, 1), 1.0, 500},
	};

	float3 BaseColour;
	float  BaseAlpha;
	float Specular;
	if (OutlineThickness > 0.0)
	{
		float PW = max(fwidth(Dist), 1e-5);
		float InteriorAlpha = clamp(0.5 - (Dist + OutlineThickness) / PW, 0.0, 1.0);
		BaseColour = mix(OutlineColour.rgb, Colour.rgb, InteriorAlpha);
		BaseAlpha  = mix(OutlineColour.a,   Colour.a,   InteriorAlpha);

		PointLights[1].Intensity = mix(1.5, 1.0, InteriorAlpha);
		PointLights[2].Intensity = mix(1.5, 1.0, InteriorAlpha);

		PointLights[0].Intensity = mix(0.0, 1.0, InteriorAlpha);
		Specular = mix(4.0, 32.0, InteriorAlpha);
	}
	else
	{
		BaseColour = Colour.rgb;
		BaseAlpha  = Colour.a;
		Specular = 32.0;
	}
	float3 Lit = ApplyLighting(
		BaseColour, Position, Normal, Specular,
		PointLights, 3
	);
	return float4(Lit, FillAlpha * BaseAlpha);
}
