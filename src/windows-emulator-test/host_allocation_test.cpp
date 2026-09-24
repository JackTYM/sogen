#include <gtest/gtest.h>
#include <memory_manager.hpp>

#include <algorithm>
#include <optional>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

namespace sogen::test
{
    namespace
    {
        // Minimal memory_interface reporting a controllable set of "foreign" host ranges - the ranges a
        // backend sharing the guest address space with the host process (FEX on Apple Silicon: guest VA ==
        // host VA) would say the guest must avoid because the host itself already occupies them. Everything
        // else is an unused stub: the code under test (find_free_host_allocation_base) only ever queries
        // reserved_host_ranges[_in]; it never maps, reads or writes guest memory (its allocate_memory_raw
        // calls are all reserve-only).
        class fake_host_memory : public memory_interface
        {
          public:
            // Foreign host mappings the guest must avoid. Reported by BOTH the full reserved_host_ranges()
            // scan and the windowed reserved_host_ranges_in() probe - an ordinary foreign occupant.
            std::vector<host_reserved_range> foreign_ranges{};

            // Ranges the windowed reserved_host_ranges_in() probe reports as occupied but the full
            // reserved_host_ranges() scan deliberately OMITS - a backend may skip ranges it considers
            // guest-owned rather than foreign in its full scan while its windowed probe still reports
            // them occupied. This asymmetry is why find_free_host_allocation_base's rescan must include
            // the windowed reserve_host_memory_ranges_in(pick): a full-only rescan never records these,
            // so a pick that lands here is re-picked identically every iteration until the retry cap.
            std::vector<host_reserved_range> hidden_from_full_scan{};

            // Guest ranges the memory manager claimed/released at the host level - the mechanism a
            // shared-address-space backend uses to keep reserved-but-uncommitted guest ranges
            // unavailable to the host's own allocator, and to hand them back once genuinely freed.
            std::vector<host_reserved_range> claimed_ranges{};
            std::vector<host_reserved_range> released_ranges{};

            bool reserve_guest_address_range(const uint64_t address, const size_t size) override
            {
                this->claimed_ranges.push_back({.address = address, .size = size});
                return true;
            }

            void release_guest_address_range(const uint64_t address, const size_t size) override
            {
                this->released_ranges.push_back({.address = address, .size = size});
            }

            std::vector<host_reserved_range> reserved_host_ranges() const override
            {
                return this->foreign_ranges;
            }

            std::vector<host_reserved_range> reserved_host_ranges_in(const uint64_t address, const size_t size) const override
            {
                std::vector<host_reserved_range> result{};
                const auto window_end = address + size;
                const auto clip = [&](const std::vector<host_reserved_range>& ranges) {
                    for (const auto& range : ranges)
                    {
                        const auto range_end = range.address + range.size;
                        const auto start = std::max(address, range.address);
                        const auto end = std::min(window_end, range_end);
                        if (start < end)
                        {
                            result.push_back({.address = start, .size = static_cast<size_t>(end - start)});
                        }
                    }
                };
                clip(this->foreign_ranges);
                clip(this->hidden_from_full_scan);
                return result;
            }

            void read_memory(uint64_t, void*, size_t) const override
            {
                throw std::logic_error("unexpected read_memory in host-allocation test");
            }

            bool try_read_memory(uint64_t, void*, size_t) const override
            {
                return false;
            }

            void write_memory(uint64_t, const void*, size_t) override
            {
                throw std::logic_error("unexpected write_memory in host-allocation test");
            }

            bool try_write_memory(uint64_t, const void*, size_t) override
            {
                return false;
            }

          private:
            void map_mmio(uint64_t, size_t, mmio_read_callback, mmio_write_callback) override
            {
            }

            void map_memory(uint64_t, size_t, memory_permission) override
            {
            }

            void unmap_memory(uint64_t, size_t) override
            {
            }

            void apply_memory_protection(uint64_t, size_t, memory_permission) override
            {
            }
        };
    }

    // Regression coverage for the module-relocation fallback's host-collision recovery.
    // map_module_from_data's relocation fallback must route through find_free_host_allocation_base
    // rather than find_free_allocation_base (sogen's own bookkeeping only): on backends sharing the
    // guest VA with the host process (FEX on Apple Silicon), any real host allocation can land on a
    // guest-owned VA, so a bookkeeping-only pick can already be occupied by a foreign host mapping.
    // find_free_host_allocation_base confirms the pick is actually free at the host level and
    // re-picks past any foreign occupant; this test exercises that shared helper.
    TEST(HostAllocationTest, FindFreeHostBaseSkipsForeignOccupiedPick)
    {
        fake_host_memory host{};
        memory_manager mm{host};

        constexpr size_t size = 0x2000;
        constexpr uint64_t start = DEFAULT_ALLOCATION_ADDRESS_64BIT;

        // The address the plain, bookkeeping-only pick hands back.
        const uint64_t naive_base = mm.find_free_allocation_base(size, start);
        ASSERT_NE(naive_base, 0u);

        // A foreign host mapping now occupies exactly that address, invisible to sogen's bookkeeping.
        host.foreign_ranges.push_back({.address = naive_base, .size = size});

        // The plain pick still returns the now-occupied address (it cannot see the foreign mapping)...
        ASSERT_EQ(mm.find_free_allocation_base(size, start), naive_base);

        // ...but the host-aware pick confirms and steps past it.
        const uint64_t host_base = mm.find_free_host_allocation_base(size, start);
        ASSERT_NE(host_base, 0u);
        ASSERT_NE(host_base, naive_base);
        ASSERT_GE(host_base, naive_base + size);

        // The returned base really is clear of every foreign range.
        ASSERT_TRUE(mm.host_window_is_free(host_base, size));
    }

    // With no foreign mappings (the default / independent-address-space backends such as unicorn), the
    // host-aware pick is identical to the plain one - the fix is a correctness no-op there.
    TEST(HostAllocationTest, FindFreeHostBaseMatchesPlainWhenNoForeignRanges)
    {
        fake_host_memory host{};
        memory_manager mm{host};

        constexpr size_t size = 0x2000;
        constexpr uint64_t start = DEFAULT_ALLOCATION_ADDRESS_64BIT;

        ASSERT_EQ(mm.find_free_host_allocation_base(size, start), mm.find_free_allocation_base(size, start));
    }

    // find_free_host_allocation_base confirms a pick with the windowed host_window_is_free probe; a
    // rescan that only used the full reserve_host_memory_ranges() would record nothing new for a range
    // the full scan omits, so the same occupied pick would be chosen every iteration until the retry
    // cap and the allocation would fail outright. The rescan therefore also records the just-rejected
    // pick window via reserve_host_memory_ranges_in, so the next pick steps past a range the full scan
    // cannot see. hidden_from_full_scan models exactly that range.
    TEST(HostAllocationTest, FindFreeHostBaseStepsPastRangeHiddenFromFullScan)
    {
        fake_host_memory host{};
        memory_manager mm{host};

        constexpr size_t size = 0x2000;
        constexpr uint64_t start = DEFAULT_ALLOCATION_ADDRESS_64BIT;

        const uint64_t naive_base = mm.find_free_allocation_base(size, start);
        ASSERT_NE(naive_base, 0u);

        // Occupy the naive pick with a range the full reserved_host_ranges() scan never reports (only the
        // windowed reserved_host_ranges_in() sees it). A full-only rescan would spin on naive_base
        // forever; the windowed record in the rescan is what lets the pick advance.
        host.hidden_from_full_scan.push_back({.address = naive_base, .size = size});

        const uint64_t host_base = mm.find_free_host_allocation_base(size, start);
        ASSERT_NE(host_base, 0u);
        ASSERT_GE(host_base, naive_base + size);
        ASSERT_TRUE(mm.host_window_is_free(host_base, size));
    }

    // The complementary need: the rescan must ALSO keep the full reserve_host_memory_ranges(), not just
    // the windowed record. A large contiguous foreign region at the naive pick - wider than
    // max_host_reserved_retries picks - can only be cleared in the bounded retry budget if the full scan
    // records the whole region at once; recording just one pick-sized slice per iteration would exhaust
    // the retries mid-region and fail. The region here is visible to the full scan and spans far more
    // than the retry budget could step past one slice at a time.
    TEST(HostAllocationTest, FindFreeHostBaseJumpsPastLargeForeignRegionWithinRetryBudget)
    {
        fake_host_memory host{};
        memory_manager mm{host};

        constexpr size_t size = 0x2000;
        constexpr uint64_t start = DEFAULT_ALLOCATION_ADDRESS_64BIT;

        const uint64_t naive_base = mm.find_free_allocation_base(size, start);
        ASSERT_NE(naive_base, 0u);

        // A single contiguous foreign region far larger than (retry budget * size) sitting on the naive pick.
        // Only a full-scan rescan (which records the whole region in one step) can clear it within the bound.
        constexpr size_t large_region = size * 4096;
        host.foreign_ranges.push_back({.address = naive_base, .size = large_region});

        const uint64_t host_base = mm.find_free_host_allocation_base(size, start);
        ASSERT_NE(host_base, 0u);
        ASSERT_GE(host_base, naive_base + large_region);
        ASSERT_TRUE(mm.host_window_is_free(host_base, size));
    }

    // allocate_mmio may claim an address inside an already-recorded host_reserved range (the KUSD
    // MMIO page lives inside the __PAGEZERO carve-out on FEX/Apple). Nesting the MMIO entry inside
    // the host_reserved one would break the pairwise-non-overlap invariant
    // overlaps_reserved_region's fast path depends on: a query above the small MMIO entry would see
    // only that entry as its predecessor and miss the much larger host_reserved range still
    // covering the queried address. The host_reserved coverage must instead be split around the
    // MMIO hole so every part of it stays visible to overlap queries.
    TEST(HostAllocationTest, OverlapQuerySeesHostReservedCoverageAroundNestedMmio)
    {
        fake_host_memory host{};
        memory_manager mm{host};

        constexpr uint64_t reserved_base = 0x70000000;
        constexpr size_t reserved_size = 0x20000000;
        constexpr uint64_t mmio_base = 0x7ffe0000;
        constexpr size_t mmio_size = 0x1000;

        host.foreign_ranges.push_back({.address = reserved_base, .size = reserved_size});
        mm.reserve_host_memory_ranges();

        ASSERT_TRUE(mm.allocate_mmio(mmio_base, mmio_size, [](uint64_t, void*, size_t) {}, [](uint64_t, const void*, size_t) {}));

        ASSERT_EQ(mm.get_region_kind(mmio_base), memory_region_kind::mmio);
        ASSERT_EQ(mm.get_region_kind(mmio_base - 1), memory_region_kind::host_reserved);
        ASSERT_EQ(mm.get_region_kind(mmio_base + mmio_size), memory_region_kind::host_reserved);

        // A range inside the host-reserved coverage above the MMIO page must still be reported as
        // occupied, and the MMIO page itself must remain queryable as its own region.
        ASSERT_TRUE(mm.overlaps_reserved_region(0x80000000, 0x1000));
        ASSERT_TRUE(mm.overlaps_reserved_region(mmio_base, mmio_size));
        ASSERT_TRUE(mm.overlaps_reserved_region(reserved_base, 0x1000));
        ASSERT_FALSE(mm.overlaps_reserved_region(reserved_base + reserved_size, 0x1000));
    }

    // A decommit must keep the guest range claimed at the host level (the range is still
    // MEM_RESERVE'd - a foreign host allocation landing there would be clobbered by a later
    // recommit), while a genuine release must hand the claim back. The memory manager signals the
    // latter via release_guest_address_range with a range covering the freed allocation.
    TEST(HostAllocationTest, ReleaseNotifiesHostClaimReleaseButDecommitDoesNot)
    {
        fake_host_memory host{};
        memory_manager mm{host};

        constexpr size_t size = 0x11000;
        const uint64_t base = mm.allocate_memory(size, nt_memory_permission{memory_permission::read_write});
        ASSERT_NE(base, 0u);

        ASSERT_EQ(host.claimed_ranges.size(), 1u);
        ASSERT_EQ(host.claimed_ranges[0].address, base);
        ASSERT_EQ(host.claimed_ranges[0].size, size);

        ASSERT_TRUE(mm.decommit_memory(base, size));
        ASSERT_TRUE(host.released_ranges.empty());

        ASSERT_TRUE(mm.release_memory(base, 0));
        ASSERT_EQ(host.released_ranges.size(), 1u);
        ASSERT_LE(host.released_ranges[0].address, base);
        ASSERT_GE(host.released_ranges[0].address + host.released_ranges[0].size, base + size);
    }

    namespace
    {
        std::optional<uint64_t> reference_checked_align_up(const uint64_t value, const uint64_t alignment)
        {
            if (value > UINT64_MAX - (alignment - 1))
            {
                return std::nullopt;
            }

            return (value + alignment - 1) & ~(alignment - 1);
        }

        // Independent reoccurrence of find_free_allocation_base's documented contract, re-derived
        // from scratch (a plain per-candidate scan of the live region map, not reusing any of the
        // seek-based production code under test) and used as a differential oracle below.
        uint64_t reference_find_free_allocation_base(const memory_manager& mm, const size_t size, const uint64_t start,
                                                     const uint64_t alignment, uint64_t lowest_address, uint64_t highest_address)
        {
            lowest_address = std::max<uint64_t>(lowest_address, MIN_ALLOCATION_ADDRESS);
            highest_address =
                std::min<uint64_t>(highest_address ? highest_address : MAX_ALLOCATION_END_EXCL - 1, MAX_ALLOCATION_END_EXCL - 1);
            if (lowest_address > highest_address || size == 0)
            {
                return 0;
            }

            auto candidate = start ? start : mm.get_default_allocation_address();
            if (candidate < lowest_address || candidate > highest_address)
            {
                candidate = lowest_address;
            }

            auto aligned = reference_checked_align_up(candidate, alignment);
            if (!aligned)
            {
                return 0;
            }

            uint64_t start_address = *aligned;

            while (start_address <= highest_address)
            {
                const auto end_address = start_address + size;
                if (end_address < start_address || end_address > MAX_ALLOCATION_END_EXCL || end_address - 1 > highest_address)
                {
                    return 0;
                }

                bool conflict = false;
                for (const auto& region : mm.get_reserved_regions())
                {
                    const auto region_end = region.first + region.second.length;
                    if (region_end <= start_address)
                    {
                        continue;
                    }

                    if (region.first >= end_address)
                    {
                        break;
                    }

                    conflict = true;
                    aligned = reference_checked_align_up(region_end, alignment);
                    if (!aligned)
                    {
                        return 0;
                    }

                    start_address = *aligned;
                    break;
                }

                if (!conflict)
                {
                    return start_address;
                }
            }

            return 0;
        }
    }

    // Differential regression coverage for find_free_allocation_base's seek-based gap search: builds up
    // a heavily fragmented reserved_regions_ map through randomized (fixed-seed, deterministic)
    // reserve/release churn, then cross-checks many queries - including ones whose candidate must skip
    // past several consecutive reserved regions in one call - against reference_find_free_allocation_base
    // above. The two must always agree; a mismatch would mean the seek-forward rewrite (which only ever
    // advances its region-map iterator, never rescans from the beginning) skipped or misjudged a region.
    TEST(HostAllocationTest, FindFreeAllocationBaseMatchesReferenceUnderFragmentation)
    {
        fake_host_memory host{};
        memory_manager mm{host};

        std::mt19937 rng(0xC0FFEEu);
        std::uniform_int_distribution<int> granule_count(1, 8);
        constexpr nt_memory_permission perms{memory_permission::read_write};

        std::vector<std::pair<uint64_t, size_t>> live{};

        for (int i = 0; i < 400; ++i)
        {
            if (!live.empty() && (rng() % 3) == 0)
            {
                const auto idx = rng() % live.size();
                const auto [addr, size] = live[idx];
                ASSERT_TRUE(mm.release_memory(addr, size));
                live.erase(live.begin() + static_cast<std::ptrdiff_t>(idx));
            }
            else
            {
                const size_t size = static_cast<size_t>(granule_count(rng)) * ALLOCATION_GRANULARITY;
                const uint64_t base = mm.allocate_memory(size, perms, true);
                if (base != 0)
                {
                    live.emplace_back(base, size);
                }
            }

            for (int q = 0; q < 5; ++q)
            {
                const size_t query_size = static_cast<size_t>(granule_count(rng)) * ALLOCATION_GRANULARITY;
                const uint64_t query_start = (rng() % 2 == 0) ? 0 : DEFAULT_ALLOCATION_ADDRESS_64BIT + (rng() % 0x10000000u);
                constexpr auto highest_address = MAX_ALLOCATION_ADDRESS;

                const auto actual =
                    mm.find_free_allocation_base(query_size, query_start, ALLOCATION_GRANULARITY, MIN_ALLOCATION_ADDRESS, highest_address);
                const auto expected = reference_find_free_allocation_base(mm, query_size, query_start, ALLOCATION_GRANULARITY,
                                                                          MIN_ALLOCATION_ADDRESS, highest_address);
                ASSERT_EQ(actual, expected) << "iteration=" << i << " query_size=" << query_size << " query_start=" << query_start
                                            << " live_regions=" << mm.get_reserved_regions().size();
            }
        }
    }
} // namespace sogen::test
