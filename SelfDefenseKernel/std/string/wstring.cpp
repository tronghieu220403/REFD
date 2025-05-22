#include "wstring.h"

#include <string.h>

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

// Default constructor
WString::WString()
	: size_(0), capacity_(0), elements_(Allocate(0))
{}

// Construct with size, fill with c
WString::WString(size_t size, WCHAR c)
	: size_(size), capacity_(size), elements_(Allocate(size))
{
	for (size_t i = 0; i < size_; ++i)
		elements_[i] = c;
}

// Copy constructor
WString::WString(const WString& other)
	: size_(other.size_), capacity_(other.capacity_), elements_(Allocate(other.capacity_))
{
	// copy including null terminator
	memcpy(elements_, other.elements_, (size_ + 1) * sizeof(WCHAR));
}

// Construct from C-string
WString::WString(const WCHAR* ws)
{
	if (!ws) {
		size_ = capacity_ = 0;
		elements_ = nullptr;
	}
	else {
		size_ = wcslen(ws);
		capacity_ = size_;
		elements_ = Allocate(size_);
		memcpy(elements_, ws, (size_ + 1) * sizeof(WCHAR));
	}
}

// Construct from UNICODE_STRING
WString::WString(const UNICODE_STRING& u)
{
	size_ = u.Length / sizeof(WCHAR);
	capacity_ = size_;
	elements_ = Allocate(size_);
	if (u.Buffer && size_ > 0) {
		memcpy(elements_, u.Buffer, size_ * sizeof(WCHAR));
	}
	elements_[size_] = L'\0';
}

// Construct from PUNICODE_STRING
WString::WString(const PUNICODE_STRING& pu)
{
	if (!pu || !pu->Buffer) {
		size_ = capacity_ = 0;
		elements_ = Allocate(0);
	}
	else {
		this->WString::WString(*pu);
	}
}

// Move constructor
WString::WString(WString&& other)
	: size_(other.size_), capacity_(other.capacity_), elements_(other.elements_)
{
	other.elements_ = nullptr;
	other.size_ = other.capacity_ = 0;
}

// Copy assignment
WString& WString::operator=(const WString& other)
{
	if (this != &other) {
		if (capacity_ < other.size_) {
			// Existing buffer too small: deallocate and allocate a new one
			Deallocate();
			elements_ = Allocate(other.size_);
			capacity_ = other.size_;
		}
		size_ = other.size_;
		memcpy(elements_, other.elements_, (size_ + 1) * sizeof(WCHAR));
	}
	return *this;
}

// Copy assignment from C-style wide string
WString& WString::operator=(const WCHAR* s)
{
	if (this->elements_ == s)
	{
		return *this;
	}
	size_t len = s ? wcslen(s) : 0;
	if (!s) {
		// Clear this string if the source is null
		Clear();
	}
	else
	{
		if (capacity_ < len) {
			// Buffer too small: deallocate and allocate a new one
			Deallocate();
			size_ = capacity_ = len;
			elements_ = Allocate(capacity_);
			memcpy(elements_, s, (len + 1) * sizeof(WCHAR));
		}
		size_ = len;
		memcpy(elements_, s, (len + 1) * sizeof(WCHAR));
	}
	return *this;
}

// Copy assignment from UNICODE_STRING reference
WString& WString::operator=(const UNICODE_STRING& u)
{
	size_t len = u.Buffer ? u.Length / sizeof(WCHAR) : 0;
	if (u.Length == 0 || u.Buffer == nullptr) {
		// clear if empty or null buffer
		Clear();
	}
	else {
		if (capacity_ < len) {
			Deallocate();
			elements_ = Allocate(len);
			capacity_ = len;
		}
		size_ = len;
		// copy len characters and null-terminate
		memcpy(elements_, u.Buffer, len * sizeof(WCHAR));
		elements_[size_] = L'\0';
	}
	return *this;
}

// Copy assignment from UNICODE_STRING pointer
WString& WString::operator=(const PUNICODE_STRING& pu)
{
	size_t len = (pu && pu->Buffer) ? pu->Length / sizeof(WCHAR) : 0;
	if (len == 0) {
		// clear if input is null
		Clear();
	}
	else {
		if (capacity_ < len) {
			Deallocate();
			elements_ = Allocate(len);
			capacity_ = len;
		}
		size_ = len;
		// copy len characters and null-terminate
		memcpy(elements_, pu->Buffer, len * sizeof(WCHAR));
		elements_[size_] = L'\0';
	}
	return *this;
}

// Destructor
WString::~WString()
{
	Deallocate();
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
	return iterator(&elements_[size_]);
}

// Return const iterator to one past last element
const WString::iterator WString::End() const
{
	return iterator(&elements_[size_]);
}

// Return const iterator to first element (alternative naming)
const WString::iterator WString::ConstBegin() const
{
	return iterator(elements_);
}

// Return const iterator to one past last element (alternative naming)
const WString::iterator WString::ConstEnd() const
{
	return iterator(&elements_[size_]);
}


bool WString::Empty() const { return size_ == 0; }

void WString::Reserve(size_t new_cap)
{
	if (new_cap <= capacity_) return;
	WCHAR* new_buf = Allocate(new_cap);
	if (elements_) {
		memcpy(new_buf, elements_, (size_ + 1) * sizeof(WCHAR));
		Deallocate();
	}
	elements_ = new_buf;
	capacity_ = new_cap;
}

void WString::Resize(size_t new_size, WCHAR val)
{
	if (new_size < size_) {
		size_ = new_size;
		elements_[size_] = L'\0';
	}
	else if (new_size > size_) {
		Reserve(new_size);
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
	size_ = 0;
	if (elements_) elements_[0] = L'\0';
}

void WString::PushBack(const WCHAR c)
{
	if (size_ + 1 >= capacity_)
	{
		Reserve(size_ * 2 + 1);
	}
	elements_[size_++] = c;
	elements_[size_] = L'\0';
}

void WString::PopBack()
{
	if (size_ == 0) return;
	elements_[--size_] = L'\0';
}

void WString::Append(const WString& other)
{
	Reserve(size_ + other.size_);
	memcpy(&elements_[size_], other.elements_, other.size_ * sizeof(WCHAR));
	size_ += other.size_;
	elements_[size_] = L'\0';
}

void WString::Append(const WCHAR* s)
{
	if (!s) return;
	size_t len = wcslen(s);
	Reserve(size_ + len);
	memcpy(&elements_[size_], s, len * sizeof(WCHAR));
	size_ += len;
	elements_[size_] = L'\0';
}

void WString::Append(const UNICODE_STRING& u)
{
	if (!u.Buffer) return;
	size_t len = u.Length / sizeof(WCHAR);
	if (size_ + 1 >= capacity_)
	{
		Reserve((size_ + len) * 3 / 2 + 1);
	}
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
	for (size_t i = 0; i < size_; ++i)
		elements_[i] = RtlUpcaseUnicodeChar(elements_[i]);
	return *this;
}

WString& WString::ConverToDowncase()
{
	for (size_t i = 0; i < size_; ++i)
		elements_[i] = RtlDowncaseUnicodeChar(elements_[i]);
	return *this;
}

WString WString::GetUpcase() {
	WString tmp(*this);
	tmp.ConverToUpcase();
	return tmp;
}

WString WString::GetDowncase() {
	WString tmp(*this);
	tmp.ConverToDowncase();
	return tmp;
}

WCHAR& WString::At(size_t n) {
	if (n >= size_)
	{
		ExRaiseAccessViolation();
	}
	return elements_[n];
}

const WCHAR& WString::At(size_t n) const {
	if (n >= size_)
	{
		return L'\0';
	}
	return elements_[n];
}

WCHAR& WString::operator[](size_t n) { return At(n); }
const WCHAR& WString::operator[](size_t n) const { return At(n); }

WCHAR& WString::Front() {
	if (size_ == 0)
	{
		ExRaiseAccessViolation();
	}
	return elements_[0];
}

const WCHAR& WString::Front() const {
	if (size_ == 0)
	{
		return L'\0';
	}
	return elements_[0];
}

WCHAR& WString::Back() {
	if (size_ == 0)
	{
		ExRaiseAccessViolation();
	}
	return elements_[size_ - 1];
}

const WCHAR& WString::Back() const {
	if (size_ == 0)
	{
		return L'\0';
	}
	return elements_[size_ - 1];
}

WCHAR* WString::Data() { return elements_; }
const WCHAR* WString::Data() const { return elements_; }

const UNICODE_STRING WString::UniStr() const
{
	UNICODE_STRING uni_str = { 0 };
	if (size_ * sizeof(WCHAR) >= UNICODE_STRING_MAX_BYTES)
	{
		ExRaiseStatus(STATUS_NAME_TOO_LONG);
	}
	uni_str.Length = static_cast<USHORT>(size_ * sizeof(WCHAR));
	uni_str.MaximumLength = static_cast<USHORT>((capacity_ + 1) * sizeof(WCHAR));
	uni_str.Buffer = elements_;
	return uni_str;
}

bool WString::IsPrefixOf(const WString& other) const {
	return other.size_ >= size_ &&
		wcsncmp(other.elements_, elements_, size_) == 0;
}

bool WString::IsCiPrefixOf(const WString& other) const {
	return other.size_ >= size_ &&
		_wcsnicmp(other.elements_, elements_, size_) == 0;
}

bool WString::HasPrefix(const WString& prefix) const {
	return size_ >= prefix.size_ &&
		wcsncmp(elements_, prefix.elements_, prefix.size_) == 0;
}

bool WString::HasCiPrefix(const WString& prefix) const {
	return size_ >= prefix.size_ &&
		_wcsnicmp(elements_, prefix.elements_, prefix.size_) == 0;
}

bool WString::IsSuffixOf(const WString& other) const {
	return other.size_ >= size_ &&
		wcsncmp(&other.elements_[other.size_ - size_], elements_, size_) == 0;
}

bool WString::IsCiSuffixOf(const WString& other) const {
	return other.size_ >= size_ &&
		_wcsnicmp(&other.elements_[other.size_ - size_], elements_, size_) == 0;
}

bool WString::HasSuffix(const WString& suffix) const {
	return size_ >= suffix.size_ &&
		wcsncmp(&elements_[size_ - suffix.size_], suffix.elements_, suffix.size_) == 0;
}

bool WString::HasCiSuffix(const WString& suffix) const {
	return size_ >= suffix.size_ &&
		_wcsnicmp(&elements_[size_ - suffix.size_], suffix.elements_, suffix.size_) == 0;
}

size_t WString::FindFirstOf(const WString& pat, size_t begin_pos) const {
	if (begin_pos > size_) return static_cast<size_t>(-1);
	const WCHAR* pos = wcsstr(elements_ + begin_pos, pat.elements_);
	return pos ? static_cast<size_t>(pos - elements_) : static_cast<size_t>(-1);
}

size_t WString::FindFirstOf(const WCHAR* s, size_t begin_pos) const {
	if (!s || begin_pos > size_) return static_cast<size_t>(-1);
	const WCHAR* pos = wcsstr(elements_ + begin_pos, s);
	return pos ? static_cast<size_t>(pos - elements_) : static_cast<size_t>(-1);
}

size_t WString::FindFirstOf(const UNICODE_STRING& u, size_t begin_pos) const {
	if (u.Length == u.MaximumLength)
	{
		WString tmp(u);
		return FindFirstOf(tmp, begin_pos);
	}
	else
	{
		u.Buffer[u.Length / sizeof(WCHAR)] = L'\0';
		return FindFirstOf(u.Buffer, begin_pos);
	}
}

bool WString::Contain(const WString& w) const {
	return FindFirstOf(w) != static_cast<size_t>(-1);
}

bool WString::Contain(const WCHAR* s) const {
	return FindFirstOf(s) != static_cast<size_t>(-1);
}

bool WString::Contain(const UNICODE_STRING& u) const {
	return FindFirstOf(u) != static_cast<size_t>(-1);
}

bool WString::operator==(const WString& o) const {
	return wcscmp(elements_, o.elements_) == 0;
}

bool WString::operator==(const WCHAR* s) const {
	return s && wcscmp(elements_, s) == 0;
}

bool WString::operator==(const UNICODE_STRING& u) const {
	if (u.Length == 0) return size_ == 0;
	size_t ulen = u.Length / sizeof(WCHAR);
	return size_ == ulen &&
		memcmp(elements_, u.Buffer, ulen * sizeof(WCHAR)) == 0;
}

bool WString::EqualCi(const WString& o) const {
	return _wcsicmp(elements_, o.elements_) == 0;
}

bool WString::EqualCi(const WCHAR* s) const {
	return s && _wcsicmp(elements_, s) == 0;
}

bool WString::EqualCi(const UNICODE_STRING& u) const {
	if (u.Length == u.MaximumLength)
	{
		WString tmp(u);
		return EqualCi(tmp);
	}
	else
	{
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
	if (u.Length == u.MaximumLength)
	{
		WString tmp(u);
		return *this < tmp;
	}
	else
	{
		u.Buffer[u.Length / sizeof(WCHAR)] = L'\0';
		return *this < u.Buffer;
	}
}

WCHAR* WString::Allocate(size_t n)
{
	WCHAR* p = new WCHAR[n + 1];
	p[n] = L'\0';
	return p;
}

void WString::Deallocate()
{
	delete[] elements_;
	elements_ = 0;
	capacity_ = 0;
	size_ = 0;
}