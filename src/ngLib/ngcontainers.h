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

template < typename T > struct DynamicArray {
	static constexpr u32 initialAllocSize = 32;

  private:
	T * data = nullptr;
	u32 capacity = 0;
	u32 count = 0;

  public:
	u32 Size() const { return count; }
	u32 Capacity() const { return capacity; }

	DynamicArray() = default;
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

	bool Resize( u32 newCapacity ) {
		if ( newCapacity == 0 ) {
			newCapacity = initialAllocSize;
		}
		T * temp = new T[ newCapacity ];

		for ( u32 i = 0; i < MIN( capacity, newCapacity ); i++ ) {
			temp[ i ] = data[ i ];
		}

		delete[] data;
		data = temp;

		capacity = newCapacity;
		if ( count > newCapacity ) {
			count = newCapacity;
		}
		return true;
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

	void Clear() {
		delete[] data;
		data = nullptr;
		capacity = 0;
		count = 0;
	}

	T & AllocateOne() {
		if ( count == capacity ) {
			Resize( capacity * 2 );
		}
		return *( data + count++ );
	}

	T & PushBack( const T & newElem ) {
		if ( count == capacity ) {
			Resize( capacity * 2 );
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