#pragma once

#include "../memory/memory.h"
#include "../algo/kmp.h"

namespace std
{
	class WString
	{
	public:
		/* ----- Constructors ----- */

		// Default constructor
		WString();

		explicit WString(size_t size, WCHAR c);

		// Copy constructor
		WString(const WString&);
		explicit WString(const WCHAR*);

		explicit WString(const UNICODE_STRING&);
		explicit WString(const PUNICODE_STRING&);

		// Move constructor
		explicit WString(WString&&) noexcept;

		// Copy Assingment
		WString& operator=(const WString&);
		WString& operator=(const WCHAR*);
		WString& operator=(const UNICODE_STRING&);
		WString& operator=(const PUNICODE_STRING&);

        // Move assignment
        WString& operator=(WString&&) noexcept;

		friend WString& Move(WString&&) noexcept;

        // Move assignment from UNICODE_STRING reference
        // DANGEROUS: Never implement this function, it will move the UNICODE_STRING to the WString. If the string buffer is in stack or predefined, the destructor will free an invalid memory address.
        // WString& operator=(const UNICODE_STRING&& u) noexcept;

		// Destructor
		~WString();

		/*----------------------------*/



		/* -------- ITERATORS --------*/

		class iterator
		{
		public:

			iterator(WCHAR* p);
			iterator& operator++();
			iterator& operator--();
			WCHAR& operator*();
			bool operator==(const iterator& b) const;
			bool operator!=(const iterator& b) const;

		private:
			WCHAR* curr_;
		};

		class iterator;

		iterator Begin();

		const iterator Begin() const;

		iterator End();

		const iterator End() const;

		const iterator ConstBegin() const;

		const iterator ConstEnd() const;

		/*----------------------------*/



		/* -------- CAPACITY -------- */

		bool Empty() const;

		// Requests a change in capacity
		// reserve() will never decrase the capacity.
		void Reserve(size_t);

		// Changes the WString's size.
		// If the newsize is smaller, the last elements will be lost.
		// Has a default value param for custom values when resizing.
		void Resize(size_t, WCHAR val = L'\0');

		// Returns the size of the WString (number of elements). 
		size_t Size() const;

		// Returns size of allocated storate capacity
		size_t Capacity() const;

		// Reduces capcity to fit the size
		void ShrinkToFit();

		/*----------------------------*/



		/* -------- MODIFIERS --------*/

		// Removes all elements from the String
		// Capacity is not changed.
		void Clear();

		// Inserts element at the back
		void PushBack(const WCHAR);

		// Removes the last element from the String
		void PopBack();

		// Append a WString to the back.
		void Append(const WString&);

		// Append a null-terminated C-String to the back.
		void Append(const WCHAR*);

		// Append an UNICODE_STRING to the back.
		void Append(const UNICODE_STRING&);

		// Add a WString to the back.
		WString& operator+=(const WString&);

		// Add a null-terminated C-String to the back.
		WString& operator+=(const WCHAR*);

		// Add an UNICODE_STRING to the back.
		WString& operator+=(const UNICODE_STRING&);

		// Combination of 2 WStrings
		WString operator+(const WString&);

		// Combination of a WString and a null-terminated C-String
		WString operator+(const WCHAR*);

		// Combination of a WString and a null-terminated C-String
		WString operator+(const UNICODE_STRING&);

		WString& ConverToUpcase();

		WString& ConverToDowncase();

		WString GetUpcase();

		WString GetDowncase();

		/*----------------------------*/



		/* ----- ELEMENT ACCESS ----- */

		// Access elements with bounds checking.
		WCHAR& At(size_t n);

		// Access elements with bounds checking.
		const WCHAR& At(size_t n) const;

		// Access elements with bounds checking
		WCHAR& operator[](size_t i);

		// Access elements with bounds checking
		const WCHAR& operator[](size_t i) const;

		// Returns a reference to the first element
		WCHAR& Front();

		// Returns a reference to the first element
		const WCHAR& Front() const;

		// Returns a reference to the last element
		WCHAR& Back();

		// Returns a reference to the last element
		const WCHAR& Back() const;

		// Returns a pointer to the array used by WString
		WCHAR* Data();

		// Returns a pointer to the array used by WString
		const WCHAR* Data() const;

		// Returns an UNICODE_STRING used by this WString if valid
		const UNICODE_STRING UniStr() const;

		/*----------------------------*/



		/* -------- COMPARISON -------*/

		// Check if this WString is a prefix of a WString
		bool IsPrefixOf(const WString&) const;

		// Check if this WString is a prefix of a WString (case insensitive)
		bool IsCiPrefixOf(const WString&) const;

		// Check if this WString has a WString as its prefix
		bool HasPrefix(const WString&) const;

		// Check if this WString has a WString as its prefix (case insensitive)
		bool HasCiPrefix(const WString&) const;

		// Check if this WString is a suffix of a WString
		bool IsSuffixOf(const WString&) const;

		// Check if this WString is a suffix of a WString (case insensitive)
		bool IsCiSuffixOf(const WString&) const;

		// Check if this a WString has a WString as its suffix
		bool HasSuffix(const WString&) const;

		// Check if this a WString has a WString as its suffix (case insensitive)
		bool HasCiSuffix(const WString&) const;

		// Get the first position of a WString in this WString
		size_t FindFirstOf(const WString&, size_t begin_pos = 0) const;

		// Get the first position of a null-terminated C-String in this WString
		size_t FindFirstOf(const WCHAR*, size_t begin_pos = 0) const;

		// Get the first position of a UNICODE_STRING in this WString
		size_t FindFirstOf(const UNICODE_STRING&, size_t begin_pos = 0) const;

		// Check if a WString is in this WString
		bool Contain(const WString&) const;

		// Check if a null-terminated C-String is in this WString
		bool Contain(const WCHAR*) const;

		// Check if a UNICODE_STRING is in this WString
		bool Contain(const UNICODE_STRING&) const;

		bool operator==(const WString&) const;
		bool operator==(const WCHAR*) const;
		bool operator==(const UNICODE_STRING&) const;

		// Comparing by case insensitive
		bool EqualCi(const WString&) const;
		// Comparing by case insensitive
		bool EqualCi(const WCHAR*) const;
		// Comparing by case insensitive
		bool EqualCi(const UNICODE_STRING&) const;

		bool operator!=(const WString&) const;
		bool operator!=(const WCHAR*) const;
		bool operator!=(const UNICODE_STRING&) const;

		// Overloading the greater operator
		bool operator>(const WString&) const;
		bool operator>(const WCHAR*) const;
		bool operator>(const UNICODE_STRING&) const;

		// Overloading the smaller operator
		bool operator<(const WString&) const;
		bool operator<(const WCHAR*) const;
		bool operator<(const UNICODE_STRING&) const;

		/*----------------------------*/

	protected:
		WCHAR* Allocate(size_t);
		void Deallocate();
	private:
		size_t size_ = 0;
		size_t capacity_ = 0;
		WCHAR* elements_ = nullptr;
	};

	WString ToWString(short);
    WString ToWString(unsigned short);
	WString ToWString(int);
    WString ToWString(unsigned int);
	WString ToWString(long);
	WString ToWString(unsigned long);
    WString ToWString(long long);
    WString ToWString(unsigned long long);

	

}