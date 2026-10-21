#include <assert.h>
#include <defs.h>
#include <pmm.h>
#include <slub.h>
#include <stdio.h>

/*
 * A deliberately small SLUB-like allocator.
 *
 * Small objects are served from one-page slabs.  Each slab contains a
 * metadata header followed by fixed-size slots.  Large requests bypass the
 * caches and are allocated directly as contiguous pages.
 */

#define SLUB_ALIGN             16UL
#define SLUB_SMALL_MAX         2048UL
#define SLUB_SLAB_MAGIC        0x534C4142UL
#define SLUB_OBJ_MAGIC         0x534C4F42UL
#define SLUB_LARGE_MAGIC       0x534C4152UL

#define SLUB_ALIGN_UP(value) \
    ((((size_t)(value)) + SLUB_ALIGN - 1) & ~(SLUB_ALIGN - 1))

struct slub_obj_hdr {
    uint32_t magic;
    uint32_t reserved;
    struct slub_slab *slab;
};

struct slub_large_hdr {
    uint32_t magic;
    uint32_t reserved;
    size_t pages;
    size_t payload_offset;
};

struct slub_slab {
    uint32_t magic;
    uint32_t total;
    uint32_t used;
    uint32_t reserved;
    struct Page *page;
    struct slub_cache *cache;
    void *free_list;
    struct slub_slab *next;
};

struct slub_cache {
    size_t object_size;
    size_t slot_size;
    struct slub_slab *slabs;
};

static const size_t slub_class_sizes[] = {
    8, 16, 32, 64, 128, 256, 512, 1024, 2048
};

#define SLUB_NR_CLASSES (sizeof(slub_class_sizes) / sizeof(slub_class_sizes[0]))

static struct slub_cache slub_caches[SLUB_NR_CLASSES];
static bool slub_ready;

static void *
slub_page_vaddr(struct Page *page) {
    return (void *)(page2pa(page) + va_pa_offset);
}

static struct slub_cache *
slub_find_cache(size_t size) {
    for (size_t i = 0; i < SLUB_NR_CLASSES; i++) {
        if (size <= slub_caches[i].object_size) {
            return &slub_caches[i];
        }
    }
    return NULL;
}

static struct slub_slab *
slub_create_slab(struct slub_cache *cache) {
    struct Page *page = alloc_pages(1);
    if (page == NULL) {
        return NULL;
    }

    char *base = (char *)slub_page_vaddr(page);
    struct slub_slab *slab = (struct slub_slab *)base;
    size_t data_offset = SLUB_ALIGN_UP(sizeof(struct slub_slab));
    size_t total = (PGSIZE - data_offset) / cache->slot_size;
    if (total == 0) {
        free_pages(page, 1);
        return NULL;
    }

    slab->magic = SLUB_SLAB_MAGIC;
    slab->total = (uint32_t)total;
    slab->used = 0;
    slab->reserved = 0;
    slab->page = page;
    slab->cache = cache;
    slab->next = cache->slabs;
    slab->free_list = NULL;

    for (size_t i = 0; i < total; i++) {
        void *slot = base + data_offset + i * cache->slot_size;
        *(void **)slot = slab->free_list;
        slab->free_list = slot;
    }
    cache->slabs = slab;
    return slab;
}

static struct slub_slab *
slub_find_slab_with_free(struct slub_cache *cache) {
    for (struct slub_slab *slab = cache->slabs; slab != NULL; slab = slab->next) {
        if (slab->free_list != NULL) {
            return slab;
        }
    }
    return slub_create_slab(cache);
}

static void
slub_remove_empty_slab(struct slub_slab *slab) {
    struct slub_cache *cache = slab->cache;
    struct slub_slab **link = &cache->slabs;

    while (*link != NULL && *link != slab) {
        link = &(*link)->next;
    }
    if (*link == slab) {
        *link = slab->next;
        free_pages(slab->page, 1);
    }
}

void
slub_init(void) {
    for (size_t i = 0; i < SLUB_NR_CLASSES; i++) {
        slub_caches[i].object_size = slub_class_sizes[i];
        slub_caches[i].slot_size = SLUB_ALIGN_UP(sizeof(struct slub_obj_hdr) +
                                                 slub_class_sizes[i]);
        slub_caches[i].slabs = NULL;
    }
    slub_ready = 1;
}

void *
slub_alloc(size_t size) {
    if (!slub_ready || size == 0) {
        return NULL;
    }

    struct slub_cache *cache = slub_find_cache(size);
    if (cache != NULL) {
        struct slub_slab *slab = slub_find_slab_with_free(cache);
        if (slab == NULL) {
            return NULL;
        }

        void *slot = slab->free_list;
        slab->free_list = *(void **)slot;
        slab->used++;

        struct slub_obj_hdr *hdr = (struct slub_obj_hdr *)slot;
        hdr->magic = SLUB_OBJ_MAGIC;
        hdr->reserved = 0;
        hdr->slab = slab;
        return (void *)((char *)slot + sizeof(struct slub_obj_hdr));
    }

    size_t payload_offset = SLUB_ALIGN_UP(sizeof(struct slub_large_hdr));
    if (size > ~(size_t)0 - payload_offset) {
        return NULL;
    }
    size_t total_bytes = payload_offset + size;
    if (total_bytes > ~(size_t)0 - (PGSIZE - 1)) {
        return NULL;
    }
    size_t pages = (total_bytes + PGSIZE - 1) / PGSIZE;
    struct Page *page = alloc_pages(pages);
    if (page == NULL) {
        return NULL;
    }

    char *base = (char *)slub_page_vaddr(page);
    struct slub_large_hdr *hdr = (struct slub_large_hdr *)base;
    hdr->magic = SLUB_LARGE_MAGIC;
    hdr->reserved = 0;
    hdr->pages = pages;
    hdr->payload_offset = payload_offset;
    return base + payload_offset;
}

void
slub_free(void *ptr) {
    if (ptr == NULL) {
        return;
    }

    uintptr_t address = (uintptr_t)ptr;
    assert((address & (SLUB_ALIGN - 1)) == 0);

    struct slub_obj_hdr *hdr =
        (struct slub_obj_hdr *)(address - sizeof(struct slub_obj_hdr));
    if (hdr->magic == SLUB_OBJ_MAGIC) {
        struct slub_slab *slab = hdr->slab;
        assert(slab->magic == SLUB_SLAB_MAGIC);
        assert(slab->used > 0);

        *(void **)hdr = slab->free_list;
        slab->free_list = hdr;
        slab->used--;

        if (slab->used == 0) {
            slub_remove_empty_slab(slab);
        }
        return;
    }

    uintptr_t page_base = address & ~((uintptr_t)PGSIZE - 1);
    struct slub_large_hdr *large = (struct slub_large_hdr *)page_base;
    assert(large->magic == SLUB_LARGE_MAGIC);
    assert(large->payload_offset == address - page_base);

    size_t pages = large->pages;
    struct Page *page = pa2page(page_base - va_pa_offset);
    free_pages(page, pages);
}

void
slub_check(void) {
    size_t baseline = nr_free_pages();
    void *ptrs[96];

    assert(slub_alloc(0) == NULL);

    for (size_t i = 0; i < 96; i++) {
        ptrs[i] = slub_alloc(64);
        assert(ptrs[i] != NULL);
        assert(((uintptr_t)ptrs[i] & (SLUB_ALIGN - 1)) == 0);
        for (size_t j = 0; j < i; j++) {
            assert(ptrs[i] != ptrs[j]);
        }
    }
    for (size_t i = 0; i < 96; i++) {
        slub_free(ptrs[i]);
    }
    assert(nr_free_pages() == baseline);

    unsigned char *small = (unsigned char *)slub_alloc(100);
    assert(small != NULL);
    for (size_t i = 0; i < 100; i++) {
        small[i] = (unsigned char)(i + 1);
    }
    for (size_t i = 0; i < 100; i++) {
        assert(small[i] == (unsigned char)(i + 1));
    }

    void *page_sized = slub_alloc(SLUB_SMALL_MAX);
    assert(page_sized != NULL);
    void *large = slub_alloc(5000);
    assert(large != NULL);
    ((unsigned char *)large)[0] = 0x5a;
    ((unsigned char *)large)[4999] = 0xa5;
    assert(((unsigned char *)large)[0] == 0x5a);
    assert(((unsigned char *)large)[4999] == 0xa5);

    void *reused = slub_alloc(100);
    assert(reused != NULL);
    slub_free(reused);
    slub_free(large);
    slub_free(page_sized);
    slub_free(small);
    slub_free(NULL);

    assert(nr_free_pages() == baseline);
    cprintf("[SLUB] self-check: PASS\n");
}
