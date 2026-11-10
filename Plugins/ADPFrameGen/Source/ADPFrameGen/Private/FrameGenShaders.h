// The frame generation's shaders (compiled once at start with Windows' own shader compiler): each a small compute
// pass on the graphics card. Written from scratch for Airport Dynasty Pro.
//  Convert  the real frame into our working copy (the display's own format)
//  Luma4    brightness at a quarter of the size (each 4x4 block averaged): what the motion search compares
//  Down2    half that size again (the coarser search levels)
//  Flow     the motion search at one level: for each point, where its 5x5 patch was in the previous frame, searched
//           round the coarser level's answer (a small cost for longer jumps keeps flat areas still)
//  Smooth   the motion evened out (a 3x3 average weighted by how alike the neighbours are)
//  Interp   the in-between frame: each point taken half-way back along its motion from the new frame and half-way
//           forward from the old one, blended; what didn't change at all (the menus, the information boxes, still
//           parts of the picture) stays exactly as it is
#pragma once

static const char* GFrameGenHlsl = R"HLSL(
cbuffer C : register(b0) { uint2 Size; uint2 Src; float4 P; };
Texture2D<float4> T0 : register(t0);
Texture2D<float4> T1 : register(t1);
Texture2D<float4> T2 : register(t2);
RWTexture2D<float4> U0 : register(u0);
SamplerState S : register(s0);

float Luma(float3 c) { return dot(c, float3(0.299, 0.587, 0.114)); }

[numthreads(8, 8, 1)] void Convert(uint3 id : SV_DispatchThreadID)
{
	if (any(id.xy >= Size)) return;
	U0[id.xy] = float4(T0[id.xy].rgb, 1);
}

[numthreads(8, 8, 1)] void Luma4(uint3 id : SV_DispatchThreadID)
{
	if (any(id.xy >= Size)) return;
	float s = 0;
	[unroll] for (int y = 0; y < 4; y++) [unroll] for (int x = 0; x < 4; x++)
		s += Luma(T0[min(id.xy * 4 + uint2(x, y), Src - 1)].rgb);
	U0[id.xy] = float4(s / 16, 0, 0, 0);
}

[numthreads(8, 8, 1)] void Down2(uint3 id : SV_DispatchThreadID)
{
	if (any(id.xy >= Size)) return;
	uint2 b = id.xy * 2;
	float s = T0[min(b, Src - 1)].x + T0[min(b + uint2(1, 0), Src - 1)].x + T0[min(b + uint2(0, 1), Src - 1)].x + T0[min(b + 1, Src - 1)].x;
	U0[id.xy] = float4(s / 4, 0, 0, 0);
}

float At(Texture2D<float4> t, int2 p) { return t[clamp(p, int2(0, 0), int2(Size) - 1)].x; }

[numthreads(8, 8, 1)] void Flow(uint3 id : SV_DispatchThreadID)
{
	if (any(id.xy >= Size)) return;
	int2 p = int2(id.xy);
	// the coarser level's answer, twice as long here (none on the coarsest level)
	float2 init = P.w > 0 ? T2.SampleLevel(S, (float2(p) + 0.5) / float2(Size), 0).xy * 2 : float2(0, 0);
	int2 i0 = int2(round(init));
	int r = int(P.x);
	float best = 1e9; int2 bd = int2(0, 0);
	for (int dy = -r; dy <= r; dy++)
		for (int dx = -r; dx <= r; dx++)
		{
			int2 d = i0 + int2(dx, dy);
			float c = 0;
			[unroll] for (int ky = -2; ky <= 2; ky++)
				[unroll] for (int kx = -2; kx <= 2; kx++)
					c += abs(At(T0, p + int2(kx, ky)) - At(T1, p + int2(kx, ky) + d));
			c += P.y * length(float2(d));
			if (c < best) { best = c; bd = d; }
		}
	U0[id.xy] = float4(float2(bd), best, 0);
}

[numthreads(8, 8, 1)] void Smooth(uint3 id : SV_DispatchThreadID)
{
	if (any(id.xy >= Size)) return;
	int2 p = int2(id.xy);
	float2 c = T0[p].xy, s = 0; float w = 0;
	[unroll] for (int y = -1; y <= 1; y++) [unroll] for (int x = -1; x <= 1; x++)
	{
		float2 f = T0[clamp(p + int2(x, y), int2(0, 0), int2(Size) - 1)].xy;
		float k = 1 / (1 + dot(f - c, f - c));
		s += f * k; w += k;
	}
	U0[id.xy] = float4(s / w, 0, 0);
}

[numthreads(8, 8, 1)] void Interp(uint3 id : SV_DispatchThreadID)
{
	if (any(id.xy >= Size)) return;
	float3 o = T0[id.xy].rgb, n = T1[id.xy].rgb;
	// nothing changed here: exactly as it is (menus, boxes, still parts of the picture)
	if (all(o == n)) { U0[id.xy] = float4(n, 1); return; }
	float2 uv = (float2(id.xy) + 0.5) / float2(Size);
	float2 f = T2.SampleLevel(S, uv, 0).xy * 4;     // the motion, in full-size pixels (new to old)
	float2 h = 0.5 * f / float2(Size);
	float3 a = T0.SampleLevel(S, uv + h, 0).rgb;    // the old frame, half-way forward
	float3 b = T1.SampleLevel(S, uv - h, 0).rgb;    // the new frame, half-way back
	// where the two disagree a lot (something uncovered), lean on the new frame
	float k = saturate(abs(Luma(a) - Luma(b)) * 4);
	U0[id.xy] = float4(lerp((a + b) * 0.5, b, k * 0.5), 1);
}
)HLSL";
