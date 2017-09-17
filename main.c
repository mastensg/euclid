#define _XOPEN_SOURCE

#include <err.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include <GL/gl.h>
#include <GL/glut.h>

#include "audio.h"
#include "tga.h"

////////////////////////////////////////////////////////////////////////

#define RECORD 0

#define UNUSED(x) (void)(x)

#define TAU 6.283185307179586

////////////////////////////////////////////////////////////////////////

struct point
{
    float x, y;
};

////////////////////////////////////////////////////////////////////////

static struct point koch0_points[768];
static struct point koch1_points[768];
static struct point koch2_points[768];
static struct point koch3_points[768];
static struct point koch4_points[768];

static struct point qoch0_points[16000];
static struct point qoch1_points[16000];
static struct point qoch2_points[16000];
static struct point qoch3_points[16000];
static struct point qoch4_points[16000];

static uint8_t **flame;

static uint8_t mandl_palette[256][3];
static uint8_t flame_palette[256][3];

uint8_t *kochz;
uint8_t *qochz;

static uint8_t *pixels;

static uint8_t *frame;

static struct point *points;
static int numpoints = 0;
static float t_a, t_x, t_y;

static double started = -1;

static const int quality = 4;
static int bw, bh;
static int sw, sh;

////////////////////////////////////////////////////////////////////////

static void
hsl2rgb(uint8_t rgb[3], float h, float s, float l) {
    float v;
    float r, g, b;

    r = l;
    g = l;
    b = l;
    v = (l <= 0.5) ? (l * (1.0 + s)) : (l + s - l * s);
    if (v > 0)
    {
        double m;
        double sv;
        int sextant;
        double fract, vsf, mid1, mid2;

        m = l + l - v;
        sv = (v - m ) / v;
        h *= 6.0;
        sextant = (int)h;
        fract = h - sextant;
        vsf = v * sv * fract;
        mid1 = m + vsf;
        mid2 = v - vsf;
        switch (sextant)
        {
        case 0:
            r = v;
            g = mid1;
            b = m;
            break;
        case 1:
            r = mid2;
            g = v;
            b = m;
            break;
        case 2:
            r = m;
            g = v;
            b = mid1;
            break;
        case 3:
            r = m;
            g = mid2;
            b = v;
            break;
        case 4:
            r = mid1;
            g = m;
            b = v;
            break;
        case 5:
            r = v;
            g = m;
            b = mid2;
            break;
        }
    }

    rgb[0] = 255 * r;
    rgb[1] = 255 * g;
    rgb[2] = 255 * b;
}

float
lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

float
smoothstep(float x)
{
    return x * x * (3 - 2 * x);
}

////////////////////////////////////////////////////////////////////////

#if 1
static float
elapsed()
{
    struct timeval tv;
    double now;

    gettimeofday(&tv, NULL);

    now = tv.tv_sec + tv.tv_usec * 1e-6;

    if (started == -1)
        started = now;

    return now - started;
}
#else
static float
elapsed()
{
    return alsa_offset() / 44100.0;
}
#endif

////////////////////////////////////////////////////////////////////////

static void
make_mandelbrot()
{
    float h, s, l, x;
    int i;

    for (i = 0; i < 192; ++i)
    {
        x = (float)i / 192;

        h = fmodf(powf(2 - x, 2), .8);
        s = 1;
        l = 0.5 * powf(h + .1, .1);

        hsl2rgb(mandl_palette[i], h, s, l);
    }

    mandl_palette[191][0] = 0;
    mandl_palette[191][1] = 0;
    mandl_palette[191][2] = 0;
}

static void
draw_mandelbrot(float cx, float cy, float scale, float d, float t)
{
    float a, b, za, zb, zaa, zbb, dx, dy;
    int R, G, B;
    int i, x, y;

    if (t > .95)
        d = 20 * (t - .95);

    d = pow(d, .5);

    for(y = 0; y < bh; ++y) {
        b = cy + scale * (1 - 2 * (float)y / bh);

        for (x = 0; x < bw; ++x)
        {
            a = cx + bw * scale / bh * (-1 + 2 * (float)x / bw);

            i = 192;

            if (scale < 2. / 100)
            {

                dx = a + 0.6506;
                dy = b + 0.4780;

                if (dx * dx + dy * dy < 0.0000007)
                    goto setpixel;

                dx = a + 0.64915;
                dy = b + 0.47855;

                if (dx * dx + dy * dy < 0.0000002)
                    goto setpixel;
            }
            else
            {
                dx = a + 0.25;
                dy = b + 0.0;

                if (dx * dx + dy * dy < 0.23)
                    goto setpixel;

                dx = a + 1;
                dy = b + 0;

                if (dx * dx + dy * dy < 0.05)
                    goto setpixel;

                dx = a + 0.623;
                dy = b + 0.425;

                if (dx * dx + dy * dy < 0.00035)
                    goto setpixel;
            }

            za = a;
            zb = b;

            for (i = 0; i < 192; ++i)
            {
                zaa = za * za;
                zbb = zb * zb;

                if (zaa + zbb > 4)
                    break;

                zb = (2 * (za * zb)) + b;
                za = zaa - zbb + a;
            }

setpixel:
            if (i == 192)
            {
                i = rand() % 48;

                i -= 48 * d;

                if (i < 0)
                    i = 0;

                pixels[y * bw * 3 + x * 3 + 0] = i;
                pixels[y * bw * 3 + x * 3 + 1] = i;
                pixels[y * bw * 3 + x * 3 + 2] = i;
            }
            else
            {
                R = mandl_palette[i][0];
                G = mandl_palette[i][1];
                B = mandl_palette[i][2];
                R -= 256 * d; if (R < 0) R = 0;
                G -= 256 * d; if (G < 0) G = 0;
                B -= 256 * d; if (B < 0) B = 0;
                pixels[y * bw * 3 + x * 3 + 0] = R;
                pixels[y * bw * 3 + x * 3 + 1] = G;
                pixels[y * bw * 3 + x * 3 + 2] = B;
            }
        }
    }

    glPixelZoom((float)sw / bw, (float)sh / bh);
    glDrawPixels(bw, bh, GL_RGB, GL_UNSIGNED_BYTE, pixels);
}

////////////////////////////////////////////////////////////////////////

static void
forward(float d)
{
    t_x += d * cos(t_a);
    t_y += d * sin(t_a);
}

static void
point()
{
    points->x = t_x;
    points->y = t_y;
    ++points;
}

static void
morph(float *x, float *y, struct point *a, struct point *b, float t)
{
    float x0, y0, x1, y1;

    x0 = a->x;
    y0 = a->y;
    x1 = b->x;
    y1 = b->y;

    *x = lerp(x0, x1, smoothstep(t));
    *y = lerp(y0, y1, smoothstep(t));
}

////////////////////////////////////////////////////////////////////////

static void
koch(float length, int m, int n)
{
    float d;

    d = length / 3;

    if (!n)
    {
        if (!m)
        {
            forward(length);
            point();
            return;
        }

        koch(d, m - 1, n);
        koch(d / 2, m - 1, n);
        koch(d / 2, m - 1, n);
        koch(d, m - 1, n);

        return;
    }

    koch(d, m, n - 1);
    t_a -= TAU / 6;
    koch(d, m, n - 1);
    t_a += TAU / 3;
    koch(d, m, n - 1);
    t_a -= TAU / 6;
    koch(d, m, n - 1);
}

static void
make_koch()
{
    int i;

    numpoints = 0;
    points = koch0_points;

    t_a = 0;
    t_x = -.6;
    t_y = -.35;

    for (i = 0; i < 3; ++i)
    {
        koch(1.2, 4, 0);
        t_a += TAU / 3;
    }

    numpoints = 0;
    points = koch1_points;

    t_a = 0;
    t_x = -.6;
    t_y = -.35;

    for (i = 0; i < 3; ++i)
    {
        koch(1.2, 3, 1);
        t_a += TAU / 3;
    }

    numpoints = 0;
    points = koch2_points;

    t_a = 0;
    t_x = -.6;
    t_y = -.35;

    for (i = 0; i < 3; ++i)
    {
        koch(1.2, 2, 2);
        t_a += TAU / 3;
    }

    numpoints = 0;
    points = koch3_points;

    t_a = 0;
    t_x = -.6;
    t_y = -.35;

    for (i = 0; i < 3; ++i)
    {
        koch(1.2, 1, 3);
        t_a += TAU / 3;
    }

    numpoints = 0;
    points = koch4_points;

    t_a = 0;
    t_x = -.6;
    t_y = -.35;

    for (i = 0; i < 3; ++i)
    {
        koch(1.2, 0, 4);
        t_a += TAU / 3;
    }
}

static void
draw_koch(float t)
{
    float x, y;
    int i;

    glBegin(GL_LINE_LOOP);
    glColor3f(1.0, 1.0, 1.0);
    for (i = 0; i < 768; ++i)
    {
        if (t < .25)
            morph(&x, &y, koch0_points + i, koch1_points + i, (t - .00) * 4);
        else if (t < .50)
            morph(&x, &y, koch1_points + i, koch2_points + i, (t - .25) * 4);
        else if (t < .75)
            morph(&x, &y, koch2_points + i, koch3_points + i, (t - .50) * 4);
        else
            morph(&x, &y, koch3_points + i, koch4_points + i, (t - .75) * 4);

        glVertex2f(x * sh / sw, y);
    }
    glEnd();
}

////////////////////////////////////////////////////////////////////////

static void
make_kochz()
{
    int x, y, X, Y;
    uint8_t *p;

    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    draw_koch(1);
    glFlush();
    glFinish();
    glReadPixels((sw - sh) / 2, 0, sh, sh, GL_RGB, GL_UNSIGNED_BYTE, kochz);

    for (y = 0; y < sh; ++y)
    {
        for (x = 0; x < sh; ++x)
        {
            p = kochz + 3 * (y * sh + x);

            if (p[3 +  0] || p[3 * sh +  0]) p[0] = 255, p[1] = 255, p[2] = 255;
            if (p[3 +  3] || p[3 * sh +  3]) p[0] = 255, p[1] = 255, p[2] = 255;
            if (p[3 +  6] || p[3 * sh +  6]) p[0] = 255, p[1] = 255, p[2] = 255;
            if (p[3 +  9] || p[3 * sh +  9]) p[0] = 255, p[1] = 220, p[2] = 150;
            if (p[3 + 12] || p[3 * sh + 12]) p[0] = 255, p[1] = 192, p[2] = 128;

            X = (x / (sh >> 2)) % 2;
            Y = (y / (sh >> 2)) % 2;
            if (!p[0] && (X ^ Y))
            {
                p[0] = 32;
                p[1] = 32;
                p[2] = 32;
            }
        }
    }
}

static void
draw_kochz(float t)
{
    float ox, oy, roto, zoom;
    float X, Y, a, r;
    float u;
    int sx, sy, x, y;
    uint8_t *p, q;

    u = elapsed();

    if (u > 34)
        q = 64 + 32 * pow(1 - fmod(u - 0.133976, 0.472667) / 0.2, 4);
    else
        q = 32;

    if (t < .25)
        ox = 0, oy = 0, roto = 0, zoom = 4;
    else if (t < .5)
        ox = 0, oy = 0, roto = 0, zoom = 12;
    else
    {
        t = 2 * (t - .5);
        ox = lerp(0, .25, t);
        oy = lerp(0, .25, t);
        roto = lerp(0, 24, t);
        zoom = lerp(12, 2, t);
    }

    for (y = 0; y < bh; ++y)
    {
        for (x = 0; x < bw; ++x)
        {
            X = x - bw / 2;
            Y = y - bh / 2;

            a = TAU / 4 + atan2(X, Y);
            r = sqrtf(X * X + Y * Y);

            a += roto;
            r *= zoom;

            X = r * cosf(a) + zoom * sh * ox;
            Y = r * sinf(a) + zoom * sh * oy;

            sx = ((int)X + sh / 2) % sh;
            sy = (((int)Y + sh / 2) + 16 * sh) % sh;

            p = kochz + 3 * (sy * sh + sx);

            if (p[0] == 32)
            {
                pixels[y * bw * 3 + x * 3 + 0] = q;
                pixels[y * bw * 3 + x * 3 + 1] = q;
                pixels[y * bw * 3 + x * 3 + 2] = q;
            }
            else
            {
                pixels[y * bw * 3 + x * 3 + 0] = p[0];
                pixels[y * bw * 3 + x * 3 + 1] = p[1];
                pixels[y * bw * 3 + x * 3 + 2] = p[2];
            }
        }
    }

    glPixelZoom((float)sw / bw, (float)sh / bh);
    glDrawPixels(bw, bh, GL_RGB, GL_UNSIGNED_BYTE, pixels);
}

////////////////////////////////////////////////////////////////////////

static void
qoch(float length, int m, int n)
{
    float d;

    d = length / 3;

    if (!n)
    {
        if (!m)
        {
            forward(length);
            point();
            return;
        }

        qoch(d, m - 1, n);
        qoch(0, m - 1, n);
        qoch(d, m - 1, n);
        qoch(0, m - 1, n);
        qoch(d, m - 1, n);

        return;
    }

    qoch(d, m, n - 1);
    t_a -= TAU / 4;
    qoch(d, m, n - 1);
    t_a += TAU / 4;
    qoch(d, m, n - 1);
    t_a += TAU / 4;
    qoch(d, m, n - 1);
    t_a -= TAU / 4;
    qoch(d, m, n - 1);
}

static void
make_qoch()
{
    static const float x = -.5;
    static const float y = -.5;
    int i;

    points = qoch0_points;
    t_a = 0; t_x = x; t_y = y;
    for (i = 0; i < 4; ++i)
    {
        qoch(1.0, 4, 0);
        t_a += TAU / 4;
    }

    points = qoch1_points;
    t_a = 0; t_x = x; t_y = y;
    for (i = 0; i < 4; ++i)
    {
        qoch(1.0, 3, 1);
        t_a += TAU / 4;
    }

    points = qoch2_points;
    t_a = 0; t_x = x; t_y = y;
    for (i = 0; i < 4; ++i)
    {
        qoch(1.0, 2, 2);
        t_a += TAU / 4;
    }

    points = qoch3_points;
    t_a = 0; t_x = x; t_y = y;
    for (i = 0; i < 4; ++i)
    {
        qoch(1.0, 1, 3);
        t_a += TAU / 4;
    }

    points = qoch4_points;
    t_a = 0; t_x = x; t_y = y;
    for (i = 0; i < 4; ++i)
    {
        qoch(1.0, 0, 4);
        t_a += TAU / 4;
    }
}

static void
draw_qoch(float t)
{
    float x, y;
    int i;

    glBegin(GL_LINE_LOOP);
    glColor3f(0.0, 0.0, 0.0);
    for (i = 0; i < 2500; ++i)
    {
        if (t < .25)
            morph(&x, &y, qoch0_points + i, qoch1_points + i, (t - .00) * 4 + 0.13 * sin(200*t));
        else if (t < .50)
            morph(&x, &y, qoch1_points + i, qoch2_points + i, (t - .25) * 4 + 0.13 * sin(200*t));
        else if (t < .75)
            morph(&x, &y, qoch2_points + i, qoch3_points + i, (t - .50) * 4 + 0.13 * sin(200*t));
        else
            morph(&x, &y, qoch3_points + i, qoch4_points + i, (t - .75) * 4 + 0.13 * sin(200*t));

        glVertex2f(x * sh / sw, y);
    }
    glEnd();
}

////////////////////////////////////////////////////////////////////////

static void
make_qochz()
{
    int x, y, X, Y;
    uint8_t *p;

    glClearColor(1, 1, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    draw_qoch(1);
    glFlush();
    glFinish();
    glReadPixels((sw - sh) / 2, 0, sh, sh, GL_RGB, GL_UNSIGNED_BYTE, qochz);

    for (y = 0; y < sh; ++y)
    {
        for (x = 0; x < sh; ++x)
        {
            p = qochz + 3 * (y * sh + x);

            if (!p[3 +  0] || !p[3 * sh +  0]) p[0] = 0, p[1] = 0, p[2] = 0;

            X = (x / (sh >> 2)) % 2;
            Y = (y / (sh >> 2)) % 2;
            if (p[0] == 255 && (X ^ Y))
            {
                p[0] = 224;
                p[1] = 224;
                p[2] = 224;
            }
        }
    }
}

static void
draw_qochz(float t)
{
    float ox, oy, roto, zoom;
    float X, Y, a, r;
    int sx, sy, x, y;
    uint8_t *p;

    if (t < .25)
        ox = 0, oy = 0, roto = 0, zoom = 4;
    else if (t < .5)
        ox = 0, oy = 0, roto = 0, zoom = 12;
    else
    {
        t = 2 * (t - .5);
        ox = lerp(0, .14746, t);
        oy = lerp(0, .22265, t);
        roto = lerp(32, 0, t);
        zoom = lerp(12, .5, t);
    }

    for (y = 0; y < bh; ++y)
    {
        for (x = 0; x < bw; ++x)
        {
            X = x - bw / 2;
            Y = y - bh / 2;

            a = atan2(X, Y);
            r = sqrtf(X * X + Y * Y);

            a += roto;
            r *= zoom;

            X = r * cosf(a) + zoom * sh * ox;
            Y = r * sinf(a) + zoom * sh * oy;

            sx = ((int)X + sh / 2) % sh;
            sy = (((int)Y + sh / 2) + 16 * sh) % sh;

            p = qochz + 3 * (sy * sh + sx);

            pixels[y * bw * 3 + x * 3 + 0] = p[0];
            pixels[y * bw * 3 + x * 3 + 1] = p[1];
            pixels[y * bw * 3 + x * 3 + 2] = p[2];
        }
    }

    glPixelZoom((float)sw / bw, (float)sh / bh);
    glDrawPixels(bw, bh, GL_RGB, GL_UNSIGNED_BYTE, pixels);
}

////////////////////////////////////////////////////////////////////////

static void
make_fire()
{
    float h, s, l, x;
    int i;

    for (i = 0; i < 256; ++i)
    {
        x = (float)i / 256;

        h = powf(x, 1.3);
        s = 1;
        l = powf(x, .7);

        hsl2rgb(flame_palette[i], h, s, l);
    }
}

static void
draw_euclid(int ceil)
{
    int x, y;

    for (x =  50; x <  70; ++x) flame[140 * bh / 192][  x * bw / 256] = rand() % ceil;
    for (x =  50; x <  70; ++x) flame[120 * bh / 192][  x * bw / 256] = rand() % ceil;
    for (x =  50; x <  70; ++x) flame[100 * bh / 192][  x * bw / 256] = rand() % ceil;
    for (y = 100; y < 140; ++y) flame[  y * bh / 192][ 50 * bw / 256] = rand() % ceil;

    for (x =  80; x < 100; ++x) flame[100 * bh / 192][  x * bw / 256] = rand() % ceil;
    for (y = 100; y < 140; ++y) flame[  y * bh / 192][ 80 * bw / 256] = rand() % ceil;
    for (y = 100; y < 140; ++y) flame[  y * bh / 192][100 * bw / 256] = rand() % ceil;

    for (x = 110; x < 130; ++x) flame[100 * bh / 192][  x * bw / 256] = rand() % ceil;
    for (x = 110; x < 130; ++x) flame[140 * bh / 192][  x * bw / 256] = rand() % ceil;
    for (y = 100; y < 140; ++y) flame[  y * bh / 192][110 * bw / 256] = rand() % ceil;

    for (x = 140; x < 160; ++x) flame[100 * bh / 192][  x * bw / 256] = rand() % ceil;
    for (y = 100; y < 140; ++y) flame[  y * bh / 192][140 * bw / 256] = rand() % ceil;

    for (y = 100; y < 140; ++y) flame[  y * bh / 192][170 * bw / 256] = rand() % ceil;

    for (y = 100; y < 140; ++y) flame[  y * bh / 192][170 * bw / 256] = rand() % ceil;
    for (y = 100; y < 140; ++y)
    {
        flame[  y * bh / 192][180 * bw / 256] = rand() % ceil;

        if (y < 120)
            x = 180 + (y - 100);
        else
            x = 220 - (y - 100);

        flame[  y * bh / 192][  x * bw / 256] = rand() % ceil;
    }
}

static void
draw_fire(float t)
{
    int x, y;

    if (t > 0 && t < 1)
        draw_euclid(64);

    for (y = bh - 1; y > 0; --y)
    {
        for (x = 0; x < bw; ++x)
        {
            double a = 0;

            a += 0.60 * flame[y - 1][x];

            if (x > 0)
                a += 0.05 * flame[y - 1][x - 1];

            if (x < bw - 1)
                a += 0.05 * flame[y - 1][x + 1];

            if (y > 1)
            {
                a += 0.20 * flame[y - 2][x];

                if (x > 0)
                    a += 0.05 * flame[y - 2][x - 1];

                if (x < bw - 1)
                    a += 0.05 * flame[y - 2][x + 1];
            }

            flame[y][x] = 0.99 * a;
        }
    }

    for (x = 0; x < bw; ++x)
        flame[0][x] = 0;

    if (t > 0 && t < 1)
        draw_euclid(128);

    for (y = 0; y < bh; ++y)
    {
        for (x = 0; x < bw; ++x)
        {
            pixels[y * bw * 3 + x * 3 + 0] = flame_palette[flame[y][x]][0];
            pixels[y * bw * 3 + x * 3 + 1] = flame_palette[flame[y][x]][1];
            pixels[y * bw * 3 + x * 3 + 2] = flame_palette[flame[y][x]][2];
        }
    }

    glPixelZoom((float)sw / bw, (float)sh / bh);
    glDrawPixels(bw, bh, GL_RGB, GL_UNSIGNED_BYTE, pixels);

    if (t < 1)
        for (x = 0; x < bw; ++x)
            flame[0][x] = rand();
}

////////////////////////////////////////////////////////////////////////

static void
tga_write(FILE *file, uint8_t *pixels, const uint16_t width, const uint16_t height)
{
    struct tga_header h;
    uint8_t *p;
    int i;

    memset(&h, 0, sizeof(struct tga_header));
    h.image_type = 2;
    h.y_origin = 0;
    h.image_width = width;
    h.image_height = height;
    h.pixel_depth = 24;
    h.image_descriptor = 0;

    if(1 != fwrite(&h, sizeof(struct tga_header), 1, file))
        err(EXIT_FAILURE, "fwrite header");

    p = pixels;

    for(i = 0; i < width * height; ++i)
    {
        if(1 != fwrite(p + 2, 1, 1, file))
            err(EXIT_FAILURE, "fwrite pixels");

        if(1 != fwrite(p + 1, 1, 1, file))
            err(EXIT_FAILURE, "fwrite pixels");

        if(1 != fwrite(p + 0, 1, 1, file))
            err(EXIT_FAILURE, "fwrite pixels");

        p += 3;
    }
}

////////////////////////////////////////////////////////////////////////

static void
display(void) {
    static int frames = 0;

    float t, u, x, y, z;

    if (RECORD)
        t = (float)frames / 30 - 2;
    else
        t = elapsed() - 2;

    if (t < 0)
    {
        z = 2;
        x = 0;
        y = 0;

        draw_mandelbrot(x, y, z, -.5 * t, 0);
    }
    else if (t < 16.5)
    {
        u = (t - 0) / (16.5 - 0);

        z = pow(2, lerp(1, -12, u));
        x = lerp(.0, -0.6506, 1 - z / 2);
        y = lerp(.0, -0.4785, 1 - z / 2);

        draw_mandelbrot(x, y, z, 0, u);
    }
    else if (t < 30)
    {
        u = (t - 16.5) / (30 - 16.5);

        glClearColor(0.0, 0.0, 0.0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);

        draw_koch(u);
    }
    else if (t < 57.73)
    {
        u = (t - 30) / (45 - 30);

        draw_kochz(u);
    }
    else if (t < 66.06)
    {
        u = (t - 57.73) / (66.25 - 57.73);

        draw_qochz(1 - u / 2);
    }
    else if (t < 66.28)
    {
        draw_qochz(.3);
    }
    else if (t < 66.8)
    {
        draw_qochz(0);
    }
    else if (t < 77)
    {
        u = (t - 65) / (77 - 65);

        glClearColor(1.0, 1.0, 1.0, 0.0);
        glClear(GL_COLOR_BUFFER_BIT);

        draw_qoch(1 - u);
    }
    else if (t < 80)
    {
        u = (t - 77) / (80 - 77);

        glClearColor(1 - u, 1 - u, 1 - u, 0);
        glClear(GL_COLOR_BUFFER_BIT);

        draw_qoch(0);
    }
    else if (t < 81.22)
    {
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    else if (t < 84.14)
    {
        draw_fire(0);
    }
    else if (t < 87.88)
    {
        draw_fire(.2);
    }
    else if (t < 93)
    {
        draw_fire(1);
    }
    else
    {
        exit(EXIT_SUCCESS);
    }

    glFlush();
    glFinish();

    if (RECORD)
    {
        FILE *f;
        char path[100];

        glReadPixels(0, 0, sw, sh, GL_RGB, GL_UNSIGNED_BYTE, frame);
        sprintf(path, "frame_%06d.tga", frames);
        f = fopen(path, "w");
        tga_write(f, frame, sw, sh);
        fclose(f);

        ++frames;
    }

    glutSwapBuffers();
}

static void
keyboard(unsigned char key, int x, int y) {
    UNUSED(x);
    UNUSED(y);

    if (key == 27)
        exit(0);
}

static void
reshape(int w, int h) {
    glViewport(0, 0, w, h);
}

int
main(int argc, char *argv[])
{
    int i;

    if (argc != 3)
        errx(EXIT_FAILURE, "usage: %s WIDTH HEIGHT", argv[0]);

    sw = atoi(argv[1]);
    sh = atoi(argv[2]);

    bw = sw / quality;
    bh = sh / quality;

    if (NULL == (flame = malloc(bh  * sizeof(uint8_t *))))
        errx(EXIT_FAILURE, "malloc flame");
    for (i = 0; i < bh; ++i)
        if (NULL == (flame[i] = malloc(bw)))
            errx(EXIT_FAILURE, "malloc flame");

    if (NULL == (pixels = malloc(bh * bw * 3)))
        errx(EXIT_FAILURE, "malloc pixels");

    if (RECORD)
        if (NULL == (frame = malloc(sh * sw * 3)))
            errx(EXIT_FAILURE, "malloc pixels");

    if (NULL == (kochz = malloc(sh * sh * 3)))
        errx(EXIT_FAILURE, "malloc kochz");

    if (NULL == (qochz = malloc(sh * sh * 3)))
        errx(EXIT_FAILURE, "malloc qochz");

    putenv("__GL_SYNC_TO_VBLANK=1");

    glutInit(&argc, argv);

    glutInitWindowPosition(0, 0);
    glutInitWindowSize(sw, sh);
    glutInitDisplayMode(GLUT_DEPTH | GLUT_DOUBLE | GLUT_RGBA);
    glutCreateWindow(argv[0]);

    if (!RECORD)
        glutFullScreen();

    glDepthMask(0);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    glutDisplayFunc(display);
    glutIdleFunc(display);
    glutKeyboardFunc(keyboard);
    glutReshapeFunc(reshape);

    make_mandelbrot();
    make_koch();
    make_kochz();
    make_qoch();
    make_qochz();
    make_fire();

    if (!RECORD)
    {
        alsa_init();
        alsa_play("euclid.ogg");
    }

    glutMainLoop();

    return EXIT_SUCCESS;
}
