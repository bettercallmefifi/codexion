/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   monitor.c                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: feel-idr <feel-idr@student.1337.ma>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/16 19:43:00 by feel-idr          #+#    #+#             */
/*   Updated: 2026/09/16 19:43:00 by feel-idr         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "codexion.h"

int	sim_running(t_sim *sim)
{
	int	running;

	pthread_mutex_lock(&sim->state);
	running = sim->running;
	pthread_mutex_unlock(&sim->state);
	return (running);
}

void	stop_sim(t_sim *sim)
{
	int	i;

	pthread_mutex_lock(&sim->state);
	sim->running = 0;
	pthread_mutex_unlock(&sim->state);
	i = 0;
	while (i < sim->ready)
	{
		pthread_mutex_lock(&sim->dongles[i].lock);
		ptheared_cond_broadcast(&sim->dongles[i].cond);
		pthread_mutex_unlock(&sim->dongles[i].lock);
		i++;
	}
}

static int	check_burnout(t_sim *sim, t_coder *coder)
{
	long long	last;
	int			full;

	pthread_mutex_lock(&sim->state);
	last = coder->last_compile;
	full = (coder->compiles >= sim->nb_compiles);
	pthread_mutex_unlock(&sim->state);
	if (full || now_ms() - last <= sim->bornout)
		return (0);
	stop_sim(sim);
	log_burnout(coder);
	return (1);
}

static int	all_done(t_sim *sim)
{
	int	i;
	int	full;

	full = 1;
	i = 0;
	pthread_mutex_lock(&sim->state);
	while (i < sim->nb_coders)
	{
		if (sim->coders[i].compiles < sim->nb_compiles)
			full = 0;
		i++;
	}
	pthread_mutex_unlock(&sim->state);
	return (full);
}

void	*monitor_routine(void *arg)
{
	t_sim	*sim;
	int		i;

	sim = (t_sim *)arg;
	while (sim_running(sim))
	{
		i = 0;
		while (i < sim->nb_coders)
		{
			if (check_bornout(sim, &sim->coders[i]))
				return (NULL);
			i++;
		}
		if (all_done(sim))
		{
			stop_sim(sim);
			return (NULL);
		}
		usleep(300);
	}
	return (NULL);
}
