#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include "vincent.h"

static struct fb_var_screeninfo vinfo;
static struct fb_fix_screeninfo finfo;
static char *fbp;

void setpixel(int x, int y, char r, char g, char b)
{
    int location = (x + vinfo.xoffset) * (vinfo.bits_per_pixel/8) +
        (y + vinfo.yoffset) * finfo.line_length;

    if (vinfo.bits_per_pixel == 32) {
        *(fbp + location) = b;
        *(fbp + location + 1) = g;
        *(fbp + location + 2) = r;
        *(fbp + location + 3) = 0;
    } else  { //assume 16bpp
        unsigned short int t = (r >> 3) << 11 | (g >> 2) << 5 | (b >> 3);
        *((unsigned short int*)(fbp + location)) = t;
    }
}

void setpixel_scaled(int factor, int x, int y, char r, char g, char b)
{
    int dx, dy;
    for (dx=0; dx<factor; dx++) {
        for (dy=0; dy<factor; dy++) {
            setpixel(x * factor + dx, y * factor + dy, r, g, b);
        }
    }
}

static struct {
    char r, g, b;
} colors[] = {
    { 255, 0, 0 },
    { 0, 255, 0 },
    { 0, 0, 255 },
    { 255, 255, 0 },
    { 255, 0, 255 },
    { 0, 255, 255 },
};

// http://stackoverflow.com/questions/4996777
int main()
{
    int fbfd = 0;
    long int screensize = 0;
    int x = 0, y = 0;
    int row, col;
    long int location = 0;

    fbfd = open("/dev/fb0", O_RDWR);
    if (fbfd == -1) {
        fbfd = open("/dev/graphics/fb0", O_RDWR);
        if (fbfd == -1) {
            perror("Error: cannot open framebuffer device");
            return 1;
        }
    }

    if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo) == -1) {
        perror("FBIOGET_FSCREENINFO");
        return 1;
    }

    if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo) == -1) {
        perror("FBIOGET_VSCREENINFO");
        return 1;
    }

    printf("%dx%d, %dbpp\n", vinfo.xres, vinfo.yres, vinfo.bits_per_pixel);

    // Figure out the size of the screen in bytes
    screensize = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;

    // Map the device to memory
    fbp = (char *)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
    if ((int)fbp == -1) {
        perror("mmap");
        return 1;
    }

    // Clear to black
    memset(fbp, 0, screensize);

    int scale = 2;

    char r = 255;
    char g = 255;
    char b = 255;

    int ch;
    while ((ch = fgetc(stdin)) != EOF) {
        if (ch < 0 || ch >= 128) {
            x += 8;
            continue;
        } else if (ch == '\n') {
            r = g = b = 255;
            x = 0;
            y += 12;
            continue;
        } else if (ch == '\t') {
            x += 8 * 8;
            continue;
        } else if (ch == 1) {
            r = g = b = 255;
            continue;
        } else if (ch >= 2 && ch <= (sizeof(colors)/sizeof(colors[0]))-2) {
            r = colors[ch-2].r;
            g = colors[ch-2].g;
            b = colors[ch-2].b;
            continue;
        }

        // Wrap lines
        if ((x + 8) * scale > vinfo.xres - 1) {
            x = 0;
            y += 12;
        }

        // If we end up at the end of the screen, clear + start fresh
        // TODO: scroll vertically
        if ((y + 8) * scale > vinfo.yres - 1) {
            memset(fbp, 0, screensize);
            x = y = 0;
        }

        for (row=0; row<8; row++) {
            int rowd = vincent_data[ch][row];
            for (col=0; col<8; col++) {
                int xx = x + 8 - col;
                int yy = y + row;
                if ((rowd & (1 << col)) != 0) {
                    setpixel_scaled(scale, xx, yy, r, g, b);
                }
            }
        }

        x += 8;
    }

    munmap(fbp, screensize);
    close(fbfd);
    return 0;
}
