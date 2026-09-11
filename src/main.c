/* ************************************************************************** */
/*                                                                            */
/*                                                       :::      ::::::::    */
/*   main.c                                            :+:      :+:    :+:    */
/*                                                   +:+ +:+         +:+      */
/*   By: feel-idr <feel-idr@student.1337.ma>       +#+  +:+       +#+         */
/*                                               +#+#+#+#+#+   +#+            */
/*   Created: 2026/09/06 01:07:50 by feel-idr         #+#    #+#              */
/*   Updated: 2026/09/11 00:00:00 by feel-idr        ###   ########.fr        */
/*                                                                            */
/* ************************************************************************** */

#include "header.h"

int	main(int arg_count, char **arg_values)
{
	t_options	opts;
	t_context	ctx;
	pthread_t	watchdog;
	int			slot;

	if (read_arguments(arg_count, arg_values, &opts)
		|| setup_context(&ctx, &opts))
		return (1);
	pthread_create(&watchdog, NULL, watchdog_main, &ctx);
	slot = 0;
	while (slot < opts.worker_count)
	{
		pthread_create(&ctx.workers[slot].handle, NULL,
			worker_main, &ctx.workers[slot]);
		slot++;
	}
	slot = 0;
	while (slot < opts.worker_count)
	{
		pthread_join(ctx.workers[slot].handle, NULL);
		slot++;
	}
	pthread_join(watchdog, NULL);
	destroy_context(&ctx);
	return (0);
}
