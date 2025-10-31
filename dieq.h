#ifndef _DIEQ_H
#define _DIEQ_H

#ifndef NULL
#  define NULL ((void*)0)
#endif // NULL

#ifndef __bool_true_false_are_defined
#  define __bool_true_false_are_defined 1
#  define bool _Bool
#  define true 1
#  define false 0
#endif // __bool_true_false_are_defined

typedef __SIZE_TYPE__ dieq_uisz;
typedef unsigned char dieq_byte;

typedef struct {
  dieq_byte *buf;
  dieq_uisz idx;
  dieq_uisz cap;
} Dieq_Arena;

void *dieq_mem_set(void *ptr, dieq_byte b, dieq_uisz count);
void *dieq_mem_cpy(void *restrict dst, void *restrict src, dieq_uisz count);

void dieq_global_setup(void *start, void *end);

void *dieq_alloc(dieq_uisz size);

void dieq_free(void *ptr);

void *dieq_realloc(void *ptr, dieq_uisz new_size);


bool dieq_arena_init(Dieq_Arena *arena, dieq_uisz capacity);

void dieq_arena_deinit(Dieq_Arena *arena);

void *dieq_arena_alloc(Dieq_Arena *arena, dieq_uisz size);

dieq_uisz dieq_arena_save_point(Dieq_Arena *arena);

void dieq_arena_restore_point(Dieq_Arena *arena, dieq_uisz save_point);

#endif // _DIEQ_H

#ifdef DIEQ_IMPLEMENTATION

typedef struct {
  void *next;
  void *prev;
  dieq_uisz size;
  dieq_uisz padding;
} Dieq__Block_Header;

static void *dieq__global_start = NULL;
static void *dieq__global_end   = NULL;
static void *dieq__global_head  = NULL;

static inline dieq_uisz dieq__align_forward(dieq_uisz n, dieq_uisz alignment) {
  return n + (n & (alignment-1));
}

void *dieq_mem_set(void *ptr, dieq_byte b, dieq_uisz count) {
  dieq_byte *data = (dieq_byte*)ptr;
  for (dieq_uisz i = 0; i < count; ++i) {
    data[i] = b;
  }
  return ptr;
}

void *dieq_mem_cpy(void *restrict dst, void *restrict src, dieq_uisz count) {
  dieq_byte *dst_buf = (dieq_byte*)dst;
  dieq_byte *src_buf = (dieq_byte*)src;
  for (dieq_uisz i = 0; i < count; ++i) dst_buf[i] = src_buf[i];
  return dst;
}

void dieq_global_setup(void *start, void *end) {
  if (dieq__global_start == start) {
    if (end > dieq__global_end) {
      dieq_uisz sz = (dieq_uisz)(end - dieq__global_end);
      dieq_mem_set(dieq__global_end, 0, sz);
    }

    dieq__global_end = end;
    return;
  }

  dieq__global_start = start;
  dieq__global_end = end;
  dieq__global_head = NULL;

  dieq_mem_set(start, 0, (dieq_uisz)(end - start));
}

void *dieq__find_space(dieq_uisz desired_space) {
  dieq_uisz true_space = dieq__align_forward(desired_space, sizeof(void*));

  if (dieq__global_head == NULL) {
    Dieq__Block_Header *space_header = (Dieq__Block_Header*)dieq__global_start;
    space_header->size = true_space;
    space_header->padding = true_space - desired_space;
    space_header->next = NULL;
    space_header->prev = NULL;
    return dieq__global_start;
  }

  if (dieq__global_head > dieq__global_start) {
    if (dieq__global_start + true_space <= dieq__global_head) {
      void *space = dieq__global_start;
      Dieq__Block_Header *space_header = (Dieq__Block_Header*)space;
      space_header->size = true_space;
      space_header->padding = true_space - desired_space;
      space_header->next = dieq__global_head;
      space_header->prev = NULL;
      space_header->size = desired_space;
      return space;
    }
  }

  Dieq__Block_Header *node = (Dieq__Block_Header*)dieq__global_head;

  while (node) {
    void *space = node + node->size;

    if (node->next == NULL ) {
      if (space < dieq__global_end) {
        if (space + true_space <= dieq__global_end) {
          Dieq__Block_Header *space_header = (Dieq__Block_Header*)space;
          space_header->size = true_space;
          space_header->padding = true_space - desired_space;
          space_header->next = NULL;
          space_header->prev = node;
          node->next = space;
          return space;
        }
      }

      return NULL;
    }

    if (space < node->next) {
      dieq_uisz diff = (dieq_uisz)node->next - (dieq_uisz)(node + node->size);
      if (diff <= desired_space) {
        Dieq__Block_Header *space_header = (Dieq__Block_Header*)space;
        space_header->size = true_space;
        space_header->padding = true_space - desired_space;
        space_header->next = node->next;
        space_header->prev = node;
        node->next = space;
        return space;
      }
    }

    node = node->next;
  }

  return NULL;
}

bool dieq__node_exists(void *n) {
  Dieq__Block_Header *node = (Dieq__Block_Header*)dieq__global_head;
  while (node) {
    if (node == n) return true;
    node = node->next;
  }
  return false;
}

void *dieq_alloc(dieq_uisz size) {
  void *space = dieq__find_space(sizeof(Dieq__Block_Header) + size);
  if (space == NULL) return NULL;
  
  Dieq__Block_Header *header = (Dieq__Block_Header*)space;
  if (dieq__global_head == NULL) dieq__global_head = (void*)header;
  dieq_uisz total_size = header->size;

  void *user_ptr = space + sizeof(*header);
  dieq_mem_set(user_ptr, 0, total_size - sizeof(*header));

  return user_ptr;
}

void dieq_free(void *ptr) {
  if (ptr <= dieq__global_start || ptr >= dieq__global_end) {
    return; // Maybe should print something here?
  }

  Dieq__Block_Header *header = (Dieq__Block_Header*)(ptr - sizeof(header));
  if (!dieq__node_exists(header)) {
    // An error should be presented here since the pointer looks valid but it's not a known node
    return;
  }

  Dieq__Block_Header *prev = header->prev;
  Dieq__Block_Header *next = header->next;
  if (prev) prev->next = next;
  if (next) next->prev = prev;
}

void *dieq_realloc(void *old_ptr, dieq_uisz new_size) {
  void *new_ptr = dieq_alloc(new_size);
  if (old_ptr == NULL) return new_ptr;

  Dieq__Block_Header *old_header = (Dieq__Block_Header*)(old_ptr - sizeof(Dieq__Block_Header));
  dieq_uisz old_size = old_header->size - old_header->padding - sizeof(Dieq__Block_Header);

  dieq_uisz smaller_size = old_size < new_size ? old_size : new_size;
  dieq_mem_cpy(new_ptr, old_ptr, smaller_size);

  dieq_free(old_ptr);

  return new_ptr;
}


bool dieq_arena_init(Dieq_Arena *arena, dieq_uisz capacity) {
  void *buf = dieq_alloc(capacity);
  if (buf == NULL) return false;

  *arena = (Dieq_Arena) {
    .buf = buf,
    .idx = 0,
    .cap = capacity,
  };
  return true;
}

void *dieq_arena_alloc(Dieq_Arena *arena, dieq_uisz size) {
  size = dieq__align_forward(size, sizeof(void*));
  if (arena->idx + size > arena->cap) return NULL;
  void *ptr = arena->buf + arena->idx;
  arena->idx += size;
  return ptr;
}

dieq_uisz dieq_arena_save_point(Dieq_Arena *arena) {
  return arena->idx;
}

void dieq_arena_restore_point(Dieq_Arena *arena, dieq_uisz save_point) {
  arena->idx = save_point;
}

void dieq_arena_deinit(Dieq_Arena *arena) {
  dieq_free(arena->buf);
  arena->buf = NULL;
  arena->idx = 0;
  arena->cap = 0;
}

#endif // DIEQ_IMPLEMENTATION
