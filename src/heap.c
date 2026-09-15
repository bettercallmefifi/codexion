/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   heap.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: feel-idr <feel-idr@student.1337.ma>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/13 19:48:00 by feel-idr          #+#    #+#             */
/*   Updated: 2026/09/15 06:49:23 by feel-idr         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "codexion.h"

int	heap_init(t_heap *heap, int mode)
{
	heap->data = malloc(sizeof(t_request) * HEAP_CAPACITY);
	if (!heap->data)
		return (0);
	memset(heap->data, 0, sizeof(t_request) * HEAP_CAPACITY);
	heap->size = 0;
	heap->capacity = HEAP_CAPACITY;
	heap->mode = mode;
	return (1);
}

void	heap_free(t_heap *heap)
{
	free(heap->data);
	heap->data = NULL;
	heap->size = 0;
	heap->capacity = 0;
}

int	heap_push(t_heap *heap, t_request req)
{
	if (heap->size >= heap->capacity)
		return (0);
	heap->data[heap->size] = req;
	heap->size++;
	sift_up(heap, heap->size - 1);
	return (1);
}

int	heap_pop(t_heap *heap)
{
	if (heap->size == 0)
		return (0);
	heap->size--;
	heap->data[0] = heap->data[heap->size];
	sift_down(heap, 0);
	return (1);
}

t_request	*heap_peek(t_heap *heap)
{
	if (heap->size == 0)
		return (NULL);
	return (&heap->data[0]);
}
