#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <math.h>
#include <time.h>

#include "mcmap.h"
#include "cswap.h"

struct mcmap_region *mcmap_region_read(int ix, int iz, const char *path)
	{
	FILE *r_file;
	char r_name[MCMAP_MAXSTR];
	struct stat r_stat;
	uint8_t *buff;
	struct mcmap_region *r;
	unsigned int x,z,i;
	if (path == NULL)
		{
		snprintf(mcmap_error,MCMAP_MAXSTR,"'path' is NULL");
		return NULL;
		}

	//resolve filename from regions directory...
	i = strlen(path);
	if (i==0 || path[i-1] == '/')
		snprintf(r_name, MCMAP_MAXSTR, "%sr.%d.%d.mca", path, ix, iz);
	else
		snprintf(r_name, MCMAP_MAXSTR, "%s/r.%d.%d.mca", path, ix, iz);
	//open file...
	if ((r_file = fopen(r_name,"rb")) == NULL)
		{
		snprintf(mcmap_error,MCMAP_MAXSTR,"fopen() on '%s': %s",r_name,strerror(errno));
		return NULL;
		}
	//determine filesize...
	if (fstat(fileno(r_file),&r_stat) != 0)
		{
		snprintf(mcmap_error,MCMAP_MAXSTR,"fstat() on '%s': %s",r_name,strerror(errno));
		return NULL;
		}
	//allocate buffer...
	if ((buff = (uint8_t *)calloc(r_stat.st_size,1)) == NULL)
		{
		snprintf(mcmap_error,MCMAP_MAXSTR,"calloc() returned NULL");
		return NULL;
		}
	//copy file to buffer...
	if ((i = fread(buff,1,r_stat.st_size,r_file)) != r_stat.st_size)
		{
		snprintf(mcmap_error,MCMAP_MAXSTR,"fread() encountered %s on the last %d requested bytes of '%s'",(ferror(r_file)?"an error":"EOF"),(unsigned int)r_stat.st_size-i,r_name);
		return NULL;
		}
	//don't need this anymore...
	fclose(r_file);

	//allocate navigation structure
	if ((r = (struct mcmap_region *)calloc(1,sizeof(struct mcmap_region))) == NULL)
		{
		snprintf(mcmap_error,MCMAP_MAXSTR,"calloc() returned NULL");
		return NULL;
		}

	//engage structure to memory space
	r->header = (struct mcmap_region_header *)buff;
	r->size = r_stat.st_size;
	for (z=0;z<32;z++)
		{
		for (x=0;x<32;x++)
			{
			if (r->header->locations[z][x].sector_count > 0)
				{
				//extract big-endian 32-bit integer from r->header->dates[z][x]
				r->dates[z][x] = cswapr_32(&(r->header->dates[z][x][0]));
				//extract big-endian 24-bit integer from r->header->location[z][x].offset
				r->locations[z][x] = cswapr_24(&(r->header->locations[z][x].offset[0]));
				//connect pointers
				_mcmap_region_chunk_refresh(r,x,z);
				//extract big-endian 32-bit integer for precise chunk size
				r->chunks[z][x].size = cswapr_32(&(r->chunks[z][x].header->length[0]))-1;
				//sanity check
				if (_mcmap_region_chunk_check(r,x,z) == -1)
					{
					snprintf(mcmap_error,MCMAP_MAXSTR,"'%s': %s",r_name,mcmap_error);
					return NULL;
					}
				}
			else
				{
				r->chunks[z][x].header = NULL;
				r->chunks[z][x].size = 0;
				r->chunks[z][x].data = NULL;
				r->locations[z][x] = 0;
				}
			}
		}

	return r;
	}

	//assign 'mcmap_region_chunk' pointers, of the chunk at the given coordinates, to the proper places in the memory buffer - assuming correct values everywhere else
void _mcmap_region_chunk_refresh(struct mcmap_region *r, int x, int z)
	{
	uint8_t *b;
	int i;
	//let's avoid dereferencing any NULL pointers, shall we?
	if (r == NULL || r->header == NULL)
		return;
	b = (uint8_t *)r->header;
	i = r->locations[z][x]*4096;
	//connect pointers
	if (r->locations[z][x] >= 2)
		{
		//connect 5-byte chunk header
		r->chunks[z][x].header = (struct mcmap_region_chunk_header *)&(b[i]);
		//'r->chunks[z][x].data' will now point to a block of 'r->chunks[z][x].size' bytes
		r->chunks[z][x].data = &(b[i+5]);
		}
	return;
	}

//check an 'mcmap_region_chunk' for sanity, return 0 if good and -1 if bad
int _mcmap_region_chunk_check(struct mcmap_region *r, int x, int z)
	{
	int i;
	//let's avoid dereferencing any NULL pointers, shall we?
	if (r == NULL || r->header == NULL)
		return 0;
	//chunk listing should not point anywhere in the file header
	if (r->locations[z][x] < 2)
		{
		snprintf(mcmap_error,MCMAP_MAXSTR,"malformed region: chunk (%d,%d) was listed with invalid location %u",x,z,r->locations[z][x]);
		return -1;
		}
	//chunk listing should not point past the end of the file
	i = r->locations[z][x]*4096;
	if (i+r->header->locations[z][x].sector_count*4096 > r->size)
		{
		snprintf(mcmap_error,MCMAP_MAXSTR,"malformed region: chunk (%d,%d) was listed to inhabit %u 4KiB sectors ending at byte %u; file is only %u bytes long",x,z,r->header->locations[z][x].sector_count,i+((unsigned int)r->header->locations[z][x].sector_count)*4096,(unsigned int)r->size);
		return -1;
		}
	//listed chunk size should not be larger than the rest of the file
	if (i+5+r->chunks[z][x].size > r->size)
		{
		snprintf(mcmap_error,MCMAP_MAXSTR,"malformed region: chunk (%d,%d) was listed to be %u bytes when only %u bytes remain of the file",x,z,(unsigned int)r->chunks[z][x].size,((unsigned int)r->size)-(i+5));
		return -1;
		}
	//in fact neither should it be larger than the sector count in the file header
	if (r->chunks[z][x].size+5 > r->header->locations[z][x].sector_count*4096)
		{
		snprintf(mcmap_error,MCMAP_MAXSTR,"malformed region: chunk (%d,%d) was listed to be %u bytes, which exceeds the %u bytes designated in the header",x,z,(unsigned int)r->chunks[z][x].size+5,r->header->locations[z][x].sector_count*4096);
		return -1;
		}
	return 0;
	}
