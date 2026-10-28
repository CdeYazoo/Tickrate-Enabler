#ifndef __EXCEPTIONS_H__
#define __EXCEPTIONS_H__

#include <cstdio>
#include <cstdarg>
#include <cstring>

#if defined(_MSC_VER) && _MSC_VER < 1900
#define PATCHEXC_VSNPRINTF _vsnprintf
#else
#define PATCHEXC_VSNPRINTF vsnprintf
#endif

// Owns a copy of its message so it can carry runtime details (addresses, bytes, offsets).
class PatchException
{
public:
	PatchException(const char * str)
	{
		m_str[0] = '\0';
		if(str)
		{
			strncpy(m_str, str, sizeof(m_str) - 1);
			m_str[sizeof(m_str) - 1] = '\0';
		}
	}

	static PatchException Format(const char * fmt, ...)
	{
		PatchException e(NULL);
		va_list args;
		va_start(args, fmt);
		PATCHEXC_VSNPRINTF(e.m_str, sizeof(e.m_str) - 1, fmt, args);
		va_end(args);
		e.m_str[sizeof(e.m_str) - 1] = '\0';
		return e;
	}

	const char * GetDescription() const { return m_str; }
private:
	char m_str[512];
};

#endif
