#pragma once

#include <cstdint>

enum class KeyCode : int32_t {
	// From glfw3.h
	Space = 32,
	Apostrophe = 39, /* ' */
	Comma = 44,      /* , */
	Minus = 45,      /* - */
	Period = 46,     /* . */
	Slash = 47,      /* / */

	D0 = 48, /* 0 */
	D1 = 49, /* 1 */
	D2 = 50, /* 2 */
	D3 = 51, /* 3 */
	D4 = 52, /* 4 */
	D5 = 53, /* 5 */
	D6 = 54, /* 6 */
	D7 = 55, /* 7 */
	D8 = 56, /* 8 */
	D9 = 57, /* 9 */

	Semicolon = 59, /* ; */
	Equal = 61,     /* = */

	A = 65,
	B = 66,
	C = 67,
	D = 68,
	E = 69,
	F = 70,
	G = 71,
	H = 72,
	I = 73,
	J = 74,
	K = 75,
	L = 76,
	M = 77,
	N = 78,
	O = 79,
	P = 80,
	Q = 81,
	R = 82,
	S = 83,
	T = 84,
	U = 85,
	V = 86,
	W = 87,
	X = 88,
	Y = 89,
	Z = 90,

	LeftBracket = 91,  /* [ */
	Backslash = 92,    /* \ */
	RightBracket = 93, /* ] */
	GraveAccent = 96,  /* ` */

	Escape = 256,
	Enter = 257,
	Tab = 258,
	Backspace = 259,
	Insert = 260,
	Delete = 261,
	Right = 262,
	Left = 263,
	Down = 264,
	Up = 265,
	PageUp = 266,
	PageDown = 267,
	Home = 268,
	End = 269,
	CapsLock = 280,
	ScrollLock = 281,
	NumLock = 282,
	PrintScreen = 283,
	Pause = 284,
	F1 = 290,
	F2 = 291,
	F3 = 292,
	F4 = 293,
	F5 = 294,
	F6 = 295,
	F7 = 296,
	F8 = 297,
	F9 = 298,
	F10 = 299,
	F11 = 300,
	F12 = 301,

	LeftShift = 340,
	LeftControl = 341,
	LeftAlt = 342,
	LeftSuper = 343,
	RightShift = 344,
	RightControl = 345,
	RightAlt = 346,
	RightSuper = 347,
	Menu = 348
};

namespace Key {
	inline constexpr KeyCode Space = KeyCode::Space;
	inline constexpr KeyCode A = KeyCode::A;
	inline constexpr KeyCode B = KeyCode::B;
	inline constexpr KeyCode C = KeyCode::C;
	inline constexpr KeyCode D = KeyCode::D;
	inline constexpr KeyCode E = KeyCode::E;
	inline constexpr KeyCode F = KeyCode::F;
	inline constexpr KeyCode G = KeyCode::G;
	inline constexpr KeyCode H = KeyCode::H;
	inline constexpr KeyCode I = KeyCode::I;
	inline constexpr KeyCode J = KeyCode::J;
	inline constexpr KeyCode K = KeyCode::K;
	inline constexpr KeyCode L = KeyCode::L;
	inline constexpr KeyCode M = KeyCode::M;
	inline constexpr KeyCode N = KeyCode::N;
	inline constexpr KeyCode O = KeyCode::O;
	inline constexpr KeyCode P = KeyCode::P;
	inline constexpr KeyCode Q = KeyCode::Q;
	inline constexpr KeyCode R = KeyCode::R;
	inline constexpr KeyCode S = KeyCode::S;
	inline constexpr KeyCode T = KeyCode::T;
	inline constexpr KeyCode U = KeyCode::U;
	inline constexpr KeyCode V = KeyCode::V;
	inline constexpr KeyCode W = KeyCode::W;
	inline constexpr KeyCode X = KeyCode::X;
	inline constexpr KeyCode Y = KeyCode::Y;
	inline constexpr KeyCode Z = KeyCode::Z;
	inline constexpr KeyCode Escape = KeyCode::Escape;
	inline constexpr KeyCode Enter = KeyCode::Enter;
	inline constexpr KeyCode Tab = KeyCode::Tab;
	inline constexpr KeyCode LeftShift = KeyCode::LeftShift;
	inline constexpr KeyCode LeftControl = KeyCode::LeftControl;
	inline constexpr KeyCode LeftAlt = KeyCode::LeftAlt;
}
