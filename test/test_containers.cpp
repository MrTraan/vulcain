#include "ngLib/ngcontainers.h"
#include <catch.hpp>

TEST_CASE( "Linked list", "[linked lists]" ) {

	SECTION("list is copyable") {
		ng::LinkedList <int> list;
		list.PushFront( 5 );
		list.PushFront( 4 );
		list.PushFront( 3 );

		{
			ng::LinkedList<int> subList = list;
			subList.PushFront(1);
			REQUIRE(subList.head->data == 1);
			REQUIRE(subList.head->next->data == 3);
		}
		REQUIRE(list.head->data == 3);
	}
	
	SECTION("list is copyable even when empty") {
		ng::LinkedList <int> list;
		ng::LinkedList<int> subList = list;
		subList.PushFront(1);
		list.PushFront(2);
		REQUIRE(list.head->data == 2);
		REQUIRE(subList.head->data == 1);
	}

	SECTION( "can be iterated over" ) {
		ng::LinkedList< int > list;
		list.PushFront( 5 );
		list.PushFront( 4 );
		list.PushFront( 3 );

		int expectedValues[ 3 ] = { 3, 4, 5 };
		int index = 0;
		for ( int & elem : list ) {
			REQUIRE( elem == expectedValues[ index++ ] );
		}
		REQUIRE( index == 3 );
	}
	
	SECTION( "handle sorted insert" ) {
		ng::LinkedList< int > list;
		list.PushFront( 10 );
		list.PushFront( 5 );
		list.PushFront( 1 );

		LinkedListInsertSorted(list, 7, ng::SortOrder::ASCENDING );

		REQUIRE( list[0] == 1 );
		REQUIRE( list[1] == 5 );
		REQUIRE( list[2] == 7 );
		REQUIRE( list[3] == 10 );
	}
}
