#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <stdlib.h>

// https://gist.github.com/badboy/6267743
uint32_t hash6432shift(uint64_t key) {
    key = (~key) + (key << 18);
    key ^= key >> 31;
    key *= 21;
    key ^= key >> 11;
    key += key << 6;
    key ^= key >> 22;
    return 0xFFFFFFFF & key;
}


uint64_t inv_hash6432shift(uint32_t hash, uint32_t trunc) {
	/* Invert: key & 0xFFFFFFFF */
	uint64_t key = hash | ((uint64_t)trunc << 32);
	
	/* Invert: key ^= key >> 22; */
	key ^= key >> 22;
	key ^= key >> 44;
	
	/* Invert: key += key << 6; */
	//key -= key << 6;
	//key += key << 12;
	//key += key << 24;
	//key += key << 48;
	key *= 0xFC0FC0FC0FC0FC1U;
	
	/* Invert: key ^= key >> 11; */
	key ^= key >> 11;
	key ^= key >> 22;
	key ^= key >> 44;
	
	/* Invert: key *= 21; */
	//key -= key << 2;
	//key += key << 6;
	//key += key << 12;
	//key += key << 24;
	//key += key << 48;
	key *= 0xCF3CF3CF3CF3CF3DU;
	
	/* Invert: key ^= key >> 31; */
	key ^= key >> 31;
	key ^= key >> 62;
	
	/* Invert:  key = (~key) + (key << 18); */
	//uint64_t ktmp = 0;
	//ktmp = ~(key - (ktmp << 18));
	//ktmp = ~(key - (ktmp << 18));
	//ktmp = ~(key - (ktmp << 18));
	//ktmp = ~(key - (ktmp << 18));
	//key = ktmp;
	key++;
	key *= 0xFFBFFFEFFFFBFFFFU;
	
	/* Done */
	return key;
}


#define RANDU64() (((uint64_t)(rand() & 0x7FFF) << 49) | \
                   ((uint64_t)(rand() & 0x7FFF) << 34) | \
				   ((uint64_t)(rand() & 0x7FFF) << 19) | \
				   ((uint64_t)(rand() & 0x7FFF) <<  4) | \
				   ((uint64_t)(rand() & 0x7FFF) >> 11))

int main(void) {
    srand(time(0));
    
	/* Good practice to do it like this, it ensures that the target hash
	   is one that we know can be outputted by the hash function */
    uint64_t randinput = RANDU64();
    uint32_t targethash = hash6432shift(randinput);
    
    printf("Target:\n");
    printf("%016llx -> %08x\n\n", randinput, targethash);
    
    printf("Preimages:\n");
	
	uint32_t trunc = 0;
    do {
        uint64_t preimage = inv_hash6432shift(targethash, trunc);
		uint32_t hash = hash6432shift(preimage);
		
		/* For debugging purposes */
		if (hash != targethash) {
			fprintf(stderr, "FAIL: H(%016llx) = %08x, but expected %08x (hash=%08x, trunc=%%08x)\n", preimage, hash, targethash, trunc);
			return 1;
		}
		
		printf("%016llx -> %08x\n", preimage, hash);
    } while (++i != 0);
	
	return 0;
}