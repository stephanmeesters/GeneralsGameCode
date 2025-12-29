/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <float.h>
#include <math.h>
#include <limits>
#include <Utility/stdint_adapter.h>

#ifndef UINT64_C
#define UINT64_C(val) (val##ULL)
#endif

// Quantizes float32 values to a stable byte representation for CRC.
class CRCFloatQuantizer
{
public:
	static void quantizeFloat32ToBytes( float x, double q, unsigned char out[8] )
	{
		// q must be positive and non-zero
		if( !( q > 0.0 ) )
		{
			// fallback: treat as NaN token
			const uint64_t token = UINT64_C( 0x7FF8000000000001 );
			u64ToLEBytes( token, out );
			return;
		}

		x = canonicalizeZero( x );

		// Canonicalize special values to stable tokens.
		if( isNanF( x ) )
		{
			// All NaNs collapse to one token
			const uint64_t token = UINT64_C( 0x7FF8000000000001 );
			u64ToLEBytes( token, out );
			return;
		}
		if( isInfF( x ) )
		{
			// Distinguish +inf / -inf
			const uint64_t token = ( x > 0.0f )
				? UINT64_C( 0x7FF0000000000001 )
				: UINT64_C( 0xFFF0000000000001 );
			u64ToLEBytes( token, out );
			return;
		}

		// Quantize: n = round(x / q)
		// Use double to reduce rounding noise during scaling.
		const double xd = static_cast<double>( x );
		double scaled = xd / q;

		// Clamp before rounding to avoid UB on huge casts.
		scaled = static_cast<double>( clampToI64( scaled ) );

		// Deterministic rounding rule
		const int64_t n = roundHalfAwayFromZero( scaled );

		// Serialize for CRC
		i64ToLEBytes( n, out );
	}

	static void quantizeFloat32ToBytes( float x, unsigned char out[8] )
	{
		quantizeFloat32ToBytes( x, defaultQuantum(), out );
	}

	static double defaultQuantum( void )
	{
		return 1.0e-6;
	}

private:
	// helpers: classify float without C++11 <cmath> isnan/isinf
	static bool isNanF( float x )
	{
		return x != x;
	}

	static bool isInfF( float x )
	{
		// Works for IEEE-754: +inf > FLT_MAX, -inf < -FLT_MAX.
		// Also catches some overflowed values; that's fine for CRC canonicalization.
		return ( x > FLT_MAX ) || ( x < -FLT_MAX );
	}

	// Canonicalize -0.0 to +0.0 deterministically.
	static float canonicalizeZero( float x )
	{
		return ( x == 0.0f ) ? 0.0f : x;
	}

	// Deterministic rounding: half away from zero.
	// This avoids dependence on the current FP rounding mode.
	static int64_t roundHalfAwayFromZero( double v )
	{
		if( v >= 0.0 )
		{
			return static_cast<int64_t>( floor( v + 0.5 ) );
		}
		return static_cast<int64_t>( ceil( v - 0.5 ) );
	}

	// Clamp to int64 range safely.
	static int64_t clampToI64( double v )
	{
		const double hi = static_cast<double>( maxI64() );
		const double lo = static_cast<double>( minI64() );
		if( v > hi )
		{
			return maxI64();
		}
		if( v < lo )
		{
			return minI64();
		}
		return static_cast<int64_t>( v );
	}

	// Serialize uint64 to little-endian bytes (stable across host endianness).
	static void u64ToLEBytes( uint64_t u, unsigned char out[8] )
	{
		out[0] = static_cast<unsigned char>( ( u >> 0 ) & 0xFF );
		out[1] = static_cast<unsigned char>( ( u >> 8 ) & 0xFF );
		out[2] = static_cast<unsigned char>( ( u >> 16 ) & 0xFF );
		out[3] = static_cast<unsigned char>( ( u >> 24 ) & 0xFF );
		out[4] = static_cast<unsigned char>( ( u >> 32 ) & 0xFF );
		out[5] = static_cast<unsigned char>( ( u >> 40 ) & 0xFF );
		out[6] = static_cast<unsigned char>( ( u >> 48 ) & 0xFF );
		out[7] = static_cast<unsigned char>( ( u >> 56 ) & 0xFF );
	}

	// Serialize int64 to little-endian bytes (stable across host endianness).
	static void i64ToLEBytes( int64_t n, unsigned char out[8] )
	{
		u64ToLEBytes( static_cast<uint64_t>( n ), out );
	}

	static int64_t maxI64( void )
	{
#if defined( INT64_MAX )
		return static_cast<int64_t>( INT64_MAX );
#else
		return std::numeric_limits<int64_t>::max();
#endif
	}

	static int64_t minI64( void )
	{
#if defined( INT64_MIN )
		return static_cast<int64_t>( INT64_MIN );
#else
		return std::numeric_limits<int64_t>::min();
#endif
	}
};
