/*
 * catalog.h - Module 1: doubly linked list catalog.
 *
 * The registry of live objects. Chosen over a dynamic array because insert
 * and delete are O(1) given a node pointer, which is exactly what we need to
 * model launches (push_back) and de-orbits / collisions (remove) mid-sim.
 * Each Object stores a back-pointer to its node, so removal by object is O(1)
 * too (no linear scan to find the node).
 */
#ifndef CATALOG_H
#define CATALOG_H

#include "astro.h"

typedef struct CatalogNode {
    Object             *obj;
    struct CatalogNode *prev;
    struct CatalogNode *next;
} CatalogNode;

typedef struct {
    CatalogNode *head;
    CatalogNode *tail;
    size_t       count;
} Catalog;

void         catalog_init(Catalog *c);
CatalogNode *catalog_push_back(Catalog *c, Object *o);  /* O(1), sets o->node */
void         catalog_remove(Catalog *c, CatalogNode *n); /* O(1) */
CatalogNode *catalog_find(const Catalog *c, size_t id);  /* O(n) */
void         catalog_free(Catalog *c);                   /* frees nodes only */

#endif /* CATALOG_H */
