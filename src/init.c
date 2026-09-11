/* ************************************************************************** */
/*                                                                            */
/*                                                       :::      ::::::::    */
/*   init.c                                            :+:      :+:    :+:    */
/*                                                   +:+ +:+         +:+      */
/*   By: feel-idr <feel-idr@student.1337.ma>       +#+  +:+       +#+         */
/*                                               +#+#+#+#+#+   +#+            */
/*   Created: 2026/09/06 01:07:43 by feel-idr         #+#    #+#              */
/*   Updated: 2026/09/11 00:00:00 by feel-idr        ###   ########.fr        */
/*                                                                            */
/* ************************************************************************** */

#include "header.h"

static void	init_workers(t_context *ctx)
{
	int	slot;

	slot = 0;
	while (slot < ctx->opts.worker_count)
	{
		ctx->workers[slot].worker_id = slot + 1;
		ctx->workers[slot].finished = 0;
		ctx->workers[slot].left_device = &ctx->devices[slot];
		ctx->workers[slot].right_device = &ctx->devices[
			(slot + 1) % ctx->opts.worker_count];
		ctx->workers[slot].build_count = 0;
		ctx->workers[slot].ctx = ctx;
		ctx->workers[slot].last_build_ms = 0;
		ctx->devices[slot].queued = 0;
		ctx->devices[slot].busy = 0;
		ctx->devices[slot].released_ms = 0;
		pthread_mutex_init(&ctx->devices[slot].lock, NULL);
		slot++;
	}
}

int	setup_context(t_context *ctx, t_options *opts)
{
	ctx->opts = *opts;
	ctx->active = 1;
	ctx->epoch_ms = clock_ms();
	pthread_mutex_init(&ctx->output_lock, NULL);
	pthread_mutex_init(&ctx->state_lock, NULL);
	ctx->workers = malloc(sizeof(t_worker)
			* ctx->opts.worker_count);
	if (!ctx->workers)
		return (1);
	ctx->devices = malloc(sizeof(t_device)
			* ctx->opts.worker_count);
	if (!ctx->devices)
	{
		free(ctx->workers);
		return (1);
	}
	init_workers(ctx);
	return (0);
}

void	destroy_context(t_context *ctx)
{
	int	slot;

	slot = 0;
	while (slot < ctx->opts.worker_count)
	{
		pthread_mutex_destroy(&ctx->devices[slot].lock);
		slot++;
	}
	pthread_mutex_destroy(&ctx->output_lock);
	pthread_mutex_destroy(&ctx->state_lock);
	free(ctx->workers);
	free(ctx->devices);
}
