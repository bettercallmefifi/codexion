/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   parser.c                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: feel-idr <feel-idr@student.1337.ma>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/06 01:08:09 by feel-idr          #+#    #+#             */
/*   Updated: 2026/09/06 01:08:10 by feel-idr         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "header.h"

static int	is_positive_number(char *str)
{
	int	i;

	if (!str || !str[0])
		return (0);
	i = 0;
	while (str[i])
	{
		if (str[i] < '0' || str[i] > '9')
			return (0);
		i++;
	}
	return (1);
}

static int	parse_number(char *str, int min, int *value, char *name)
{
	if (!is_positive_number(str))
	{
		fprintf(stderr, "Error: '%s' must be a positive integer\n", name);
		return (1);
	}
	*value = atoi(str);
	if (*value < min)
	{
		fprintf(stderr, "Error: '%s' must be >= %d\n", name, min);
		return (1);
	}
	return (0);
}

static int	parse_scheduler(char *str, t_config *config)
{
	if (strcmp(str, "fifo") == 0)
		config->scheduler = FIFO;
	else if (strcmp(str, "edf") == 0)
		config->scheduler = EDF;
	else
	{
		fprintf(stderr, "Error: scheduler must be 'fifo' or 'edf'\n");
		return (1);
	}
	return (0);
}

int	parse_args(int argc, char **argv, t_config *config)
{
	if (argc != 9)
	{
		fprintf(stderr, "Error: expected 8 arguments, got %d\n", argc - 1);
		return (1);
	}
	if (parse_number(argv[1], 1, &config->nbr_of_coders,
			"number_of_coders")
		|| parse_number(argv[2], 1, &config->time_to_burnout,
			"time_to_burnout")
		|| parse_number(argv[3], 1, &config->time_to_compile,
			"time_to_compile")
		|| parse_number(argv[4], 1, &config->time_to_debug,
			"time_to_debug")
		|| parse_number(argv[5], 1, &config->time_to_refactor,
			"time_to_refactor")
		|| parse_number(argv[6], 1, &config->nbr_of_compiles_required,
			"number_of_compiles_required")
		|| parse_number(argv[7], 0, &config->dongle_cooldown,
			"dongle_cooldown")
		|| parse_scheduler(argv[8], config))
		return (1);
	return (0);
}
