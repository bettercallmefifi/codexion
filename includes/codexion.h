/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   codexion.h                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: feel-idr <feel-idr@student.1337.ma>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/16 15:39:52 by feel-idr          #+#    #+#             */
/*   Updated: 2026/09/16 15:39:52 by feel-idr         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CODEXION_H
# define CODEXION_H

# include <pthread.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <sys/time.h>
# include <unistd.h>

# define MAX_CODERS 200
# define HEAP_CAPACITY 2
# define MODE_FIFO 0
# define MODE_EDF 1

typedef struct s_request
{
	int			id;
	long long	seq;
	long long	deadline;
}	t_request;

typedef struct s_heap
{
	t_request	*data;
	int			size;
	int			capacity;
	int			mode;
}	t_heap;

typedef struct s_dongle
{
	int				taken;
	long long		free_at;
	t_heap			queue;
	pthread_mutex_t	lock;
	pthread_cond_t	cond;
}	t_dongle;

typedef struct s_sim	t_sim;

typedef struct s_coder
{
	int			id;
	int			left;
	int			right;
	int			compiles;
	long long	last_compile;
	pthread_t	thread;
	t_sim		*sim;
}	t_coder;

struct	s_sim
{
	int				nb_coders;
	int				nb_compiles;
	int				mode;
	int				running;
	int				ready;
	long long		burnout;
	long long		compile_ms;
	long long		debug_ms;
	long long		refactor_ms;
	long long		cooldown;
	long long		start;
	long long		seq;
	t_coder			*coders;
	t_dongle		*dongles;
	pthread_t		monitor;
	pthread_mutex_t	state;
	pthread_mutex_t	print;
};

int			parse_args(t_sim *sim, int ac, char **av);
int			init_sim(t_sim *sim);
void		destroy_sim(t_sim *sim);
long long	now_ms(void);
void		ms_to_timespec(long long ms, struct timespec *ts);
void		precise_sleep(t_sim *sim, long long ms);
void		log_state(t_coder *coder, const char *msg);
void		log_burnout(t_coder *coder);
int			heap_init(t_heap *heap, int mode);
void		heap_free(t_heap *heap);
int			heap_push(t_heap *heap, t_request req);
int			heap_pop(t_heap *heap);
t_request	*heap_peek(t_heap *heap);
int			heap_less(t_heap *heap, t_request *a, t_request *b);
void		heap_swap(t_request *a, t_request *b);
void		sift_up(t_heap *heap, int i);
void		sift_down(t_heap *heap, int i);
int			heap_remove(t_heap *heap, int id);
int			take_dongle(t_coder *coder, t_dongle *dongle);
void		drop_dongle(t_sim *sim, t_dongle *dongle);
int			take_two(t_coder *coder);
void		drop_two(t_coder *coder);
void		*coder_routine(void *arg);
void		*monitor_routine(void *arg);
int			sim_running(t_sim *sim);
void		stop_sim(t_sim *sim);

#endif
