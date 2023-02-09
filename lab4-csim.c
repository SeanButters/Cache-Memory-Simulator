//Name: Sean Butterfield

#include "lab4.h"
#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

//define data type to store 8 byte unsigned integer
typedef unsigned long long int int8;

//struct used to hold arguments passed by user
struct Arguments{
    int s; //number of set index bits (the number of sets is 2^s)
    int S; //total number of sets (calculated from 2^s)
    int E; //number of lines per set (associativity)
    int b; //number of block bits (the block size is 2^b)
    int B; //block size (calculated from 2^b)
    char verbose; //verbose flag
};

//struct used to hold results or program between function calls
struct Results
{
    int hits; //Number of hits
    int misses; //Number of misses
    int evicts; //Number of evictions
};


//struct used for the lines in each set
struct Line{
    int uses; //used to find MRU and LRU
    int valid; //check if not empty
    int8 tag;
    char* block;
};

//struct used to for the sets in a cache, hold a set of lines
struct Set{
    struct Line* lines;
};

//struct used to simulated a cache, contains sets, which themselves contain lines
struct Cache{
    struct Set* sets;
};

//returns index of first empty line in set return -1 if no empty lines
int getEmptyLine(struct Set set, struct Arguments* args){
    for (int i = 0; i < (args->E); i++){
        if(set.lines[i].valid == 0){
            return i;
        }
    }
    return -1;
}

//returns index of most recently used line
int getMRU(struct Set set, struct Arguments* args){
    int max = set.lines[0].uses;
    int MRU = 0;
    //find line with highest use
    for(int i = 1; i < (args->E); i++){
        if(set.lines[i].uses > max){
            MRU = i;
            max = set.lines[i].uses;
        }
    }
    return MRU;
}

//returns index of the least recently used line
int getLRU(struct Set set, struct Arguments* args){
    int min = set.lines[0].uses;
    int LRU = 0;
    //find line with least use
    for(int i = 1; i < (args->E); i++){
        if(set.lines[i].uses < min){
            LRU = i;
            min = set.lines[i].uses;
        }
    }
    return LRU;
}

//simulates cache and updates results from a memory trace
void simulateCache(struct Cache cache, struct Arguments* args, struct Results* results, int8 address){
    int tagSize = 64 - (args->b + args->s); //tag size = 64 - (num of block bits + num of sets)
    int8 addressTag = address >> (args->s + args->b); //adress tag = adress >> (num of bits + num of sets)
    int8 setNum = (address << (tagSize)) >> (tagSize + args->b); //index of curent set
    struct Set set = cache.sets[setNum]; //current set
    int hasHit = 0; //flag to keep of wether a hit was found

    //check for hit
    for (int i = 0; i < args->E; i++){
        if((set.lines[i].tag == addressTag) && set.lines[i].valid){ //found hit
            results->hits++; //increment hits
            set.lines[i].uses = set.lines[getMRU(set, args)].uses+1; //increment uses
            hasHit = 1; //set hasHit flag
            break;
        }
    } 

    int emptyLine = getEmptyLine(set, args); //finds first index of empty line, -1 if set is full
    //if no hit and set is not full
    if(!hasHit && emptyLine != -1){
        results->misses++; //increment misses
        set.lines[emptyLine].tag = addressTag; //update empty line
        set.lines[emptyLine].valid = 1; //set valid flag to show line is not empty
        set.lines[emptyLine].uses = set.lines[getMRU(set, args)].uses+1; //increment uses
    }
    else if(!hasHit){ //else evict
        results->misses++; //increment misses
        int LRU = getLRU(set, args); //find index of LRU line
        set.lines[LRU].tag = addressTag; //evict least recently used line
        results->evicts++; //increment evictions
        set.lines[LRU].uses = set.lines[getMRU(set, args)].uses+1; //increment uses
    }
}

//Prints program argument info
void printUsage(){
    printf("Usafe: /csim-ref [-hv] -s <s> -E <E> -b <b> -t <tracefile>\n");
    printf("-h: Optional help flag that prints usage info\n");
    printf("-v: Optional verbose flag that displays trace info\n");
    printf("-s <s>: Number of set index bits (the number of sets is 2^s)\n");
    printf("-E <E>: Associativity (number of lines per set)\n");
    printf("-b <b>: Number of block bits (the block size is 2^b)\n");
    printf("-t <tracefile>: Name of the valgrind trace to replay\n");
}

//main method
int main(int argc, char **argv){
	struct Arguments* args = (struct Arguments*)malloc(sizeof(struct Arguments)); //contains argument data
    struct Results* results = (struct Results*)malloc(sizeof(struct Results)); //contains result data
    int8 totalSets; //total number of sets

    char c; //holds the arguments the users send
	char *traceFile; //file with trace data

    //Take arguments
    while( (c=getopt(argc,argv,"s:E:b:t:vh")) != -1){
        switch(c) {
        case 's':
            args->s = atoi(optarg);
            totalSets = pow(2.0, args->s);
            break;
        case 'E':
            args->E = atoi(optarg);
            break;
        case 'b':
            args->b = atoi(optarg);
            break;
        case 't':
            traceFile = optarg;
            break;
        case 'v':
            args->verbose = 1;
            break;
        case 'h':
            printUsage();
            exit(0);
        default:
            printf("Unkown argument %c \nUse ./csim -h to display valid arguments",c);
            exit(0);
        }
    }

    //initialize results to 0
	results->hits = 0;
	results->misses = 0;
	results->evicts = 0;

    //create simulated cache
	struct Cache cache;
    struct Set set;
    struct Line line;
    //allocate memory for cache structure
    cache.sets = (struct Set*) malloc(sizeof(struct Set)* totalSets);
    for (int i = 0; i < totalSets; i++){
        set.lines = (struct Line*) malloc (sizeof(struct Line) * args->E);
        cache.sets[i] = set;
        //initialize values to 0
        for (int j = 0; j < (args->E); j++){
            line.uses = 0;
            line.valid = 0;
            line.tag = 0;
            set.lines[j] = line;
        }
    }

    char instruction; //holds current instruciton argument
    int8 address; //holds current memory adress
    int size;
    //read trace file data and run tests
	FILE* file = fopen(traceFile, "r");
	if (file != NULL) {
		while (fscanf(file, " %c %llx,%d", &instruction, &address, &size) == 3) {
            if(instruction == 'L' || instruction == 'S'){//load or save
                simulateCache(cache, args, results, address);
            }
            else if(instruction == 'M'){//load followed by save
                simulateCache(cache, args, results, address);
                simulateCache(cache, args, results, address);
            }
		}
	}
    fclose(file);

    //ouput results
    printSummary(results->hits, results->misses, results->evicts);

    //free memory
    for (int i = 0; i < (args->E); i++){ //free lines
        if(cache.sets[i].lines != NULL){
            free(cache.sets[i].lines);
        }
    }
    if(cache.sets != NULL){ //free sets
        free(cache.sets);
    }
    free(args);
    free(results);
    return 0;
}