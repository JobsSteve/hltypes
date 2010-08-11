/************************************************************************************\
* This source file is part of the High Level C++ types library                       *
* For latest info, see http://libhltypes.sourceforge.net/                            *
**************************************************************************************
* Copyright (c) 2010 Kresimir Spes, Boris Mikic, Domagoj Cerjan                      *
*                                                                                    *
* This program is free software; you can redistribute it and/or modify it under      *
* the terms of the BSD license: http://www.opensource.org/licenses/bsd-license.php   *
\************************************************************************************/
#ifndef _HLTYPES_EXCEPTION
#define _HLTYPES_EXCEPTION

#include "hltypesExport.h"
#include "hstring.h"

namespace hltypes
{
	class hltypesExport exception
	{
	protected:
		hstr msg;
	public:
		exception(chstr message, const char* source_file, int line);
		virtual ~exception();
		virtual hstr message() { return this->msg; }
		virtual hstr getErrorText() { return message(); }
	};
	#define hl_exception(msg) hltypes::exception(msg, __FILE__, __LINE__)
/*********************************************************************************/
	class hltypesExport _file_not_found : public exception
	{
	public:
		_file_not_found(chstr filename, const char* source_file, int line);
	};
	#define file_not_found(filename) hltypes::_file_not_found(filename, __FILE__, __LINE__)
/*********************************************************************************/
	class hltypesExport _key_error : public exception
	{
	public:
		_key_error(chstr key, chstr container, const char* source_file, int line);
	};
	#define key_error(filename, container) hltypes::_key_error(filename, container, __FILE__, __LINE__)
/*********************************************************************************/
	class hltypesExport _resource_error : public exception
	{
	public:
		_resource_error(chstr type, chstr name, chstr container, const char* source_file, int line);
	};
	#define resource_error(type, name, container) hltypes::_resource_error(type, name, container, __FILE__, __LINE__)
}

#endif