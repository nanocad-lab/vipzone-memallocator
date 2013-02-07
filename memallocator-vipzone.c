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
/*int check_for_keyboard_input() {
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
}*/

/* Args: 
	[1] Number of MB to allocate
	[2] Number of seconds to sleep between iterations. Both ints.
	[3] Allocator to use: g = malloc, m = mmap, v = vip_mmap (when applicable)
	[4] Access pattern: n = sequential, s = staggered, r = random
	[5] Write or read: w = write, r = read
	[6] Number of iterations
	*/
int main(const int argc, const char *argv[]) {
	unsigned long MB_to_alloc = 0; //How many MB we want to use for the test. Command-line argument.
	unsigned long page = 0; //for iterating through all allocated pages
	unsigned long total_pages = 0;
	unsigned long val = 0; //dummy variable
	unsigned long word = 0; //for iterating through words in a page
	unsigned long **page_array = NULL; //to store pointers to each page we allocate
	int allocator_choice = 0; //0: malloc, 1: mmap, 2: vip_malloc, 3: vip_mmap
	int stagger = 0;
	int staggerFlag = 0;
	int randomFlag = 0;
	unsigned long pageCount = 0;
	unsigned long wordsPerPage = PAGE_SIZE/sizeof(val);
	int writeFlag = 0;
	int sleepTime = 0;
	int numIters = 0;
	int iter = 1;

	srand(time(NULL));

	//Check arguments
	if (argc != 7) {
		fprintf(stderr, "Invalid number of arguments: (1) Amount of memory to hog in MB, (2) Number of sleep seconds between iterations, (3) Allocator to use (M[malloc], m[mmap], V[vip_malloc], v[vip_mmap]), (4) Access pattern (n[sequential], s[staggered], r[random]), (5) Write or read (w[write], r[read]) (6) Number of iterations\n");
		return 1;
	}

	//get arg 1
	MB_to_alloc = (unsigned long) atol(argv[1]); //Be nice, I didn't bother validating input ><
	printf("will alloc = %lu MB\n", MB_to_alloc);

	//get arg 2
	sleepTime = (int) atol(argv[2]);
	if (sleepTime < 0) {
		fprintf(stderr, "Sleep time was < 0s, setting to 0s.\n");
		sleepTime = 0;
	}
	printf("sleep time = %d s\n", sleepTime);


	//get arg 3
	if (argv[3][0] == 'v') { //use vip_mmap()
		allocator_choice = 3;
		printf("allocator = vip_mmap\n");
	} else if (argv[3][0] == 'm') { //use mmap()
		allocator_choice = 1;
		printf("allocator = mmap\n");
	} else if (argv[3][0] == 'M') {
		allocator_choice = 0; //use malloc()
		printf("allocator = malloc\n");
	} else if (argv[3][0] == 'V' ) {  //use vip_malloc()
		allocator_choice = 2;
		printf("allocator = vip_malloc\n");
	} else {
		fprintf(stderr, "Bad allocator choice\n");
		return 1;
	}

	//get arg 4
	if (argv[4][0] == 's') { //stagger
		staggerFlag = 1;
		printf("access pattern = staggered\n");
	} else if (argv[4][0] == 'r') { //random
		randomFlag = 1;
		printf("access pattern = random\n");
		printf("RAND_MAX = %d\n", RAND_MAX);
	} else if (argv[4][0] == 'n') { //sequential
		printf("access pattern = sequential\n");
		randomFlag = 0;
		staggerFlag = 0;
	} else { //bad
		fprintf(stderr, "Bad access pattern flag\n");
		return 1;
	}

	//get arg 5
	if (argv[5][0] == 'w') { //write
		writeFlag = 1;
		printf("access mode = write\n");
	} else if (argv[5][0] == 'r') { //read
		writeFlag = 0;
		printf("access mode = read\n");
	} else { 
		fprintf(stderr, "Bad write/read input\n");
		return 1;
	}

	//get arg 6
	numIters = (int)atoi(argv[6]);
	if (numIters < 1) {
		fprintf(stderr, "Number of iterations was < 1, setting to 1\n");
		numIters = 1;
	}
	printf("num iterations = %d\n", numIters);

	printf("\n==============================\n\n");

	val = 0xFFFFFFFFFFFFFFFF; //We will deal with purely all 1s in the allocated memory, for consistency.
	
	//allocate the huge array for storing page address pointers
	if (allocator_choice == 2) { //use vip_malloc
		printf("Getting memory pointer array using vip_malloc with vip_flags READ/HI...\n");
		if (!(page_array = vip_malloc(MAX_NUM_PAGES*sizeof(unsigned long *), _VIP_TYP_READ | _VIP_UTIL_HI))) {
			printf("Error: Could not allocate page array using vip_malloc (READ/HI). Terminating.\n");
			perror("Failure reason");
			return 1;
		}
 	} else if (allocator_choice == 3) { //use vip_mmap
		printf("Getting memory pointer array using vip_mmap with vip_flags READ/HI...\n");
		page_array = (unsigned long **) syscall(NR_vip_mmap, NULL, MAX_NUM_PAGES*sizeof(unsigned long *), (PROT_READ | PROT_WRITE | _VIP_TYP_READ | _VIP_UTIL_HI), (MAP_ANONYMOUS | MAP_PRIVATE), -1, 0);
		if (page_array == MAP_FAILED) {
			printf("Error: Could not allocate page array using vip_mmap (READ/HI). Terminating.\n");
			perror("Failure reason");
			return 1;
		}
	} else if (allocator_choice == 1) { //use mmap
		printf("Getting memory pointer array using mmap...\n");
		page_array = (unsigned long **) syscall(NR_mmap, NULL, MAX_NUM_PAGES*sizeof(unsigned long *), (PROT_READ | PROT_WRITE), (MAP_ANONYMOUS | MAP_PRIVATE), -1, 0);
		if (page_array == MAP_FAILED) {
			printf("Error: Could not allocate page array using mmap. Terminating.\n");
			perror("Failure reason");
			return 1;
		}
	} else { //use malloc()
		printf("Getting memory pointer array using malloc...\n");
		if (!(page_array = malloc(MAX_NUM_PAGES*sizeof(unsigned long *)))) {
			printf("Error: Could not allocate page array using malloc. Terminating.\n");
			perror("Failure reason");
			return 1;
		}
	}
	
	printf("sizeof(val) == %lu\nsizeof(page_array) == %lu\nsizeof(*(page_array)) == %lu\nsizeof(**page_array) == %lu\n", sizeof(val), sizeof(page_array), sizeof(*page_array), sizeof(**page_array));
	printf("PAGE_SIZE == %lu\nwordsPerPage == %lu\n", (unsigned long) PAGE_SIZE, (unsigned long) wordsPerPage);

	printf("\n==============================\n\n");

	//Hog memory
	if (allocator_choice == 2) { //vip_malloc
		printf("Hogging memory using vip_malloc...\n");
		while ((page < MAX_NUM_PAGES) && (page < MB_to_alloc*1024*1024/PAGE_SIZE)) {
			*(page_array+page) = vip_malloc(PAGE_SIZE, _VIP_TYP_READ | _VIP_UTIL_HI);
			if (*(page_array+page) == NULL) {
				printf("Error: Failed to vip_malloc page number %lu, at %0.2f MB.\n", page, (double)page*PAGE_SIZE/(1024*1024));
				perror("Failure reason");
				break;
			}
			page++;
		}
	} else if (allocator_choice == 3) { //vip_mmap
		printf("Hogging memory using vip_mmap with flags WRITE/HI...\n");
		while ((page < MAX_NUM_PAGES) && (page < MB_to_alloc*1024*1024/PAGE_SIZE) && (*(page_array+page) = (unsigned long *) syscall(NR_vip_mmap, NULL, PAGE_SIZE, (PROT_READ | PROT_WRITE | _VIP_TYP_WRITE | _VIP_UTIL_HI), (MAP_ANONYMOUS | MAP_PRIVATE), -1, 0)) != MAP_FAILED) 
			page++;
		if (*(page_array+page) == MAP_FAILED) {
			printf("Error: Failed to vip_mmap page number %lu, at %0.2f MB.\n", page, (double)page*PAGE_SIZE/(1024*1024));
			perror("Failure reason");
		}
	} else if (allocator_choice == 1) {  //mmap
		printf("Hogging memory using mmap...\n");
		while ((page < MAX_NUM_PAGES) && (page < MB_to_alloc*1024*1024/PAGE_SIZE) && (*(page_array+page) = (unsigned long *) syscall(NR_mmap, NULL, PAGE_SIZE, (PROT_READ | PROT_WRITE), (MAP_ANONYMOUS | MAP_PRIVATE), -1, 0)) != MAP_FAILED) 
			page++;
		if (*(page_array+page) == MAP_FAILED) {
			printf("Error: Failed to mmap page number %lu, at %0.2f MB.\n", page, (double)page*PAGE_SIZE/(1024*1024));
			perror("Failure reason");
		}
	} else { //malloc
		printf("Hogging memory using malloc...\n");
		while ((page < MAX_NUM_PAGES) && (page < MB_to_alloc*1024*1024/PAGE_SIZE)) {
			*(page_array+page) = malloc(PAGE_SIZE);
			if (*(page_array+page) == NULL) {
				printf("Error: Failed to malloc page number %lu, at %0.2f MB.\n", page, (double)page*PAGE_SIZE/(1024*1024));
				perror("Failure reason");
				break;
			}
			page++;
		}
	}

	total_pages = page;
	
	printf("We allocated %lu pages == %lu KB == %0.2f MB == %0.2f GB. Done.\n", total_pages, total_pages*PAGE_SIZE/1024, (double)total_pages*PAGE_SIZE/(1024*1024), (double)total_pages*PAGE_SIZE/(1024*1024*1024));
	
	printf("\n==============================\n\n");
	
	/************ WORKLOAD ************/
	if (writeFlag) { //User chose to write
		printf("Writing pattern: 0x%X %X\n", (unsigned int) (val>>32), (unsigned int) val);
		for (iter = 1; iter <= numIters; iter++) {
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
			
			printf("Finished write iteration %d\n", iter);
			/*if (check_for_keyboard_input())
				break;*/
		
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
		for (iter = 1; iter <= numIters; iter++) {
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

			printf("Finished read iteration %d\n", iter);
/*			if (check_for_keyboard_input())
				break;*/
			
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
	/*while (check_for_keyboard_input()) //flush stdin
		getchar();*/

	return 0; 
}


