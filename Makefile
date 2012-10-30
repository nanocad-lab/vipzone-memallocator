cc=gcc

files = memallocator
      
all : $(files)

% : %.c
	$(cc) $< -m64 -o $@

clean :
	rm -f $(files)
