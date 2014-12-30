/// @file
/// @version 3.0
/// 
/// @section LICENSE
/// 
/// This program is free software; you can redistribute it and/or modify it under
/// the terms of the BSD license: http://opensource.org/licenses/BSD-3-Clause

#ifndef _WIN32
#include <sys/stat.h>
#include <sys/types.h>
#endif

#include "hdir.h"
#include "hfbase.h"
#include "hstring.h"
#include "hthread.h"

namespace hltypes
{
	int FileBase::repeats = 0;
	float FileBase::timeout = 100.0f;

	FileInfo::FileInfo()
	{
		this->size = 0LL;
		this->creationTime = 0LL;
		this->accessTime = 0LL;
		this->modificationTime = 0LL;
	}

	FileInfo::~FileInfo()
	{
	}

	FileBase::FileBase(const String& filename) : StreamBase(), cfile(NULL)
	{
		this->filename = Dir::normalize(filename);
	}
	
	FileBase::FileBase() : StreamBase(), cfile(NULL)
	{
	}
	
	FileBase::~FileBase()
	{
		// this measure is not universal for all derived classes!
		if (this->_fisOpen())
		{
			this->_fclose();
		}
	}

	String FileBase::extensionOf(const String& path)
	{
		if (Dir::baseName(path).contains("."))
		{
			int index = path.rfind(".");
			if (index >= 0)
			{
				return path(index + 1, -1);
			}
		}
		return "";
	}

	String FileBase::withoutExtension(const String& path)
	{
		if (Dir::baseName(path).contains("."))
		{
			int index = path.rfind(".");
			if (index >= 0)
			{
				return path.substr(0, index);
			}
		}
		return path;
	}
	
	String FileBase::_descriptor()
	{
		return this->filename;
	}
	
	void FileBase::_fopen(const String& filename, AccessMode access_mode, int repeats, float timeout)
	{
		if (this->_isOpen())
		{
			this->_fclose();
		}
		this->filename = Dir::normalize(filename);
		String mode = "rb";
		switch (access_mode)
		{
		case READ:
			mode = "rb";
			break;
		case WRITE:
			mode = "wb";
			break;
		case APPEND:
			mode = "ab";
			break;
		case READ_WRITE:
			mode = "r+b";
			break;
		case READ_WRITE_CREATE:
			mode = "w+b";
			break;
		case READ_APPEND:
			mode = "a+b";
			break;
		}
		int attempts = repeats + 1;
		while (true)
		{
#ifdef _WIN32
			this->cfile = _wfopen(this->filename.w_str().c_str(), mode.w_str().c_str());
#else
			this->cfile = fopen(this->filename.c_str(), mode.c_str()); // TODO - should be ported to Unix systems as well
#endif
			if (this->cfile != NULL)
			{
				break;
			}
			--attempts;
			if (attempts <= 0)
			{
				break;
			}
			Thread::sleep(timeout);
		}
		if (this->cfile == NULL)
		{
			throw FileCouldNotOpenException(this->_descriptor());
		}
		this->_updateDataSize();
	}
	
	void FileBase::_fclose()
	{
		this->_validate();
		fclose((FILE*)this->cfile);
		this->cfile = NULL;
		this->dataSize = 0;
	}
	
	int FileBase::_fread(void* buffer, int count)
	{
		return (int)fread(buffer, 1, count, (FILE*)this->cfile);
	}
	
	int FileBase::_fwrite(const void* buffer, int count)
	{
		return (int)fwrite(buffer, 1, count, (FILE*)this->cfile);
	}
	
	bool FileBase::_fisOpen()
	{
		return (this->cfile != NULL);
	}
	
	int64_t FileBase::_fposition()
	{
		fpos_t position = 0;
		if (fgetpos((FILE*)this->cfile, &position) != 0)
		{
			return -1;
		}
		return (int64_t)position;
	}
	
	bool FileBase::_fseek(int64_t offset, SeekMode seekMode)
	{
		fpos_t position = 0;
		switch (seekMode)
		{
		case CURRENT:
			position = this->_fposition();
			break;
		case START:
			break;
		case END:
			position = this->size();
			break;
		}
		position += offset;
		return (fsetpos((FILE*)this->cfile, &position) == 0);
	}
	
	bool FileBase::_fexists(const String& filename, bool caseSensitive)
	{
		String name = Dir::normalize(filename);
		bool result = false;
#ifdef _WIN32
		FILE* f = _wfopen(name.w_str().c_str(), L"rb");
#else
		FILE* f = fopen(name.c_str(), "rb"); // TODO - should be ported to Unix systems as well
#endif
		if (f != NULL)
		{
			fclose(f);
			result = true;
		}
		if (!result && !caseSensitive)
		{
			String baseDir = Dir::baseDir(name);
			String baseName = Dir::baseName(name);
			Array<String> files = Dir::files(baseDir);
			foreach (String, it, files)
			{
				if ((*it).lower() == baseName.lower())
				{
					name = Dir::joinPath(baseDir, (*it));
					result = true;
					break;
				}
			}
		}
#ifndef _WIN32
		if (result)
		{
			struct stat stats;
			// on UNIX fopen on a directory actually works so this check prevents
			// errorously reporting the existence of a file if it's a directory
			if (stat(name.c_str(), &stats) == 0 && (stats.st_mode & S_IFMT) == S_IFDIR)
			{
				result = false;
			}
		}
#endif
		return result;
	}
	
}
