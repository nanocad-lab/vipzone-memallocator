/* Author: Mark Gottscho
*  Written for testing memory power use on a per-DIMM basis, on top of a modified or vanilla Linux kernel.
*/

#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_NUM_PAGES 4194304 //Equals 16GB assuming 4KB pages
#define PAGE_SIZE 4096 //in bytes
#define DEBUG_FLAG 0

/* vip_mmap stuff */
#include <sys/mman.h>
#define NR_vip_mmap 312
#define NR_mmap 9

#define _VIP_TYP_READ 0x0000
#define _VIP_TYP_WRITE 0x0010
#define _VIP_UTIL_LO 0x0000
#define _VIP_UTIL_HI 0x0020

//From Internet somewhere
int check_for_keyboard_input() {
	fd_set rfds;
	struct timeval tv;
	int retval = 0;

	FD_ZERO(&rfds);
	FD_SET(0, &rfds);

	tv.tv_sec = 0;
	tv.tv_usec = 0;
        
	retval = select(1, &rfds, NULL, NULL, &tv);
        //tv is now unreliable

        if (retval == -1)
            printf("error: select()\n");
	return retval;
}

// Args: Number of MB to allocate, number of seconds to sleep between iterations. Both ints.
int main(const int argc, const char *argv[]) {
	unsigned long MB_to_alloc = 0; //How many MB we want to use for the test. Command-line argument.
	unsigned long page = 0; //for iterating through all allocated pages
	unsigned long total_pages = 0;
	unsigned long val = 0; //dummy variable
	unsigned long word = 0; //for iterating through words in a page
	unsigned long **page_array = NULL; //to store pointers to each page we allocate
	char choice = '\0'; //for simple keyboard input
	int iter = 1; //counter
	int allocator_choice = 0; //0: malloc, 1: mmap, 2: vip_mmap
	int stagger = 0;
	int staggerFlag = 0;
	int randomFlag = 0;
	unsigned long pageCount = 0;
	unsigned long wordsPerPage = PAGE_SIZE/sizeof(val);
	int writeFlag = 0;
	int sleepTime = 0;

	srand(time(NULL));

	//Check arguments
	if (argc != 3) {
		printf("Error: Need to specify amount of memory to hog in MB, and cycle sleep time in seconds (>= 0). Terminating.\n");
		return 1;
	}

	//User: malloc() or vip_mmap()?
	choice = '\0';
	do {
		printf("Use GLIBC malloc(), syscall mmap, or syscall vip_mmap for memory allocation? (g/m/v): ");
		choice = getchar();
		getchar();
	} while (choice != 'g' && choice != 'm' && choice != 'v');

	if (choice == 'v') //use vip_mmap()
		allocator_choice = 2;
	else if (choice == 'm') //use mmap()
		allocator_choice = 1;
	else
		allocator_choice = 0; //use malloc()


	//allocate the huge array for storing page address pointers
	if (allocator_choice == 2) { //use vip_mmap
		printf("Getting memory pointer array using vip_mmap with vip_flags READ/HI...\n");
		page_array = (unsigned long **) syscall(NR_vip_mmap, NULL, MAX_NUM_PAGES*sizeof(unsigned long *), (PROT_READ | PROT_WRITE | _VIP_TYP_READ | _VIP_UTIL_HI), (MAP_ANONYMOUS | MAP_PRIVATE), -1, 0);
		if (page_array == MAP_FAILED) {
			printf("Error: Could not allocate page array using vip_mmap (READ/HI). Terminating.\n");
			perror("vip_mmap");
			return 1;
		}
	} else if (allocator_choice == 1) { //use mmap
		printf("Getting memory pointer array using mmap...\n");
		page_array = (unsigned long **) syscall(NR_mmap, NULL, MAX_NUM_PAGES*sizeof(unsigned long *), (PROT_READ | PROT_WRITE), (MAP_ANONYMOUS | MAP_PRIVATE), -1, 0);
		if (page_array == MAP_FAILED) {
			printf("Error: Could not allocate page array using mmap. Terminating.\n");
			perror("mmap");
			return 1;
		}
	} else { //use malloc()
		printf("Getting memory pointer array using malloc...\n");
		if (!(page_array = malloc(MAX_NUM_PAGES*sizeof(unsigned long *)))) {
			printf("Error: Could not allocate page array using malloc. Terminating.\n");
			perror("malloc");
			return 1;
		}
	}
	
	printf("sizeof(val) == %lu\nsizeof(page_array) == %lu\nsizeof(*(page_array)) == %lu\nsizeof(**page_array) == %lu\n", sizeof(val), sizeof(page_array), sizeof(*page_array), sizeof(**page_array));
	printf("PAGE_SIZE == %lu\nwordsPerPage == %lu\n", (unsigned long) PAGE_SIZE, (unsigned long) wordsPerPage);
	
	MB_to_alloc = (unsigned long) atol(argv[1]); //Be nice, I didn't bother validating input ><
	sleepTime = (int) atol(argv[2]);
	if (sleepTime < 0) {
		fprintf(stderr, "Sleep time was < 0s, setting to 0s.\n");
		sleepTime = 0;
	}

	//Hog memory
	if (allocator_choice == 2) {
		printf("Hogging memory using vip_mmap with flags WRITE/HI...\n");
		while ((page < MAX_NUM_PAGES) && (page < MB_to_alloc*1024*1024/PAGE_SIZE) && (*(page_array+page) = (unsigned long *) syscall(NR_vip_mmap, NULL, PAGE_SIZE, (PROT_READ | PROT_WRITE | _VIP_TYP_WRITE | _VIP_UTIL_HI), (MAP_ANONYMOUS | MAP_PRIVATE), -1, 0)) != MAP_FAILED) 
			page++;
		if (*(page_array+page) == MAP_FAILED) {
			printf("Error: Failed to vip_mmap page number %lu, at %0.2f MB.\n", page, (double)page*PAGE_SIZE/(1024*1024));
			perror("vip_mmap");
		}
	} else if (allocator_choice == 1) {  //mmap
		printf("Hogging memory using mmap...\n");
		while ((page < MAX_NUM_PAGES) && (page < MB_to_alloc*1024*1024/PAGE_SIZE) && (*(page_array+page) = (unsigned long *) syscall(NR_mmap, NULL, PAGE_SIZE, (PROT_READ | PROT_WRITE), (MAP_ANONYMOUS | MAP_PRIVATE), -1, 0)) != MAP_FAILED) 
			page++;
		if (*(page_array+page) == MAP_FAILED) {
			printf("Error: Failed to mmap page number %lu, at %0.2f MB.\n", page, (double)page*PAGE_SIZE/(1024*1024));
			perror("mmap");
		}
	} else { //malloc
		printf("Hogging memory using malloc...\n");
		while ((page < MAX_NUM_PAGES) && (page < MB_to_alloc*1024*1024/PAGE_SIZE) && (*(page_array+page) = malloc(PAGE_SIZE))) 
			page++;
		if (*(page_array+page) == NULL) {
			printf("Error: Failed to malloc page number %lu, at %0.2f MB.\n", page, (double)page*PAGE_SIZE/(1024*1024));
			perror("malloc");
		}
	}

	total_pages = page;
	
	printf("We allocated %lu pages == %lu KB == %0.2f MB == %0.2f GB. Done.\n", total_pages, total_pages*PAGE_SIZE/1024, (double)total_pages*PAGE_SIZE/(1024*1024), (double)total_pages*PAGE_SIZE/(1024*1024*1024));
	
	//User: Stagger, random, or sequential?
	do {
		printf("Staggered words or random page accesses? (s/r/n)\n");
		choice = getchar();
		getchar();
	} while (choice != 's' && choice != 'r' && choice != 'n');
	
	if (choice == 's') { //stagger
		staggerFlag = 1;
		printf("Staggered accesses enabled!\n");
	} else if (choice == 'r') { //random
		randomFlag = 1;
		printf("Random accesses enabled!\n");
		printf("RAND_MAX = %d\n", RAND_MAX);
	} else if (choice == 'n') { //sequential
		printf("Sequential accesses enabled!\n");
	}

	choice = '\0';
	
	//User: write or read?
	do {
		printf("Type 'w' to infinite write, 'r' to infinite read.\nPress ENTER at any time to stop.\n");
		choice = getchar();
		getchar();
	} while (choice != 'w' && choice != 'r');
	
	val = 0xFFFFFFFFFFFFFFFF; //We will deal with purely all 1s in the allocated memory, for consistency.
	
	if (choice == 'w')
		writeFlag = 1;
	
	/************ WORKLOAD ************/
	if (writeFlag) { //User chose to write
		printf("Writing pattern: 0x%X %X\n", (unsigned int) (val>>32), (unsigned int) val);
		while (1) {
			if (randomFlag) { //randomize pages
				for (pageCount = 0; pageCount < total_pages; pageCount++) {
					page = rand() % total_pages;
					for (word = 0; word < wordsPerPage; word++) { //for each word in the page
						*(*(page_array+page)+word) = val; //Write all 1s to the word
#if DEBUG_FLAG == 1	
							fprintf(stderr, "page %d, word %d: page_array+page == 0x%X, *(page_array+page) == 0x%X, *(page_array+page)+word == 0x%X, *(*(page_array+page)+word) == 0x%X\n", page, word, page_array+page, *(page_array+page), *(page_array+page)+word, *(*(page_array+page)+word));
#endif
					}
				}
			} 
			else if (staggerFlag) { //staggered words
				for (page = 0; page < total_pages; page++) { //for each page
					for (stagger = 0; stagger < 4; stagger++)
						for (word = 0; word < wordsPerPage; word=word+4) {
							*(*(page_array+page)+stagger+word) = val; //Write all 1s to the word
#if DEBUG_FLAG == 1	
							fprintf(stderr, "page %d, word %d: page_array+page == 0x%X, *(page_array+page) == 0x%X, *(page_array+page)+word == 0x%X, *(*(page_array+page)+word) == 0x%X\n", page, word, page_array+page, *(page_array+page), *(page_array+page)+word, *(*(page_array+page)+word));
#endif
						}
				}
			} 
			else { //purely sequential
				for (page = 0; page < total_pages; page++)
					for (word = 0; word < wordsPerPage; word++) { //for each word in the page
						*(*(page_array+page)+word) = val; //Write all 1s to the word
#if DEBUG_FLAG == 1	
							fprintf(stderr, "page %d, word %d: page_array+page == 0x%X, *(page_array+page) == 0x%X, *(page_array+page)+word == 0x%X, *(*(page_array+page)+word) == 0x%X\n", page, word, page_array+page, *(page_array+page), *(page_array+page)+word, *(*(page_array+page)+word));
#endif
					}
			}
			
			printf("Finished write iteration %d\n", iter++);
			if (check_for_keyboard_input())
				break;
		
			//Check sleep mode
			if (sleepTime > 0)
				sleep(sleepTime);
		}
	}
	else { //User chose to read
		//Init memory space before the read loop
		printf("Initializing memory with pattern: 0x%X %X\n", (unsigned int) (val>>32), (unsigned int) val);
		for (page = 0; page < total_pages; page++) //for each page
			for (word = 0; word < wordsPerPage; word++) //for each word in the page
				*(*(page_array+page)+word) = val; //Write all 1s to the word
		
		printf("Reading...\n");
		while (1) {
			if (randomFlag) { //randomize pages
				for (pageCount = 0; pageCount < total_pages; pageCount++) {
					page = rand() % total_pages;
					for (word = 0; word < wordsPerPage; word++) { //for each word in the page
						val = *(*(page_array+page)+word); //Get the value
#if DEBUG_FLAG == 1	
							fprintf(stderr, "page %d, word %d: page_array+page == 0x%X, *(page_array+page) == 0x%X, *(page_array+page)+word == 0x%X, *(*(page_array+page)+word) == 0x%X\n", page, word, page_array+page, *(page_array+page), *(page_array+page)+word, *(*(page_array+page)+word));
#endif
					}
				}
			} 
			else if (staggerFlag) { //staggered words
				for (page = 0; page < total_pages; page++) { //for each page
					for (stagger = 0; stagger < 4; stagger++)
						for (word = 0; word < wordsPerPage; word=word+4) { //for each word in the page
							val = *(*(page_array+page)+stagger+word); //Get the value
#if DEBUG_FLAG == 1	
							fprintf(stderr, "page %d, word %d: page_array+page == 0x%X, *(page_array+page) == 0x%X, *(page_array+page)+word == 0x%X, *(*(page_array+page)+word) == 0x%X\n", page, word, page_array+page, *(page_array+page), *(page_array+page)+word, *(*(page_array+page)+word));
#endif
		 				}
				}
			}
			else { //purely sequential
				for (page = 0; page < total_pages; page++)
					for (word = 0; word < wordsPerPage; word++) { //for each word in the page
						val = *(*(page_array+page)+word); //Get the value
#if DEBUG_FLAG == 1	
							fprintf(stderr, "page %d, word %d: page_array+page == 0x%X, *(page_array+page) == 0x%X, *(page_array+page)+word == 0x%X, *(*(page_array+page)+word) == 0x%X\n", page, word, page_array+page, *(page_array+page), *(page_array+page)+word, *(*(page_array+page)+word));
#endif
					}
			}

			printf("Finished read iteration %d\n", iter++);
			if (check_for_keyboard_input())
				break;
			
			//Check sleep mode
			if (sleepTime > 0)
				sleep(sleepTime);
		}
	}		

	//free memory here
	printf("Freeing memory...\n");
	if (allocator_choice == 1 || allocator_choice == 2) { //vip_mmap or mmap
		for (page = 0; page < total_pages; page++)
			munmap(*(page_array+page), PAGE_SIZE);
		munmap(page_array, MAX_NUM_PAGES);
	} else { //malloc
		for (page = 0; page < total_pages; page++)
			free(*(page_array+page));
		free(page_array);
	}
	
	printf("Done.\n");
	while (check_for_keyboard_input()) //flush stdin
		getchar();

	return 0; 
}


