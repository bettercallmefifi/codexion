/* ************************************************************************** */
/*                                                                            */
/*                                                       :::      ::::::::    */
/*   coder.c                                           :+:      :+:    :+:    */
/*                                                   +:+ +:+         +:+      */
/*   By: feel-idr <feel-idr@student.1337.ma>       +#+  +:+       +#+         */
/*                                               +#+#+#+#+#+   +#+            */
/*   Created: 2026/09/06 01:07:19 by feel-idr         #+#    #+#              */
/*   Updated: 2026/09/11 00:00:00 by feel-idr        ###   ########.fr        */
/*                                                                            */
/* ************************************************************************** */

#include "header.h"

void	*worker_main(void *payload)
{
	t_worker	*worker;

	worker = (t_worker *)payload;
	if (worker->worker_id % 2 != 0)
		usleep(1000);
	while (worker->finished == 0)
	{
		pthread_mutex_lock(&worker->ctx->state_lock);
		if (worker->ctx->active == 0)
		{
			pthread_mutex_unlock(&worker->ctx->state_lock);
			break ;
		}
		pthread_mutex_unlock(&worker->ctx->state_lock);
		if (run_compile(worker))
			break ;
		run_debug(worker);
		run_refactor(worker);
	}
	return (NULL);
}

void	wait_interval(t_worker *worker, long delay_ms)
{
	long	begin_ms;

	begin_ms = clock_ms();
	while (clock_ms() - begin_ms < delay_ms)
	{
		pthread_mutex_lock(&worker->ctx->state_lock);
		if (worker->ctx->active == 0)
		{
			pthread_mutex_unlock(&worker->ctx->state_lock);
			return ;
		}
		pthread_mutex_unlock(&worker->ctx->state_lock);
		usleep(1000);
	}
}

long	clock_ms(void)
{
	struct timeval	stamp;

	gettimeofday(&stamp, NULL);
	return (stamp.tv_sec * 1000 + stamp.tv_usec / 1000);
}
