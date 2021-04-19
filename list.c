#include "list.h"

/*
 * Allocates a new list_node_t. NULL on failure.
 */
list_node_t *list_node_new(void *val)
{
    list_node_t *self;

    if (!val)// empty nodes are not allowed to be inserted, not checked when used
        return NULL;
    if (!(self = LIST_MALLOC(sizeof(list_node_t))))
        return NULL;
    self->prev = NULL;
    self->next = NULL;
    self->val = val;
    return self;
}

/*
 * Allocate a new list_t. NULL on failure.
 */
list_t *list_new(void)
{
    list_t *self;
    if (!(self = LIST_MALLOC(sizeof(list_t))))
        return NULL;
    self->head = NULL;
    self->tail = NULL;
    self->free = NULL;
    self->match = NULL;
    self->compare = NULL;
    self->len = 0;
    return self;
}

/*
 * Remove the given node from the list, freeing it and it's value.
 */
int list_remove(list_t *self, list_node_t *node)
{
    if (!self || !node)
        return -1;

    node->prev ? (node->prev->next = node->next) : (self->head = node->next);

    node->next ? (node->next->prev = node->prev) : (self->tail = node->prev);

    if (self->free)
        self->free(node->val);

    LIST_FREE(node);
    node = NULL;
    --self->len;

    return 0;
}

/*
 * Free the list.
 */
int list_destroy(list_t *self)
{
    unsigned int len = 0;
    list_node_t *next = NULL;
    list_node_t *curr = NULL;

    if (!self)
        return -1;

    len = self->len;
    curr = self->head;

    while (len--) {
        next = curr->next;
        if (self->free)
            self->free(curr->val);
        LIST_FREE(curr);
        curr = next;
    }

    LIST_FREE(self);
    self = NULL;

    return 0;
}

/*
 * Append the given node to the list
 * and return the node, NULL on failure.
 */
list_node_t *list_rpush(list_t *self, list_node_t *node)
{
    if (!self || !node)
        return NULL;

    if (self->len) {
        node->prev = self->tail;
        node->next = NULL;
        self->tail->next = node;
        self->tail = node;
    } else {
        self->head = self->tail = node;
        node->prev = node->next = NULL;
    }

    ++self->len;
    return node;
}

/*
 * Prepend the given node to the list
 * and return the node, NULL on failure.
 */
list_node_t *list_lpush(list_t *self, list_node_t *node)
{
    if (!self || !node)
        return NULL;

    if (self->len) {
        node->next = self->head;
        node->prev = NULL;
        self->head->prev = node;
        self->head = node;
    } else {
        self->head = self->tail = node;
        node->prev = node->next = NULL;
    }

    ++self->len;
    return node;
}

/*
 * Return / detach the last node in the list, or NULL.
 */
list_node_t *list_rpop(list_t *self)
{
    if (!self || !self->len)
        return NULL;

    list_node_t *node = self->tail;

    if (--self->len) {
        (self->tail = node->prev)->next = NULL;
    } else {
        self->tail = self->head = NULL;
    }

    node->next = node->prev = NULL;
    return node;
}

/*
 * Return / detach the first node in the list, or NULL.
 */
list_node_t *list_lpop(list_t *self)
{
    if (!self || !self->len)
        return NULL;

    list_node_t *node = self->head;

    if (--self->len) {
        (self->head = node->next)->prev = NULL;
    } else {
        self->head = self->tail = NULL;
    }

    node->next = node->prev = NULL;
    return node;
}

/*
 * Allocate a new list_iterator_t with the given start
 * node. NULL on failure.
 */
static list_iterator_t *list_iterator_new_from_node(list_node_t *node, list_direction_t direction)
{
    list_iterator_t *self;
    if (!(self = LIST_MALLOC(sizeof(list_iterator_t))))
        return NULL;
    self->next = node;
    self->direction = direction;
    return self;
}

/*
 * Allocate a new list_iterator_t. NULL on failure.
 * Accepts a direction, which may be LIST_HEAD or LIST_TAIL.
 */
static list_iterator_t *list_iterator_new(list_t *list, list_direction_t direction)
{
    list_node_t *node = direction == LIST_HEAD ? list->head : list->tail;
    return list_iterator_new_from_node(node, direction);
}

/*
 * Return the next list_node_t or NULL when no more
 * nodes remain in the list.
 */
static list_node_t *list_iterator_next(list_iterator_t *self)
{
    list_node_t *curr = self->next;
    if (curr) {
        self->next = self->direction == LIST_HEAD ? curr->next : curr->prev;
    }
    return curr;
}

/*
 * Free the list iterator.
 */
static void list_iterator_destroy(list_iterator_t *self)
{
    LIST_FREE(self);
    self = NULL;
}

/*
 * Return the node associated to val or NULL.
 */
list_node_t *list_find(list_t *self, const void *val)
{
    list_iterator_t *it = NULL;
    list_node_t *node = NULL;

    if (!self)
        return NULL;

    it = list_iterator_new(self, LIST_HEAD);

    while ((node = list_iterator_next(it))) {
        if (self->match) {
            if (self->match(node->val, val)) {
                list_iterator_destroy(it);
                return node;
            }
        } else {
            if (node->val == val) {
                list_iterator_destroy(it);
                return node;
            }
        }
    }

    list_iterator_destroy(it);
    return NULL;
}

/*
 * Return the node at the given index or NULL.
 */
list_node_t *list_at(list_t *self, int index)
{
    list_direction_t direction = LIST_HEAD;

    if (!self)
        return NULL;

    if (index < 0) {
        direction = LIST_TAIL;
        index = ~index;
    }

    if ((unsigned)index < self->len) {
        list_iterator_t *it = list_iterator_new(self, direction);
        list_node_t *node = list_iterator_next(it);
        while (index--)
            node = list_iterator_next(it);
        list_iterator_destroy(it);
        return node;
    }

    return NULL;
}

/*
 * Ascending order the given node to the list
 * and return the node, NULL on failure.
 */
list_node_t *list_push_asc(list_t *self, list_node_t *node_in)
{
    list_iterator_t *it = NULL;
    list_node_t *node = NULL;

    if (!self || !node_in)
        return NULL;

    if (0 == self->len)
        return list_rpush(self, node_in);

    it = list_iterator_new(self, LIST_HEAD);

    while ((node = list_iterator_next(it))) {
        if (self->compare) {
            if (self->compare(node->val, node_in->val)) {
                break;
            }
        } else {
            if (node->val > node_in->val) {
                break;
            }
        }
    }
    list_iterator_destroy(it);

    if (NULL == node)
        return list_rpush(self, node_in);
    else if (NULL == node->prev)
        return list_lpush(self, node_in);
    else {
        node_in->next = node;// node->prev->next
        node_in->prev = node->prev;
        node->prev->next = node_in;
        node->prev = node_in;
        self->len++;
    }

    return node_in;
}

/*
 * Descending order the given node to the list
 * and return the node, NULL on failure.
 */
list_node_t *list_push_desc(list_t *self, list_node_t *node_in)
{
    list_iterator_t *it = NULL;
    list_node_t *node = NULL;

    if (!self || !node_in)
        return NULL;

    if (0 == self->len)
        return list_rpush(self, node_in);

    it = list_iterator_new(self, LIST_HEAD);

    while ((node = list_iterator_next(it))) {
        if (self->compare) {
            if (!(self->compare(node->val, node_in->val))) {
                break;
            }
        } else {
            if (node->val < node_in->val) {
                break;
            }
        }
    }
    list_iterator_destroy(it);

    if (NULL == node)
        return list_rpush(self, node_in);
    else if (NULL == node->prev)
        return list_lpush(self, node_in);
    else {
        node_in->next = node;// node->prev->next
        node_in->prev = node->prev;
        node->prev->next = node_in;
        node->prev = node_in;
        self->len++;
    }

    return node_in;
}
