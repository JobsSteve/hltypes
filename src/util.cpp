/************************************************************************************\
* This source file is part of the High Level C++ types library                       *
* For latest info, see http://libhltypes.sourceforge.net/                            *
**************************************************************************************
* Copyright (c) 2010 Kresimir Spes, Boris Mikic, Domagoj Cerjan                      *
*                                                                                    *
* This program is free software; you can redistribute it and/or modify it under      *
* the terms of the BSD license: http://www.opensource.org/licenses/bsd-license.php   *
\************************************************************************************/
#include "harray.h"
#include "hstring.h"
#include "util.h"

int hrand(int min, int max)
{
#ifdef _WIN32
	return min + rand() * (max - min) / (RAND_MAX + 1);
#else
	return min + ((double) rand()) / ((double) RAND_MAX + 1) * (max - min);
#endif
}

int hrand(int max)
{
#ifdef _WIN32
	return rand() * max / (RAND_MAX + 1);
#else
	return (((double) rand()) / ((double) RAND_MAX + 1)) * max;
#endif
}

float hrandf(float min, float max)
{
#ifdef _WIN32
	return min + rand() * (max - min) / (RAND_MAX + 1);
#else
	return min + ((double) rand()) / ((double) RAND_MAX + 1) * (max - min);
#endif
}

float hrandf(float max)
{
#ifdef _WIN32
	return rand() * max / (RAND_MAX + 1);
#else
	return (((double) rand()) / ((double) RAND_MAX + 1)) * max;
#endif
}

hstr normalize_path(chstr path)
{
	harray<hstr> directories = path.replace('\\', '/').rtrim('/').split('/');
	harray<hstr> result;
	while (directories.size() > 0)
	{
		if (directories.front() == ".")
		{
			directories.pop_front();
		}
		else if (directories.front() == "..")
		{
			if (result.size() == 0)
			{
				result += directories.pop_front();
			}
			else if (result.back() == "..")
			{
				result += directories.pop_front();
			}
			else
			{
				result.pop_back();
				directories.pop_front();
			}
		}
		else
		{
			result += directories.pop_front();
		}
	}
	return result.join('/');
}

hstr unicode_to_utf8(unsigned int value)
{
	hstr result;
	if (value < 0x80)
	{
		result += (char)value;
	}
	else if (value < 0x800)
	{
		result += (char)(0xC0 | (value >> 6));
		result += (char)(0x80 | (value & 0x3F));
	}
	else
	{
		result += (char)(0xE0 | (value >> 12));
		result += (char)(0x80 | ((value >> 12) & 0x3F));
		result += (char)(0x80 | (value & 0x3F));
	}
    return result;
}
