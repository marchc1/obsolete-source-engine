//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: - defines SIMD "structure of arrays" classes and functions.
//
//===========================================================================//
#ifndef SSEQUATMATH_H
#define SSEQUATMATH_H

#ifdef _WIN32
#pragma once
#endif


#include "mathlib/ssemath.h"

// If you compile assuming the presence of SSE2, the MSVC will abandon
// the traditional x87 FPU operations altogether and make everything use
// the SSE2 registers, which lessens this problem a little.

// dimhotepus: Enable SSE quaternions as we use SSE2+ for all code.
#define ALLOW_SIMD_QUATERNION_MATH 1

//---------------------------------------------------------------------
// Load/store quaternions
//---------------------------------------------------------------------

#if ALLOW_SIMD_QUATERNION_MATH

// Using STDC or SSE
FORCEINLINE fltx4 LoadAlignedSIMD( const QuaternionAligned & pSIMD )
{
	return LoadAlignedSIMD( pSIMD.Base() );
}

FORCEINLINE fltx4 LoadAlignedSIMD( const QuaternionAligned * RESTRICT pSIMD )
{
	return LoadAlignedSIMD( pSIMD );
}

FORCEINLINE void StoreAlignedSIMD( QuaternionAligned * RESTRICT pSIMD, const fltx4 & a )
{
	StoreAlignedSIMD( pSIMD->Base(), a );
}

//---------------------------------------------------------------------
// Make sure quaternions are within 180 degrees of one another, if not, reverse q
//---------------------------------------------------------------------
FORCEINLINE fltx4 QuaternionAlignSIMD( const fltx4 &p, const fltx4 &q )
{
	// decide if one of the quaternions is backwards
	fltx4 a = SubSIMD( p, q );
	fltx4 b = AddSIMD( p, q );
	a = Dot4SIMD( a, a );
	b = Dot4SIMD( b, b );
	fltx4 cmp = CmpGtSIMD( a, b );
	fltx4 result = MaskedAssign( cmp, NegSIMD(q), q );
	return result;
}

//---------------------------------------------------------------------
// Normalize Quaternion
//---------------------------------------------------------------------
#if USE_STDC_FOR_SIMD

FORCEINLINE fltx4 QuaternionNormalizeSIMD( const fltx4 &q )
{
	fltx4 radius, result;
	radius = Dot4SIMD( q, q );

	if ( SubFloat( radius, 0 ) ) // > FLT_EPSILON && ((radius < 1.0f - 4*FLT_EPSILON) || (radius > 1.0f + 4*FLT_EPSILON))
	{
		float iradius = 1.0f / sqrtf( SubFloat( radius, 0 ) );
		result = ReplicateX4( iradius );
		result = MulSIMD( result, q );
		return result;
	}
	return q;
}

#else

// SSE + X360 implementation
FORCEINLINE fltx4 QuaternionNormalizeSIMD( const fltx4 &q )
{
	fltx4 radius = Dot4SIMD( q, q );
	fltx4 mask = CmpEqSIMD( radius, Four_Zeros ); // all ones iff radius = 0
	fltx4 result = ReciprocalSqrtSIMD( radius );
	result = MulSIMD( result, q );
	return MaskedAssign( mask, q, result );	// if radius was 0, just return q
}

#endif


//---------------------------------------------------------------------
// 0.0 returns p, 1.0 return q.
//---------------------------------------------------------------------
FORCEINLINE fltx4 QuaternionBlendNoAlignSIMD( const fltx4 &p, const fltx4 &q, float t )
{
	fltx4 sclq = ReplicateX4( t );
	fltx4 sclp = SubSIMD( Four_Ones, sclq );
	fltx4 result = MulSIMD( sclp, p );
	result = MaddSIMD( sclq, q, result );
	return QuaternionNormalizeSIMD( result );
}


//---------------------------------------------------------------------
// Blend Quaternions
//---------------------------------------------------------------------
FORCEINLINE fltx4 QuaternionBlendSIMD( const fltx4 &p, const fltx4 &q, float t )
{
	// decide if one of the quaternions is backwards
	fltx4 q2 = QuaternionAlignSIMD( p, q );
	fltx4 result = QuaternionBlendNoAlignSIMD( p, q2, t );
	return result;
}


//---------------------------------------------------------------------
// Multiply Quaternions
//---------------------------------------------------------------------

// SSE and STDC
FORCEINLINE fltx4 QuaternionMultSIMD( const fltx4 &p, const fltx4 &q )
{
	// decide if one of the quaternions is backwards
	fltx4 result;
	fltx4 q2 = QuaternionAlignSIMD( p, q );
	SubFloat( result, 0 ) =  SubFloat( p, 0 ) * SubFloat( q2, 3 ) + SubFloat( p, 1 ) * SubFloat( q2, 2 ) - SubFloat( p, 2 ) * SubFloat( q2, 1 ) + SubFloat( p, 3 ) * SubFloat( q2, 0 );
	SubFloat( result, 1 ) = -SubFloat( p, 0 ) * SubFloat( q2, 2 ) + SubFloat( p, 1 ) * SubFloat( q2, 3 ) + SubFloat( p, 2 ) * SubFloat( q2, 0 ) + SubFloat( p, 3 ) * SubFloat( q2, 1 );
	SubFloat( result, 2 ) =  SubFloat( p, 0 ) * SubFloat( q2, 1 ) - SubFloat( p, 1 ) * SubFloat( q2, 0 ) + SubFloat( p, 2 ) * SubFloat( q2, 3 ) + SubFloat( p, 3 ) * SubFloat( q2, 2 );
	SubFloat( result, 3 ) = -SubFloat( p, 0 ) * SubFloat( q2, 0 ) - SubFloat( p, 1 ) * SubFloat( q2, 1 ) - SubFloat( p, 2 ) * SubFloat( q2, 2 ) + SubFloat( p, 3 ) * SubFloat( q2, 3 );
	return result;
}

//---------------------------------------------------------------------
// Quaternion scale
//---------------------------------------------------------------------

// SSE and STDC
FORCEINLINE fltx4 QuaternionScaleSIMD( const fltx4 &p, float t )
{
	// FIXME: nick, this isn't overly sensitive to accuracy, and it may be faster to 
	// use the cos part (w) of the quaternion (sin(omega)*N,cos(omega)) to figure the new scale.
	float sinom = sqrtf( SubFloat( p, 0 ) * SubFloat( p, 0 ) + SubFloat( p, 1 ) * SubFloat( p, 1 ) + SubFloat( p, 2 ) * SubFloat( p, 2 ) );
	sinom = min( sinom, 1.f );

	float sinsom = sinf( asinf( sinom ) * t );

	t = sinsom / (sinom + FLT_EPSILON);

	fltx4 q;
	SubFloat( q, 0 ) = t * SubFloat( p, 0 );
	SubFloat( q, 1 ) = t * SubFloat( p, 1 );
	SubFloat( q, 2 ) = t * SubFloat( p, 2 );

	// rescale rotation
	float r = 1.0f - sinsom * sinsom;

	// Assert( r >= 0 );
	if (r < 0.0f) 
		r = 0.0f;
	r = sqrtf( r );

	// keep sign of rotation
	SubFloat( q, 3 ) = fsel( SubFloat( p, 3 ), r, -r );
	return q;
}


//-----------------------------------------------------------------------------
// Quaternion sphereical linear interpolation
//-----------------------------------------------------------------------------

// SSE and STDC
FORCEINLINE fltx4 QuaternionSlerpNoAlignSIMD( const fltx4 &p, const fltx4 &q, float t )
{
	float omega, cosom, sinom, sclp, sclq;

	fltx4 result;

	// 0.0 returns p, 1.0 return q.
	cosom = SubFloat( p, 0 ) * SubFloat( q, 0 ) + SubFloat( p, 1 ) * SubFloat( q, 1 ) + 
		SubFloat( p, 2 ) * SubFloat( q, 2 ) + SubFloat( p, 3 ) * SubFloat( q, 3 );

	if ( (1.0f + cosom ) > 0.000001f ) 
	{
		if ( (1.0f - cosom ) > 0.000001f ) 
		{
			omega = acosf( cosom );
			sinom = sinf( omega );
			sclp = sinf( (1.0f - t)*omega) / sinom;
			sclq = sinf( t*omega ) / sinom;
		}
		else 
		{
			// TODO: add short circuit for cosom == 1.0f?
			sclp = 1.0f - t;
			sclq = t;
		}

		result = DirectX::XMVectorAdd( DirectX::XMVectorScale( p, sclp ), DirectX::XMVectorScale( q, sclq ) );
	}
	else 
	{
		SubFloat( result, 0 ) = -SubFloat( q, 1 );
		SubFloat( result, 1 ) =  SubFloat( q, 0 );
		SubFloat( result, 2 ) = -SubFloat( q, 3 );

		sclp = sinf( (1.0f - t) * (0.5f * M_PI_F));
		sclq = sinf( t * (0.5f * M_PI_F));

		result = DirectX::XMVectorAdd( DirectX::XMVectorScale( p, sclp ), DirectX::XMVectorScale( result, sclq ) );

		SubFloat( result, 3 ) = SubFloat( q, 2 );
	}

	return result;
}


FORCEINLINE fltx4 QuaternionSlerpSIMD( const fltx4 &p, const fltx4 &q, float t )
{
	fltx4 q2 = QuaternionAlignSIMD( p, q );
	fltx4 result = QuaternionSlerpNoAlignSIMD( p, q2, t );
	return result;
}


#endif // ALLOW_SIMD_QUATERNION_MATH

#endif // SSEQUATMATH_H

