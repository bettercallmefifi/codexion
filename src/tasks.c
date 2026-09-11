/* ************************************************************************** */
/*                                                                            */
/*                                                       :::      ::::::::    */
/*   tasks.c                                           :+:      :+:    :+:    */
/*                                                   +:+ +:+         +:+      */
/*   By: feel-idr <feel-idr@student.1337.ma>       +#+  +:+       +#+         */
/*                                               +#+#+#+#+#+   +#+            */
/*   Created: 2026/09/06 01:08:20 by feel-idr         #+#    #+#              */
/*   Updated: 2026/09/11 00:00:00 by feel-idr        ###   ########.fr        */
/*                                                                            */
/* ************************************************************************** */

#include "header.h"

static void	log_devices(t_worker *worker)
{
	long	elapsed_ms;

	pthread_mutex_lock(&worker->ctx->output_lock);
	elapsed_ms = clock_ms() - worker->ctx->epoch_ms;
	printf("%ld %d has taken a dongle\n", elapsed_ms, worker->worker_id);
	printf("%ld %d has taken a dongle\n", elapsed_ms, worker->worker_id);
	pthread_mutex_unlock(&worker->ctx->output_lock);
}

static int	acquire_pair(t_worker *worker)
{
	if (worker->worker_id % 2 == 0)
	{
		if (acquire_device(worker, worker->left_device))
			return (1);
		if (acquire_device(worker, worker->right_device))
		{
			release_device(worker->left_device);
			return (1);
		}
	}
	else
	{
		if (acquire_device(worker, worker->right_device))
			return (1);
		if (acquire_device(worker, worker->left_device))
		{
			release_device(worker->right_device);
			return (1);
		}
	}
	log_devices(worker);
	return (0);
}

int	run_compile(t_worker *worker)
{
	long	compile_begin;

	if (acquire_pair(worker))
		return (1);
	compile_begin = clock_ms();
	pthread_mutex_lock(&worker->ctx->output_lock);
	printf("%ld %d is compiling\n",
		compile_begin - worker->ctx->epoch_ms, worker->worker_id);
	pthread_mutex_unlock(&worker->ctx->output_lock);
	pthread_mutex_lock(&worker->ctx->state_lock);
	worker->last_build_ms = compile_begin;
	worker->build_count++;
	pthread_mutex_unlock(&worker->ctx->state_lock);
	wait_interval(worker, worker->ctx->opts.compile_ms);
	release_device(worker->left_device);
	release_device(worker->right_device);
	return (0);
}

void	run_debug(t_worker *worker)
{
	long	debug_begin;

	pthread_mutex_lock(&worker->ctx->state_lock);
	if (worker->ctx->active == 0)
	{
		pthread_mutex_unlock(&worker->ctx->state_lock);
		return ;
	}
	pthread_mutex_unlock(&worker->ctx->state_lock);
	debug_begin = clock_ms();
	pthread_mutex_lock(&worker->ctx->output_lock);
	printf("%ld %d is debugging\n",
		debug_begin - worker->ctx->epoch_ms, worker->worker_id);
	pthread_mutex_unlock(&worker->ctx->output_lock);
	wait_interval(worker, worker->ctx->opts.debug_ms);
}

void	run_refactor(t_worker *worker)
{
	long	refactor_begin;

	pthread_mutex_lock(&worker->ctx->state_lock);
	if (worker->ctx->active == 0)
	{
		pthread_mutex_unlock(&worker->ctx->state_lock);
		return ;
	}
	pthread_mutex_unlock(&worker->ctx->state_lock);
	refactor_begin = clock_ms();
	pthread_mutex_lock(&worker->ctx->output_lock);
	printf("%ld %d is refactoring\n",
		refactor_begin - worker->ctx->epoch_ms, worker->worker_id);
	pthread_mutex_unlock(&worker->ctx->output_lock);
	wait_interval(worker, worker->ctx->opts.refactor_ms);
	pthread_mutex_lock(&worker->ctx->state_lock);
	if (worker->build_count
		== worker->ctx->opts.cycle_limit)
		worker->finished = 1;
	pthread_mutex_unlock(&worker->ctx->state_lock);
}
