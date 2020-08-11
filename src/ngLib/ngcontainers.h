#pragma once

#include "ngLib/types.h"

namespace ng {
template < class T, size_t N = 16 > struct ObjectPool {
	ObjectPool() {
		pool.resize( N );
		for ( u32 i = 0; i < N; i++ ) {
			pool[ i ] = new T();
		}
	}

	~ObjectPool() {
		for ( auto obj : pool ) {
			if ( obj != nullptr ) {
				delete obj;
			}
		}
	}

	std::vector< T * > pool;

	T * Pop() {
		if ( pool.size() == 0 ) {
			return new T();
		}
		T * obj = pool.back();
		obj = new ( obj ) T();
		pool.pop_back();
		return obj;
	}

	void Push( T * obj ) { pool.push_back( obj ); }
};
}; // namespace ng