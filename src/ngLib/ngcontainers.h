#pragma once

#include "ngLib/types.h"

namespace ng {
template < class T, size_t N = 64 > struct ObjectPool {
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

template < typename T > struct DynamicArray {
	static constexpr u32 initialAllocSize = 32;

	T * data = nullptr;

  private:
	u32 capacity = 0;
	u32 count = 0;

  public:
	u32  Size() const { return count; }
	u32  Capacity() const { return capacity; }
	bool Empty() const { return count == 0; }

	DynamicArray() = default;
	DynamicArray( u32 initialCapacity ) { Resize( initialCapacity ); }
	DynamicArray( const DynamicArray< T > & src ) { *this = src; }

	DynamicArray< T > & operator=( const DynamicArray< T > & rhs ) {
		if ( rhs.capacity ) {
			Resize( rhs.capacity );
			for ( u32 i = 0; i < rhs.capacity; i++ ) {
				data[ i ] = rhs.data[ i ];
			}
			count = rhs.count;
		} else {
			capacity = 0;
			count = 0;
			delete[] data;
			data = nullptr;
		}
		return *this;
	}

	~DynamicArray() { delete[] data; }

	void Grow() {
		if ( capacity == 0 ) {
			data = new T[ initialAllocSize ];
			capacity = initialAllocSize;
		} else {
			Resize( capacity * 2 );
		}
	}

	// Resize never reduce the compact, use "shrink" for that
	bool Resize( u32 newCapacity ) {
		if ( newCapacity <= capacity ) {
			return false;
		}
		T * temp = new T[ newCapacity ];

		for ( u32 i = 0; i < count; i++ ) {
			temp[ i ] = data[ i ];
		}

		delete[] data;
		data = temp;

		capacity = newCapacity;
		return true;
	}

	void Shrink() {
		capacity = size;
		T * temp = new T[ capacity ];

		for ( u32 i = 0; i < count; i++ ) {
			temp[ i ] = data[ i ];
		}

		delete[] data;
		data = temp;
	}

	bool DeleteIndexFast( u32 index ) {
		if ( index >= count ) {
			return false;
		}
		if ( index == count - 1 ) {
			count--;
			return true;
		}
		data[ index ] = data[ --count ];
		return true;
	}

	u32 DeleteValueFast( const T & value ) {
		u32 numDeletions = 0;
		for ( int64 i = count - 1; i >= 0; i-- ) {
			if ( data[ i ] == value ) {
				DeleteIndexFast( ( u32 )i );
				numDeletions++;
			}
		}
		return numDeletions;
	}

	void Clear() { count = 0; }

	T & AllocateOne() {
		if ( count == capacity ) {
			Grow();
		}
		return *( data + count++ );
	}

	T & PushBack( const T & newElem ) {
		if ( count == capacity ) {
			Grow();
		}
		data[ count ] = newElem;
		return *( data + count++ );
	}

	T & At( u32 index ) {
		ng_assert( index < count );
		return *( data + index );
	}

	const T & At( u32 index ) const {
		ng_assert( index < count );
		return *( data + index );
	}

	T &       operator[]( int index ) { return At( index ); }
	const T & operator[]( int index ) const { return At( index ); }

	T &       Last() { return At( count - 1 ); }
	const T & Last() const { return At( count - 1 ); }

	T *       begin() { return data == nullptr || count == 0 ? nullptr : data; }
	T *       end() { return data == nullptr || count == 0 ? nullptr : data + count; }
	const T * begin() const { return data == nullptr || count == 0 ? nullptr : data; }
	const T * end() const { return data == nullptr || count == 0 ? nullptr : data + count; }
};

template < typename T, u32 N > struct StaticArray {
  private:
	T   data[ N ] = {};
	u32 capacity = N;
	u32 count = 0;

  public:
	u32 Size() const { return count; }
	u32 Capacity() const { return capacity; }

	T & PushBack( const T & newElem ) {
		ng_assert( count < capacity );
		data[ count ] = newElem;
		return *( data + count++ );
	}

	T & At( u32 index ) {
		ng_assert( index < count );
		return *( data + index );
	}

	const T & At( u32 index ) const {
		ng_assert( index < count );
		return *( data + index );
	}

	bool DeleteIndexFast( u32 index ) {
		if ( index >= count ) {
			return false;
		}
		if ( index == count - 1 ) {
			count--;
			return true;
		}
		data[ index ] = data[ --count ];
		return true;
	}

	T &       operator[]( int index ) { return At( index ); }
	const T & operator[]( int index ) const { return At( index ); }

	T *       begin() { return data == nullptr || count == 0 ? nullptr : data; }
	T *       end() { return data == nullptr || count == 0 ? nullptr : data + count; }
	const T * begin() const { return data == nullptr || count == 0 ? nullptr : data; }
	const T * end() const { return data == nullptr || count == 0 ? nullptr : data + count; }
};

}; // namespace ng