#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#define PASSWORD_LENGTH 8
#define CANDIDATE_MIN 0x20
#define CANDIDATE_MAX 0x7e

static const uint8_t check_bytes[PASSWORD_LENGTH] = {
    0x17, 0x2b, 0x6d, 0x39, 0x4f, 0x12, 0x58, 0x21,
};

static char password[PASSWORD_LENGTH + 1];

static void search(size_t position, uint32_t eax, uint32_t esi)
{
    if (position == PASSWORD_LENGTH) {
        if (eax == UINT32_C(0x7c1e2a93)) {
            password[PASSWORD_LENGTH] = '\0';
            printf("Match: %s\n", password);
        }
        return;
    }

    for (uint32_t candidate = CANDIDATE_MIN;
         candidate <= CANDIDATE_MAX;
         ++candidate) {
        uint32_t value = eax ^ (candidate + esi);
        uint32_t edx = value ^ (value >> 16);
        edx *= UINT32_C(0x045d9f3b);
        value = edx ^ (edx >> 16);

        if ((value & UINT32_C(0xff)) == check_bytes[position]) {
            value ^= UINT32_C(0xa5a5a5a5);
        }

        password[position] = (char)candidate;
        search(position + 1, value, esi + UINT32_C(0x11));
    }
}

int main(void)
{
    search(0, UINT32_C(0x13579bdf), UINT32_C(0x3));
    return 0;
}
