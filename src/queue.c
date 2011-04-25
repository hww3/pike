/*
|| This file is part of Pike. For copyright information see COPYRIGHT.
|| Pike is distributed under GPL, LGPL and MPL. See the file COPYING
|| for more information.
|| $Id$
*/

#include "global.h"
#include "pike_macros.h"
#include "queue.h"
#include "pike_error.h"

struct queue_entry
{
  queue_call call;
  void *data;
};

/* FIXME: Add a way to keep the first block even when the queue
 * becomes empty. In e.g. the gc the queue becomes empty very
 * frequently which causes the first block to be freed and allocated a
 * lot. */

#define QUEUE_ENTRIES 8191

struct queue_block
{
  struct queue_block *next;
  int used;
  struct queue_entry entries[QUEUE_ENTRIES];
};

void run_queue(struct pike_queue *q)
{
  struct queue_block *b;

#ifdef PIKE_DEBUG
  if (q->first && q->last == (struct queue_block *)(ptrdiff_t)1)
    Pike_fatal("This looks like a lifo queue.\n");
#endif

  while((b=q->first))
  {
    int e;
    for(e=0;e<b->used;e++)
    {
      debug_malloc_touch(b->entries[e].data);
      b->entries[e].call(b->entries[e].data);
    }

    q->first=b->next;
    free((char *)b);
  }
  q->last=0;
}

void discard_queue(struct pike_queue *q)
{
  struct queue_block *b = q->first;
  while (b)
  {
    struct queue_block *next = b->next;
    free((char *) b);
    b = next;
  }
  q->first = q->last = 0;
}

void enqueue(struct pike_queue *q, queue_call call, void *data)
{
  struct queue_block *b;

#ifdef PIKE_DEBUG
  if (!q->first) q->last = 0;
  else if (q->last == (struct queue_block *)(ptrdiff_t)1)
    Pike_fatal("This looks like a lifo queue.\n");
#endif

  b=q->last;
  if(!b || b->used >= QUEUE_ENTRIES)
  {
    b=ALLOC_STRUCT(queue_block);
    b->used=0;
    b->next=0;
    if(q->first)
      q->last->next=b;
    else
      q->first=b;
    q->last=b;
  }

  b->entries[b->used].call=call;
  b->entries[b->used].data=debug_malloc_pass(data);
  b->used++;
}

/* LIFO queue, commonly known as a stack.. */

void run_lifo_queue(struct pike_queue *q)
{
  struct queue_block *b;

#ifdef PIKE_DEBUG
  if (q->first && q->last != (struct queue_block *)(ptrdiff_t)1)
    Pike_fatal("This does not look like a lifo queue.\n");
#endif

  while((b=q->first))
  {
    if (b->used > 0) {
      int e = --b->used;
      debug_malloc_touch(b->entries[e].data);
      b->entries[e].call(b->entries[e].data);
    }
    else {
      q->first=b->next;
      free((char *)b);
    }
  }
}

void enqueue_lifo(struct pike_queue *q, queue_call call, void *data)
{
  struct queue_block *b=q->first;

#ifdef PIKE_DEBUG
  if (!q->first) q->last = (struct queue_block *)(ptrdiff_t)1;
  else if (q->last != (struct queue_block *)(ptrdiff_t)1)
    Pike_fatal("This does not look like a lifo queue.\n");
#endif

  if(!b || b->used >= QUEUE_ENTRIES)
  {
    b=ALLOC_STRUCT(queue_block);
    b->used=0;
    b->next=q->first;
    q->first=b;
  }

  b->entries[b->used].call=call;
  b->entries[b->used].data=debug_malloc_pass(data);
  b->used++;
}

void *dequeue_lifo(struct pike_queue *q, queue_call call)
{
  struct queue_block *b;

#ifdef PIKE_DEBUG
  if (q->first && q->last != (struct queue_block *)(ptrdiff_t)1)
    Pike_fatal("This does not look like a lifo queue.\n");
#endif

  while((b=q->first))
  {
    if (b->used > 0) {
      int e = --b->used;
      debug_malloc_touch(b->entries[e].data);
      if (b->entries[e].call == call)
	return b->entries[e].data;
    }
    else {
      q->first=b->next;
      free((char *)b);
    }
  }

  return 0;
}
