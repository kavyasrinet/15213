/* 
 * CS:APP Data Lab 
 * 
 * <Please put your name and userid here>
 * 
 * bits.c - Source file with your solutions to the Lab.
 *          This is the file you will hand in to your instructor.
 *
 * WARNING: Do not include the <stdio.h> header; it confuses the dlc
 * compiler. You can still use printf for debugging without including
 * <stdio.h>, although you might get a compiler warning. In general,
 * it's not good practice to ignore compiler warnings, but in this
 * case it's OK.  
 */

#if 0
/*
 * Instructions to Students:
 *
 * STEP 1: Read the following instructions carefully.
 */

You will provide your solution to the Data Lab by
editing the collection of functions in this source file.

INTEGER CODING RULES:
 
  Replace the "return" statement in each function with one
  or more lines of C code that implements the function. Your code 
  must conform to the following style:
 
  int Funct(arg1, arg2, ...) {
      /* brief description of how your implementation works */
      int var1 = Expr1;
      ...
      int varM = ExprM;

      varJ = ExprJ;
      ...
      varN = ExprN;
      return ExprR;
  }

  Each "Expr" is an expression using ONLY the following:
  1. Integer constants 0 through 255 (0xFF), inclusive. You are
      not allowed to use big constants such as 0xffffffff.
  2. Function arguments and local variables (no global variables).
  3. Unary integer operations ! ~
  4. Binary integer operations & ^ | + << >>
    
  Some of the problems restrict the set of allowed operators even further.
  Each "Expr" may consist of multiple operators. You are not restricted to
  one operator per line.

  You are expressly forbidden to:
  1. Use any control constructs such as if, do, while, for, switch, etc.
  2. Define or use any macros.
  3. Define any additional functions in this file.
  4. Call any functions.
  5. Use any other operations, such as &&, ||, -, or ?:
  6. Use any form of casting.
  7. Use any data type other than int.  This implies that you
     cannot use arrays, structs, or unions.

 
  You may assume that your machine:
  1. Uses 2s complement, 32-bit representations of integers.
  2. Performs right shifts arithmetically.
  3. Has unpredictable behavior when shifting an integer by more
     than the word size.

EXAMPLES OF ACCEPTABLE CODING STYLE:
  /*
   * pow2plus1 - returns 2^x + 1, where 0 <= x <= 31
   */
  int pow2plus1(int x) {
     /* exploit ability of shifts to compute powers of 2 */
     return (1 << x) + 1;
  }

  /*
   * pow2plus4 - returns 2^x + 4, where 0 <= x <= 31
   */
  int pow2plus4(int x) {
     /* exploit ability of shifts to compute powers of 2 */
     int result = (1 << x);
     result += 4;
     return result;
  }

FLOATING POINT CODING RULES

For the problems that require you to implent floating-point operations,
the coding rules are less strict.  You are allowed to use looping and
conditional control.  You are allowed to use both ints and unsigneds.
You can use arbitrary integer and unsigned constants.

You are expressly forbidden to:
  1. Define or use any macros.
  2. Define any additional functions in this file.
  3. Call any functions.
  4. Use any form of casting.
  5. Use any data type other than int or unsigned.  This means that you
     cannot use arrays, structs, or unions.
  6. Use any floating point data types, operations, or constants.


NOTES:
  1. Use the dlc (data lab checker) compiler (described in the handout) to 
     check the legality of your solutions.
  2. Each function has a maximum number of operators (! ~ & ^ | + << >>)
     that you are allowed to use for your implementation of the function. 
     The max operator count is checked by dlc. Note that '=' is not 
     counted; you may use as many of these as you want without penalty.
  3. Use the btest test harness to check your functions for correctness.
  4. Use the BDD checker to formally verify your functions
  5. The maximum number of ops for each function is given in the
     header comment for each function. If there are any inconsistencies 
     between the maximum ops in the writeup and in this file, consider
     this file the authoritative source.

/*
 * STEP 2: Modify the following functions according the coding rules.
 * 
 *   IMPORTANT. TO AVOID GRADING SURPRISES:
 *   1. Use the dlc compiler to check that your solutions conform
 *      to the coding rules.
 *   2. Use the BDD checker to formally verify that your solutions produce 
 *      the correct answers.
 */


#endif
/* 
 * evenBits - return word with all even-numbered bits set to 1
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 8
 *   Rating: 1
 */
int evenBits(void) {
/* the only number that has all even bits set to is 0x55555555
 */
	int v = 0x55;
	v = (v<<8) | v;
	v= (v<<16) | v;
	return v;
}
/* 
 * isEqual - return 1 if x == y, and 0 otherwise 
 *   Examples: isEqual(5,5) = 1, isEqual(4,5) = 0
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 5
 *   Rating: 2
 */
int isEqual(int x, int y) {
/* Take the XoR of the two numbers , if they are equal the result is 0 otherwise it is 1
 * Take the logical negation of the result
 */
	int z, n;
	z = (x^y);
	n = !z;
	return n;
}
/* 
 * byteSwap - swaps the nth byte and the mth byte
 *  Examples: byteSwap(0x12345678, 1, 3) = 0x56341278
 *            byteSwap(0xDEADBEEF, 0, 2) = 0xDEEFBEAD
 *  You may assume that 0 <= n <= 3, 0 <= m <= 3
 *  Legal ops: ! ~ & ^ | + << >>
 *  Max ops: 25
 *  Rating: 2
 */
int byteSwap(int x, int n, int m) {
/* Mask the rest of the number to get the numbers with only bytes n and m
 * Move the bytes to extreme left and the shift them right in reversed order
 * take the union of the two numbers and 'And' them with the original number with its nth and mth bytes set to 0
 */
	int a = 0xFF <<(n<<3);
	int a1, b, b1, b2, b3, a2, a3, swapped, number_mask, filler, final_swapped;
	a1 = x&a;
	b= 0xFF << (m<<3);
	b1 = x&b;
	b2 = (b1 >> (m<<3)) & 0xFF;
	b3 = b2 << (n<<3);
	a2 = (a1 >> (n<<3)) & 0xFF;
	a3 = a2 << (m<<3);
	swapped = a3|b3;
	number_mask = a|b;
	filler = ~number_mask & x;
	final_swapped = swapped | filler;
	return final_swapped;
}
/* 
 * rotateRight - Rotate x to the right by n
 *   Can assume that 0 <= n <= 31
 *   Examples: rotateRight(0x87654321,4) = 0x18765432
 *   Legal ops: ~ & ^ | + << >>
 *   Max ops: 25
 *   Rating: 3 
 */
int rotateRight(int x, int n) {
/* rotate the number right by so many bits
 * set the rightmost inserted bits to 0, if 1
 * mask the leftmost n bits dropped in the number
 * shift the bits left by 32 - n
 * take the union of shifted bits and rotated number
 */
	int rotate_right = x >> n;
	int carry_left = x << (32 + (~n + 1));
	int number_mask, final_rotated, update_right_shift;
	number_mask = 0xFF;
	number_mask = (number_mask << 8) | number_mask;
	number_mask = (number_mask << 16) | number_mask;
	number_mask = (number_mask << (32 + (~n +1)));
	number_mask = ~(number_mask);
	update_right_shift =  (rotate_right & number_mask);
	final_rotated = update_right_shift | carry_left;
	return final_rotated;
}
/* 
 * logicalNeg - implement the ! operator using any of 
 *              the legal operators except !
 *   Examples: logicalNeg(3) = 0, logicalNeg(0) = 1
 *   Legal ops: ~ & ^ | + << >>
 *   Max ops: 12
 *   Rating: 4 
 */
int logicalNeg(int x) {
/* return 0 for any non zero number (positive or negative) and 1 for 0
 * Take the union of number and its 2s complement and get the complement of sign of the union
 */
	int complement = (~x) + 1;
	int change = x | complement;
	int sign;
	sign = change >> 31;
	sign  = ~sign & 0x01;
	return sign;

}
/* 
 * TMax - return maximum two's complement integer 
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 4
 *   Rating: 1
 */
int tmax(void) {
/* The value of a number is its maximum positive value
 * when the sign bit is set to 0 and every other bit is 1 i.e. 0x7FFFFFFF
 */
	int max_int = 0x01;
	max_int = max_int << 31;
	return ~max_int;

}
/* 
 * sign - return 1 if positive, 0 if zero, and -1 if negative
 *  Examples: sign(130) = 1
 *            sign(-23) = -1
 *  Legal ops: ! ~ & ^ | + << >>
 *  Max ops: 10
 *  Rating: 2
 */
int sign(int x) {
/*Get the sign bit value and see if the number is a non zero number
 * for zero return zero, for positive return 1 and -1 for negative, using 0xFFFFFFFF mask
 */
	int sign_bit = x >> 31;
	int logical_not = (!(!x));
	int type, sign_mask, sign;
	type = sign_bit | logical_not;
	sign_mask = 0xFF;
	sign_mask = (sign_mask << 8) | sign_mask;
	sign_mask = (sign_mask << 16) | sign_mask;
	sign  = type & sign_mask;
	return sign;

}
/* 
 * isGreater - if x > y  then return 1, else return 0 
 *   Example: isGreater(4,5) = 0, isGreater(5,4) = 1
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 24
 *   Rating: 3
 */
int isGreater(int x, int y) {
/* take the difference of the two operands x and y
 * if the difference is positive, then x is greater than y
 * if the difference is 0 or negative return 0 else 1
 */
	int difference = x + ~y + 1;
	int check = !(!(difference));
	int right_shift = difference >> 31;
	int change = ~right_shift & 0x01;
	int sign_x;
	int sign_y;
	change = change & check;
	sign_x = x >> 31;
	sign_x = ~sign_x & 0x01;
	sign_y = y >> 31;
	sign_y = ~sign_y & 0x01;
	return ((change & ~(sign_x ^ sign_y)) | ((sign_x ^sign_y) & ~sign_y));

}
/* 
 * subOK - Determine if can compute x-y without overflow
 *   Example: subOK(0x80000000,0x80000000) = 1,
 *            subOK(0x80000000,0x70000000) = 0, 
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 20
 *   Rating: 3
 */
int subOK(int x, int y) {
/* Get the signs of the sum, x and y
 * overflow occurs if x and y ahve different signs but the sum has the same sign as that of y
 */
	int difference = x + ~y + 1 ;
	int sign_diff = difference >> 31;
	int sign_x;
	int sign_y, different_type, similar;
	sign_diff = ~sign_diff & 0x01;
	sign_x = x >> 31;
	sign_x = ~sign_x & 0x01;
	sign_y = y >> 31;
	sign_y = ~sign_y & 0x01;
	different_type = sign_x ^ sign_y;
	similar = !(sign_diff ^ sign_y);
	return !(( different_type & similar));

}
/*
 * satAdd - adds two numbers but when positive overflow occurs, returns
 *          maximum possible value, and when negative overflow occurs,
 *          it returns minimum possible value.
 *   Examples: satAdd(0x40000000,0x40000000) = 0x7fffffff
 *             satAdd(0x80000000,0xffffffff) = 0x80000000
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 30
 *   Rating: 4
 */
int satAdd(int x, int y) {
/*Get the signs of x, y and the sum
 * if the two numbers  have the same sign (both positive or both negative) and overflow has a different sign than y, an overflow has occured
 * if there is an overflow shift the sum by 31 and XoR with the MSB of overflow set to 1
 * if not an overflow, the sum remains the same
 */
	int z = x +y;
	int sign_z = z >> 31;
	int shift_overflow, shift_right, sum_mask, sum, value, sign_x, sign_y, similar, different, overflow;
	sign_z = ~sign_z & 0x01;
	sign_x = x >> 31;
	sign_x = ~sign_x & 0x01;
	sign_y = y >> 31;
	sign_y = ~sign_y & 0x01;
	similar = !(sign_x ^ sign_y);
	different = (sign_z ^ sign_y);
	overflow =(similar & different) ;
	shift_overflow  = overflow << 31;
	shift_right = shift_overflow >> 31;
	sum_mask = shift_right & 0x1F;
	sum = z >> sum_mask;
	value =  sum ^ shift_overflow;
	return value;

}
/* howManyBits - return the minimum number of bits required to represent x in
 *             two's complement
 *  Examples: howManyBits(12) = 5
 *            howManyBits(298) = 10
 *            howManyBits(-5) = 4
 *            howManyBits(0)  = 1
 *            howManyBits(-1) = 1
 *            howManyBits(0x80000000) = 32
 *  Legal ops: ! ~ & ^ | + << >>
 *  Max ops: 90
 *  Rating: 4
 */
int howManyBits(int x) {
/*Check if the number is negative or positive
 * for a negative number , take the 2's complement, for a positive number , it remains the same
 * now we check for the position of a non-zero MSB, i.e. the position where the MSB becomes 1 from 0
 * the number of bits required will be the position number plus 1 for the sign and position number plus 2 for positive numbers 
 */
	int sign_mask, sign, if_zero, not_zero, no_of_bits, mask_16, mask_8, mask_4, mask_2, mask_1, bits_sixteen, bits_eight, bits_four, bits_two, bits_one, number, adjust, sign_no;
	sign_mask = (x >> 31) & 0x01;
	sign = ~sign_mask + 0x01;
	if_zero = (!x);
	not_zero = (~( !(!x))) + 0x01;
	number = ((~sign & x ) | (sign & (~x)));
	sign_no = (number >> 31) & 0x01;

	mask_16 = (0xFF << 8)| (0xFF);
	mask_16 = mask_16 << 16;
	mask_8 = (0x0FF << 8);
	mask_4 = (0x0F << 4);
	mask_2 = (0x03 <<2);
	mask_1 = 0x02;

	bits_sixteen = (!(!(number & mask_16)));
	bits_sixteen = bits_sixteen << 4;
	number = (number >> bits_sixteen);
	
	bits_eight = (!(!(number & mask_8)));
	bits_eight = bits_eight << 3;
	number = (number >> bits_eight);

	bits_four = (!(!(number & mask_4)));
	bits_four = bits_four << 2;
	number = (number >> bits_four);

	bits_two = (!(!(number & mask_2)));
	bits_two = bits_two << 1;
	number = (number >> bits_two);

	bits_one = (!(!(number >> 1)));
	no_of_bits = bits_sixteen + bits_eight + bits_four + bits_two + bits_one + 1;
	adjust =  ((((!(!(x ^ ~0)) << 31) >> 31)) & (!sign_no & 0x01));
	no_of_bits = no_of_bits + adjust;
	
	return ( if_zero | (not_zero & no_of_bits));
}
/*
 * float_half - Return bit-level equivalent of expression 0.5*f for
 *   floating point argument f.
 *   Both the argument and result are passed as unsigned int's, but
 *   they are to be interpreted as the bit-level representation of
 *   single-precision floating point values.
 *   When argument is NaN, return argument
 *   Legal ops: Any integer/unsigned operations incl. ||, &&. also if, while
 *   Max ops: 30
 *   Rating: 4
 */
unsigned float_half(unsigned uf) {
/*
 * multiply the undefined number with a negative power of 2,
 * if exp is 0, shift the fraction left by 1, 
 * if exp is 1, set exp to 0 and shift the fraction  left by 1,
 * otherwise, decrement the exp by 1 and fraction remains the same
 */
	int x, ones, sign, filter_exp, fract_update, filter_frac, exp, fraction, lsb, sec_lsb, lsb_update;
	x = 0x01 << 31;
	ones = ~(0x00);
	sign = x & uf;
	filter_exp = (ones << 23) ^ x;
	filter_frac = (~filter_exp) ^ x;
	exp = filter_exp & uf;
	fraction = filter_frac & uf;
	lsb = uf & 0x01;
	sec_lsb = (uf & 0x03) >> 1;
	lsb_update = lsb & sec_lsb;
	if(exp == filter_exp ) 
	{
		return uf;
	}
	if((exp == 0) || (exp == (0x01 << 23))){
		fract_update = (fraction | exp);
	        fract_update = (fract_update >> 1);
        	fract_update = (fract_update + lsb_update) | sign;
		return fract_update;
	}
		exp = ((exp + ones) & filter_exp);
		exp = exp & filter_exp;
		fraction = sign | exp | fraction;
		return fraction;
}
/* 
 * float_f2i - Return bit-level equivalent of expression (int) f
 *   for floating point argument f.
 *   Argument is passed as unsigned int, but
 *   it is to be interpreted as the bit-level representation of a
 *   single-precision floating point value.
 *   Anything out of range (including NaN and infinity) should return
 *   0x80000000u.
 *   Legal ops: Any integer/unsigned operations incl. ||, &&. also if, while
 *   Max ops: 30
 *   Rating: 4
 */
int float_f2i(unsigned uf) {
/*Get the sign, exp and fraction of the number, and calculate E and M accordingly
 * for numbers close to 0 and numbers multiplied by negative powers of 2, round to 0 and return 0
 * for NaN and infinity and E out of bound for integers i.e. E>30, return as mentioned
 * normalize the value
 * for E in bound, if it is less than 23(the maximum number of bits available to frac part) shift left by the difference, else shift right by the difference
 * for negative numbers return 2s complement, or just the number for positive numbers
 */
 	int x= (0x01)<<31;
	int x1 = 0x01 << 23;
	int ones, filter_exp, check_exp, filter_frac, bias, exp, fraction,E;
	ones = ~(0x00);
	filter_exp = (ones << 23) ^ x;
	check_exp = filter_exp >> 23;
	filter_frac = ~(filter_exp) ^ x;
	bias = 127;
	exp = uf & filter_exp;
	exp = exp >> 23;
	fraction = filter_frac & uf;
	E = exp - bias;
	if( (exp == check_exp)|| ( E > 30)){
		return 0x80000000u;
	}
	if((exp ==0) || (E < 0)){
		return 0;
	}
		fraction = fraction | x1;
	if (E < 23){
		fraction = fraction >> (23 - E);
	}
	else if (E >= 23){
		fraction = fraction << (E-23);
	}
	if( uf >> 31 & 0x01)
		return  (~fraction +1);
		return fraction;
}
