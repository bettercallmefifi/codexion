/* ************************************************************************** */
/*                                                                            */
/*                                                       :::      ::::::::    */
/*   dongles_config.c                                  :+:      :+:    :+:    */
/*                                                   +:+ +:+         +:+      */
/*   By: feel-idr <feel-idr@student.1337.ma>       +#+  +:+       +#+         */
/*                                               +#+#+#+#+#+   +#+            */
/*   Created: 2026/09/06 01:07:11 by feel-idr         #+#    #+#              */
/*   Updated: 2026/09/11 00:00:00 by feel-idr        ###   ########.fr        */
/*                                                                            */
/* ************************************************************************** */

#include "header.h"

static long	worker_deadline(t_worker *worker)
{
	return (worker->last_build_ms
		+ worker->ctx->opts.burnout_ms);
}

static void	prepare_request(t_worker *worker, t_request *request)
{
	request->worker_id = worker->worker_id;
	if (worker->ctx->opts.policy == POLICY_FIFO)
		request->priority_ms = clock_ms();
	else
		request->priority_ms = worker_deadline(worker);
}

static int	wait_device(t_worker *worker, t_device *device)
{
	while (1)
	{
		if (device->queued > 0
			&& device->busy == 0
			&& device->pending[0].worker_id == worker->worker_id
			&& clock_ms() - device->released_ms
			>= worker->ctx->opts.cooldown_ms)
			return (0);
		pthread_mutex_unlock(&device->lock);
		usleep(1000);
		pthread_mutex_lock(&worker->ctx->state_lock);
		if (worker->ctx->active == 0)
		{
			pthread_mutex_unlock(&worker->ctx->state_lock);
			pthread_mutex_lock(&device->lock);
			return (1);
		}
		pthread_mutex_unlock(&worker->ctx->state_lock);
		pthread_mutex_lock(&device->lock);
	}
}

int	acquire_device(t_worker *worker, t_device *device)
{
	t_request	request;

	pthread_mutex_lock(&device->lock);
	prepare_request(worker, &request);
	queue_push(device, request);
	if (wait_device(worker, device))
	{
		pthread_mutex_unlock(&device->lock);
		return (1);
	}
	queue_pop(device);
	device->busy = 1;
	pthread_mutex_unlock(&device->lock);
	return (0);
}

void	release_device(t_device *device)
{
	pthread_mutex_lock(&device->lock);
	device->busy = 0;
	device->released_ms = clock_ms();
	pthread_mutex_unlock(&device->lock);
}
