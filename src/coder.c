/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   coder.c                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: feel-idr <feel-idr@student.1337.ma>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/16 17:09:24 by feel-idr          #+#    #+#             */
/*   Updated: 2026/09/16 17:09:24 by feel-idr         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "codexion.h"

static void	do_compile(t_coder *coder)
{
	pthread_mutex_lock(&coder->sim->state);
	coder->last_compile = now_ms();
	coder->compiles++;
	pthread_mutex_unlock(&coder->sim->state);
	log_state(coder, "is compiling");
	precise_sleep(coder->sim, coder->sim->compile_ms);
}

static int	coder_done(t_coder *coder)
{
	int	done;

	pthread_mutex_lock((&coder->sim->state));
	done = (coder->compiles >= coder->sim->nb_compiles);
	pthread_mutex_unlock(&coder->sim->state);
	return (done);
}

static void	*lone_coder(t_coder *coder)
{
	take_dongle(coder, &coder->sim->dongles[0]);
	while (sim_running(coder->sim))
		usleep(300);
	return (NULL);
}

void	*coder_routine(void *arg)
{
	t_coder	*coder;

	coder = (t_coder *)arg;
	if (coder->sim->nb_coders == 1)
		return (lone_coder(coder));
	if (coder->id % 2 == 0)
		precise_sleep(coder->sim, 1);
	while (sim_running(coder->sim) && !coder_done(coder))
	{
		if (!take_two(coder))
			break ;
		do_compile(coder);
		drop_two(coder);
		if (coder_done(coder))
			break ;
		log_state(coder, "is debugging");
		precise_sleep(coder->sim, coder->sim->debug_ms);
		log_state(coder, "is refactoring");
		precise_sleep(coder->sim, coder->sim->refactor_ms);
	}
	return (NULL);
}
