/*
    clsync - file tree sync utility based on fanotify and inotify

    Copyright (C) 2013  Dmitry Yu Okunev <dyokunev@ut.mephi.ru> 0x8E30679C

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>	// fprintf()
#include <stdlib.h>	// size_t
#include <string.h>	// strerror()
#include <errno.h>	// errno

#include "malloc.h"

void *xmalloc(size_t size) {
#ifdef PARANOID
	size++;	// Just in case
#endif

	void *ret = malloc(size);

	if(ret == NULL) {
		fprintf(stderr, "xmalloc(%li): Cannot allocate memory (#%i: %s).\n", size, errno, strerror(errno));
		exit(errno);
	}

#ifdef PARANOID
	memset(ret, 0, size);
#endif
	return ret;
}

void *xcalloc(size_t nmemb, size_t size) {
#ifdef PARANOID
	nmemb++; // Just in case
	size++;	 // Just in case
#endif

	void *ret = calloc(nmemb, size);

	if(ret == NULL) {
		fprintf(stderr, "xcalloc(%li): Cannot allocate memory (#%i: %s).\n", size, errno, strerror(errno));
		exit(errno);
	}

//	memset(ret, 0, nmemb*size);	// Just in case
	return ret;
}

void *xrealloc(void *oldptr, size_t size) {
#ifdef PARANOID
	size++;	// Just in case
#endif

	void *ret = realloc(oldptr, size);

	if(ret == NULL) {
		fprintf(stderr, "xrealloc(%p, %li): Cannot reallocate memory (#%i: %s).\n", oldptr, size, errno, strerror(errno));
		exit(errno);
	}

	return ret;
}
