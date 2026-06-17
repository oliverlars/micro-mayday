#include <metal_stdlib>

using namespace metal;

struct vertex_output
{
	float4 Position [[position]];
	float2 LocalPos;
	float PointSize;
	float4 Colour;
	float2 Centre;
};

struct vertex_input
{
	float4 Position;
	float4 Colour;
	float  PointSize;
	float2 Centre;
};

struct uniforms
{
	float4x4 PVMMatrix;
};

vertex vertex_output VertexMain(constant vertex_input *Vertices [[buffer(0)]],
								constant uniforms &Uniforms [[buffer(1)]],
								uint VertexID [[vertex_id]])
{
	vertex_output Output;
	Output.Position = Uniforms.PVMMatrix * Vertices[VertexID].Position;
	Output.LocalPos = Vertices[VertexID].Position.xy;
	Output.Colour = Vertices[VertexID].Colour;
	Output.PointSize = Vertices[VertexID].PointSize;
	Output.Centre = Vertices[VertexID].Centre;
	return Output;
}

float DistanceToCircle(float2 Position, float2 Centre, float Radius)
{
	return length(Position - Centre) - Radius; 
}

fragment float4 FragmentMain(vertex_output Input [[stage_in]])
{
	float HalfPointSize = 0.5*Input.PointSize;
	float Distance = DistanceToCircle(Input.LocalPos, Input.Centre, HalfPointSize);
	float AA = smoothstep(HalfPointSize, -HalfPointSize, Distance);
	return float4(Input.Colour.rgb, Input.Colour.a*float(Distance < 0.0));
}