// Copyright by Procgen Labs Ltd. All Rights Reserved.
//
// HLSL helper macro definitions for Voxel-to-HLSL translation.
// These are #define macros because PCG Custom HLSL injects user code INSIDE
// a function body, so function definitions are not allowed.
//
// Two patterns are used:
//   Expression macros: #define FOO(A,B) ((A)+(B))  — usable inline
//   Block macros: #define FOO(RESULT,A,B) { ... RESULT = ...; } — must be used as statements
//
// These are approximate re-implementations of Voxel Plugin ISPC functions.
// They are NOT bit-identical but produce visually comparable results.

#pragma once

#include "CoreMinimal.h"

class FPGLHLSLHelperLibrary
{
public:
	// Temp variable declarations needed by block macros.
	// These must be emitted at the top of the code block.
	static FString GetTempVariableDeclarations()
	{
		return TEXT(
			"// Temp variables for helper macros\n"
			"uint _pgl_h1, _pgl_h2, _pgl_h3;\n"
			"float2 _pgl_g2;\n"
			"float3 _pgl_g3;\n"
		);
	}

	// --- Hash ---
	// Block macro: PGL_HASH(DEST, K) — computes MurmurHash32-style hash, stores in DEST
	// PGL_HASH2(DEST, A, B) — hash with two inputs
	static FString GetHashMacro()
	{
		return TEXT(
			"#define PGL_HASH(DEST, K) { uint _ph = (uint)(K); _ph ^= _ph >> 16u; _ph *= 0x85ebca6bu; _ph ^= _ph >> 13u; _ph *= 0xc2b2ae35u; _ph ^= _ph >> 16u; DEST = _ph; }\n"
			"#define PGL_HASH2(DEST, A, B) { uint _pha; PGL_HASH(_pha, A); uint _phb; PGL_HASH(_phb, _pha ^ (uint)(B)); DEST = _phb; }\n"
			"#define PGL_HASH3(DEST, A, B, C) { uint _phab; PGL_HASH2(_phab, A, B); uint _phc; PGL_HASH(_phc, _phab ^ (uint)(C)); DEST = _phc; }\n"
			"#define PGL_HASH_TO_FLOAT(H) (float(H) / 4294967295.0f)\n"
		);
	}

	// --- Random ---
	// Block macro: PGL_RAND_RANGE(DEST, SEED, RANGE) — deterministic random float in range
	static FString GetRandRangeMacro()
	{
		return TEXT(
			"#define PGL_RAND_RANGE(DEST, SEED, RANGE) { uint _prh; PGL_HASH(_prh, SEED); DEST = lerp((RANGE).x, (RANGE).y, PGL_HASH_TO_FLOAT(_prh)); }\n"
		);
	}

	// --- Interpolation (expression macros) ---
	static FString GetLerpMacro()
	{
		return TEXT(
			"#define pgl_lerp(A, B, Alpha) ((A) + (Alpha) * ((B) - (A)))\n"
		);
	}

	static FString GetSmoothStepMacro()
	{
		return TEXT(
			"#define pgl_smoothstep(Edge0, Edge1, X) smoothstep(Edge0, Edge1, X)\n"
		);
	}

	static FString GetBilerpMacro()
	{
		return TEXT(
			"#define pgl_bilerp(X0Y0, X1Y0, X0Y1, X1Y1, Ax, Ay) lerp(lerp(X0Y0, X1Y0, Ax), lerp(X0Y1, X1Y1, Ax), Ay)\n"
		);
	}

	// --- Noise Common (expression + block macros) ---
	// These match the Voxel Plugin's FastNoise2-derived implementation exactly.
	static FString GetNoiseCommonMacros()
	{
		return TEXT(
			"// Noise Primes (from FastNoise2, used for lattice coordinate hashing)\n"
			"#define PGL_NOISE_PRIME_X 501125321\n"
			"#define PGL_NOISE_PRIME_Y 1136930381\n"
			"#define PGL_NOISE_PRIME_Z 1720413743\n"
			"\n"
			"// Noise lerp: matches ISPC formula A*(1-t)+B*t (NOT HLSL's built-in A+(B-A)*t)\n"
			"// The two forms are mathematically equivalent but differ in float32 rounding.\n"
			"// Using the ISPC form ensures noise output matches the Voxel Plugin CPU implementation.\n"
			"#define PGL_NLERP(A, B, T) ((A) * (1.0f - (T)) + (B) * (T))\n"
			"\n"
			"// InterpQuintic (Fade): expression macro — 6t^5 - 15t^4 + 10t^3\n"
			"#define PGL_FADE(T) ((T) * (T) * (T) * ((T) * ((T) * 6.0f - 15.0f) + 10.0f))\n"
			"\n"
			"// HashPrimes: block macro — the lattice hash used by noise (NOT MurmurHash)\n"
			"// Matches: int32 Hash = Seed; Hash ^= (A ^ B); Hash *= 0x27d4eb2d; return (Hash >> 15) ^ Hash;\n"
			"#define PGL_HASH_PRIMES(DEST, SEED, A, B) { int _hp = (int)(SEED); _hp ^= ((int)(A) ^ (int)(B)); _hp *= 0x27d4eb2d; DEST = (_hp >> 15) ^ _hp; }\n"
			"\n"
			"// GradientDot2D: expression macro — selects one of 8 gradient vectors and dots with (X,Y)\n"
			"// Gradients use 1+sqrt(2) = 2.414213562 and 1.0 in 8 symmetric directions\n"
			"#define PGL_GRAD_DOT2(H, X, Y) ( \\\n"
			"    ((H) & 4) == 0 \\\n"
			"    ? (((H) & 1) ? -2.414213562f : 2.414213562f) * (X) + (((H) & 2) ? -1.0f : 1.0f) * (Y) \\\n"
			"    : (((H) & 1) ? -1.0f : 1.0f) * (X) + (((H) & 2) ? -2.414213562f : 2.414213562f) * (Y) )\n"
			"\n"
			"// HashPrimes3: block macro — 3D lattice hash (Seed ^ A ^ B ^ C)\n"
			"#define PGL_HASH_PRIMES3(DEST, SEED, A, B, C) { int _hp3 = (int)(SEED); _hp3 ^= ((int)(A) ^ (int)(B) ^ (int)(C)); _hp3 *= 0x27d4eb2d; DEST = (_hp3 >> 15) ^ _hp3; }\n"
			"\n"
			"// GradientDot3D: expression macro — 12 cube-edge gradients via bit manipulation\n"
			"// Matches Voxel Plugin's GetGradientDot(Hash, X, Y, Z) using sign-bit XOR trick\n"
			"#define PGL_GRAD_DOT3(H, X, Y, Z) ( \\\n"
			"    asfloat(asint(((H) & 13) < 8 ? (X) : (Y)) ^ ((H) << 31)) + \\\n"
			"    asfloat(asint(((H) & 13) < 2 ? (Y) : (((H) & 13) == 12 ? (X) : (Z))) ^ (((H) & 2) << 30)) )\n"
			"\n"
			"// Legacy gradient macros (for Simplex/Value/Cellular which haven't been ported to exact algorithm yet)\n"
			"#define PGL_GRAD2(DEST, HASH_VAL) { uint _gh = (HASH_VAL) & 7u; float _gu = _gh < 4u ? float(int(_gh & 1u) * 2 - 1) : 0.0f; float _gv = _gh < 4u ? 0.0f : float(int(_gh & 1u) * 2 - 1); DEST = float2(_gu + _gv, _gu - _gv) * 0.70710678f; }\n"
			"#define PGL_GRAD3(DEST, HASH_VAL) { uint _g3h = (HASH_VAL) & 15u; float _g3u = _g3h < 8u ? float(int(_g3h & 1u) * 2 - 1) : 0.0f; float _g3v = _g3h < 4u ? float(int((_g3h >> 1u) & 1u) * 2 - 1) : (_g3h == 12u || _g3h == 14u ? float(int(_g3h & 1u) * 2 - 1) : 0.0f); float _g3w = _g3h >= 8u ? float(int((_g3h >> 1u) & 1u) * 2 - 1) : 0.0f; DEST = float3(_g3u, _g3v, _g3w); }\n"
		);
	}

	// --- Perlin Noise 2D ---
	// Block macro: PGL_PERLIN_2D(DEST, SEED, POS)
	// This is an exact port of the Voxel Plugin's GetPerlin2D from VoxelNoiseNodesImpl.isph
	// (FastNoise2-derived). Uses HashPrimes, noise primes, correct gradients, and normalization.
	//
	// Precision note: We split floor/frac BEFORE adding the small irrational offset to avoid
	// catastrophic cancellation when POS is large (e.g. Position / small_FeatureScale).
	// The offset is added to the fractional part (in [0,1)), keeping all arithmetic in [0,2).
	static FString GetPerlin2DMacro()
	{
		return TEXT(
			"#define PGL_PERLIN_2D(DEST, SEED, POS) { \\\n"
			"    float2 _pnRawFloor = floor((POS)); \\\n"
			"    float2 _pnFrac = (POS) - _pnRawFloor; \\\n"
			"    float2 _pnP = _pnFrac + float2(0.04902460144f, 0.02112610644f); \\\n"
			"    float2 _pnOffFloor = floor(_pnP); \\\n"
			"    float2 _pnFloor = _pnRawFloor + _pnOffFloor; \\\n"
			"    int2 _pnIA = int2(_pnFloor) * int2(PGL_NOISE_PRIME_X, PGL_NOISE_PRIME_Y); \\\n"
			"    int2 _pnIB = _pnIA + int2(PGL_NOISE_PRIME_X, PGL_NOISE_PRIME_Y); \\\n"
			"    float2 _pnAA = _pnP - _pnOffFloor; \\\n"
			"    float2 _pnAB = _pnAA - 1.0f; \\\n"
			"    float2 _pnU = float2(PGL_FADE(_pnAA.x), PGL_FADE(_pnAA.y)); \\\n"
			"    int _pnH; \\\n"
			"    PGL_HASH_PRIMES(_pnH, (int)(SEED), _pnIA.x, _pnIA.y) float _pnN00 = PGL_GRAD_DOT2(_pnH, _pnAA.x, _pnAA.y); \\\n"
			"    PGL_HASH_PRIMES(_pnH, (int)(SEED), _pnIB.x, _pnIA.y) float _pnN10 = PGL_GRAD_DOT2(_pnH, _pnAB.x, _pnAA.y); \\\n"
			"    PGL_HASH_PRIMES(_pnH, (int)(SEED), _pnIA.x, _pnIB.y) float _pnN01 = PGL_GRAD_DOT2(_pnH, _pnAA.x, _pnAB.y); \\\n"
			"    PGL_HASH_PRIMES(_pnH, (int)(SEED), _pnIB.x, _pnIB.y) float _pnN11 = PGL_GRAD_DOT2(_pnH, _pnAB.x, _pnAB.y); \\\n"
			"    DEST = 0.579106986522674560546875f * PGL_NLERP(PGL_NLERP(_pnN00, _pnN10, _pnU.x), PGL_NLERP(_pnN01, _pnN11, _pnU.x), _pnU.y); \\\n"
			"}\n"
		);
	}

	// --- Perlin Noise 3D ---
	// Exact port of the Voxel Plugin's GetPerlin3D from VoxelNoiseNodesImpl.isph
	// Same precision-preserving floor/frac split as 2D (see GetPerlin2DMacro comment).
	static FString GetPerlin3DMacro()
	{
		return TEXT(
			"#define PGL_PERLIN_3D(DEST, SEED, POS) { \\\n"
			"    float3 _p3RawFloor = floor((POS)); \\\n"
			"    float3 _p3Frac = (POS) - _p3RawFloor; \\\n"
			"    float3 _p3P = _p3Frac + float3(0.04902460144f, 0.02112610644f, 0.06403176963f); \\\n"
			"    float3 _p3OffFloor = floor(_p3P); \\\n"
			"    float3 _p3Floor = _p3RawFloor + _p3OffFloor; \\\n"
			"    int3 _p3IA = int3(_p3Floor) * int3(PGL_NOISE_PRIME_X, PGL_NOISE_PRIME_Y, PGL_NOISE_PRIME_Z); \\\n"
			"    int3 _p3IB = _p3IA + int3(PGL_NOISE_PRIME_X, PGL_NOISE_PRIME_Y, PGL_NOISE_PRIME_Z); \\\n"
			"    float3 _p3AA = _p3P - _p3OffFloor; \\\n"
			"    float3 _p3AB = _p3AA - 1.0f; \\\n"
			"    float3 _p3U = float3(PGL_FADE(_p3AA.x), PGL_FADE(_p3AA.y), PGL_FADE(_p3AA.z)); \\\n"
			"    int _p3H; \\\n"
			"    PGL_HASH_PRIMES3(_p3H, (int)(SEED), _p3IA.x, _p3IA.y, _p3IA.z) float _p3N000 = PGL_GRAD_DOT3(_p3H, _p3AA.x, _p3AA.y, _p3AA.z); \\\n"
			"    PGL_HASH_PRIMES3(_p3H, (int)(SEED), _p3IB.x, _p3IA.y, _p3IA.z) float _p3N100 = PGL_GRAD_DOT3(_p3H, _p3AB.x, _p3AA.y, _p3AA.z); \\\n"
			"    PGL_HASH_PRIMES3(_p3H, (int)(SEED), _p3IA.x, _p3IB.y, _p3IA.z) float _p3N010 = PGL_GRAD_DOT3(_p3H, _p3AA.x, _p3AB.y, _p3AA.z); \\\n"
			"    PGL_HASH_PRIMES3(_p3H, (int)(SEED), _p3IB.x, _p3IB.y, _p3IA.z) float _p3N110 = PGL_GRAD_DOT3(_p3H, _p3AB.x, _p3AB.y, _p3AA.z); \\\n"
			"    PGL_HASH_PRIMES3(_p3H, (int)(SEED), _p3IA.x, _p3IA.y, _p3IB.z) float _p3N001 = PGL_GRAD_DOT3(_p3H, _p3AA.x, _p3AA.y, _p3AB.z); \\\n"
			"    PGL_HASH_PRIMES3(_p3H, (int)(SEED), _p3IB.x, _p3IA.y, _p3IB.z) float _p3N101 = PGL_GRAD_DOT3(_p3H, _p3AB.x, _p3AA.y, _p3AB.z); \\\n"
			"    PGL_HASH_PRIMES3(_p3H, (int)(SEED), _p3IA.x, _p3IB.y, _p3IB.z) float _p3N011 = PGL_GRAD_DOT3(_p3H, _p3AA.x, _p3AB.y, _p3AB.z); \\\n"
			"    PGL_HASH_PRIMES3(_p3H, (int)(SEED), _p3IB.x, _p3IB.y, _p3IB.z) float _p3N111 = PGL_GRAD_DOT3(_p3H, _p3AB.x, _p3AB.y, _p3AB.z); \\\n"
			"    float _p3Lx00 = PGL_NLERP(_p3N000, _p3N100, _p3U.x); float _p3Lx10 = PGL_NLERP(_p3N010, _p3N110, _p3U.x); \\\n"
			"    float _p3Lx01 = PGL_NLERP(_p3N001, _p3N101, _p3U.x); float _p3Lx11 = PGL_NLERP(_p3N011, _p3N111, _p3U.x); \\\n"
			"    DEST = 0.964921414852142333984375f * PGL_NLERP(PGL_NLERP(_p3Lx00, _p3Lx10, _p3U.y), PGL_NLERP(_p3Lx01, _p3Lx11, _p3U.y), _p3U.z); \\\n"
			"}\n"
		);
	}

	// --- Simplex Noise 2D ---
	static FString GetSimplex2DMacro()
	{
		return TEXT(
			"#define PGL_SIMPLEX_2D(DEST, SEED, POS) { \\\n"
			"    const float _sF2 = 0.36602540378f; const float _sG2 = 0.21132486540f; \\\n"
			"    float _sS = ((POS).x + (POS).y) * _sF2; \\\n"
			"    int2 _sI = int2(floor((POS) + _sS)); \\\n"
			"    float _sT = float(_sI.x + _sI.y) * _sG2; \\\n"
			"    float2 _sX0 = (POS) - (float2(_sI) - _sT); \\\n"
			"    int2 _sI1 = _sX0.x > _sX0.y ? int2(1,0) : int2(0,1); \\\n"
			"    float2 _sX1 = _sX0 - float2(_sI1) + _sG2; \\\n"
			"    float2 _sX2 = _sX0 - 1.0f + 2.0f * _sG2; \\\n"
			"    float _sN0 = 0, _sN1 = 0, _sN2 = 0; uint _sH; float2 _sGr; \\\n"
			"    float _sT0 = 0.5f - dot(_sX0, _sX0); \\\n"
			"    if (_sT0 > 0) { _sT0 *= _sT0; PGL_HASH3(_sH, uint(_sI.x), (uint)(SEED), uint(_sI.y)); PGL_GRAD2(_sGr, _sH); _sN0 = _sT0 * _sT0 * dot(_sGr, _sX0); } \\\n"
			"    float _sT1 = 0.5f - dot(_sX1, _sX1); \\\n"
			"    if (_sT1 > 0) { _sT1 *= _sT1; PGL_HASH3(_sH, uint(_sI.x+_sI1.x), (uint)(SEED), uint(_sI.y+_sI1.y)); PGL_GRAD2(_sGr, _sH); _sN1 = _sT1 * _sT1 * dot(_sGr, _sX1); } \\\n"
			"    float _sT2 = 0.5f - dot(_sX2, _sX2); \\\n"
			"    if (_sT2 > 0) { _sT2 *= _sT2; PGL_HASH3(_sH, uint(_sI.x+1), (uint)(SEED), uint(_sI.y+1)); PGL_GRAD2(_sGr, _sH); _sN2 = _sT2 * _sT2 * dot(_sGr, _sX2); } \\\n"
			"    DEST = 70.0f * (_sN0 + _sN1 + _sN2); \\\n"
			"}\n"
		);
	}

	// --- Simplex Noise 3D ---
	static FString GetSimplex3DMacro()
	{
		return TEXT(
			"#define PGL_SIMPLEX_3D(DEST, SEED, POS) { \\\n"
			"    const float _s3F = 1.0f/3.0f; const float _s3G = 1.0f/6.0f; \\\n"
			"    float _s3S = ((POS).x + (POS).y + (POS).z) * _s3F; \\\n"
			"    int3 _s3I = int3(floor((POS) + _s3S)); \\\n"
			"    float _s3T = float(_s3I.x + _s3I.y + _s3I.z) * _s3G; \\\n"
			"    float3 _s3X0 = (POS) - (float3(_s3I) - _s3T); \\\n"
			"    int3 _s3I1, _s3I2; \\\n"
			"    if (_s3X0.x >= _s3X0.y) { if (_s3X0.y >= _s3X0.z) { _s3I1=int3(1,0,0); _s3I2=int3(1,1,0); } else if (_s3X0.x >= _s3X0.z) { _s3I1=int3(1,0,0); _s3I2=int3(1,0,1); } else { _s3I1=int3(0,0,1); _s3I2=int3(1,0,1); } } \\\n"
			"    else { if (_s3X0.y < _s3X0.z) { _s3I1=int3(0,0,1); _s3I2=int3(0,1,1); } else if (_s3X0.x < _s3X0.z) { _s3I1=int3(0,1,0); _s3I2=int3(0,1,1); } else { _s3I1=int3(0,1,0); _s3I2=int3(1,1,0); } } \\\n"
			"    float3 _s3X1 = _s3X0 - float3(_s3I1) + _s3G; \\\n"
			"    float3 _s3X2 = _s3X0 - float3(_s3I2) + 2.0f*_s3G; \\\n"
			"    float3 _s3X3 = _s3X0 - 1.0f + 3.0f*_s3G; \\\n"
			"    float _s3N = 0; uint _s3H; float3 _s3Gr; \\\n"
			"    float _s3Tn = 0.6f - dot(_s3X0,_s3X0); if (_s3Tn>0) { _s3Tn*=_s3Tn; PGL_HASH3(_s3H,uint(_s3I.x),(uint)(SEED),uint(_s3I.y)); { uint _t; PGL_HASH(_t,_s3H^uint(_s3I.z)); _s3H=_t; } PGL_GRAD3(_s3Gr,_s3H); _s3N+=_s3Tn*_s3Tn*dot(_s3Gr,_s3X0); } \\\n"
			"    _s3Tn = 0.6f - dot(_s3X1,_s3X1); if (_s3Tn>0) { _s3Tn*=_s3Tn; PGL_HASH3(_s3H,uint(_s3I.x+_s3I1.x),(uint)(SEED),uint(_s3I.y+_s3I1.y)); { uint _t; PGL_HASH(_t,_s3H^uint(_s3I.z+_s3I1.z)); _s3H=_t; } PGL_GRAD3(_s3Gr,_s3H); _s3N+=_s3Tn*_s3Tn*dot(_s3Gr,_s3X1); } \\\n"
			"    _s3Tn = 0.6f - dot(_s3X2,_s3X2); if (_s3Tn>0) { _s3Tn*=_s3Tn; PGL_HASH3(_s3H,uint(_s3I.x+_s3I2.x),(uint)(SEED),uint(_s3I.y+_s3I2.y)); { uint _t; PGL_HASH(_t,_s3H^uint(_s3I.z+_s3I2.z)); _s3H=_t; } PGL_GRAD3(_s3Gr,_s3H); _s3N+=_s3Tn*_s3Tn*dot(_s3Gr,_s3X2); } \\\n"
			"    _s3Tn = 0.6f - dot(_s3X3,_s3X3); if (_s3Tn>0) { _s3Tn*=_s3Tn; PGL_HASH3(_s3H,uint(_s3I.x+1),(uint)(SEED),uint(_s3I.y+1)); { uint _t; PGL_HASH(_t,_s3H^uint(_s3I.z+1)); _s3H=_t; } PGL_GRAD3(_s3Gr,_s3H); _s3N+=_s3Tn*_s3Tn*dot(_s3Gr,_s3X3); } \\\n"
			"    DEST = 32.0f * _s3N; \\\n"
			"}\n"
		);
	}

	// --- Cellular Noise 2D ---
	static FString GetCellular2DMacro()
	{
		return TEXT(
			"#define PGL_CELLULAR_2D(DEST, SEED, POS, JITTER) { \\\n"
			"    int2 _cCell = int2(floor(POS)); float _cMin = 1e10f; \\\n"
			"    for (int _cdy = -1; _cdy <= 1; _cdy++) \\\n"
			"    for (int _cdx = -1; _cdx <= 1; _cdx++) { \\\n"
			"        int2 _cNb = _cCell + int2(_cdx, _cdy); \\\n"
			"        uint _cH; PGL_HASH2(_cH, uint(_cNb.x), uint(_cNb.y)); \\\n"
			"        float2 _cOff = float2(float(_cH & 0xFFFFu), float((_cH >> 16u) & 0xFFFFu)) / 65535.0f * (JITTER); \\\n"
			"        float2 _cPt = float2(_cNb) + 0.5f + (_cOff - 0.5f * (JITTER)); \\\n"
			"        _cMin = min(_cMin, length((POS) - _cPt)); \\\n"
			"    } \\\n"
			"    DEST = _cMin; \\\n"
			"}\n"
		);
	}

	// --- Cellular Noise 3D ---
	static FString GetCellular3DMacro()
	{
		return TEXT(
			"#define PGL_CELLULAR_3D(DEST, SEED, POS, JITTER) { \\\n"
			"    int3 _c3Cell = int3(floor(POS)); float _c3Min = 1e10f; \\\n"
			"    for (int _c3dz = -1; _c3dz <= 1; _c3dz++) \\\n"
			"    for (int _c3dy = -1; _c3dy <= 1; _c3dy++) \\\n"
			"    for (int _c3dx = -1; _c3dx <= 1; _c3dx++) { \\\n"
			"        int3 _c3Nb = _c3Cell + int3(_c3dx, _c3dy, _c3dz); \\\n"
			"        uint _c3H; PGL_HASH3(_c3H, uint(_c3Nb.x), uint(_c3Nb.y), uint(_c3Nb.z)); \\\n"
			"        float3 _c3Off = float3(float(_c3H & 0x3FFu), float((_c3H >> 10u) & 0x3FFu), float((_c3H >> 20u) & 0x3FFu)) / 1023.0f * (JITTER); \\\n"
			"        float3 _c3Pt = float3(_c3Nb) + 0.5f + (_c3Off - 0.5f * (JITTER)); \\\n"
			"        _c3Min = min(_c3Min, length((POS) - _c3Pt)); \\\n"
			"    } \\\n"
			"    DEST = _c3Min; \\\n"
			"}\n"
		);
	}

	// --- Value Noise 2D ---
	static FString GetValue2DMacro()
	{
		return TEXT(
			"#define PGL_VALUE_2D(DEST, SEED, POS) { \\\n"
			"    int2 _vI = int2(floor(POS)); float2 _vF = frac(POS); \\\n"
			"    float2 _vU = float2(PGL_FADE(_vF.x), PGL_FADE(_vF.y)); \\\n"
			"    uint _vH; \\\n"
			"    PGL_HASH3(_vH, uint(_vI.x), (uint)(SEED), uint(_vI.y)); float _vV00 = PGL_HASH_TO_FLOAT(_vH); \\\n"
			"    PGL_HASH3(_vH, uint(_vI.x+1), (uint)(SEED), uint(_vI.y)); float _vV10 = PGL_HASH_TO_FLOAT(_vH); \\\n"
			"    PGL_HASH3(_vH, uint(_vI.x), (uint)(SEED), uint(_vI.y+1)); float _vV01 = PGL_HASH_TO_FLOAT(_vH); \\\n"
			"    PGL_HASH3(_vH, uint(_vI.x+1), (uint)(SEED), uint(_vI.y+1)); float _vV11 = PGL_HASH_TO_FLOAT(_vH); \\\n"
			"    DEST = lerp(lerp(_vV00, _vV10, _vU.x), lerp(_vV01, _vV11, _vU.x), _vU.y) * 2.0f - 1.0f; \\\n"
			"}\n"
		);
	}

	// --- Value Noise 3D ---
	static FString GetValue3DMacro()
	{
		return TEXT(
			"#define PGL_VALUE_3D(DEST, SEED, POS) { \\\n"
			"    int3 _v3I = int3(floor(POS)); float3 _v3F = frac(POS); \\\n"
			"    float3 _v3U = float3(PGL_FADE(_v3F.x), PGL_FADE(_v3F.y), PGL_FADE(_v3F.z)); \\\n"
			"    uint _v3H; \\\n"
			"    PGL_HASH3(_v3H, uint(_v3I.x), (uint)(SEED), uint(_v3I.y)); { uint _t; PGL_HASH(_t, _v3H^uint(_v3I.z)); _v3H=_t; } float _v3V000 = PGL_HASH_TO_FLOAT(_v3H); \\\n"
			"    PGL_HASH3(_v3H, uint(_v3I.x+1), (uint)(SEED), uint(_v3I.y)); { uint _t; PGL_HASH(_t, _v3H^uint(_v3I.z)); _v3H=_t; } float _v3V100 = PGL_HASH_TO_FLOAT(_v3H); \\\n"
			"    PGL_HASH3(_v3H, uint(_v3I.x), (uint)(SEED), uint(_v3I.y+1)); { uint _t; PGL_HASH(_t, _v3H^uint(_v3I.z)); _v3H=_t; } float _v3V010 = PGL_HASH_TO_FLOAT(_v3H); \\\n"
			"    PGL_HASH3(_v3H, uint(_v3I.x+1), (uint)(SEED), uint(_v3I.y+1)); { uint _t; PGL_HASH(_t, _v3H^uint(_v3I.z)); _v3H=_t; } float _v3V110 = PGL_HASH_TO_FLOAT(_v3H); \\\n"
			"    PGL_HASH3(_v3H, uint(_v3I.x), (uint)(SEED), uint(_v3I.y)); { uint _t; PGL_HASH(_t, _v3H^uint(_v3I.z+1)); _v3H=_t; } float _v3V001 = PGL_HASH_TO_FLOAT(_v3H); \\\n"
			"    PGL_HASH3(_v3H, uint(_v3I.x+1), (uint)(SEED), uint(_v3I.y)); { uint _t; PGL_HASH(_t, _v3H^uint(_v3I.z+1)); _v3H=_t; } float _v3V101 = PGL_HASH_TO_FLOAT(_v3H); \\\n"
			"    PGL_HASH3(_v3H, uint(_v3I.x), (uint)(SEED), uint(_v3I.y+1)); { uint _t; PGL_HASH(_t, _v3H^uint(_v3I.z+1)); _v3H=_t; } float _v3V011 = PGL_HASH_TO_FLOAT(_v3H); \\\n"
			"    PGL_HASH3(_v3H, uint(_v3I.x+1), (uint)(SEED), uint(_v3I.y+1)); { uint _t; PGL_HASH(_t, _v3H^uint(_v3I.z+1)); _v3H=_t; } float _v3V111 = PGL_HASH_TO_FLOAT(_v3H); \\\n"
			"    float _v3Lx00 = lerp(_v3V000, _v3V100, _v3U.x); float _v3Lx10 = lerp(_v3V010, _v3V110, _v3U.x); \\\n"
			"    float _v3Lx01 = lerp(_v3V001, _v3V101, _v3U.x); float _v3Lx11 = lerp(_v3V011, _v3V111, _v3U.x); \\\n"
			"    DEST = lerp(lerp(_v3Lx00, _v3Lx10, _v3U.y), lerp(_v3Lx01, _v3Lx11, _v3U.y), _v3U.z) * 2.0f - 1.0f; \\\n"
			"}\n"
		);
	}

	// --- Quaternion helpers ---
	static FString GetQuatFromEulerMacro()
	{
		return TEXT(
			"#define PGL_QUAT_FROM_EULER(DEST, EULER_DEG) { \\\n"
			"    float3 _qHR = (EULER_DEG) * (PI / 360.0f); \\\n"
			"    float3 _qC = cos(_qHR); float3 _qS = sin(_qHR); \\\n"
			"    DEST = float4( \\\n"
			"        _qC.z*_qS.x*_qC.y - _qS.z*_qC.x*_qS.y, \\\n"
			"        -_qC.z*_qS.x*_qS.y - _qS.z*_qC.x*_qC.y, \\\n"
			"        _qC.z*_qC.x*_qS.y - _qS.z*_qS.x*_qC.y, \\\n"
			"        _qC.z*_qC.x*_qC.y + _qS.z*_qS.x*_qS.y); \\\n"
			"}\n"
		);
	}

	static FString GetEulerFromQuatMacro()
	{
		return TEXT(
			"#define PGL_EULER_FROM_QUAT(DEST, Q) { \\\n"
			"    float _eqSing = (Q).z*(Q).x - (Q).w*(Q).y; \\\n"
			"    float _eqYaw = atan2(2.0f*((Q).w*(Q).z+(Q).x*(Q).y), 1.0f-2.0f*((Q).y*(Q).y+(Q).z*(Q).z)) * (180.0f/PI); \\\n"
			"    float _eqPitch = asin(clamp(2.0f*_eqSing, -1.0f, 1.0f)) * (180.0f/PI); \\\n"
			"    float _eqRoll = atan2(2.0f*((Q).w*(Q).x+(Q).y*(Q).z), 1.0f-2.0f*((Q).x*(Q).x+(Q).y*(Q).y)) * (180.0f/PI); \\\n"
			"    DEST = float3(_eqRoll, _eqPitch, _eqYaw); \\\n"
			"}\n"
		);
	}

	static FString GetQuatFromAxisMacro()
	{
		return TEXT(
			"#define PGL_QUAT_FROM_AXIS_X(DEST, ANGLE_DEG) { float _qax = (ANGLE_DEG)*(PI/360.0f); DEST = float4(sin(_qax),0,0,cos(_qax)); }\n"
			"#define PGL_QUAT_FROM_AXIS_Y(DEST, ANGLE_DEG) { float _qay = (ANGLE_DEG)*(PI/360.0f); DEST = float4(0,sin(_qay),0,cos(_qay)); }\n"
			"#define PGL_QUAT_FROM_AXIS_Z(DEST, ANGLE_DEG) { float _qaz = (ANGLE_DEG)*(PI/360.0f); DEST = float4(0,0,sin(_qaz),cos(_qaz)); }\n"
		);
	}

	// --- Distance Field ---
	static FString GetDistanceFieldColorMacro()
	{
		return TEXT(
			"#define PGL_DISTANCE_FIELD_COLOR(DEST, DIST) { float _dft = saturate((DIST)*0.01f+0.5f); DEST = float4(lerp(float3(0.2f,0.5f,1.0f), float3(1.0f,0.3f,0.1f), _dft), 1.0f); }\n"
		);
	}
};
