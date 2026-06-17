#include <metal_stdlib>

using namespace metal;

struct vertex_output
{
	float4 Position [[position]];
	float2 LocalPos;
	float4 Colour;
	float2 Start;
	float2 End;
	float Thickness;
};

struct vertex_input
{
	float4 Position;
	float4 Colour;
	float2 Start;
	float2 End;
	float  Thickness;
};

struct uniforms
{
	float4x4 PVMMatrix;
};

// https://iquilezles.org/articles/distfunctions2d/
float DistanceToSegment(float2 Position, float2 Start, float2 End)
{
	float2 EndToStart = End - Start;
	float2 PositionToStart = Position - Start;
	float H = clamp( dot(PositionToStart, EndToStart)/dot(EndToStart,EndToStart), 0.0, 1.0);
	return length(PositionToStart - H*EndToStart);
}

vertex vertex_output VertexMain(constant vertex_input *Vertices [[buffer(0)]],
								constant uniforms &Uniforms [[buffer(1)]],
								uint VertexID [[vertex_id]])
{
	vertex_output Output;
	Output.Position = Uniforms.PVMMatrix * Vertices[VertexID].Position;
	Output.LocalPos = Vertices[VertexID].Position.xy;
	Output.Colour = Vertices[VertexID].Colour;
	Output.Start = Vertices[VertexID].Start;
	Output.End = Vertices[VertexID].End;
	Output.Thickness = Vertices[VertexID].Thickness;
	return Output;
}

fragment float4 FragmentMain(vertex_output Input [[stage_in]])
{
	float HalfThickness = 0.5 * Input.Thickness;
	float Distance = DistanceToSegment(Input.LocalPos, Input.Start, Input.End) - HalfThickness;
	float AA = smoothstep(HalfThickness + HalfThickness, HalfThickness - HalfThickness, Distance);

	return float4(Input.Colour.rgb, float(AA)*Input.Colour.a);
}