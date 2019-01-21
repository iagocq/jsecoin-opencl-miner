/*
MIT License

Copyright (c) 2019 iagocq

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#define ROR(a,b) ((a>>b)|(a<<(32-b)))

#define CH(x,y,z) ((x&y)^(~x&z))
#define MAJ(x,y,z) ((x&y)^(x&z)^(y&z))
#define EP0(x) (ROR(x,2)^ROR(x,13)^ROR(x,22))
#define EP1(x) (ROR(x,6)^ROR(x,11)^ROR(x,25))
#define SIG0(x) (ROR(x,7)^ROR(x,18)^(x>>3))
#define SIG1(x) (ROR(x,17)^ROR(x,19)^(x>>10))

__constant const uint k[64] = {
   0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
   0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
   0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
   0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
   0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
   0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
   0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
   0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

char ito10(uint num, uchar* buf) {
    char r;
    char s;
    char n = 0;

    uchar *cpy = buf;

    if (num == 0) {
        *buf++ = '0';
        n = 1;
    }
    while (num) {
        r = num % 10;
        s = '0' + r;
        num /= 10;
        *buf++ = s;
        n++;
    }
    while (cpy < buf) {
        s = *(--buf);
        *buf = *cpy;
        *cpy++ = s;
    }
    return n;
}

void sha256round(uchar *data, uint *state) {
    uint a, b, c, d, e, f, g, h, i, j, t1, t2, m[64];

	for (i = 0, j = 0; i < 16; ++i, j += 4)
		m[i] = (data[j] << 24) | (data[j + 1] << 16) | (data[j + 2] << 8) | (data[j + 3]);
	for ( ; i < 64; ++i)
		m[i] = SIG1(m[i - 2]) + m[i - 7] + SIG0(m[i - 15]) + m[i - 16];

	a = state[0];
	b = state[1];
	c = state[2];
	d = state[3];
	e = state[4];
	f = state[5];
	g = state[6];
	h = state[7];

	for (i = 0; i < 64; ++i) {
		t1 = h + EP1(e) + CH(e,f,g) + k[i] + m[i];
		t2 = EP0(a) + MAJ(a,b,c);
		h = g;
		g = f;
		f = e;
		e = d + t1;
		d = c;
		c = b;
		b = a;
		a = t1 + t2;
	}

	state[0] += a;
	state[1] += b;
	state[2] += c;
	state[3] += d;
	state[4] += e;
	state[5] += f;
	state[6] += g;
	state[7] += h;
}

__kernel void sha256(__global uint *hashedPrehash, __global uint *result, uint startNonce, uint difficultyMask) {
    const int id = get_global_id(0) + get_global_id(1) * get_global_size(0) + get_global_id(2) * get_global_size(0) * get_global_size(1);
    const uint nonce = id + startNonce;
    uchar padded[64];
    uint state[8];

    result[id] = 0;
    state[0] = hashedPrehash[0];
    state[1] = hashedPrehash[1];
    state[2] = hashedPrehash[2];
    state[3] = hashedPrehash[3];
    state[4] = hashedPrehash[4];
    state[5] = hashedPrehash[5];
    state[6] = hashedPrehash[6];
    state[7] = hashedPrehash[7];

    padded[0] = ',';
    char n = ito10(nonce, padded + 1);
    padded[n + 1] = 0x80;
    for (int i = n + 2; i < 63; i++) {
        padded[i] = 0;
    }

    uint bitlen = (65 + n) * 8;
  	padded[63] = bitlen;
	padded[62] = bitlen >> 8;

    sha256round(padded, state);
    if ((state[0] & difficultyMask) == 0)
        result[id] = 1;
}
