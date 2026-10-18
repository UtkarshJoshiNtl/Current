/* catalog.c - Module 1: doubly linked list */
#include "catalog.h"
#include <stdlib.h>

void catalog_init(Catalog *c)
{
    c->head = NULL;
    c->tail = NULL;
    c->count = 0;
}

CatalogNode *catalog_push_back(Catalog *c, Object *o)
{
    CatalogNode *n = (CatalogNode *)malloc(sizeof(CatalogNode));
    if (!n) return NULL;
    n->obj = o;
    n->next = NULL;
    n->prev = c->tail;
    if (c->tail) c->tail->next = n;
    else c->head = n;
    c->tail = n;
    c->count++;
    o->node = n;     /* back-pointer => O(1) delete later */
    return n;
}

void catalog_remove(Catalog *c, CatalogNode *n)
{
    if (!n) return;
    if (n->prev) n->prev->next = n->next;
    else c->head = n->next;
    if (n->next) n->next->prev = n->prev;
    else c->tail = n->prev;
    if (n->obj) n->obj->node = NULL;
    free(n);
    c->count--;
}

CatalogNode *catalog_find(const Catalog *c, size_t id)
{
    for (CatalogNode *n = c->head; n; n = n->next)
        if (n->obj->id == id) return n;
    return NULL;
}

void catalog_free(Catalog *c)
{
    CatalogNode *n = c->head;
    while (n) {
        CatalogNode *next = n->next;
        free(n);
        n = next;
    }
    catalog_init(c);
}
