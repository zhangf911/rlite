#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "list.h"
#include "util.h"

void rl_print_list_node(rl_list *list, rl_list_node *node);
rl_list_node *rl_list_node_create(rl_list *list);

int long_cmp(void *v1, void *v2)
{
	long a = *((long *)v1), b = *((long *)v2);
	if (a == b) {
		return 0;
	}
	return a > b ? 1 : -1;
}

long long_list_list_node_serialize(void *_list, void *_node, unsigned char **_data, long *data_size)
{
	rl_list *list = (rl_list *)_list;
	rl_list_node *node = (rl_list_node *)_node;
	unsigned char *data = malloc(sizeof(unsigned char) * ((4 * list->max_node_size) + 12));
	put_4bytes(data, node->size);
	put_4bytes(&data[4], node->left);
	put_4bytes(&data[8], node->right);
	long i, pos = 12;
	for (i = 0; i < node->size; i++) {
		put_4bytes(&data[pos], *(long *)(node->elements[i]));
		pos += 4;
	}
	*_data = data;
	if (data_size) {
		*data_size = pos;
	}
	return 0;
}

long long_list_list_node_deserialize(void *_list, unsigned char *data, void **_node)
{
	rl_list *list = (rl_list *)_list;
	rl_list_node *node = rl_list_node_create(list);
	if (!node) {
		return 1;
	}
	node->size = (long)get_4bytes(data);
	node->left = (long)get_4bytes(&data[4]);
	node->right = (long)get_4bytes(&data[8]);
	long i, pos = 12;
	for (i = 0; i < node->size; i++) {
		node->elements[i] = malloc(sizeof(long));
		*(long *)node->elements[i] = get_4bytes(&data[pos]);
		pos += 4;
	}
	*_node = node;
	return 0;
}

void long_formatter(void *v2, char **formatted, int *size)
{
	*formatted = malloc(sizeof(char) * 22);
	*size = snprintf(*formatted, 22, "%ld", *(long *)v2);
}

void init_long_list()
{
	long_list.element_size = sizeof(long);
	long_list.formatter = long_formatter;
	long_list.serialize = long_list_list_node_serialize;
	long_list.deserialize = long_list_list_node_deserialize;
	long_list.cmp = long_cmp;
}

rl_list_node *rl_list_node_create(rl_list *list)
{
	rl_list_node *node = malloc(sizeof(rl_list_node));
	node->elements = malloc(sizeof(void *) * list->max_node_size);
	node->size = 0;
	return node;
}

long rl_list_node_destroy(rl_list *list, rl_list_node *node)
{
	list = list;
	long i;
	if (node->elements) {
		for (i = 0; i < node->size; i++) {
			free(node->elements[i]);
		}
		free(node->elements);
	}
	free(node);
	return 0;
}

rl_list *rl_list_create(rl_list_type *type, long max_node_size, rl_accessor *accessor)
{
	rl_list *list = malloc(sizeof(rl_list));
	if (!list) {
		return NULL;
	}
	list->max_node_size = max_node_size;
	list->type = type;
	rl_list_node *node = rl_list_node_create(list);
	if (!node) {
		free(list);
		return NULL;
	}
	node->left = 0;
	node->right = 0;
	list->accessor = accessor;
	list->size = 0;
	if (0 != list->accessor->insert(list, &list->left, node)) {
		free(node);
		free(list);
		return NULL;
	}
	list->right = list->left;
	return list;
}

int rl_list_destroy(rl_list *list)
{
	long i;
	rl_list_node **nodes;
	long size;
	if (0 != list->accessor->list(list, &nodes, &size)) {
		return 1;
	}
	for (i = 0; i < size; i++) {
		if (0 != rl_list_node_destroy(list, nodes[i])) {
			return 1;
		}
	}
	free(list);
	return 0;
}

int rl_list_find_element(rl_list *list, void *element, long *position)
{
	rl_list_node *node;
	long pos = 0, i, number = list->left;
	while (number != 0) {
		node = list->accessor->select(list, number);
		if (!node) {
			break;
		}
		for (i = 0; i < node->size; i++) {
			if (list->type->cmp(node->elements[i], element) == 0) {
				if (position) {
					*position = pos + i;
					return 1;
				}
			}
		}
		pos += node->size;
		number = node->right;
	}
	return 0;
}

int rl_list_add_element(rl_list *list, void *element, long position)
{
	rl_list_node *node;
	long pos = 0, number;
	if (position > list->size + 1 || position < - list->size - 1) {
		return 1;
	}
	if (position >= 0) {
		number = list->left;
		do {
			node = list->accessor->select(list, number);
			if (pos + node->size >= position && position - pos < list->max_node_size) {
				break;
			}
			number = node->right;
			if (number != 0) {
				pos += node->size;
			}
		}
		while (number != 0 && pos < position);
	}
	else {
		position = list->size + position + 1;
		pos = list->size;
		number = list->right;
		do {
			node = list->accessor->select(list, number);
			pos -= node->size;
			if (pos <= position) {
				break;
			}
			number = node->left;
		}
		while (number != 0);
	}

	if (node->size != list->max_node_size) {
		if (position - pos + 1 < list->max_node_size) {
			memmove_dbg(&node->elements[position - pos + 1], &node->elements[position - pos], sizeof(void *) * (node->size - (position - pos)), __LINE__);
		}
		node->elements[position - pos] = element;
		node->size++;
		list->accessor->update(list, NULL, node);
	}
	else {
		rl_list_node *sibling_node;
		if (node->left) {
			sibling_node = list->accessor->select(list, node->left);
			if (sibling_node->size != list->max_node_size) {
				sibling_node->elements[sibling_node->size] = node->elements[0];
				memmove_dbg(&node->elements[0], &node->elements[1], sizeof(void *) * position - pos - 1, __LINE__);
				node->elements[position - pos - 1] = element;
				sibling_node->size++;
				list->accessor->update(list, NULL, node);
				list->accessor->update(list, NULL, sibling_node);
				goto cleanup;
			}
		}
		if (node->right) {
			sibling_node = list->accessor->select(list, node->right);
			if (sibling_node->size != list->max_node_size) {
				memmove_dbg(&sibling_node->elements[1], &sibling_node->elements[0], sizeof(void *) * sibling_node->size, __LINE__);
				sibling_node->elements[0] = node->elements[node->size - 1];
				memmove_dbg(&node->elements[position - pos + 1], &node->elements[position - pos], sizeof(void *) * (node->size - position + pos - 1), __LINE__);
				node->elements[position - pos] = element;
				sibling_node->size++;
				list->accessor->update(list, NULL, node);
				list->accessor->update(list, NULL, sibling_node);
				goto cleanup;
			}
		}
		rl_list_node *new_node, *old_node;
		new_node = rl_list_node_create(list);
		if (position - pos == node->size) {
			new_node->elements[0] = element;
		}
		else {
			new_node->elements[0] = node->elements[node->size - 1];
			memmove_dbg(&node->elements[position - pos + 1], &node->elements[position - pos], sizeof(void *) * (node->size - position + pos - 1), __LINE__);
			node->elements[position - pos] = element;
		}
		new_node->size = 1;
		new_node->right = node->right;
		list->accessor->insert(list, &node->right, new_node);
		list->accessor->update(list, &new_node->left, node);
		if (new_node->right != 0) {
			old_node = list->accessor->select(list, new_node->right);
			old_node->left = node->right;
			list->accessor->update(list, NULL, old_node);
		}
		else {
			list->right = node->right;
		}
	}

cleanup:
	list->size++;
	return 0;
}

int rl_list_remove_element(rl_list *list, long position)
{
	list = list;
	position = position;
	return 1;
}

int rl_list_is_balanced(rl_list *list)
{
	list = list;
	return 1;
}

void rl_print_list(rl_list *list)
{
	printf("-------\n");
	rl_list_node *node;
	char *element;
	int size;
	long i, number = list->left;
	while (number != 0) {
		node = list->accessor->select(list, number);
		for (i = 0; i < node->size; i++) {
			list->type->formatter(node->elements[i], &element, &size);
			fwrite(element, sizeof(char), size, stdout);
			free(element);
			printf("\n");
		}
		number = node->right;
	}
	printf("-------\n");
}

void rl_flatten_list(rl_list *list, void *** scores)
{
	long i, number = list->left, pos = 0;
	rl_list_node *node;
	while (number != 0) {
		node = list->accessor->select(list, number);
		for (i = 0; i < node->size; i++) {
			*scores[pos++] = node->elements[i];
		}
		number = node->right;
	}
}