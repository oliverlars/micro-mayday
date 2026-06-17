#pragma once

#include "misc.h"
#include "strings.h"

#define M_FNV_64_PRIME ((u64)0x100000001b3ULL)
#define M_FNV_HASH_START ((u64)0xcbf29ce484222325ULL)
u64 M_FNVHashString(u64 Hash, str8 String);
u64 M_FNVHashBytes(u64 Hash, u8 *Bytes, s32 ByteCount);

f32 LerpF32(f32 Source, f32 Target, float T);

f32 ClampF32(f32 Value, f32 Low, f32 High);
s32 ClampS32(s32 Value, s32 Low, s32 High);

typedef struct random32_state
{
	u32 State;
} random32_state;

f32 RandomBilateral(random32_state *State);
f32 RandomUnilateral(random32_state *State);
u32 XorShift32(random32_state *State);
random32_state XorShiftInit(void);

/* V2 */
typedef struct v2f { f32 X; f32 Y; } v2f;
#define v2f(...) (v2f){ __VA_ARGS__ }
v2f AddV2F(v2f A, v2f B);
v2f SubV2F(v2f A, v2f B);
v2f MulV2F(v2f A, v2f B);
v2f DivV2F(v2f A, v2f B);
v2f LerpV2F(v2f Source, v2f Target, float T);
f32 LengthV2F(v2f V);
v2f NormalizeV2F(v2f V);
f32 DotV2F(v2f A, v2f B);

typedef struct v2i { s32 X; s32 Y; } v2i;
#define v2i(...) (v2i){ __VA_ARGS__ }

v2f V2FromF32(f32 X);
v2i V2FromS32(s32 X);

/* V3 */
typedef struct v3f { f32 X; f32 Y; f32 Z; } v3f;
#define v3f(...) (v3f){ __VA_ARGS__ }
v3f AddV3F(v3f A, v3f B);
v3f SubV3F(v3f A, v3f B);
v3f MulV3F(v3f A, v3f B);
v3f DivV3F(v3f A, v3f B);
v3f LerpV3F(v3f Source, v3f Target, float T);
f32 LengthV3F(v3f V);
v3f NormalizeV3F(v3f V);
v3f CrossV3F(v3f A, v3f B);
f32 DotV3F(v3f A, v3f B);

v3f V3FromF32(f32 X);

/* V4 */
typedef struct v4f { f32 X; f32 Y; f32 Z; f32 W; } v4f;
#define v4f(...) (v4f){ __VA_ARGS__ }
v4f AddV4F(v4f A, v4f B);
v4f SubV4F(v4f A, v4f B);
v4f MulV4F(v4f A, v4f B);
v4f DivV4F(v4f A, v4f B);
f32 DotV4F(v4f A, v4f B);
v4f LerpV4F(v4f A, v4f B, f32 T);

typedef struct v4i { s32 X; s32 Y; s32 Z; s32 W; } v4i;
#define v4i(...) (v4i){ __VA_ARGS__ }

v4f V4FromF32(f32 X);

/* M44 */
typedef struct m44f { f32 E[4][4]; } m44f;
v4f MulM44FV4F(m44f A, v4f B);
m44f MulM44FM44F(m44f A, m44f B);

m44f M44FTranspose(m44f M);

m44f M44FromXAngle(f32 Angle);
m44f M44FromYAngle(f32 Angle);
m44f M44FromZAngle(f32 Angle);

m44f M44FPerspective(f32 Aspect, f32 FOV, f32 Near, f32 Far);
m44f M44FOrthographic(f32 Left, f32 Right, f32 Top, f32 Bottom, f32 Near, f32 Far);
m44f M44FLookAt(v3f From, v3f To, v3f ArbitraryUp);

m44f M44FTranslate(v3f Translate);
m44f M44FScale(v3f Scale);

m44f M44FInverse(m44f M);

extern m44f M44F_IDENTITY;
