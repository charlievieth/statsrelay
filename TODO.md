## TODO: statsrelay

1. Remove use of low-level buffer functions replace with high-level functions
  * Change `buffer_consume_until` => `buffer_consume_until_eol`
1. Logging and error handling - make sure this is never inlined
1. Hash map
  * Make allocation lazy
  * Check performance (specifically, around growing the hash - it should be lazy and incremental)

#### Nice to have: 
1. Add function and type annotations
1. Improve tests
1. Add additional compiler checks
  * Clean compile with all errors/warnings enabled
