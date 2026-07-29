NAME = webserver

CXX = c++
CXXFLAGS =  -Wall -Wextra -Werror -std=c++98 -Iincludes

SRCS =	src/main.cpp \
		src/network/Socket.cpp \
		src/network/Client.cpp \
		src/http/HttpRequest.cpp \
		src/http/HttpResponse.cpp \
		src/http/RequestParser.cpp \
		src/http/ResponseBuilder.cpp \
		src/http/Router.cpp \
		src/server/Server.cpp \
		src/server/ConfigParser.cpp \
		src/server/CgiHandler.cpp \
		src/server/LocationConfig.cpp \
		src/server/ServerConfig.cpp  


OBJS_DIR = obj
OBJS = $(SRCS:%.cpp=$(OBJS_DIR)/%.o)


# Make silent
# MAKEFLAGS += --silent

all: $(NAME)

$(NAME): $(OBJS) 
	@echo "Compiling $(NAME).."
	$(CXX) $(CXXFLAGS) -o $(NAME) $(OBJS)
	@echo "$(NAME) ready!"

$(OBJS_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJS_DIR):
	mkdir -p $(OBJS_DIR)

clean:
	@echo "Cleaning objects..."
	rm -rf $(OBJS_DIR)
	@echo "Objects cleaned!"

fclean: clean
	@echo "Removing executable..."
	rm -f $(NAME)
	@echo "Full clean done!"

re: fclean all

.PHONY: all clean fclean re
