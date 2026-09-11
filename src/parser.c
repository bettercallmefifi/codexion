/* ************************************************************************** */
/*                                                                            */
/*                                                       :::      ::::::::    */
/*   parser.c                                          :+:      :+:    :+:    */
/*                                                   +:+ +:+         +:+      */
/*   By: feel-idr <feel-idr@student.1337.ma>       +#+  +:+       +#+         */
/*                                               +#+#+#+#+#+   +#+            */
/*   Created: 2026/09/06 01:08:09 by feel-idr         #+#    #+#              */
/*   Updated: 2026/09/11 00:00:00 by feel-idr        ###   ########.fr        */
/*                                                                            */
/* ************************************************************************** */

#include "header.h"

static int	has_digits(char *text)
{
	int	slot;

	if (!text || !text[0])
		return (0);
	slot = 0;
	while (text[slot])
	{
		if (text[slot] < '0' || text[slot] > '9')
			return (0);
		slot++;
	}
	return (1);
}

static int	read_integer(char *text, int lower_bound, int *parsed, char *label)
{
	if (!has_digits(text))
	{
		fprintf(stderr, "Error: '%s' must be a positive integer\n", label);
		return (1);
	}
	*parsed = atoi(text);
	if (*parsed < lower_bound)
	{
		fprintf(stderr, "Error: '%s' must be >= %d\n", label, lower_bound);
		return (1);
	}
	return (0);
}

static int	read_policy(char *text, t_options *opts)
{
	if (strcmp(text, "fifo") == 0)
		opts->policy = POLICY_FIFO;
	else if (strcmp(text, "edf") == 0)
		opts->policy = POLICY_EDF;
	else
	{
		fprintf(stderr, "Error: scheduler must be 'fifo' or 'edf'\n");
		return (1);
	}
	return (0);
}

int	read_arguments(int arg_count, char **arg_values, t_options *opts)
{
	if (arg_count != 9)
	{
		fprintf(stderr, "Error: expected 8 arguments, got %d\n", arg_count - 1);
		return (1);
	}
	if (read_integer(arg_values[1], 1, &opts->worker_count,
			"number_of_coders")
		|| read_integer(arg_values[2], 1, &opts->burnout_ms,
			"time_to_burnout")
		|| read_integer(arg_values[3], 1, &opts->compile_ms,
			"time_to_compile")
		|| read_integer(arg_values[4], 1, &opts->debug_ms,
			"time_to_debug")
		|| read_integer(arg_values[5], 1, &opts->refactor_ms,
			"time_to_refactor")
		|| read_integer(arg_values[6], 1, &opts->cycle_limit,
			"number_of_compiles_required")
		|| read_integer(arg_values[7], 0, &opts->cooldown_ms,
			"dongle_cooldown")
		|| read_policy(arg_values[8], opts))
		return (1);
	return (0);
}
