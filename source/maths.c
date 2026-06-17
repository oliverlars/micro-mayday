#include "maths.h"
#include "strings.h"

#define _USE_MATH_DEFINES
#include <math.h>

random32_state XorShiftInit(void)
{
	return (random32_state){ .State = 2463534242 };
}

u32 XorShift32(random32_state *State)
{
	State->State ^= State->State << 13;
	State->State ^= State->State >> 17;
	State->State ^= State->State << 5;
	return State->State;
}

float RandomBilateral(random32_state *State)
{
	float R = ((float)XorShift32(State) / (float)UINT32_MAX);
	return R * 2.0f - 1.0f;
}

float RandomUnilateral(random32_state *State)
{
	return ((float)XorShift32(State) / (float)UINT32_MAX);
}

u64 M_FNVHashString(u64 Hash, str8 String)
{
	for (s64 Index = 0; Index < String.Size; Index++)
	{
		Hash *= M_FNV_64_PRIME;
		Hash ^= (u64)String.Bytes[Index];
	}
	return Hash;
}

u64 M_FNVHashBytes(u64 Hash, u8 *Bytes, int ByteCount)
{
	str8 BytesAsString = (str8){ByteCount, Bytes};
	return M_FNVHashString(Hash, BytesAsString);
}

f32 ClampF32(f32 V, f32 Lo, f32 Hi) { return V < Lo ? Lo : (V > Hi ? Hi : V); }
s32 ClampS32(s32 V, s32 Lo, s32 Hi) { return V < Lo ? Lo : (V > Hi ? Hi : V); }

f32 LerpF32(f32 A, f32 B, float T) { return A + T * (B - A); }

v2f AddV2F(v2f A, v2f B) { return v2f(A.X+B.X, A.Y+B.Y); }
v2f SubV2F(v2f A, v2f B) { return v2f(A.X-B.X, A.Y-B.Y); }
v2f MulV2F(v2f A, v2f B) { return v2f(A.X*B.X, A.Y*B.Y); }
v2f DivV2F(v2f A, v2f B) { return v2f(A.X/B.X, A.Y/B.Y); }
v2f LerpV2F(v2f A, v2f B, float T) { return v2f(A.X+T*(B.X-A.X), A.Y+T*(B.Y-A.Y)); }
f32 DotV2F(v2f A, v2f B) { return A.X*B.X + A.Y*B.Y; }
f32 LengthV2F(v2f V) { return sqrtf(DotV2F(V, V)); }
v2f NormalizeV2F(v2f V) { f32 L = LengthV2F(V); return DivV2F(V, v2f(L, L)); }

v2f V2FromF32(f32 X) { return (v2f){X, X}; }
v2i V2FromS32(s32 X) { return (v2i){X, X}; }

v3f AddV3F(v3f A, v3f B) { return v3f(A.X+B.X, A.Y+B.Y, A.Z+B.Z); }
v3f SubV3F(v3f A, v3f B) { return v3f(A.X-B.X, A.Y-B.Y, A.Z-B.Z); }
v3f MulV3F(v3f A, v3f B) { return v3f(A.X*B.X, A.Y*B.Y, A.Z*B.Z); }
v3f DivV3F(v3f A, v3f B) { return v3f(A.X/B.X, A.Y/B.Y, A.Z/B.Z); }
v3f LerpV3F(v3f A, v3f B, float T) { return v3f(A.X+T*(B.X-A.X), A.Y+T*(B.Y-A.Y), A.Z+T*(B.Z-A.Z)); }
f32 DotV3F(v3f A, v3f B) { return A.X*B.X + A.Y*B.Y + A.Z*B.Z; }
f32 LengthV3F(v3f V) { return sqrtf(DotV3F(V, V)); }
v3f NormalizeV3F(v3f V) { f32 L = LengthV3F(V); return DivV3F(V, v3f(L, L, L)); }
v3f CrossV3F(v3f A, v3f B) {
	return v3f(A.Y*B.Z - A.Z*B.Y, A.Z*B.X - A.X*B.Z, A.X*B.Y - A.Y*B.X);
}
v3f V3FromF32(f32 X) { return v3f(X, X, X); }

v4f AddV4F(v4f A, v4f B) { return v4f(A.X+B.X, A.Y+B.Y, A.Z+B.Z, A.W+B.W); }
v4f SubV4F(v4f A, v4f B) { return v4f(A.X-B.X, A.Y-B.Y, A.Z-B.Z, A.W-B.W); }
v4f MulV4F(v4f A, v4f B) { return v4f(A.X*B.X, A.Y*B.Y, A.Z*B.Z, A.W*B.W); }
v4f DivV4F(v4f A, v4f B) { return v4f(A.X/B.X, A.Y/B.Y, A.Z/B.Z, A.W/B.W); }
f32 DotV4F(v4f A, v4f B) { return A.X*B.X + A.Y*B.Y + A.Z*B.Z + A.W*B.W; }
v4f LerpV4F(v4f A, v4f B, f32 T) {
	return v4f(A.X+T*(B.X-A.X), A.Y+T*(B.Y-A.Y), A.Z+T*(B.Z-A.Z), A.W+T*(B.W-A.W));
}

v4f V4FromF32(f32 X) { return v4f(X, X, X, X); }

m44f M44F_IDENTITY = {
	1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 1, 0,
	0, 0, 0, 1
};

v4f MulM44FV4F(m44f A, v4f B)
{
	v4f R;
	R.X = A.E[0][0]*B.X + A.E[0][1]*B.Y + A.E[0][2]*B.Z + A.E[0][3]*B.W;
	R.Y = A.E[1][0]*B.X + A.E[1][1]*B.Y + A.E[1][2]*B.Z + A.E[1][3]*B.W;
	R.Z = A.E[2][0]*B.X + A.E[2][1]*B.Y + A.E[2][2]*B.Z + A.E[2][3]*B.W;
	R.W = A.E[3][0]*B.X + A.E[3][1]*B.Y + A.E[3][2]*B.Z + A.E[3][3]*B.W;
	return R;
}

m44f MulM44FM44F(m44f A, m44f B)
{
	m44f R;
	for (int I = 0; I < 4; I++)
	{
		for (int J = 0; J < 4; J++)
		{
			R.E[I][J] = A.E[I][0]*B.E[0][J] + A.E[I][1]*B.E[1][J] +
			            A.E[I][2]*B.E[2][J] + A.E[I][3]*B.E[3][J];
		}
	}
	return R;
}

m44f M44FTranspose(m44f M)
{
	m44f R;
	for (int I = 0; I < 4; I++)
		for (int J = 0; J < 4; J++)
			R.E[I][J] = M.E[J][I];
	return R;
}

m44f M44FromXAngle(f32 A)
{
	m44f R = M44F_IDENTITY;
	R.E[1][1] = cosf(A); R.E[1][2] = -sinf(A);
	R.E[2][1] = sinf(A); R.E[2][2] = cosf(A);
	return R;
}

m44f M44FromYAngle(f32 A)
{
	m44f R = M44F_IDENTITY;
	R.E[0][0] = cosf(A); R.E[0][2] = -sinf(A);
	R.E[2][0] = sinf(A); R.E[2][2] = cosf(A);
	return R;
}

m44f M44FromZAngle(f32 A)
{
	m44f R = M44F_IDENTITY;
	R.E[0][0] = cosf(A); R.E[0][1] = -sinf(A);
	R.E[1][0] = sinf(A); R.E[1][1] = cosf(A);
	return R;
}

m44f M44FPerspective(f32 Aspect, f32 FOV, f32 Near, f32 Far)
{
	FOV = (FOV/360.0f)*2.0f*(f32)M_PI;
	m44f R = M44F_IDENTITY;
	R.E[0][0] = 1.0f / (Aspect * tanf(FOV / 2.0f));
	R.E[1][1] = 1.0f / tanf(FOV / 2.0f);
	R.E[2][2] = -(Far + Near) / (Far - Near);
	R.E[2][3] = -1.0f;
	R.E[3][2] = -2.0f * (Far * Near) / (Far - Near);
	R.E[3][3] = 0.0f;
	return R;
}

m44f M44FOrthographic(f32 Left, f32 Right, f32 Top, f32 Bottom, f32 Near, f32 Far)
{
	m44f R = M44F_IDENTITY;
	R.E[0][0] =  2.0f/(Right - Left);
	R.E[1][1] =  2.0f/(Top - Bottom);
	R.E[2][2] = -2.0f/(Far - Near);
	R.E[3][0] = -(Right + Left)/(Right - Left);
	R.E[3][1] = -(Top + Bottom)/(Top - Bottom);
	R.E[3][2] = -(Far + Near)/(Far - Near);
	return R;
}

m44f M44FLookAt(v3f From, v3f To, v3f Up)
{
	v3f F = NormalizeV3F(SubV3F(From, To));
	v3f R = NormalizeV3F(CrossV3F(Up, F));
	v3f U = CrossV3F(F, R);
	v3f T = (v3f){ DotV3F(R, From), DotV3F(U, From), DotV3F(F, From) };
	return (m44f){
		R.X, U.X, F.X, 0,
		R.Y, U.Y, F.Y, 0,
		R.Z, U.Z, F.Z, 0,
		-T.X, -T.Y, -T.Z, 1,
	};
}

m44f M44FTranslate(v3f T)
{
	m44f R = M44F_IDENTITY;
	R.E[3][0] = T.X;
	R.E[3][1] = T.Y;
	R.E[3][2] = T.Z;
	return R;
}

m44f M44FScale(v3f S)
{
	m44f R = M44F_IDENTITY;
	R.E[0][0] = S.X;
	R.E[1][1] = S.Y;
	R.E[2][2] = S.Z;
	return R;
}

m44f M44FInverse(m44f Matrix)
{
	float F[16] = {
		Matrix.E[0][0], Matrix.E[0][1], Matrix.E[0][2], Matrix.E[0][3],
		Matrix.E[1][0], Matrix.E[1][1], Matrix.E[1][2], Matrix.E[1][3],
		Matrix.E[2][0], Matrix.E[2][1], Matrix.E[2][2], Matrix.E[2][3],
		Matrix.E[3][0], Matrix.E[3][1], Matrix.E[3][2], Matrix.E[3][3],
	};
	float Inv[16] = {0};

	Inv[0]  =  F[5]*F[10]*F[15] - F[5]*F[11]*F[14] - F[9]*F[6]*F[15] + F[9]*F[7]*F[14] + F[13]*F[6]*F[11] - F[13]*F[7]*F[10];
	Inv[4]  = -F[4]*F[10]*F[15] + F[4]*F[11]*F[14] + F[8]*F[6]*F[15] - F[8]*F[7]*F[14] - F[12]*F[6]*F[11] + F[12]*F[7]*F[10];
	Inv[8]  =  F[4]*F[9]*F[15] - F[4]*F[11]*F[13] - F[8]*F[5]*F[15] + F[8]*F[7]*F[13] + F[12]*F[5]*F[11] - F[12]*F[7]*F[9];
	Inv[12] = -F[4]*F[9]*F[14] + F[4]*F[10]*F[13] + F[8]*F[5]*F[14] - F[8]*F[6]*F[13] - F[12]*F[5]*F[10] + F[12]*F[6]*F[9];
	Inv[1]  = -F[1]*F[10]*F[15] + F[1]*F[11]*F[14] + F[9]*F[2]*F[15] - F[9]*F[3]*F[14] - F[13]*F[2]*F[11] + F[13]*F[3]*F[10];
	Inv[5]  =  F[0]*F[10]*F[15] - F[0]*F[11]*F[14] - F[8]*F[2]*F[15] + F[8]*F[3]*F[14] + F[12]*F[2]*F[11] - F[12]*F[3]*F[10];
	Inv[9]  = -F[0]*F[9]*F[15] + F[0]*F[11]*F[13] + F[8]*F[1]*F[15] - F[8]*F[3]*F[13] - F[12]*F[1]*F[11] + F[12]*F[3]*F[9];
	Inv[13] =  F[0]*F[9]*F[14] - F[0]*F[10]*F[13] - F[8]*F[1]*F[14] + F[8]*F[2]*F[13] + F[12]*F[1]*F[10] - F[12]*F[2]*F[9];
	Inv[2]  =  F[1]*F[6]*F[15] - F[1]*F[7]*F[14] - F[5]*F[2]*F[15] + F[5]*F[3]*F[14] + F[13]*F[2]*F[7]  - F[13]*F[3]*F[6];
	Inv[6]  = -F[0]*F[6]*F[15] + F[0]*F[7]*F[14] + F[4]*F[2]*F[15] - F[4]*F[3]*F[14] - F[12]*F[2]*F[7]  + F[12]*F[3]*F[6];
	Inv[10] =  F[0]*F[5]*F[15] - F[0]*F[7]*F[13] - F[4]*F[1]*F[15] + F[4]*F[3]*F[13] + F[12]*F[1]*F[7]  - F[12]*F[3]*F[5];
	Inv[14] = -F[0]*F[5]*F[14] + F[0]*F[6]*F[13] + F[4]*F[1]*F[14] - F[4]*F[2]*F[13] - F[12]*F[1]*F[6]  + F[12]*F[2]*F[5];
	Inv[3]  = -F[1]*F[6]*F[11] + F[1]*F[7]*F[10] + F[5]*F[2]*F[11] - F[5]*F[3]*F[10] - F[9]*F[2]*F[7]   + F[9]*F[3]*F[6];
	Inv[7]  =  F[0]*F[6]*F[11] - F[0]*F[7]*F[10] - F[4]*F[2]*F[11] + F[4]*F[3]*F[10] + F[8]*F[2]*F[7]   - F[8]*F[3]*F[6];
	Inv[11] = -F[0]*F[5]*F[11] + F[0]*F[7]*F[9]  + F[4]*F[1]*F[11] - F[4]*F[3]*F[9]  - F[8]*F[1]*F[7]   + F[8]*F[3]*F[5];
	Inv[15] =  F[0]*F[5]*F[10] - F[0]*F[6]*F[9]  - F[4]*F[1]*F[10] + F[4]*F[2]*F[9]  + F[8]*F[1]*F[6]   - F[8]*F[2]*F[5];

	float Det = F[0]*Inv[0] + F[1]*Inv[4] + F[2]*Inv[8] + F[3]*Inv[12];
	if (Det == 0.0f) return M44F_IDENTITY;
	Det = 1.0f / Det;

	m44f R;
	R.E[0][0] = Inv[0]*Det;  R.E[0][1] = Inv[1]*Det;  R.E[0][2] = Inv[2]*Det;  R.E[0][3] = Inv[3]*Det;
	R.E[1][0] = Inv[4]*Det;  R.E[1][1] = Inv[5]*Det;  R.E[1][2] = Inv[6]*Det;  R.E[1][3] = Inv[7]*Det;
	R.E[2][0] = Inv[8]*Det;  R.E[2][1] = Inv[9]*Det;  R.E[2][2] = Inv[10]*Det; R.E[2][3] = Inv[11]*Det;
	R.E[3][0] = Inv[12]*Det; R.E[3][1] = Inv[13]*Det; R.E[3][2] = Inv[14]*Det; R.E[3][3] = Inv[15]*Det;
	return R;
}
