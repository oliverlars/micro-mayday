#include <metal_stdlib>
using namespace metal;

struct vertex_output
{
	float4 Position [[position]];
	uint CurveStart;
	uint CurveCount;
	float4 Colour;
	float2 LocalPos;
};

struct vertex_input
{
	float4 Position;
	float4 Colour;
	float2 LocalPos;
	uint CurveStart;
	uint CurveCount;
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
	Output.Colour = Vertices[VertexID].Colour;
	Output.LocalPos = Vertices[VertexID].LocalPos;
	Output.CurveStart = Vertices[VertexID].CurveStart;
	Output.CurveCount = Vertices[VertexID].CurveCount;
	return Output;
}

float2 CalculateQuadraticRoots(float A, float B, float C)
{
	float Epsilon = 1e-5;
	float2 Roots = float2(-99999.0, -99999.0);
	if (fabs(A) < Epsilon)
	{
		if (B != 0.0) Roots.x = -C / B;
	}
	else
	{
		float Discriminant = B * B - 4.0 * A * C;
		if (Discriminant > -Epsilon)
		{
			float S = sqrt(fmax(0.0, Discriminant));
			Roots.x = (-B + S) / (2.0 * A);
			Roots.y = (-B - S) / (2.0 * A);
		}
	}
	return Roots;
}

fragment float4 FragmentMain(vertex_output Input [[stage_in]],
							 texture2d<float> FontAtlas [[texture(0)]],
							 device const float2 *Curves [[buffer(0)]],
							 sampler Sampler [[sampler(0)]])
{
	float PixelWidth = fabs(dfdx(Input.LocalPos.x));
	float PixelHeight = fabs(dfdy(Input.LocalPos.y));
	float InversePixelWidth = 1.0 / PixelWidth;

	float AlphaSum = 0.0;

	for(int OffsetX = -1; OffsetX <= 1; OffsetX++)
	{
		for(int OffsetY = -1; OffsetY <= 1; OffsetY++)
		{
			float2 SamplePos = Input.LocalPos + float2(float(OffsetX) * PixelWidth / 3.0, float(OffsetY) * PixelHeight / 3.0);
			float Coverage = 0.0;

			for (uint I = 0; I < Input.CurveCount; I += 3)
			{
				float2 P0 = Curves[(Input.CurveStart + I) + 0] - SamplePos;
				float2 P1 = Curves[(Input.CurveStart + I) + 1] - SamplePos;
				float2 P2 = Curves[(Input.CurveStart + I) + 2] - SamplePos;

				bool IsDownwardCurve = (P0.y > 0.0) || (P2.y < 0.0);

				if (IsDownwardCurve)
				{
					if (P0.y < 0.0 && P2.y <= 0.0) continue;
					if (P0.y > 0.0 && P2.y >= 0.0) continue;
				}
				else
				{
					if (P0.y <= 0.0 && P2.y < 0.0) continue;
					if (P0.y >= 0.0 && P2.y > 0.0) continue;
				}

				float2 A = P0 - 2.0 * P1 + P2;
				float2 B = 2.0 * (P1 - P0);
				float2 C = P0;

				float2 Roots = CalculateQuadraticRoots(A.y, B.y, C.y);
				float Epsilon = 1e-4;

				bool OnSeg0 = (Roots.x >= -Epsilon) && (Roots.x <= 1.0 + Epsilon);
				bool OnSeg1 = (Roots.y >= -Epsilon) && (Roots.y <= 1.0 + Epsilon);

				float T0 = clamp(Roots.x, 0.0, 1.0);
				float T1 = clamp(Roots.y, 0.0, 1.0);

				float Intersect0 = A.x * T0 * T0 + B.x * T0 + C.x;
				float Intersect1 = A.x * T1 * T1 + B.x * T1 + C.x;

				int Sign = IsDownwardCurve ? 1 : -1;
				if (OnSeg0) Coverage += clamp(0.5 + Intersect0 * InversePixelWidth, 0.0, 1.0) * Sign;
				if (OnSeg1) Coverage += clamp(0.5 + Intersect1 * InversePixelWidth, 0.0, 1.0) * Sign;
			}

			AlphaSum += Coverage;
		}

	}

	// Clamp Alpha. With float framebuffer, out-of-range Alpha ends up writing
	// negative or HDR values via SrcAlpha/OneMinusSrcAlpha blend:
	// dst = rgb*a*Alpha + dst*(1 - a*Alpha). Alpha>1 makes (1-a*Alpha)<0 →
	// negative dst channels → composite `1 - exp(-negative)` produces large
	// values → whites/artifacts at winding-rule edges.
	float Alpha = clamp(fabs(AlphaSum) / 9.0, 0.0, 1.0);
	return float4(Input.Colour.rgb, Input.Colour.a * Alpha);
	//return float4(mix(float3(0, 1, 1), float3(1, 1, 1), Alpha), 1.0);
}
