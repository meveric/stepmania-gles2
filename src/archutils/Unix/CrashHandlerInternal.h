#ifndef CRASH_HANDLER_INTERNAL_H
#define CRASH_HANDLER_INTERNAL_H

#include "Backtrace.h"
#define BACKTRACE_MAX_SIZE 128

struct CrashData
{
	enum CrashType
	{
		/* We received a fatal signal.  si and uc are valid. */
		SIGNAL,

		/* We're forcing a crash (eg. failed ASSERT). */
		FORCE_CRASH,
	} type;

	/* Everything except FORCE_CRASH_THIS_THREAD: */
	enum { MAX_BACKTRACE_THREADS = 32 };
	const void *BacktracePointers[MAX_BACKTRACE_THREADS][BACKTRACE_MAX_SIZE];
	char m_ThreadName[MAX_BACKTRACE_THREADS][128];

	/* SIGNAL only: */
	int signal;
	siginfo_t si;
	
	/* FORCE_CRASH_THIS_THREAD and FORCE_CRASH_DEADLOCK only: */
	char reason[256];
};

#define CHILD_MAGIC_PARAMETER "--private-do-crash-handler"

/* These can return a pointer to static memory. Copy the returned string if you wish to save it. */
const char *itoa( unsigned n );
const char *SignalName( int signo );
const char *SignalCodeName( int signo, int code );

#endif

/*
 * (c) 2003-2006 Glenn Maynard
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, and/or sell copies of the Software, and to permit persons to
 * whom the Software is furnished to do so, provided that the above
 * copyright notice(s) and this permission notice appear in all copies of
 * the Software and that both the above copyright notice(s) and this
 * permission notice appear in supporting documentation.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF
 * THIRD PARTY RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS
 * INCLUDED IN THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

