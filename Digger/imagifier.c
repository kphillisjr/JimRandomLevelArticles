// imagifier: Convert an ASCII representation of a roguelike map into an image
// so it can be marked up in Photoshop.
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
	unsigned width, height;
	unsigned char *data;
} framebuffer;

int get_pixel(const framebuffer *fb, unsigned x, unsigned y)
{
	assert(fb && x<fb->width && y<fb->height);
	return fb->data[y*fb->width + x];
}
void set_pixel(framebuffer *fb, unsigned x, unsigned y, int value)
{
	assert(fb && x<fb->width && y<fb->height);
	fb->data[y*fb->width + x] = value;
}
framebuffer *framebuffer_new(unsigned width, unsigned height)
{
	framebuffer *ret = (framebuffer*)malloc(sizeof(framebuffer));
	ret->width = width;
	ret->height = height;
	ret->data = (char*)malloc(width*height);
	return ret;
}

void blit(framebuffer *dest, framebuffer *source, int dest_x, int dest_y, int width, int height)
{
	int ii, jj;
	for(ii=0; ii<height; ii++)
	for(jj=0; jj<width; jj++)
	{
		set_pixel(dest, jj+dest_x, ii+dest_y, get_pixel(source, jj, ii));
	}
}



typedef struct
{
	unsigned char identsize;          // size of ID field that follows 18 byte header (0 usually)
	unsigned char colourmaptype;      // type of colour map 0=none, 1=has palette
	unsigned char imagetype;          // type of image 0=none,1=indexed,2=rgb,3=grey,+8=rle packed
	
	unsigned char colourmapstart1;    // first colour map entry in palette
	unsigned char colourmapstart2;    // first colour map entry in palette
	unsigned char colourmaplength1;   // number of colours in palette
	unsigned char colourmaplength2;   // number of colours in palette
	unsigned char colourmapbits;      // number of bits per palette entry 15,16,24,32
	
	unsigned char xstart1;            // image x origin
	unsigned char xstart2;            // image x origin
	unsigned char ystart1;            // image y origin
	unsigned char ystart2;            // image y origin
	
	unsigned char width1;             // image width in pixels
	unsigned char width2;             // image width in pixels
	unsigned char height1;            // image height in pixels
	unsigned char height2;            // image height in pixels
	unsigned char bits;               // image bits per pixel 8,16,24,32
	unsigned char descriptor;         // image descriptor bits (vh flip bits)
	
	// pixel data follows header
} TGA_HEADER;


void save_tga(framebuffer *fb, FILE *fout, int left, int top, int right, int bottom)
{
	int width = (right-left),
	    height = (bottom-top);
	int x, y;
	
	int bufsiz = (right-left) * (bottom-top);
	char *pixbuf = (char*)malloc(bufsiz);
	
	// Check preconditions
	assert(right>left && bottom>top);
	assert(fb);
	assert(fout);
	
	// Collect pixel values from the framebuffer
	for(x=left; x<right; x++)
	for(y=top; y<bottom; y++)
		pixbuf[y*width + x] = get_pixel(fb, x, y);
	
	// Write out data
	TGA_HEADER tgaHead = {
		0, 0, 3,
		0, 0, 0, 0, 0,
		0, 0, 0, 0,
		width, width>>8, height, height>>8, 8, 0x20
		};
	fwrite(&tgaHead, sizeof tgaHead, 1, fout);
	fwrite(pixbuf,   bufsiz,         1, fout);
	free(pixbuf);
}



const int tilesize_x = 5,
          tilesize_y = 5;

framebuffer tile_wall = {
	5, 5,
	"#####"
	"#####"
	"#####"
	"#####"
	"#####" };
framebuffer tile_space = {
	5, 5,
	"====="
	"====="
	"====="
	"====="
	"=====" };
framebuffer tile_dot = {
	5, 5,
	"     "
	"     "
	"     "
	"  -  "
	"     " };
framebuffer tile_plus = {
	5, 5,
	"     "
	"  #  "
	" ### "
	"  #  "
	"     " };
framebuffer tile_question = {
	5, 5,
	" ##  "
	"   # "
	"  #  "
	"     "
	"  #  " };

void init_tile(framebuffer *tile)
{
	int ii, jj, v;
	
	for(ii=0; ii<tile->height; ii++)
	for(jj=0; jj<tile->width; jj++)
	{
		v = get_pixel(tile, jj, ii);
		switch(v) {
			case ' ': v=255; break;
			case '-': v=128; break;
			case '#': v=0;   break;
			case '=': v=95;  break;
			default:  v=128; break;
		}
		set_pixel(tile, jj, ii, v);
	}
}

void init_all_tiles(void)
{
	init_tile(&tile_wall);
	init_tile(&tile_space);
	init_tile(&tile_dot);
	init_tile(&tile_plus);
	init_tile(&tile_question);
}

framebuffer *get_tile(char tile)
{
	switch(tile)
	{
		case '#': return &tile_wall;
		case ' ': return &tile_space;
		case '.': return &tile_dot;
		case '+': return &tile_plus;
		default:  return &tile_question;
	}
}



int main(int argc, char **argv)
{
	const char *out_filename = NULL;
	int size_x=0, size_y=0;
	int ii, jj;
	char line_inbuf[512];
	char **tilebuf;
	framebuffer *fb;
	FILE *fout;
	
	if(argc < 2) {
		fprintf(stderr, "Usage: %s [-o filename] size_x size_y\n", argv[0]);
		return 1;
	}
	
	if(!strcmp(argv[1], "-o")) {
		if(argc < 5) {
			fprintf(stderr, "Not enough arguments.\n");
			return 1;
		}
		out_filename = argv[2];
		size_x = atoi(argv[3]);
		size_y = atoi(argv[4]);
	} else {
		size_x = atoi(argv[1]);
		size_y = atoi(argv[2]);
	}
	assert(size_x < 512);
	
	tilebuf = (char**)malloc( sizeof(char*) * size_y );
	for(ii=0; ii<size_y; ii++)
		tilebuf[ii] = (char*)malloc(sizeof(char*) * size_x);
	
	for(ii=0; ii<size_y; ii++)
	{
		fgets(line_inbuf, 512, stdin);
		memcpy(tilebuf[ii], line_inbuf, size_x);
	}
	
	fb = framebuffer_new(size_x*tilesize_x, size_y*tilesize_y);
	init_all_tiles();
	
	for(ii=0; ii<size_y; ii++)
	for(jj=0; jj<size_x; jj++)
	{
		blit(fb, get_tile(tilebuf[ii][jj]), jj*tilesize_x, ii*tilesize_y, tilesize_x, tilesize_y);
	}
	
	if(out_filename)
		fout = fopen(out_filename, "wb");
	else
		fout = stdout;
	if(!fout) {
		fprintf(stderr, "Could not open output file.\n");
		return 1;
	}
	save_tga(fb, fout, 0, 0, fb->width, fb->height);
	fclose(fout);
	
	return 0;
}

