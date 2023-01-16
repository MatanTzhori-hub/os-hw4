#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <cstring>
#include "linked_list.h" 

#define MAX_SIZE 100000000

AllocedBlocksList allocatedBlocks = AllocedBlocksList();

void* smalloc(size_t size){
    if(size == 0 || size > MAX_SIZE){
        return NULL;
    }

    void* new_block = allocatedBlocks.allocateFreeBlock(size);

    if(new_block == NULL){
        new_block = allocatedBlocks.insertBlock(size);
    }

    return new_block;
}

void* scalloc(size_t num, size_t size){
    char* new_block = (char*)smalloc(num*size);
    if(new_block == NULL){
        return NULL;
    }

    std::memset(new_block, 0, num*size);

    return new_block;
}

void sfree(void* p){
    if(p == NULL){
        return;
    }

    allocatedBlocks.releaseBlock(p);
}

void* srealloc(void* oldp, size_t size){
    if (oldp == NULL) {
        return smalloc(size);
    }

    if(size == 0 || size > MAX_SIZE){
        return NULL;
    }
    MallocMetadata* meta_data_ptr = AllocedBlocksList::data_to_meta(oldp);

    if(meta_data_ptr->size >= size){
        return oldp;
    }

    void* new_block = smalloc(size);
    if(new_block == NULL){
        return NULL;
    }

    memmove(new_block, oldp, size);
    sfree(oldp);

    return new_block;
}

size_t _num_free_blocks(){
    return allocatedBlocks.num_free_blocks();
}

size_t _num_free_bytes(){
    return allocatedBlocks.num_free_bytes();
}

size_t _num_allocated_blocks(){
    return allocatedBlocks.num_allocated_blocks();
}

size_t _num_allocated_bytes(){
    return allocatedBlocks.num_allocated_bytes();
}

size_t _num_meta_data_bytes(){
    return allocatedBlocks.num_meta_data_bytes();
}

size_t _size_meta_data(){
    return allocatedBlocks.size_meta_data();
}


// int main(){
//     void *base = sbrk(0);
//     char *a = (char *)smalloc(10);
//     if(a!=NULL){
//         printf("good\n");
//     }
//     if((size_t)base + _size_meta_data() == (size_t)a){
//         printf("good\n");
//     }
//     //verify_blocks(1, 10, 0, 0);
//     //verify_size(base);
//     sfree(a);
//     //verify_blocks(1, 10, 1, 10);
//     //verify_size(base);
// }

// typedef struct{
//     int i1;
//     int i2;
//     char ch;
// }dummy;

// int main(){
//     int* array = (int*)smalloc(sizeof(*array)*(10));

//     for(int i=0; i<10; i++)
//         array[i] = i;

//     dummy* dum1 = (dummy*)smalloc(sizeof(*dum1));
//     dummy* dum2 = (dummy*)smalloc(sizeof(*dum2));
//     dummy* dum3 = (dummy*)smalloc(sizeof(*dum3));
//     int* arr2 = (int *)srealloc(array, 15*sizeof(int));

//     for(int i=0; i<15; i++)
//         printf("%d\n",arr2[i]);
    
//     sfree(arr2);
//     int* arr3 = (int *)scalloc(15, sizeof(int));

//     for(int i=0; i<15; i++)
//         printf("%d\n",arr3[i]);
// }