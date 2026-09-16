/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   init.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: feel-idr <feel-idr@student.1337.ma>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/16 16:40:39 by feel-idr          #+#    #+#             */
/*   Updated: 2026/09/16 16:40:39 by feel-idr         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "codexion.h"

static int	init_dongles(t_sim *sim)
{
	sim->dongles = malloc(sizeof(t_dongle) * sim->nb_coders);
	if (!sim->dongles)
		return (0);
	memset(sim->dongles, 0, sizeof(t_dongle) * sim->nb_coders);
	while (sim->ready < sim->nb_coders)
	{
		if (!heap_init(&sim->dongles[sim->ready].queue, sim->mode))
			return (0);
		pthread_mutex_init(&sim->dongles[sim->ready].lock, NULL);
		pthread_cond_init(&sim->dongles[sim->ready].cond, NULL);
		sim->ready++;
	}
	return (1);
}

static int	init_coders(t_sim *sim)
{
	int	i;

	sim->coders = malloc(sizeof(t_coder) * sim->nb_coders);
	if (!sim->coders)
		return (0);
	memset(sim->coders, 0, sizeof(t_coder) * sim->nb_coders);
	i = 0;
	while (i < sim->nb_coders)
	{
		sim->coders[i].id = i + 1;
		sim->coders[i].left = i;
		sim->coders[i].right = (i + 1) % sim->nb_coders;
		sim->coders[i].sim = sim;
		i++;
	}
	return (1);
}

int	init_sim(t_sim *sim)
{
	sim->running = 1;
	sim->seq = 0;
	sim->ready = 0;
	pthread_mutex_init(&sim->state, NULL);
	pthread_mutex_init(&sim->print, NULL);
	if (!init_dongles(sim))
		return (0);
	if (!init_coders(sim))
		return (0);
	return (1);
}
