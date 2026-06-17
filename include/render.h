#pragma once
#include "memory.h"
#include "log.h"
#include "maths.h"
#include "strings.h"

#include "render_common.h"

typedef struct r_init_options
{
	void *Layer;        // CAMetalLayer (opaque to non-Metal callers)
	void *Device;       // id<MTLDevice>
	str8 ShaderDir;
} r_init_options;

void R_Init(render_context *R, m_arena *Arena, v2i Size, r_init_options Options);
void R_BeginFrame(render_context *R, v2i ViewportSize);
void R_Update(render_context *R, void *Drawable);
