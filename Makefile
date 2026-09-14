NAME		= codexion

CC			= cc
CFLAGS		= -Wall -Wextra -Werror -pthread
INC_DIR		= includes
SRC_DIR		= src
OBJ_DIR		= obj

SRCS		= main.c parse.c init.c cleanup.c time_utils.c log.c \
			  heap.c heap_utils.c dongle.c dongle_pair.c coder.c monitor.c
OBJS		= $(addprefix $(OBJ_DIR)/, $(SRCS:.c=.o))

all: $(NAME)

$(NAME): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(NAME)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(INC_DIR)/codexion.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I$(INC_DIR) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re
