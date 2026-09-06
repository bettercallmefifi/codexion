NAME	= codexion

CC		= cc
CFLAGS	= -Wall -Wextra -Werror -pthread
HEADER	= src/header.h

SRC		= src/main.c \
		  src/parser.c \
		  src/init.c \
		  src/coder.c \
		  src/dongles_config.c \
		  src/heap.c \
		  src/monitor.c \
		  src/tasks.c

OBJ		= $(SRC:.c=.o)

all: $(NAME)

$(NAME): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $(NAME)

%.o: %.c $(HEADER)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re