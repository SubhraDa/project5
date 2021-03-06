/*
   Main program for the virtual memory project.
   Make all of your modifications to this file.
   You may add or rearrange any code or data as you need.
   The header files page_table.h and disk.h explain
   how to use the page table and disk interfaces.
   */

#include "page_table.h"
#include "disk.h"
#include "program.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

//Global Variables
int npages;
int nframes;
int OLDEST_FRAME = 0;
const char *algorithm;
int diskReads, diskWrites, pageFaults;
int *frameTable = NULL;
int *lruTable = NULL;
struct disk *disk; 
char *physmem;

//initialize array with -1 values (empty spots)
void initialize_frame_table(){
    int i;
    for(i=0; i < nframes; i++){
        frameTable[i] = -1;
    }
}
//check if physical memory is full
int table_full(){
    int i;
    int tableFull = 1;
    for(i = 0; i < nframes; i++){
        if(frameTable[i] == -1){
            tableFull = 0;
        }
    }
    return tableFull;
}

//check if a page is in the frame table array (in physical memory)
int in_frame_table(int page){
    int i;
    for(i = 0; i < nframes; i++){
        if(frameTable[i] == page) return 1;
    }
    return 0;
}

//choose a page at random to replace
void rand_handler(struct page_table *pt, int page){
    pageFaults++;
    int fr = rand() % nframes;
    //printf("%d ", fr);
    int pg = frameTable[fr];
    disk_write(disk, pg, &physmem[fr*npages]);
    diskWrites++;
    disk_read(disk, page, &physmem[fr*npages]);
    diskReads++;
    page_table_set_entry(pt, page, fr, PROT_READ);
    frameTable[fr] = page;
    page_table_set_entry(pt, pg, 0, 0);
}

//replace pages on a first in, first out basis
void fifo_handler(struct page_table *pt, int page) {
    pageFaults++;
    int fr = OLDEST_FRAME;
    int pg = frameTable[fr];
    disk_write(disk, pg, &physmem[fr*npages]);
    diskWrites++;
    disk_read(disk, page, &physmem[fr*npages]);
    diskReads++;
    page_table_set_entry(pt, page, fr, PROT_READ);
    frameTable[fr] = page;
    page_table_set_entry(pt, pg, 0, 0);
    OLDEST_FRAME++;
    if (OLDEST_FRAME == nframes) {
        OLDEST_FRAME = 0;
    }
}

void custom_handler(struct page_table *pt, int page){
    pageFaults++;
    int i, fr, val;
    int max = 0;
    //replace the page with the highest value -- least recently had its bits changed
    for(i = 0; i < nframes; i++){
        if(lruTable[i] > max){
            max = lruTable[i];
            fr = i;
        }
    }
    int pg = frameTable[fr];
    disk_write(disk, pg, &physmem[fr*npages]);
    diskWrites++;
    disk_read(disk, page, &physmem[fr*npages]);
    diskReads++;
    page_table_set_entry(pt, page, fr, PROT_READ);
    frameTable[fr] = page;
    page_table_set_entry(pt, pg, 0, 0);
    lruTable[fr] = 0;
    //updating array for custom algorithm
    for(i=0; i<nframes; i++){
        if(i == fr) continue;
        val = lruTable[i];
        val++;
        lruTable[i] = val;
    }
}

void same_num_pf_handler(struct page_table *pt, int page){
    //same number of pages and frames
    pageFaults++;
    page_table_set_entry(pt, page, page, PROT_READ|PROT_WRITE);
    //page_table_print(pt);
    pageFaults++;
}

void diff_num_pf_handler(struct page_table *pt, int page){
    //different number of pages and frames
    int frame, bits, j, i, val;
    page_table_get_entry(pt, page, &frame, &bits);
    //checking to see if the page is in frameTable
    int found = in_frame_table(page);
    //we didn't find it in frameTable
    if(!found && bits == 0){
        for(j = 0; j < nframes; j++){
            //printf("frameTable[%d]: %d\n", j, frameTable[j]);
            if(frameTable[j] == -1){ //we found an empty spot!
                lruTable[j] = 0;
                for(i=0; i<nframes; i++){ //update array for custom algorithm
                    if(i == j) continue;
                    val = lruTable[i];
                    val++;
                    lruTable[i] = val;
                }
                page_table_set_entry(pt, page, j, PROT_READ);
                frameTable[j] = page;
                disk_read(disk, page, &physmem[j*nframes]);
                diskReads++;
                if(table_full()) return;
                else break;
            }
        }
	//table is full - we need to replace a page
        if(table_full()){
            if (!strcmp(algorithm, "rand")){
                rand_handler(pt, page);
                //printf("\n");
            }
            else if(!strcmp(algorithm, "fifo")){
                fifo_handler(pt, page);
                //printf("\n");
            }
            else if(!strcmp(algorithm, "custom")){
                custom_handler(pt, page);
                //printf("\n");
            }
	    else{
	      printf("unknown algorithm\n");
	      printf("use: virtmem <npages> <nframes> <rand|fifo|custom> <sort|scan|focus>\n");
	      exit(1);
	    }
        }
    }
    //found page in frame table -- need to change permission bits
    if(found){
      if(bits == 1){ //need to set write bits
            page_table_set_entry(pt, page, frame, PROT_READ|PROT_WRITE);
            //custom algorithm - updating array
            int i, val;
            lruTable[frame] = 0;
            for(i=0; i<nframes; i++){
                if(i == frame) continue;
                val = lruTable[i];
                val++;
                lruTable[i] = val;
            }
        }
        else if(bits == 3){
            page_table_set_entry(pt, page, frame, PROT_READ|PROT_WRITE|PROT_EXEC);
        }    
    }
}

void page_fault_handler( struct page_table *pt, int page ){
    //pageFaults++;
    //printf("page fault on page #%d\n",page);
    if(npages == nframes){
        same_num_pf_handler(pt, page);
    }
    else{
        diff_num_pf_handler(pt, page);
        //page_table_print(pt);
        //exit(1);*/
        return;
    }
}

int main( int argc, char *argv[] ){
    //used time so random values were not consistent
    time_t t;
    srand((unsigned) time(&t)); 
    pageFaults = 0;
    diskReads = 0;
    diskWrites = 0;
    if(argc!=5) {
        printf("use: virtmem <npages> <nframes> <rand|fifo|custom> <sort|scan|focus>\n");
        return 1;
    }

    npages = atoi(argv[1]);
    nframes = atoi(argv[2]);
    if(nframes > npages){ //if more frames than pages
        nframes = npages;
    }
    frameTable = malloc(sizeof(int)*(nframes-1));
    lruTable = malloc(sizeof(int)*(nframes-1));
    initialize_frame_table();
    algorithm = argv[3];
    const char *program = argv[4]; 

    //  struct disk *disk = disk_open("myvirtualdisk",npages);
    disk = disk_open("myvirtualdisk", npages);
    if(!disk) {
        fprintf(stderr,"couldn't create virtual disk: %s\n",strerror(errno));
        return 1;
    }  
    struct page_table *pt = page_table_create( npages, nframes, page_fault_handler );
    if(!pt) {
        fprintf(stderr,"couldn't create page table: %s\n",strerror(errno));
        return 1;
    }

    char *virtmem = page_table_get_virtmem(pt);

    //char *physmem = page_table_get_physmem(pt);
    physmem = page_table_get_physmem(pt);

    if(!strcmp(program,"sort")) {
        sort_program(virtmem,npages*PAGE_SIZE);

    } else if(!strcmp(program,"scan")) {
        scan_program(virtmem,npages*PAGE_SIZE);

    } else if(!strcmp(program,"focus")) {
        focus_program(virtmem,npages*PAGE_SIZE);

    } else {
        fprintf(stderr,"unknown program: %s\n",argv[3]);
        return 1;
    }
    printf("Page Faults: %d\n", pageFaults);
    printf("Disk Reads: %d\n", diskReads);
    printf("Disk Writes: %d\n", diskWrites);
    page_table_delete(pt);
    disk_close(disk);
    free(frameTable);
    free(lruTable);

    return 0;
}
