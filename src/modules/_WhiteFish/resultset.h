typedef struct result_set
{
  INT32 num_docs;
  struct hits {
    INT32 doc_id;
    INT32 ranking;
  } hits[1];
} ResultSet;
#ifdef PIKE_DEBUG
extern struct program *resultset_program;
struct object *wf_not_resultset( struct object *o );
# define WF_RESULTSET(X) ((X&&get_storage(X,resultset_program))?X:wf_not_resultset(X))
#else
# define WF_RESULTSET(X) X
#endif

#define WS_RESULTSET_SET(o) (((struct result_set_p*)(WS_RESULTSET(o)->storage))->d)
/* Returns the ResultSet member for the given resultset class */

void init_resultset_program(void);
void exit_resultset_program(void);


struct object *wf_resultset_new( );
/* Create a new (empty) set */

void wf_resultset_free ( struct object *o );
/* Free a set */

void wf_resultset_add  ( struct object *o, int document, int weight );
/* Add a new entry to the resultset. document _must_ be larger than
 * the last added document if and, or, sub and intersect will be used
 * on this set later on.
 */

void wf_resultset_avg_ranking( struct object *o, int ind, int weight );
/* Update the ranking element of the given element so that it is
 * (old_value+weight)/2.
 *
 * If ind is -1, the last added element will be updated.
 */

void wf_resultset_add_ranking( struct object *o, int ind, int weight );
/* Update the ranking element of the given element so that it is
 * (old_value+weight).
 *
 * If ind is -1, the last added element will be updated.
 */

void wf_resultset_clear( struct object *o );
/* Remove all entries from the set. */

/* Pike methods in the set class, not availble from C:
 *  
 *  cast(string) --> string|array
 *  test(num,start,step) --> Generate random set
 *  sort()   --> sort on weight
 *  dup()    --> copy
 *  slice(start,num) --> partial (array)
 *  or `| `+
 *  sub `-
 *  intersect `&
 *  size _sizeof
 *  memsize
 *  overhead
 */
