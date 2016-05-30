#include "encryption.h"
static BigInt p = 11, q = 13;
static BigInt e = 43; // 互质即可

BigInt inv(BigInt e, BigInt r) {
	BigInt d;
	for (d=2; d<r; d++) {
		BigInt re = (e*d) % r;
		if (re == 1) {
			return d;
		}
	}
}
 
BigInt power(BigInt a, BigInt k, BigInt N) {
	BigInt p=1, i;
	for (i=1; i<=k; i++) { 
		p = (p*a)%N;
	}
	return p;
}
 
int encry(int m) {
	if (m == 'u' ) {
		return -1;
	} else if (m == 'e') {
		return -2;
	}
	
	BigInt N = (p * q);
	BigInt r = (p-1)*(q-1);
	BigInt d = inv(e, r);
	BigInt c = power(m, e, N);
	return c;
}

int decry(int m) {
	if (m == -1) {
		return 'u';
	} else if (m == -2) {
		return 'e';
	}
	BigInt N = (p * q);
	BigInt r = (p-1)*(q-1);
	BigInt d = inv(e, r);
	BigInt m2 = power(m, d, N);	
	return m2;
}

//int main() {
//	char t[] = "uu";
//	int len = strlen(t);
//	int source[len];
//	for (int i = 0; i < len;i++) {
//		source[i] = encry(t[i]);
//		printf("%c\n", source[i]);
//	}
//	for (int i = 0; i < len;i++) {
//		char c = decry(source[i]);
//		printf("%c", c);
//	}
//
//}