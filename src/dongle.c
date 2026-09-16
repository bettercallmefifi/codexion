/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   dongle.c                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: feel-idr <feel-idr@student.1337.ma>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/16 18:54:57 by feel-idr          #+#    #+#             */
/*   Updated: 2026/09/16 18:54:57 by feel-idr         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "codexion.h"

static t_request	build_request(t_coder *coder)
{
	t_request	req;

	pthread_mutex_lock(&coder->sim->state);
	coder->sim->seq++;
	req.seq = coder->sim->seq;
	req.deadline = coder->last_compile + coder->sim->burnout;
	pthread_mutex_unlock(&coder->sim->state);
	req.id = coder->id;
	return (req);
}

static int	dongle_ready(t_dongle *dongle, t_coder *coder)
{
	t_request	*top;

	if (dongle->taken)
		return (0);
	if (now_ms() < dongle->free_at)
		return (0);
	top = heap_peek(&dongle->queue);
	if (!top)
		return (0);
	return (top->id == coder->id);
}

static int	wait_for_turn(t_coder *coder, t_dongle *dongle)
{
	struct timespec	ts;

	while (!dongle_ready(dongle, coder))
	{
		if (!sim_running(coder->sim))
			return (0);
		ms_to_timespec(now_ms() + 1, &ts);
		pthread_cond_timedwait(&dongle->cond, &dongle->lock, &ts);
	}
	return (1);
}

int	take_dongle(t_coder *coder, t_dongle *dongle)
{
	int	granted;

	pthread_mutex_lock(&dongle->lock);
	if (!heap_push(&dongle->queue, build_request(coder)))
	{
		pthread_mutex_unlock(&dongle->lock);
		return (0);
	}
	granted = wait_for_turn(coder, dongle);
	if (granted)
	{
		heap_pop(&dongle->queue);
		dongle->taken = 1;
	}
	else
		heap_remove(&dongle->queue, coder->id);
	pthread_cond_broadcast(&dongle->cond);
	pthread_mutex_unlock(&dongle->lock);
	if (granted)
		log_state(coder, "has taken a dongle");
	return (granted);
}

void	drop_dongle(t_sim *sim, t_dongle *dongle)
{
	pthread_mutex_lock(&dongle->lock);
	dongle->taken = 0;
	dongle->free_at = now_ms() + sim->cooldown;
	pthread_cond_broadcast(&dongle->cond);
	pthread_mutex_unlock(&dongle->lock);
}
