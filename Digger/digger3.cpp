/*
 * Copyright (c) 2017 James Babcock
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * 
 * 
 */
//
// Digging map generator (third)
// By Jim Babcock
//
// This program is part of the article 'Digging Features' and may be
// distributed under the same terms as that article.
//
// www.jimrandomh.org/rldev
//
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cassert>
#include <vector>
using namespace std;

//
// Some handy things about vectors:
//
// point + direction     = one step away from (point) in (direction)
// point + direction*num = (num) steps in (direction) away from (point)
// dir.right() = 90 degrees clockwise from dir
// dir.left()  = 90 degrees counter-clockwise from dir
//
class Vector
{
public:
	int x, y;
	
	inline Vector()             { x=y=0; }
	inline Vector(int x, int y) { this->x=x; this->y=y; }
	
	inline Vector  operator+(const Vector &vec)  { return Vector(x+vec.x, y+vec.y); }
	inline Vector  operator-(const Vector &vec)  { return Vector(x-vec.x, y-vec.y); }
	inline Vector& operator+=(const Vector &vec) { x += vec.x; y += vec.y; return *this; }
	inline Vector& operator-=(const Vector &vec) { x -= vec.x; y -= vec.y; return *this; }
	inline Vector  operator*(int scalar)         { return Vector(x*scalar, y*scalar); }
	
	inline friend Vector operator*(int scalar, Vector vec) { return Vector(vec.x*scalar, vec.y*scalar); }
	
	inline bool operator==(const Vector &vec) { return x==vec.x && y==vec.y; }
	inline bool operator!=(const Vector &vec) { return x!=vec.x || y!=vec.y; }
	
	inline Vector left()   { return Vector(y, -x); }
	inline Vector right()  { return Vector(-y, x); }
};
int dig_random(Vector pos, Vector heading);
int dig_room(Vector entrance, Vector heading);
int dig_corridor(Vector entrance, Vector heading);
int is_in_bounds(Vector v);
int is_in_bounds_or_border(Vector v);
int is_known(Vector v);
int is_floor(Vector v);
int is_wall(Vector v);
int is_permawall(Vector v);
void dig_tile(Vector v);
void door_tile(Vector v);
void fill_tile(Vector v);
void permawall_tile(Vector v);
int rand_range(int Min, int Max);


enum { TILE_UNKNOWN, TILE_FLOOR, TILE_WALL, TILE_PERMAWALL, TILE_DOOR };
int **grid;
int size_x, size_y;
const int max_tries = 5;


class Doorway
{
public:
	Doorway(Vector l, Vector h, bool door) { location=l; heading=h; has_door=door; }
	Vector location, heading;
	bool has_door;
};
std::vector<Doorway> doorways;


void dig_loop(void)
{
	Vector entrance = Vector(size_x/2, size_y-1);
	doorways.push_back(Doorway(entrance, Vector(0, -1), true));
	
	door_tile(entrance);
	fill_tile(entrance+Vector(1, 0));
	fill_tile(entrance-Vector(1, 0));
	
	while(doorways.size() > 0)
	{
		int which = rand_range(0, doorways.size()-1);
		Doorway door = doorways[which];
		doorways.erase(doorways.begin()+which);
		
		if(dig_random(door.location, door.heading))
		{
			if(door.has_door)
				door_tile(door.location);
			else if(is_wall(door.location))
				dig_tile(door.location);
		}
	}
}

// Dig either a room or a corridor. Retry until something fits, or max_tries
// times total.
int dig_random(Vector pos, Vector heading)
{
	int success = 0;
	int tries;
	
	for(tries=0; tries<max_tries; tries++)
	{
		switch( rand_range(0, 1) ) {
			default:
			case 0: success = dig_room         (pos, heading); break;
			case 1: success = dig_corridor     (pos, heading); break;
		}
		if(success)
			return 1;
	}
	return 0;
}

// Dig a randomly sized room with an entrance at #entrance, facing in the
// direction given by #heading. If it doesn't fit, return 0 without changing
// anything. If it does fit, try to place more connected to the room as well.
int dig_room(Vector entrance, Vector heading)
{
	Vector size;
	Vector pos, corner;
	Vector door_pos;
	int entrance_offset;
	
	// ########
	// #......# ^
	// #......# |size.y
	// #......# |
	// #......# v
	// C####.##
	//  <---->
	//  size.x
	// <---->
	// entrance_offset
	
	size.x = rand_range(3, 6);
	size.y = rand_range(3, 6);
	entrance_offset = rand_range(1, size.x);
	
	corner = entrance + (heading.left()*entrance_offset);
	
	// Check the area to see if any of it has already been dug
	pos=corner;
	for(int yi=0; yi<size.y+2; yi++) {
		for(int xi=0; xi<size.x+2; xi++) {
			if(!is_in_bounds_or_border(pos))
				return 0;
			if(!is_wall(pos) && pos!=entrance)
				return 0;
			pos += heading.right();
		}
		pos -= heading.right()*(size.x+2);
		pos += heading;
	}
	
	// Fill the whole area with rock
	pos=corner;
	for(int yi=0; yi<size.y+2; yi++) {
		for(int xi=0; xi<size.x+2; xi++) {
			fill_tile(pos);
			
			pos += heading.right();
		}
		pos -= heading.right()*(size.x+2);
		pos += heading;
	}
	
	// Turn the corners into permawall
	permawall_tile(corner);
	permawall_tile(corner + (heading.right()*(size.x+1)));
	permawall_tile(corner                                + (heading*(size.y+1)));
	permawall_tile(corner + (heading.right()*(size.x+1)) + (heading*(size.y+1)));
	
	// Dig out the inside
	pos=corner+heading+heading.right();
	for(int yi=0; yi<size.y; yi++) {
		for(int xi=0; xi<size.x; xi++) {
			dig_tile(pos);
			pos += heading.right();
		}
		pos -= heading.right()*size.x;
		pos += heading;
	}
	
	// Make the entrance a door
	door_tile(entrance);
	
	// Left wall connection
	door_pos = corner + heading*rand_range(1, size.y);
	doorways.push_back(Doorway(door_pos, heading.left(), true));
	// Opposite wall connection
	door_pos = corner + heading*(size.y+1) + heading.right()*rand_range(1, size.x);
	doorways.push_back(Doorway(door_pos, heading, true));
	// Right wall connection
	door_pos = corner + heading.right()*(size.x+1) + heading*rand_range(1, size.y);
	doorways.push_back(Doorway(door_pos, heading.right(), true));
	
	return 1;
}

int dig_corridor(Vector entrance, Vector heading)
{
	int length = rand_range(2, 6);
	int ii;
	Vector pos;
	Vector left_pos, right_pos;
	bool found_intersect = false;
	
	pos       = entrance;
	left_pos  = entrance + heading.left();
	right_pos = entrance + heading.right();
	
	// Check that the corridor doesn't intersect something in a bad way
	for(ii=0; ii<length; ii++)
	{
		pos       += heading;
		left_pos  += heading;
		right_pos += heading;
		
		if(!is_in_bounds(pos))
			return 0;
		if(!is_wall(pos)) {
			found_intersect = true;
			length = ii;
			break;
		}
		if( !is_wall(left_pos) || !is_wall(right_pos)
		 || is_permawall(pos) )
			return 0;
	}
	
	// Drop corridors that're so short they'd have 2 consecutive doors, like
	// this:
	//     ..##..
	//     ..++..
	//     ..##..
	if(length<=1)
		return 0;
	
	// Prune dead ends
	if(!found_intersect)
		return 0;
	
	// Dig the corridor
	pos       = entrance;
	left_pos  = entrance + heading.left();
	right_pos = entrance + heading.right();
	
	for(ii=0; ii<length; ii++)
	{
		pos       += heading;
		left_pos  += heading;
		right_pos += heading;
		
		dig_tile(pos);
		fill_tile(left_pos);
		fill_tile(right_pos);
		
		if(!is_in_bounds(pos+heading))
			break;
		if(!is_wall(pos+heading))
			break;
	}
	
	if(!is_in_bounds(pos+heading) || is_wall(pos+heading)) {
		// If not connected to anything
		// Seal off the end (it'll turn into a door when connected)
		fill_tile(pos);
		doorways.push_back(Doorway(pos, heading, false));
	} else {
		// Put a doorway at the end
		door_tile(pos);
	}
	
//	// Put something at the end, or, if that fails, seal off the dead end.
//	if(!dig_random(pos, heading))
//		fill_tile(pos);
	
	return 1;
}


int is_in_bounds(Vector v)
{
	return v.x>=1 && v.y>=1 && v.x<size_x-1 && v.y<size_y-1;
}
int is_in_bounds_or_border(Vector v)
{
	return v.x>=0 && v.y>=0 && v.x<size_x && v.y<size_y;
}

int is_known(Vector v) {
	return grid[v.y][v.x] != TILE_UNKNOWN;
}
int is_floor(Vector v) {
	return grid[v.y][v.x] == TILE_FLOOR;
}
int is_wall(Vector v) {
	return grid[v.y][v.x] == TILE_WALL
	    || grid[v.y][v.x] == TILE_UNKNOWN
	    || grid[v.y][v.x] == TILE_PERMAWALL;
}
int is_permawall(Vector v) {
	return grid[v.y][v.x] == TILE_PERMAWALL;
}
void dig_tile(Vector v) {
	grid[v.y][v.x] = TILE_FLOOR;
}
void door_tile(Vector v) {
	grid[v.y][v.x] = TILE_DOOR;
}
void fill_tile(Vector v) {
	grid[v.y][v.x] = TILE_WALL;
}
void permawall_tile(Vector v) {
	grid[v.y][v.x] = TILE_PERMAWALL;
}


// Return a random number between Min and Max.
int rand_range(int Min, int Max)
{
	return rand() % (Max-Min+1) + Min;
}


void init_map(void)
{
	int xi, yi;
	
	grid = new int*[size_y];
	
	for(yi=0; yi<size_y; yi++)
		grid[yi] = new int[size_x];
	
	for(yi=0; yi<size_y; yi++)
	for(xi=0; xi<size_x; xi++)
		grid[yi][xi] = TILE_UNKNOWN;
}


void print_map(void)
{
	for(int yi=0; yi<size_y; yi++)
	{
		for(int xi=0; xi<size_x; xi++)
		{
			switch(grid[yi][xi]) {
				case TILE_UNKNOWN:   putchar(' '); break;
				case TILE_FLOOR:     putchar('.'); break;
				case TILE_WALL:      putchar('#'); break;
				case TILE_PERMAWALL: putchar('#'); break;
				case TILE_DOOR:      putchar('+'); break;
			}
		}
		putchar('\n');
	}
}

int main(int argc, char **argv)
{
	if(argc < 3) {
		printf("Usage: %s xsize ysize\n", argv[0]);
		return 1;
	}
	size_x     = atoi(argv[1]);
	size_y     = atoi(argv[2]);
	
	srand(time(NULL));
	init_map();
	
	dig_loop();
	
	print_map();
	return 0;
}


