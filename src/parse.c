/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   parse.c                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: feel-idr <feel-idr@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/13 19:48:00 by feel-idr          #+#    #+#             */
/*   Updated: 2026/09/13 19:48:00 by feel-idr         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "codexion.h"

static int	usage_error(void)
{
	fprintf(stderr, "Usage: ./codexion number_of_coders time_to_burnout ");
	fprintf(stderr, "time_to_compile time_to_debug time_to_refactor ");
	fprintf(stderr, "number_of_compiles_required dongle_cooldown <fifo|edf>\n");
	fprintf(stderr, "Values must be integers >= 0, time_to_burnout > 0, ");
	fprintf(stderr, "number_of_coders between 1 and %d.\n", MAX_CODERS);
	return (0);
}

static int	parse_number(const char *str, long long *out)
{
	long long	value;
	int			i;

	value = 0;
	i = 0;
	if (!str[0])
		return (0);
	while (str[i])
	{
		if (str[i] < '0' || str[i] > '9')
			return (0);
		value = value * 10 + (str[i] - '0');
		if (value > 2147483647)
			return (0);
		i++;
	}
	*out = value;
	return (1);
}

static int	parse_times(t_sim *sim, char **av)
{
	long long	val[6];
	int			i;

	i = 0;
	while (i < 6)
	{
		if (!parse_number(av[i + 2], &val[i]))
			return (0);
		i++;
	}
	sim->burnout = val[0];
	sim->compile_ms = val[1];
	sim->debug_ms = val[2];
	sim->refactor_ms = val[3];
	sim->nb_compiles = (int)val[4];
	sim->cooldown = val[5];
	return (sim->burnout > 0);
}

static int	parse_sched(t_sim *sim, const char *str)
{
	if (!strcmp(str, "fifo"))
		sim->mode = MODE_FIFO;
	else if (!strcmp(str, "edf"))
		sim->mode = MODE_EDF;
	else
		return (0);
	return (1);
}

int	parse_args(t_sim *sim, int ac, char **av)
{
	long long	nb;

	if (ac != 9)
		return (usage_error());
	if (!parse_number(av[1], &nb) || nb < 1 || nb > MAX_CODERS)
		return (usage_error());
	sim->nb_coders = (int)nb;
	if (!parse_times(sim, av))
		return (usage_error());
	if (!parse_sched(sim, av[8]))
		return (usage_error());
	return (1);
}
