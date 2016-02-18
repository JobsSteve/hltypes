/// @file
/// @version 3.0
/// 
/// @section LICENSE
/// 
/// This program is free software; you can redistribute it and/or modify it under
/// the terms of the BSD license: http://opensource.org/licenses/BSD-3-Clause

#include "hdir.h"
#include "hexception.h"
#include "hfile.h"
#include "hlog.h"
#include "hrdir.h"
#include "hresource.h"
#include "hthread.h"
#include "platform_internal.h"
#ifdef _ZIPRESOURCE
#include "zipaccess.h"
#endif

namespace hltypes
{
	bool Resource::zipArchive = false;
	String Resource::cwd = _platformResourceCwd();
	String Resource::archive = "";

	void Resource::setArchive(const String& value)
	{
#ifdef _ZIPRESOURCE
		if (archive != value)
		{
			ResourceDir::cacheDirectories.clear();
			ResourceDir::cacheFiles.clear();
		}
		zip::setArchive(value);
#endif
		archive = value;
#ifdef _ZIPRESOURCE
		void* a = zip::open(NULL); // NULL, because this is a static function which will close the archive right after it is done
		if (a != NULL)
		{
			zipArchive = true;
			zip::close(NULL, a);
		}
		else
		{
			zipArchive = false;
		}
#endif
	}

	Resource::Resource() : FileBase(), dataPosition(0), archiveFile(NULL)
	{
	}

	Resource::~Resource()
	{
		if (this->_isOpen())
		{
			this->close();
		}
	}
	
	void Resource::open(const String& filename)
	{
		this->resourceFilename = Dir::normalize(filename);
#ifdef _ZIPRESOURCE
		if (Resource::zipArchive)
		{
			if (this->_isOpen())
			{
				this->close();
			}
			this->filename = this->resourceFilename;
			int attempts = Resource::repeats + 1;
			while (true)
			{
				this->archiveFile = zip::open(this);
				if (this->archiveFile != NULL)
				{
					this->cfile = zip::fopen(this->archiveFile, Resource::makeFullPath(this->filename));
					if (this->cfile != NULL)
					{
						break;
					}
					// file wasn't found so let's rather close the archive
					zip::close(this, this->archiveFile);
					this->archiveFile = NULL;
				}
				--attempts;
				if (attempts <= 0)
				{
					break;
				}
				Thread::sleep(Resource::timeout);
			}
			if (this->archiveFile == NULL)
			{
				throw ResourceFileCouldNotOpenException(this->_descriptor());
			}
			if (this->cfile == NULL)
			{
				throw ResourceFileCouldNotOpenException(this->_descriptor());
			}
			this->_updateDataSize();
			return;
		}
#endif
		this->_fopen(Resource::makeFullPath(filename), READ, FileBase::repeats, FileBase::timeout);
		this->_updateDataSize();
	}
	
	void Resource::close()
	{
#ifdef _ZIPRESOURCE
		if (Resource::zipArchive)
		{
			this->_validate();
			if (this->cfile != NULL)
			{
				zip::fclose(this->cfile);
				this->cfile = NULL;
			}
			if (this->archiveFile != NULL)
			{
				zip::close(this, this->archiveFile);
				this->archiveFile = NULL;
			}
			this->dataSize = 0;
			this->dataPosition = 0;
			return;
		}
#endif
		this->_fclose();
		this->dataPosition = 0;
	}
	
	bool Resource::hasZip()
	{
#ifdef _ZIPRESOURCE
		return true;
#else
		return false;
#endif
	}

	void Resource::_updateDataSize()
	{
#ifdef _ZIPRESOURCE
		if (Resource::zipArchive)
		{
			this->dataSize = zip::finfo(this->archiveFile, Resource::makeFullPath(this->filename)).size;
			return;
		}
#endif
		// using hinfo, because the default FileBase implementation with _position() and _seek() can cause problems and Resources are always read-only anyway
		this->dataSize = Resource::hinfo(this->resourceFilename).size;
	}

	int Resource::_read(void* buffer, int count)
	{
#ifdef _ZIPRESOURCE
		if (Resource::zipArchive)
		{
			this->dataPosition += count;
			return zip::fread(this->cfile, buffer, count);
		}
#endif
		return this->_fread(buffer, count);
	}
	
	int Resource::_write(const void* buffer, int count)
	{
		throw FileNotWriteableException(this->filename);
	}

	bool Resource::_isOpen() const
	{
#ifdef _ZIPRESOURCE
		if (Resource::zipArchive)
		{
			return (this->archiveFile != NULL && this->cfile != NULL);
		}
#endif
		return this->_fisOpen();
	}
	
	int64_t Resource::_position() const
	{
#ifdef _ZIPRESOURCE
		if (Resource::zipArchive)
		{
			return zip::fposition(this->cfile);
		}
#endif
		return this->_fposition();
	}
	
	bool Resource::_seek(int64_t offset, SeekMode seekMode)
	{
#ifdef _ZIPRESOURCE
		if (Resource::zipArchive)
		{
			bool result = zip::fseek(this->cfile, offset, seekMode);
			this->dataPosition = zip::fposition(this->cfile);
			return result;
		}
#endif
		return this->_fseek(offset, seekMode);
	}
	
	bool Resource::exists(const String& filename, bool caseSensitive)
	{
#ifdef _ZIPRESOURCE
		if (Resource::zipArchive)
		{
			bool result = false;
			void* a = zip::open(NULL); // NULL, because this is a static function which will close the archive right after it is done
			if (a != NULL)
			{
				result = zip::fexists(a, Resource::makeFullPath(filename));
				if (!result && !caseSensitive)
				{
					String name = filename;
					String baseDir = ResourceDir::baseDir(name);
					String baseName = ResourceDir::baseName(name);
					Array<String> files = ResourceDir::files(baseDir);
					foreach (String, it, files)
					{
						if ((*it).lowered() == baseName.lowered())
						{
							name = ResourceDir::joinPath(baseDir, (*it));
							result = true;
							break;
						}
					}
				}
				zip::close(NULL, a);
			}
			return result;
		}
#endif
		return FileBase::_fexists(Resource::makeFullPath(filename), caseSensitive);
	}
	
	String Resource::hread(const String& filename, int count)
	{
		Resource file;
		file.open(filename);
		return file.read(count);
	}
	
	String Resource::hread(const String& filename, const String& delimiter)
	{
		Resource file;
		file.open(filename);
		return file.read(delimiter);
	}

	FileInfo Resource::hinfo(const String& filename)
	{
#ifdef _ZIPRESOURCE
		if (Resource::zipArchive)
		{
			FileInfo info;
			void* a = zip::open(NULL); // NULL, because this is a static function which will close the archive right after it is done
			if (a != NULL)
			{
				info = zip::finfo(a, Resource::makeFullPath(filename));
				zip::close(NULL, a);
				try // it's not that important to get access and creation time if this fails for some reason
				{
					FileInfo archive = File::hinfo(Resource::archive);
					info.creationTime = archive.creationTime;
					info.accessTime = archive.accessTime;
				}
				catch (_FileCouldNotOpenException& e)
				{
				}
			}
			return info;
		}
#endif
		return File::hinfo(Resource::makeFullPath(filename));
	}

	String Resource::makeFullPath(const String& filename)
	{
		String path = hdir::joinPath(Resource::cwd, filename);
		if (!Resource::zipArchive && Resource::archive != "" && Resource::archive != ".")
		{
			path = hdir::joinPath(Resource::archive, path);
		}
		return Dir::normalize(path);
	}

	Resource::Resource(const Resource& other)
	{
		throw ObjectCannotCopyException("hltypes::Resource");
	}

	Resource& Resource::operator=(Resource& other)
	{
		throw ObjectCannotAssignException("hltypes::Resource");
		return (*this);
	}

}

