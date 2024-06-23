hdrs:=apf.h hexdump.h die.h mem_extract.h
srcs:=apf.cpp hexdump.cpp apf_messages.cpp apfd.cpp
libs:=absl_strings absl_flags_parse absl_str_format

ahi_hdrs:=ahi.h ahi_messages.h die.h mem_extract.h hexdump.h
ahi_srcs:=ahi.cpp ahi_messages.cpp ahi_info.cpp hexdump.cpp

apfd: $(hdrs) $(srcs) Makefile
	g++ -ggdb -Wall -Werror $(srcs) $(shell pkg-config --libs $(libs)) -o apfd

ahi_info: $(ahi_hdrs) $(ahi_srcs) Makefile
	g++ -ggdb -Wall -Werror $(ahi_srcs) $(shell pkg-config --libs $(libs)) -o ahi_info

clean:
	$(RM) apfd ahi_info *.o
