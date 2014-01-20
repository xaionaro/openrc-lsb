/*
    lsb2rcconf - prints configuration for openrc's rc.conf based on LSB

Copyright (c) 2014, Dmitry Yu Okunev <dyokunev@ut.mephi.ru> 0x8E30679C
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 */

// If you have any question, I'll recommend you to ask irc.freenode.net#openrc


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

#include "xmalloc.h"

extern const char *lsb_v2s(const char *const lsb_virtual);

#define PATH_INSSERV "/etc/insserv.conf"

#define HT_SIZE_VSRV	(1<<8)

// virtual to value
struct hsearch_data ht_lsb_v2s = {0};
// value to virtual
struct hsearch_data ht_lsb_s2v = {0};

char *description  = NULL;
char *service_me;

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
	//free(services);

	return 0;
}

#define MAX_need	(1<<16)
#define MAX_use		MAX_need
#define MAX_provide	MAX_need
#define MAX_before	MAX_need

struct relation_arg {
	char 			**relation;
	int  			 *relation_count_p;
	int			  relation_max;
	struct hsearch_data 	 *relation_ht_p;
};

#define RELATION(relation_name)\
	char 			*relation_name[MAX_ ## relation_name + 1] = {NULL};\
	int			 relation_name ## _count 	= 0;\
	struct hsearch_data	 relation_name ## _ht		={0};\
	struct relation_arg	 relation_name ## _arg 		= \
		{relation_name, &relation_name ## _count, MAX_ ## relation_name, &relation_name ##_ht};

RELATION(need);
RELATION(use);
RELATION(provide);
RELATION(before);


void relation_add_oneservice(char *service, struct relation_arg *arg_p) {
	switch(*service) {
		case '+': {
			service++;
			if(arg_p->relation == need)	// Moving optional services from need to use
				arg_p = &use_arg;
		}
		default: {
			ENTRY entry = {service, NULL}, *entry_res_ptr;
			hsearch_r(entry, FIND, &entry_res_ptr, arg_p->relation_ht_p);
			if(entry_res_ptr != NULL)
				return;

			
			arg_p->relation[(*arg_p->relation_count_p)++] = service;
			hsearch_r(entry, ENTER, &entry_res_ptr, arg_p->relation_ht_p);
		}
	}
}

void relation_add(const char *const _service, struct relation_arg *arg_p) {
	if(!strcmp(_service, service_me)) {
		return;
	}

	char *service_buf = strdup(_service), *service = service_buf;

	if(*arg_p->relation_count_p >= arg_p->relation_max) {
		fprintf(stderr, "Too many records.\n");
		exit(EOVERFLOW);
	}
	ENTRY entry = {service, NULL}, *entry_res_ptr;

	hsearch_r(entry, FIND, &entry_res_ptr, arg_p->relation_ht_p);

	if(entry_res_ptr == NULL) {
		switch(*service) {
			case '$': {
				service++;
				const char *const services = lsb_v2s(service);
				if(services != NULL) {
					void relation_add_mark_real_service(char *service, void *arg) {
						relation_add_oneservice(service, arg_p);
					}
					services_foreach(services, (services_foreach_funct_t)relation_add_mark_real_service, NULL);
				}
				break;
			}
			default: {
				relation_add_oneservice(service, arg_p);
			}
		}
	}
}

#define RELATION_ADD(_relation, _services) {\
	char *services = strdupa(_services);\
	services_foreach(services, (services_foreach_funct_t)relation_add, &_relation ## _arg);\
}

static inline void NEED(const char *const _services) {
	RELATION_ADD(need, _services);
}
static inline void USE(const char *const _services) {
	RELATION_ADD(use, _services);
}
static inline void PROVIDE(const char *const _services) {
	RELATION_ADD(provide, _services);
}
static inline void BEFORE(const char *const _services) {
	RELATION_ADD(before, _services);
}

void syntax() {
	fprintf(stderr, "lsb2rcconf /path/to/init/script\n");
	exit(EINVAL);
}

static inline void lsb_x2x_add(char *key, char *data, struct hsearch_data *ht) {
	ENTRY entry = {key, data}, *entry_res_ptr;

	hsearch_r(entry, ENTER, &entry_res_ptr, ht);
}

void lsb_s2v_add(char *key, char *data) {
	return lsb_x2x_add(key, data, &ht_lsb_s2v);
}

void lsb_v2s_add(char *key, char *data) {
	return lsb_x2x_add(key, data, &ht_lsb_v2s);
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
			char *virtual    = strdup(&line_ptr[matches[1].rm_so]);	// TODO: free() this
			virtual[ matches[1].rm_eo - matches[1].rm_so] = 0;
			if(*virtual == '$')
				virtual++;

			char *services = strdup(&line_ptr[matches[2].rm_so]);	// TODO: free() this
			services[matches[2].rm_eo - matches[2].rm_so] = 0;

			// $virtual:	+service +service +service +service
			//			      services

			char services_unrolled[BUFSIZ], *services_unrolled_ptr = services_unrolled, *services_unrolled_end = &services_unrolled[BUFSIZ];

			void parse_insserv_parse_service(char *service, void *arg) {
				const char *services;
				switch(*service) {
					case '$':
						service++;
						services = lsb_v2s(service);
						break;
					default:
						services = service;
						break;
				}
				if(services == NULL)
					return;

				size_t len = strlen(services);

				if(&services_unrolled_ptr[len] >= services_unrolled_end) {
					fprintf(stderr, "Error: Too long field value.\n");
					exit(EOVERFLOW);
				}

				memcpy(services_unrolled_ptr, services, len);
				services_unrolled_ptr = &services_unrolled_ptr[len];
				*(services_unrolled_ptr++) = ' ';
			}

			services_foreach(services, (services_foreach_funct_t)parse_insserv_parse_service, NULL);
			*(--services_unrolled_ptr) = 0;

			lsb_v2s_add(virtual, strdup(services_unrolled));
		}
	}

	fclose(file_insserv);
}

void lsb_init() {
	// Hardcoded:
	ENTRY entries_v2s[] = {
		{"all",		"*"},
		{NULL,		NULL},
	};
	ENTRY entries_s2v[] = {
		{"*",		"all"},
		{NULL,		NULL},
	};


	// Initialization:
	hcreate_r(HT_SIZE_VSRV,	&ht_lsb_v2s);
	hcreate_r(HT_SIZE_VSRV,	&ht_lsb_s2v);
	hcreate_r(MAX_need,	&need_ht);
	hcreate_r(MAX_use,	&use_ht);
	hcreate_r(MAX_before,	&before_ht);
	hcreate_r(MAX_provide,	&provide_ht);

	ENTRY *entry_ptr, *entry_res_ptr;

	// Remembering hardcoded values:
	entry_ptr = entries_v2s;
	while(entry_ptr->key != NULL) {
		hsearch_r(*entry_ptr, ENTER, &entry_res_ptr, &ht_lsb_v2s);
		entry_ptr++;
	}
	entry_ptr = entries_s2v;
	while(entry_ptr->key != NULL) {
		hsearch_r(*entry_ptr, ENTER, &entry_res_ptr, &ht_lsb_s2v);
		entry_ptr++;
	}

	// Parse /etc/insserv.conf
	parse_insserv();
}

static inline const char *lsb_x2x(const char *const lsb_virtual, struct hsearch_data *ht) {
	ENTRY entry, *entry_ptr;

	entry.key = (char *)lsb_virtual;

	hsearch_r(entry, FIND, &entry_ptr, ht);
	if(entry_ptr != NULL)
		return entry_ptr->data;

	return NULL;
}

const char *lsb_v2s(const char *const lsb_virtual) {
	return lsb_x2x(lsb_virtual, &ht_lsb_v2s);
}

#if 0
const char *lsb_s2v(const char *const lsb_virtual) {
	return lsb_x2x(lsb_virtual, &ht_lsb_s2v);
}
#endif

char *lsb_expand(const char *const _services) {
	char *ret = xmalloc(BUFSIZ);
	char *ptr = ret, *ret_end = &ret[BUFSIZ];

	char *services = strdup(_services);

	void lsb_expand_parse_service(const char *service, void *arg) {
		switch(*service) {
			case '$': {
				const char * const service_expanded = lsb_v2s(&service[1]);

				if(service_expanded == NULL)
					return;

				service = service_expanded;
				break;
			}

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

	services_foreach(services, lsb_expand_parse_service, NULL);

	*(--ptr) = 0;

	free(services);
	return ret;
}

static void lsb_header_parse(const char *const header, char *value) {
	if(!strcmp(header, "provides")) {
		PROVIDE(value);
	} else
	if(!strcmp(header, "required-start")) {
		NEED(value);
	} else
/*	if(!strcmp(header, "required-stop")) {
	} else
	if(!strcmp(header, "default-start")) {
	} else
	if(!strcmp(header, "default-stop")) {
	} else*/
	if(!strcmp(header, "short-description")) {
		description = value;
	} else
/*	if(!strcmp(header, "description")) {
	} else*/
	if(!strcmp(header, "should-start")) {
		USE(value);
	} else
/*	if(!strcmp(header, "should-stop")) {
	} else*/
	if(!strcmp(header, "x-start-before")) {
		BEFORE(value);
	} else
/*	if(!strcmp(header, "x-stop-after")) {
	} else*/
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

	if(*provide != NULL) {
		printf("%s", "\tprovide");
		print_relation(provide);
		printf("\n");
	} else
		printf("\tprovide %s\n", service_me);
	if(*use != NULL) {
		printf("%s", "\tuse");
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
