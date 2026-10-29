// Engine globals the harness has to provide.
//
// The engine defines this in src\sdk\moho\math\MathReflection.cpp:1295
// (`extern "C" int global_mode_sse2 = 1;`).  MathReflection.cpp is far too
// entangled to compile into the harness, but src\sdk\legacy\math\X87Math.cpp
// reads the symbol (X87Math.cpp:585, and its own `extern "C" int
// global_mode_sse2;` at X87Math.cpp:6), so the link needs a definition.
//
// The value is 1, exactly as the engine has it: the retail copy lives at
// 0x01107030 and is set by register_sseCompatMode (0x00A8ECBF) from the
// CPUID/movapd probe, i.e. 1 on any machine that runs the game.  The harness
// pokes the same 1 into the mapped retail image.
//
// This is the same trick the older mirror-based harness used.  If the engine
// ever moves the definition, this file must follow it.

extern "C" int global_mode_sse2 = 1;
