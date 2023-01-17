#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <cstring>

#define MAX_SIZE 100000000

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
    
        void* insertBlock(size_t size, MallocMetadata* block = nullptr);
        void* allocateFreeBlock(size_t size);
        void releaseBlock(void* ptr);
        void* SplitAndInsert(size_t new_size, MallocMetadata* old_block);
        void RemoveBlock(MallocMetadata* block);
        MallocMetadata* GetNextIfFree(MallocMetadata* ptr);
        MallocMetadata* GetPrevIfFree(MallocMetadata* ptr);
        void UnionAndInsert(MallocMetadata* curr, MallocMetadata* next, MallocMetadata* prev);

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

void* AllocedBlocksList::insertBlock(size_t size, MallocMetadata* new_block)
{   
    if (new_block == nullptr) {
        new_block = (MallocMetadata*)sbrk(sizeof(*new_block) + size);
        if(new_block == (void*)-1) {
            return NULL;
        }
    }

    new_block->is_free = 0;
    new_block->size = size;
    new_block->next = NULL;
    new_block->prev = NULL;

    if (head == NULL) {
        head = new_block;
        return meta_to_data(head);
    }

    if (head->size > new_block->size) {
        new_block->next = head;
        head->prev = new_block;
        head = new_block;

        return meta_to_data(head);
    }
    if (head->size == new_block->size && head > new_block) {
        new_block->next = head;
        head->prev = new_block;
        head = new_block;

        return meta_to_data(head);
    }
 
    MallocMetadata* temp = head;
    while (temp->next != NULL) {
        if (temp->next->size > new_block->size) {
            temp->next->prev = new_block;
            new_block->next = temp->next;
            new_block->prev = temp;
            temp->next = new_block;
            break;
        }
        if (temp->next->size == new_block->size && temp->next > new_block) {
            temp->next->prev = new_block;
            new_block->next = temp->next;
            new_block->prev = temp;
            temp->next = new_block;
            break;
        }
        temp = temp->next;
    }
    
    if (temp->next == NULL) {
        temp->next = new_block;
        new_block->prev = temp;
    }
    
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

MallocMetadata* AllocedBlocksList::GetNextIfFree(MallocMetadata* ptr) {
    // Find ptr + size + metadata_size
    MallocMetadata* curr = this->head;
    while (curr != NULL) {
        if((char*)curr == (char*)ptr + ptr->size + size_meta_data()) {
            return curr->is_free ? curr : nullptr;
        }
        curr = curr->next;
    }
    return nullptr;
}

MallocMetadata* AllocedBlocksList::GetPrevIfFree(MallocMetadata* ptr) {
    // Find curr + curr_size + metadata_size == ptr
    MallocMetadata* curr = this->head;
    while (curr != NULL) {
        if((char*)curr + curr->size + size_meta_data() == (char*)ptr) {
            return curr->is_free ? curr : nullptr;
        }
        curr = curr->next;
    }
    return nullptr;
}

void AllocedBlocksList::UnionAndInsert(MallocMetadata* curr, MallocMetadata* next, MallocMetadata* prev) {
    int situation = 0;
    RemoveBlock(curr);
    if(next != NULL){
        RemoveBlock(next);
        situation+= 1;
    }
    if(prev != NULL){
        RemoveBlock(prev);
        situation+= 2;
    }

    switch(situation){
        case 1:
            insertBlock(curr->size + next->size + size_meta_data(), curr);
            curr->is_free = 1;
            break;
        case 2:
            insertBlock(curr->size + prev->size + size_meta_data(), prev);
            prev->is_free = 1;
            break;
        case 3:
            insertBlock(prev->size + curr->size + next->size + 2 * size_meta_data(), prev);
            prev->is_free = 1;
            break;
    }

    return;
}

void AllocedBlocksList::releaseBlock(void* ptr){
    MallocMetadata* meta_data_ptr = data_to_meta(ptr);
    if(meta_data_ptr->is_free){
        return;
    }
    meta_data_ptr->is_free = 1;

    MallocMetadata* next_free = this->GetNextIfFree(meta_data_ptr);
    MallocMetadata* prev_free = this->GetPrevIfFree(meta_data_ptr);

    if(next_free != NULL ||  prev_free != NULL){
        this->UnionAndInsert(meta_data_ptr, next_free, prev_free);
    }
}

void AllocedBlocksList::RemoveBlock(MallocMetadata* block) {
    if (block->prev == NULL) {
        head = block->next;
        if(block->next != NULL){
        head->prev = NULL;
        }

    } else {
        block->prev->next = block->next;
        if(block->next!=NULL){
            block->next->prev = block->prev;
        }
    }

    block->next = NULL;
    block->prev = NULL;
}


void* AllocedBlocksList::SplitAndInsert(size_t new_size, MallocMetadata* old_block) {
    RemoveBlock(old_block);
    void* free_block = insertBlock(old_block->size - new_size - size_meta_data(), (MallocMetadata*)((char*)old_block + new_size + size_meta_data()));
    data_to_meta(free_block)->is_free = 1;
    return insertBlock(new_size, old_block);
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


////////////////////////////////////////////////////////
/*
                    Malloc3 Functions                 
                                                      */
////////////////////////////////////////////////////////

AllocedBlocksList allocatedBlocks = AllocedBlocksList();

void* smalloc(size_t size){
    if(size == 0 || size > MAX_SIZE){
        return NULL;
    }

    void* new_block = allocatedBlocks.allocateFreeBlock(size);

    if(new_block == NULL){
        new_block = allocatedBlocks.insertBlock(size);
    }
    else if (AllocedBlocksList::data_to_meta(new_block)->size - size >= 128) {
        new_block = allocatedBlocks.SplitAndInsert(size, AllocedBlocksList::data_to_meta(new_block));
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

int main(){
    void *base = sbrk(0);
    MallocMetadata *a = (MallocMetadata *)smalloc(256);
    sfree(a);
    MallocMetadata *b = (MallocMetadata *)smalloc(128);
    MallocMetadata *c = (MallocMetadata *)smalloc(64);
    MallocMetadata *d = (MallocMetadata *)smalloc(64);
    MallocMetadata *e = (MallocMetadata *)smalloc(128);
    MallocMetadata *f = (MallocMetadata *)smalloc(256);
    sfree(d);
    sfree(e);
    sfree(b);
    MallocMetadata *g = (MallocMetadata *)smalloc(128);

    sfree(c);
    sfree(f);
    sfree(g);

    MallocMetadata *h = (MallocMetadata *)smalloc(400);
    MallocMetadata *i = (MallocMetadata *)smalloc(100);
    MallocMetadata *j = (MallocMetadata *)smalloc(100);
    sfree(h);
    MallocMetadata *k = (MallocMetadata *)smalloc(20);

    sfree(i);
    sfree(j);
    sfree(k);

    return 0;
}