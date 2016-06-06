/*
    lsb2rcconf - prints configuration for openrc's rc.conf based on LSB

Copyright (c) 2014-2016, Dmitry Yu Okunev <dyokunev@ut.mephi.ru> 0x8E30679C
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 */

/*
   Below functions are already in src/includes/helpers.h of OpenRC.
   So, if the helpers.h is loaded, then __HELPERS_H__ will be set
   and we don't need to load own implementation.
 */
#ifndef __HELPERS_H__
#define MALLOC malloc
#define REALLOC realloc
#define CALLOC calloc
#define STRDUP strdup

#include <stdio.h>	/* fprintf()	*/
#include <stdlib.h>	/* size_t	*/
#include <string.h>	/* strerror()	*/
#include <errno.h>	/* errno	*/

static inline void *xmalloc(size_t size)
{
#ifdef PARANOID
	size++;	/* Just in case	*/
#endif

	void *ret = MALLOC(size);

	if (ret == NULL) {
		fprintf(stderr, "xmalloc(%li): Cannot allocate memory (#%i: %s).\n", size, errno, strerror(errno));
		exit(errno);
	}

#ifdef PARANOID
	memset(ret, 0, size);
#endif
	return ret;
}

static inline void *xcalloc(size_t nmemb, size_t size)
{
#ifdef PARANOID
	nmemb++; /* Just in case	*/
	size++;	 /* Just in case	*/
#endif

	void *ret = CALLOC(nmemb, size);

	if (ret == NULL) {
		fprintf(stderr, "xcalloc(%li): Cannot allocate memory (#%i: %s).\n", size, errno, strerror(errno));
		exit(errno);
	}

/*	memset(ret, 0, nmemb*size);	/ * Just in case */
	return ret;
}

static inline void *xrealloc(void *oldptr, size_t size)
{
#ifdef PARANOID
	size++;	/* Just in case */
#endif

	void *ret = REALLOC(oldptr, size);

	if (ret == NULL) {
		fprintf(stderr, "xrealloc(%p, %li): Cannot reallocate memory (#%i: %s).\n", oldptr, size, errno, strerror(errno));
		exit(errno);
	}

	return ret;
}

static inline char *xstrdup(const char *s) {
	char *ret = STRDUP(s);

	if (ret == NULL) {
		fprintf(stderr, "xstrdup(%p): Cannot duplicate string (#%i: %s).\n", s, errno, strerror(errno));
		exit(errno);
	}

	return ret;
}

#endif /* ifndef __HELPERS_H__ */
