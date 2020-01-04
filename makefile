demo.run:$(shell find *.c *.h)
		@echo "\e[1;32mBuild...\e[0m"
		@gcc -o $@ $^ `pkg-config --libs --cflags cairo libv4l2` -w -g
all:demo.run
