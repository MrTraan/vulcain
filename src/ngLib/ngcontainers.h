#pragma once

#include "ngLib/types.h"
#include "nglib.h"
#include <bitset>

namespace ng {
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
	DynamicArray( u32 initialCapacity ) { Reserve( initialCapacity ); }
	DynamicArray( u32 initialCapacity, const T & defaultValue ) {
		Reserve( initialCapacity );
		count = initialCapacity;
		for ( u32 i = 0; i < initialCapacity; i++ ) {
			data[ i ] = defaultValue;
		}
	}
	DynamicArray( const DynamicArray< T > & src ) { *this = src; }

	DynamicArray< T > & operator=( const DynamicArray< T > & rhs ) {
		if ( rhs.capacity ) {
			Reserve( rhs.capacity );
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
			Reserve( capacity * 2 );
		}
	}

	// Resize never reduce the compact, use "shrink" for that
	bool Reserve( u32 newCapacity ) {
		if ( newCapacity <= capacity ) {
			return false;
		}
		// Round newCapacity to the smallest power of 2 greater than or equal
		// https://www.geeksforgeeks.org/smallest-power-of-2-greater-than-or-equal-to-n/
		newCapacity = ( u32 )pow( 2ul, ( u32 )ceil( log2( newCapacity ) ) );
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

	int64 FindIndexByValue( const T & value ) {
		for ( int64 i = 0; i < count; i++ ) {
			if ( data[ i ] == value ) {
				return i;
			}
		}
		return -1;
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

	T PopBack() {
		ng_assert( count > 0 );
		T last = Last();
		count--;
		return last;
	}

	void Append( const DynamicArray< T > & v ) {
		Reserve( count + v.count );
		memcpy( data + count, v.data, sizeof( T ) * v.count );
		count += v.count;
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

template < typename T > void ReverseArrayInplace( DynamicArray< T > & v ) {
	if ( v.Size() == 0 ) {
		return;
	}
	u32 i = 0;
	u32 j = v.Size() - 1;
	while ( i < j ) {
		T tmp = v[ i ];
		v[ i ] = v[ j ];
		v[ j ] = tmp;
		i++;
		j--;
	}
}

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

constexpr u64 objectPoolBucketSize = 64;

struct Bitfield64 {
	u64 word = 0;

	inline bool AllBitsAreSet() const { return word == ULLONG_MAX; }
	inline void Set( u32 index ) { word |= 1ULL << index; }
	inline void Reset( u32 index ) { word &= ~( 1ULL << index ); }
	inline bool Test( u32 index ) { return ( word >> index ) & 1ULL; }
	inline void Clear() { word = 0; }
};

template < class T > struct ObjectPool {
	ObjectPool() = default;
	ObjectPool( const ObjectPool & ) = delete;             // non construction-copyable
	ObjectPool & operator=( const ObjectPool & ) = delete; // non copyable

	struct Bucket {
		T          content[ objectPoolBucketSize ];
		Bitfield64 indicesDistributed;
		u32        nextFreeIndex = 0;

		void FindNextFreeIndex() {
			if ( indicesDistributed.AllBitsAreSet() == false ) {
				for ( u32 i = nextFreeIndex + 1; i < 64; i++ ) {
					if ( indicesDistributed.Test( i ) == false ) {
						nextFreeIndex = i;
						return;
					}
				}
				for ( u32 i = 0; i < nextFreeIndex; i++ ) {
					if ( indicesDistributed.Test( i ) == false ) {
						nextFreeIndex = i;
						return;
					}
				}
				ng_assert( false );
			}
		}
	};

	ng::DynamicArray< Bucket * > buckets;

	~ObjectPool() {
		for ( Bucket * bucket : buckets ) {
			delete bucket;
		}
	}

	T * Pop() {
		for ( Bucket * bucket : buckets ) {
			if ( bucket->indicesDistributed.AllBitsAreSet() == false ) {
				u32 i = bucket->nextFreeIndex;
				bucket->indicesDistributed.Set( i );
				bucket->FindNextFreeIndex();
				return bucket->content + i;
			}
		}
		Bucket * newBucket = new Bucket();
		buckets.PushBack( newBucket );
		newBucket->indicesDistributed.Set( 0 );
		newBucket->nextFreeIndex = 1;
		return newBucket->content;
	}

	void Push( T * obj ) {
		for ( Bucket * bucket : buckets ) {
			int64 offset = obj - bucket->content;
			if ( offset >= 0 && offset < objectPoolBucketSize ) {
				if ( bucket->indicesDistributed.Test( ( u32 )offset ) == true ) {
					// default construct the object given back so it's clean for the next user
					new ( bucket->content + offset ) T();
					bucket->indicesDistributed.Reset( ( u32 )offset );
					bucket->nextFreeIndex = ( u32 )offset;
				} else {
					ng_assert_msg( false, "An object was \"double freed\", ie pushed twice to an object pool\n" );
				}
				return;
			}
		}
		ng_assert_msg(
		    false, "An item was pushed to an object pool but it wasn't created by this pool. This is FOR-BI-DDEN\n" );
	}

	void Clear() {
		for ( Bucket * bucket : buckets ) {
			delete bucket;
		}
		buckets.Clear();
	}
};

template < typename T > struct LinkedList {
	LinkedList() = default;
	LinkedList( const LinkedList & rhs ) { *this = rhs; }

	LinkedList & operator=( const LinkedList & rhs ) {
		if ( rhs.head == nullptr ) {
			return *this;
		}
		head = nodePool.Pop();
		head->data = rhs.head->data;
		Node * thisCursor = head;
		Node * rhsCursor = rhs.head->next;
		while ( rhsCursor != nullptr ) {
			thisCursor->next = nodePool.Pop();
			thisCursor->next->data = rhsCursor->data;
			thisCursor = thisCursor->next;
			rhsCursor = rhsCursor->next;
		}
		return *this;
	}

	struct Node {
		T      data{};
		Node * next = nullptr;
		Node * previous = nullptr;
	};

	Node *             head = nullptr;
	ObjectPool< Node > nodePool;

	T & Alloc() {
		Node * newNode = nodePool.Pop();
		if ( head != nullptr ) {
			head->previous = newNode;
			newNode->next = head;
		}
		head = newNode;
		return head->data;
	}
	
	T & PushFront( const T & elem ) {
		Node * newNode = nodePool.Pop();
		newNode->data = elem;
		if ( head != nullptr ) {
			head->previous = newNode;
			newNode->next = head;
		}
		head = newNode;
		return head->data;
	}
	

	void PopFront() {
		if ( head != nullptr ) {
			head = head->next;
			if ( head != nullptr ) {
				head->previous = nullptr;
			}
		}
	}

	void DeleteNode( Node * node ) {
		if ( node == head ) {
			head = node->next;
		}
		if ( node->previous != nullptr ) {
			node->previous->next = node->next;
		}
		if ( node->next != nullptr ) {
			node->next->previous = node->previous;
		}
		nodePool.Push( node );
	}

	T & Front() {
		ng_assert( head != nullptr );
		return head->data;
	}

	Node * GetNodeWithOffset( u64 offset ) {
		ng_assert( offset < size );
		Node * cursor = head;
		for ( u64 i = 0; i < offset; i++ ) {
			cursor = cursor->next;
		}
		return cursor;
	}

	T & operator[]( u64 index ) {
		Node * cursor = head;
		for ( u64 i = 0; i < index; i++ ) {
			cursor = cursor->next;
		}
		return cursor->data;
	}

	const T & operator[]( u64 index ) const {
		ng_assert( index < size );
		const Node * cursor = head;
		for ( u64 i = 0; i < index; i++ ) {
			cursor = cursor->next;
		}
		return cursor->data;
	}

	void Clear() {
		head = nullptr;
		nodePool.Clear();
	}

	bool Empty() const { return head == nullptr; }

	struct Iterator {
		Iterator( Node * node ) : node( node ) {}
		Iterator operator++( int ignored ) {
			( void )ignored;
			// postfix
			Iterator copy( node );
			node = node->next;
			return copy;
		}
		Iterator & operator++() {
			// prefix
			node = node->next;
			return *this;
		}

		bool      operator!=( const Iterator & other ) const { return node != other.node; }
		T &       operator*() { return node->data; }
		const T & operator*() const { return node->data; }

		Node * node;
	};

	Iterator DeleteNode( Iterator & it ) {
		if ( it.node == nullptr ) {
			return Iterator( nullptr );
		}
		Iterator next( it.node->next );
		DeleteNode( it.node );
		return next;
	}

	// iterators
	auto begin() { return Iterator( head ); }
	auto begin() const { return Iterator( head ); }
	auto end() { return Iterator( nullptr ); }
	auto end() const { return Iterator( nullptr ); }
};

enum class SortOrder {
	ASCENDING,
	DESCENDING,
};

template < typename T > void LinkedListInsertSorted( LinkedList< T > & list, const T & elem, SortOrder order ) {
	if ( list.head == nullptr ) {
		list.PushFront( elem );
		return;
	}

	if ( ( order == SortOrder::ASCENDING && elem < list.head->data ) ||
	     ( order == SortOrder::DESCENDING && elem > list.head->data ) ) {
		list.PushFront( elem );
		return;
	}

	LinkedList< T >::Node * cursor = list.head;
	while ( cursor->next != nullptr ) {
		if ( ( order == SortOrder::ASCENDING && elem < cursor->next->data ) ||
		     ( order == SortOrder::DESCENDING && elem > cursor->next->data ) ) {
			break;
		}
		cursor = cursor->next;
	}
	LinkedList< T >::Node * newNode = list.nodePool.Pop();
	newNode->data = elem;
	newNode->next = cursor->next;
	newNode->previous = cursor;
	if ( newNode->next != nullptr ) {
		newNode->next->previous = newNode;
	}
	cursor->next = newNode;
}

template < typename T1, typename T2 > struct Tuple {
	Tuple() = default;
	Tuple( const T1 & a, const T2 & b ) : t1( a ), t2( b ) {}

	T1 t1{};
	T2 t2{};

	T1 &       First() { return t1; }
	T2 &       Second() { return t2; }
	const T1 & First() const { return t1; }
	const T2 & Second() const { return t2; }
};

}; // namespace ng