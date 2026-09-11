/* ************************************************************************** */
/*                                                                            */
/*                                                       :::      ::::::::    */
/*   heap.c                                            :+:      :+:    :+:    */
/*                                                   +:+ +:+         +:+      */
/*   By: feel-idr <feel-idr@student.1337.ma>       +#+  +:+       +#+         */
/*                                               +#+#+#+#+#+   +#+            */
/*   Created: 2026/09/06 01:07:37 by feel-idr         #+#    #+#              */
/*   Updated: 2026/09/11 00:00:00 by feel-idr        ###   ########.fr        */
/*                                                                            */
/* ************************************************************************** */

#include "header.h"

void	queue_push(t_device *device, t_request request)
{
	t_request	swap;
	int			slot;
	int			ancestor;

	device->pending[device->queued] = request;
	device->queued++;
	slot = device->queued - 1;
	while (slot > 0)
	{
		ancestor = (slot - 1) / 2;
		if (device->pending[slot].priority_ms
			> device->pending[ancestor].priority_ms)
			break ;
		if (device->pending[slot].priority_ms
			== device->pending[ancestor].priority_ms
			&& device->pending[slot].worker_id
			> device->pending[ancestor].worker_id)
			break ;
		swap = device->pending[slot];
		device->pending[slot] = device->pending[ancestor];
		device->pending[ancestor] = swap;
		slot = ancestor;
	}
}

static int	select_child(t_device *device, int left_child, int right_child)
{
	if (right_child < device->queued
		&& (device->pending[right_child].priority_ms
			< device->pending[left_child].priority_ms
			|| (device->pending[right_child].priority_ms
				== device->pending[left_child].priority_ms
				&& device->pending[right_child].worker_id
				< device->pending[left_child].worker_id)))
		return (right_child);
	return (left_child);
}

static int	parent_precedes(t_device *device, int cursor, int best_child)
{
	if (device->pending[cursor].priority_ms
		< device->pending[best_child].priority_ms)
		return (1);
	if (device->pending[cursor].priority_ms
		== device->pending[best_child].priority_ms
		&& device->pending[cursor].worker_id
		< device->pending[best_child].worker_id)
		return (1);
	return (0);
}

void	queue_sift_down(t_device *device)
{
	int			cursor;
	int			left_child;
	int			right_child;
	int			best_child;
	t_request	swap;

	cursor = 0;
	while (1)
	{
		left_child = 2 * cursor + 1;
		right_child = 2 * cursor + 2;
		if (left_child >= device->queued)
			break ;
		best_child = select_child(device, left_child, right_child);
		if (parent_precedes(device, cursor, best_child))
			break ;
		swap = device->pending[cursor];
		device->pending[cursor] = device->pending[best_child];
		device->pending[best_child] = swap;
		cursor = best_child;
	}
}

t_request	queue_pop(t_device *device)
{
	t_request	removed;

	removed = device->pending[0];
	device->pending[0] = device->pending[device->queued - 1];
	device->queued--;
	queue_sift_down(device);
	return (removed);
}
