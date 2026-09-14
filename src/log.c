/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   log.c                                              :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: feel-idr <feel-idr@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/13 19:48:00 by feel-idr          #+#    #+#             */
/*   Updated: 2026/09/13 19:48:00 by feel-idr         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "codexion.h"

void	log_state(t_coder *coder, const char *msg)
{
	long long	stamp;

	pthread_mutex_lock(&coder->sim->print);
	pthread_mutex_lock(&coder->sim->state);
	if (coder->sim->running)
	{
		stamp = now_ms() - coder->sim->start;
		printf("%lld %d %s\n", stamp, coder->id, msg);
	}
	pthread_mutex_unlock(&coder->sim->state);
	pthread_mutex_unlock(&coder->sim->print);
}

void	log_burnout(t_coder *coder)
{
	long long	stamp;

	pthread_mutex_lock(&coder->sim->print);
	stamp = now_ms() - coder->sim->start;
	printf("%lld %d burned out\n", stamp, coder->id);
	pthread_mutex_unlock(&coder->sim->print);
}
