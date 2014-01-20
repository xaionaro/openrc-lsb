/*
    lsb2rcconf - prints configuration for openrc's rc.conf based on LSB

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

#define _GNU_SOURCE	// hsearch_r()

#include <stdio.h>	// fprintf()
#include <stdlib.h>	// exit()
#include <string.h>	// strcmp()
#include <errno.h>	// ENVAL
#include <libgen.h>	// basename()
#include <search.h>	// hsearch_r()
#include <sys/types.h>	// regexec()
#include <regex.h>	// regexec()
#include <unistd.h>	// access()

//#include "malloc.h"

#define PATH_INSSERV "/etc/insserv.conf"

#define HT_SIZE_VSRV	(1<<8)

// macro to value
struct hsearch_data ht_lsb_m2v = {0};
// value to macro
struct hsearch_data ht_lsb_v2m = {0};

void syntax() {
	fprintf(stderr, "lsb2rcconf /path/to/init/script\n");
	exit(EINVAL);
}

static inline void lsb_x2x_add(char *key, char *data, struct hsearch_data *ht) {
	ENTRY entry = {key, data}, *entry_res_ptr;
//	printf("%s: %s\n", key, data);

	hsearch_r(entry, ENTER, &entry_res_ptr, ht);
}

static inline void lsb_v2m_add(char *key, char *data) {
	return lsb_x2x_add(key, data, &ht_lsb_v2m);
}

static inline void lsb_m2v_add(char *key, char *data) {
	return lsb_x2x_add(key, data, &ht_lsb_m2v);
}

static const int xregcomp(regex_t *preg, const char *regex, int cflags) {
	int r;
	if((r=regcomp(preg, regex, cflags))) {
		char buf[BUFSIZ];
		regerror(r, preg, buf, BUFSIZ);
		fprintf(stderr, "Error: Cannot compile regex: %i: %s\n", r, buf);
		exit(r);
	}

	return r;
}

void parse_insserv() {
	FILE *file_insserv = fopen(PATH_INSSERV, "r");
	if(file_insserv == NULL) {
		fprintf(stderr, "Error: Unable to read \""PATH_INSSERV"\": %i: %s\n", errno, strerror(errno));
		exit(errno);
	}

	regex_t regex;

	xregcomp(&regex, "^(\\$\\S+)\\s*(\\S?.*)$", REG_EXTENDED);

	size_t line_len;
	size_t line_size = 0;
	char *line_ptr = NULL;
	while((line_len = getline(&line_ptr, &line_size, file_insserv))!=-1) {
		regmatch_t matches[4] = {{0}};

		if(!line_len)
			continue;

		if(*line_ptr == '#')
			continue;

		line_ptr[--line_len] = 0;	// cutting-off '\n'

		if(!regexec(&regex, line_ptr, 3, matches, 0)) {
			char *macro    = strdup(&line_ptr[matches[1].rm_so]);	// TODO: free() this
			macro[   matches[1].rm_eo - matches[1].rm_so] = 0;

			char *services = strdup(&line_ptr[matches[2].rm_so]);	// TODO: free() this
			services[matches[2].rm_eo - matches[2].rm_so] = 0;

			// $macro:	service service service service
			//			   services

			// splitting services
			char *strtok_saveptr = NULL;
			char *service = strtok_r(services, " \t", &strtok_saveptr);
			do {
				// remembering: service => macro
				lsb_v2m_add(service, macro);
			} while((service = strtok_r(NULL, " \t", &strtok_saveptr)));
		}
	}

	fclose(file_insserv);
}

void lsb_init() {
	ENTRY entries[] = {
		{"$local_fs",	"mountall mountall-bootclean umountfs"},
		{"$remote_fs",	"mountnfs mountnfs-bootclean umountnfs sendsigs"},
		{"$network",	"networking ifupdown"},
		{"$syslog",	"syslog"},
		{"$time",	"hwclock"},
		{"$portmap",	"rpcbind"},
		{NULL,		NULL}
	};

	hcreate_r(6,		&ht_lsb_m2v);
	hcreate_r(HT_SIZE_VSRV,	&ht_lsb_v2m);

	ENTRY *entry_ptr = entries, *entry_res_ptr;
	while(entry_ptr->key != NULL) {
		hsearch_r(*entry_ptr, ENTER, &entry_res_ptr, &ht_lsb_m2v);
		entry_ptr++;
	}

	parse_insserv();
}

static const char *lsb_x2x(const char *lsb_macro, struct hsearch_data *ht) {
	ENTRY entry, *entry_ptr;

	entry.key = (char *)lsb_macro;

	hsearch_r(entry, FIND, &entry_ptr, ht);
	if(entry_ptr != NULL)
		return entry_ptr->data;

	return NULL;
}

static const char *lsb_m2v(const char *lsb_macro) {
	return lsb_x2x(lsb_macro, &ht_lsb_m2v);
}

static const char *lsb_v2m(const char *lsb_macro) {
	return lsb_x2x(lsb_macro, &ht_lsb_v2m);
}

static void lsb_header_parse(const char *const header, char *value) {
	printf("%s: %s\n", header, value);
	return;
}

void lsb_parse(const char *initdscript) {
	FILE *file_initdscript = fopen(initdscript, "r");

	if(file_initdscript == NULL) {
		fprintf(stderr, "Error: Unable to read \"%s\": %i: %s\n", 
			initdscript, errno, strerror(errno));
		exit(errno);
	}

	regex_t regex_start, regex_header, regex_end;

	xregcomp(&regex_start,  "^### BEGIN INIT INFO\\s*$",     REG_EXTENDED);
	xregcomp(&regex_header, "^#\\s*(\\S+):\\s\\s*(.*\\S)\\s*$", REG_EXTENDED);
	xregcomp(&regex_end,    "^### END INIT INFO\\s*$",       REG_EXTENDED);

	enum lsb_parse_state {
		LP_STARTED = 0,
		LP_PARSING_LSB = 1
	};

	enum lsb_parse_state state = LP_STARTED;
	size_t line_len;
	size_t line_size = 0;
	char *line_ptr = NULL;
	while((line_len = getline(&line_ptr, &line_size, file_initdscript))!=-1) {
		if(!line_len)
			continue;

		line_ptr[--line_len] = 0;	// cutting-off '\n'

		switch(state) {
			case LP_STARTED: {
				if(!regexec(&regex_start, line_ptr, 0, NULL, 0)) {
					state = LP_PARSING_LSB;
					continue;
				}
				break;
			}
			case LP_PARSING_LSB: {
				regmatch_t matches[4] = {{0}};

				if(!regexec(&regex_header, line_ptr, 3, matches, 0)) {
					char *header = strdup(&line_ptr[matches[1].rm_so]);
					header[matches[1].rm_eo - matches[1].rm_so] = 0;

					char *value  = strdup(&line_ptr[matches[2].rm_so]);	// TODO: free() this
					value[ matches[2].rm_eo - matches[2].rm_so] = 0;

					lsb_header_parse(header, value);

					free(header);
				} else
				if(!regexec(&regex_end, line_ptr, 0, NULL, 0))
					goto l_lsb_parse_end;
				break;
			}
		}
	}

l_lsb_parse_end:

	fclose(file_initdscript);
	return;
}

void lsb_print_orc() {
	return;
}

int main(int argc, char *argv[]) {
	if(argc <= 1)
		syntax();

	const char *initdscript = argv[1];
	const char *service	= basename(strdup(initdscript));

	if(access(initdscript, R_OK)) {
		fprintf(stderr, "Cannot get read access to file \"%s\": %i: %s\n",
			initdscript, errno, strerror(errno));
		exit(errno);
	}

	lsb_init();

	lsb_parse(initdscript);

	lsb_print_orc();

	exit(0);
}
