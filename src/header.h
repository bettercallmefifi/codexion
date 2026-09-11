/* ************************************************************************** */
/*                                                                            */
/*                                                       :::      ::::::::    */
/*   header.h                                          :+:      :+:    :+:    */
/*                                                   +:+ +:+         +:+      */
/*   By: feel-idr <feel-idr@student.1337.ma>       +#+  +:+       +#+         */
/*                                               +#+#+#+#+#+   +#+            */
/*   Created: 2026/09/06 01:07:29 by feel-idr         #+#    #+#              */
/*   Updated: 2026/09/11 00:00:00 by feel-idr        ###   ########.fr        */
/*                                                                            */
/* ************************************************************************** */

#ifndef HEADER_H
# define HEADER_H

# include <pthread.h>
# include <stdio.h>
# include <stdlib.h>
# include <unistd.h>
# include <string.h>
# include <sys/time.h>

typedef struct s_context	t_context;

typedef enum e_policy
{
	POLICY_FIFO,
	POLICY_EDF
}	t_policy;

typedef struct s_request
{
	int		worker_id;
	long	priority_ms;
}	t_request;

typedef struct s_options
{
	int			worker_count;
	int			burnout_ms;
	int			compile_ms;
	int			debug_ms;
	int			refactor_ms;
	int			cycle_limit;
	int			cooldown_ms;
	t_policy	policy;
}	t_options;

typedef struct s_device
{
	pthread_mutex_t	lock;
	t_request		pending[2];
	int				queued;
	int				busy;
	long			released_ms;
}	t_device;

typedef struct s_worker
{
	int			worker_id;
	int			finished;
	pthread_t	handle;
	t_device	*left_device;
	t_device	*right_device;
	long		last_build_ms;
	int			build_count;
	t_context	*ctx;
}	t_worker;

typedef struct s_context
{
	pthread_mutex_t	output_lock;
	pthread_mutex_t	state_lock;
	t_options		opts;
	t_worker		*workers;
	t_device		*devices;
	int				active;
	long			epoch_ms;
}	t_context;

int			read_arguments(int arg_count, char **arg_values, t_options *opts);
int			setup_context(t_context *ctx, t_options *opts);
void		destroy_context(t_context *ctx);
void		*worker_main(void *payload);
int			acquire_device(t_worker *worker, t_device *device);
void		release_device(t_device *device);
long		clock_ms(void);
void		queue_push(t_device *device, t_request request);
void		queue_sift_down(t_device *device);
t_request	queue_pop(t_device *device);
int			run_compile(t_worker *worker);
void		run_debug(t_worker *worker);
void		run_refactor(t_worker *worker);
void		*watchdog_main(void *payload);
void		wait_interval(t_worker *worker, long delay_ms);

#endif
