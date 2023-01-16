#include <unistd.h> 

class MallocMetadata{
public:
    size_t size;
    bool is_free;
    MallocMetadata* next;
    MallocMetadata* prev;

    MallocMetadata() = default;
    MallocMetadata(size_t size) : size(size), is_free(false){};
};
 
class AllocedBlocksList{

    public:
        MallocMetadata* head;

        size_t num_free_blocks;
        size_t num_free_bytes;
        size_t num_allocated_blocks;
        size_t num_allocated_bytes;
        size_t num_meta_data_bytes;
        size_t size_meta_data;
 
public:
    AllocedBlocksList() = default;
 
    void* insertBlock(size_t size);
    void* allocateFreeBlock(size_t size);
    void releaseBlock(void* ptr);
};
 

void* AllocedBlocksList::insertBlock(size_t size)
{
    MallocMetadata* new_block = (MallocMetadata*)sbrk(sizeof(*new_block) + size);
    if(*(int*)new_block == -1){
        return NULL;
    }
    new_block->is_free = 0;
    new_block->size = size;
    new_block->next = NULL;
    new_block->prev = NULL;

    num_allocated_blocks++;
    num_allocated_bytes+=size;
    num_meta_data_bytes+=size_meta_data;

    if (head == NULL) {
        head = new_block;
        return (char*)head + size_meta_data;
    }
 
    MallocMetadata* temp = head;
    while (temp->next != NULL) {
        temp = temp->next;
    }
    
    new_block->prev = temp;
    temp->next = new_block;
    return (char*)new_block + size_meta_data;
}

void* AllocedBlocksList::allocateFreeBlock(size_t size){
    MallocMetadata* temp = this->head;

    if (temp == NULL) {
        return NULL;
    }
 
    while (temp != NULL) {
        if(temp->is_free == 1 && temp->size >= size){
            temp->is_free = 0;
            num_free_blocks--;
            num_free_bytes -= temp->size;

            return (char*)temp + size_meta_data;
        }
        temp = temp->next;
    }
    return NULL;
}

void AllocedBlocksList::releaseBlock(void* ptr){
    MallocMetadata* meta_data_ptr = (MallocMetadata*)((char*)ptr - size_meta_data);
    if(meta_data_ptr->is_free){
        return;
    }

    meta_data_ptr->is_free = 1;
    this->num_free_blocks++;
    this->num_free_bytes += meta_data_ptr->size;
}