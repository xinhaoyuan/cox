#ifndef __KERN_IOSEM_H__
#define __KERN_IOSEM_H__

#include <types.h>
#include <spinlock.h>
#include <algo/list.h>

struct iosem_hash_head_s
{
    spinlock_s   lock;
    list_entry_s hash_list;
};

typedef struct iosem_hash_head_s iosem_hash_head_s;
typedef iosem_hash_head_s       *iosem_hash_head_t;

struct iosem_hash_node_s
{
    list_entry_s hash_node;
    uintptr_t    key;
    uintptr_t    up_count;
    list_entry_s down_queue;
};

typedef struct iosem_hash_node_s iosem_hash_node_s;
typedef iosem_hash_node_s       *iosem_hash_node_t;

struct iosem_hash_s
{
    size_t            size;
    iosem_hash_head_t head;
};

typedef struct iosem_hash_s  iosem_hash_s;
typedef iosem_hash_s        *iosem_hash_t;

void iosem_hash_init(iosem_hash_t hash, size_t size, iosem_hash_head_t head_entry);
int  iosem_del(iosem_hash_t hash, uintptr_t key);
int  iosem_down(iosem_hash_t hash, uintptr_t key, list_entry_t queue_node);
int  iosem_up(iosem_hash_t hash, uintptr_t key, size_t num);

#endif
