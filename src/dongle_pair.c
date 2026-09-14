/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   dongle_pair.c                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: feel-idr <feel-idr@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/13 19:48:00 by feel-idr          #+#    #+#             */
/*   Updated: 2026/09/13 19:48:00 by feel-idr         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "codexion.h"

int	take_two(t_coder *coder)
{
	t_dongle	*first;
	t_dongle	*second;

	first = &coder->sim->dongles[coder->left];
	second = &coder->sim->dongles[coder->right];
	if (coder->id % 2 == 0)
	{
		first = &coder->sim->dongles[coder->right];
		second = &coder->sim->dongles[coder->left];
	}
	if (!take_dongle(coder, first))
		return (0);
	if (!take_dongle(coder, second))
	{
		drop_dongle(coder->sim, first);
		return (0);
	}
	return (1);
}

void	drop_two(t_coder *coder)
{
	drop_dongle(coder->sim, &coder->sim->dongles[coder->left]);
	drop_dongle(coder->sim, &coder->sim->dongles[coder->right]);
}
