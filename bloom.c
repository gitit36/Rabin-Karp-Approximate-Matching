/***********************************************************
 Implementation of bloom filter goes here 
 **********************************************************/

#include "bloom.h"

/* Constants for bloom filter implementation */
const int H1PRIME = 4189793;
const int H2PRIME = 3296731;
const int BLOOM_HASH_NUM = 10;

/* The hash function used by the bloom filter */
int
hash_i(int i, /* which of the BLOOM_HASH_NUM hashes to use */ 
       long long x /* a long long value to be hashed */)
{
	return ((x % H1PRIME) + i*(x % H2PRIME) + 1 + i*i);
}

/* Initialize a bloom filter by allocating a character array that can pack bsz bits.
   (each char represents 8 bits)
   Furthermore, clear all bits for the allocated character array. 
   Hint:  use the malloc and bzero library function 
   Return value is the newly initialized bloom_filter struct.*/

bloom_filter 
bloom_init(int bsz /* size of bitmap to allocate in bits*/ )
{
	bloom_filter f;
	f.bsz = bsz;

	int arr_size = (bsz/8) + 1; // size of memory to be allocated in bytes
	f.buf = calloc(arr_size, sizeof(char)); // f.buf is a pointer to a allocated space of a character type
							                // calloc() automatically sets the allocated memory to 0
	return f;
}

/* Add elm into the given bloom filter*/
void
bloom_add(bloom_filter f,
          long long elm /* the element to be added (a RK hash value) */)
{
	int i = 0;
	int hash;
	int byteloc;
	//printf("%d\n", f.bsz);

	//printf("BLOOM_ADD WHILE LOOP STARTS\n");
	while(i < BLOOM_HASH_NUM){

		hash = hash_i(i,elm) % f.bsz; // getting the modulus of the hash value so that the value doesn't go out of range
		byteloc = hash / 8; // divide by 8 so we can determine the byte at the location

		f.buf[byteloc] |= (1 << (7 - (hash % 8))); // Basically can be re-written as, 
												   // 00000000 = 00000000 OR (00000001 << ((8-1)-scale)).
												   // (8 - 1) because 1 is already taking up space in a total of 8 bits.
												   // Adds 1 to the bitmap.
		i++;
	}
	//printf("BLOOM_ADD WHILE LOOP ENDS\n");

	return;
}

/* Query if elm is probably in the given bloom filter */ 
int
bloom_query(bloom_filter f,
            long long elm /* the query element */ )
{	
	int i = 0;
	int cnt = 0;
	int hash;
	int byteloc; 

	//printf("BLOOM_HASH_NUM WHILE LOOP STARTS\n");
	while(i < BLOOM_HASH_NUM){

		hash = hash_i(i,elm) % f.bsz; 
		byteloc = hash / 8;

		if((f.buf[byteloc] & (1 << (7 - (hash % 8)))) != 0){ // If the hashed query element matches the values in a bitmap using AND operator. 
			cnt ++;
		}
		i ++;
	}
	if (cnt == BLOOM_HASH_NUM) return 1;
	//printf("BLOOM_HASH_NUM WHILE LOOP STARTS\n");

	return 0;
}

void 
bloom_free(bloom_filter *f)
{
	free(f->buf);
	f->buf = f->bsz = 0;
}

/* print out the first count bits in the bloom filter */
void
bloom_print(bloom_filter f,
            int count     /* number of bits to display*/ )
{
	int i;

	assert(count % 8 == 0);

	for(i=0; i< (f.bsz>>3) && i < (count>>3); i++) {
		printf("%02x ", (unsigned char)(f.buf[i]));
	}
	printf("\n");
	return;
}

