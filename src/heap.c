/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   heap.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: feel-idr <feel-idr@student.1337.ma>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/16 17:04:29 by feel-idr          #+#    #+#             */
/*   Updated: 2026/09/16 17:04:29 by feel-idr         ###   ########.fr       */
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
