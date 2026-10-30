#if SOGEN_ENABLE_FEX_HVF

#include <gtest/gtest.h>

#include <hvf_address_space.hpp>

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include <sys/mman.h>

namespace sogen::test
{
    namespace
    {
        using fex::hvf::address_space;
        using fex::hvf::stage1_page_size;
        using fex::hvf::stage2_page_size;

        constexpr uint64_t base = 0x7000'0000'0000ULL;
        constexpr uint64_t ipa_limit = 1ULL << 36;
        constexpr int rw = PROT_READ | PROT_WRITE;

        struct stage2_call
        {
            enum class kind
            {
                map,
                unmap,
                protect,
            };

            kind type{};
            uint64_t host_address{};
            uint64_t ipa{};
            uint64_t size{};
            int prot{};

            bool operator==(const stage2_call&) const = default;
        };

        stage2_call map_call(const uint64_t host_address, const uint64_t ipa, const uint64_t size, const int prot)
        {
            return {.type = stage2_call::kind::map, .host_address = host_address, .ipa = ipa, .size = size, .prot = prot};
        }

        stage2_call unmap_call(const uint64_t ipa, const uint64_t size)
        {
            return {.type = stage2_call::kind::unmap, .ipa = ipa, .size = size};
        }

        stage2_call protect_call(const uint64_t ipa, const uint64_t size, const int prot)
        {
            return {.type = stage2_call::kind::protect, .ipa = ipa, .size = size, .prot = prot};
        }

        class recording_stage2 final : public fex::hvf::stage2_mapper
        {
          public:
            std::vector<stage2_call> calls;
            stage2_call table_pool{};
            bool host_executable = false;

            bool is_host_executable(uint64_t /*host_address*/, size_t /*size*/) const override
            {
                return this->host_executable;
            }

            void map(const uint64_t host_address, const uint64_t ipa, const size_t size, const int prot) override
            {
                const auto call = map_call(host_address, ipa, size, prot);
                if (this->table_pool.size == 0)
                {
                    this->table_pool = call;
                    return;
                }

                this->calls.push_back(call);
            }

            void unmap(const uint64_t ipa, const size_t size) override
            {
                this->calls.push_back(unmap_call(ipa, size));
            }

            void protect(const uint64_t ipa, const size_t size, const int prot) override
            {
                this->calls.push_back(protect_call(ipa, size, prot));
            }

            // Walks the stage-1 tables the way the vCPU's MMU would, reading them through the table pool's
            // stage-2 mapping.
            uint64_t stage1_descriptor(const address_space& space, const uint64_t address) const
            {
                uint64_t table_ipa = space.stage1_root();
                for (int shift = 39; shift >= 12; shift -= 9)
                {
                    const auto* table =
                        reinterpret_cast<const uint64_t*>(this->table_pool.host_address + (table_ipa - this->table_pool.ipa));
                    const uint64_t descriptor = table[(address >> shift) & 0x1FF];
                    if (shift == 12 || (descriptor & 0b11) != 0b11)
                    {
                        return descriptor;
                    }

                    table_ipa = descriptor & 0x0000FFFFFFFFF000ULL;
                }

                return 0;
            }
        };

        class FexHvfAddressSpaceTest : public testing::Test
        {
          protected:
            recording_stage2 stage2{};
            address_space space{stage2, ipa_limit};

            uint64_t map_and_get_ipa(const uint64_t address, const size_t size, const int prot = rw)
            {
                this->stage2.calls.clear();
                this->space.map(address, size, prot);
                EXPECT_EQ(this->stage2.calls.size(), 1U);
                return this->stage2.calls.empty() ? 0 : this->stage2.calls.back().ipa;
            }
        };
    }

    TEST_F(FexHvfAddressSpaceTest, ContiguousUnmappedPagesBecomeOneStage2Mapping)
    {
        this->space.map(base, 3 * stage2_page_size, rw);

        ASSERT_EQ(this->stage2.calls.size(), 1U);
        EXPECT_EQ(this->stage2.calls[0].type, stage2_call::kind::map);
        EXPECT_EQ(this->stage2.calls[0].host_address, base);
        EXPECT_EQ(this->stage2.calls[0].size, 3 * stage2_page_size);
        EXPECT_EQ(this->stage2.calls[0].prot, rw);
    }

    TEST_F(FexHvfAddressSpaceTest, RangesWidenToWholeStage2Pages)
    {
        this->space.map(base + 0x3000, 0x2000, rw);

        ASSERT_EQ(this->stage2.calls.size(), 1U);
        EXPECT_EQ(this->stage2.calls[0].host_address, base);
        EXPECT_EQ(this->stage2.calls[0].size, 2 * stage2_page_size);
    }

    TEST_F(FexHvfAddressSpaceTest, ProtectOnlyChangesPagesWhoseProtectionDiffers)
    {
        const auto ipa = this->map_and_get_ipa(base, 4 * stage2_page_size);
        this->space.protect(base + stage2_page_size, stage2_page_size, PROT_READ);
        this->stage2.calls.clear();

        this->space.protect(base, 4 * stage2_page_size, PROT_READ);

        const std::vector expected{
            protect_call(ipa, stage2_page_size, PROT_READ),
            protect_call(ipa + 2 * stage2_page_size, 2 * stage2_page_size, PROT_READ),
        };
        EXPECT_EQ(this->stage2.calls, expected);
    }

    TEST_F(FexHvfAddressSpaceTest, MapReprotectsAlreadyMappedPages)
    {
        const auto ipa = this->map_and_get_ipa(base, stage2_page_size);
        this->stage2.calls.clear();

        this->space.map(base, 2 * stage2_page_size, PROT_READ);

        ASSERT_EQ(this->stage2.calls.size(), 2U);
        EXPECT_EQ(this->stage2.calls[0], protect_call(ipa, stage2_page_size, PROT_READ));
        EXPECT_EQ(this->stage2.calls[1].type, stage2_call::kind::map);
        EXPECT_EQ(this->stage2.calls[1].host_address, base + stage2_page_size);
    }

    TEST_F(FexHvfAddressSpaceTest, ProtectSplitsAtIpaDiscontinuities)
    {
        const auto first = this->map_and_get_ipa(base, stage2_page_size);
        this->map_and_get_ipa(base + 0x100000, stage2_page_size);
        const auto second = this->map_and_get_ipa(base + stage2_page_size, stage2_page_size);
        this->stage2.calls.clear();

        this->space.protect(base, 2 * stage2_page_size, PROT_READ);

        const std::vector expected{
            protect_call(first, stage2_page_size, PROT_READ),
            protect_call(second, stage2_page_size, PROT_READ),
        };
        EXPECT_EQ(this->stage2.calls, expected);
    }

    TEST_F(FexHvfAddressSpaceTest, UnmapSplitsAtIpaDiscontinuities)
    {
        const auto first = this->map_and_get_ipa(base, stage2_page_size);
        this->map_and_get_ipa(base + 0x100000, stage2_page_size);
        const auto second = this->map_and_get_ipa(base + stage2_page_size, stage2_page_size);
        this->stage2.calls.clear();

        this->space.unmap(base, 4 * stage2_page_size);

        const std::vector expected{unmap_call(first, stage2_page_size), unmap_call(second, stage2_page_size)};
        EXPECT_EQ(this->stage2.calls, expected);
    }

    TEST_F(FexHvfAddressSpaceTest, RepeatedMapAndUnmapKeepsIpaUseBounded)
    {
        uint64_t highest_ipa_end = 0;
        for (uint64_t i = 0; i < 1000; ++i)
        {
            const uint64_t address = base + (i % 7) * 0x1000000;
            const size_t size = (1 + (i % 5)) * stage2_page_size;
            const auto ipa = this->map_and_get_ipa(address, size);
            highest_ipa_end = std::max(highest_ipa_end, ipa + size);
            this->space.unmap(address, size);
        }

        EXPECT_LE(highest_ipa_end, this->stage2.table_pool.ipa + this->stage2.table_pool.size + 5 * stage2_page_size);
    }

    TEST_F(FexHvfAddressSpaceTest, ReleasedIpasAreReusedBestFit)
    {
        const auto a = this->map_and_get_ipa(base, stage2_page_size);
        this->map_and_get_ipa(base + 0x100000, stage2_page_size);
        const auto c = this->map_and_get_ipa(base + 0x200000, 2 * stage2_page_size);
        this->map_and_get_ipa(base + 0x300000, stage2_page_size);

        this->space.unmap(base + 0x200000, 2 * stage2_page_size);
        this->space.unmap(base, stage2_page_size);

        EXPECT_EQ(this->map_and_get_ipa(base + 0x400000, stage2_page_size), a);
        EXPECT_EQ(this->map_and_get_ipa(base + 0x500000, 2 * stage2_page_size), c);
    }

    TEST_F(FexHvfAddressSpaceTest, ReleasedNeighboursCoalesce)
    {
        const auto a = this->map_and_get_ipa(base, stage2_page_size);
        this->map_and_get_ipa(base + 0x100000, stage2_page_size);
        this->map_and_get_ipa(base + 0x200000, stage2_page_size);
        this->map_and_get_ipa(base + 0x300000, stage2_page_size);

        this->space.unmap(base, stage2_page_size);
        this->space.unmap(base + 0x200000, stage2_page_size);
        this->space.unmap(base + 0x100000, stage2_page_size);

        EXPECT_EQ(this->map_and_get_ipa(base + 0x400000, 3 * stage2_page_size), a);
    }

    TEST_F(FexHvfAddressSpaceTest, ReleasingTheTopRunLowersTheCursor)
    {
        this->map_and_get_ipa(base, stage2_page_size);
        const auto b = this->map_and_get_ipa(base + 0x100000, stage2_page_size);

        this->space.unmap(base + 0x100000, stage2_page_size);

        EXPECT_EQ(this->map_and_get_ipa(base + 0x200000, 4 * stage2_page_size), b);
    }

    TEST_F(FexHvfAddressSpaceTest, Stage1EntriesCoverEveryGuestPageOfAStage2Page)
    {
        const auto ipa = this->map_and_get_ipa(base, stage2_page_size);

        for (uint64_t offset = 0; offset < stage2_page_size; offset += stage1_page_size)
        {
            const auto descriptor = this->stage2.stage1_descriptor(this->space, base + offset);
            EXPECT_EQ(descriptor & 0b11, 0b11U);
            EXPECT_EQ(descriptor & 0x0000FFFFFFFFF000ULL, ipa + offset);
        }

        EXPECT_EQ(this->stage2.stage1_descriptor(this->space, base + stage2_page_size), 0U);
    }

    TEST_F(FexHvfAddressSpaceTest, UnmapClearsStage1Entries)
    {
        this->map_and_get_ipa(base, stage2_page_size);
        this->space.unmap(base, stage2_page_size);

        for (uint64_t offset = 0; offset < stage2_page_size; offset += stage1_page_size)
        {
            EXPECT_EQ(this->stage2.stage1_descriptor(this->space, base + offset), 0U);
        }
    }

    TEST_F(FexHvfAddressSpaceTest, OnlyRemovingAMappingChangesTheStage1Generation)
    {
        const auto initial = this->space.stage1_generation();

        this->space.map(base, 2 * stage2_page_size, rw);
        this->space.protect(base, stage2_page_size, PROT_READ);
        this->space.refresh_backing(base, 2 * stage2_page_size);
        EXPECT_EQ(this->space.stage1_generation(), initial);

        this->space.unmap(base, stage2_page_size);
        EXPECT_NE(this->space.stage1_generation(), initial);
    }

    TEST_F(FexHvfAddressSpaceTest, RefreshRemapsTheSameIpaWithTheSameProtection)
    {
        const auto ipa = this->map_and_get_ipa(base, 2 * stage2_page_size);
        this->space.protect(base + stage2_page_size, stage2_page_size, PROT_READ);
        this->stage2.calls.clear();

        this->space.refresh_backing(base, 3 * stage2_page_size);

        const std::vector expected{
            unmap_call(ipa, stage2_page_size),
            map_call(base, ipa, stage2_page_size, rw),
            unmap_call(ipa + stage2_page_size, stage2_page_size),
            map_call(base + stage2_page_size, ipa + stage2_page_size, stage2_page_size, PROT_READ),
        };
        EXPECT_EQ(this->stage2.calls, expected);
    }

    TEST_F(FexHvfAddressSpaceTest, ProtectingAnUnmappedPageThrows)
    {
        this->map_and_get_ipa(base, stage2_page_size);
        this->stage2.calls.clear();

        EXPECT_THROW(this->space.protect(base, 2 * stage2_page_size, PROT_READ), std::runtime_error);
        EXPECT_TRUE(this->stage2.calls.empty());
    }

    TEST_F(FexHvfAddressSpaceTest, HostExecutableMemoryIsNeverMapped)
    {
        const auto ipa = this->map_and_get_ipa(base, stage2_page_size);
        this->space.unmap(base, stage2_page_size);
        this->stage2.calls.clear();

        this->stage2.host_executable = true;
        EXPECT_THROW(this->space.map(base, stage2_page_size, PROT_READ | PROT_EXEC), std::runtime_error);
        EXPECT_TRUE(this->stage2.calls.empty());

        this->stage2.host_executable = false;
        EXPECT_EQ(this->map_and_get_ipa(base, stage2_page_size), ipa);
    }
}

#endif
