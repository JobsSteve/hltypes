/// @file
/// @author  Boris Mikic
/// @version 1.8
/// 
/// @section LICENSE
/// 
/// This program is free software; you can redistribute it and/or modify it under
/// the terms of the BSD license: http://www.opensource.org/licenses/bsd-license.php

#ifdef HAVE_ZIPRESOURCE
#include <stdio.h>
#include <zip/zip.h>
#endif

#include "exception.h"
#include "hdir.h"
#include "hresource.h"
#ifdef HAVE_ZIPRESOURCE
#include "hmap.h"
#include "hmutex.h"
#include "hstring.h"
#include "hthread.h"
#endif

namespace hltypes
{
#ifndef HAVE_ZIPRESOURCE
	hstr Resource::cwd = ".";
#else
	hstr Resource::cwd = "assets";
#define READ_BUFFER_SIZE 32768
	static unsigned char _read_buffer[READ_BUFFER_SIZE];
#endif
	hstr Resource::archive = "";

#ifdef HAVE_ZIPRESOURCE
	hmutex access_mutex;
	hmap<void*, harray<hresource*> > activeHandles;
	void* currentArchive = NULL;

	void* _zip_open(hresource* resource)
	{
		if (currentArchive == NULL)
		{
			hstr archive = Resource::getArchive();
			if (archive == "")
			{
				return NULL;
			}
			currentArchive = zip_open(archive.c_str(), 0, NULL);
			if (currentArchive == NULL)
			{
				return NULL;
			}
			activeHandles[currentArchive] = harray<hresource*>();
		}
		activeHandles[currentArchive] += resource;
		return currentArchive;
	}

	void _zip_close(hresource* resource, void* archive)
	{
		harray<hresource*> references = activeHandles[archive];
		references -= resource;
		activeHandles[archive] = references;
		if (currentArchive != archive && references.size() == 0)
		{
			zip_close((struct zip*)archive);
			activeHandles.remove_key(archive);
		}
	}

	void* _zip_fopen(void* archivefile, chstr filename)
	{
		access_mutex.lock();
		void* file = zip_fopen((struct zip*)archivefile, filename.c_str(), 0);
		access_mutex.unlock();
		return file;
	}

	void _zip_fclose(void* file)
	{
		access_mutex.lock();
		zip_fclose((struct zip_file*)file);
		access_mutex.unlock();
	}

	long _zip_fread(void* file, void* buffer, int count)
	{
		access_mutex.lock();
		long result = zip_fread((struct zip_file*)file, buffer, count);
		access_mutex.unlock();
		return result;
	}

	long _zip_fsize(void* archivefile, chstr filename)
	{
		struct zip_stat stat;
		stat.size = 0;
		access_mutex.lock();
		zip_stat((struct zip*)archivefile, Resource::make_full_path(filename).c_str(), 0, &stat);
		access_mutex.unlock();
		return stat.size;
	}

	void* _zip_freopen(void* file, void* archivefile, chstr filename)
	{
		access_mutex.lock();
		zip_fclose((struct zip_file*)file);
		file = zip_fopen((struct zip*)archivefile, filename.c_str(), 0);
		access_mutex.unlock();
		return file;
	}
#endif

	void Resource::setArchive(chstr value)
	{
#ifdef HAVE_ZIPRESOURCE
		if (currentArchive != NULL && activeHandles[currentArchive].size() == 0)
		{
			zip_close((struct zip*)currentArchive);
			activeHandles.remove_key(currentArchive);
			currentArchive = NULL;
		}
		harray<void*> handles = activeHandles.keys();
		foreach (void*, it, handles)
		{
			if (activeHandles[*it].size() == 0)
			{
				zip_close((struct zip*)(*it));
				activeHandles.remove_key((*it));
			}
		}
#endif
		archive = value;
	}

	Resource::Resource(chstr filename) : FileBase(), data_position(0), archivefile(NULL)
	{
		this->filename = normalize_path(filename);
		this->open(filename);
	}
	
	Resource::Resource() : FileBase(), data_position(0), archivefile(NULL)
	{
	}

	Resource::~Resource()
	{
		if (this->is_open())
		{
			this->close();
		}
	}
	
	void Resource::open(chstr filename)
	{
#ifndef HAVE_ZIPRESOURCE
		this->_fopen(Resource::make_full_path(filename), READ, 0, FileBase::repeats, FileBase::timeout);
#else
		if (this->is_open())
		{
			this->close();
		}
		this->filename = normalize_path(filename);
		this->encryption_offset = 0;
		int attempts = Resource::repeats + 1;
		while (true)
		{
			this->archivefile = _zip_open(this);
			if (this->archivefile != NULL)
			{
				this->cfile = _zip_fopen(this->archivefile, Resource::make_full_path(this->filename));
				if (this->cfile != NULL)
				{
					break;
				}
				// file wasn't found so let's rather close the archive
				_zip_close(this, this->archivefile);
				this->archivefile = NULL;
			}
			attempts--;
			if (attempts <= 0)
			{
				break;
			}
			Thread::sleep(Resource::timeout);
		}
		if (this->archivefile == NULL)
		{
			throw file_not_found(this->_descriptor());
		}
		if (this->cfile == NULL)
		{
			throw file_not_found(this->_descriptor());
		}
		this->_update_data_size();
#endif
	}
	
	void Resource::close()
	{
#ifndef HAVE_ZIPRESOURCE
		this->_fclose();
#else
		this->_check_availability();
		_zip_fclose(this->cfile);
		this->cfile = NULL;
		_zip_close(this, this->archivefile);
		this->archivefile = NULL;
		this->data_size = 0;
#endif
		this->data_position = 0;
	}
	
	bool Resource::hasZip()
	{
#ifndef HAVE_ZIPRESOURCE
		return false;
#else
		return true;
#endif
	}

	void Resource::_update_data_size()
	{
#ifndef HAVE_ZIPRESOURCE
		long position = this->_position();
		this->_fseek(0, END);
		this->data_size = this->_position();
		this->_fseek(position, START);
#else
		this->data_size = _zip_fsize(this->archivefile, this->filename);
#endif
	}

	long Resource::_read(void* buffer, int size, int count)
	{
#ifndef HAVE_ZIPRESOURCE
		return this->_fread(buffer, size, count);
#else
		int read_count = size * count;
		this->data_position += read_count;
		return _zip_fread(this->cfile, buffer, read_count);
#endif
	}
	
	long Resource::_write(const void* buffer, int size, int count)
	{
		throw file_not_writeable(this->filename);
	}

	bool Resource::_is_open()
	{
#ifndef HAVE_ZIPRESOURCE
		return this->_fis_open();
#else
		return (this->archivefile != NULL && this->cfile != NULL);
#endif
	}
	
	long Resource::_position()
	{
#ifndef HAVE_ZIPRESOURCE
		return this->_fposition();
#else
		return this->data_position;
#endif
	}
	
	void Resource::_seek(long offset, SeekMode seek_mode)
	{
#ifndef HAVE_ZIPRESOURCE
		this->_fseek(offset, seek_mode);
#else
		// zip can only read forward and doesn't really have seeking
		long target = offset;
		switch (seek_mode)
		{
		case CURRENT:
			target = offset + this->data_position;
			break;
		case START:
			target = offset;
			break;
		case END:
			target = this->data_size + offset;
			break;
		}
		if (target >= this->data_position)
		{
			target -= this->data_position;
		}
		else
		{
			// reopening the file as the target position was already passed
			this->cfile = _zip_freopen(this->cfile, this->archivefile, Resource::make_full_path(this->filename));
			this->data_position = 0;
		}
		if (target > 0)
		{
			unsigned char* buffer = _read_buffer;
			if (target > READ_BUFFER_SIZE)
			{
				buffer = new unsigned char[target];
			}
			this->_read(buffer, 1, target);
			if (target > READ_BUFFER_SIZE)
			{
				delete [] buffer;
			}
		}
#endif
	}
	
	bool Resource::exists(chstr filename)
	{
#ifndef HAVE_ZIPRESOURCE
		return FileBase::_fexists(Resource::make_full_path(filename));
#else
		bool result = false;
		void* a = _zip_open(NULL); // NULL, because this is a static function which will close the archive right after it is done
		if (a != NULL)
		{
			hstr name = normalize_path(Resource::make_full_path(filename));
			void* f = _zip_fopen(a, name);
			if (f != NULL)
			{
				_zip_fclose(f);
				result = true;
			}
			_zip_close(NULL, a);
		}
		return result;
#endif
	}
	
	long Resource::hsize(chstr filename)
	{
		return Resource(filename).size();
	}
	
	hstr Resource::hread(chstr filename, int count)
	{
		return Resource(filename).read(count);
	}
	
	hstr Resource::hread(chstr filename, chstr delimiter)
	{
		return Resource(filename).read(delimiter);
	}

	hstr Resource::make_full_path(chstr filename)
	{
		return normalize_path(Resource::cwd + "/" + filename);
	}

}

