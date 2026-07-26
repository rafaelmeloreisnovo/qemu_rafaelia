/*
 * Exact dyadic test for opposite 45-degree median-contact square rotations.
 *
 * T+ = (1/2) M+, M+ = [[1,-1],[1,1]]
 * T- = (1/2) M-, M- = [[1,1],[-1,1]]
 *
 * The program uses no floating point. It is discoverable by QEMU's
 * tests/tcg/multiarch/Makefile.target and is valid under ILP32 and LP64.
 */

struct vec2 {
    long x;
    long y;
};

static struct vec2 mplus(struct vec2 v)
{
    struct vec2 out = { v.x - v.y, v.x + v.y };
    return out;
}

static struct vec2 mminus(struct vec2 v)
{
    struct vec2 out = { v.x + v.y, -v.x + v.y };
    return out;
}

static unsigned long norm_num(struct vec2 v)
{
    return (unsigned long)(v.x * v.x + v.y * v.y);
}

static int determinant_area(void)
{
    /* det(M+) = det(M-) = 2; division by 2 on both axes gives det(T)=1/2. */
    const long det_plus = 1 * 1 - (-1) * 1;
    const long det_minus = 1 * 1 - 1 * (-1);
    return det_plus == 2 && det_minus == 2;
}

static int midpoint_contact(void)
{
    const long a = 256;
    const struct vec2 vertices[4] = {
        { a, a }, { -a, a }, { -a, -a }, { a, -a }
    };
    const struct vec2 expected_minus[4] = {
        { 2 * a, 0 }, { 0, 2 * a }, { -2 * a, 0 }, { 0, -2 * a }
    };
    int i;

    /* mminus output is a numerator over 2: 2*a/2 is midpoint a. */
    for (i = 0; i < 4; i++) {
        struct vec2 got = mminus(vertices[i]);
        if (got.x != expected_minus[i].x || got.y != expected_minus[i].y) {
            return 0;
        }
    }
    return 1;
}

static int norm_progression(void)
{
    struct vec2 v = { 256, 256 };
    int i;

    for (i = 0; i < 12; i++) {
        unsigned long before = norm_num(v);
        struct vec2 next = mminus(v);
        unsigned long after = norm_num(next);
        if (after != 2UL * before) {
            return 0;
        }
        /* Denominator squares grow by 4, so represented norm^2 halves. */
        v = next;
    }
    return 1;
}

static int opposite_pair(void)
{
    struct vec2 v = { 73, -41 };
    struct vec2 pair = mplus(mminus(v));

    /* M+ M- = 2I. With denominator 4, T+ T- = I/2. */
    return pair.x == 2 * v.x && pair.y == 2 * v.y;
}

static int eight_step_cycle(void)
{
    struct vec2 start = { 19, -7 };
    struct vec2 v = start;
    int i;

    for (i = 0; i < 8; i++) {
        v = mminus(v);
    }

    /* M-^8 = 16I; denominator 2^8 gives T-^8 = I/16. */
    return v.x == 16 * start.x && v.y == 16 * start.y;
}

int main(void)
{
    if (sizeof(void *) != 4 && sizeof(void *) != 8) {
        return 90;
    }
    if (!midpoint_contact()) {
        return 1;
    }
    if (!norm_progression()) {
        return 2;
    }
    if (!opposite_pair()) {
        return 3;
    }
    if (!eight_step_cycle()) {
        return 4;
    }
    if (!determinant_area()) {
        return 5;
    }
    return 0;
}
