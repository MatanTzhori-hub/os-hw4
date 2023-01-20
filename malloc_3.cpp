#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <sys/mman.h>

#define MAX_SIZE 100000000
#define LARGE_BLOCK 128 * 1024
#define MIN_SPLIT_SIZE 128
#define DEADBEEF 0xdeadbeef


/***************************************************/
/************* Malloc Functions Headers*************/
void* smalloc(size_t size);
void* scalloc(size_t num, size_t size);
void  sfree(void* p);
void* srealloc(void* oldp, size_t size);
/***************************************************/

class MallocMetadata{
public:
    int cookie;
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
        MallocMetadata* head_large;
        MallocMetadata* wilderness_block;
        int cookie_code;

        AllocedBlocksList();
        ~AllocedBlocksList() = default;
    
        void* insertBlock(size_t size, MallocMetadata* block = nullptr);
        void* allocateFreeBlock(size_t size);
        void releaseBlock(void* ptr);
        void releaseRegularBlock(void* ptr);
        void* SplitAndInsert(size_t new_size, MallocMetadata* old_block);
        void RemoveBlock(MallocMetadata* block);
        MallocMetadata* GetNextIfFree(MallocMetadata* ptr);
        MallocMetadata* GetPrevIfFree(MallocMetadata* ptr);
        void UnionAndInsert(MallocMetadata* curr, MallocMetadata* next, MallocMetadata* prev, bool isfree = 1);
        void* insertLargeBlock(size_t size);
        void releaseLargeBlock(void* ptr);
        void VerifyCookieCode(MallocMetadata* block);
        void* ReallocateRegularBlock(MallocMetadata* block, size_t size);
        void* ReallocateLargeBlock(MallocMetadata* block, size_t size);

        size_t num_free_blocks();
        size_t num_free_bytes();
        size_t num_allocated_blocks();
        size_t num_allocated_bytes();
        size_t num_meta_data_bytes();
        size_t size_meta_data();

        MallocMetadata* data_to_meta(void* p);
        void* meta_to_data(MallocMetadata* p);
};

AllocedBlocksList::AllocedBlocksList() : head(nullptr), head_large(nullptr), wilderness_block(nullptr), cookie_code(rand()){}

MallocMetadata* AllocedBlocksList::data_to_meta(void* p){
    MallocMetadata* meta_data_ptr = (MallocMetadata*)((char*)p - sizeof(MallocMetadata));
    VerifyCookieCode(meta_data_ptr);
    return meta_data_ptr;
}

void* AllocedBlocksList::meta_to_data(MallocMetadata* p){
    VerifyCookieCode(p);
    return (char*)p + sizeof(MallocMetadata);
}

void AllocedBlocksList::VerifyCookieCode(MallocMetadata* block){
    if(block == NULL){
        return;
    }
    if(block->cookie != cookie_code){
        exit(DEADBEEF);
    }
}

void* AllocedBlocksList::insertBlock(size_t size, MallocMetadata* new_block)
{   
    if (new_block == nullptr) {
        if (wilderness_block != nullptr && wilderness_block->is_free) {
            RemoveBlock(wilderness_block);
            sbrk(size - wilderness_block->size);
            wilderness_block->size += size - wilderness_block->size;
            new_block = wilderness_block;
        } else {
            new_block = (MallocMetadata*)sbrk(size_meta_data() + size);
            if(new_block == (void*)-1) {
                return NULL;
            }
            wilderness_block = new_block;
        }
    }
    VerifyCookieCode(head);

    new_block->cookie = this->cookie_code;
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
        VerifyCookieCode(temp->next);
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
        VerifyCookieCode(temp->next);
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
        VerifyCookieCode(temp);
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
    VerifyCookieCode(ptr);
    MallocMetadata* curr = this->head;
    while (curr != NULL) {
        VerifyCookieCode(curr);
        if((char*)curr == (char*)ptr + ptr->size + size_meta_data()) {
            return curr->is_free ? curr : nullptr;
        }
        curr = curr->next;
    }
    return nullptr;
}

MallocMetadata* AllocedBlocksList::GetPrevIfFree(MallocMetadata* ptr) {
    // Find curr + curr_size + metadata_size == ptr
    VerifyCookieCode(ptr);
    MallocMetadata* curr = this->head;
    while (curr != NULL) {
        VerifyCookieCode(curr);
        if((char*)curr + curr->size + size_meta_data() == (char*)ptr) {
            return curr->is_free ? curr : nullptr;
        }
        curr = curr->next;
    }
    return nullptr;
}

void AllocedBlocksList::UnionAndInsert(MallocMetadata* curr, MallocMetadata* next, MallocMetadata* prev, bool isfree) {
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
        case 1: // Union curr and next
            insertBlock(curr->size + next->size + size_meta_data(), curr);
            curr->is_free = isfree;
            if (next == wilderness_block) {
                wilderness_block = curr;
            }
            break;
        case 2: // Union curr and prev
            insertBlock(curr->size + prev->size + size_meta_data(), prev);
            prev->is_free = isfree;
            if (curr == wilderness_block) {
                wilderness_block = prev;
            }
            break;
        case 3: // Union curr, next and prev
            insertBlock(prev->size + curr->size + next->size + 2 * size_meta_data(), prev);
            prev->is_free = isfree;
            if (next == wilderness_block) {
                wilderness_block = prev;
            }
            break;
    }

    return;
}

void AllocedBlocksList::releaseBlock(void* ptr){
    if(ptr == NULL){
        return;
    }
    MallocMetadata* meta_data_ptr = data_to_meta(ptr);
    if(meta_data_ptr->size >= LARGE_BLOCK){
        releaseLargeBlock(ptr);
    }
    else{
        releaseRegularBlock(ptr);
    }
}

void AllocedBlocksList::releaseRegularBlock(void* ptr){
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
    VerifyCookieCode(block);
    if (block->prev == NULL) {
        head = block->next;
        if(block->next != NULL){
        head->prev = NULL;
        }

    } else {
        VerifyCookieCode(block->prev);
        VerifyCookieCode(block->next);
        block->prev->next = block->next;
        if(block->next!=NULL){
            block->next->prev = block->prev;
        }
    }

    block->next = NULL;
    block->prev = NULL;
}


void* AllocedBlocksList::SplitAndInsert(size_t new_size, MallocMetadata* old_block) {
    VerifyCookieCode(old_block);
    RemoveBlock(old_block);
    void* free_block = insertBlock(old_block->size - new_size - size_meta_data(), (MallocMetadata*)((char*)old_block + new_size + size_meta_data()));
    if (old_block == wilderness_block) {
        wilderness_block = data_to_meta(free_block);
    }
    data_to_meta(free_block)->is_free = 1;

    MallocMetadata* next_free = GetNextIfFree(data_to_meta(free_block));
    if(next_free != NULL){
        UnionAndInsert(data_to_meta(free_block), next_free, NULL);
    }
    insertBlock(new_size, old_block);
    return meta_to_data(old_block);
}

void* AllocedBlocksList::insertLargeBlock(size_t size) {
    MallocMetadata* new_large_block = (MallocMetadata*)mmap(NULL ,sizeof(*new_large_block) + size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS , -1, 0);
    if(new_large_block == (void*)-1){
        return NULL;
    }
    VerifyCookieCode(head_large);

    new_large_block->cookie = this->cookie_code;
    new_large_block->is_free = 0;
    new_large_block->size = size;
    new_large_block->next = NULL;
    new_large_block->prev = NULL;

    if (head_large == NULL) {
        head_large = new_large_block;
        return meta_to_data(head_large);
    }
 
    MallocMetadata* temp = head_large;
    while (temp->next != NULL) {
        VerifyCookieCode(temp);
        temp = temp->next;
    }
    
    new_large_block->prev = temp;
    temp->next = new_large_block;
    return meta_to_data(new_large_block);
}

void AllocedBlocksList::releaseLargeBlock(void* ptr){
    MallocMetadata* meta_data_ptr = data_to_meta(ptr);
    VerifyCookieCode(head_large);

    if (meta_data_ptr->prev == NULL) {
        head_large = meta_data_ptr->next;
        if(meta_data_ptr->next != NULL){
        head_large->prev = NULL;
        }

    } else { 
        VerifyCookieCode(meta_data_ptr->next);
        VerifyCookieCode(meta_data_ptr->prev);
        meta_data_ptr->prev->next = meta_data_ptr->next;
        if(meta_data_ptr->next!=NULL){
            meta_data_ptr->next->prev = meta_data_ptr->prev;
        }
    }

    meta_data_ptr->next = NULL;
    meta_data_ptr->prev = NULL;

    munmap(meta_data_ptr, meta_data_ptr->size + size_meta_data());
}

void* AllocedBlocksList::ReallocateRegularBlock(MallocMetadata* block, size_t size){
    if(block->size >= size){ // 1.a
        if(block->size - size >= (MIN_SPLIT_SIZE + size_meta_data())){
            SplitAndInsert(size, block);
        }
        return meta_to_data(block);
    }

    MallocMetadata* next = GetNextIfFree(block);
    MallocMetadata* prev = GetPrevIfFree(block);

    if(prev != NULL){ // 1.b
        if(block->size + prev->size + size_meta_data() >= size){ // 1.b no wilderness
            RemoveBlock(block);
            RemoveBlock(prev);
            insertBlock(block->size + prev->size + size_meta_data(), prev);
            if(block == wilderness_block){
                wilderness_block = prev;
            }
            if(prev->size - size >= (MIN_SPLIT_SIZE + size_meta_data())){
                SplitAndInsert(size, prev);
                // After splitting, we check if we need to update the wilderness_block (that we change to prev in the last if)
                if(prev == wilderness_block){
                    wilderness_block = (MallocMetadata*)((char*)prev + size_meta_data() + size);
                }
            }
            memmove(meta_to_data(prev), meta_to_data(block), block->size);
            return meta_to_data(prev);
        }
        else if(block == wilderness_block){ // 1.b with wilderness
            RemoveBlock(block);
            RemoveBlock(prev);
            insertBlock(block->size + prev->size + size_meta_data(), prev);
            wilderness_block = prev;
            sbrk(size - wilderness_block->size);
            wilderness_block->size += size - wilderness_block->size;
            memmove(meta_to_data(prev), meta_to_data(block), block->size);
            return meta_to_data(prev);
        }
    }
    else if(block == wilderness_block){  // 1.c
        sbrk(size - wilderness_block->size);
        wilderness_block->size += size - wilderness_block->size;
        return meta_to_data(wilderness_block);
    }

    if(next != NULL){ // 1.d
        if(block->size + next->size + size_meta_data() >= size){
            RemoveBlock(block);
            RemoveBlock(next);
            insertBlock(block->size + next->size + size_meta_data(), block);
            if(next == wilderness_block){
                wilderness_block = block;
            }
            if(block->size - size >= (MIN_SPLIT_SIZE + size_meta_data())){
                SplitAndInsert(size, block);
                // After splitting, we check if we need to update the wilderness_block (that we change to prev in the last if)
                if(block == wilderness_block){
                    wilderness_block = (MallocMetadata*)((char*)block + size_meta_data() + size);
                }
            }
            return meta_to_data(block);
        }
    }

    if(prev != NULL && next != NULL){ // 1.e
        if(block->size + prev->size + next->size + 2*size_meta_data() >= size){
            UnionAndInsert(block, next, prev, 0);
            memmove(meta_to_data(prev), meta_to_data(block), block->size);
            if(prev->size - size >= (MIN_SPLIT_SIZE + size_meta_data())){
                SplitAndInsert(size, prev);
                // After splitting, we check if we need to update the wilderness_block (that we change to prev in the last if)
                if(prev == wilderness_block){
                    wilderness_block = (MallocMetadata*)((char*)prev + size_meta_data() + size);
                }
            }
            return meta_to_data(prev);
        }
    }
    
    if(next != NULL && next == wilderness_block){ // 1.f
        if(prev != NULL){ // 1.f1
            UnionAndInsert(block, next, prev, 0);
            memmove(meta_to_data(prev), meta_to_data(block), block->size);

            wilderness_block = prev;
            sbrk(size - wilderness_block->size);
            wilderness_block->size += size - wilderness_block->size;
            return meta_to_data(wilderness_block);
        }
        else{ // 1.f2
            UnionAndInsert(block, next, NULL, 0);

            wilderness_block = block;
            sbrk(size - wilderness_block->size);
            wilderness_block->size += size - wilderness_block->size;
            return meta_to_data(wilderness_block);
        }
    }

    void* new_block = smalloc(size); // g + h
    if(new_block == NULL){
        return NULL;
    }

    memmove(new_block, meta_to_data(block), block->size);
    sfree(meta_to_data(block));

    return new_block;
}


void* AllocedBlocksList::ReallocateLargeBlock(MallocMetadata* oldblock, size_t size){
    VerifyCookieCode(oldblock);
    if(oldblock->size == size){
        return meta_to_data(oldblock);
    }

    void* new_block = insertLargeBlock(size);
    if(new_block == NULL){
        return NULL;
    }

    memmove(new_block, oldblock, size);
    releaseLargeBlock(meta_to_data(oldblock));

    return new_block;
}

size_t AllocedBlocksList::num_free_blocks() {
    size_t free_blocks = 0;

    MallocMetadata* temp = this->head;
    while (temp != NULL) {
        VerifyCookieCode(temp);
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
        VerifyCookieCode(temp);
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
        VerifyCookieCode(temp);
        alloced_blocks++;
        temp = temp->next;
    }

    temp = this->head_large;
    while (temp != NULL) {
        VerifyCookieCode(temp);
        alloced_blocks++;
        temp = temp->next;
    }
    
    return alloced_blocks;
}

size_t AllocedBlocksList::num_allocated_bytes() {
    size_t alloced_bytes = 0;

    MallocMetadata* temp = this->head;
    while (temp != NULL) {
        VerifyCookieCode(temp);
        alloced_bytes += temp->size;
        temp = temp->next;
    }

    temp = this->head_large;
    while (temp != NULL) {
        VerifyCookieCode(temp);
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

    if (size >= LARGE_BLOCK) {
        return allocatedBlocks.insertLargeBlock(size);
    }

    void* new_block = allocatedBlocks.allocateFreeBlock(size);

    if(new_block == NULL){
        new_block = allocatedBlocks.insertBlock(size);
    }
    else if (allocatedBlocks.data_to_meta(new_block)->size - size >= (MIN_SPLIT_SIZE + allocatedBlocks.size_meta_data())) {
        new_block = allocatedBlocks.SplitAndInsert(size, allocatedBlocks.data_to_meta(new_block));
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

    MallocMetadata* meta_data_ptr = allocatedBlocks.data_to_meta(oldp);
    if(meta_data_ptr->size >= LARGE_BLOCK){
        return allocatedBlocks.ReallocateLargeBlock(meta_data_ptr, size);
    }
    else{
        return allocatedBlocks.ReallocateRegularBlock(meta_data_ptr, size);
    }
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
