/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   heap_utils.c                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: feel-idr <feel-idr@student.1337.ma>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/16 19:43:12 by feel-idr          #+#    #+#             */
/*   Updated: 2026/09/16 19:43:12 by feel-idr         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "codexion.h"

int	heap_less(t_heap *heap, t_request *a, t_request *b)
{
	if (heap->mode == MODE_EDF)
	{
		if (a->deadline != b->deadline)
			return (a->deadline < b->deadline);
		return (a->id < b->id);
	}
	return (a->seq < b->seq);
}

void	heap_swap(t_request *a, t_request *b)
{
	t_request	tmp;

	tmp = *a;
	*a = *b;
	*b = tmp;
}

void	sift_up(t_heap *heap, int i)
{
	int	parent;

	while (i > 0)
	{
		parent = (i - 1) / 2;
		if (!heap_less(heap, &heap->data[i], &heap->data[parent]))
			return ;
		heap_swap(&heap->data[i], &heap->data[parent]);
		i = parent;
	}
}

void	sift_down(t_heap *heap, int i)
{
	int	child;

	child = 2 * i + 1;
	while (child < heap->size)
	{
		if (child + 1 < heap->size
			&& heap_less(heap, &heap->data[child + 1], &heap->data[child]))
			child++;
		if (!heap_less(heap, &heap->data[child], &heap->data[i]))
			return ;
		heap_swap(&heap->data[child], &heap->data[i]);
		i = child;
		child = 2 * i + 1;
	}
}

int	heap_remove(t_heap	*heap, int id)
{
	int	i;

	i = 0;
	while (i < heap->size)
	{
		if (heap->data[i].id == id)
		{
			heap->size--;
			heap->data[i] = heap->data[heap->size];
			sift_down(heap, i);
			sift_up(heap, i);
			return (1);
		}
		i++;
	}
	return (0);
}
