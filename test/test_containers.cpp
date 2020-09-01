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
			REQUIRE(subList.size == 4);
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
		REQUIRE(list.size == 1);
		REQUIRE(subList.size == 1);
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

		REQUIRE( list.size == 4);
		REQUIRE( list[0] == 1 );
		REQUIRE( list[1] == 5 );
		REQUIRE( list[2] == 7 );
		REQUIRE( list[3] == 10 );
	}

	SECTION("can be sorted") {
		ng::LinkedList<int> list;
		list.PushFront( 5 );
		list.PushFront( 7 );
		list.PushFront( 3 );
		list.PushFront( 19 );
		list.PushFront( -1 );
		SortLinkedList(list, ng::SortOrder::ASCENDING);
		REQUIRE( list.size == 5);
		REQUIRE( list[0] == -1 );
		REQUIRE( list[1] == 3 );
		REQUIRE( list[2] == 5 );
		REQUIRE( list[3] == 7 );
		REQUIRE( list[4] == 19 );
		
		SortLinkedList(list, ng::SortOrder::DESCENDING);
		REQUIRE( list.size == 5);
		REQUIRE( list[4] == -1 );
		REQUIRE( list[3] == 3 );
		REQUIRE( list[2] == 5 );
		REQUIRE( list[1] == 7 );
		REQUIRE( list[0] == 19 );
	}
}
