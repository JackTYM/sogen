#include "../std_include.hpp"
#include "../emulator_utils.hpp"
#include "../syscall_utils.hpp"

#include <limits>
#include <map>
#include <set>

namespace sogen
{

    namespace syscalls
    {
        namespace
        {
            constexpr uint32_t token_authentication_id = 0x1000;
            constexpr uint32_t token_id = 0x1001;

            // Windows 11 26200 answers classes up to 0x3A, past the last one TOKEN_INFORMATION_CLASS names.
            constexpr ULONG last_token_information_class = 0x3A;

            constexpr std::array<uint8_t, 12> interactive_sid = {
                0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x04, 0x00, 0x00, 0x00,
            };

            // S-1-5-5-0-1000: a synthetic logon-session SID (SECURITY_LOGON_IDS_RID); real Windows
            // derives the trailing two sub-authorities from the token's actual logon LUID, which sogen
            // does not track, so a fixed, stable value is used instead.
            constexpr std::array<uint8_t, 20> logon_sid = {
                0x01, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xE8, 0x03, 0x00, 0x00,
            };

            constexpr std::array<uint8_t, 12> anonymous_sid{1, 1, 0, 0, 0, 0, 0, 5, 7, 0, 0, 0};

            // S-1-5-32-544, BUILTIN\Administrators
            constexpr std::array<uint8_t, 16> administrators_sid = {
                0x01, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x20, 0x00, 0x00, 0x00, 0x20, 0x02, 0x00, 0x00,
            };

            constexpr ULONG se_group_enabled = 0x7; // SE_GROUP_MANDATORY | SE_GROUP_ENABLED_BY_DEFAULT | SE_GROUP_ENABLED
            constexpr ULONG se_group_owner = 0x8;
            constexpr NTSTATUS status_no_such_logon_session = 0xC000005F;
            constexpr ULONG se_group_logon_id = 0xC0000000;
            constexpr std::array<uint8_t, 12> everyone_sid = {
                0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
            };

            struct token_group
            {
                std::span<const uint8_t> sid;
                ULONG attributes;
            };

            // A LocalSystem token carries the Administrators group and is elevated, like a token started with --elevated.
            bool is_elevated_token(const syscall_context& c, const handle token_handle)
            {
                return token_handle != ANONYMOUS_IMPERSONATION_TOKEN && (c.win_emu.runs_elevated() || c.win_emu.runs_as_local_system());
            }

            struct created_group
            {
                std::vector<uint8_t> sid;
                ULONG attributes;
            };

            std::map<uint64_t, std::vector<created_group>>& created_token_groups()
            {
                static std::map<uint64_t, std::vector<created_group>> groups;
                return groups;
            }

            std::vector<token_group> get_token_groups(const syscall_context& c, const handle token_handle)
            {
                if (token_handle == ANONYMOUS_IMPERSONATION_TOKEN)
                {
                    return {};
                }

                if (const auto it = created_token_groups().find(token_handle.bits); it != created_token_groups().end())
                {
                    std::vector<token_group> stored;
                    for (const auto& g : it->second)
                    {
                        stored.push_back(token_group{.sid = g.sid, .attributes = g.attributes});
                    }
                    return stored;
                }

                std::vector<token_group> groups{
                    token_group{.sid = c.proc.sid, .attributes = 0},
                    token_group{.sid = everyone_sid, .attributes = se_group_enabled},
                    token_group{.sid = interactive_sid, .attributes = se_group_enabled},
                };

                if (is_elevated_token(c, token_handle))
                {
                    groups.push_back(token_group{.sid = administrators_sid, .attributes = se_group_enabled | se_group_owner});
                }

                groups.push_back(token_group{.sid = logon_sid, .attributes = se_group_enabled | se_group_logon_id});
                return groups;
            }

            LUID token_logon_id(const process_context& proc)
            {
                if (proc.authentication_id.LowPart != 0 || proc.authentication_id.HighPart != 0)
                {
                    return proc.authentication_id;
                }

                return LUID{.LowPart = token_authentication_id, .HighPart = 0};
            }

            handle resolve_token(const syscall_context& c, const handle token)
            {
                if (token == CURRENT_THREAD_TOKEN || token == CURRENT_THREAD_EFFECTIVE_TOKEN)
                {
                    const auto impersonation = c.vcpu.active_thread ? c.vcpu.active_thread->impersonation_token : handle{};
                    if (impersonation != 0 || token == CURRENT_THREAD_TOKEN)
                    {
                        return impersonation;
                    }
                    return CURRENT_PROCESS_TOKEN;
                }
                return token;
            }

            bool sid_matches(const memory_manager& memory, const uint64_t address, const std::span<const uint8_t> expected)
            {
                std::array<uint8_t, 8> header{};
                memory.read_memory(address, header.data(), header.size());
                const auto size = 8 + static_cast<size_t>(header[1]) * sizeof(uint32_t);
                if (size != expected.size())
                {
                    return false;
                }

                std::vector<uint8_t> sid(size);
                memory.read_memory(address, sid.data(), sid.size());
                return std::ranges::equal(sid, expected);
            }

            ACCESS_MASK map_generic_access(ACCESS_MASK access, const EMU_GENERIC_MAPPING& mapping)
            {
                constexpr ACCESS_MASK generic_read = 0x80000000;
                constexpr ACCESS_MASK generic_write = 0x40000000;
                constexpr ACCESS_MASK generic_execute = 0x20000000;
                constexpr ACCESS_MASK generic_all = 0x10000000;

                if (access & generic_read)
                {
                    access = (access & ~generic_read) | mapping.GenericRead;
                }
                if (access & generic_write)
                {
                    access = (access & ~generic_write) | mapping.GenericWrite;
                }
                if (access & generic_execute)
                {
                    access = (access & ~generic_execute) | mapping.GenericExecute;
                }
                if (access & generic_all)
                {
                    access = (access & ~generic_all) | mapping.GenericAll;
                }
                return access;
            }

            bool is_recognized_token_handle(process_context& proc, const handle token_handle)
            {
                return token_handle == CURRENT_PROCESS_TOKEN || token_handle == CURRENT_THREAD_TOKEN ||
                       token_handle == CURRENT_THREAD_EFFECTIVE_TOKEN || token_handle == DUMMY_IMPERSONATION_TOKEN ||
                       token_handle == ANONYMOUS_IMPERSONATION_TOKEN || proc.tokens.get(token_handle) != nullptr;
            }

            constexpr ULONG se_privilege_enabled = 0x2;
            constexpr ULONG se_privilege_removed = 0x4;
            constexpr ULONG se_privilege_used_for_access = 0x80000000;

            uint64_t privilege_list_size(const size_t count)
            {
                return offsetof(TOKEN_PRIVILEGES64, Privileges) + count * sizeof(LUID_AND_ATTRIBUTES);
            }

            void write_privilege_list(const syscall_context& c, const uint64_t address, const std::vector<token_privilege>& privileges)
            {
                c.emu.write_memory<ULONG>(address, static_cast<ULONG>(privileges.size()));
                for (size_t i = 0; i < privileges.size(); ++i)
                {
                    LUID_AND_ATTRIBUTES entry{};
                    entry.Luid.LowPart = privileges[i].luid;
                    entry.Attributes = privileges[i].attributes;
                    c.emu.write_memory(address + offsetof(TOKEN_PRIVILEGES64, Privileges) + i * sizeof(LUID_AND_ATTRIBUTES), entry);
                }
            }

            std::vector<token_privilege>::iterator find_privilege(std::vector<token_privilege>& privileges, const LUID luid)
            {
                if (luid.HighPart != 0)
                {
                    return privileges.end();
                }

                return std::ranges::find(privileges, luid.LowPart, &token_privilege::luid);
            }

            std::vector<token_privilege>& token_privileges(process_context& proc, const handle token_handle)
            {
                auto* const token = proc.tokens.get(token_handle);
                return token && token->privileges ? *token->privileges : proc.privileges;
            }

            bool holds_enabled_privilege(const std::vector<token_privilege>& privileges, const LUID luid)
            {
                return std::ranges::any_of(privileges, [&](const token_privilege& held) {
                    return held.luid == luid.LowPart && luid.HighPart == 0 && (held.attributes & se_privilege_enabled) != 0;
                });
            }

            struct access_decision
            {
                NTSTATUS status{};
                ACCESS_MASK granted{};
            };

            access_decision evaluate_descriptor_access(const syscall_context& c, const handle client_token,
                                                       const uint64_t security_descriptor, const ACCESS_MASK desired_access,
                                                       const EMU_GENERIC_MAPPING& mapping)
            {
                const auto control = emulator_object<SECURITY_DESCRIPTOR_CONTROL>{c.emu, security_descriptor + 2}.read();
                uint64_t dacl_address = 0;
                if (control & SE_SELF_RELATIVE)
                {
                    const auto descriptor = emulator_object<SECURITY_DESCRIPTOR_RELATIVE>{c.emu, security_descriptor}.read();
                    dacl_address = descriptor.Dacl == 0 ? 0 : security_descriptor + descriptor.Dacl;
                }
                else
                {
                    dacl_address = emulator_object<uint64_t>{c.emu, security_descriptor + 32}.read();
                }

                const auto user_sid = client_token == ANONYMOUS_IMPERSONATION_TOKEN ? std::span<const uint8_t>{anonymous_sid}
                                                                                    : std::span<const uint8_t>{c.proc.sid};
                const auto token_groups = get_token_groups(c, client_token);
                constexpr ACCESS_MASK maximum_allowed = 0x02000000;
                constexpr ACCESS_MASK all_access = 0xFFFFFFFF;
                const bool maximum = (desired_access & maximum_allowed) != 0;
                const auto requested = map_generic_access(desired_access & ~maximum_allowed, mapping);
                ACCESS_MASK remaining = maximum ? all_access : requested;
                ACCESS_MASK granted = 0;
                bool denied = false;

                if (!(control & SE_DACL_PRESENT) || dacl_address == 0)
                {
                    granted = maximum ? map_generic_access(0x10000000, mapping) : requested;
                    remaining = 0;
                }
                else
                {
                    const auto acl = emulator_object<ACL>{c.emu, dacl_address}.read();
                    auto ace_address = dacl_address + sizeof(ACL);
                    for (USHORT i = 0; i < acl.AceCount; ++i)
                    {
                        const auto ace = emulator_object<ACCESS_ALLOWED_ACE>{c.emu, ace_address}.read();
                        if (ace.Header.AceSize < sizeof(ACCESS_ALLOWED_ACE))
                        {
                            return {.status = STATUS_INVALID_PARAMETER};
                        }

                        const auto ace_mask = map_generic_access(ace.Mask, mapping);
                        const auto ace_sid = ace_address + offsetof(ACCESS_ALLOWED_ACE, SidStart);
                        const bool applies = sid_matches(c.emu.memory(), ace_sid, user_sid) ||
                                             std::ranges::any_of(token_groups, [&](const token_group& group) {
                                                 return (group.attributes & se_group_enabled) == se_group_enabled &&
                                                        sid_matches(c.emu.memory(), ace_sid, group.sid);
                                             });
                        if (applies && ace.Header.AceType == 1 && maximum)
                        {
                            remaining &= ~ace_mask;
                        }
                        else if (applies && ace.Header.AceType == 1 && (ace_mask & remaining) != 0)
                        {
                            denied = true;
                            break;
                        }
                        if (applies && ace.Header.AceType == 0)
                        {
                            const auto allowed = ace_mask & remaining;
                            granted |= allowed;
                            remaining &= ~allowed;
                        }
                        ace_address += ace.Header.AceSize;
                    }
                }

                const bool success = !denied && (maximum ? granted != 0 : remaining == 0);
                return {.status = success ? STATUS_SUCCESS : STATUS_ACCESS_DENIED, .granted = granted};
            }

            // The caller believes the returned handle is distinct from existing_token_handle and will
            // close both independently; since it reuses the same numeric value here, its object needs
            // an extra reference or the first CloseHandle erases it out from under the second.
            void retain_existing_token(process_context& proc, const handle existing_token_handle)
            {
                auto* token = proc.tokens.get(existing_token_handle);
                if (token)
                {
                    ++token->ref_count;
                }
            }
        }

        TOKEN_TYPE get_token_type(const handle token_handle)
        {
            const auto impersonation = token_handle == DUMMY_IMPERSONATION_TOKEN || token_handle == ANONYMOUS_IMPERSONATION_TOKEN;
            return impersonation ? TokenImpersonation : TokenPrimary;
        }

        NTSTATUS handle_NtDuplicateToken(const syscall_context& c, const handle requested_token, ACCESS_MASK /*desired_access*/,
                                         const emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>>
                                         /*object_attributes*/,
                                         const BOOLEAN /*effective_only*/, const TOKEN_TYPE type,
                                         const emulator_object<handle> new_token_handle)
        {
            const auto existing_token_handle = resolve_token(c, requested_token);
            if (existing_token_handle != CURRENT_PROCESS_TOKEN && existing_token_handle != DUMMY_IMPERSONATION_TOKEN &&
                existing_token_handle != ANONYMOUS_IMPERSONATION_TOKEN && c.proc.tokens.get(existing_token_handle) == nullptr)
            {
                return existing_token_handle != 0 ? STATUS_INVALID_HANDLE : STATUS_NO_TOKEN;
            }
            if (type != TokenPrimary && type != TokenImpersonation)
            {
                return STATUS_INVALID_PARAMETER;
            }
            if (existing_token_handle == ANONYMOUS_IMPERSONATION_TOKEN && type == TokenPrimary)
            {
                return STATUS_NOT_SUPPORTED;
            }
            if (get_token_type(existing_token_handle) == type)
            {
                retain_existing_token(c.proc, existing_token_handle);
                new_token_handle.write(existing_token_handle);
            }
            else if (type == TokenPrimary)
            {
                new_token_handle.write(CURRENT_PROCESS_TOKEN);
            }
            else
            {
                new_token_handle.write(DUMMY_IMPERSONATION_TOKEN);
            }

            return STATUS_SUCCESS;
        }

        // Tokens another process handed over (a service logon token) are tracked apart from tokens the process made itself (a restricted sandbox token).
        namespace
        {
            std::set<uint64_t>& adopted_tokens()
            {
                static std::set<uint64_t> tokens;
                return tokens;
            }
        }

        void mark_adopted_token(const uint64_t handle_bits)
        {
            adopted_tokens().insert(handle_bits);
        }

        bool is_adopted_or_process_token(const uint64_t handle_bits)
        {
            return handle_bits == CURRENT_PROCESS_TOKEN.bits || adopted_tokens().contains(handle_bits);
        }

        std::vector<uint8_t> export_created_token_groups(const uint64_t handle_bits)
        {
            std::vector<uint8_t> out;
            const auto it = created_token_groups().find(handle_bits);
            if (it == created_token_groups().end())
            {
                return out;
            }
            const auto put = [&](const uint32_t v) {
                const auto* p = reinterpret_cast<const uint8_t*>(&v);
                out.insert(out.end(), p, p + sizeof(v));
            };
            put(static_cast<uint32_t>(it->second.size()));
            for (const auto& g : it->second)
            {
                put(g.attributes);
                put(static_cast<uint32_t>(g.sid.size()));
                out.insert(out.end(), g.sid.begin(), g.sid.end());
            }
            return out;
        }

        void import_created_token_groups(const uint64_t handle_bits, const std::vector<uint8_t>& in)
        {
            if (in.size() < 4)
            {
                return;
            }
            size_t off = 0;
            const auto get = [&]() {
                uint32_t v{};
                std::memcpy(&v, in.data() + off, sizeof(v));
                off += sizeof(v);
                return v;
            };
            const auto count = get();
            std::vector<created_group> list;
            for (uint32_t i = 0; i < count && off + 8 <= in.size(); ++i)
            {
                const auto attributes = get();
                const auto len = get();
                if (off + len > in.size())
                {
                    return;
                }
                list.push_back({.sid = std::vector<uint8_t>(in.begin() + off, in.begin() + off + len), .attributes = attributes});
                off += len;
            }
            created_token_groups()[handle_bits] = std::move(list);
        }

        NTSTATUS handle_NtCreateTokenEx(const syscall_context& c, const emulator_object<handle> token_handle,
                                        ACCESS_MASK /*desired_access*/,
                                        const emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>>
                                        /*object_attributes*/,
                                        const TOKEN_TYPE type, uint64_t /*authentication_id*/, uint64_t /*expiration_time*/,
                                        uint64_t /*user*/, uint64_t groups)
        {
            if (type == TokenPrimary)
            {
                process_token token{};
                token.privileges = c.proc.privileges;
                const auto stored_handle = c.proc.tokens.store(std::move(token));
                if (groups)
                {
                    ULONG count{};
                    c.emu.memory().read_memory(groups, &count, sizeof(count));
                    std::vector<created_group> list;
                    for (ULONG i = 0; i < count && i < 64; ++i)
                    {
                        SID_AND_ATTRIBUTES64 entry{};
                        c.emu.memory().read_memory(groups + offsetof(TOKEN_GROUPS64, Groups) + i * sizeof(SID_AND_ATTRIBUTES64), &entry,
                                                   sizeof(entry));
                        std::array<uint8_t, 8> header{};
                        c.emu.memory().read_memory(entry.Sid, header.data(), header.size());
                        std::vector<uint8_t> sid(8 + static_cast<size_t>(header[1]) * sizeof(uint32_t));
                        c.emu.memory().read_memory(entry.Sid, sid.data(), sid.size());
                        const bool is_logon = (entry.Attributes & se_group_logon_id) == se_group_logon_id;
                        list.push_back({.sid = is_logon ? std::vector<uint8_t>(logon_sid.begin(), logon_sid.end()) : std::move(sid),
                                        .attributes = entry.Attributes});
                    }
                    created_token_groups()[stored_handle.bits] = std::move(list);
                }
                token_handle.write(stored_handle);
            }
            else if (type == TokenImpersonation)
            {
                token_handle.write(DUMMY_IMPERSONATION_TOKEN);
            }
            else
            {
                return STATUS_INVALID_PARAMETER;
            }

            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtFilterToken(const syscall_context& c, const handle existing_token_handle, ULONG /*flags*/,
                                      uint64_t /*sids_to_disable*/, uint64_t /*privileges_to_delete*/, uint64_t /*restricted_sids*/,
                                      const emulator_object<handle> new_token_handle)
        {
            // sogen doesn't model per-handle SID/privilege restrictions, so the filtered token is
            // just the same recognized identity as the source token.
            retain_existing_token(c.proc, existing_token_handle);
            new_token_handle.write(existing_token_handle);
            return STATUS_SUCCESS;
        }

        NTSTATUS query_token_information_impl(const syscall_context& c, const handle requested_token,
                                                const TOKEN_INFORMATION_CLASS token_information_class, const uint64_t token_information,
                                                const ULONG token_information_length, const emulator_object<ULONG> return_length);
        NTSTATUS handle_NtQueryInformationToken(const syscall_context& c, const handle requested_token,
                                                const TOKEN_INFORMATION_CLASS token_information_class, const uint64_t token_information,
                                                const ULONG token_information_length, const emulator_object<ULONG> return_length)
        {
            const auto status = query_token_information_impl(c, requested_token, token_information_class, token_information, token_information_length, return_length);
            fprintf(stderr, "V6TOKQUERY host=%d pid=%u handle=%llx class=%d len=%u status=%x\n", static_cast<int>(getpid()), c.proc.process_id, static_cast<unsigned long long>(requested_token.bits), static_cast<int>(token_information_class), token_information_length, static_cast<unsigned>(status));
            return status;
        }

        NTSTATUS query_token_information_impl(const syscall_context& c, const handle requested_token,
                                                const TOKEN_INFORMATION_CLASS token_information_class, const uint64_t token_information,
                                                const ULONG token_information_length, const emulator_object<ULONG> return_length)
        {
            if (token_information_class < TokenUser || static_cast<ULONG>(token_information_class) > last_token_information_class)
            {
                c.win_emu.log.error("Unsupported NtQueryInformationToken info class: 0x%X\n",
                                    static_cast<uint32_t>(token_information_class));
                return STATUS_INVALID_INFO_CLASS;
            }

            const auto token_handle = resolve_token(c, requested_token);
            if (token_handle == 0 && requested_token == CURRENT_THREAD_TOKEN)
            {
                return STATUS_NO_TOKEN;
            }
            if (!is_recognized_token_handle(c.proc, token_handle))
            {
                return STATUS_NOT_SUPPORTED;
            }

            if (token_information_class == TokenAppContainerSid)
            {
                constexpr auto required_size = sizeof(EmulatorTraits<Emu64>::PVOID);
                return_length.write_if_valid(required_size);
                if (token_information_length < required_size)
                {
                    return STATUS_BUFFER_TOO_SMALL;
                }
                emulator_object<EmulatorTraits<Emu64>::PVOID>{c.emu, token_information}.write(0);
                return STATUS_SUCCESS;
            }

            const auto anonymous = token_handle == ANONYMOUS_IMPERSONATION_TOKEN;
            const auto sid = anonymous ? std::span<const uint8_t>{anonymous_sid} : std::span<const uint8_t>{c.proc.sid};

            if (token_information_class == TokenUser)
            {
                const auto required_size = sizeof(TOKEN_USER64) + sid.size();
                return_length.write(static_cast<ULONG>(required_size));

                if (required_size > token_information_length)
                {
                    return STATUS_BUFFER_TOO_SMALL;
                }

                TOKEN_USER64 user{};
                user.User.Attributes = 0;
                user.User.Sid = token_information + sizeof(TOKEN_USER64);

                emulator_object<TOKEN_USER64>{c.emu, token_information}.write(user);
                c.emu.write_memory(token_information + sizeof(TOKEN_USER64), sid.data(), sid.size());
                return STATUS_SUCCESS;
            }

            if (token_information_class == TokenGroups)
            {
                if (anonymous)
                {
                    return_length.write(sizeof(ULONG));
                    if (token_information_length < sizeof(ULONG))
                    {
                        return STATUS_BUFFER_TOO_SMALL;
                    }
                    c.emu.write_memory(token_information, ULONG{0});
                    return STATUS_SUCCESS;
                }

                const auto groups = get_token_groups(c, token_handle);
                const auto entries_offset = offsetof(TOKEN_GROUPS64, Groups);
                auto sid_offset = entries_offset + groups.size() * sizeof(SID_AND_ATTRIBUTES64);

                auto required_size = sid_offset;
                for (const auto& group : groups)
                {
                    required_size += group.sid.size();
                }

                return_length.write(static_cast<ULONG>(required_size));

                if (required_size > token_information_length)
                {
                    return STATUS_BUFFER_TOO_SMALL;
                }

                c.emu.write_memory(token_information, static_cast<ULONG>(groups.size()));

                for (size_t i = 0; i < groups.size(); ++i)
                {
                    const SID_AND_ATTRIBUTES64 entry{
                        .Sid = token_information + sid_offset,
                        .Attributes = groups[i].attributes,
                    };

                    c.emu.write_memory(token_information + entries_offset + i * sizeof(SID_AND_ATTRIBUTES64), entry);
                    c.emu.write_memory(entry.Sid, groups[i].sid.data(), groups[i].sid.size());
                    sid_offset += groups[i].sid.size();
                }

                return STATUS_SUCCESS;
            }

            if (token_information_class == TokenLogonSid)
            {
                const auto required_size = sizeof(TOKEN_GROUPS64) + logon_sid.size();
                return_length.write(static_cast<ULONG>(required_size));

                if (required_size > token_information_length)
                {
                    return STATUS_BUFFER_TOO_SMALL;
                }

                TOKEN_GROUPS64 groups{};
                groups.GroupCount = 1;
                groups.Groups[0].Attributes = 0xC0000004; // SE_GROUP_LOGON_ID | SE_GROUP_ENABLED
                groups.Groups[0].Sid = token_information + sizeof(TOKEN_GROUPS64);

                emulator_object<TOKEN_GROUPS64>{c.emu, token_information}.write(groups);
                c.emu.write_memory(groups.Groups[0].Sid, logon_sid);
                return STATUS_SUCCESS;
            }

            if (token_information_class == TokenOwner)
            {
                const auto required_size = sizeof(TOKEN_OWNER64) + sid.size();
                return_length.write(static_cast<ULONG>(required_size));

                if (required_size > token_information_length)
                {
                    return STATUS_BUFFER_TOO_SMALL;
                }

                TOKEN_OWNER64 owner{};
                owner.Owner = token_information + sizeof(TOKEN_OWNER64);

                emulator_object<TOKEN_OWNER64>{c.emu, token_information}.write(owner);
                c.emu.write_memory(token_information + sizeof(TOKEN_OWNER64), sid.data(), sid.size());
                return STATUS_SUCCESS;
            }

            if (token_information_class == TokenPrimaryGroup)
            {
                const auto required_size = sizeof(TOKEN_PRIMARY_GROUP64) + sid.size();
                return_length.write(static_cast<ULONG>(required_size));

                if (required_size > token_information_length)
                {
                    return STATUS_BUFFER_TOO_SMALL;
                }

                TOKEN_PRIMARY_GROUP64 primary_group{};
                primary_group.PrimaryGroup = token_information + sizeof(TOKEN_PRIMARY_GROUP64);

                emulator_object<TOKEN_PRIMARY_GROUP64>{c.emu, token_information}.write(primary_group);
                c.emu.write_memory(token_information + sizeof(TOKEN_PRIMARY_GROUP64), sid.data(), sid.size());
                return STATUS_SUCCESS;
            }

            if (token_information_class == TokenDefaultDacl)
            {
                const auto acl_size = sizeof(ACL) + sizeof(ACCESS_ALLOWED_ACE) + sid.size() - sizeof(ULONG);
                const auto required_size = sizeof(TOKEN_DEFAULT_DACL64) + acl_size;
                return_length.write(static_cast<ULONG>(required_size));

                if (required_size > token_information_length)
                {
                    return STATUS_BUFFER_TOO_SMALL;
                }

                TOKEN_DEFAULT_DACL64 default_dacl{};
                default_dacl.DefaultDacl = token_information + sizeof(TOKEN_DEFAULT_DACL64);

                emulator_object<TOKEN_DEFAULT_DACL64>{c.emu, token_information}.write(default_dacl);

                const auto acl_offset = token_information + sizeof(TOKEN_DEFAULT_DACL64);
                ACL acl{};
                acl.AclRevision = 2; // ACL_REVISION
                acl.Sbz1 = 0;
                acl.AclSize = static_cast<USHORT>(acl_size);
                acl.AceCount = 1;
                acl.Sbz2 = 0;

                c.emu.write_memory(acl_offset, acl);

                const auto ace_offset = acl_offset + sizeof(ACL);
                ACCESS_ALLOWED_ACE ace{};
                ace.Header.AceType = 0; // ACCESS_ALLOWED_ACE_TYPE
                ace.Header.AceFlags = 0;
                ace.Header.AceSize = static_cast<USHORT>(sizeof(ACCESS_ALLOWED_ACE) + sid.size() - sizeof(ULONG));
                ace.Mask = GENERIC_ALL;

                c.emu.write_memory(ace_offset, ace);

                const auto sid_offset = ace_offset + sizeof(ACCESS_ALLOWED_ACE) - sizeof(ULONG);
                c.emu.write_memory(sid_offset, sid.data(), sid.size());

                return STATUS_SUCCESS;
            }

            if (token_information_class == TokenType)
            {
                constexpr auto required_size = sizeof(TOKEN_TYPE);
                return_length.write(required_size);

                if (required_size > token_information_length)
                {
                    return STATUS_BUFFER_TOO_SMALL;
                }

                emulator_object<TOKEN_TYPE>{c.emu, token_information}.write(get_token_type(token_handle));
                return STATUS_SUCCESS;
            }

            if (token_information_class == TokenImpersonationLevel)
            {
                constexpr auto required_size = sizeof(SECURITY_IMPERSONATION_LEVEL);
                return_length.write(required_size);

                if (required_size > token_information_length)
                {
                    return STATUS_BUFFER_TOO_SMALL;
                }

                const auto level =
                    !anonymous && get_token_type(token_handle) == TokenImpersonation ? SecurityImpersonation : SecurityAnonymous;
                emulator_object<SECURITY_IMPERSONATION_LEVEL>{c.emu, token_information}.write(level);
                return STATUS_SUCCESS;
            }

            if (token_information_class == TokenSessionId)
            {
                constexpr auto required_size = sizeof(ULONG);
                return_length.write(required_size);

                if (required_size > token_information_length)
                {
                    return STATUS_BUFFER_TOO_SMALL;
                }

                emulator_object<ULONG>{c.emu, token_information}.write(1);
                return STATUS_SUCCESS;
            }

            if (token_information_class == TokenPrivateNameSpace)
            {
                constexpr auto required_size = sizeof(ULONG);
                return_length.write(required_size);

                if (required_size > token_information_length)
                {
                    return STATUS_BUFFER_TOO_SMALL;
                }

                emulator_object<ULONG>{c.emu, token_information}.write(0);
                return STATUS_SUCCESS;
            }

            if (token_information_class == TokenUIAccess)
            {
                constexpr auto required_size = sizeof(ULONG);
                return_length.write(required_size);

                if (required_size > token_information_length)
                {
                    return STATUS_BUFFER_TOO_SMALL;
                }

                emulator_object<ULONG>{c.emu, token_information}.write(1);
                return STATUS_SUCCESS;
            }

            if (token_information_class == TokenElevation)
            {
                constexpr auto required_size = sizeof(TOKEN_ELEVATION);
                return_length.write(required_size);

                if (required_size > token_information_length)
                {
                    return STATUS_BUFFER_TOO_SMALL;
                }

                c.emu.write_memory(token_information, TOKEN_ELEVATION{
                                                          .TokenIsElevated = is_elevated_token(c, token_handle) ? 1u : 0u,
                                                      });
                return STATUS_SUCCESS;
            }

            if (token_information_class == TokenLinkedToken)
            {
                // Windows reports no linked token for a token without a UAC split.
                return status_no_such_logon_session;
            }

            if (token_information_class == TokenElevationType)
            {
                constexpr auto required_size = sizeof(ULONG);
                return_length.write(required_size);

                if (required_size > token_information_length)
                {
                    return STATUS_BUFFER_TOO_SMALL;
                }

                // TokenElevationTypeDefault: no UAC split token, as for the built-in Administrator (RID 500) the token
                // belongs to.
                emulator_object<ULONG>{c.emu, token_information}.write(1);
                return STATUS_SUCCESS;
            }

            if (token_information_class == TokenPrivileges)
            {
                // base::win::AccessToken::Privileges() hard-CHECKs the buffer against sizeof(TOKEN_PRIVILEGES), so the
                // answer never reports less space than that, even when fewer privileges are held.
                const auto privileges = anonymous ? std::vector<token_privilege>{} : token_privileges(c.proc, token_handle);
                const auto required_size = std::max<uint64_t>(sizeof(TOKEN_PRIVILEGES64), privilege_list_size(privileges.size()));
                return_length.write(static_cast<ULONG>(required_size));

                if (required_size > token_information_length)
                {
                    return STATUS_BUFFER_TOO_SMALL;
                }

                write_privilege_list(c, token_information, privileges);
                return STATUS_SUCCESS;
            }

            if (token_information_class == TokenRestrictedSids)
            {
                constexpr auto required_size = sizeof(ULONG);
                return_length.write(required_size);

                if (required_size > token_information_length)
                {
                    return STATUS_BUFFER_TOO_SMALL;
                }

                emulator_object<ULONG>{c.emu, token_information}.write(0);
                return STATUS_SUCCESS;
            }

            if (token_information_class == TokenSandBoxInert || token_information_class == TokenHasRestrictions ||
                token_information_class == TokenIsAppContainer || token_information_class == TokenIsAppSilo)
            {
                constexpr auto required_size = sizeof(ULONG);
                return_length.write(required_size);

                if (required_size > token_information_length)
                {
                    return STATUS_BUFFER_TOO_SMALL;
                }

                emulator_object<ULONG>{c.emu, token_information}.write(0);
                return STATUS_SUCCESS;
            }

            if (token_information_class == TokenIsSandboxed || token_information_class == TokenIsAppSilo)
            {
                constexpr auto required_size = sizeof(ULONG);
                return_length.write(required_size);

                if (required_size > token_information_length)
                {
                    return STATUS_BUFFER_TOO_SMALL;
                }

                emulator_object<ULONG>{c.emu, token_information}.write(0);
                return STATUS_SUCCESS;
            }

            if (token_information_class == TokenStatistics)
            {
                constexpr auto required_size = sizeof(TOKEN_STATISTICS);
                return_length.write(required_size);

                if (required_size > token_information_length)
                {
                    return STATUS_BUFFER_TOO_SMALL;
                }

                // NOLINTNEXTLINE(bugprone-invalid-enum-default-initialization)
                TOKEN_STATISTICS stats{};
                stats.TokenId.LowPart = token_id;
                stats.AuthenticationId = token_logon_id(c.proc);
                stats.ExpirationTime.QuadPart = std::numeric_limits<LONGLONG>::max();
                stats.TokenType = get_token_type(token_handle);
                stats.ImpersonationLevel = !anonymous && stats.TokenType == TokenImpersonation ? SecurityImpersonation : SecurityAnonymous;
                stats.GroupCount = static_cast<ULONG>(get_token_groups(c, token_handle).size());
                stats.PrivilegeCount = anonymous ? 0 : static_cast<ULONG>(token_privileges(c.proc, token_handle).size());
                stats.ModifiedId = process_context::token_modified_id(c.proc.process_id);

                c.emu.write_memory(token_information, stats);

                return STATUS_SUCCESS;
            }

            if (token_information_class == TokenUserClaimAttributes || token_information_class == TokenDeviceClaimAttributes ||
                token_information_class == TokenRestrictedUserClaimAttributes ||
                token_information_class == TokenRestrictedDeviceClaimAttributes || token_information_class == TokenSecurityAttributes)
            {
                constexpr auto required_size = sizeof(TOKEN_SECURITY_ATTRIBUTES_INFORMATION);
                return_length.write(required_size);

                if (required_size > token_information_length)
                {
                    return STATUS_BUFFER_TOO_SMALL;
                }

                c.emu.write_memory(token_information, TOKEN_SECURITY_ATTRIBUTES_INFORMATION{
                                                          .Version = 1,
                                                          .Reserved = {},
                                                          .AttributeCount = 0,
                                                          .Attribute = {},
                                                      });

                return STATUS_SUCCESS;
            }

            if (token_information_class == TokenIntegrityLevel)
            {
                // S-1-16-8192 (medium) or S-1-16-12288 (high)
                std::array<uint8_t, 12> integrity_sid = {
                    0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x20, 0x00, 0x00,
                };
                if (is_elevated_token(c, token_handle))
                {
                    integrity_sid[9] = 0x30;
                }

                constexpr auto required_size = sizeof(integrity_sid) + sizeof(TOKEN_MANDATORY_LABEL64);
                return_length.write(required_size);

                if (required_size > token_information_length)
                {
                    return STATUS_BUFFER_TOO_SMALL;
                }

                TOKEN_MANDATORY_LABEL64 label{};
                label.Label.Attributes = 0x60;
                label.Label.Sid = token_information + sizeof(TOKEN_MANDATORY_LABEL64);

                emulator_object<TOKEN_MANDATORY_LABEL64>{c.emu, token_information}.write(label);
                c.emu.write_memory(token_information + sizeof(TOKEN_MANDATORY_LABEL64), integrity_sid.data(), integrity_sid.size());
                return STATUS_SUCCESS;
            }

            if (token_information_class == TokenProcessTrustLevel)
            {
                constexpr auto required_size = sizeof(TOKEN_PROCESS_TRUST_LEVEL64);
                return_length.write(required_size);

                if (required_size > token_information_length)
                {
                    return STATUS_BUFFER_TOO_SMALL;
                }

                c.emu.write_memory(token_information, TOKEN_PROCESS_TRUST_LEVEL64{
                                                          .TrustLevelSid = 0,
                                                      });

                return STATUS_SUCCESS;
            }

            if (token_information_class == TokenBnoIsolation)
            {
                constexpr auto required_size = sizeof(TOKEN_BNO_ISOLATION_INFORMATION64);
                return_length.write(required_size);

                if (required_size > token_information_length)
                {
                    return STATUS_BUFFER_TOO_SMALL;
                }

                c.emu.write_memory(token_information, TOKEN_BNO_ISOLATION_INFORMATION64{
                                                          .IsolationPrefix = 0,
                                                          .IsolationEnabled = FALSE,
                                                      });

                return STATUS_SUCCESS;
            }

            c.win_emu.log.error("Unsupported token info class: %X\n", token_information_class);
            c.emu.stop();
            return STATUS_NOT_SUPPORTED;
        }

        NTSTATUS handle_NtSetInformationToken(const syscall_context& c, const handle token_handle,
                                              const TOKEN_INFORMATION_CLASS token_information_class, const uint64_t /*token_information*/,
                                              const ULONG /*token_information_length*/)
        {
            if (!is_recognized_token_handle(c.proc, token_handle))
            {
                return STATUS_NOT_SUPPORTED;
            }

            switch (token_information_class)
            {
            case TokenOwner:
            case TokenPrimaryGroup:
            case TokenDefaultDacl:
            case TokenSessionId:
            case TokenSessionReference:
            case TokenAuditPolicy:
            case TokenOrigin:
            case TokenLinkedToken:
            case TokenVirtualizationAllowed:
            case TokenVirtualizationEnabled:
            case TokenIntegrityLevel:
            case TokenUIAccess:
            case TokenMandatoryPolicy:
            case TokenSecurityAttributes:
            case TokenPrivateNameSpace:
            case TokenChildProcessFlags:
                // sogen models a single synthetic token identity with no per-token mutable security
                // state consulted elsewhere (NtAccessCheck etc. do not read these back) - accepting
                // the write without persisting it matches every other settable class this emulator
                // exposes as a fixed/default value on the query side.
                return STATUS_SUCCESS;
            default:
                break;
            }

            c.win_emu.log.error("Unsupported token set info class: %X\n", token_information_class);
            c.emu.stop();
            return STATUS_NOT_SUPPORTED;
        }

        NTSTATUS handle_NtAccessCheck(const syscall_context& c, const uint64_t security_descriptor, const handle requested_token,
                                      const ACCESS_MASK desired_access, const emulator_object<EMU_GENERIC_MAPPING> generic_mapping,
                                      const uint64_t /*privilege_set*/, const emulator_object<ULONG> privilege_set_length,
                                      const emulator_object<ACCESS_MASK> granted_access, const emulator_object<NTSTATUS> access_status)
        {
            const auto client_token = resolve_token(c, requested_token);
            if (client_token == 0 || !is_recognized_token_handle(c.proc, client_token))
            {
                return STATUS_INVALID_HANDLE;
            }

            const auto decision = evaluate_descriptor_access(c, client_token, security_descriptor, desired_access, generic_mapping.read());
            if (decision.status == STATUS_INVALID_PARAMETER)
            {
                return decision.status;
            }

            privilege_set_length.write(0);
            granted_access.write(decision.granted);
            access_status.write(decision.status);
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtAccessCheckByType(const syscall_context& c, const uint64_t security_descriptor,
                                            const uint64_t /*principal_self_sid*/, const handle client_token,
                                            const ACCESS_MASK desired_access, const uint64_t object_type_list,
                                            const ULONG object_type_list_length, const emulator_object<EMU_GENERIC_MAPPING> generic_mapping,
                                            const uint64_t privilege_set, const emulator_object<ULONG> privilege_set_length,
                                            const emulator_object<ACCESS_MASK> granted_access,
                                            const emulator_object<NTSTATUS> access_status)
        {
            if (object_type_list != 0 || object_type_list_length != 0)
            {
                c.win_emu.log.error("NtAccessCheckByType with an object type list is not supported\n");
                return STATUS_NOT_SUPPORTED;
            }

            return handle_NtAccessCheck(c, security_descriptor, client_token, desired_access, generic_mapping, privilege_set,
                                        privilege_set_length, granted_access, access_status);
        }

        NTSTATUS handle_NtAccessCheckByTypeAndAuditAlarm(
            const syscall_context& c, const uint64_t /*subsystem_name*/, const uint64_t /*handle_id*/, const uint64_t /*object_type_name*/,
            const uint64_t /*object_name*/, const uint64_t security_descriptor, const uint64_t /*principal_self_sid*/,
            const ACCESS_MASK desired_access, const ULONG /*audit_type*/, const ULONG /*flags*/, const uint64_t object_type_list,
            const ULONG object_type_list_length, const emulator_object<EMU_GENERIC_MAPPING> generic_mapping,
            const BOOLEAN /*object_creation*/, const emulator_object<ACCESS_MASK> granted_access,
            const emulator_object<NTSTATUS> access_status, const emulator_object<BOOLEAN> generate_on_close)
        {
            if (object_type_list != 0 || object_type_list_length != 0)
            {
                c.win_emu.log.error("NtAccessCheckByTypeAndAuditAlarm with an object type list is not supported\n");
                return STATUS_NOT_SUPPORTED;
            }

            const auto decision = evaluate_descriptor_access(c, resolve_token(c, CURRENT_THREAD_EFFECTIVE_TOKEN), security_descriptor,
                                                             desired_access, generic_mapping.read());
            if (decision.status == STATUS_INVALID_PARAMETER)
            {
                return decision.status;
            }

            granted_access.write(decision.granted);
            access_status.write(decision.status);
            generate_on_close.write(FALSE);
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtAccessCheckAndAuditAlarm(const syscall_context& c, const uint64_t /*subsystem_name*/,
                                                   const uint64_t /*handle_id*/, const uint64_t /*object_type_name*/,
                                                   const uint64_t /*object_name*/, const uint64_t security_descriptor,
                                                   const ACCESS_MASK desired_access,
                                                   const emulator_object<EMU_GENERIC_MAPPING> generic_mapping,
                                                   const BOOLEAN /*object_creation*/, const emulator_object<ACCESS_MASK> granted_access,
                                                   const emulator_object<NTSTATUS> access_status,
                                                   const emulator_object<BOOLEAN> generate_on_close)
        {
            const auto decision = evaluate_descriptor_access(c, resolve_token(c, CURRENT_THREAD_EFFECTIVE_TOKEN), security_descriptor,
                                                             desired_access, generic_mapping.read());
            if (decision.status == STATUS_INVALID_PARAMETER)
            {
                return decision.status;
            }

            granted_access.write(decision.granted);
            access_status.write(decision.status);
            generate_on_close.write(FALSE);
            return STATUS_SUCCESS;
        }

        // sogen keeps no security event log, so audit alarms have no records to open, close or write.
        NTSTATUS handle_NtPrivilegedServiceAuditAlarm(const syscall_context& /*c*/, const uint64_t /*subsystem_name*/,
                                                      const uint64_t /*service_name*/, const uint64_t /*client_token*/,
                                                      const uint64_t /*privileges*/, const BOOLEAN /*access_granted*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtCloseObjectAuditAlarm(const syscall_context& /*c*/, const uint64_t /*subsystem_name*/,
                                                const uint64_t /*handle_id*/, const BOOLEAN /*generate_on_close*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtPrivilegeCheck(const syscall_context& c, const handle client_token, const emulator_pointer required_privileges,
                                         const emulator_object<BOOLEAN> result)
        {
            struct privilege_set_header
            {
                ULONG privilege_count;
                ULONG control;
            };

            constexpr ULONG privilege_set_all_necessary = 1;
            constexpr ULONG max_privilege_count = 66;
            constexpr uint64_t luid_and_attributes_size = 12;

            if (client_token != CURRENT_PROCESS_TOKEN && client_token != DUMMY_IMPERSONATION_TOKEN &&
                client_token != ANONYMOUS_IMPERSONATION_TOKEN && c.proc.tokens.get(client_token) == nullptr)
            {
                const auto object_handle = c.proc.resolve_object_pseudo_handle(client_token, c.vcpu.active_thread);
                auto* store = c.proc.get_handle_store(object_handle);
                return store && store->get_object(object_handle) ? STATUS_OBJECT_TYPE_MISMATCH : STATUS_INVALID_HANDLE;
            }

            const auto header = emulator_object<privilege_set_header>{c.emu, required_privileges}.try_read();
            if (!header)
            {
                return STATUS_ACCESS_VIOLATION;
            }

            // PRIVILEGE_SET declares one entry, so Windows probes that much even for an empty set.
            const auto probed_entries = std::max<uint64_t>(header->privilege_count, 1);
            const auto set_size = sizeof(privilege_set_header) + (probed_entries * luid_and_attributes_size);
            if (!is_writable_range(c.win_emu.memory, required_privileges, set_size) ||
                !is_writable_range(c.win_emu.memory, result.value(), sizeof(BOOLEAN)))
            {
                return STATUS_ACCESS_VIOLATION;
            }

            if (header->privilege_count > max_privilege_count)
            {
                return STATUS_INVALID_PARAMETER;
            }

            const auto privileges =
                client_token == ANONYMOUS_IMPERSONATION_TOKEN ? std::vector<token_privilege>{} : token_privileges(c.proc, client_token);

            bool all_held = true;
            bool any_held = false;
            for (ULONG i = 0; i < header->privilege_count; ++i)
            {
                const auto entry_address = required_privileges + sizeof(privilege_set_header) + (i * luid_and_attributes_size);
                const auto entry = c.emu.read_memory<LUID_AND_ATTRIBUTES>(entry_address);
                const bool held = holds_enabled_privilege(privileges, entry.Luid);
                if (held)
                {
                    c.emu.write_memory<ULONG>(entry_address + offsetof(LUID_AND_ATTRIBUTES, Attributes),
                                              entry.Attributes | se_privilege_used_for_access);
                }

                all_held &= held;
                any_held |= held;
            }

            result.write((header->control & privilege_set_all_necessary) ? all_held : any_held);
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtQuerySecurityAttributesToken(const syscall_context& c, const handle token_handle,
                                                       const emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> attributes,
                                                       const ULONG number_of_attributes, const uint64_t buffer, const ULONG buffer_length,
                                                       const emulator_object<ULONG> return_length)
        {
            if (!is_recognized_token_handle(c.proc, token_handle))
            {
                return STATUS_NOT_SUPPORTED;
            }

            if ((number_of_attributes != 0) != (attributes.value() != 0))
            {
                return STATUS_INVALID_PARAMETER;
            }

            // Emulated tokens carry no security attributes. Querying named ones fails as a whole when any is
            // absent, which is how ntdll's RtlQueryPackageClaims recognizes an unpackaged (desktop) process.
            if (number_of_attributes != 0)
            {
                for (ULONG index = 0; index < number_of_attributes; ++index)
                {
                    read_unicode_string(c.emu, attributes, index);
                }
                if (return_length.value())
                {
                    return_length.write(0);
                }
                return STATUS_NOT_FOUND;
            }

            constexpr auto required_size = sizeof(TOKEN_SECURITY_ATTRIBUTES_INFORMATION);
            if (return_length.value())
            {
                return_length.write(required_size);
            }

            if (buffer == 0 || buffer_length < required_size)
            {
                return STATUS_BUFFER_TOO_SMALL;
            }

            c.emu.write_memory(buffer, TOKEN_SECURITY_ATTRIBUTES_INFORMATION{
                                           .Version = 1,
                                           .Reserved = {},
                                           .AttributeCount = 0,
                                           .Attribute = {},
                                       });

            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtCompareSigningLevels(const syscall_context&, UCHAR, UCHAR)
        {
            return STATUS_NOT_SUPPORTED;
        }

        NTSTATUS handle_NtAdjustPrivilegesToken(const syscall_context& c, const handle token_handle, const BOOLEAN disable_all_privileges,
                                                const emulator_object<TOKEN_PRIVILEGES64> new_state, const ULONG buffer_length,
                                                const emulator_object<TOKEN_PRIVILEGES64> previous_state,
                                                const emulator_object<ULONG> return_length)
        {
            auto& privileges = token_privileges(c.proc, token_handle);
            auto updated = privileges;
            std::vector<token_privilege> previous{};
            bool all_assigned = true;

            if (disable_all_privileges)
            {
                for (const auto& privilege : privileges)
                {
                    if (privilege.attributes & se_privilege_enabled)
                    {
                        previous.push_back(privilege);
                    }
                }

                for (auto& privilege : updated)
                {
                    privilege.attributes &= ~se_privilege_enabled;
                }
            }
            else if (new_state)
            {
                const auto count = new_state.read().PrivilegeCount;
                for (ULONG i = 0; i < count; ++i)
                {
                    const auto entry = c.emu.read_memory<LUID_AND_ATTRIBUTES>(new_state.value() + offsetof(TOKEN_PRIVILEGES64, Privileges) +
                                                                              i * sizeof(LUID_AND_ATTRIBUTES));
                    const auto held = find_privilege(updated, entry.Luid);
                    if (held == updated.end())
                    {
                        all_assigned = false;
                        continue;
                    }

                    const auto before = *held;
                    if (entry.Attributes & se_privilege_removed)
                    {
                        updated.erase(held);
                        previous.push_back(before);
                        continue;
                    }

                    held->attributes = (held->attributes & ~se_privilege_enabled) | (entry.Attributes & se_privilege_enabled);
                    if (held->attributes != before.attributes)
                    {
                        previous.push_back(before);
                    }
                }
            }

            const auto previous_size = privilege_list_size(previous.size());
            return_length.write_if_valid(static_cast<ULONG>(previous_size));
            if (previous_state && buffer_length < previous_size)
            {
                return STATUS_BUFFER_TOO_SMALL;
            }

            privileges = std::move(updated);
            if (previous_state)
            {
                write_privilege_list(c, previous_state.value(), previous);
            }

            return all_assigned ? STATUS_SUCCESS : STATUS_NOT_ALL_ASSIGNED;
        }

        NTSTATUS handle_NtQuerySecurityPolicy()
        {
            return STATUS_NOT_SUPPORTED;
        }
    }

} // namespace sogen
