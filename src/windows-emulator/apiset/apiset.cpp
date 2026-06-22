#include "../std_include.hpp"

#include "apiset.hpp"
#include "default_apiset.hpp"

#include "../emulator_utils.hpp"

#include <arch_emulator.hpp>

#include <utils/io.hpp>
#include <utils/compression.hpp>

#include <algorithm>
#include <cstring>

namespace sogen
{

    namespace apiset
    {
        namespace
        {
            uint64_t copy_string(x86_64_emulator& emu, emulator_allocator& allocator, const void* base_ptr, const uint64_t offset,
                                 const size_t length)
            {
                if (!length)
                {
                    return 0;
                }

                const auto length_to_allocate = length + 2;
                const auto str_obj = allocator.reserve(length_to_allocate);
                emu.write_memory(str_obj, static_cast<const uint8_t*>(base_ptr) + offset, length);

                return str_obj;
            }

            ULONG copy_string_as_relative(x86_64_emulator& emu, emulator_allocator& allocator, const uint64_t result_base,
                                          const void* base_ptr, const uint64_t offset, const size_t length)
            {
                const auto address = copy_string(emu, allocator, base_ptr, offset, length);
                if (!address)
                {
                    return 0;
                }

                assert(address > result_base);
                return static_cast<ULONG>(address - result_base);
            }

            std::vector<std::byte> decompress_apiset(const std::vector<std::byte>& apiset)
            {
                auto buffer = utils::compression::zstd::decompress(apiset);
                if (buffer.empty())
                {
                    throw std::runtime_error("Failed to decompress API-SET");
                }

                return buffer;
            }

            struct extra_entry
            {
                std::u16string_view name;
                std::u16string_view value;
            };

            static constexpr extra_entry k_gdi_forwarders[] = {
                {u"ext-ms-win-gdi-internal-desktop-l1-1-0", u"gdi32full.dll"},
                {u"ext-ms-win-gdi-internal-desktop-l1-1-1", u"gdi32full.dll"},
                {u"ext-ms-win-gdi-internal-desktop-l1-1-2", u"gdi32full.dll"},
                {u"ext-ms-win-gdi-internal-desktop-l1-1-3", u"gdi32full.dll"},
                {u"ext-ms-win-gdi-dc-l1-2-0", u"gdi32full.dll"},
                {u"ext-ms-win-gdi-dc-create-l1-1-0", u"gdi32full.dll"},
                {u"ext-ms-win-gdi-draw-l1-1-0", u"gdi32full.dll"},
                {u"ext-ms-win-gdi-draw-l1-1-1", u"gdi32full.dll"},
                {u"ext-ms-win-gdi-draw-l1-1-2", u"gdi32full.dll"},
                {u"ext-ms-win-gdi-font-l1-1-0", u"gdi32full.dll"},
                {u"ext-ms-win-gdi-font-l1-1-1", u"gdi32full.dll"},
                {u"ext-ms-win-gdi-font-l1-1-2", u"gdi32full.dll"},
                {u"ext-ms-win-gdi-metafile-l1-1-0", u"gdi32full.dll"},
                {u"ext-ms-win-gdi-metafile-l1-1-1", u"gdi32full.dll"},
                {u"ext-ms-win-dx-d3dkmt-dxcore-l1-1-0", u"dxcore.dll"},
            };

            // Returns hashed byte length for an API set name: bytes up to (not including) the last
            // '-' separator and version digit.
            // E.g. "ext-ms-win-gdi-draw-l1-1-3" → hashed_length = len("ext-ms-win-gdi-draw-l1-1") * 2 = 48
            static ULONG hashed_byte_length(std::u16string_view name)
            {
                const auto last_dash = name.rfind(u'-');
                const auto hashed_chars = (last_dash != std::u16string_view::npos) ? last_dash : name.size();
                return static_cast<ULONG>(hashed_chars * 2);
            }

            static ULONG compute_apiset_hash(std::u16string_view name, ULONG hashed_bytes, ULONG factor)
            {
                ULONG h = 0;
                for (size_t i = 0; i < hashed_bytes / 2; ++i)
                {
                    h = h * factor + static_cast<ULONG>(name[i]);
                }
                return h;
            }

            // Rebuilds the API_SET_NAMESPACE binary with additional entries injected.
            // Only entries not already present in the binary are added.
            static void augment_apiset(std::vector<std::byte>& data, std::span<const extra_entry> extras)
            {
                constexpr ULONG entry_size = sizeof(API_SET_NAMESPACE_ENTRY);
                constexpr ULONG value_size = sizeof(API_SET_VALUE_ENTRY);
                constexpr ULONG hash_size = sizeof(API_SET_HASH_ENTRY);
                constexpr ULONG header_size = sizeof(API_SET_NAMESPACE);

                const auto& ns = *reinterpret_cast<const API_SET_NAMESPACE*>(data.data());
                const ULONG orig_count = ns.Count;
                const ULONG entry_offset = ns.EntryOffset;
                const ULONG hash_offset = ns.HashOffset;
                const ULONG hash_factor = ns.HashFactor;
                const ULONG version = ns.Version;

                std::unordered_set<std::u16string> existing;
                for (ULONG i = 0; i < orig_count; ++i)
                {
                    const auto& e = *reinterpret_cast<const API_SET_NAMESPACE_ENTRY*>(data.data() + entry_offset + i * entry_size);
                    const auto* name_ptr = reinterpret_cast<const char16_t*>(data.data() + e.NameOffset);
                    existing.emplace(name_ptr, e.NameLength / 2);
                }

                std::vector<const extra_entry*> to_add;
                for (const auto& ex : extras)
                {
                    if (!existing.count(std::u16string(ex.name)))
                    {
                        to_add.push_back(&ex);
                    }
                }
                if (to_add.empty())
                {
                    return;
                }

                const ULONG n_extra = static_cast<ULONG>(to_add.size());
                const ULONG new_count = orig_count + n_extra;

                // Rebuild binary. Layout:
                //   header  (header_size)
                //   entry array  (new_count * entry_size)
                //   string+value data for all entries
                //   hash array  (new_count * hash_size, sorted)

                std::vector<std::byte> out;
                out.reserve(data.size() + n_extra * (entry_size + value_size + 80));

                out.resize(header_size, std::byte{0});

                const ULONG new_entry_offset = static_cast<ULONG>(out.size());
                out.resize(new_entry_offset + new_count * entry_size, std::byte{0});

                auto append_u16str = [&](const char16_t* str, ULONG char_count) -> ULONG {
                    const ULONG off = static_cast<ULONG>(out.size());
                    const auto* bytes = reinterpret_cast<const std::byte*>(str);
                    out.insert(out.end(), bytes, bytes + char_count * 2);
                    out.push_back(std::byte{0});
                    out.push_back(std::byte{0});
                    return off;
                };

                std::vector<API_SET_NAMESPACE_ENTRY> entry_records(new_count);
                std::vector<std::pair<ULONG, ULONG>> hash_pairs;
                hash_pairs.reserve(new_count);

                // Build reverse map: entry_index -> hash
                std::vector<ULONG> entry_hashes(orig_count);
                for (ULONG i = 0; i < orig_count; ++i)
                {
                    const auto& he = *reinterpret_cast<const API_SET_HASH_ENTRY*>(data.data() + hash_offset + i * hash_size);
                    entry_hashes[he.Index] = he.Hash;
                }

                for (ULONG i = 0; i < orig_count; ++i)
                {
                    const auto& src = *reinterpret_cast<const API_SET_NAMESPACE_ENTRY*>(data.data() + entry_offset + i * entry_size);
                    auto& dst = entry_records[i];

                    dst.Flags = src.Flags;
                    dst.HashedLength = src.HashedLength;
                    dst.NameLength = src.NameLength;
                    dst.NameOffset = append_u16str(reinterpret_cast<const char16_t*>(data.data() + src.NameOffset), src.NameLength / 2);

                    hash_pairs.push_back({entry_hashes[i], i});

                    if (!src.ValueCount)
                    {
                        dst.ValueOffset = 0;
                        dst.ValueCount = 0;
                        continue;
                    }

                    dst.ValueOffset = static_cast<ULONG>(out.size());
                    dst.ValueCount = src.ValueCount;
                    out.resize(dst.ValueOffset + src.ValueCount * value_size, std::byte{0});

                    for (ULONG j = 0; j < src.ValueCount; ++j)
                    {
                        const auto& sv = *reinterpret_cast<const API_SET_VALUE_ENTRY*>(data.data() + src.ValueOffset + j * value_size);
                        API_SET_VALUE_ENTRY dv{};
                        dv.Flags = sv.Flags;
                        if (sv.ValueLength)
                        {
                            dv.ValueOffset =
                                append_u16str(reinterpret_cast<const char16_t*>(data.data() + sv.ValueOffset), sv.ValueLength / 2);
                            dv.ValueLength = sv.ValueLength;
                        }
                        if (sv.NameLength)
                        {
                            dv.NameOffset =
                                append_u16str(reinterpret_cast<const char16_t*>(data.data() + sv.NameOffset), sv.NameLength / 2);
                            dv.NameLength = sv.NameLength;
                        }
                        std::memcpy(out.data() + dst.ValueOffset + j * value_size, &dv, value_size);
                    }
                }

                for (ULONG i = 0; i < n_extra; ++i)
                {
                    const ULONG idx = orig_count + i;
                    const auto& ex = *to_add[i];
                    auto& dst = entry_records[idx];

                    dst.Flags = 0;
                    dst.NameLength = static_cast<ULONG>(ex.name.size() * 2);
                    dst.HashedLength = hashed_byte_length(ex.name);
                    dst.NameOffset = append_u16str(ex.name.data(), static_cast<ULONG>(ex.name.size()));

                    hash_pairs.push_back({compute_apiset_hash(ex.name, dst.HashedLength, hash_factor), idx});

                    dst.ValueOffset = static_cast<ULONG>(out.size());
                    dst.ValueCount = 1;
                    out.resize(dst.ValueOffset + value_size, std::byte{0});

                    API_SET_VALUE_ENTRY ve{};
                    ve.ValueLength = static_cast<ULONG>(ex.value.size() * 2);
                    ve.ValueOffset = append_u16str(ex.value.data(), static_cast<ULONG>(ex.value.size()));
                    std::memcpy(out.data() + dst.ValueOffset, &ve, value_size);
                }

                const ULONG new_hash_offset = static_cast<ULONG>(out.size());
                out.resize(new_hash_offset + new_count * hash_size, std::byte{0});

                std::sort(hash_pairs.begin(), hash_pairs.end());
                for (ULONG i = 0; i < new_count; ++i)
                {
                    API_SET_HASH_ENTRY he{.Hash = hash_pairs[i].first, .Index = hash_pairs[i].second};
                    std::memcpy(out.data() + new_hash_offset + i * hash_size, &he, hash_size);
                }

                for (ULONG i = 0; i < new_count; ++i)
                {
                    std::memcpy(out.data() + new_entry_offset + i * entry_size, &entry_records[i], entry_size);
                }

                const ULONG total_size = static_cast<ULONG>(out.size());
                API_SET_NAMESPACE new_ns{};
                new_ns.Version = version;
                new_ns.Size = total_size;
                new_ns.Flags = 0;
                new_ns.Count = new_count;
                new_ns.EntryOffset = new_entry_offset;
                new_ns.HashOffset = new_hash_offset;
                new_ns.HashFactor = hash_factor;
                std::memcpy(out.data(), &new_ns, header_size);

                data = std::move(out);
            }

            std::vector<std::byte> obtain_data(const location location, const std::filesystem::path& root)
            {
                switch (location)
                {
#ifdef OS_WINDOWS_64
                case location::host: {
                    const auto* teb = NtCurrentTeb64();
                    const auto* peb = reinterpret_cast<PEB64*>(teb->ProcessEnvironmentBlock);
                    const auto* api_set_map = reinterpret_cast<const API_SET_NAMESPACE*>(peb->ApiSetMap);
                    const auto* data_ptr = reinterpret_cast<const std::byte*>(api_set_map);
                    return {data_ptr, data_ptr + api_set_map->Size};
                }
#else
                case location::host:
                    throw std::runtime_error("The APISET host location is not supported on this platform");
#endif
                case location::file: {
                    const auto apiset = utils::io::read_file(root / "api-set.bin");
                    if (apiset.empty())
                    {
                        throw std::runtime_error("Failed to read file api-set.bin");
                    }

                    return decompress_apiset(apiset);
                }
                case location::default_windows_10: {
                    const auto* byte_apiset = reinterpret_cast<const std::byte*>(apiset_w10);
                    const std::vector<std::byte> apiset{byte_apiset, byte_apiset + sizeof(apiset_w10)};
                    return decompress_apiset(apiset);
                }
                case location::default_windows_11: {
                    const auto* byte_apiset = reinterpret_cast<const std::byte*>(apiset_w11);
                    const std::vector<std::byte> apiset{byte_apiset, byte_apiset + sizeof(apiset_w11)};
                    return decompress_apiset(apiset);
                }
                default:
                    throw std::runtime_error("Bad API set location");
                }
            }
        }

        container obtain(const location location, const std::filesystem::path& root)
        {
            auto data = obtain_data(location, root);
            if (location != location::host)
            {
                augment_apiset(data, k_gdi_forwarders);
            }
            return {.data = std::move(data)};
        }

        container obtain(const std::filesystem::path& root)
        {
            auto apiset_loc = location::file;

            if (root.empty())
            {
#ifdef OS_WINDOWS
                apiset_loc = location::host;
#else
                apiset_loc = location::default_windows_11;
#endif
            }

            return obtain(apiset_loc, root);
        }

        emulator_object<API_SET_NAMESPACE> clone(x86_64_emulator& emu, emulator_allocator& allocator, const container& container)
        {
            return clone(emu, allocator, container.get());
        }

        emulator_object<API_SET_NAMESPACE> clone(x86_64_emulator& emu, emulator_allocator& allocator,
                                                 const API_SET_NAMESPACE& orig_api_set_map)
        {
            const auto api_set_map_obj = allocator.reserve<API_SET_NAMESPACE>();
            const auto ns_entries_obj = allocator.reserve<API_SET_NAMESPACE_ENTRY>(orig_api_set_map.Count);
            const auto hash_entries_obj = allocator.reserve<API_SET_HASH_ENTRY>(orig_api_set_map.Count);

            api_set_map_obj.access([&](API_SET_NAMESPACE& api_set) {
                api_set = orig_api_set_map;
                api_set.EntryOffset = static_cast<ULONG>(ns_entries_obj.value() - api_set_map_obj.value());
                api_set.HashOffset = static_cast<ULONG>(hash_entries_obj.value() - api_set_map_obj.value());
            });

            const auto* orig_ns_entries = offset_pointer<API_SET_NAMESPACE_ENTRY>(&orig_api_set_map, orig_api_set_map.EntryOffset);
            const auto* orig_hash_entries = offset_pointer<API_SET_HASH_ENTRY>(&orig_api_set_map, orig_api_set_map.HashOffset);

            for (ULONG i = 0; i < orig_api_set_map.Count; ++i)
            {
                auto ns_entry = orig_ns_entries[i];
                const auto hash_entry = orig_hash_entries[i];

                ns_entry.NameOffset = copy_string_as_relative(emu, allocator, api_set_map_obj.value(), &orig_api_set_map,
                                                              ns_entry.NameOffset, ns_entry.NameLength);

                if (!ns_entry.ValueCount)
                {
                    continue;
                }

                const auto values_obj = allocator.reserve<API_SET_VALUE_ENTRY>(ns_entry.ValueCount);
                const auto* orig_values = offset_pointer<API_SET_VALUE_ENTRY>(&orig_api_set_map, ns_entry.ValueOffset);

                ns_entry.ValueOffset = static_cast<ULONG>(values_obj.value() - api_set_map_obj.value());

                for (ULONG j = 0; j < ns_entry.ValueCount; ++j)
                {
                    auto value = orig_values[j];

                    value.ValueOffset = copy_string_as_relative(emu, allocator, api_set_map_obj.value(), &orig_api_set_map,
                                                                value.ValueOffset, value.ValueLength);

                    if (value.NameLength)
                    {
                        value.NameOffset = copy_string_as_relative(emu, allocator, api_set_map_obj.value(), &orig_api_set_map,
                                                                   value.NameOffset, value.NameLength);
                    }

                    values_obj.write(value, j);
                }

                ns_entries_obj.write(ns_entry, i);
                hash_entries_obj.write(hash_entry, i);
            }

            return api_set_map_obj;
        }

        apiset_map get_namespace_table(const API_SET_NAMESPACE* apiset_ns_data)
        {
            std::map<std::u16string, std::u16string> apiset;

            for (size_t i = 0; i < apiset_ns_data->Count; i++)
            {
                const auto* entry = reinterpret_cast<const API_SET_NAMESPACE_ENTRY*>(
                    reinterpret_cast<uint64_t>(apiset_ns_data) + apiset_ns_data->EntryOffset + i * sizeof(API_SET_NAMESPACE_ENTRY));

                std::u16string name(reinterpret_cast<const char16_t*>(reinterpret_cast<uint64_t>(apiset_ns_data) + entry->NameOffset),
                                    entry->NameLength / sizeof(char16_t));

                if (!entry->ValueCount)
                {
                    continue;
                }

                const auto* value =
                    reinterpret_cast<const API_SET_VALUE_ENTRY*>(reinterpret_cast<uint64_t>(apiset_ns_data) + entry->ValueOffset +
                                                                 (entry->ValueCount - 1) * sizeof(API_SET_VALUE_ENTRY));
                std::u16string base_name(reinterpret_cast<const char16_t*>(reinterpret_cast<uint64_t>(apiset_ns_data) + value->ValueOffset),
                                         value->ValueLength / sizeof(char16_t));

                apiset[name + u".dll"] = base_name;
            }

            return apiset;
        }
    }

} // namespace sogen
