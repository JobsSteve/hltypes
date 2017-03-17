/// @file
/// @version 4.0
/// 
/// @section LICENSE
/// 
/// This program is free software; you can redistribute it and/or modify it under
/// the terms of the BSD license: http://opensource.org/licenses/BSD-3-Clause

#include <algorithm>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harray.h"
#include "hexception.h"
#include "hlog.h"
#include "hltypesUtil.h"
#include "hplatform.h"
#include "hstring.h"
#include "platform_internal.h"

#ifdef __APPLE__
#import <TargetConditionals.h>
#endif

// some platforms don't have this defined in this way
#ifndef va_copy
	#ifdef __va_copy
		#define va_copy __va_copy
	#else
		#define va_copy(dest, src) ((dest) = (src))
		#ifdef va_end
			#undef va_end
		#endif
		#define va_end(dest)
	#endif
#endif

/*
7	U+7F		0xxxxxxx
11	U+7FF		110xxxxx	10xxxxxx
16	U+FFFF		1110xxxx	10xxxxxx	10xxxxxx
21	U+1FFFFF	11110xxx	10xxxxxx	10xxxxxx	10xxxxxx
26	U+3FFFFFF	111110xx	10xxxxxx	10xxxxxx	10xxxxxx	10xxxxxx
31	U+7FFFFFFF	1111110x	10xxxxxx	10xxxxxx	10xxxxxx	10xxxxxx	10xxxxxx
*/
#define _FROM_UNICODE_FAST(string, value) \
	if ((value) < 0x80) \
	{ \
		string += (char)(value); \
	} \
	else if ((value) < 0x800) \
	{ \
		string += (char)(0xC0 | ((value) >> 6)); \
		string += (char)(0x80 | ((value) & 0x3F)); \
	} \
	else if ((value) < 0x10000) \
	{ \
		string += (char)(0xE0 | ((value) >> 12)); \
		string += (char)(0x80 | (((value) >> 6) & 0x3F)); \
		string += (char)(0x80 | ((value) & 0x3F)); \
	} \
	else if ((value) < 0x200000) \
	{ \
		string += (char)(0xF0 | ((value) >> 18)); \
		string += (char)(0x80 | (((value) >> 12) & 0x3F)); \
		string += (char)(0x80 | (((value) >> 6) & 0x3F)); \
		string += (char)(0x80 | ((value) & 0x3F)); \
	} \
	else if ((value) < 0x4000000) \
	{ \
		string += (char)(0xF8 | ((value) >> 24)); \
		string += (char)(0x80 | (((value) >> 18) & 0x3F)); \
		string += (char)(0x80 | (((value) >> 12) & 0x3F)); \
		string += (char)(0x80 | (((value) >> 6) & 0x3F)); \
		string += (char)(0x80 | ((value) & 0x3F)); \
	} \
	else if ((value) < 0x80000000) \
	{ \
		string += (char)(0xFC | ((value) >> 30)); \
		string += (char)(0x80 | (((value) >> 24) & 0x3F)); \
		string += (char)(0x80 | (((value) >> 18) & 0x3F)); \
		string += (char)(0x80 | (((value) >> 12) & 0x3F)); \
		string += (char)(0x80 | (((value) >> 6) & 0x3F)); \
		string += (char)(0x80 | ((value) & 0x3F)); \
	}
#define _TO_UNICODE_FAST(result, str, i, size) \
	if (str[i] < 0x80) \
	{ \
		result = str[i]; \
		size = 1; \
	} \
	else if ((str[i] & 0xE0) == 0xC0) \
	{ \
		result = ((str[i] & 0x1F) << 6) | (str[i + 1] & 0x3F); \
		size = 2; \
	} \
	else if ((str[i] & 0xF0) == 0xE0) \
	{ \
		result = ((((str[i] & 0xF) << 6) | (str[i + 1] & 0x3F) ) << 6) | (str[i + 2] & 0x3F); \
		size = 3; \
	} \
	else \
	{ \
		result = ((((((str[i] & 0x7) << 6) | (str[i + 1] & 0x3F)) << 6) | (str[i + 2] & 0x3F)) << 6) | (str[i + 3] & 0x3F); \
		size = 4; \
	}

// MacOS 10.9 SDK has problems linking to tolower/toupper for some reason, so this is used... sigh..
#ifdef __APPLE__
	inline int __tolower__(int c)
	{
		return tolower(c);
	}

	inline int __toupper__(int c)
	{
		return toupper(c);
	}
#else
	#define __tolower__ tolower
	#define __toupper__ toupper
#endif

#define MIN_STRING_CAPACITY 16

namespace hltypes
{
	String::String() : capacity(MIN_STRING_CAPACITY)
	{
		this->data = (char*)malloc((int)this->capacity);
		this->data[0] = '\0';
	}

	String::String(const char c) : capacity(MIN_STRING_CAPACITY)
	{
		this->data = (char*)malloc((int)this->capacity);
		this->data[0] = c;
		this->data[1] = '\0';
	}

	String::String(const char c, const int times) : capacity(MIN_STRING_CAPACITY)
	{
		if (this->capacity < times + 1)
		{
			this->capacity = times + 1;
		}
		this->data = (char*)malloc((int)this->capacity);
		memset(this->data, c, times);
		this->data[times] = '\0';
	}

	String::String(const char* string) : capacity(MIN_STRING_CAPACITY)
	{
		int size = (int)strlen(string);
		if (this->capacity < size + 1)
		{
			this->capacity = size + 1;
		}
		this->data = (char*)malloc((int)this->capacity);
		memcpy(this->data, string, size + 1);
	}

	String::String(const String& string) : capacity(string.capacity)
	{
		this->data = (char*)malloc((int)this->capacity);
		memcpy(this->data, string.data, this->capacity);
	}

	String::String(const char* string, const int length) : capacity(MIN_STRING_CAPACITY)
	{
		if (this->capacity < length + 1)
		{
			this->capacity = length + 1;
		}
		this->data = (char*)malloc((int)this->capacity);
		memcpy(this->data, string, length);
		this->data[length] = '\0';
	}

	String::String(const String& string, const int length) : capacity(MIN_STRING_CAPACITY)
	{
		if (this->capacity < length + 1)
		{
			this->capacity = length + 1;
		}
		this->data = (char*)malloc((int)this->capacity);
		memcpy(this->data, string.data, length);
		this->data[length] = '\0';
	}

	String::String(const short s) : capacity(MIN_STRING_CAPACITY)
	{
		this->data = (char*)malloc((int)this->capacity);
		this->operator=(s);
	}

	String::String(const unsigned short s) : capacity(MIN_STRING_CAPACITY)
	{
		this->data = (char*)malloc((int)this->capacity);
		this->operator=(s);
	}

	String::String(const int i) : capacity(MIN_STRING_CAPACITY)
	{
		this->data = (char*)malloc((int)this->capacity);
		this->operator=(i);
	}

	String::String(const unsigned int i) : capacity(MIN_STRING_CAPACITY)
	{
		this->data = (char*)malloc((int)this->capacity);
		this->operator=(i);
	}

	String::String(const int64_t i) : capacity(MIN_STRING_CAPACITY)
	{
		this->data = (char*)malloc((int)this->capacity);
		this->operator=(i);
	}

	String::String(const uint64_t i) : capacity(MIN_STRING_CAPACITY)
	{
		this->data = (char*)malloc((int)this->capacity);
		this->operator=(i);
	}

	String::String(const float f) : capacity(MIN_STRING_CAPACITY)
	{
		this->data = (char*)malloc((int)this->capacity);
		this->operator=(f);
	}

	String::String(const float f, int precision)
	{
		char format[16];
		char string[64];
		_platformSprintf(format, "%%.%df", precision);
		_platformSprintf(string, format, f);
		int size = (int)strlen(string);
		if (this->capacity < size + 1)
		{
			this->capacity = size + 1;
		}
		this->data = (char*)malloc((int)this->capacity);
		memcpy(this->data, string, size + 1);
	}

	String::String(const double d) : capacity(MIN_STRING_CAPACITY)
	{
		this->data = (char*)malloc((int)this->capacity);
		this->operator=(d);
	}

	String::String(const double d, int precision)
	{
		char format[16];
		char string[64];
		_platformSprintf(format, "%%.%dlf", precision);
		_platformSprintf(string, format, d);
		int size = (int)strlen(string);
		if (this->capacity < size + 1)
		{
			this->capacity = size + 1;
		}
		this->data = (char*)malloc((int)this->capacity);
		memcpy(this->data, string, size + 1);
	}

	String::~String()
	{
		free(this->data);
	}

	void String::set(const char* string)
	{
		int size = (int)strlen(string);
		if (this->_tryIncreaseCapacity(size + 1))
		{
			memcpy(this->data, string, size + 1);
		}
	}

	void String::set(const char* string, const int length)
	{
		if (this->_tryIncreaseCapacity(length + 1))
		{
			memcpy(this->data, string, length);
			this->data[length] = '\0';
		}
	}

	String String::lowered() const
	{
		String result(*this);
		std::transform(result.begin(), result.end(), result.begin(), __tolower__);
		return result;
	}

	String String::uppered() const
	{
		String result(*this);
		std::transform(result.begin(), result.end(), result.begin(), __toupper__);
		return result;
	}

	String String::reversed() const
	{
		String result(*this);
		std::reverse(result.begin(), result.end());
		return result;
	}

	String String::utf8Reversed() const
	{
		std::basic_string<unsigned int> result = this->uStr();
		std::reverse(result.begin(), result.end());
		return fromUnicode(result.c_str());
	}

	String String::trimmed(const char c) const
	{
		return this->trimmedLeft(c).trimmedRight(c);
	}

	String String::trimmedLeft(const char c) const
	{
		const char* string = this->data;
		while (*string == c)
		{
			++string;
		}
		return string;
	}

	String String::trimmedRight(const char c) const
	{
		int length = strlen(this->data);
		if (length == 0)
		{
			return "";
		}
		int i = length - 1;
		while (i >= 0 && this->data[i] == c)
		{
			--i;
		}
		return String(this->data, i + 1);
	}

	void String::replace(const String& what, const String& withWhat)
	{
		this->replace(what.cStr(), withWhat.cStr());
	}

	void String::replace(const String& what, const char* withWhat)
	{
		this->replace(what.cStr(), withWhat);
	}

	void String::replace(const String& what, const char withWhat, int times)
	{
		this->replace(what.cStr(), withWhat, times);
	}

	void String::replace(const char* what, const String& withWhat)
	{
		this->replace(what, withWhat.cStr());
	}

	void String::replace(const char* what, const char* withWhat)
	{
		int whatLength = (int)strlen(what);
		if (whatLength == 0)
		{
			Log::warn(logTag, "Cannot replace empty string in string: " + *this);
			return;
		}
		int withWhatLength = (int)strlen(withWhat);
		size_t position = 0;
		while (true)
		{
			position = stdstr::find(what, position);
			if (position == std::string::npos)
			{
				break;
			}
			stdstr::replace(position, whatLength, withWhat);
			position += withWhatLength;
		}
	}

	void String::replace(const char* what, const char withWhat, int times)
	{
		int whatLength = (int)strlen(what);
		if (whatLength == 0)
		{
			Log::warn(logTag, "Cannot replace empty string in string: " + *this);
			return;
		}
		size_t position = 0;
		while (true)
		{
			position = stdstr::find(what, position);
			if (position == std::string::npos)
			{
				break;
			}
			stdstr::replace(position, whatLength, times, withWhat);
			position += times;
		}
	}

	void String::replace(const char what, const String& withWhat)
	{
		const char string[2] = { what, '\0' };
		return this->replace(string, withWhat.cStr());
	}

	void String::replace(const char what, const char* withWhat)
	{
		const char string[2] = { what, '\0' };
		return this->replace(string, withWhat);
	}

	void String::replace(const char what, const char withWhat, int times)
	{
		const char string[2] = { what, '\0' };
		return this->replace(string, withWhat, times);
	}

	void String::replace(int position, int count, const String& string)
	{
		stdstr::replace(position, count, string.cStr());
	}

	void String::replace(int position, int count, const char* string)
	{
		stdstr::replace(position, count, string);
	}

	void String::replace(int position, int count, const char character, int times)
	{
		stdstr::replace(position, count, times, character);
	}

	String String::replaced(const String& what, const String& withWhat) const
	{
		String result(*this);
		result.replace(what, withWhat);
		return result;
	}

	String String::replaced(const String& what, const char* withWhat) const
	{
		String result(*this);
		result.replace(what, withWhat);
		return result;
	}

	String String::replaced(const String& what, const char withWhat, int times) const
	{
		String result(*this);
		result.replace(what, withWhat, times);
		return result;
	}

	String String::replaced(const char* what, const String& withWhat) const
	{
		String result(*this);
		result.replace(what, withWhat);
		return result;
	}

	String String::replaced(const char* what, const char* withWhat) const
	{
		String result(*this);
		result.replace(what, withWhat);
		return result;
	}

	String String::replaced(const char* what, const char withWhat, int times) const
	{
		String result(*this);
		result.replace(what, withWhat, times);
		return result;
	}

	String String::replaced(const char what, const String& withWhat) const
	{
		String result(*this);
		result.replace(what, withWhat);
		return result;
	}

	String String::replaced(const char what, const char* withWhat) const
	{
		String result(*this);
		result.replace(what, withWhat);
		return result;
	}

	String String::replaced(const char what, const char withWhat, int times) const
	{
		String result(*this);
		result.replace(what, withWhat, times);
		return result;
	}

	String String::replaced(int position, int count, const String& string) const
	{
		String result(*this);
		result.replace(position, count, string);
		return result;
	}
	String String::replaced(int position, int count, const char* string) const
	{
		String result(*this);
		result.replace(position, count, string);
		return result;
	}
	String String::replaced(int position, int count, const char character, int times) const
	{
		String result(*this);
		result.replace(position, count, character, times);
		return result;
	}

	void String::insertAt(int position, const String& string)
	{
		stdstr::insert(position, string.cStr()).c_str();
	}

	void String::insertAt(int position, const char* string)
	{
		stdstr::insert(position, string).c_str();
	}

	void String::insertAt(int position, const char character, int times)
	{
		stdstr::insert(position, times, character);
	}

	String String::insertedAt(int position, const String& string) const
	{
		String result(*this);
		result.insertAt(position, string);
		return result;
	}

	String String::insertedAt(int position, const char* string) const
	{
		String result(*this);
		result.insertAt(position, string);
		return result;
	}

	String String::insertedAt(int position, const char character, int times) const
	{
		String result(*this);
		result.insertAt(position, character, times);
		return result;
	}

	void String::randomize()
	{
		std::random_shuffle(stdstr::begin(), stdstr::end());
	}

	void String::utf8Randomize()
	{
		std::basic_string<unsigned int> ustr = this->uStr();
		std::random_shuffle(ustr.begin(), ustr.end());
		this->operator=(fromUnicode(ustr.c_str()));
	}

	String String::randomized() const
	{
		String result(*this);
		result.randomize();
		return result;
	}

	String String::utf8Randomized() const
	{
		String result(*this);
		result.utf8Randomize();
		return result;
	}

	Array<char> String::split() const
	{
		return Array<char>(stdstr::c_str(), (int)stdstr::size());
	}
	
	Array<String> String::split(const char* delimiter, unsigned int n, bool removeEmpty) const
	{
		Array<String> out;
		const char* s = this->data;
		const char* p = NULL;
		int delimiterLength = (int)strlen(delimiter);
		while ((p = strstr(s, delimiter)) != 0 && n > 0)
		{
			out += String(s, (int)(p - s));
			s = p + delimiterLength;
			--n;
		}
		out += String(s);
		if (removeEmpty)
		{
			out.removeAll("");
		}
		return out;
	}
	
	Array<String> String::split(const char delimiter, unsigned int n, bool removeEmpty) const
	{
		const char string[2] = { delimiter, '\0'};
		return this->split(string, n, removeEmpty);
	}

	Array<String> String::split(const String& delimiter, unsigned int n, bool removeEmpty) const
	{
		return this->split(delimiter.cStr(), n, removeEmpty);
	}
	
	bool String::split(const char* delimiter, String& outLeft, String& outRight) const
	{
		int index = (int)stdstr::find(delimiter);
		if (index < 0)
		{
			return false;
		}
		outLeft = stdstr::substr(0, index).c_str();
		outRight = stdstr::substr(index + strlen(delimiter)).c_str();
		return true;
	}
	
	bool String::split(const char delimiter, String& outLeft, String& outRight) const
	{
		const char string[2] = { delimiter, '\0'};
		return this->split(string, outLeft, outRight);
	}

	bool String::split(const String& delimiter, String& outLeft, String& outRight) const
	{
		return this->split(delimiter.cStr(), outLeft, outRight);
	}

	Array<String> String::rsplit(const char* delimiter, unsigned int n, bool removeEmpty) const
	{
		Array<String> out;
		const char* s = this->data;
		const char* p = NULL;
		int delimiter_len = (int)strlen(delimiter);
		for (p = s + strlen(s) - 1; p != s && n > 0; --p)
		{
			if (strncmp(p, delimiter, delimiter_len) == 0)
			{
				--n;
			}
		}
		if (s != p)
		{
			out += String(s, (int)(p - s + 1));
			s = p + 1 + delimiter_len;
		}
		while ((p = strstr(s, delimiter)) != 0)
		{
			out += String(s, (int)(p - s));
			s = p + delimiter_len;
		}
		out += String(s);
		if (removeEmpty)
		{
			out.removeAll("");
		}
		return out;
	}

	Array<String> String::rsplit(const char delimiter, unsigned int n, bool removeEmpty) const
	{
		const char string[2] = { delimiter, '\0' };
		return this->rsplit(string, n, removeEmpty);
	}

	Array<String> String::rsplit(const String& delimiter, unsigned int n, bool removeEmpty) const
	{
		return this->rsplit(delimiter.cStr(), n, removeEmpty);
	}

	bool String::rsplit(const char* delimiter, String& outLeft, String& outRight) const
	{
		int index = (int)stdstr::rfind(delimiter);
		if (index < 0)
		{
			return false;
		}
		outLeft = stdstr::substr(0, index).c_str();
		outRight = stdstr::substr(index + strlen(delimiter)).c_str();
		return true;
	}

	bool String::rsplit(const char delimiter, String& outLeft, String& outRight) const
	{
		const char string[2] = { delimiter, '\0' };
		return this->rsplit(string, outLeft, outRight);
	}

	bool String::rsplit(const String& delimiter, String& outLeft, String& outRight) const
	{
		return this->rsplit(delimiter.cStr(), outLeft, outRight);
	}

	int String::indexOf(const char c, int index) const
	{
		return (int)stdstr::find(c, index);
	}

	int String::indexOf(const char* string, int index) const
	{
		return (int)stdstr::find(string, index);
	}

	int String::indexOf(const String& string, int index) const
	{
		return (int)stdstr::find(string, index);
	}

	int String::rindexOf(const char c, int index) const
	{
		return (int)stdstr::rfind(c, index);
	}

	int String::rindexOf(const char* string, int index) const
	{
		return (int)stdstr::rfind(string, index);
	}

	int String::rindexOf(const String& string, int index) const
	{
		return (int)stdstr::rfind(string, index);
	}

	int String::indexOfAny(const char* string, int index) const
	{
		return (int)stdstr::find_first_of(string, index);
	}

	int String::indexOfAny(const String& string, int index) const
	{
		return (int)stdstr::find_first_of(string.cStr(), index);
	}

	int String::rindexOfAny(const char* string, int index) const
	{
		return (int)stdstr::find_last_of(string, index);
	}

	int String::rindexOfAny(const String& string, int index) const
	{
		return (int)stdstr::find_last_of(string.cStr(), index);
	}

	int String::count(const char c) const
	{
		const char string[2] = { c, '\0' };
		return this->count(string);
	}

	int String::count(const char* string) const
	{
		int result = 0;
		String thisString = stdstr::c_str();
		for_iter (i, 0, this->size())
		{
			if (thisString(i, -1).startsWith(string))
			{
				++result;
				i += strlen(string) - 1;
			}
		}
		return result;
	}

	int String::count(const String& string) const
	{
		return this->count(string.cStr());
	}

	bool String::startsWith(const char* string) const
	{
		return (strncmp(stdstr::c_str(), string, strlen(string)) == 0);
	}

	bool String::startsWith(const String& string) const
	{
		return this->startsWith(string.cStr());
	}

	bool String::endsWith(const char* string) const
	{
		const char* cString = stdstr::c_str();
		int thisLength = this->size();
		int stringLength = (int)strlen(string);
		if (stringLength > thisLength)
		{
			return false;
		}
		return (strcmp(cString + thisLength - stringLength, string) == 0);
	}

	bool String::endsWith(const String& string) const
	{
		return this->endsWith(string.cStr());
	}

	bool String::contains(const char c) const
	{
		const char string[2] = { c, '\0' };
		return this->contains(string);
	}

	bool String::contains(const char* string) const
	{
		return (stdstr::find(string) != stdstr::npos);
	}

	bool String::contains(const String& string) const
	{
		return this->contains(string.cStr());
	}

	bool String::containsAny(const char* string) const
	{
		int size = (int)strlen(string);
		for_iter (i, 0, size)
		{
			if (stdstr::find(string[i]) != stdstr::npos)
			{
				return true;
			}
		}
		return false;
	}

	bool String::containsAny(const String& string) const
	{
		return this->containsAny(string.cStr());
	}

	bool String::containsAll(const char* string) const
	{
		int size = (int)strlen(string);
		for_iter (i, 0, size)
		{
			if (stdstr::find(string[i]) != stdstr::npos)
			{
				return false;
			}
		}
		return true;
	}

	bool String::containsAll(const String& string) const
	{
		return this->containsAll(string.cStr());
	}

	bool String::isDigit() const
	{
		return (this->size() == 1 && isdigit(this->data[0]));
	}
	
	bool String::isInt() const
	{
		if (this->data[0] == '\0')
		{
			return false;
		}
		int i = 0;
		if (this->data[i] == '-')
		{
			++i;
		}
		for (; this->data[i] != '\0'; ++i)
		{
			if (!isdigit(this->data[i]))
			{
				return false;
			}
		}
		return true;
	}

	bool String::isFloat(bool requireDot) const
	{
		if (this->data[0] == '\0')
		{
			return false;
		}
		bool foundDot = false;
		int i = 0;
		if (this->data[i] == '-')
		{
			++i;
		}
		for (; this->data[i] != '\0'; ++i)
		{
			if (this->data[i] == '.')
			{
				if (foundDot)
				{
					return false;
				}
				foundDot = true;
			}
			else if (!isdigit(this->data[i]))
			{
				return false;
			}
		}
		return (!requireDot || foundDot);
	}

	bool String::isNumber() const
	{
		return this->isFloat(false);
	}

	bool String::isHex() const
	{
		if (this->data[0] == '\0')
		{
			return false;
		}
		for (int i = 0; this->data[i] != '\0'; ++i)
		{
			if (!isxdigit(this->data[i]))
			{
				return false;
			}
		}
		return true;
	}

	bool String::isAscii() const
	{
		const unsigned char* string = (const unsigned char*)this->data;
		int i = 0;
		while (string[i] != 0)
		{
			if (string[i] > 0x7F)
			{
				return false;
			}
		}
		return true;
	}

	String String::subString(int start, int count) const
	{
		return stdstr::substr(start, count).c_str();
	}

	String String::utf8SubString(int start, int count) const
	{
		const unsigned char* string = (const unsigned char*)this->data;
		int startIndex = 0;
		int i = 0;
		while (string[i] != 0 && startIndex < start)
		{
			if (string[i] < 0x80)
			{
				i += 1;
			}
			else if ((string[i] & 0xE0) == 0xC0)
			{
				i += 2;
			}
			else if ((string[i] & 0xF0) == 0xE0)
			{
				i += 3;
			}
			else
			{
				i += 4;
			}
			++startIndex;
		}
#ifdef _DEBUG
		if (string[i] == 0 && count > 0)
		{
			Log::warn(logTag, "Parameter 'start' in utf8SubString() is after end of string: " + *this);
		}
#endif
		int startSize = i;
		int size = 0;
		while (string[i] != 0 && size < count)
		{
			if (string[i] < 0x80)
			{
				i += 1;
			}
			else if ((string[i] & 0xE0) == 0xC0)
			{
				i += 2;
			}
			else if ((string[i] & 0xF0) == 0xE0)
			{
				i += 3;
			}
			else
			{
				i += 4;
			}
			++size;
		}
#ifdef _DEBUG
		if (size < count)
		{
			Log::warn(logTag, "Parameter 'count' in utf8SubString() is out of bounds in string: " + *this);
		}
#endif
		return String((char*)&string[startSize], i - startSize);
	}
	
	int String::size() const
	{
		return strlen(this->data);
	}
	
	int String::utf8Size() const
	{
		int result = 0;
		const unsigned char* string = (const unsigned char*)this->data;
		int i = 0;
		while (string[i] != 0)
		{
			if (string[i] < 0x80)
			{
				i += 1;
			}
			else if ((string[i] & 0xE0) == 0xC0)
			{
				i += 2;
			}
			else if ((string[i] & 0xF0) == 0xE0)
			{
				i += 3;
			}
			else
			{
				i += 4;
			}
			++result;
		}
		return result;
	}
	
	String String::toHex() const
	{
		String hex;
		int size = this->size();
		for_iter (i, 0, size)
		{
			hex += hsprintf("%02X", this->data[i]);
		}
		return hex;
	}

	unsigned int String::unhex() const
	{
		unsigned int i = 0;
		if (this->isHex())
		{
			sscanf(this->uppered().cStr(), "%X", &i);
		}
		return i;
	}

	String String::operator()(int start, int count) const
	{
		if (count < 0)
		{
			count = strlen(this->data) + count + 1;
		}
		return stdstr::substr(start, count).c_str();
	}
	
	String String::operator()(int start, int count, int step) const
	{
		if (count < 0)
		{
			count = strlen(this->data) + count + 1;
		}
		if (step == 1)
		{
			return stdstr::substr(start, count).c_str();
		}
		String result;
		for_iter_step (i, start, start + count, step)
		{
			result += this->data[i];
		}
		return result;
	}
	
	String String::operator()(int index) const
	{
		return this->data[index];
	}
	
	char& String::operator[](int index)
	{
		return this->data[index];
	}
	
	const char& String::operator[](int index) const
	{
		return this->data[index];
	}
	
	String::operator short() const
	{
		short s = 0;
		sscanf(this->data, "%hd", &s);
		return s;
	}

	String::operator unsigned short() const
	{
		unsigned short s = 0;
		sscanf(this->data), "%hu", &s);
		return s;
	}

	String::operator int() const
	{
		int i = 0;
		sscanf(this->data, "%d", &i);
		return i;
	}

	String::operator unsigned int() const
	{
		unsigned int i = 0U;
		sscanf(this->data, "%u", &i);
		return i;
	}

	String::operator int64_t() const
	{
		int64_t i = 0LL;
		sscanf(this->data, "%lld", &i);
		return i;
	}

	String::operator uint64_t() const
	{
		uint64_t i = 0ULL;
		sscanf(this->data, "%llu", &i);
		return i;
	}

	String::operator float() const
	{
		float f = 0.0f;
		sscanf(this->data, "%f", &f);
		return f;
	}

	String::operator double() const
	{
		double d = 0.0;
		sscanf(this->data, "%lf", &d);
		return d;
	}

	String::operator bool() const
	{
		return (*this != "" && *this != "0" && this->lowered() != "false");
	}
	
	String String::operator=(const short s)
	{
		char string[64] = { '\0' };
		sprintf(string, "%hd", s);
		stdstr::operator=(string);
		return *this;
	}

	String String::operator=(const unsigned short s)
	{
		char string[64] = { '\0' };
		sprintf(string, "%hu", s);
		stdstr::operator=(string);
		return *this;
	}

	String String::operator=(const int i)
	{
		char string[64] = { '\0' };
		sprintf(string, "%d", i);
		stdstr::operator=(string);
		return *this;
	}

	String String::operator=(const unsigned int i)
	{
		char string[64] = { '\0' };
		sprintf(string, "%u", i);
		stdstr::operator=(string);
		return *this;
	}

	String String::operator=(const int64_t i)
	{
		char string[64] = { '\0' };
		sprintf(string, "%lld", i);
		stdstr::operator=(string);
		return *this;
	}

	String String::operator=(const uint64_t i)
	{
		char string[64] = { '\0' };
		sprintf(string, "%llu", i);
		stdstr::operator=(string);
		return *this;
	}

	String String::operator=(const float f)
	{
		char string[64] = { '\0' };
		sprintf(string, "%f", f);
		stdstr::operator=(string);
		return *this;
	}
	
	String String::operator=(const double d)
	{
		char string[64] = { '\0' };
		sprintf(string, "%lf", d);
		stdstr::operator=(string);
		return *this;
	}
	
	String String::operator=(const bool b)
	{
		stdstr::operator=(b ? "true" : "false");
		return *this;
	}

	String String::operator=(const char* string)
	{
		stdstr::operator=(string);
		return *this;
	}

	String String::operator=(char* string)
	{
		stdstr::operator=(string);
		return *this;
	}

	String String::operator=(const String& string)
	{
		stdstr::operator=(string.cStr());
		return *this;
	}

	void String::operator+=(const short s)
	{
		stdstr::append(String(s));
	}

	void String::operator+=(const unsigned short s)
	{
		stdstr::append(String(s));
	}

	void String::operator+=(const int i)
	{
		stdstr::append(String(i));
	}

	void String::operator+=(const unsigned int i)
	{
		stdstr::append(String(i));
	}

	void String::operator+=(const int64_t i)
	{
		stdstr::append(String(i));
	}

	void String::operator+=(const uint64_t i)
	{
		stdstr::append(String(i));
	}

	void String::operator+=(const float f)
	{
		String s = f;
		stdstr::append(s);
	}

	void String::operator+=(const double d)
	{
		String s = d;
		stdstr::append(s);
	}

	void String::operator+=(const bool b)
	{
		stdstr::append(String(b));
	}

	void String::operator+=(const char c)
	{
		stdstr::append(1, c);
	}

	void String::operator+=(char* string)
	{
		stdstr::append(string);
	}
	
	void String::operator+=(const char* string)
	{
		stdstr::append(string);
	}

	void String::operator+=(const String& string)
	{
		stdstr::append(string.cStr());
	}

	String String::operator+(const char c) const
	{
		String result(*this);
		result.append(1, c);
		return result;
	}

	String String::operator+(char* string) const
	{
		String result(*this);
		result.append(string);
		return result;
	}

	String String::operator+(const char* string) const
	{
		String result(*this);
		result.append(string);
		return result;
	}

	String String::operator+(const String& string) const
	{
		String result(*this);
		result.append(string);
		return result;
	}

	bool String::operator==(const short s) const
	{
		return ((short)*this == s);
	}

	bool String::operator==(const unsigned short s) const
	{
		return ((unsigned short)*this == s);
	}

	bool String::operator==(const int i) const
	{
		return ((int)*this == i);
	}

	bool String::operator==(const unsigned int i) const
	{
		return ((unsigned int)*this == i);
	}

	bool String::operator==(const int64_t i) const
	{
		return ((int64_t)*this == i);
	}

	bool String::operator==(const uint64_t i) const
	{
		return ((uint64_t)*this == i);
	}

	bool String::operator==(const float f) const
	{
		return heqf((float)*this, f);
	}

	bool String::operator==(const double d) const
	{
		return heqd((double)*this, d);
	}

	bool String::operator==(const bool b) const
	{
		return ((strcmp(this->data, "1") == 0 && b) ||
				(strcmp(this->data, "0") == 0 && !b) ||
				(strcmp(this->data, "true") == 0 && b) ||
				(strcmp(this->data, "false") == 0 && !b));
	}

	bool String::operator==(const char* string) const
	{
		return (strcmp(this->data, string) == 0);
	}
	
	bool String::operator==(const String& string) const
	{
		return (strcmp(this->data, string.data) == 0);
	}

	bool String::operator!=(const short s) const
	{
		return !(this->operator==(s));
	}

	bool String::operator!=(const unsigned short s) const
	{
		return !(this->operator==(s));
	}

	bool String::operator!=(const int i) const
	{
		return !(this->operator==(i));
	}

	bool String::operator!=(const unsigned int i) const
	{
		return !(this->operator==(i));
	}

	bool String::operator!=(const int64_t i) const
	{
		return !(this->operator==(i));
	}

	bool String::operator!=(const uint64_t i) const
	{
		return !(this->operator==(i));
	}

	bool String::operator!=(const float f) const
	{
		return !(this->operator==(f));
	}

	bool String::operator!=(const double d) const
	{
		return !(this->operator==(d));
	}

	bool String::operator!=(const bool b) const
	{
		return !(this->operator==(b));
	}

	bool String::operator!=(const char* string) const
	{
		return !(this->operator==(string));
	}

	bool String::operator!=(const String& string) const
	{
		return !(this->operator==(string));
	}

	bool String::operator<(const String& string) const
	{
		return (strcmp(this->data, string.data) < 0);
	}

	bool String::operator>(const String& string) const
	{
		return (strcmp(this->data, string.data) > 0);
	}

	bool String::operator<=(const String& string) const
	{
		return (strcmp(this->data, string.data) <= 0);
	}

	bool String::operator>=(const String& string) const
	{
		return (strcmp(this->data, string.data) >= 0);
	}

	const char* String::cStr() const
	{
		return this->data;
	}

	std::basic_string<unsigned int> String::uStr() const
	{
		std::basic_string<unsigned int> result;
#ifdef __APPLE__ // bugfix for apple llvm compiler, has allocation problems in std::string with unsigned int combination
		if (stdstr::size() == 0)
		{
			unsigned int ary[] = {'x', 0};
			result = ary;
			ary[0] = 0;
			result = ary;
			return result;
		}
#endif
		unsigned int code = 0;
		const unsigned char* string = (const unsigned char*)this->data;
		int i = 0;
		int size = 0;
		while (string[i] != 0)
		{
			_TO_UNICODE_FAST(code, string, i, size);
			result += code;
			i += size;
		}
		return result;
	}

	std::basic_string<wchar_t> String::wStr() const
	{
		std::basic_string<wchar_t> result;
#ifdef __APPLE__ // bugfix for apple llvm compiler, has allocation problems in std::string with unsigned int combination
		if (stdstr::size() == 0)
		{
			wchar_t ary[] = {'x', 0};
			result = ary;
			ary[0] = 0;
			result = ary;
			return result;
		}
#endif
		unsigned int code = 0;
		const unsigned char* string = (const unsigned char*)this->data;
		int i = 0;
		int size = 0;
#ifdef _DEBUG
		bool checked = false;
#endif
		while (string[i] != 0)
		{
			_TO_UNICODE_FAST(code, string, i, size);
#ifdef _DEBUG
			if (!checked && code > 0xFFFF)
			{
				hltypes::_platformPrint(logTag, "String uses Unicode characters above 0xFFFF:", 1000);
				hltypes::_platformPrint(logTag, *this, 1000); // usually causes a stack overflow
				checked = true;
			}
#endif
			result += code;
			i += size;
		}
		return result;
	}

	unsigned int String::firstUnicodeChar(int index, int* byteCount) const
	{
		unsigned int result = 0;
		const unsigned char* string = (const unsigned char*)this->data;
		int size = 0;
		_TO_UNICODE_FAST(result, string, index, size);
		if (byteCount != NULL)
		{
			*byteCount = size;
		}
		return result;
	}

	String String::fromUnicode(unsigned int value)
	{
		String result;
		_FROM_UNICODE_FAST(result, value);
		return result;
	}

	String String::fromUnicode(wchar_t value)
	{
		String result;
		unsigned int code = value;
		_FROM_UNICODE_FAST(result, code);
		return result;
	}

	String String::fromUnicode(const char value)
	{
		return value;
	}

	String String::fromUnicode(unsigned char value)
	{
		return (char)value;
	}

	String String::fromUnicode(const unsigned int* string)
	{
		String result;
		if (string != NULL)
		{
			for (int i = 0; string[i] != 0; ++i)
			{
				_FROM_UNICODE_FAST(result, string[i]);
			}
		}
		return result;
	}

	String String::fromUnicode(const wchar_t* string)
	{
		String result;
		if (string != NULL)
		{
			unsigned int code;
			for (int i = 0; string[i] != 0; ++i)
			{
				code = string[i];
				_FROM_UNICODE_FAST(result, code);
			}
		}
		return result;
	}

	String String::fromUnicode(const char* string)
	{
		return (string != NULL ? string : "");
	}

	String String::fromUnicode(const unsigned char* string)
	{
		return (string != NULL ? (const char*)string : "");
	}

	String String::fromUnicode(Array<unsigned int> chars)
	{
		String result;
		foreach (unsigned int, it, chars)
		{
			_FROM_UNICODE_FAST(result, (*it));
		}
		return result;
	}

	String String::fromUnicode(Array<wchar_t> chars)
	{
		String result;
		unsigned int code;
		foreach (wchar_t, it, chars)
		{
			code = (*it);
			_FROM_UNICODE_FAST(result, code);
		}
		return result;
	}

	String String::fromUnicode(Array<char> chars)
	{
		String result;
		foreach (char, it, chars)
		{
			result += (*it);
		}
		return result;
	}

	String String::fromUnicode(Array<unsigned char> chars)
	{
		String result;
		foreach (unsigned char, it, chars)
		{
			result += (char)(*it);
		}
		return result;
	}

	bool String::_tryIncreaseCapacity(int size)
	{
		if (size > this->capacity)
		{
			int newCapacity = hmax(MIN_STRING_CAPACITY, hpotCeil(size)); // not allowing less than MIN_STRING_CAPACITY bytes
			if (this->capacity != newCapacity)
			{
				char* newData = (char*)realloc(this->data, newCapacity);
				if (newData == NULL) // could not reallocate enough memory
				{
					return false;
				}
				this->data = newData;
				this->capacity = newCapacity;
			}
		}
		return true;
	}

}

hltypes::String operator+(const char* string1, const hltypes::String& string2)
{
	return (hltypes::String(string1) + string2);
}

hltypes::String operator+(char* string1, const hltypes::String& string2)
{
	return (hltypes::String(string1) + string2);
}

hltypes::String hvsprintf(const char* format, va_list args)
{
	va_list vaCopy; // need to copy args because vsnprintf delets va_args on some platforms, and if we re-call it with a larger buffer, it will fail
	int size = 256; // safe assumption that most strings will be under 257 characters
	// not using a static buffer here to assure thread safety
	char* c = new char[size + 1];
	c[0] = '\0';
	int count = 0;
	int i;
	for_iterx (i, 0, 8)
	{
		// due to different (and non-standard) behavior in different implementations, there is one safe byte
		va_copy(vaCopy, args);
		count = hltypes::_platformVsnprintf(c, size, format, vaCopy);
		va_end(vaCopy);
		if (count >= 0 && count < size)
		{
			c[count] = '\0'; // terminate string
			break;
		}
		size *= 2; // not enough characters, double current buffer
		delete[] c;
		c = new char[size + 1];
		c[0] = '\0';
	}
#ifdef _DEBUG
	if (i >= 8)
	{
		delete[] c;
		throw Exception("Resulting string for hsprintf is longer than 2^16 (65536) characters!");
	}
#endif
	hltypes::String result(c);
	delete[] c;
	return result;
}

hltypes::String hsprintf(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	hltypes::String result = hvsprintf(format, args);
	va_end(args);
	return result;
}
