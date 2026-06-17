#pragma once
#include "strings.h"
#include "maths.h"

typedef void* os_handle;

typedef struct os_window
{
	int Width;
	int Height;

	os_handle Handle;   // NSWindow*
	os_handle MetalView; // MTKView*
	os_handle Layer;    // CAMetalLayer*
	os_handle Device;   // id<MTLDevice>

	str8 Title;
}
os_window;

typedef enum os_event_type
{
	OS_EVENT_NONE,
	OS_EVENT_RESIZE,
	OS_EVENT_KEY_DOWN,
	OS_EVENT_KEY_UP,
	OS_EVENT_MOUSE_DOWN,
	OS_EVENT_MOUSE_UP,
	OS_EVENT_MOUSE_MOVE,
	OS_EVENT_SCROLL,
	OS_EVENT_CLOSE,
} os_event_type;

// Mouse button number for MOUSE_DOWN/MOUSE_UP events (stored in KeyCode).
// Matches NSEvent.buttonNumber: 0=left, 1=right, 2=middle.
#define OS_MOUSE_LEFT   0
#define OS_MOUSE_RIGHT  1
#define OS_MOUSE_MIDDLE 2

// Named macOS virtual key codes (kVK_*). KeyCode in os_event uses these.
#define OS_KEY_A             0
#define OS_KEY_S             1
#define OS_KEY_D             2
#define OS_KEY_F             3
#define OS_KEY_H             4
#define OS_KEY_G             5
#define OS_KEY_Z             6
#define OS_KEY_X             7
#define OS_KEY_C             8
#define OS_KEY_V             9
#define OS_KEY_B             11
#define OS_KEY_Q             12
#define OS_KEY_W             13
#define OS_KEY_E             14
#define OS_KEY_R             15
#define OS_KEY_Y             16
#define OS_KEY_T             17
#define OS_KEY_1             18
#define OS_KEY_2             19
#define OS_KEY_3             20
#define OS_KEY_4             21
#define OS_KEY_6             22
#define OS_KEY_5             23
#define OS_KEY_EQUAL         24
#define OS_KEY_9             25
#define OS_KEY_7             26
#define OS_KEY_MINUS         27
#define OS_KEY_8             28
#define OS_KEY_0             29
#define OS_KEY_RIGHT_BRACKET 30
#define OS_KEY_O             31
#define OS_KEY_U             32
#define OS_KEY_LEFT_BRACKET  33
#define OS_KEY_I             34
#define OS_KEY_P             35
#define OS_KEY_RETURN        36
#define OS_KEY_L             37
#define OS_KEY_J             38
#define OS_KEY_QUOTE         39
#define OS_KEY_K             40
#define OS_KEY_SEMICOLON     41
#define OS_KEY_BACKSLASH     42
#define OS_KEY_COMMA         43
#define OS_KEY_SLASH         44
#define OS_KEY_N             45
#define OS_KEY_M             46
#define OS_KEY_PERIOD        47
#define OS_KEY_TAB           48
#define OS_KEY_SPACE         49
#define OS_KEY_GRAVE         50
#define OS_KEY_DELETE        51
#define OS_KEY_ESCAPE        53
#define OS_KEY_COMMAND       55
#define OS_KEY_SHIFT         56
#define OS_KEY_CAPS_LOCK     57
#define OS_KEY_OPTION        58
#define OS_KEY_CONTROL       59
#define OS_KEY_LEFT          123
#define OS_KEY_RIGHT         124
#define OS_KEY_DOWN          125
#define OS_KEY_UP            126

typedef struct os_event
{
	os_event_type Type;

	// For KEY_DOWN/KEY_UP: virtual key code (macOS kVK_* constants).
	// For MOUSE_DOWN/MOUSE_UP: button number (OS_MOUSE_LEFT etc).
	int KeyCode;

	// Set for every event that has a meaningful pointer location.
	v2f MousePosition;

	// Set for OS_EVENT_SCROLL — vertical scroll lives in Y, horizontal in X
	// (both in unspecified "scroll units", same sign convention as Cocoa).
	v2f Scroll;
} os_event;

typedef void (*os_frame_fn)(void *UserData, v2i ViewportSize);

os_window OS_MakeWindow(str8 Name, int Width, int Height);
void OS_Run(os_window *Window, os_frame_fn FrameFn, void *UserData);

void *OS_NextDrawable(os_window *Window); // CAMetalDrawable* (autoreleased)

int OS_WindowWidth(os_window *Window);
int OS_WindowHeight(os_window *Window);

// Cursor position relative to the window's content view, top-left origin, in
// the same pixel-space units the renderer uses for ViewportSize (so retina-
// scaled). Returns coordinates even when the cursor is outside the window.
v2f OS_MousePosition(os_window *Window);

// Pop the next event from the queue into *Out. Returns true if an event was
// returned, false if the queue is empty. Typical use inside the frame:
//
//   os_event E;
//   while (OS_NextEvent(Window, &E)) {
//       switch (E.Type) {
//           case OS_EVENT_MOUSE_DOWN: ... break;
//           case OS_EVENT_KEY_DOWN:   ... break;
//           ...
//       }
//   }
//
// The queue is drained event-at-a-time and is refilled by the host run loop
// between frames. It has finite capacity; overflow events are dropped.
b32 OS_NextEvent(os_window *Window, os_event *Out);

// State polling — true while the key or button is currently held.
b32 OS_KeyDown    (int Key);
b32 OS_MouseDown  (int Button);

// Transition polling — true on the single frame the key/button transitioned
// to down (Pressed) or up (Released). Reset at the start of each frame, so
// you'll only ever see one Pressed per physical press.
b32 OS_KeyPressed (int Key);
b32 OS_KeyReleased(int Key);
b32 OS_MousePressed (int Button);
b32 OS_MouseReleased(int Button);

u64 OS_TimeStampNow(void);
f32 OS_MilliSecondsFromTimeStamp(u64 TimeStamp);

str8 OS_LoadEntireFileIntoMemory(m_arena *Arena, str8 Filename);
