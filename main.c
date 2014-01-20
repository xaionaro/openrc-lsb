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

#include "malloc.h"

#define PATH_INSSERV "/etc/insserv.conf"

#define HT_SIZE_VSRV	(1<<8)

// http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=714039
// should be removed, when the bug will be fixed
#define BUGFIX_DEBIAN_714039

// macro to value
struct hsearch_data ht_lsb_m2v = {0};
// value to macro
struct hsearch_data ht_lsb_v2m = {0};

char *description  = NULL;
char *service_me;

#define MAX_need	(1<<16)
#define MAX_use		MAX_need
#define MAX_provide	MAX_need
#define MAX_before	MAX_need

char    *need[MAX_need+1]    = {NULL};
char     *use[MAX_use+1]     = {NULL};
char *provide[MAX_provide+1] = {NULL};
char  *before[MAX_before+1]  = {NULL};

int need_count=0, use_count=0, provide_count=0, before_count=0;

#define RELATION(relation, service) {\
	if(relation ## _count >= MAX_ ## relation) {\
		fprintf(stderr, "Too many records.\n");\
		exit(EOVERFLOW);\
	}\
	relation[relation ## _count++] = service;\
}

static inline void NEED(char *const service) {
	RELATION(need, service);
}
static inline void USE(char *const service) {
	RELATION(use, service);
}
static inline void PROVIDE(char *const service) {
	RELATION(provide, service);
}
static inline void BEFORE(char *const service) {
	RELATION(before, service);
}

void syntax() {
	fprintf(stderr, "lsb2rcconf /path/to/init/script\n");
	exit(EINVAL);
}

typedef void (*services_foreach_funct_t)(const char *const service, void *arg);

static inline int services_foreach(const char *const _services, services_foreach_funct_t funct, void *arg) {
	if(_services == NULL) {
		fprintf(stderr, "Internal error (#0)\n");
		exit(-1);
	}

	char *services = strdup(_services);
	char *strtok_saveptr = NULL;
	char *service = strtok_r(services, " \t", &strtok_saveptr);
	do {
		funct(service, arg);
	} while((service = strtok_r(NULL, " \t", &strtok_saveptr)));
	free(services);

	return 0;
}

static inline void lsb_x2x_add(char *key, char *data, struct hsearch_data *ht) {
	ENTRY entry = {key, data}, *entry_res_ptr;
//	printf("%s: %s\n", key, data);

	hsearch_r(entry, ENTER, &entry_res_ptr, ht);
}

void lsb_v2m_add(char *key, char *data) {
	return lsb_x2x_add(key, data, &ht_lsb_v2m);
}

void lsb_m2v_add(char *key, char *data) {
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

			services_foreach(services, (services_foreach_funct_t)lsb_v2m_add, macro);
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

static const char *lsb_x2x(const char *const lsb_macro, struct hsearch_data *ht) {
	ENTRY entry, *entry_ptr;

	entry.key = (char *)lsb_macro;

	hsearch_r(entry, FIND, &entry_ptr, ht);
	if(entry_ptr != NULL)
		return entry_ptr->data;

	return NULL;
}

static const char *lsb_m2v(const char *const lsb_macro) {
	return lsb_x2x(lsb_macro, &ht_lsb_m2v);
}

static const char *lsb_v2m(const char *const lsb_macro) {
	return lsb_x2x(lsb_macro, &ht_lsb_v2m);
}

#ifdef BUGFIX_DEBIAN_714039
static inline int isall(const char *const services, char **ret) {
	if(*services == 0) {
		fprintf(stderr, "Internal error (#1)\n");
		exit(-1);
	}

	int rc = 0;
	*ret = xmalloc(BUFSIZ);
	char *ptr = *ret, *ret_end = &(*ret)[BUFSIZ];

	void isall_parse_service(const char *const service, void *arg) {
		if(!strcmp(service, "*")) {
			rc = 1;
			return;
		}

		size_t service_len = strlen(service);
		if(&ptr[service_len+2] > ret_end) {
			fprintf(stderr, "Error: Services list line is too long: %s\n", services);
			exit(EMSGSIZE);
		}
		memcpy(ptr, service, service_len);
		ptr += service_len;
		*(ptr++) = ' ';
	}

	services_foreach(services, isall_parse_service, NULL);

	*(--ptr) = 0;

	return rc;
}
#endif

char *lsb_expand(const char *const _services) {
	char *ret = xmalloc(BUFSIZ);
	char *ptr = ret, *ret_end = &ret[BUFSIZ];

	char *services = strdup(_services);

	void lsb_expand_parse_service(const char *service, void *arg) {
		const char * const service_expanded = lsb_m2v(service);

		if(service_expanded != NULL)
			service = service_expanded;

		size_t service_len = strlen(service);
		if(&ptr[service_len+2] > ret_end) {
			fprintf(stderr, "Error: Services list line is too long: %s\n", services);
			exit(EMSGSIZE);
		}
		memcpy(ptr, service, service_len);
		ptr += service_len;
		*(ptr++) = ' ';
	}

	services_foreach(services, lsb_expand_parse_service, NULL);

	*(--ptr) = 0;

	free(services);
	return ret;
}

void lsb_header_provide(const char *const service, void *arg) {
	const char *const macro = lsb_v2m(service);
	if(macro != NULL) {
		char *services_expanded = lsb_expand(macro);
		if(services_expanded != NULL);
			PROVIDE(services_expanded);
	}

	char *services_expanded = lsb_expand(service);
	if(services_expanded != NULL);
		PROVIDE(services_expanded);

	if(strcmp(service, service_me))
		PROVIDE(strdup(service));

	return;
}

static void lsb_header_parse(const char *const header, char *value) {
//	printf("%s: %s\n", header, value);

	if(!strcmp(header, "provides")) {
		services_foreach(value, lsb_header_provide, NULL);
	} else
	if(!strcmp(header, "required-start")) {
		char *services_expanded = lsb_expand(value);
#ifdef BUGFIX_DEBIAN_714039
		char *services_fixed;
		if(isall(value, &services_fixed)) {
			NEED(services_fixed);
			USE("*");
			free(services_expanded);
		} else {
#endif
			NEED(services_expanded);
#ifdef BUGFIX_DEBIAN_714039
			free(services_fixed);
		}
#endif
	} else
/*
	if(!strcmp(header, "required-stop")) {
	} else
	if(!strcmp(header, "default-start")) {
	} else
	if(!strcmp(header, "default-stop")) {
	} else
*/
	if(!strcmp(header, "short-description")) {
		description = value;
	} else
/*
	if(!strcmp(header, "description")) {
	} else
*/
	if(!strcmp(header, "should-start")) {
		char *services_expanded = lsb_expand(value);
		if(services_expanded != NULL)
			USE(services_expanded);
	} else
/*
	if(!strcmp(header, "should-stop")) {
	} else
*/
	if(!strcmp(header, "x-start-before")) {
		char *services_expanded = lsb_expand(value);
		if(services_expanded != NULL)
			BEFORE(services_expanded);
	} else
/*
	if(!strcmp(header, "x-stop-after")) {
	} else
*/
	{}

	return;
}

char *strtolower(char *_str) {
	char *str = _str;
	while(*str) *(str++) |= 0x20;
	return _str;
}

void lsb_parse(const char *initdscript) {
	FILE *file_initdscript = fopen(initdscript, "r");

	if(file_initdscript == NULL) {
		fprintf(stderr, "Error: Unable to read \"%s\": %i: %s\n", 
			initdscript, errno, strerror(errno));
		exit(errno);
	}

	regex_t regex_start, regex_header, regex_end;

	xregcomp(&regex_start,  "^### BEGIN INIT INFO\\s*$",        REG_EXTENDED);
	xregcomp(&regex_header, "^#\\s*(\\S+):\\s\\s*(.*\\S)\\s*$", REG_EXTENDED);
	xregcomp(&regex_end,    "^### END INIT INFO\\s*$",          REG_EXTENDED);

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

					lsb_header_parse(strtolower(header), value);

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

static inline void print_relation(char **relation) {
	char **ptr = relation;
	while(*ptr != NULL) {
		printf(" %s", *ptr);
		ptr++;
	}

	return;
}

void lsb_print_orc() {

	if(description != NULL)
		printf("description=\"%s\"\n\n", description);

	printf("%s", "depend () {\n");

	if(*use != NULL) {
		printf("%s", "\tuse ");
		print_relation(use);
		printf("\n");
	}
	if(*need != NULL) {
		printf("%s", "\tneed");
		print_relation(need);
		printf("\n");
	}
	if(*before != NULL) {
		printf("%s", "\tbefore");
		print_relation(before);
		printf("\n");
	}
	if(*provide != NULL) {
		printf("%s", "\tprovide");
		print_relation(provide);
		printf("\n");
	}
	printf("}\n");

	return;
}

int main(int argc, char *argv[]) {
	if(argc <= 1)
		syntax();

	const char *initdscript = argv[1];
	service_me		= basename(strdup(initdscript));

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
