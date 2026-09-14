/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   time_utils.c                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: feel-idr <feel-idr@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/13 19:48:00 by feel-idr          #+#    #+#             */
/*   Updated: 2026/09/13 19:48:00 by feel-idr         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "codexion.h"

long long	now_ms(void)
{
	struct timeval	tv;

	gettimeofday(&tv, NULL);
	return ((long long)tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

void	ms_to_timespec(long long ms, struct timespec *ts)
{
	ts->tv_sec = ms / 1000;
	ts->tv_nsec = (ms % 1000) * 1000000;
}

void	precise_sleep(t_sim *sim, long long ms)
{
	long long	end;

	end = now_ms() + ms;
	while (now_ms() < end)
	{
		if (!sim_running(sim))
			return ;
		usleep(200);
	}
}
