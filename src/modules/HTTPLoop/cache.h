struct cache_entry *aap_cache_lookup(char *s, ptrdiff_t len, 
				     char *h, ptrdiff_t hlen,
				     struct cache *c, int nl, 
				     struct cache_entry **p, 
				     size_t *hv);


void aap_free_cache_entry(struct cache *c, struct cache_entry *e,
		      struct cache_entry *prev, size_t b);

void simple_aap_free_cache_entry(struct cache *c, struct cache_entry *e);

void aap_cache_insert(struct cache_entry *ce, struct cache *c);

void aap_clean_cache();

void aap_enqueue_string_to_free( struct pike_string *s );
struct cache_entry *new_cache_entry( );

extern struct cache *first_cache;


