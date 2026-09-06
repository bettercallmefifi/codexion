/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: feel-idr <feel-idr@student.1337.ma>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/06 01:07:50 by feel-idr          #+#    #+#             */
/*   Updated: 2026/09/06 01:07:52 by feel-idr         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "header.h"

int	main(int argc, char **argv)
{
	t_config		config;
	t_simulation	sim;
	pthread_t		monitor;
	int				i;

	if (parse_args(argc, argv, &config)
		|| init_simulation(&sim, &config))
		return (1);
	pthread_create(&monitor, NULL, monitor_routine, &sim);
	i = 0;
	while (i < config.nbr_of_coders)
	{
		pthread_create(&sim.coders[i].thread, NULL,
			coder_routine, &sim.coders[i]);
		i++;
	}
	i = 0;
	while (i < config.nbr_of_coders)
	{
		pthread_join(sim.coders[i].thread, NULL);
		i++;
	}
	pthread_join(monitor, NULL);
	cleanup_simulation(&sim);
	return (0);
}
