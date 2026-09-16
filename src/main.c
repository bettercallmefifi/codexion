/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: feel-idr <feel-idr@student.1337.ma>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/16 15:56:18 by feel-idr          #+#    #+#             */
/*   Updated: 2026/09/16 15:56:18 by feel-idr         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "codexion.h"

static int	start_threads(t_sim *sim, int *made)
{
	sim->start = now_ms();
	*made = 0;
	while (*made < sim->nb_coders)
	{
		sim->coders[*made].last_compile = sim->start;
		(*made)++;
	}
	*made = 0;
	while (*made < sim->nb_coders)
	{
		if (pthread_create(&sim->coders[*made].thread, NULL,
				coder_routine, &sim->coders[*made]) != 0)
			return (0);
		(*made)++;
	}
	if (pthread_create(&sim->monitor, NULL, monitor_routine, sim) != 0)
		return (0);
	return (1);
}

static void	join_threads(t_sim *sim, int made, int monitor_up)
{
	int	i;

	if (monitor_up)
		pthread_join(sim->monitor, NULL);
	i = 0;
	while (i < made)
	{
		pthread_join(sim->coders[i].thread, NULL);
		i++;
	}
}

int	main(int ac, char **av)
{
	t_sim	sim;
	int		made;
	int		started;

	memset(&sim, 0, sizeof(t_sim));
	if (!parse_args(&sim, ac, av))
		return (1);
	if (sim.nb_compiles == 0)
		return (0);
	if (!init_sim(&sim))
	{
		destroy_sim(&sim);
		return (1);
	}
	made = 0;
	started = start_threads(&sim, &made);
	if (!started)
		stop_sim(&sim);
	join_threads(&sim, made, started);
	destroy_sim(&sim);
	return (!started);
}
