/* Match every k-character snippet of the query_doc document
	 among a collection of documents doc1, doc2, ....

	 ./rkmatch snippet_size query_doc doc1 [doc2...]

*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <strings.h>
#include <assert.h>
#include <time.h>

#include "bloom.h"

enum algotype { SIMPLE = 0, RK, RKBATCH};

/* a large prime for RK hash (BIG_PRIME*256 does not overflow)*/
long long BIG_PRIME = 5003943032159437; 

/* constants used for printing debug information */
const int PRINT_RK_HASH = 5;
const int PRINT_BLOOM_BITS = 160;

/* modulo addition */
long long
madd(long long a, long long b)
{
	return ((a+b)>BIG_PRIME?(a+b-BIG_PRIME):(a+b));
}

/* modulo substraction */
long long
mdel(long long a, long long b)
{
	return ((a>b)?(a-b):(a+BIG_PRIME-b));
}

/* modulo multiplication*/
long long
mmul(long long a, long long b)
{
	return ((a*b) % BIG_PRIME);
}

/* read the entire content of the file 'fname' into a 
	 character array allocated by this procedure.
	 Upon return, *doc contains the address of the character array
	 *doc_len contains the length of the array
	 */
void
read_file(const char *fname, char **doc, int *doc_len) 
{
	struct stat st;
	int fd;
	int n = 0;

	fd = open(fname, O_RDONLY);
	if (fd < 0) {
		perror("read_file: open ");
		exit(1);
	}

	if (fstat(fd, &st) != 0) {
		perror("read_file: fstat ");
		exit(1);
	}

	*doc = (char *)malloc(st.st_size);
	if (!(*doc)) {
		fprintf(stderr, " failed to allocate %d bytes. No memory\n", (int)st.st_size);
		exit(1);
	}

	n = read(fd, *doc, st.st_size);
	if (n < 0) {
		perror("read_file: read ");
		exit(1);
	}else if (n != st.st_size) {
		fprintf(stderr,"read_file: short read!\n");
		exit(1);
	}
	
	close(fd);
	*doc_len = n;
}




/* The normalize procedure examines a character array of size len 
	 in ONE PASS and does the following:
	 1) turn all upper case letters into lower case ones
	 2) turn any white-space character into a space character and, 
	    shrink any n>1 consecutive spaces into exactly 1 space only
			Hint: use C library function isspace() 
	 You must do the normalization IN PLACE so that when the procedure
	 returns, the character array buf contains the normalized string and 
	 the return value is the length of the normalized string.
*/
int
normalize(char *buf,	/* The character array containing the string to be normalized*/
					int len			/* the size of the original character array */)
{
	// Iterate through the string. Convert all uppercase to lowercase + convert all white-space characters into a single-space characters
	int i;

	for(i=0; i<len; i++){
		if(isspace(buf[i])!=0){ // If white-space
			buf[i] = ' '; // Convert any white-space into single-space character 
		}
		if(isspace(buf[i])==0){ // If string character
			buf[i] = tolower(buf[i]); // Convert any string character into lowercase
		}
	}

	/* Convert so that there is only 1 single-space character in a string. 
	   Also, eliminate any white-space before the first string character and after the last string character */
	
	// 3 Pointers
	int a = 0;
	int b = 0;
	int c = 0;

	// Integer "Check" that will work as a substitute for a boollean operator
	// Simply put, this will become a mark-point or a "switch" indicator
	int check = 0;

	while(c < (len-1)){ // While the pointer "c" is still within the string
		if(isspace(buf[c])==0){ // If buf[c] is a string character
			while(isspace(buf[b])==0){ // While buf[b] is a string character, increase pointer a & b by 1
				a++;
				b++;
				if(b == (len-1)){ // If buf[b] is a null character, set the position of pointer "c" to that of "b".
					c = b;
					check = 1; 
					break;
				}
			}
			while(isspace(buf[b])!=0){ // While buf[b] is a white-space character, increase pointer "b"
				b++;
				if((b-a)==2){ /* If the difference between "a" & "b" is at 2, the program will assign all pointers to the location between "a" & "b" 
				                 This is to ensure that if there exists any number of white-space characters between string-characters, it should be
				                 reduced to 1 single-space character */ 
					a = a+1; 
					b = a;
					c = a;
					check = 1;
					break;
				}
				if(b = (len-1)){ 
					buf[a] = buf[b]; // In order to reduce the white-space at the end of the string
					len = len - (b-a);
					check = 1;
					break;
				}
			}
			if(check == 1){ 
				int check = 0; // Set "check" back to 0 
				continue; // Continue to evaluate the character at the position
			}
			/* Set the positions of "a" & "c" to that of "b" because the white-space has been reduced 
			   and you want to begin again at the next string-character */
			a = b; 
			c = b; 
		}

		if(isspace(buf[c])!=0){ // this is white-space
			while(isspace(buf[b])!=0){ // while white-space
				b++;
			}
			if(b == (len-1)){ // If there is only white-space before NULL
				len = len - (b-a); 
				buf[a] = buf[b];
				c = a-1;
			}
			else{
				while(b <= (len-1)){
					buf[a] = buf[b];
					a++;
					b++;
				}
				len = len - (b-a); 

				//buf[a] = '\0';
				a=c; 
				b=c;
				check = 0; // Check needs to be set to 0 here. Otherwise it would skip characters.
			}
		} 
	}

	// // Get the Length of the string
	// len = 0;
	// for(i = 0; buf[i]!='\0'; i++){
	// 	len++;
	// }

	return len;
}

/* check if a query string ps (of length k) appears 
	 in ts (of length n) as a substring 
	 If so, return 1. Else return 0
	 You may want to use the library function strncmp
	 */
int
simple_match(const char *ps,	/* the query string */
						 int k, 					/* the length of the query string */
						 const char *ts,	/* the document string (Y) */ 
						 int n						/* the length of the document Y */)
{
	int i;

	for(i=0; i<n-k+1; i++){ 
		if(strncmp(ps,ts+i,k)==0){ /* ts+i because after k-size substring is compared from the beginning of the string to (n-k), 
                                      you move to the next string character of document string ts and do the same thing again.
                                      Also, strncmp() returns 0 is the comparing strings are equal. Therefore, if strncmp()==0, 
                                      it should return 1 */
			return 1;
		}
	}

	return 0;
}

/* Check if a query string ps (of length k) appears 
	 in ts (of length n) as a substring using the rabin-karp algorithm
	 If so, return 1. Else return 0
	 In addition, print the first 'PRINT_RK_HASH' hash values of ts
	 Example:
	 $ ./rkmatch -t 1 -k 20 X Y
	 605818861882592 812687061542252 1113263531943837 1168659952685767 4992125708617222 
	 0.01 matched: 1 out of 148
	 */


int
rabin_karp_match(const char *ps,	/* the query string */
								 int k, 					/* the length of the query string */
								 const char *ts,	/* the document string (Y) */ 
								 int n						/* the length of the document Y */ )
{
	int i;
	long long p = 0; // hash values for ps
	long long t = 0; // hash values for ts
	long long power[k]; // array to store the values of the different powers of 256
	power[0] = 1;

	for(i = 1; i <= k-1; i++){ //we have to remember the value of 256^(𝑘−1) in a variable instead of re-computing it for 𝑦𝑖 each time.
		power[i] = mmul(power[i-1],256);
	}

	for(i = 0; i < k; i++){ //hash(P[0...k − 1]) = 256^(𝑘−1) ∗ P[0] + 256^(𝑘−2) ∗ P[1] + ... + 256 ∗ P[k − 2] + P[k − 1]. For ts and ps each.
		p = madd(p, mmul(ps[i], power[k-1-i]));
		t = madd(t, mmul(ts[i], power[k-1-i]));
	}

	for(i = 0; i < n-k+1; i++){
		if(i < PRINT_RK_HASH){ //print the first 'PRINT_RK_HASH' hash values of ts
			printf("%lld ",t);
		}

		if((p == t) && strncmp(ps,ts+i,k) == 0){ //if the hash values equal, we should still compare the actual strings to see if they are identifcal
			printf("\n");
			return 1;
		}

		t = madd(mmul(mdel(t,mmul(ts[i],power[k-1])), 256),ts[k+i]); //y 𝑖+1 = 256 ∗ (y 𝑖 − 256^(𝑘−1) ∗ Y[i]) + Y[i + k]
		t %= BIG_PRIME;
	}

	printf("\n");
	return 0;
}

/* Initialize the bitmap for the bloom filter using bloom_init().
	 Insert all m/k RK hashes of qs into the bloom filter using bloom_add().
	 Then, compute each of the n-k+1 RK hashes of ts and check if it's in the filter using bloom_query().
	 Use the given procedure, hash_i(i, p), to compute the i-th bloom filter hash value for the RK value p.

	 Return the number of matched chunks. 
	 Additionally, print out the first PRINT_BLOOM_BITS of the bloom filter using the given bloom_print 
	 after inserting m/k substrings from qs.
*/
int
rabin_karp_batchmatch(int bsz,        /* size of bitmap (in bits) to be used */
                      int k,          /* chunk length to be matched */
                      const char *qs, /* query docoument (X)*/
                      int m,          /* query document length */ 
                      const char *ts, /* to-be-matched document (Y) */
                      int n           /* to-be-matched document length*/)
{
	bloom_filter f = bloom_init(bsz);
	int i;
	int j;
	int match = 0;
	long long q = 0; // hash for qs
	long long t = 0; // hash for ts
	long long power[k]; // array to store the values of the different powers of 256
	power[0] = 1;

	//printf("CALCULATE POWER\n");
	for(i = 1; i <= k-1; i++){ //we have to remember the value of 256^(𝑘−1) in a variable again.
		power[i] = mmul(power[i-1], 256);
	}
	//printf("DONE CALCULATING POWER\n");

	// for(i = 0; i < (m/k); i++)
	//printf("CALCULATE THE HASH FOR (X)\n");
	for(i = 0; i + k <= m; i+=k){ 
		// calculate the hash function for query document (X)
		long long q = 0;
		for(j = 0; j < k; j++){
			q = madd(q, mmul(qs[i+j], power[k-1-j]));
		}
		// q = hash_function(qs[i*k], k)
		bloom_add(f,q); // Add to the bloom filter
	}
	//printf("DONE CALCULATING THE HASH FOR (X)\n");

	bloom_print(f, PRINT_BLOOM_BITS); // Display the 160 bits in the bloom filter

	//printf("CALCULATE THE HASH FOR (Y)\n");
	for(i = 0; i < k; i++){ 
		t = madd(t, mmul(ts[i], power[k-1-i])); // calculate the hash function for to-be-match document (Y)
	}
	//printf("CALCULATE THE HASH FOR (Y)\n");

	//printf("LET'S SEE IF THE HASHES MATCH\n");
	for(i = 0; i < n-k+1; i++){

		if(bloom_query(f,t) == 1){ // If to-be-matched document (Y) is probably in the bloom filter

			// for(j = 0; j < m/k; j++)
			//printf("COMPARE SUBSTRINGS\n");
			for (j = 0; j + k <= m; j+=k){

				//if(strncmp(ts[i],qs[j*k],k) == 0)
				if(strncmp(ts+i, qs+j, k) == 0){ // If the corresponding substring of (Y) matches one of the [m/k] chunks of (X) increase count by 1
					match++; 
					break;
				}
			}
			//printf("DONE COMPARING SUBSTRING\n");
		}
		//printf("DONE SEEING IF THE HASHES MATCH\n");

	t = madd(mmul(mdel(t,mmul(ts[i],power[k-1])), 256),ts[k+i]);
	t %= BIG_PRIME;

	}

	return match;
}

int 
main(int argc, char **argv)
{
	int k = 100; /* default match size is 100*/ 
	int which_algo = SIMPLE; /* default match algorithm is simple */

	char *qdoc, *doc; 
	int qdoc_len, doc_len;
	int i;
	int num_matched = 0;
	int to_be_matched;
	int c;
    
	/* Refuse to run on platform with a different size for long long*/
	assert(sizeof(long long) == 8);

	//printf("HELLO\n");

	/*getopt is a C library function to parse command line options */
	while (( c = getopt(argc, argv, "t:k:q:")) != -1) {
		switch (c) 
		{
			case 't':
				/*optarg is a global variable set by getopt() 
					it now points to the text following the '-t' */
				which_algo = atoi(optarg);
				break;
			case 'k':
				k = atoi(optarg);
				break;
			case 'q':
				BIG_PRIME = atoi(optarg);
				break;
			default:
				fprintf(stderr,
						"Valid options are: -t <algo type> -k <match size> -q <prime modulus>\n");
				exit(1);
			}
	}

	/* optind is a global variable set by getopt() 
		 it now contains the index of the first argv-element 
		 that is not an option*/
	if (argc - optind < 1) {
		printf("Usage: ./rkmatch query_doc doc\n");
		exit(1);
	}

	//printf("HELLO1\n");

	/* argv[optind] contains the query_doc argument */
	read_file(argv[optind], &qdoc, &qdoc_len); 
	qdoc_len = normalize(qdoc, qdoc_len);

	/* argv[optind+1] contains the doc argument */
	read_file(argv[optind+1], &doc, &doc_len);
	doc_len = normalize(doc, doc_len);

	// char* q = "abc def ght";
	// char* p = "hello abc hello def";
	// int res = rabin_karp_batchmatch(((strlen(q)*10/3)>>3)<<3, 3, q, strlen(q), p, strlen(p));
	// printf("%d\n", res);

	switch (which_algo) 
		{
			case SIMPLE:
				/* for each of the qdoc_len/k chunks of qdoc, 
					 check if it appears in doc as a substring*/
				for (i = 0; (i+k) <= qdoc_len; i += k) {
					if (simple_match(qdoc+i, k, doc, doc_len)) {
						num_matched++;
					}
				}
				break;
			case RK:
				/* for each of the qdoc_len/k chunks of qdoc, 
					 check if it appears in doc as a substring using 
				   the rabin-karp substring matching algorithm */
				for (i = 0; (i+k) <= qdoc_len; i += k) {
					if (rabin_karp_match(qdoc+i, k, doc, doc_len)) {
						num_matched++;
					}
				}
				break;
			case RKBATCH:
				/* match all qdoc_len/k chunks simultaneously (in batch) by using a bloom filter*/
				num_matched = rabin_karp_batchmatch(((qdoc_len*10/k)>>3)<<3, k, qdoc, qdoc_len, doc, doc_len);
				break;
			default :
				fprintf(stderr,"Wrong algorithm type, choose from 0 1 2\n");
				exit(1);
		}
	
	to_be_matched = qdoc_len / k;
	printf("%.2f matched: %d out of %d\n", (double)num_matched/to_be_matched, 
			num_matched, to_be_matched);

	free(qdoc);
	free(doc);

	return 0;
}
