hdrs:=apf.h   hexdump.h                    die.h
srcs:=apf.cpp hexdump.cpp apf_messages.cpp       apfd.cpp
libs:=absl_strings absl_flags_parse absl_str_format

apfd: $(hdrs) $(srcs) Makefile
	g++ -ggdb -Wall -Werror $(srcs) $(shell pkg-config --libs $(libs)) -o apfd

clean:
	$(RM) apfd *.o
