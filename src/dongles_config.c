/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   dongles_config.c                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: feel-idr <feel-idr@student.1337.ma>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/06 01:07:11 by feel-idr          #+#    #+#             */
/*   Updated: 2026/09/06 01:07:12 by feel-idr         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */


#include "header.h"

static long	get_deadline(t_coder *coder)
{
	return (coder->last_compile
		+ coder->sim->config.time_to_burnout);
}

static void	set_deadline(t_coder *coder, t_edf *info)
{
	info->id = coder->id;
	if (coder->sim->config.scheduler == FIFO)
		info->deadline = get_time_ms();
	else
		info->deadline = get_deadline(coder);
}

static int	wait_dongle(t_coder *coder, t_dongle *dongle)
{
	while (1)
	{
		if (dongle->size > 0
			&& dongle->is_taken == 0
			&& dongle->quee[0].id == coder->id
			&& get_time_ms() - dongle->release
			>= coder->sim->config.dongle_cooldown)
			return (0);
		pthread_mutex_unlock(&dongle->pause_dongle);
		usleep(1000);
		pthread_mutex_lock(&coder->sim->pause);
		if (coder->sim->simulation_running == 0)
		{
			pthread_mutex_unlock(&coder->sim->pause);
			pthread_mutex_lock(&dongle->pause_dongle);
			return (1);
		}
		pthread_mutex_unlock(&coder->sim->pause);
		pthread_mutex_lock(&dongle->pause_dongle);
	}
}

int	take_dongle(t_coder *coder, t_dongle *dongle)
{
	t_edf	info;

	pthread_mutex_lock(&dongle->pause_dongle);
	set_deadline(coder, &info);
	insert_heap(dongle, info);
	if (wait_dongle(coder, dongle))
	{
		pthread_mutex_unlock(&dongle->pause_dongle);
		return (1);
	}
	pop_heap(dongle);
	dongle->is_taken = 1;
	pthread_mutex_unlock(&dongle->pause_dongle);
	return (0);
}

void	release_dongle(t_dongle *dongle)
{
	pthread_mutex_lock(&dongle->pause_dongle);
	dongle->is_taken = 0;
	dongle->release = get_time_ms();
	pthread_mutex_unlock(&dongle->pause_dongle);
}
