/* ************************************************************************** */
/*                                                                            */
/*                                                       :::      ::::::::    */
/*   monitor.c                                         :+:      :+:    :+:    */
/*                                                   +:+ +:+         +:+      */
/*   By: feel-idr <feel-idr@student.1337.ma>       +#+  +:+       +#+         */
/*                                               +#+#+#+#+#+   +#+            */
/*   Created: 2026/09/06 01:08:02 by feel-idr         #+#    #+#              */
/*   Updated: 2026/09/11 00:00:00 by feel-idr        ###   ########.fr        */
/*                                                                            */
/* ************************************************************************** */

#include "header.h"

static int	detect_burnout(t_context *ctx, int slot)
{
	long	last_ms;
	long	begin_ms;
	int		expired;
	int		checked_id;

	expired = 0;
	pthread_mutex_lock(&ctx->output_lock);
	pthread_mutex_lock(&ctx->state_lock);
	last_ms = ctx->workers[slot].last_build_ms;
	if (last_ms == 0)
		last_ms = ctx->epoch_ms;
	if (ctx->workers[slot].finished == 0
		&& clock_ms() - last_ms > ctx->opts.burnout_ms)
	{
		ctx->active = 0;
		expired = 1;
	}
	checked_id = ctx->workers[slot].worker_id;
	begin_ms = ctx->epoch_ms;
	pthread_mutex_unlock(&ctx->state_lock);
	if (expired)
		printf("%ld %d burned out\n",
			clock_ms() - begin_ms, checked_id);
	pthread_mutex_unlock(&ctx->output_lock);
	return (expired);
}

static int	scan_workers(t_context *ctx)
{
	int	slot;
	int	finished_count;

	slot = 0;
	finished_count = 0;
	while (slot < ctx->opts.worker_count)
	{
		if (detect_burnout(ctx, slot))
			return (-1);
		pthread_mutex_lock(&ctx->state_lock);
		if (ctx->workers[slot].finished == 1)
			finished_count++;
		pthread_mutex_unlock(&ctx->state_lock);
		slot++;
	}
	return (finished_count);
}

void	*watchdog_main(void *payload)
{
	t_context	*ctx;
	int			finished_count;

	ctx = (t_context *)payload;
	while (1)
	{
		pthread_mutex_lock(&ctx->state_lock);
		if (ctx->active == 0)
		{
			pthread_mutex_unlock(&ctx->state_lock);
			return (NULL);
		}
		pthread_mutex_unlock(&ctx->state_lock);
		usleep(1000);
		finished_count = scan_workers(ctx);
		if (finished_count == -1)
			return (NULL);
		if (finished_count == ctx->opts.worker_count)
		{
			pthread_mutex_lock(&ctx->state_lock);
			ctx->active = 0;
			pthread_mutex_unlock(&ctx->state_lock);
			return (NULL);
		}
	}
}
