#include <gtest/gtest.h>
#include <memory_manager.hpp>

#include <algorithm>
#include <stdexcept>
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
            std::vector<host_reserved_range> foreign_ranges{};

            std::vector<host_reserved_range> reserved_host_ranges() const override
            {
                return this->foreign_ranges;
            }

            std::vector<host_reserved_range> reserved_host_ranges_in(const uint64_t address, const size_t size) const override
            {
                std::vector<host_reserved_range> result{};
                const auto window_end = address + size;
                for (const auto& range : this->foreign_ranges)
                {
                    const auto range_end = range.address + range.size;
                    const auto start = std::max(address, range.address);
                    const auto end = std::min(window_end, range_end);
                    if (start < end)
                    {
                        result.push_back({start, static_cast<size_t>(end - start)});
                    }
                }
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

    // Regression coverage for the module-relocation fallback's host-collision recovery. Before the fix,
    // map_module_from_data's relocation fallback picked a base with find_free_allocation_base (sogen's own
    // bookkeeping only) and retried the map exactly once; if that pick was already occupied by a foreign
    // host mapping - possible on backends sharing the guest VA with the host process (FEX on Apple Silicon)
    // where any real host allocation can land on a guest-owned VA - it threw "Memory range not allocatable".
    // The fallback now routes through find_free_host_allocation_base, which confirms the pick is actually
    // free at the host level and re-picks past any foreign occupant. This exercises that shared helper.
    TEST(HostAllocationTest, FindFreeHostBaseSkipsForeignOccupiedPick)
    {
        fake_host_memory host{};
        memory_manager mm{host};

        constexpr size_t size = 0x2000;
        constexpr uint64_t start = DEFAULT_ALLOCATION_ADDRESS_64BIT;

        // The address the plain, bookkeeping-only pick hands back - the one the old fallback used.
        const uint64_t naive_base = mm.find_free_allocation_base(size, start);
        ASSERT_NE(naive_base, 0u);

        // A foreign host mapping now occupies exactly that address, invisible to sogen's bookkeeping.
        host.foreign_ranges.push_back({naive_base, size});

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
} // namespace sogen::test
