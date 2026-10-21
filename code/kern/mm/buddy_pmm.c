#include <assert.h>
#include <buddy_pmm.h>
#include <list.h>
#include <pmm.h>

/*
 * A free block of order k contains 2^k pages and starts on a 2^k-page
 * boundary.  As in the other PMMs, property records the number of pages in
 * the block.  Two extra flag bits distinguish an allocated block and its
 * head from a free block.
 */
#define BUDDY_MAX_ORDER       20
#define BUDDY_NR_ORDERS       (BUDDY_MAX_ORDER + 1)
#define BUDDY_PG_ALLOCATED    2
#define BUDDY_PG_ALLOC_HEAD   3

#define BUDDY_ALLOCATED(page) \
    (((page)->flags & (1UL << BUDDY_PG_ALLOCATED)) != 0)
#define BUDDY_ALLOC_HEAD(page) \
    (((page)->flags & (1UL << BUDDY_PG_ALLOC_HEAD)) != 0)
#define SetBuddyAllocated(page) \
    ((page)->flags |= (1UL << BUDDY_PG_ALLOCATED))
#define SetBuddyAllocHead(page) \
    ((page)->flags |= (1UL << BUDDY_PG_ALLOC_HEAD))

static free_area_t buddy_free_area[BUDDY_NR_ORDERS];
static size_t buddy_nr_free;
static size_t buddy_total_pages;
static bool buddy_initialized;

static size_t
buddy_block_size(unsigned int order) {
    assert(order <= BUDDY_MAX_ORDER);
    return (size_t)1 << order;
}

static unsigned int
buddy_floor_log2(size_t value) {
    unsigned int order = 0;

    assert(value > 0);
    while (value >>= 1) {
        order++;
    }
    return order;
}

static bool
buddy_is_power_of_two(size_t value) {
    return value != 0 && (value & (value - 1)) == 0;
}

/*
 * Return the smallest power of two not smaller than n.  Zero denotes
 * overflow, a case which alloc_pages treats as an allocation failure.
 */
static size_t
buddy_roundup_pow2(size_t n) {
    size_t size = 1;

    if (n == 0) {
        return 0;
    }
    while (size < n) {
        if (size > (~(size_t)0 >> 1)) {
            return 0;
        }
        size <<= 1;
    }
    return size;
}

static size_t
buddy_managed_pages(void) {
    if (pages == NULL || npage <= nbase) {
        return 0;
    }
    return npage - nbase;
}

static bool
buddy_page_in_range(struct Page *page) {
    uintptr_t offset;
    size_t index;
    size_t total = buddy_managed_pages();

    if (page == NULL || total == 0) {
        return 0;
    }
    offset = (uintptr_t)page - (uintptr_t)pages;
    if (offset % sizeof(struct Page) != 0) {
        return 0;
    }
    index = offset / sizeof(struct Page);
    return index < total;
}

static bool
buddy_block_in_range(struct Page *base, size_t pages_in_block) {
    uintptr_t offset;
    size_t index;
    size_t total = buddy_managed_pages();

    if (base == NULL || pages_in_block == 0 || total == 0) {
        return 0;
    }
    offset = (uintptr_t)base - (uintptr_t)pages;
    if (offset % sizeof(struct Page) != 0) {
        return 0;
    }
    index = offset / sizeof(struct Page);
    return index < total && pages_in_block <= total - index;
}

static bool
buddy_block_aligned(struct Page *base, unsigned int order) {
    size_t alignment = buddy_block_size(order);

    return (page2ppn(base) & (alignment - 1)) == 0;
}

static void
buddy_list_insert(struct Page *base, unsigned int order) {
    size_t size = buddy_block_size(order);

    assert(buddy_page_in_range(base));
    assert(buddy_block_in_range(base, size));
    assert(buddy_block_aligned(base, order));
    assert(!PageReserved(base));
    assert(!BUDDY_ALLOCATED(base));
    assert(!PageProperty(base));
    assert(base->property == 0);
    assert(base->ref == 0);

    base->property = (unsigned int)size;
    SetPageProperty(base);
    list_add(&(buddy_free_area[order].free_list), &(base->page_link));
    buddy_free_area[order].nr_free += (unsigned int)size;
}

static void
buddy_list_remove(struct Page *base, unsigned int order) {
    size_t size = buddy_block_size(order);

    assert(buddy_page_in_range(base));
    assert(!PageReserved(base));
    assert(!BUDDY_ALLOCATED(base));
    assert(PageProperty(base));
    assert(base->property == size);
    assert(buddy_free_area[order].nr_free >= size);

    list_del_init(&(base->page_link));
    ClearPageProperty(base);
    base->property = 0;
    buddy_free_area[order].nr_free -= (unsigned int)size;
}

static struct Page *
buddy_alloc_from_order(unsigned int request_order) {
    struct Page *page = NULL;
    unsigned int order;

    assert(request_order <= BUDDY_MAX_ORDER);
    for (order = request_order; order <= BUDDY_MAX_ORDER; order++) {
        list_entry_t *head = &(buddy_free_area[order].free_list);

        if (!list_empty(head)) {
            page = le2page(list_next(head), page_link);
            assert(buddy_page_in_range(page));
            assert(buddy_block_aligned(page, order));
            assert(page->property == buddy_block_size(order));
            buddy_list_remove(page, order);
            break;
        }
    }

    if (page == NULL) {
        return NULL;
    }

    /*
     * Keep the lower half as the allocation and put each upper half back on
     * the appropriate order list.
     */
    while (order > request_order) {
        struct Page *buddy;

        order--;
        buddy = page + buddy_block_size(order);
        buddy_list_insert(buddy, order);
    }

    size_t alloc_size = buddy_block_size(request_order);
    for (size_t i = 0; i < alloc_size; i++) {
        struct Page *p = page + i;

        p->flags = 0;
        SetBuddyAllocated(p);
        p->property = (unsigned int)alloc_size;
        set_page_ref(p, 0);
        list_init(&(p->page_link));
    }
    SetBuddyAllocHead(page);
    return page;
}

static void
buddy_init(void) {
    for (unsigned int order = 0; order <= BUDDY_MAX_ORDER; order++) {
        list_init(&(buddy_free_area[order].free_list));
        buddy_free_area[order].nr_free = 0;
    }
    buddy_nr_free = 0;
    buddy_total_pages = 0;
    buddy_initialized = 0;
}

static void
buddy_init_memmap(struct Page *base, size_t n) {
    struct Page *p;
    size_t remaining;

    assert(pages != NULL);
    assert(n > 0);
    assert(buddy_block_in_range(base, n));

    /*
     * Validate the complete range before changing any page descriptor so a
     * bad call cannot leave a partially initialized allocator behind.
     */
    for (p = base; p < base + n; p++) {
        assert(PageReserved(p));
        assert(p->flags == (1UL << PG_reserved));
        assert(p->property == 0);
        assert(p->ref == 0);
    }

    for (p = base; p < base + n; p++) {
        p->flags = 0;
        p->property = 0;
        set_page_ref(p, 0);
        list_init(&(p->page_link));
    }

    /*
     * Decompose the possibly unaligned range into maximal aligned buddy
     * blocks.  The first block is aligned even when page_init does not hand
     * over an address aligned to a large power of two.
     */
    p = base;
    remaining = n;
    while (remaining > 0) {
        unsigned int order = buddy_floor_log2(remaining);

        if (order > BUDDY_MAX_ORDER) {
            order = BUDDY_MAX_ORDER;
        }
        while (order > 0 && !buddy_block_aligned(p, order)) {
            order--;
        }

        size_t block_pages = buddy_block_size(order);
        buddy_list_insert(p, order);
        p += block_pages;
        remaining -= block_pages;
    }

    buddy_total_pages += n;
    buddy_nr_free += n;
    buddy_initialized = 1;
}

static struct Page *
buddy_alloc_pages(size_t n) {
    size_t rounded;
    unsigned int order;

    if (!buddy_initialized || n == 0) {
        return NULL;
    }
    rounded = buddy_roundup_pow2(n);
    if (rounded == 0 || rounded > buddy_nr_free) {
        return NULL;
    }
    order = buddy_floor_log2(rounded);
    if (order > BUDDY_MAX_ORDER) {
        return NULL;
    }

    struct Page *page = buddy_alloc_from_order(order);
    if (page != NULL) {
        assert(buddy_nr_free >= rounded);
        buddy_nr_free -= rounded;
    }
    return page;
}

static void
buddy_free_pages(struct Page *base, size_t n) {
    unsigned int order;
    size_t block_pages;
    size_t rounded;

    assert(buddy_initialized);
    assert(n > 0);
    assert(buddy_page_in_range(base));
    assert(!PageReserved(base));
    assert(BUDDY_ALLOCATED(base));
    assert(BUDDY_ALLOC_HEAD(base));
    assert(!PageProperty(base));

    assert(buddy_is_power_of_two(base->property));
    block_pages = base->property;
    order = buddy_floor_log2(block_pages);
    assert(order <= BUDDY_MAX_ORDER);
    assert(buddy_block_in_range(base, block_pages));
    assert(buddy_block_aligned(base, order));

    /*
     * alloc_pages rounds a request up to a buddy block.  Accept every n
     * which maps to that same block, including the original non-power-of-two
     * request such as alloc_pages(5) followed by free_pages(base, 5).
     */
    rounded = buddy_roundup_pow2(n);
    assert(rounded == block_pages);

    /*
     * The allocation head is the only valid release address.  Validate all
     * descriptors in the block before modifying any of them.
     */
    for (size_t i = 0; i < block_pages; i++) {
        struct Page *p = base + i;

        assert(!PageReserved(p));
        assert(BUDDY_ALLOCATED(p));
        assert(!PageProperty(p));
        assert(p->property == block_pages);
        assert(p->ref == 0);
        assert(BUDDY_ALLOC_HEAD(p) == (i == 0));
    }

    for (size_t i = 0; i < block_pages; i++) {
        struct Page *p = base + i;

        p->flags = 0;
        p->property = 0;
        set_page_ref(p, 0);
        list_init(&(p->page_link));
    }
    buddy_nr_free += block_pages;

    /*
     * Merge with the buddy at each order.  Both blocks are guaranteed to
     * have the same order and alignment before they are coalesced.
     */
    while (order < BUDDY_MAX_ORDER) {
        size_t buddy_index;
        struct Page *buddy;
        size_t block_index = page2ppn(base) - nbase;

        block_pages = buddy_block_size(order);
        buddy_index = block_index ^ block_pages;
        if (buddy_index >= buddy_managed_pages()) {
            break;
        }
        buddy = pages + buddy_index;
        if (!PageProperty(buddy) || PageReserved(buddy) ||
            BUDDY_ALLOCATED(buddy) || buddy->property != block_pages) {
            break;
        }
        assert(buddy_block_aligned(buddy, order));

        buddy_list_remove(buddy, order);
        if (buddy < base) {
            base = buddy;
        }
        order++;
    }

    buddy_list_insert(base, order);
    assert(buddy_nr_free <= buddy_total_pages);
}

static size_t
buddy_nr_free_pages(void) {
    return buddy_nr_free;
}

static void
buddy_check_free_lists(void) {
    size_t sum = 0;

    assert(buddy_initialized);
    assert(buddy_total_pages > 0);
    assert(buddy_nr_free <= buddy_total_pages);

    for (unsigned int order = 0; order <= BUDDY_MAX_ORDER; order++) {
        free_area_t *area = &(buddy_free_area[order]);
        list_entry_t *head = &(area->free_list);
        list_entry_t *le = head;
        size_t area_sum = 0;
        size_t entries = 0;
        size_t block_pages = buddy_block_size(order);

        while ((le = list_next(le)) != head) {
            struct Page *page = le2page(le, page_link);

            assert(++entries <= buddy_total_pages);
            assert(buddy_page_in_range(page));
            assert(buddy_block_in_range(page, block_pages));
            assert(buddy_block_aligned(page, order));
            assert(!PageReserved(page));
            assert(!BUDDY_ALLOCATED(page));
            assert(PageProperty(page));
            assert(page->property == block_pages);
            assert(page->ref == 0);

            /*
             * If both a block and its same-order buddy are free, they must
             * have been coalesced.  Finding such a pair means the free path
             * left mergeable blocks in the lists.
             */
            if (order < BUDDY_MAX_ORDER) {
                size_t block_index = page2ppn(page) - nbase;
                size_t buddy_index = block_index ^ block_pages;

                if (buddy_index < buddy_managed_pages()) {
                    struct Page *buddy = pages + buddy_index;

                    assert(!PageProperty(buddy) ||
                           PageReserved(buddy) ||
                           BUDDY_ALLOCATED(buddy) ||
                           buddy->property != block_pages);
                }
            }

            /*
             * Interior descriptors of a free block must not retain allocator
             * metadata.  This catches split/merge bookkeeping mistakes.
             */
            for (size_t i = 1; i < block_pages; i++) {
                struct Page *p = page + i;

                assert(!PageReserved(p));
                assert(!BUDDY_ALLOCATED(p));
                assert(!PageProperty(p));
                assert(p->property == 0);
                assert(p->ref == 0);
            }

            area_sum += block_pages;
        }

        assert(area->nr_free == (unsigned int)area_sum);
        sum += area_sum;
    }

    assert(sum == buddy_nr_free);
    assert(nr_free_pages() == buddy_nr_free);
}

static void
buddy_check_drain_free_pages(void) {
    size_t allocated = 0;

    while (alloc_page() != NULL) {
        assert(++allocated <= buddy_total_pages);
    }
    assert(allocated > 0);
    assert(nr_free_pages() == 0);
}

/*
 * Once all free pages are allocated, find four consecutive single-page
 * allocations whose base is aligned to an order-2 block.  The middle two
 * pages are adjacent but are not buddies of each other.
 */
static struct Page *
buddy_check_find_aligned_run(void) {
    size_t total = buddy_managed_pages();

    for (struct Page *p = pages; p + 3 < pages + total; p++) {
        if ((page2ppn(p) & 3) != 0) {
            continue;
        }
        if (BUDDY_ALLOC_HEAD(p) && p->property == 1 &&
            BUDDY_ALLOC_HEAD(p + 1) && p[1].property == 1 &&
            BUDDY_ALLOC_HEAD(p + 2) && p[2].property == 1 &&
            BUDDY_ALLOC_HEAD(p + 3) && p[3].property == 1) {
            return p;
        }
    }
    assert(0);
    return NULL;
}

/*
 * Release every allocation made by the self-check.  Allocation heads carry
 * their block size, so the check does not need a large external pointer
 * array.
 */
static void
buddy_check_release_all(void) {
    size_t total = buddy_managed_pages();

    for (struct Page *p = pages; p < pages + total; p++) {
        if (BUDDY_ALLOC_HEAD(p)) {
            size_t block_pages = p->property;
            unsigned int order;

            assert(buddy_is_power_of_two(block_pages));
            order = buddy_floor_log2(block_pages);
            assert(order <= BUDDY_MAX_ORDER);
            assert(block_pages == buddy_block_size(order));
            buddy_free_pages(p, block_pages);
        }
    }
}

static void
buddy_check(void) {
    size_t original_free;
    struct Page *p0;
    struct Page *p1;
    struct Page *p2;
    struct Page *p3;

    buddy_check_free_lists();
    original_free = nr_free_pages();
    assert(original_free == buddy_total_pages);
    assert(alloc_pages(0) == NULL);
    assert(alloc_pages(original_free + 1) == NULL);
    assert(nr_free_pages() == original_free);

    /*
     * A five-page request must consume an eight-page, eight-page-aligned
     * block.  Releasing it with the original request size must return the
     * complete block.
     */
    assert((p0 = alloc_pages(5)) != NULL);
    assert(BUDDY_ALLOCATED(p0) && BUDDY_ALLOC_HEAD(p0));
    assert(!PageProperty(p0));
    assert(p0->property == 8);
    assert(buddy_block_aligned(p0, 3));
    assert(nr_free_pages() == original_free - 8);
    free_pages(p0, 5);
    assert(nr_free_pages() == original_free);
    buddy_check_free_lists();

    /*
     * Exhaust the allocator to make a controlled fragmented layout.  Freeing
     * p1 and p2 leaves two adjacent but non-buddy order-0 blocks.  Their two
     * pages are therefore enough to satisfy the page count of alloc_pages(2),
     * but no order-1 block exists.
     */
    buddy_check_drain_free_pages();
    p0 = buddy_check_find_aligned_run();
    p1 = p0 + 1;
    p2 = p0 + 2;
    p3 = p0 + 3;

    free_pages(p1, 1);
    free_pages(p2, 1);
    assert(nr_free_pages() == 2);
    assert(PageProperty(p1) && p1->property == 1);
    assert(PageProperty(p2) && p2->property == 1);
    assert(buddy_free_area[0].nr_free == 2);
    for (unsigned int order = 1; order <= BUDDY_MAX_ORDER; order++) {
        assert(list_empty(&(buddy_free_area[order].free_list)));
        assert(buddy_free_area[order].nr_free == 0);
    }

    assert(alloc_pages(2) == NULL);
    assert(nr_free_pages() == 2);

    /*
     * The most recently freed block is at the head of the order-0 list, so
     * a one-page request must return exactly that page.
     */
    assert((p2 = alloc_page()) != NULL);
    assert(p2 == p0 + 2);
    assert(nr_free_pages() == 1);
    free_pages(p2, 1);
    assert(nr_free_pages() == 2);

    /*
     * Releasing the outer pages must merge (p0,p1), (p2,p3), and finally the
     * two order-1 buddies into one aligned order-2 block.
     */
    free_pages(p0, 1);
    assert(nr_free_pages() == 3);
    free_pages(p3, 1);
    assert(nr_free_pages() == 4);
    assert(PageProperty(p0) && p0->property == 4);
    assert(buddy_free_area[2].nr_free == 4);
    assert(buddy_free_area[0].nr_free == 0);
    assert(buddy_free_area[1].nr_free == 0);

    buddy_check_release_all();
    assert(nr_free_pages() == original_free);
    buddy_check_free_lists();
    assert(nr_free_pages() == original_free);
}

const struct pmm_manager buddy_pmm_manager = {
    .name = "buddy_pmm_manager",
    .init = buddy_init,
    .init_memmap = buddy_init_memmap,
    .alloc_pages = buddy_alloc_pages,
    .free_pages = buddy_free_pages,
    .nr_free_pages = buddy_nr_free_pages,
    .check = buddy_check,
};
