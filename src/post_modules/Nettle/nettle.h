/* nettle.h
 *
 * Shared declarations for the various files. */

struct program;
extern struct program *nettle_hash_program;
extern struct program *hash_instance_program;
extern struct program *nettle_hash_program;

#define NO_WIDE_STRING(s)					\
do { if ((s)->size_shift)					\
       Pike_error("Bad argument, must be 8-bit string.");	\
} while(0)
