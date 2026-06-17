#include "render_common.h"
#include "misc.h"
#include "memory.h"

#include <assert.h>

r_vertex_attributes R_AttributesFromCommandType(r_command_type CommandType)
{
	r_vertex_attributes R = 0;
	switch (CommandType)
	{
	case R_COMMAND_TRIANGLE:
		R = R_VA_POSITION | R_VA_COLOUR;
		break;
	case R_COMMAND_POINTS:
		R = R_VA_POSITION | R_VA_COLOUR | R_VA_POINT_SIZE | R_VA_CENTRE;
		break;
	case R_COMMAND_BEZIER:
		R = R_VA_POSITION | R_VA_COLOUR | R_VA_START | R_VA_CONTROL | R_VA_END | R_VA_THICKNESS;
		break;
	case R_COMMAND_GLYPH:
		R = R_VA_POSITION | R_VA_COLOUR | R_VA_UV | R_VA_CURVE_START | R_VA_CURVE_COUNT;
		break;
	case R_COMMAND_LINES:
		R = R_VA_POSITION | R_VA_COLOUR | R_VA_START | R_VA_END | R_VA_THICKNESS;
		break;
	case R_COMMAND_ROUNDED_BOX:
	case R_COMMAND_SQUIRCLE:
		R = R_VA_POSITION | R_VA_COLOUR
		  | R_VA_THICKNESS | R_VA_CENTRE | R_VA_HALF_SIZE
		  | R_VA_OUTLINE_COLOUR | R_VA_OUTLINE_THICKNESS
		  | R_VA_EXTRUSION | R_VA_CORNER_RADII | R_VA_SHADOW_BLUR;
		break;
	}
	return R;
}

void R_PushCommand(render_context *R, r_command Command)
{
	m_arena *Arena = TemporaryArena();
	r_command *Next = (r_command*)ArenaPushZero(Arena, sizeof(r_command));
	Next[0] = Command;
	if (!R->CommandsLast) { R->CommandsFirst = Next; R->CommandsLast = Next; }
	else { R->CommandsLast->Next = Next; R->CommandsLast = Next; }
	assert(R->TransformCount >= 1);
	Next->Transform = R->TransformStack[R->TransformCount - 1];
	Next->Scissor = R->ScissorStack[R->ScissorCount - 1];
}

void R_PushTransform(render_context *R, m44f Transform)
{
	assert(R->TransformCount < MAX_TRANSFORMS);
	R->TransformStack[R->TransformCount++] = Transform;
}

void R_PopTransform(render_context *R)
{
	assert(R->TransformCount >= 1);
	--R->TransformCount;
}

void R_PushScissor(render_context *R, v4i Scissor)
{
	assert(R->ScissorCount < MAX_SCISSORS);
	R->ScissorStack[R->ScissorCount++] = Scissor;
}

void R_PopScissor(render_context *R)
{
	assert(R->ScissorCount >= 1);
	--R->ScissorCount;
}

v4i R_CurrentScissor(render_context *R)
{
	assert(R->ScissorCount >= 1);
	return R->ScissorStack[R->ScissorCount - 1];
}

m44f R_CurrentTransform(render_context *R)
{
	assert(R->TransformCount >= 1);
	return R->TransformStack[R->TransformCount - 1];
}

void R_SetLight(render_context *R, v3f Position, v3f Colour, f32 Intensity, f32 Radius)
{
	R->LightPosition  = Position;
	R->LightColour    = Colour;
	R->LightRadius    = Radius;
	R->LightIntensity = Intensity;
}
