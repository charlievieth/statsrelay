#include <string.h>

struct metric { const char *name; const int metric; };

#define METRIC_TOTAL_KEYWORDS 6
#define METRIC_MIN_WORD_LENGTH 1
#define METRIC_MAX_WORD_LENGTH 2
#define METRIC_MIN_HASH_VALUE 1
#define METRIC_MAX_HASH_VALUE 6
/* maximum key range = 6, duplicates = 0 */

static inline unsigned int hash_metric_name(const char *str, size_t len)
{
    static const unsigned char asso_values[] = {
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 5,
        7, 7, 7, 4, 3, 7, 7, 1, 7, 0,
        7, 7, 7, 7, 7, 0, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7
    };
    return len + asso_values[(unsigned char)str[0]];
}

const struct metric *lookup_metric(const char *str, size_t len)
{
    static const unsigned char lengthtable[] = {
        0,  1,  2,  2,  1,  1,  1
    };
    static const struct metric wordlist[] = {
        {"", 0},
        {"s",  5},
        {"ms", 1},
        {"kv", 2},
        {"h",  4},
        {"g",  3},
        {"c",  0}
    };

    if (len <= METRIC_MAX_WORD_LENGTH && len >= METRIC_MIN_WORD_LENGTH) {
        unsigned int key = hash_metric_name(str, len);
        if (key <= METRIC_MAX_HASH_VALUE && len == lengthtable[key]) {
            const char *s = wordlist[key].name;
            if (*str == *s && !memcmp(str + 1, s + 1, len - 1)) {
                return &wordlist[key];
            }
        }
    }
    return NULL;
}
