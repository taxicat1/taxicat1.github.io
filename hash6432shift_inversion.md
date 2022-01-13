<script type="text/x-mathjax-config">
  MathJax.Hub.Config({
    tex2jax: {
      inlineMath: [ ['$$','$$'], ["\\(","\\)"] ],
      displayMath: [ ['$$','$$'], ["\\(","\\)"] ],
    },
    TeX: {
      Macros: {
        bra: ["\\langle{#1}|", 1],
        ket: ["|{#1}\\rangle", 1],
        braket: ["\\langle{#1}\\rangle", 1],
        bk: ["\\langle{#1}|{#2}|{#3}\\rangle", 3]
     }
   }
  });
</script>

<script src='https://cdnjs.cloudflare.com/ajax/libs/mathjax/2.7.7/latest.js?config=TeX-MML-AM_CHTML' async></script>





# Reversing an integer hash function

This is an integer hash function written by Thomas Wang
(https://gist.github.com/badboy/6267743):

```C
uint32_t hash6432shift(uint64_t key) {
    key = (~key) + (key << 18);
    key ^= key >> 31;
    key *= 21;
    key ^= key >> 11;
    key += key << 6;
    key ^= key >> 22;
    return 0xFFFFFFFF & key;
}
```

It hashes down a 64-bit input to a 32-bit output. It has good mixing: basic statistical analysis can show that it has reasonably good avalanche effect if lacking in bit independence (certain pairs of bits of the output like to flip at the same time when the input changes in a specific way, in some cases more than 99% of the time). However this hash function has a more glaring flaw which is the lack of fan-out. Fan-out is necessary for a hash function, otherwise you could simply trace backwards and generate *an* input that produces a specific hash, this is called a preimage of the hash. Most hash functions accomplish this using some compression function `C` along with input data or state `i` and compute `C(i) + i`. This pattern now means that attempting to trace the process backwards requires first taking a guess at what the input may have been, and then tracing backwards, which often results in a contradiction when the reversed value is different from the initially guessed input— contradictions like this are the essence of fan-out.

However in this case it is possible to work each step backwards, starting with taking a wild guess at which the 32 truncated bits may be at the end.The remaining steps, which add or xor a shift of the original data, are fully invertible. Therefore, every possible guess at the truncated bits can be traced back to a valid preimage.

## Inverting xor-s of shifts
So let’s look at the next line before the truncation:

```C
key ^= key >> 22;
```

Looking at it, we can see that the uppermost 22 bits of `key` are unaffected by the operation. Consider a visualization of a simpler example, say `x ^ (x >> 7)` for some 16-bit wide string `x`:

```
  1011101011010010  - Some input value
^        101110101  - Shifted right by 7
 -----------------
  1011101110100111  - Output value
  *******  - Unaffected bits
```

Therefore, those upper 22 bits can be xor-ed against bits 22-44 to obtain another group of 22 bits (remember xor is its own inverse). And then those 44 bits can be xor-ed against the next bits, and so on to the end of any length bitstring with any right shift amount. In the simpler example:

```
  1011101110100111
^        1011101    - Upper bits shifted right
  -----------------
  10111010110100    - Now 14 bits recovered


  1011101110100111
^        101110101  - Known 14 bits shifted right
  -----------------
  1011101011010010  - Fully recovered input!
```


This also works when you try to analyze it algebraically. Let’s say instead of `key = key ^ (key >> a)`, we create a new variable key2 and `key2 = key ^ (key >> a)`, to make it easier to see what’s going on:

k_2 = k_1 ^ (k_1 >> a)

In order to invert this, we must remove the `k_1 >> a` term by xor-ing it again. So, since k_2 contains a k_1 term:

k_3 = k_2 ^ (k_2 >> a)

This works because shifts (in either direction) distribute with xor:

(x ^ y) >> a = (x >> a) ^ (y >> a)

Or visually with some 8-bit strings:

```
 - Add then shift    - Shift then add
   10011010             100110(10)
 ^ 01111001          ^  011110(01)
   --------             ------
   11100011             111000
       >> 2
   --------
     111000(11)
```

Now we have xor-ed another copy of `k_1 >> a` against the xor-sum. This can be seen by substituting k_2 for its value as defined in terms of k_1:

k_3 = k_2 ^ (k_2 >> a)
    = k_1 ^ (k_1 >> a) ^ ((k_1 ^ (k_1 >> a)) >> a)
    = k_1 ^ (k_1 >> a) ^ (k_1 >> a) ^ (k_1 >> 2a)
    = k_1 ^ (k_1 >> 2a)

However now we have added another term to the sum, but with a doubled shift amount. This is good, though, as this process can be repeated until the shift amount exceeds the width of the bitstring, at which point the extra xor-ed term becomes zero. That is, for a 16-bit string, a shift of 16 or greater results in a value of zero regardless of the input.

We can try this in code and see that it works:

```C
uint32_t k = 0xdeadbeef;

/* Forwards */
k ^= k >> 3;

/* Backwards */
k ^= k >> 3; // Starting at 3 and doubling each time
k ^= k >> 6;
k ^= k >> 12;
k ^= k >> 24;
//k ^= k >> 48; // We can stop, 48 is larger than the width of 32

printf("%08x\n", k); // deadbeef !
```

This same method can be applied to all of the lines of the function that xor shifts:

```C
key ^= key >> 31;
...
key ^= key >> 11;
...
key ^= key >> 22;

```

Although the function only uses xor with right shifts, this same method works just the same for left shifts. Consider reversing the order of the bits of a string, then xor-ing a right shift, and then restoring the original order. Since xor has no borrow or carry out, it is agnostic to shift direction.

## Inverting addition of shifts
The xor method does not extend to addition (or subtraction) of a right shift. Something like `k + (k >> 7)` is not invertible, because it's not even bijective.

This makes sense, because in this context a right shift is equivalent to taking the floor of a division by a power of 2, which of course does not distribute over addition:

floor(3 / 4) + floor(2 / 4) = 0 + 0 = 0
floor((3 + 2) / 4) = floor(5 / 4) = 1

More practically, addition can be viewed as just xor with carry out. So looking at a particular example of 8-bit `k + (k >> 4)` :

```
   11111111
+      1111
   --------
   00001110
```

If we try to do the same trick of extracting the upper 4 bits, assuming they have not been touched by the operation, we find that it is possible for the carry out of the addition to flip bits there. Given only `00001110` as the output value, we could assume that the upper 4 bits are either `0000` or
`1111`, since 1 could've been added from the carry out of the addition to the lower bits. We can then subtract this upper half from the lower half to recover it, and:

```
   00001110        00001110
 -     1111    -    0000
   --------        --------
   11111111        00001110
```

We now get two different answers. Which is the correct one? When you compute `k + (k >> 4)` for each of those answers:

```
   11111111        00001110
+      1111   +     0000
   --------        --------
   00001110        00001110
```

They're both correct. This is not always the case, but this function is no longer one-to-one and so this operation is not (or at least shouldn't be) commonly included in algorithms as it will pigeonhole the value passing through it. In this specific operation, 15 outputs can be produced by two different inputs, and correspondingly 15 other outputs cannot be produced no matter what the input.

However, *left* shifts do distribute, being equivalent to multiplication of 2^a for shift amount a, modulo 2^N. As seen in this next line from the initial hashing function:

```C
key += key << 6;
```

This can be modeled algebraically in the same way:

k_2 = k_1 + (k_1 << 6)

We now must subtract the `k_1 << 6` term to remove it from the sum, so:

k_3 = k_2 - (k_2 << 6)

And substituting the value for k_2:

k_3 = k_2 - (k_2 << 6)
    = k_1 + (k_1 << 6) - ((k_1 + (k_1 << 6)) << 6)
    = k_1 + (k_1 << 6) - (k_1 << 6) - (k_1 << 12)
    = k_1 - (k_1 << 12)

As a side effect of the subtraction, the negative distributes and we now are
left with a subtraction of a doubled shift amount at the end.

When this operation is repeated to remove it, we switch to adding again:

k_4 = k_3 + (k_3 << 12)
    = k_1 - (k_1 << 12) + ((k_1 - (k_1 << 12)) <<, 12)
    = k_1 - (k_1 << 12) + (k_1 << 12) - (k_1 << 24)
    = k_1 - (k_1 << 24)

The remaining term remains negative from here, having been propagated down from
the initial subtraction. As earlier, we continue doing this until the magnitude of the shift exceeds the bitwidth of `k`, and thus the value being shifted becomes zero. Doing this in code:

```C
uint32_t k = 0xdeadbeef;

/* Forwards */
k += k << 3;

/* Backwards */
k -= k << 3; // Starting at subtracting 3 and doubling each time
k += k << 6; // Adding going forward
k += k << 12;
k += k << 24;

printf("%08x\n", k); // deadbeef !
```

This also works the same for subtraction of a left shift, where the subtraction itself propagates through the iterations:

```C
uint32_t k = 0xdeadbeef;

/* Forwards */
k -= k << 3;

/* Backwards */
k += k << 3; // Starting at adding 3 and doubling each time
k += k << 6;
k += k << 12;
k += k << 24;

printf("%08x\n", k); // deadbeef !
```

However, since a left shift is multiplication modulo 2^N, we can instead invert this by multiplying by the multiplicative inverse, modulo 2^N:

```C
uint32_t k = 0xdeadbeef;

/* Forwards */
k += k << 3; // Same k += 8 * k, so same as k *= 9

/* Backwards  */
k *= 954437177; // Inverse of 9 modulo 2^32

printf("%08x\n", k); // deadbeef !
```

And of course this also works the same for subtraction of a left shift, where `k - (k << 3)` is equal to `k * -7`:

```C
uint32_t k = 0xdeadbeef;

/* Forwards */
k -= k << 3; // Same as k *= -7 (= 2^32 - 7)

/* Backwards */
k *= 1227133513; // Inverse of 2^32 - 7 modulo 2^32

printf("%08x\n", k); // deadbeef !
```

At this point I have to say thanks to this extended Euclidean algorithm calculator (https://planetcalc.com/3298/) which happily deals with numbers exceeding 2^64 and I used for every multiplicative inverse here.

Note that to have a multiplicative inverse modulo 2^N, the multiplier number must be coprime with 2^N— that is, odd. For `k += k << a` and `k -= k << a` this is always the case, as the initial `k` is adding or subtracting 1 to the multiplier of 2^a. Multiplying by an even number would be written as such with shifts:

```C
/* REPLACING k here, not adding to it */
k = k << 2;               // k *= 4
k = (k << 1) + (k << 3);  // k *= 10
k = 0 - (k << 4);         // k *= -16
```

This is obviously uninvertible because the shifts remove bits with no trace of the original `k` value. For instance an 8-bit `k` value of `0b10001100` when replaced with a left shift of itself deletes the high bit, and has the same result as shifting the value `0b00001100`.
## Code so far
Looking back at the original hash function, it's clear how to proceed with inverting most of it:

```C
uint32_t hash6432shift(uint64_t key) {
    key = (~key) + (key << 18);
    key ^= key >> 31;
    key *= 21;
    key ^= key >> 11;
    key += key << 6;
    key ^= key >> 22;
    return 0xFFFFFFFF & key;
}
```

Our code so far might look like this:

```C
uint64_t inv_hash6432shift(uint32_t hash, uint32_t trunc) {
    /* Invert: 0xFFFFFFFF & key */
    uint64_t key = hash | ((uint64_t)trunc << 32);
    
    /* Invert: key ^= key >> 22 */
    key ^= key >> 22; // Starting at 22 and doubling
    key ^= key >> 44;
    
    /* Invert: key += key << 6 (key *= 65) */
    key *= 1135184250689818600U; // Inverse of 65 modulo 2^64
    
    /* Invert: key ^= key >> 11 */
    key ^= key >> 11; // Starting at 11 and doubling
    key ^= key >> 22;
    key ^= key >> 44;
    
    /* Invert: key *= 21 */
    key *= 14933078535860113000U; // Inverse of 21 modulo 2^64
    
    /* Invert: key ^= key >> 31 */
    key ^= key >> 31; // Starting at 32 and doubling
    key ^= key >> 62;
    
    /* Invert: key = (~key) + (key << 18) */
    // ... ?
    
    return key;
}
```

## Inverting addition to complement
The last thing to do is invert the addition of a left shift to the complement. This is a bit tricky, now we have two transformations of the data instead of one. Intuitively complementing does not  distribute with addition like it does with xor, because it changes where the carry-outs are occurring.

However, we can still do the trick of looking at the least significant 18 bits, which are unaffected by the entire operation. If we complement these bits, then subtract them from the next 18 bits, and then complement the result, we can extract another 18 bits, and then repeat this process until the entire input is recovered.

Looking at a simpler example of 8-bit `k = ~k + << 3`:

```
- Forward:

   11010100  - Input value
   
   00101011  - Complement of input
 + 10100     - Input shifted left
   --------
   11001011  - Output value
        ***  - Unaffected bits


- Reverse:
   - First iteration:
   
   11001011  - Output value
 -   100     - Subtract complement of lowest 3 bits, shifted left
   --------
 ~ 10101011  - Undo complement of input
   01010100
     ******  - Correct input bits

   - Second iteration:
 
   11001011  - Output value
 - 10100     - Subtract known 6 bits, shifted left
   --------
 ~ 00101011  - Undo complement of input
   11010100  - Recovered input value!
```

This must be done in pieces while slowly assembling the original input, because otherwise the partial subtraction could cause borrowing in upper bits that could taint them for later iterations.

The code to do this might look like this:

```C
uint32_t k = 0xdeadbeef;

/* Forwards */
k = ~k + (k << 9);

/* Backwards */
uint32_t ktmp = 0; // Temp variable to hold build-up of bits
ktmp = ~(k - (ktmp << 9)); // Must run ceil(32/9) = 4 iterations
ktmp = ~(k - (ktmp << 9));
ktmp = ~(k - (ktmp << 9));
ktmp = ~(k - (ktmp << 9));
k = ktmp;

printf("%08x\n", k); // deadbeef !
```

Printing the value of `ktmp` after each iteration shows how it is extracting bits in groups of 9:

```
00000000  00000000000000000000000000000000
832fe0ef  10000011001011111110000011101111
e2f1beef  11100010111100011011111011101111
66adbeef  01100110101011011011111011101111
deadbeef  11011110101011011011111011101111
```

The bits higher than the ones extracted so far are essentially garbage from the previous iteration being incorrect, and can be ignored. These bits could be masked out at each iteration, but it’s equally correct to just ignore them.

Another interesting thing about this solution is the initialization to 0 is not necessary. What is happening here is 0 correct bits are turned into 9, which are then turned into 18, and then into 27, and finally into all 32. Because of the left shift, we know that the rightmost 9 bits are 0 and so each iteration "corrects" 9 bits of ktmp which is a "guess" as to the input. Picking any other
value for the initialization changes only the garbage data:

```
abcdef97  10101011110011011110111110010111
1f0f0eef  00011111000011110000111011101111
a14dbeef  10100001010011011011111011101111
1eadbeef  00011110101011011011111011101111
deadbeef  11011110101011011011111011101111
```

Should some of the bits of the "guess" in ktmp already be correct, even fewer iterations are needed:

```
0000beef  00000000000000001011111011101111
84adbeef  10000100101011011011111011101111
deadbeef  11011110101011011011111011101111
deadbeef  11011110101011011011111011101111
deadbeef  11011110101011011011111011101111
```

Despite how interesting this solution is, like before with multiplicative inverses, it's simpler to just solve this mathematically. Note that:

~x = (-1 * x) - 1

This is true for unsigned integers as well as two’s complement signed ones, where most people would be familiar with this equation. `-1` here means 2^N - 1 for bit width N.

Substituted into the full line:
    
key = (~key) + (key << 18)
    = (-1 * key) - 1 + (key * 2^18)
    = (key * (2^18 - 1)) - 1

So while complementing as a black box does not distribute with addition, all that needs to be done to resolve this is to add one. Then what is necessary for the code is:

```C
uint32_t k = 0xdeadbeef;

/* Forwards */
k = ~k + (k << 9); // Same as k = (k * ((1<<9) - 1)) - 1

/* Backwards */
k++;
k *= 4160486911; // Inverse of 2^9 - 1 modulo 2^32

printf("%08x\n", k); // deadbeef !
```

## Final code
And so the final inversion of the hash function is:

```C
uint64_t inv_hash6432shift(uint32_t hash, uint32_t trunc) {
    uint64_t key = hash | ((uint64_t)trunc << 32);
    
    key ^= key >> 22;
    key ^= key >> 44;
    
    key *= 1135184250689818600U;
    
    key ^= key >> 11;
    key ^= key >> 22;
    key ^= key >> 44;
    
    key *= 14933078535860113000U;
    
    key ^= key >> 31;
    key ^= key >> 62;
    
    key++;
    key *= 18428729606480330751U;
    
    return key;
}
```

## Improvements to the function
The original hash function could be improved drastically by applying the C(i) + i structure mentioned at the start, like so:

```C
uint32_t hash6432shift(uint64_t key) {
    uint64_t in = key;  // Make a copy of the input
    
    // Run the hash function as normal
    key = (~key) + (key << 18);
    key ^= key >> 31;
    key *= 21;
    key ^= key >> 11;
    key += key << 6;
    key ^= key >> 22; 
    
    key += in; // Add the original input back to key
    
    return 0xFFFFFFFF & key; // Then finally truncate
}
```

Just doing this modification I can no longer see an easy way (faster than 2^32 work) to generate preimages and so this is "left as an exercise to the reader".


