cc=gcc

files = memallocator-vipzone
      
all : $(files)

% : %.c
	$(cc) $< -m64 -o $@

clean :
	rm -f $(files)
