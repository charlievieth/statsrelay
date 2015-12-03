#ifndef STATSRELAY_YAML_CONFIG_H
#define STATSRELAY_YAML_CONFIG_H

#include "./list.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

struct duplicate_config {
	const char* prefix;
	const char* suffix;
	list_t ring;
};

struct proto_config {
	bool initialized;
	char *bind;
	bool enable_validation;
	bool enable_tcp_cork;
	uint64_t max_send_queue;
	list_t ring;
	struct duplicate_config dupl;
};

struct config {
	struct proto_config statsd_config;
	struct proto_config carbon_config;
};


static const char default_config[] = "/etc/statsrelay.yaml";

struct config* parse_config(FILE *input);
struct config* parse_json_config(FILE *input);

// release the memory associated with a config
void destroy_config(struct config *);

#endif  // STATSRELAY_YAML_CONFIG_H
