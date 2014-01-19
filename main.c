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

//#include "malloc.h"

#define PATH_INSSERV "/etc/insserv.conf"

#define HT_SIZE_VSRV	(1<<8)

struct hsearch_data ht_lsb_macro_default = {0};
struct hsearch_data ht_lsb_macro         = {0};

void syntax() {
	fprintf(stderr, "lsb2rcconf /path/to/init/script\n");
	exit(EINVAL);
}

/*
static const char *lsb2orc(const char *lsb_macro) {
	if(!strcmp(lsb_macro, "$local_fs"))
		return "mountall mountall-bootclean umountfs";
	else
	if(!strcmp(lsb_macro, "$remote_fs"))
		return "mountnfs mountnfs-bootclean umountnfs sendsigs";
	else
	if(!strcmp(lsb_macro, "$network"))
		return "networking ifupdown";
	else
	if(!strcmp(lsb_macro, "$syslog"))
		return "syslog";
	else
	if(!strcmp(lsb_macro, "$time"))
		return "hwclock";
	else
	if(!strcmp(lsb_macro, "$portmap"))
		return "rpcbind";
}
*/

static inline void lsb2orc_add(char *key, char *data) {
	ENTRY entry = {key, data}, *entry_res_ptr;

	hsearch_r(entry, ENTER, &entry_res_ptr, &ht_lsb_macro);
}

void lsb2orc_parse_insserv() {
	FILE *file_insserv = fopen(PATH_INSSERV, "r");
	if(file_insserv == NULL) {
		fprintf(stderr, "Error: Unable to read \""PATH_INSSERV"\": %i: %s\n", errno, strerror(errno));
		exit(errno);
	}

	regex_t regex;
	int r;
	if((r=regcomp(&regex, "^(\\$\\S+)\\s*(\\S?.*)$", REG_EXTENDED))) {
		char buf[BUFSIZ];
		size_t errsize = regerror(r, &regex, buf, BUFSIZ);
		fprintf(stderr, "Error: Cannot compile regex: %i: %s\n", r, buf);
		exit(r);
	}

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
			char *key  = strdup(&line_ptr[matches[1].rm_so]);	// TODO: free() this
			key[ matches[1].rm_eo] = 0;
			char *data = strdup(&line_ptr[matches[2].rm_so]);	// TODO: free() this
			data[matches[2].rm_eo] = 0;
			lsb2orc_add(key, data);
		}
	}

	fclose(file_insserv);
}

void lsb2orc_init() {
	ENTRY entries[] = {
		{"$local_fs",	"mountall mountall-bootclean umountfs"},
		{"$remote_fs",	"mountnfs mountnfs-bootclean umountnfs sendsigs"},
		{"$network",	"networking ifupdown"},
		{"$syslog",	"syslog"},
		{"$time",	"hwclock"},
		{"$portmap",	"rpcbind"},
		{NULL,		NULL}
	};

	hcreate_r(6,		&ht_lsb_macro_default);
	hcreate_r(HT_SIZE_VSRV,	&ht_lsb_macro);

	ENTRY *entry_ptr = entries, *entry_res_ptr;
	while(entry_ptr->key != NULL) {
		hsearch_r(*entry_ptr, ENTER, &entry_res_ptr, &ht_lsb_macro_default);
		entry_ptr++;
	}

	lsb2orc_parse_insserv();
}

static const char *lsb2orc(const char *lsb_macro) {
	ENTRY entry, *entry_ptr;

	entry.key = (char *)lsb_macro;

	hsearch_r(entry, FIND, &entry_ptr, &ht_lsb_macro);
	if(entry_ptr != NULL)
		return entry_ptr->data;

	hsearch_r(entry, FIND, &entry_ptr, &ht_lsb_macro_default);
	if(entry_ptr != NULL)
		return entry_ptr->data;

	return NULL;
}

int main(int argc, char *argv[]) {
	if(argc <= 1)
		syntax();

	const char *initdscript = argv[1];
	const char *name	= basename(strdup(initdscript));

	lsb2orc_init();

	exit(0);
}
