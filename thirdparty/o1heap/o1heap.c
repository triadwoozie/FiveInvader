// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
// documentation files (the "Software"), to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
// and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or substantial portions
// of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
// WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS
// OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
// Copyright (c) 2020 Pavel Kirienko
// Authors: Pavel Kirienko <pavel.kirienko@zubax.com>

#include "o1heap.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

// ---------------------------------------- BUILD CONFIGURATION OPTIONS ----------------------------------------

/// Define this macro to include build configuration header. This is an alternative to the -D compiler flag.
/// Usage example with CMake: "-DO1HEAP_CONFIG_HEADER=\"${CMAKE_CURRENT_SOURCE_DIR}/my_o1heap_config.h\""
#ifdef O1HEAP_CONFIG_HEADER
#    include O1HEAP_CONFIG_HEADER
#endif

/// The assertion macro defaults to the standard assert().
/// It can be overridden to manually suppress assertion checks or use a different error handling policy.
#ifndef O1HEAP_ASSERT
// Intentional violation of MISRA: the assertion check macro cannot be replaced with a function definition.
#    define O1HEAP_ASSERT(x) assert(x)  // NOSONAR
#endif

/// Allow usage of compiler intrinsics for branch annotation and CLZ.
#ifndef O1HEAP_USE_INTRINSICS
#    define O1HEAP_USE_INTRINSICS 1
#endif

/// Branch probability annotations are used to improve the worst case execution time (WCET). They are entirely optional.
#if O1HEAP_USE_INTRINSICS && !defined(O1HEAP_LIKELY)
#    if defined(__GNUC__) || defined(__clang__) || defined(__CC_ARM)
// Intentional violation of MISRA: branch hinting macro cannot be replaced with a function definition.
#        define O1HEAP_LIKELY(x) __builtin_expect((x), 1)  // NOSONAR
#        define O1HEAP_UNLIKELY(x) __builtin_expect((x), 0)  // NOSONAR
#    endif
#endif
#ifndef O1HEAP_LIKELY
#    define O1HEAP_LIKELY(x) x
#endif
#ifndef O1HEAP_UNLIKELY
#    define O1HEAP_UNLIKELY(x) x
#endif

/// This option is used for testing only. Do not use in production.
#ifndef O1HEAP_PRIVATE
#    define O1HEAP_PRIVATE static inline
#endif

/// Count leading zeros (CLZ) is used for fast computation of binary logarithm (which needs to be done very often).
/// Most of the modern processors (including the embedded ones) implement dedicated hardware support for fast CLZ
/// computation, which is available via compiler intrinsics. The default implementation will automatically use
/// the intrinsics for some of the compilers; for others it will default to the slow software emulation,
/// which can be overridden by the user via O1HEAP_CONFIG_HEADER. The library guarantees that the argument is positive.
#if O1HEAP_USE_INTRINSICS && !defined(O1HEAP_CLZ)
#    if defined(__GNUC__) || defined(__clang__) || defined(__CC_ARM)
#        if SIZE_MAX == 0xFFFFFFFFFFFFFFFF
#            define O1HEAP_CLZ __builtin_clzll
#        else
#            define O1HEAP_CLZ __builtin_clzl
#        endif
#    endif
#endif
#ifndef O1HEAP_CLZ
O1HEAP_PRIVATE uint_fast8_t O1HEAP_CLZ(const size_t x)
{
    O1HEAP_ASSERT(x > 0);
    size_t       t = ((size_t)1U) << ((sizeof(size_t) * CHAR_BIT) - 1U);
    uint_fast8_t r = 0;
    while ((x & t) == 0)
    {
        t >>= 1U;
        r++;
    }
    return r;
}
#endif

// ---------------------------------------- INTERNAL DEFINITIONS ----------------------------------------

#if __STDC_VERSION__ < 201112L
// Intentional violation of MISRA: static assertion macro cannot be replaced with a function definition.
#    define static_assert(x, ...) typedef char _static_assert_gl(_static_assertion_, __LINE__)[(x) ? 1 : -1]  // NOSONAR
#    define _static_assert_gl(a, b) _static_assert_gl_impl(a, b)                                              // NOSONAR
// Intentional violation of MISRA: the paste operator ## cannot be avoided in this context.
#    define _static_assert_gl_impl(a, b) a##b  // NOSONAR
#endif

/// The overhead is at most O1HEAP_ALIGNMENT bytes large,
/// then follows the user data which shall keep the next fragment aligned.
#define FRAGMENT_SIZE_MIN (O1HEAP_ALIGNMENT * 2U)

/// This is risky, handle with care: if the allocation amount plus per-fragment overhead exceeds 2**(b-1),
/// where b is the pointer bit width, then ceil(log2(amount)) yields b; then 2**b causes an integer overflow.
/// To avoid this, we put a hard limit on fragment size (which is amount + per-fragment overhead): 2**(b-1)
#define FRAGMENT_SIZE_MAX ((SIZE_MAX >> 1U) + 1U)

/// Normally we should subtract log2(FRAGMENT_SIZE_MIN) but log2 is bulky to compute using the preprocessor only.
/// We will certainly end up with unused bins this way, but it is cheap to ignore.
#define NUM_BINS_MAX (sizeof(size_t) * CHAR_BIT)

static_assert((O1HEAP_ALIGNMENT& (O1HEAP_ALIGNMENT - 1U)) == 0U, "Not a power of 2");
static_assert((FRAGMENT_SIZE_MIN& (FRAGMENT_SIZE_MIN - 1U)) == 0U, "Not a power of 2");
static_assert((FRAGMENT_SIZE_MAX& (FRAGMENT_SIZE_MAX - 1U)) == 0U, "Not a power of 2");

typedef struct Fragment Fragment;

typedef struct FragmentHeader
{
    Fragment* next;
    Fragment* prev;
    size_t    size;
    bool      used;
} FragmentHeader;
static_assert(sizeof(FragmentHeader) <= O1HEAP_ALIGNMENT, "Memory layout error");

struct Fragment
{
    FragmentHeader header;
    // Everything past the header may spill over into the allocatable space. The header survives across alloc/free.
    Fragment* next_free;  // Next free fragment in the bin; NULL in the last one.
    Fragment* prev_free;  // Same but points back; NULL in the first one.
};
static_assert(sizeof(Fragment) <= FRAGMENT_SIZE_MIN, "Memory layout error");

struct O1HeapInstance
{
    void*     arena_base;
    Fragment* bins[NUM_BINS_MAX];  ///< Smallest fragments are in the bin at index 0.
    size_t    nonempty_bin_mask;   ///< Bit 1 represents a non-empty bin; bin at index 0 is for the smallest fragments.

    O1HeapDiagnostics diagnostics;
};

/// The amount of space allocated for the heap instance.
/// Its size is padded up to O1HEAP_ALIGNMENT to ensure correct alignment of the allocation arena that follows.
#define INSTANCE_SIZE_PADDED ((sizeof(O1HeapInstance) + O1HEAP_ALIGNMENT - 1U) & ~(O1HEAP_ALIGNMENT - 1U))

static_assert(INSTANCE_SIZE_PADDED >= sizeof(O1HeapInstance), "Invalid instance footprint computation");
static_assert((INSTANCE_SIZE_PADDED% O1HEAP_ALIGNMENT) == 0U, "Invalid instance footprint computation");

static inline bool is_valid_frag(const O1HeapInstance* const handle, const void* const ptr)
{
    if (ptr == NULL || handle == NULL || handle->arena_base == NULL) return false;
    const uintptr_t p = (uintptr_t)ptr;
    const uintptr_t min = (uintptr_t)handle->arena_base;
    const uintptr_t max = min + handle->diagnostics.capacity;
    if (p < min || p >= max || (p % O1HEAP_ALIGNMENT) != 0)
    {
        return false;
    }
    return true;
}

/// Undefined for zero argument.
O1HEAP_PRIVATE uint_fast8_t log2Floor(const size_t x)
{
    O1HEAP_ASSERT(x > 0);
    // NOLINTNEXTLINE redundant cast to the same type.
    return (uint_fast8_t)(((sizeof(x) * CHAR_BIT) - 1U) - ((uint_fast8_t)O1HEAP_CLZ(x)));
}

/// Special case: if the argument is zero, returns zero.
O1HEAP_PRIVATE uint_fast8_t log2Ceil(const size_t x)
{
    // NOLINTNEXTLINE redundant cast to the same type.
    return (x <= 1U) ? 0U : (uint_fast8_t)((sizeof(x) * CHAR_BIT) - ((uint_fast8_t)O1HEAP_CLZ(x - 1U)));
}

/// Raise 2 into the specified power.
/// You might be tempted to do something like (1U << power). WRONG! We humans are prone to forgetting things.
/// If you forget to cast your 1U to size_t or ULL, you may end up with undefined behavior.
O1HEAP_PRIVATE size_t pow2(const uint_fast8_t power)
{
    return ((size_t)1U) << power;
}

/// This is equivalent to pow2(log2Ceil(x)). Undefined for x<2.
O1HEAP_PRIVATE size_t roundUpToPowerOf2(const size_t x)
{
    O1HEAP_ASSERT(x >= 2U);
    // NOLINTNEXTLINE redundant cast to the same type.
    return ((size_t)1U) << ((sizeof(x) * CHAR_BIT) - ((uint_fast8_t)O1HEAP_CLZ(x - 1U)));
}

/// Links two fragments so that their next/prev pointers point to each other; left goes before right.
O1HEAP_PRIVATE void interlink(Fragment* const left, Fragment* const right)
{
    if (O1HEAP_LIKELY(left != NULL))
    {
        left->header.next = right;
    }
    if (O1HEAP_LIKELY(right != NULL))
    {
        right->header.prev = left;
    }
}

/// Adds a new fragment into the appropriate bin and updates the lookup mask.
O1HEAP_PRIVATE void rebin(O1HeapInstance* const handle, Fragment* const fragment)
{
    if (O1HEAP_UNLIKELY(handle == NULL || fragment == NULL || !is_valid_frag(handle, fragment)))
    {
        fprintf(stderr, "[o1heap rebin DROPPED INVALID FRAG] handle=%p frag=%p\n", (void*)handle, (void*)fragment);
        return;
    }
    if (O1HEAP_UNLIKELY(fragment->header.size < FRAGMENT_SIZE_MIN || (fragment->header.size % FRAGMENT_SIZE_MIN) != 0U))
    {
        fprintf(stderr, "[o1heap rebin DROPPED BAD SIZE] frag=%p size=0x%zx\n", (void*)fragment, fragment ? fragment->header.size : 0);
        return;
    }
    const uint_fast8_t idx = log2Floor(fragment->header.size / FRAGMENT_SIZE_MIN);  // Round DOWN when inserting.
    if (O1HEAP_UNLIKELY(idx >= NUM_BINS_MAX))
    {
        fprintf(stderr, "[o1heap rebin DROPPED BAD BIN IDX] frag=%p size=0x%zx idx=%u\n", (void*)fragment, fragment->header.size, (unsigned)idx);
        return;
    }
    // Add the new fragment to the beginning of the bin list.
    // I.e., each allocation will be returning the most-recently-used fragment -- good for caching.
    Fragment* const head = handle->bins[idx];
    if (head != NULL && is_valid_frag(handle, head))
    {
        fragment->next_free = head;
        head->prev_free = fragment;
    }
    else
    {
        fragment->next_free = NULL;
    }
    fragment->prev_free = NULL;
    handle->bins[idx] = fragment;
    handle->nonempty_bin_mask |= pow2(idx);
}

/// Removes the specified fragment from its bin.
O1HEAP_PRIVATE void unbin(O1HeapInstance* const handle, const Fragment* const fragment)
{
    if (O1HEAP_UNLIKELY(handle == NULL || fragment == NULL || !is_valid_frag(handle, fragment))) return;
    if (O1HEAP_UNLIKELY(fragment->header.size < FRAGMENT_SIZE_MIN || (fragment->header.size % FRAGMENT_SIZE_MIN) != 0U)) return;
    const uint_fast8_t idx = log2Floor(fragment->header.size / FRAGMENT_SIZE_MIN);  // Round DOWN when removing.
    if (O1HEAP_UNLIKELY(idx >= NUM_BINS_MAX)) return;

    Fragment* const next_free = (fragment->next_free != NULL && is_valid_frag(handle, fragment->next_free)) ? fragment->next_free : NULL;
    Fragment* const prev_free = (fragment->prev_free != NULL && is_valid_frag(handle, fragment->prev_free)) ? fragment->prev_free : NULL;

    // Remove the bin from the free fragment list.
    if (O1HEAP_LIKELY(next_free != NULL))
    {
        next_free->prev_free = prev_free;
    }
    if (O1HEAP_LIKELY(prev_free != NULL))
    {
        prev_free->next_free = next_free;
    }
    // Update the bin header.
    if (O1HEAP_LIKELY(handle->bins[idx] == fragment))
    {
        handle->bins[idx] = next_free;
        if (O1HEAP_LIKELY(handle->bins[idx] == NULL))
        {
            handle->nonempty_bin_mask &= ~pow2(idx);
        }
    }
}

// ---------------------------------------- PUBLIC API IMPLEMENTATION ----------------------------------------

O1HeapInstance* o1heapInit(void* const base, const size_t size)
{
    O1HeapInstance* out = NULL;
    if ((base != NULL) && ((((size_t)base) % O1HEAP_ALIGNMENT) == 0U) &&
        (size >= FRAGMENT_SIZE_MIN))
    {
        // Allocate the core heap metadata structure in host memory.
        out = (O1HeapInstance*)calloc(1, sizeof(O1HeapInstance));
        if (out == NULL) return NULL;
        out->arena_base = base;
        out->nonempty_bin_mask = 0U;
        for (size_t i = 0; i < NUM_BINS_MAX; i++)
        {
            out->bins[i] = NULL;
        }

        // Limit and align the capacity.
        size_t capacity = size;
        if (capacity > FRAGMENT_SIZE_MAX)
        {
            capacity = FRAGMENT_SIZE_MAX;
        }
        while ((capacity % FRAGMENT_SIZE_MIN) != 0)
        {
            O1HEAP_ASSERT(capacity > 0U);
            capacity--;
        }
        O1HEAP_ASSERT((capacity % FRAGMENT_SIZE_MIN) == 0);
        O1HEAP_ASSERT((capacity >= FRAGMENT_SIZE_MIN) && (capacity <= FRAGMENT_SIZE_MAX));

        // Initialize the diagnostics.
        out->diagnostics.capacity = capacity;
        out->diagnostics.allocated = 0U;
        out->diagnostics.peak_allocated = 0U;
        out->diagnostics.peak_request_size = 0U;
        out->diagnostics.oom_count = 0U;

        // Initialize the root fragment.
        Fragment* const frag = (Fragment*)base;
        O1HEAP_ASSERT((((size_t)frag) % O1HEAP_ALIGNMENT) == 0U);
        frag->header.next = NULL;
        frag->header.prev = NULL;
        frag->header.size = capacity;
        frag->header.used = false;
        frag->next_free = NULL;
        frag->prev_free = NULL;
        rebin(out, frag);
        O1HEAP_ASSERT(out->nonempty_bin_mask != 0U);
    }

    return out;
}

void* o1heapAllocate(O1HeapInstance* const handle, const size_t amount)
{
    O1HEAP_ASSERT(handle != NULL);
    O1HEAP_ASSERT(handle->diagnostics.capacity <= FRAGMENT_SIZE_MAX);
    void* out = NULL;

    // If the amount approaches approx. SIZE_MAX/2, an undetected integer overflow may occur.
    // To avoid that, we do not attempt allocation if the amount exceeds the hard limit.
    // We perform multiple redundant checks to account for a possible unaccounted overflow.
    if (O1HEAP_LIKELY((amount > 0U) && (amount <= (handle->diagnostics.capacity - O1HEAP_ALIGNMENT))))
    {
        // Add the header size and align the allocation size to the power of 2.
        // See "Timing-Predictable Memory Allocation In Hard Real-Time Systems", Herter, page 27.
        const size_t fragment_size = roundUpToPowerOf2(amount + O1HEAP_ALIGNMENT);
        O1HEAP_ASSERT(fragment_size <= FRAGMENT_SIZE_MAX);
        O1HEAP_ASSERT(fragment_size >= FRAGMENT_SIZE_MIN);
        O1HEAP_ASSERT(fragment_size >= amount + O1HEAP_ALIGNMENT);
        O1HEAP_ASSERT((fragment_size & (fragment_size - 1U)) == 0U);  // Is power of 2.

        const uint_fast8_t optimal_bin_index = log2Ceil(fragment_size / FRAGMENT_SIZE_MIN);  // Use CEIL when fetching.
        O1HEAP_ASSERT(optimal_bin_index < NUM_BINS_MAX);
        const size_t candidate_bin_mask = ~(pow2(optimal_bin_index) - 1U);

        // Find the smallest non-empty bin we can use.
        size_t cur_candidate_mask = candidate_bin_mask;
        while (true)
        {
            const size_t suitable_bins = handle->nonempty_bin_mask & cur_candidate_mask;
            const size_t smallest_bin_mask = suitable_bins & ~(suitable_bins - 1U);  // Clear all bits but the lowest.
            if (O1HEAP_UNLIKELY(smallest_bin_mask == 0))
            {
                break;
            }

            const uint_fast8_t bin_index = log2Floor(smallest_bin_mask);
            if (O1HEAP_UNLIKELY(bin_index >= NUM_BINS_MAX))
            {
                break;
            }

            // The bin we found shall not be empty, otherwise it's a state divergence (memory corruption?).
            Fragment* const frag = handle->bins[bin_index];
            if (O1HEAP_UNLIKELY(frag == NULL || !is_valid_frag(handle, frag)))
            {
                fprintf(stderr, "[o1heapAllocate DROPPED INVALID IN BIN %u] frag=%p\n", (unsigned)bin_index, (void*)frag);
                handle->bins[bin_index] = NULL;
                handle->nonempty_bin_mask &= ~pow2(bin_index);
                cur_candidate_mask &= ~pow2(bin_index);
                continue;
            }
            if (O1HEAP_UNLIKELY(frag->header.size < FRAGMENT_SIZE_MIN))
            {
                size_t recovered_size = 0;
                if (frag->header.next != NULL && is_valid_frag(handle, frag->header.next))
                {
                    recovered_size = (size_t)((char*)frag->header.next - (char*)frag);
                }
                else
                {
                    const uintptr_t arena_end = (uintptr_t)handle->arena_base + handle->diagnostics.capacity;
                    if ((uintptr_t)frag < arena_end)
                    {
                        recovered_size = arena_end - (uintptr_t)frag;
                    }
                }
                recovered_size &= ~(FRAGMENT_SIZE_MIN - 1U);
                if (recovered_size >= FRAGMENT_SIZE_MIN)
                {
                    fprintf(stderr, "[o1heapAllocate RECOVERED CORRUPT FRAG IN BIN %u] frag=%p old_size=0x%zx recovered_size=0x%zx\n",
                            (unsigned)bin_index, (void*)frag, frag->header.size, recovered_size);
                    handle->bins[bin_index] = frag->next_free;
                    if (handle->bins[bin_index] == NULL)
                    {
                        handle->nonempty_bin_mask &= ~pow2(bin_index);
                    }
                    if (frag->next_free != NULL && is_valid_frag(handle, frag->next_free))
                    {
                        frag->next_free->prev_free = NULL;
                    }
                    frag->header.size = recovered_size;
                    frag->header.used = false;
                    rebin(handle, frag);
                    continue;
                }

                fprintf(stderr, "[o1heapAllocate DROPPED ZERO/CORRUPT FRAG IN BIN %u] frag=%p size=0x%zx\n",
                        (unsigned)bin_index, (void*)frag, frag->header.size);
                Fragment* walk = frag->next_free;
                while (walk != NULL && is_valid_frag(handle, walk) && walk->header.size < FRAGMENT_SIZE_MIN)
                {
                    walk = walk->next_free;
                }
                handle->bins[bin_index] = (walk != NULL && is_valid_frag(handle, walk)) ? walk : NULL;
                if (handle->bins[bin_index] == NULL)
                {
                    handle->nonempty_bin_mask &= ~pow2(bin_index);
                }
                cur_candidate_mask &= ~pow2(bin_index);
                continue;
            }
            if (O1HEAP_UNLIKELY(frag->header.size < fragment_size))
            {
                cur_candidate_mask &= ~pow2(bin_index);
                continue;
            }

            unbin(handle, frag);

            // Split the fragment if it is too large.
            const size_t leftover = frag->header.size - fragment_size;
            frag->header.size = fragment_size;
            if (O1HEAP_LIKELY(leftover >= FRAGMENT_SIZE_MIN))
            {
                Fragment* const new_frag = (Fragment*)(void*)(((char*)frag) + fragment_size);
                new_frag->header.size = leftover;
                new_frag->header.used = false;
                interlink(new_frag, frag->header.next);
                interlink(frag, new_frag);
                rebin(handle, new_frag);
            }

            // Update the diagnostics.
            handle->diagnostics.allocated += fragment_size;
            if (O1HEAP_LIKELY(handle->diagnostics.peak_allocated < handle->diagnostics.allocated))
            {
                handle->diagnostics.peak_allocated = handle->diagnostics.allocated;
            }

            // Finalize the fragment we just allocated.
            frag->header.used = true;
            out = ((char*)frag) + O1HEAP_ALIGNMENT;
            break;
        }
    }

    // Update the diagnostics.
    if (O1HEAP_LIKELY(handle->diagnostics.peak_request_size < amount))
    {
        handle->diagnostics.peak_request_size = amount;
    }
    if (O1HEAP_LIKELY((out == NULL) && (amount > 0U)))
    {
        handle->diagnostics.oom_count++;
        if (handle->diagnostics.oom_count <= 5 || handle->diagnostics.oom_count == 69)
        {
            const size_t req_frag_size = roundUpToPowerOf2(amount + O1HEAP_ALIGNMENT);
            const uint_fast8_t req_opt_bin = log2Ceil(req_frag_size / FRAGMENT_SIZE_MIN);
            fprintf(stderr, "[o1heapAllocate OOM #%llu] amount=%zu req_frag_size=%zu opt_bin=%u non_empty=0x%zx cand_mask=0x%zx\n",
                    (unsigned long long)handle->diagnostics.oom_count, amount, req_frag_size, (unsigned)req_opt_bin,
                    handle->nonempty_bin_mask, ~(pow2(req_opt_bin) - 1U));
            for (size_t b = 0; b < NUM_BINS_MAX; b++)
            {
                if (handle->bins[b] != NULL)
                {
                    if (is_valid_frag(handle, handle->bins[b]))
                    {
                        fprintf(stderr, "  bin[%zu]: frag=%p size=0x%zx used=%d next=%p\n",
                                b, (void*)handle->bins[b], handle->bins[b]->header.size, (int)handle->bins[b]->header.used,
                                (void*)handle->bins[b]->next_free);
                    }
                    else
                    {
                        fprintf(stderr, "  bin[%zu]: CORRUPTED frag=%p\n", b, (void*)handle->bins[b]);
                    }
                }
            }
        }
    }

    return out;
}

void o1heapFree(O1HeapInstance* const handle, void* const pointer)
{
    if (O1HEAP_UNLIKELY(handle == NULL || pointer == NULL)) return;
    Fragment* const frag = (Fragment*)(void*)(((char*)pointer) - O1HEAP_ALIGNMENT);
    if (O1HEAP_UNLIKELY(!is_valid_frag(handle, frag))) return;
    if (O1HEAP_UNLIKELY(!frag->header.used)) return;  // Catch double-free
    if (O1HEAP_UNLIKELY(frag->header.size < FRAGMENT_SIZE_MIN || frag->header.size > handle->diagnostics.capacity)) return;

    // Even if we're going to drop the fragment later, mark it free anyway to prevent double-free.
    frag->header.used = false;

    // Update the diagnostics. It must be done before merging because it invalidates the fragment size information.
    if (handle->diagnostics.allocated >= frag->header.size)
    {
        handle->diagnostics.allocated -= frag->header.size;
    }

    // Merge with siblings and insert the returned fragment into the appropriate bin and update metadata.
    Fragment* const prev = (frag->header.prev != NULL && is_valid_frag(handle, frag->header.prev)) ? frag->header.prev : NULL;
    Fragment* const next = (frag->header.next != NULL && is_valid_frag(handle, frag->header.next)) ? frag->header.next : NULL;
    const bool      join_left = (prev != NULL) && (!prev->header.used);
    const bool      join_right = (next != NULL) && (!next->header.used);
    if (join_left && join_right)  // [ prev ][ this ][ next ] => [ ------- prev ------- ]
    {
        unbin(handle, prev);
        unbin(handle, next);
        prev->header.size += frag->header.size + next->header.size;
        frag->header.size = 0;  // Invalidate the dropped fragment headers to prevent double-free.
        next->header.size = 0;
        interlink(prev, (next->header.next != NULL && is_valid_frag(handle, next->header.next)) ? next->header.next : NULL);
        rebin(handle, prev);
    }
    else if (join_left)  // [ prev ][ this ][ next ] => [ --- prev --- ][ next ]
    {
        unbin(handle, prev);
        prev->header.size += frag->header.size;
        frag->header.size = 0;
        interlink(prev, next);
        rebin(handle, prev);
    }
    else if (join_right)  // [ prev ][ this ][ next ] => [ prev ][ --- this --- ]
    {
        unbin(handle, next);
        frag->header.size += next->header.size;
        next->header.size = 0;
        interlink(frag, (next->header.next != NULL && is_valid_frag(handle, next->header.next)) ? next->header.next : NULL);
        rebin(handle, frag);
    }
    else  // [ prev ][ this ][ next ]
    {
        rebin(handle, frag);
    }
}

bool o1heapDoInvariantsHold(const O1HeapInstance* const handle)
{
    O1HEAP_ASSERT(handle != NULL);
    bool valid = true;

    // Check the bin mask consistency.
    for (size_t i = 0; i < NUM_BINS_MAX; i++)  // Dear compiler, feel free to unroll this loop.
    {
        const bool mask_bit_set = (handle->nonempty_bin_mask & pow2((uint_fast8_t)i)) != 0U;
        const bool bin_nonempty = handle->bins[i] != NULL;
        valid = valid && (mask_bit_set == bin_nonempty);
    }

    // Create a local copy of the diagnostics struct.
    const O1HeapDiagnostics diag = handle->diagnostics;

    // Capacity check.
    valid = valid && (diag.capacity <= FRAGMENT_SIZE_MAX) && (diag.capacity >= FRAGMENT_SIZE_MIN) &&
        ((diag.capacity % FRAGMENT_SIZE_MIN) == 0U);

    // Allocation info check.
    valid = valid && (diag.allocated <= diag.capacity) && ((diag.allocated % FRAGMENT_SIZE_MIN) == 0U) &&
        (diag.peak_allocated <= diag.capacity) && (diag.peak_allocated >= diag.allocated) &&
        ((diag.peak_allocated % FRAGMENT_SIZE_MIN) == 0U);

    // Peak request check
    valid = valid && ((diag.peak_request_size < diag.capacity) || (diag.oom_count > 0U));
    if (diag.peak_request_size == 0U)
    {
        valid = valid && (diag.peak_allocated == 0U) && (diag.allocated == 0U) && (diag.oom_count == 0U);
    }
    else
    {
        valid = valid &&  // Overflow on summation is possible but safe to ignore.
            (((diag.peak_request_size + O1HEAP_ALIGNMENT) <= diag.peak_allocated) || (diag.oom_count > 0U));
    }

    return valid;
}

O1HeapDiagnostics o1heapGetDiagnostics(const O1HeapInstance* const handle)
{
    O1HEAP_ASSERT(handle != NULL);
    const O1HeapDiagnostics out = handle->diagnostics;
    return out;
}
