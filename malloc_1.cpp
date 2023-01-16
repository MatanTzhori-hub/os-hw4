#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 

void* smalloc(size_t size){
    if(size == 0 || size > 100000000){
        return NULL;
    }

    void* allocated_mem = sbrk(size);
    if(*(int*)allocated_mem == -1){
        return NULL;
    }

    return allocated_mem;
}


// int main(){
//     int* array = (int*)smalloc(sizeof(*array)*(-10));

//     for(int i=0; i<11; i++)
//         array[i] = i;

//     printf("%d", array[10]);
// }