#include "wstring.h"

#include <string.h>
#include <ntstrsafe.h>

namespace std
{

	// Constructor: store the pointer to current character
	WString::iterator::iterator(WCHAR* p)
		: curr_(p)
	{
	}

	// Prefix increment: move to the next character
	WString::iterator& WString::iterator::operator++()
	{
		++curr_;
		return *this;
	}

	// Prefix decrement: move to the previous character
	WString::iterator& WString::iterator::operator--()
	{
		--curr_;
		return *this;
	}

	// Dereference: return reference to the character at current position
	WCHAR& WString::iterator::operator*()
	{
		return *curr_;
	}

	// Equality comparison
	bool WString::iterator::operator==(const iterator& b) const
	{
		return curr_ == b.curr_;
	}

	// Inequality comparison
	bool WString::iterator::operator!=(const iterator& b) const
	{
		return curr_ != b.curr_;
	}

	// ============================= Constructors =============================

	// Default constructor
	WString::WString()
		: size_(0), capacity_(0), elements_(nullptr)
	{}

	// Construct with size, fill with c
	WString::WString(size_t size, WCHAR c)
		: size_(0), capacity_(0), elements_(nullptr)
	{
		if (size == 0) {
			return;
		}
		size_ = size;
		capacity_ = size;
		elements_ = Allocate(size_);
		for (size_t i = 0; i < size_; ++i)
			elements_[i] = c;
		elements_[size_] = L'\0';
	}

	// Copy constructor
	WString::WString(const WString& other)
		: size_(0), capacity_(0), elements_(nullptr)
	{
		// If other is corrupt (elements_ == nullptr but size_ > 0), treat as empty.
		if (other.size_ == 0 || other.elements_ == nullptr) {
			return;
		}
		size_ = other.size_;
		capacity_ = other.size_;
		elements_ = Allocate(capacity_);
		memcpy(elements_, other.elements_, (size_ + 1) * sizeof(WCHAR));
	}

	// Construct from C-string
	WString::WString(const WCHAR* ws)
		: size_(0), capacity_(0), elements_(nullptr)
	{
		if (ws == nullptr) {
			return;
		}
		size_t len = wcslen(ws);
		if (len == 0) {
			return;
		}
		size_ = len;
		capacity_ = len;
		elements_ = Allocate(capacity_);
		memcpy(elements_, ws, size_ * sizeof(WCHAR));
	}

	// Construct from UNICODE_STRING
	WString::WString(const UNICODE_STRING& u)
		: size_(0), capacity_(0), elements_(nullptr)
	{
		if (u.Buffer == nullptr || u.Length == 0) {
			return;
		}
		auto len = u.Length / sizeof(WCHAR);
		if (len == 0) {
			return;
		}

		size_ = len;
		capacity_ = len;
		elements_ = Allocate(capacity_);
		memcpy(elements_, u.Buffer, size_ * sizeof(WCHAR));
	}

	// Construct from PUNICODE_STRING
	WString::WString(const PUNICODE_STRING& pu)
		: size_(0), capacity_(0), elements_(nullptr)
	{
		if (!pu || !pu->Buffer || pu->Length == 0) {
			return; // empty => nullptr
		}

		UNICODE_STRING u = *pu;
		*this = u; // call WString::WString(const UNICODE_STRING& u)
	}

	// Move constructor
	WString::WString(WString&& other) noexcept
		: size_(other.size_), capacity_(other.capacity_), elements_(other.elements_)
	{
		other.elements_ = nullptr;
		other.size_ = other.capacity_ = 0;
	}

	// ============================= Assignment =============================

	// Copy assignment
	WString& WString::operator=(const WString& other)
	{
		if (this == &other) return *this;
		if (other.Empty() == true) {
			Clear();
			return *this;
		}

		if (capacity_ < other.size_ || elements_ == nullptr) {
			Reset();
			elements_ = Allocate(other.size_);
			capacity_ = other.size_;
		}

		size_ = other.size_;
		memcpy(elements_, other.elements_, (size_ + 1) * sizeof(WCHAR));

		return *this;
	}

	// Copy assignment from C-style wide string
	WString& WString::operator=(const WCHAR* s)
	{
		if (s == nullptr) {
			Clear();
			return *this;
		}

		if (this->elements_ == s) {
			return *this;
		}

		size_t len = wcslen(s);
		if (len == 0) {
			Clear();
			return *this;
		}

		Repair();

		if (capacity_ < len || elements_ == nullptr) {
			Reset();
			elements_ = Allocate(len);
			capacity_ = len;
		}

		size_ = len;
		memmove(elements_, s, (size_ + 1) * sizeof(WCHAR));

		return *this;
	}

	// Copy assignment from UNICODE_STRING reference
	WString& WString::operator=(const UNICODE_STRING& u)
	{
		if (u.Buffer == nullptr || u.Length < sizeof(WCHAR) || u.MaximumLength < sizeof(WCHAR)) {
			Clear();
			return *this;
		}

		size_t len = u.Length / sizeof(WCHAR);

		if (capacity_ < len || elements_ == nullptr) {
			Reset();
			elements_ = Allocate(len);
			capacity_ = len;
		}
		size_ = len;
		// copy len characters and null-terminate
		memcpy(elements_, u.Buffer, len * sizeof(WCHAR));
		elements_[size_] = L'\0';

		return *this;
	}

	// Copy assignment from UNICODE_STRING pointer
	WString& WString::operator=(const PUNICODE_STRING& pu)
	{
		if (!pu) {
			Clear();
			return *this;
		}
		return (*this = *pu);
	}

	WString& WString::operator=(WString&& other) noexcept
	{
        if (this != &other) {
            Reset();
            size_ = other.size_;
            capacity_ = other.capacity_;
            elements_ = other.elements_;
            other.elements_ = nullptr;
            other.size_ = other.capacity_ = 0;
        }
        return *this;
	}

	// Destructor
	WString::~WString()
	{
		Reset();
	}

	// Return iterator to first element
	WString::iterator WString::Begin()
	{
		return iterator(elements_);
	}

	// Return const iterator to first element
	const WString::iterator WString::Begin() const
	{
		return iterator(elements_);
	}

	// Return iterator to one past last element
	WString::iterator WString::End()
	{
		return iterator(elements_ ? (elements_ + size_) : nullptr);
	}

	// Return const iterator to one past last element
	const WString::iterator WString::End() const
	{
		return iterator(elements_ ? (elements_ + size_) : nullptr);
	}

	// Return const iterator to first element (alternative naming)
	const WString::iterator WString::ConstBegin() const
	{
		return iterator(elements_);
	}

	// Return const iterator to one past last element (alternative naming)
	const WString::iterator WString::ConstEnd() const
	{
		return iterator(elements_ ? (elements_ + size_) : nullptr);
	}

	bool WString::Empty() const { return size_ == 0 || capacity_ == 0 || elements_ == nullptr; }

	void WString::Reserve(size_t new_cap)
	{
		Repair();
		if (new_cap == 0) {
			return;
		}

		if (elements_ == nullptr) {
			Reset();
			elements_ = Allocate(new_cap);
			size_ = new_cap;
			capacity_ = new_cap;
			return;
		}

		if (new_cap <= capacity_) {
			return;
		}

		auto cur_size = size_;

		WCHAR* new_buf = Allocate(new_cap);
		new_buf[0] = L'\0';
		if (size_ > 0) {
			memcpy(new_buf, elements_, (size_ + 1) * sizeof(WCHAR));
			Reset();
			size_ = cur_size;
		}
		elements_ = new_buf;
		capacity_ = new_cap;
	}

	void WString::Resize(size_t new_size, WCHAR val)
	{
		Repair();

		if (new_size == 0) {
			Clear();
			return;
		}

		if (elements_ == nullptr || capacity_ < new_size) {
			Reserve(new_size);
		}

		if (new_size < size_) {
			size_ = new_size;
			elements_[size_] = L'\0';
		}
		else if (new_size > size_) {
			for (size_t i = size_; i < new_size; ++i)
				elements_[i] = val;
			size_ = new_size;
			elements_[size_] = L'\0';
		}
	}

	size_t WString::Size() const { return size_; }

	size_t WString::Capacity() const { return capacity_; }

	void WString::Clear()
	{
		Repair();
		size_ = 0;
		if (elements_) elements_[0] = L'\0';
	}

	void WString::PushBack(const WCHAR c)
	{
		Repair();
		if (size_ + 1 >= capacity_) {
			auto new_cap = (capacity_ == 0) ? 1 : (capacity_ * 2);
			Reserve(new_cap + 1);
		}
		// Ensure buffer exists
		if (elements_ == nullptr) {
			Reserve(1);
		}
		elements_[size_++] = c;
		elements_[size_] = L'\0';
	}

	void WString::PopBack()
	{
		Repair();
		if (Empty() == true) return;
		elements_[--size_] = L'\0';
	}

	static __forceinline bool IsOverlap(const void* p1, size_t s1, const void* p2, size_t s2) {
		if (p1 == nullptr || p2 == nullptr) {
			return false; 
		}

		uintptr_t a1 = (uintptr_t)p1;
		uintptr_t a2 = (uintptr_t)p2;

		return (a1 < a2 + s2) && (a2 < a1 + s1);
	}

	void WString::Append(const WString& other)
	{
		if (other.Empty() == true) return;
		Repair();
		if (IsOverlap(elements_, size_, other.elements_, other.size_) == true) {
			WString tmp(other);
			Append(tmp);
			return;
		}

		Reserve(size_ + other.size_);
		memcpy(&elements_[size_], other.elements_, other.size_ * sizeof(WCHAR));
		size_ += other.size_;
		elements_[size_] = L'\0';
	}

	void WString::Append(const WCHAR* s)
	{
		if (!s) return;
		auto len = wcslen(s);
		if (len == 0) {
			return;
		}

		Repair();

		if (IsOverlap(elements_, size_, s, len) == true) {
			WString tmp(s);
			Append(tmp);
			return;
		}

		Reserve(size_ + len);
		memcpy(&elements_[size_], s, len * sizeof(WCHAR));
		size_ += len;
		elements_[size_] = L'\0';
	}

	void WString::Append(const UNICODE_STRING& u)
	{
		if (u.Buffer == nullptr || u.Length < sizeof(WCHAR) || u.MaximumLength == 0) {
			return;
		}
		Repair();

		size_t len = u.Length / sizeof(WCHAR);
		if (IsOverlap(elements_, size_, u.Buffer, len) == true) {
			WString tmp(u.Buffer);
			Append(tmp);
			return;
		}
		Reserve((size_ + len) * 3 / 2 + 1);
		memcpy(&elements_[size_], u.Buffer, len * sizeof(WCHAR));
		size_ += len;
		elements_[size_] = L'\0';
	}

	WString& WString::operator+=(const WString& other)
	{ 
		Append(other);
		return *this; 
	}

	WString& WString::operator+=(const WCHAR* s)
	{
		Append(s);
		return *this;
	}

	WString& WString::operator+=(const UNICODE_STRING& u)
	{
		Append(u);
		return *this;
	}

	WString WString::operator+(const WString& other) {
		WString tmp(*this);
		tmp.Append(other);
		return tmp;
	}

	WString WString::operator+(const WCHAR* s) {
		WString tmp(*this);
		tmp.Append(s);
		return tmp;
	}

	WString WString::operator+(const UNICODE_STRING& u) {
		WString tmp(*this);
		tmp.Append(u);
		return tmp;
	}

	WString& WString::ConverToUpcase()
	{
		Repair();
		for (size_t i = 0; i < size_; ++i)
			elements_[i] = RtlUpcaseUnicodeChar(elements_[i]);
		return *this;
	}

	WString& WString::ConverToDowncase()
	{
		Repair();
		for (size_t i = 0; i < size_; ++i)
			elements_[i] = RtlDowncaseUnicodeChar(elements_[i]);
		return *this;
	}

	WString WString::GetUpcase() const {
		WString tmp(*this);
		tmp.ConverToUpcase();
		return tmp;
	}

	WString WString::GetDowncase() const {
		WString tmp(*this);
		tmp.ConverToDowncase();
		return tmp;
	}

	WCHAR& WString::At(size_t n) {
		Repair();
		if (n >= size_ || Empty() == true) {
			ExRaiseAccessViolation();
		}
		return elements_[n];
	}

	const WCHAR& WString::At(size_t n) const {
		if (n >= size_ || Empty() == true) {
			return L'\0';
		}
		return elements_[n];
	}

	WCHAR& WString::operator[](size_t n) { return At(n); }
	const WCHAR& WString::operator[](size_t n) const { return At(n); }

	WCHAR& WString::Front() {
		Repair();
		if (Empty() == true) {
			ExRaiseAccessViolation();
		}
		return elements_[0];
	}

	const WCHAR& WString::Front() const {
		if (Empty() == true) {
			return L'\0';
		}
		return elements_[0];
	}

	WCHAR& WString::Back() {
		Repair();
		if (Empty() == true) {
			ExRaiseAccessViolation();
		}
		return elements_[size_ - 1];
	}

	const WCHAR& WString::Back() const {
		if (Empty() == true) {
			return L'\0';
		}
		return elements_[size_ - 1];
	}

	WCHAR* WString::Data() { return elements_; }
	const WCHAR* WString::Data() const { return elements_; }

	const UNICODE_STRING WString::UniStr() const
	{
		UNICODE_STRING uni_str = { 0 };
		if (size_ * sizeof(WCHAR) >= UNICODE_STRING_MAX_BYTES) {
			ExRaiseStatus(STATUS_NAME_TOO_LONG);
		}
		if (Empty() == true) {
			uni_str.Length = 0;
			uni_str.MaximumLength = 0;
			uni_str.Buffer = nullptr;
			return uni_str;
		}
		uni_str.Length = static_cast<USHORT>(size_ * sizeof(WCHAR));
		uni_str.MaximumLength = static_cast<USHORT>((capacity_ + 1) * sizeof(WCHAR));
		uni_str.Buffer = elements_;
		return uni_str;
	}

	bool WString::IsPrefixOf(const WString& other) const {
		if (Empty() == true) {
			return true;
		}
		if (other.Empty() == false) {
			return false;
		}
		return other.size_ >= size_ &&
			wcsncmp(other.elements_, elements_, size_) == 0;
	}

	bool WString::IsCiPrefixOf(const WString& other) const {
		if (Empty() == true) {
			return true;
		}
		if (other.Empty() == false) {
			return false;
		}
		return other.size_ >= size_ &&
			_wcsnicmp(other.elements_, elements_, size_) == 0;
	}

	bool WString::HasPrefix(const WString& prefix) const {
		return prefix.IsPrefixOf(*this);
	}

	bool WString::HasCiPrefix(const WString& prefix) const {
		return prefix.IsCiPrefixOf(*this);
	}

	bool WString::IsSuffixOf(const WString& other) const {
		if (Empty() == true) {
			return true;
		}
		if (other.Empty() == false) {
			return false;
		}
		return other.size_ >= size_ &&
			wcsncmp(&other.elements_[other.size_ - size_], elements_, size_) == 0;
	}

	bool WString::IsCiSuffixOf(const WString& other) const {
		if (Empty() == true) {
			return true;
		}
		if (other.Empty() == false) {
			return false;
		}
		return other.size_ >= size_ &&
			_wcsnicmp(&other.elements_[other.size_ - size_], elements_, size_) == 0;
	}

	bool WString::HasSuffix(const WString& suffix) const {
		return suffix.IsSuffixOf(*this);
	}

	bool WString::HasCiSuffix(const WString& suffix) const {
		return suffix.IsCiSuffixOf(*this);
	}

	size_t WString::FindFirstOf(const WString& pat, size_t begin_pos) const {
		if (begin_pos > size_ || Empty() == true || pat.Empty() == true) return kNPos;
		const WCHAR* pos = wcsstr(elements_ + begin_pos, pat.elements_);
		return pos ? static_cast<size_t>(pos - elements_) : kNPos;
	}

	size_t WString::FindFirstOf(const WCHAR* s, size_t begin_pos) const {
		if (begin_pos > size_ || Empty() == true || s == nullptr ) return kNPos;
		const WCHAR* pos = wcsstr(elements_ + begin_pos, s);
		return pos ? static_cast<size_t>(pos - elements_) : kNPos;
	}

	size_t WString::FindFirstOf(const UNICODE_STRING& u, size_t begin_pos) const {
		bool is_unistr_empty = u.Buffer == nullptr || u.Length < sizeof(WCHAR) || u.MaximumLength <= sizeof(WCHAR);
		if (begin_pos > size_ || Empty() == true || is_unistr_empty == true) return kNPos;
		if (u.Length == u.MaximumLength) {
			WString tmp(u);
			return FindFirstOf(tmp, begin_pos);
		}
		else {
			u.Buffer[u.Length / sizeof(WCHAR)] = L'\0';
			return FindFirstOf(u.Buffer, begin_pos);
		}
	}

	bool WString::Contain(const WString& w) const {
		return FindFirstOf(w) != kNPos;
	}

	bool WString::Contain(const WCHAR* s) const {
		return FindFirstOf(s) != kNPos;
	}

	bool WString::Contain(const UNICODE_STRING& u) const {
		return FindFirstOf(u) != kNPos;
	}

	bool WString::operator==(const WString& o) const {
		if ((Empty() == true) + (o.Empty() == true) == 1) {
			return false;
		}
		if ((Empty() == true) + (o.Empty() == true) == 1) {
			return true;
		}
		return wcscmp(elements_, o.elements_) == 0;
	}

	bool WString::operator==(const WCHAR* s) const {
		return s && wcscmp(elements_, s) == 0;
	}

	bool WString::operator==(const UNICODE_STRING& u) const {
		bool is_unistr_empty = u.Buffer == nullptr || u.Length < sizeof(WCHAR) || u.MaximumLength <= sizeof(WCHAR);
		if (Empty() == true && is_unistr_empty == true) {
			return true;
		}
		if ((Empty() == true) != is_unistr_empty) {
			return false;
		}
		size_t ulen = u.Length / sizeof(WCHAR);
		return size_ == ulen && memcmp(elements_, u.Buffer, ulen * sizeof(WCHAR)) == 0;
	}

	bool WString::EqualCi(const WString& o) const {
		if ((Empty() == true) + (o.Empty() == true) == 1) {
			return false;
		}
		if ((Empty() == true) + (o.Empty() == true) == 1) {
			return true;
		}
		return _wcsicmp(elements_, o.elements_) == 0;
	}

	bool WString::EqualCi(const WCHAR* s) const {
		return s && _wcsicmp(elements_, s) == 0;
	}

	bool WString::EqualCi(const UNICODE_STRING& u) const {
		bool is_unistr_empty = u.Buffer == nullptr || u.Length < sizeof(WCHAR) || u.MaximumLength <= sizeof(WCHAR);
		if (Empty() == true && is_unistr_empty == true) {
			return true;
		}
		if ((Empty() == true) != is_unistr_empty) {
			return false;
		}
		if (u.Length == u.MaximumLength) {
			WString tmp(u);
			return EqualCi(tmp);
		}
		else {
			u.Buffer[u.Length / sizeof(WCHAR)] = L'\0';
			return EqualCi(u.Buffer);
		}
	}

	bool WString::operator!=(const WString& o) const { return !(*this == o); }
	bool WString::operator!=(const WCHAR* s) const { return !(*this == s); }
	bool WString::operator!=(const UNICODE_STRING& u) const { return !(*this == u); }

	bool WString::operator>(const WString& o) const
	{
		return wcscmp(elements_, o.elements_) > 0;
	}

	bool WString::operator>(const WCHAR* s) const
	{
		return s && wcscmp(elements_, s) > 0;
	}

	bool WString::operator>(const UNICODE_STRING& u) const
	{
		if (u.Length == u.MaximumLength)
		{
			WString tmp(u);
			return *this > tmp;
		}
		else
		{
			u.Buffer[u.Length / sizeof(WCHAR)] = L'\0';
			return *this > u.Buffer;
		}
	}

	bool WString::operator<(const WString& o) const
	{
		return wcscmp(elements_, o.elements_) < 0;
	}

	bool WString::operator<(const WCHAR* s) const
	{
		return s && wcscmp(elements_, s) < 0;
	}

	bool WString::operator<(const UNICODE_STRING& u) const
	{
		if (u.Length == u.MaximumLength) {
			WString tmp(u);
			return *this < tmp;
		}
		else {
			u.Buffer[u.Length / sizeof(WCHAR)] = L'\0';
			return *this < u.Buffer;
		}
	}

	WCHAR* WString::Allocate(size_t n)
	{
		WCHAR* p = new WCHAR[n + 1];
		if (p != nullptr) {
			p[n] = L'\0';
		}
		else {
			ExRaiseStatus(STATUS_INSUFFICIENT_RESOURCES);
		}
		return p;
	}

	void WString::Deallocate(PVOID buf)
	{
		if (buf != nullptr) {
			delete[] buf;
		}
	}

	void WString::Reset()
	{
		if (elements_ != nullptr) {
			delete[] elements_;
		}
		elements_ = nullptr;
		capacity_ = 0;
		size_ = 0;
	}

	void WString::Repair()
	{
		if (elements_ == nullptr || size_ > capacity_ || capacity_ == 0) {
			Reset();
		}
	}

	const bool WString::IsInvalid() const
	{
		if (size_ > capacity_) {
			return true;
		}
		if (elements_ == nullptr && (size_ != 0 || capacity_ != 0)) {
			return true;
		}
		return false;
	}

	WString ToWString(short num)
	{
        WString str;
        str.Resize(8);
        RtlStringCchPrintfW(str.Data(), str.Capacity(), L"%d", num);
        str.Resize(wcslen(str.Data()));
        return str;
	}

	WString ToWString(unsigned short num)
	{
        WString str;
        str.Resize(8);
        RtlStringCchPrintfW(str.Data(), str.Capacity(), L"%u", num);
        str.Resize(wcslen(str.Data()));
        return str;
	}

    WString ToWString(int num)
    {
        WString str;
        str.Resize(16);
        RtlStringCchPrintfW(str.Data(), str.Capacity(), L"%d", num);
        str.Resize(wcslen(str.Data()));
        return str;
    }

    WString ToWString(unsigned int num)
    {
        WString str;
        str.Resize(16);
        RtlStringCchPrintfW(str.Data(), str.Capacity(), L"%u", num);
        str.Resize(wcslen(str.Data()));
        return str;
    }

    WString ToWString(long num)
    {
        WString str;
        str.Resize(16);
        RtlStringCchPrintfW(str.Data(), str.Capacity(), L"%ld", num);
        str.Resize(wcslen(str.Data()));
        return str;
    }

    WString ToWString(unsigned long num)
    {
        WString str;
        str.Resize(16);
        RtlStringCchPrintfW(str.Data(), str.Capacity(), L"%lu", num);
        str.Resize(wcslen(str.Data()));
        return str;
    }

    WString ToWString(long long num)
    {
        WString str;
        str.Resize(32);
        RtlStringCchPrintfW(str.Data(), str.Capacity(), L"%lld", num);
        str.Resize(wcslen(str.Data()));
        return str;
    }

	WString ToWString(unsigned long long num)
    {
        WString str;
        str.Resize(32);
        RtlStringCchPrintfW(str.Data(), str.Capacity(), L"%llu", num);
        str.Resize(wcslen(str.Data()));
        return str;
    }
}
