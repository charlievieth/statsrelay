#include "json_config.h"
#include "log.h"

#include <stdbool.h>
#include <string.h>
#include <jansson.h>

static void json_error_log(const char* msg, json_error_t* error) {
    stats_error_log("JSON error %s: %s (%s) at line %d", msg, error->text, error->source, error->line);
}

static void init_proto_config(struct proto_config *protoc) {
    protoc->initialized = false;
    protoc->send_health_metrics = false;
    protoc->bind = NULL;
    protoc->enable_validation = true;
    protoc->enable_tcp_cork = true;
    protoc->max_send_queue = 134217728;
    protoc->auto_reconnect = false;
    protoc->reconnect_threshold = 1.0;
    protoc->ring = statsrelay_list_new();
    protoc->dupl = statsrelay_list_new();
    protoc->sstats = statsrelay_list_new();
}

static bool get_bool_orelse(const json_t* json, const char* key, bool def) {
    json_t* v = json_object_get(json, key);
    if (v == NULL)
        return def;
    if (json_is_null(v))
        return def;
    return json_is_true(v) ? true : false;
}

static double get_real_orelse(json_t* json, const char *key, double def) {
    json_t* v = json_object_get(json, key);
    if (v == NULL || json_is_null(v))
        return def;
    return json_real_value(v);
}

static char* get_string(const json_t* json, const char* key) {
    json_t* j = json_object_get(json, key);
    /**
     * Dont error if the key is optional
     */
    if (j == NULL || key == NULL)
        return NULL;
    if (!json_is_string(j)) {
        stats_error_log("Expected a string value for '%s' - ignoring config value", key);
        return NULL;
    }
    const char* str = json_string_value(j);
    if (str == NULL)
        return NULL;
    else
        return strdup(str);
}

static int get_int_orelse(const json_t* json, const char* key, int def) {
    json_t* v = json_object_get(json, key);
    if (v == NULL)
        return def;
    if (!json_is_number(v)) {
        stats_error_log("Expected an integer value for '%s' - using default of '%d'", key, def);
    }
    return (int)json_integer_value(v);
}

static void parse_server_list(const json_t* jshards, list_t ring) {
    if (jshards == NULL) {
        stats_error_log("no servers specified for routing");
        return;
    }

    const json_t* jserver = NULL;
    size_t index;
    json_array_foreach(jshards, index, jserver) {
        statsrelay_list_expand(ring);
        char* serverline = strdup(json_string_value(jserver));
        stats_log("adding server %s", serverline);
        stats_log("ring size %d", ring->size);
        ring->data[ring->size - 1] = serverline;
    }
}

static int parse_additional_config(const json_t* additional_config, struct proto_config* config,
                                   list_t target_list, const char *type) {
    if (additional_config != NULL) {
        struct additional_config* aconfig = calloc(1, sizeof(struct additional_config));
        aconfig->ring = statsrelay_list_new();
        aconfig->prefix = get_string(additional_config, "prefix");
        if (aconfig->prefix) {
            aconfig->prefix_len = strlen(aconfig->prefix);
        } else {
            aconfig->prefix_len = 0;
        }
        aconfig->suffix = get_string(additional_config, "suffix");
        if (aconfig->suffix) {
            aconfig->suffix_len = strlen(aconfig->suffix);
        } else {
            aconfig->suffix_len = 0;
        }
        aconfig->ingress_filter = get_string(additional_config, "input_filter");
        aconfig->ingress_blacklist = get_string(additional_config, "input_blacklist");

        aconfig->sampling_threshold = get_int_orelse(additional_config, "sampling_threshold", -1);
        aconfig->sampling_window = get_int_orelse(additional_config, "sampling_window", -1);

        aconfig->timer_sampling_threshold = get_int_orelse(additional_config, "timer_sampling_threshold", -1);
        aconfig->timer_sampling_window = get_int_orelse(additional_config, "timer_sampling_window", -1);
        aconfig->timer_flush_min_max = get_bool_orelse(additional_config, "timer_flush_min_max", false);
        aconfig->reservoir_size = get_int_orelse(additional_config, "reservoir_size", 100);

        aconfig->gauge_sampling_threshold = get_int_orelse(additional_config, "gauge_sampling_threshold", -1);
        aconfig->gauge_sampling_window = get_int_orelse(additional_config, "gauge_sampling_window", -1);

        // parse the cardinality limits (only applied when sampling is enabled)
        aconfig->max_counters = get_int_orelse(additional_config, "counter_cardinality", 10000);
        aconfig->max_timers = get_int_orelse(additional_config, "timer_cardinality", 10000);
        aconfig->max_gauges = get_int_orelse(additional_config, "gauge_cardinality", 10000);

        // run purge timer at hourly rate (default)
        aconfig->hm_key_expiration_frequency_in_seconds = get_int_orelse(additional_config, "hm_key_expiration_frequency", 3600);
        // purge entries older than a day! (default)
        aconfig->hm_key_ttl_in_seconds = get_int_orelse(additional_config, "hm_key_ttl", 21600);

        if ((aconfig->sampling_threshold > 0 || aconfig->timer_sampling_threshold > 0) && !config->enable_validation) {
            stats_error_log("enabling sampling requires turning on validation of the statsd packet format. sorry.");
            return -1;
        }

        stats_log("adding %s cluster with prefix '%s' and suffix '%s'",
                  type, aconfig->prefix, aconfig->suffix);

        const json_t* jdshards = json_object_get(additional_config, "shard_map");
        parse_server_list(jdshards, aconfig->ring);
        stats_log("added %s cluster with %d servers", type, aconfig->ring->size);

        statsrelay_list_expand(target_list);
        target_list->data[target_list->size - 1] = aconfig;
    }
    return 0;
}

static int parse_proto(json_t* json, struct proto_config* config) {
    config->initialized = true;
    config->enable_validation = get_bool_orelse(json, "validate", true);
    config->enable_tcp_cork = get_bool_orelse(json, "tcp_cork", true);
    config->auto_reconnect = get_bool_orelse(json, "auto_reconnect", false);

    char* jbind = get_string(json, "bind");
    if (jbind != NULL) {
        if (config->bind)
            free(config->bind);
        config->bind = jbind;
    }

    config->max_send_queue = get_int_orelse(json, "max_send_queue", 134217728);
    config->reconnect_threshold = get_real_orelse(json, "reconnect_threshold", 1.0);

    const json_t* jshards = json_object_get(json, "shard_map");
    /**
     * shard_map is optional
     */
    if (jshards != NULL) {
        parse_server_list(jshards, config->ring);
    }

    const json_t* duplicate = json_object_get(json, "duplicate_to");
    if (json_is_object(duplicate)) {
        if (parse_additional_config(duplicate, config, config->dupl, "duplicate"))
            return -1;
    } else if (json_is_array(duplicate)) {
        size_t index;
        const json_t* duplicate_v;
        json_array_foreach(duplicate, index, duplicate_v) {
            parse_additional_config(duplicate_v, config, config->dupl, "duplicate");
        }
    }

    const json_t* health_metrics_json = json_object_get(json, "health_metrics_to");
    if (health_metrics_json != NULL) {
        if (json_is_object(health_metrics_json)) {
            parse_additional_config(health_metrics_json, config, config->sstats, "monitoring");
            config->send_health_metrics = true;
        } else {
            stats_error_log("health_metrics_to option does not accept arrays");
            return -1;
        }
    }
    return 0;
}

struct config* parse_json_config(FILE* input) {
    struct config *config = malloc(sizeof(struct config));
    if (config == NULL) {
        stats_error_log("malloc() error");
        return NULL;
    }

    init_proto_config(&config->statsd_config);
    config->statsd_config.bind = strdup("127.0.0.1:8125");

    json_error_t jerror;
    json_t* json = json_loadf(input, 0, &jerror);
    if (json == NULL) {
        json_error_log("loading", &jerror);
        goto parse_error;
    }

    if (!json_is_object(json)) {
        stats_error_log("Config needs to be a json object");
        goto parse_error;
    }

    json_t* statsd_json = json_object_get(json, "statsd");
    if (statsd_json) {
        if (parse_proto(statsd_json, &config->statsd_config) < 0) {
            goto parse_error;
        }
    }

    json_decref(json);
    return config;

    parse_error:
    if (json != NULL) {
        json_decref(json);
    }
    destroy_json_config(config);
    return NULL;
}

static void destroy_proto_config(struct proto_config *config) {
    if (config->ring->size > 0) {
        statsrelay_list_destroy_full(config->ring);
    }
    for (int i = 0; i < config->dupl->size; i++) {
        struct additional_config* dupl = (struct additional_config*)config->dupl->data[i];
        if (dupl->prefix)
            free(dupl->prefix);
        if (dupl->suffix)
            free(dupl->suffix);
        statsrelay_list_destroy_full(dupl->ring);
    }
    for (int i = 0; i < config->sstats->size; i++) {
        struct additional_config* sstats = (struct additional_config*)config->sstats->data[i];
        if (sstats->prefix)
            free(sstats->prefix);
        if (sstats->suffix)
            free(sstats->suffix);
        statsrelay_list_destroy_full(sstats->ring);
    }
    free(config->bind);
}

void destroy_json_config(struct config *config) {
    if (config != NULL) {
        destroy_proto_config(&config->statsd_config);
        free(config);
    }
}
