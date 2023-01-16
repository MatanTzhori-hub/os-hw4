#include <unistd.h> 

class MallocMetadata{
public:
    size_t size;
    bool is_free;
    MallocMetadata* next;
    MallocMetadata* prev;

    MallocMetadata() = default;
    MallocMetadata(size_t size) : size(size), is_free(false) {};
};
 
class AllocedBlocksList{
    public:
        MallocMetadata* head;
        AllocedBlocksList() = default;
        ~AllocedBlocksList() = default;
    
        void* insertBlock(size_t size);
        void* allocateFreeBlock(size_t size);
        void releaseBlock(void* ptr);

        size_t num_free_blocks();
        size_t num_free_bytes();
        size_t num_allocated_blocks();
        size_t num_allocated_bytes();
        size_t num_meta_data_bytes();
        size_t size_meta_data();

        static MallocMetadata* data_to_meta(void* p);
        static void* meta_to_data(void* p);
};

MallocMetadata* AllocedBlocksList::data_to_meta(void* p){
    return (MallocMetadata*)((char*)p - sizeof(MallocMetadata));
}

void* AllocedBlocksList::meta_to_data(void* p){
    return (char*)p + sizeof(MallocMetadata);
}

void* AllocedBlocksList::insertBlock(size_t size)
{
    MallocMetadata* new_block = (MallocMetadata*)sbrk(sizeof(*new_block) + size);
    if(new_block == (void*)-1){
        return NULL;
    }
    new_block->is_free = 0;
    new_block->size = size;
    new_block->next = NULL;
    new_block->prev = NULL;

    if (head == NULL) {
        head = new_block;
        return meta_to_data(head);
    }
 
    MallocMetadata* temp = head;
    while (temp->next != NULL) {
        temp = temp->next;
    }
    
    new_block->prev = temp;
    temp->next = new_block;
    return meta_to_data(new_block);
}

void* AllocedBlocksList::allocateFreeBlock(size_t size){
    MallocMetadata* temp = this->head;

    if (temp == NULL) {
        return NULL;
    }
 
    while (temp != NULL) {
        if(temp->is_free == 1 && temp->size >= size){
            temp->is_free = 0;
            return meta_to_data(temp);
        }
        temp = temp->next;
    }
    return NULL;
}

void AllocedBlocksList::releaseBlock(void* ptr){
    MallocMetadata* meta_data_ptr = data_to_meta(ptr);
    if(meta_data_ptr->is_free){
        return;
    }
    meta_data_ptr->is_free = 1;
}

size_t AllocedBlocksList::num_free_blocks() {
    size_t free_blocks = 0;

    MallocMetadata* temp = this->head;
    while (temp != NULL) {
        if(temp->is_free == 1) {
            free_blocks++;
        }
        temp = temp->next;
    }
    return free_blocks;
}

size_t AllocedBlocksList::num_free_bytes() {
    size_t free_bytes = 0;

    MallocMetadata* temp = this->head;
    while (temp != NULL) {
        if(temp->is_free == 1) {
            free_bytes += temp->size;
        }
        temp = temp->next;
    }
    return free_bytes;
}

size_t AllocedBlocksList::num_allocated_blocks() {
    size_t alloced_blocks = 0;

    MallocMetadata* temp = this->head;
    while (temp != NULL) {
        alloced_blocks++;
        temp = temp->next;
    }
    return alloced_blocks;
}

size_t AllocedBlocksList::num_allocated_bytes() {
    size_t alloced_bytes = 0;

    MallocMetadata* temp = this->head;
    while (temp != NULL) {
        alloced_bytes += temp->size;
        temp = temp->next;
    }
    return alloced_bytes;
}

size_t AllocedBlocksList::num_meta_data_bytes() {
    return num_allocated_blocks() * size_meta_data();
}

size_t AllocedBlocksList::size_meta_data() {
    return sizeof(MallocMetadata);
}