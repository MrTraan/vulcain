#pragma once

#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358f
#endif

#define TO_RADIAN( x ) ( ( x ) * ( float )M_PI / 180.0f )

struct Vec2 {
	Vec2() : x( 0.0f ), y( 0.0f ) {}
	Vec2( float _x, float _y ) : x( _x ), y( _y ) {}
	Vec2( const Vec2 & other ) { *this = other; }

	Vec2 & operator=( const Vec2 & rhs ) {
		x = rhs.x;
		y = rhs.y;
		return *this;
	}

	float   x;
	float   y;
	float * Data() { return &x; }
	float & operator[]( int index ) { return *( &x + index ); }
	float   operator[]( int index ) const { return *( &x + index ); }

	Vec2 operator+( const Vec2 & o ) const { return Vec2( x + o.x, y + o.y ); }
	Vec2 operator-( const Vec2 & o ) const { return Vec2( x - o.x, y - o.y ); }
	Vec2 operator*( const Vec2 & o ) const { return Vec2( x * o.x, y * o.y ); }
	Vec2 operator*( float f ) const { return Vec2( x * f, y * f ); }
	Vec2 operator/( float f ) const { return Vec2( x / f, y / f ); }

	float Length() const { return sqrtf( x * x + y * y ); }
	float SqLength() const { return x * x + y * y; }

	Vec2 ToRadians() { return Vec2( TO_RADIAN( x ), TO_RADIAN( y ) ); }

	static float Dot( const Vec2 & a, const Vec2 & b ) { return a.x * b.x + a.y * b.y; }

	static Vec2 Normalize( const Vec2 & v ) {
		float k = 1.0f / v.Length();
		return v * k;
	}
};

struct Vec3 {
	Vec3() { x = 0.0f, y = 0.0f, z = 0.0f; }
	constexpr Vec3( float _x, float _y, float _z ) : x( _x ), y( _y ), z( _z ) {}
	Vec3( const Vec2 & v, float _z ) { x = v.x, y = v.y, z = _z; }
	Vec3( const Vec3 & other ) { *this = other; }

	Vec3 & operator=( const Vec3 & rhs ) {
		x = rhs.x;
		y = rhs.y;
		z = rhs.z;
		return *this;
	}

	float   x;
	float   y;
	float   z;
	float * Data() { return &x; }
	float & operator[]( int index ) { return *( &x + index ); }
	float   operator[]( int index ) const { return *( &x + index ); }
	Vec2    ToVec2() const { return Vec2( x, y ); }

	void operator+=( const Vec3 & o ) {
		x += o.x;
		y += o.y;
		z += o.z;
	}
	void operator-=( const Vec3 & o ) {
		x -= o.x;
		y -= o.y;
		z -= o.z;
	}
	Vec3 operator+( const Vec3 & o ) const { return Vec3( x + o.x, y + o.y, z + o.z ); }
	Vec3 operator-( const Vec3 & o ) const { return Vec3( x - o.x, y - o.y, z - o.z ); }
	Vec3 operator*( const Vec3 & o ) const { return Vec3( x * o.x, y * o.y, z * o.z ); }
	Vec3 operator*( float f ) const { return Vec3( x * f, y * f, z * f ); }
	Vec3 operator/( float f ) const { return Vec3( x / f, y / f, z / f ); }

	bool operator==( const Vec3 & o ) const { return x == o.x && y == o.y && z == o.z; }

	float Length() const { return sqrtf( x * x + y * y + z * z ); }
	float SqLength() const { return x * x + y * y + z * z; }

	Vec3 ToRadians() { return Vec3( TO_RADIAN( x ), TO_RADIAN( y ), TO_RADIAN( z ) ); }

	static float Dot( const Vec3 & a, const Vec3 & b ) { return a.x * b.x + a.y * b.y + a.z * b.z; }

	static Vec3 Cross( const Vec3 & a, const Vec3 & b ) {
		return Vec3( a.y * b.z - a.z * b.y, -( a.x * b.z - a.z * b.x ), a.x * b.y - a.y * b.x );
	}

	static Vec3 Normalize( const Vec3 & v ) {
		float k = 1.0f / v.Length();
		return v * k;
	}
};

struct Vec4 {
	Vec4() { x = 0.0f, y = 0.0f, z = 0.0f; }
	Vec4( float _x, float _y, float _z, float _w ) {
		x = _x, y = _y, z = _z;
		w = _w;
	}
	Vec4( const Vec4 & other ) { *this = other; }

	Vec4 & operator=( const Vec4 & rhs ) {
		x = rhs.x;
		y = rhs.y;
		z = rhs.z;
		w = rhs.w;
		return *this;
	}

	float   x;
	float   y;
	float   z;
	float   w;
	float * Data() { return &x; }
	float & operator[]( int index ) { return *( &x + index ); }
	float   operator[]( int index ) const { return *( &x + index ); }

	void operator+=( const Vec4 & o ) {
		x += o.x;
		y += o.y;
		z += o.z;
		w += o.w;
	}
	Vec4 operator+( const Vec4 & o ) const { return Vec4( x + o.x, y + o.y, z + o.z, w + o.w ); }
	Vec4 operator-( const Vec4 & o ) const { return Vec4( x - o.x, y - o.y, z - o.z, w + o.w ); }
	Vec4 operator*( const Vec4 & o ) const { return Vec4( x * o.x, y * o.y, z * o.z, w + o.w ); }
	Vec4 operator*( float f ) const { return Vec4( x * f, y * f, z * f, w * f ); }
	Vec4 operator/( float f ) const { return Vec4( x / f, y / f, z / f, w / f ); }

	float Length() const { return sqrtf( x * x + y * y + z * z + w * w ); }
	float SqLength() const { return x * x + y * y + z * z + w * w; }

	static float Dot( const Vec4 & a, const Vec4 & b ) { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; }

	// static Vec4 Cross( const Vec4 & a, const Vec4 & b ) {
	//	return Vec4( a.y * b.z - a.z * b.y, -( a.x * b.z - a.z * b.x ), a.x * b.y - a.y * b.x );
	//}

	static Vec4 Normalize( const Vec4 & v ) {
		float k = 1.0f / v.Length();
		return v * k;
	}
};

static_assert( sizeof( Vec2 ) == sizeof( float ) * 2 );
static_assert( sizeof( Vec3 ) == sizeof( float ) * 3 );
static_assert( sizeof( Vec4 ) == sizeof( float ) * 4 );

constexpr float uniformMatrixValue[ 16 ] = {
    1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
};

struct Matrix {
	float v[ 16 ];

	Matrix() { ToUniform(); }
	Matrix( float * floatArray ) { memcpy( v, floatArray, sizeof( float ) * 16 ); }

	Matrix & operator=( const Matrix & rhs ) {
		for ( int i = 0; i < 16; i++ ) {
			v[ i ] = rhs.v[ i ];
		}
		return *this;
	}

	float *       Data() { return v; }
	const float * Data() const { return v; }

	float & At( int x, int y ) { return v[ x + y * 4 ]; }
	float   At( int x, int y ) const { return v[ x + y * 4 ]; }
	float & operator[]( int index ) { return v[ index ]; }
	float   operator[]( int index ) const { return v[ index ]; }

	void ToUniform() { memcpy( v, uniformMatrixValue, sizeof( uniformMatrixValue ) ); }

	void Zero() {
		for ( int i = 0; i < 16; i++ ) {
			v[ i ] = 0.0f;
		}
	}

	static Matrix Perspective( float fov, float aspect, float _near, float _far );
	static Matrix Orthographic( float _near, float _far, float l, float r, float t, float b );
	static Matrix LookAt( Vec3 position, Vec3 target, Vec3 up );

	void Translate( const Vec3 & translation ) {
		Matrix translateMatrix;
		translateMatrix[ 3 ] = translation[ 0 ];
		translateMatrix[ 7 ] = translation[ 1 ];
		translateMatrix[ 11 ] = translation[ 2 ];
		*this = *this * translateMatrix;
	}

	void Scale( const Vec3 & scale ) {
		Matrix scaleMatrix;
		scaleMatrix[ 0 ] = scale[ 0 ];
		scaleMatrix[ 5 ] = scale[ 1 ];
		scaleMatrix[ 10 ] = scale[ 2 ];
		*this = *this * scaleMatrix;
	}

	void EulerRotation( const Vec3 & rot );

	Matrix Transpose() const;

	Matrix & operator*=( float a );
	Matrix   operator*( float a ) const;
	Vec3     operator*( const Vec3 & v ) const;
	Vec2     operator*( const Vec2 & v ) const;
	Matrix   operator*( const Matrix & m ) const;
	Matrix   operator+( const Matrix & rhs ) const;
};

static_assert( sizeof( Matrix ) == sizeof( float ) * 16 );
