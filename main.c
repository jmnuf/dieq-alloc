#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "dieq.h"

typedef struct LL_Node LL_Node;

struct LL_Node {
  LL_Node *next;
  int value;
};

typedef struct {
  LL_Node *head;
  size_t len;
} Linked_List;

#define ll_foreach(it, list) for (LL_Node *it = (list)->head; it != NULL; it = (it)->next)

bool ll_append(Linked_List *ll, int n) {
  LL_Node new_node = { .value = n, .next = NULL };
  if (!ll->head) {
    void *ptr = dieq_alloc(sizeof(new_node));
    if (ptr == NULL) {
      fprintf(stderr, "Failed to allocate head node\n");
      return false;
    }

    ll->head = dieq_mem_cpy(ptr, &new_node, sizeof(new_node));
    ll->len++;
    return true;
  }

  LL_Node *node = ll->head;

  while (node->next != NULL) node = node->next;

  void *ptr = dieq_alloc(sizeof(new_node));
  if (ptr == NULL) {
    fprintf(stderr, "Failed to allocate new node\n");
    return false;
  }
  node->next = dieq_mem_cpy(ptr, &new_node, sizeof(new_node));
  ll->len++;
  return true;
}

LL_Node *ll_get_index(Linked_List *ll, size_t index) {
  if (index >= ll->len) return NULL;

  size_t i = 0;
  ll_foreach(n, ll) {
    if (i == index) return n;
    ++i;
  }
  return NULL;
}

bool ll_remove_index(Linked_List *ll, size_t index) {
  if (index >= ll->len) return false;

  if (index == 0) {
    LL_Node *n = ll->head;
    ll->head = n->next;
    dieq_free(n);
    ll->len--;
    return true;
  }

  size_t i = 0;
  LL_Node *prev = NULL;
  ll_foreach(n, ll) {
    if (i == index) {
      LL_Node *next = n->next;
      if (prev) prev->next = next;
      dieq_free(n);
      ll->len--;
      return true;
    }

    prev = n;
    ++i;
  }
  
  return false;
}

int main(void) {
  printf("Hello, World!\n");

  {
    size_t count = 1024*8*2;
    void *start = malloc(count);
    dieq_global_setup(start, start + count);
  }

  Linked_List ll = {0};
  for (int i = 0; i < 10; ++i) {
    if (ll_append(&ll, i + 1)) printf("Appended number: %d\n", i + 1);
  }

  ll_foreach(n, &ll) {
    if (n != ll.head) printf("->");
    printf("Node(%d)", n->value);
  }
  printf("\n");


  {
    LL_Node n;
    size_t index;

    index = 4;
    n = *ll_get_index(&ll, index);
    ll_remove_index(&ll, index); printf("Removed item: %d\n", n.value);
    index = 0;
    n = *ll_get_index(&ll, index);
    ll_remove_index(&ll, index); printf("Removed item: %d\n", n.value);
  }

  if (ll_append(&ll, 30)) printf("Appended number: %d\n", 30);

  ll_foreach(n, &ll) {
    if (n != ll.head) printf("->");
    printf("Node(%d)", n->value);
  }
  printf("\n");
  
  return 0;
}

#define DIEQ_IMPLEMENTATION
#include "dieq.h"

