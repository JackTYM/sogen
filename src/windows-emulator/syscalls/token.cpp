#include "../std_include.hpp"
#include "../emulator_utils.hpp"
#include "../syscall_utils.hpp"

namespace sogen
{

    namespace syscalls
    {
        namespace
        {
            constexpr std::array<uint8_t, 12> interactive_sid = {
                0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x04, 0x00, 0x00, 0x00,
            };

            // S-1-5-5-0-1000: a synthetic logon-session SID (SECURITY_LOGON_IDS_RID); real Windows
            // derives the trailing two sub-authorities from the token's actual logon LUID, which sogen
            // does not track, so a fixed, stable value is used instead.
            constexpr std::array<uint8_t, 20> logon_sid = {
                0x01, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xE8, 0x03, 0x00, 0x00,
            };

            constexpr std::array<uint8_t, 16> administrators_sid = {
                0x01, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x20, 0x00, 0x00, 0x00, 0x20, 0x02, 0x00, 0x00,
            };

            constexpr std::array<uint8_t, 12> high_integrity_sid = {
                0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x30, 0x00, 0x00,
            };

            constexpr ULONG se_group_enabled = 0x7; // SE_GROUP_MANDATORY | SE_GROUP_ENABLED_BY_DEFAULT | SE_GROUP_ENABLED
            constexpr ULONG se_group_owner = 0x8;

            struct token_group
            {
                std::span<const uint8_t> sid{};
                ULONG attributes{};
            };

            // The emulated user is an administrator that is not subject to UAC Admin Approval Mode (like the
            // built-in Administrator account): there is no filtered linked token, every process runs with the full
            // administrator token and the elevation type is TokenElevationTypeDefault. A split token would need an
            // Explorer shell to de-elevate through (WebView2 launches its browser via CLSID_ShellWindows when its
            // host reports TokenElevationTypeFull).
            std::array<token_group, 3> get_token_groups(const process_context& proc)
            {
                return {{
                    {.sid = proc.sid, .attributes = 0},
                    {.sid = interactive_sid, .attributes = se_group_enabled},
                    {.sid = administrators_sid, .attributes = se_group_enabled | se_group_owner},
                }};
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
                       proc.tokens.get(token_handle) != nullptr;
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
            return token_handle == DUMMY_IMPERSONATION_TOKEN //
                       ? TokenImpersonation
                       : TokenPrimary;
        }

        NTSTATUS handle_NtDuplicateToken(const syscall_context& c, const handle existing_token_handle, ACCESS_MASK /*desired_access*/,
                                         const emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>>
                                         /*object_attributes*/,
                                         const BOOLEAN /*effective_only*/, const TOKEN_TYPE type,
                                         const emulator_object<handle> new_token_handle)
        {
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

        NTSTATUS handle_NtQueryInformationToken(const syscall_context& c, const handle token_handle,
                                                const TOKEN_INFORMATION_CLASS token_information_class, const uint64_t token_information,
                                                const ULONG token_information_length, const emulator_object<ULONG> return_length)
        {
            if (!is_recognized_token_handle(c.proc, token_handle))
            {
                return STATUS_NOT_SUPPORTED;
            }

            if (token_information_class == TokenAppContainerSid)
            {
                return STATUS_NOT_SUPPORTED;
            }

            const auto& sid = c.proc.sid;

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
                const auto groups = get_token_groups(c.proc);
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

                const auto level = get_token_type(token_handle) == TokenImpersonation ? SecurityImpersonation : SecurityAnonymous;
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
                                                          .TokenIsElevated = 1,
                                                      });
                return STATUS_SUCCESS;
            }

            if (token_information_class == TokenElevationType)
            {
                constexpr auto required_size = sizeof(ULONG);
                return_length.write(required_size);

                if (required_size > token_information_length)
                {
                    return STATUS_BUFFER_TOO_SMALL;
                }

                constexpr ULONG token_elevation_type_default = 1;
                emulator_object<ULONG>{c.emu, token_information}.write(token_elevation_type_default);
                return STATUS_SUCCESS;
            }

            if (token_information_class == TokenPrivileges)
            {
                // sogen doesn't track per-token privilege sets, so every token (the process's own
                // primary token as well as any "restricted" token NtFilterToken hands back - see its
                // own comment above) reports the one privilege real Windows practically never strips:
                // SeChangeNotifyPrivilege. Chromium's own sandbox::RestrictedToken::DeleteAllPrivileges()
                // keeps it by default even when stripping everything else, and
                // base::win::AccessToken::Privileges() hard-CHECKs the returned buffer against
                // sizeof(TOKEN_PRIVILEGES) regardless of PrivilegeCount - an honest, empty
                // PrivilegeCount=0 answer undersizes that buffer below what any real token this API is
                // ever actually called on has been observed to produce.
                constexpr auto required_size = sizeof(TOKEN_PRIVILEGES64);
                return_length.write(static_cast<ULONG>(required_size));

                if (required_size > token_information_length)
                {
                    return STATUS_BUFFER_TOO_SMALL;
                }

                TOKEN_PRIVILEGES64 privileges{};
                privileges.PrivilegeCount = 1;
                privileges.Privileges[0].Luid.LowPart = 23; // SE_CHANGE_NOTIFY_PRIVILEGE
                privileges.Privileges[0].Luid.HighPart = 0;
                privileges.Privileges[0].Attributes = 0x3; // SE_PRIVILEGE_ENABLED_BY_DEFAULT | SE_PRIVILEGE_ENABLED

                emulator_object<TOKEN_PRIVILEGES64>{c.emu, token_information}.write(privileges);
                return STATUS_SUCCESS;
            }

            if (token_information_class == TokenIsAppContainer || token_information_class == TokenIsAppSilo)
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

                TOKEN_STATISTICS stats{};
                stats.TokenType = get_token_type(token_handle);
                stats.ImpersonationLevel = stats.TokenType == TokenImpersonation ? SecurityImpersonation : SecurityAnonymous;
                stats.GroupCount = static_cast<ULONG>(get_token_groups(c.proc).size());
                stats.PrivilegeCount = 0;

                c.emu.write_memory(token_information, stats);

                return STATUS_SUCCESS;
            }

            if (token_information_class == TokenSecurityAttributes)
            {
                constexpr auto required_size = sizeof(TOKEN_SECURITY_ATTRIBUTES_INFORMATION);
                return_length.write(required_size);

                if (required_size > token_information_length)
                {
                    return STATUS_BUFFER_TOO_SMALL;
                }

                c.emu.write_memory(token_information, TOKEN_SECURITY_ATTRIBUTES_INFORMATION{
                                                          .Version = 0,
                                                          .Reserved = {},
                                                          .AttributeCount = 0,
                                                          .Attribute = {},
                                                      });

                return STATUS_SUCCESS;
            }

            if (token_information_class == TokenIntegrityLevel)
            {
                constexpr auto required_size = sizeof(TOKEN_MANDATORY_LABEL64) + sizeof(high_integrity_sid);
                return_length.write(required_size);

                if (required_size > token_information_length)
                {
                    return STATUS_BUFFER_TOO_SMALL;
                }

                TOKEN_MANDATORY_LABEL64 label{};
                label.Label.Attributes = 0x60;
                label.Label.Sid = token_information + sizeof(TOKEN_MANDATORY_LABEL64);

                emulator_object<TOKEN_MANDATORY_LABEL64>{c.emu, token_information}.write(label);
                c.emu.write_memory(token_information + sizeof(TOKEN_MANDATORY_LABEL64), high_integrity_sid);
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

        NTSTATUS handle_NtAccessCheck(const syscall_context& c, const uint64_t security_descriptor, const handle client_token,
                                      const ACCESS_MASK desired_access, const emulator_object<EMU_GENERIC_MAPPING> generic_mapping,
                                      const uint64_t /*privilege_set*/, const emulator_object<ULONG> privilege_set_length,
                                      const emulator_object<ACCESS_MASK> granted_access, const emulator_object<NTSTATUS> access_status)
        {
            if (!is_recognized_token_handle(c.proc, client_token))
            {
                return STATUS_INVALID_HANDLE;
            }

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

            const auto token_groups = get_token_groups(c.proc);
            const auto mapping = generic_mapping.read();
            const auto requested = map_generic_access(desired_access, mapping);
            ACCESS_MASK remaining = requested;
            ACCESS_MASK granted = 0;
            bool denied = false;

            if (!(control & SE_DACL_PRESENT) || dacl_address == 0)
            {
                granted = requested;
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
                        return STATUS_INVALID_PARAMETER;
                    }

                    const auto ace_mask = map_generic_access(ace.Mask, mapping);
                    const auto ace_sid = ace_address + offsetof(ACCESS_ALLOWED_ACE, SidStart);
                    const bool applies = sid_matches(c.emu.memory(), ace_sid, c.proc.sid) ||
                                         std::ranges::any_of(token_groups, [&](const token_group& group) {
                                             return (group.attributes & se_group_enabled) == se_group_enabled &&
                                                    sid_matches(c.emu.memory(), ace_sid, group.sid);
                                         });
                    if (applies && ace.Header.AceType == 1 && (ace_mask & remaining) != 0)
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

            privilege_set_length.write(0);
            granted_access.write(granted);
            access_status.write(!denied && remaining == 0 ? STATUS_SUCCESS : STATUS_ACCESS_DENIED);
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtQuerySecurityAttributesToken(const syscall_context& c, const handle token_handle,
                                                       const emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> /*attributes*/,
                                                       const ULONG number_of_attributes, const uint64_t buffer, const ULONG buffer_length,
                                                       const emulator_object<ULONG> return_length)
        {
            if (!is_recognized_token_handle(c.proc, token_handle))
            {
                return STATUS_NOT_SUPPORTED;
            }

            // Emulated tokens carry no security attributes. Querying named ones fails as a whole when any is
            // absent, which is how ntdll's RtlQueryPackageClaims recognizes an unpackaged (desktop) process.
            if (number_of_attributes != 0)
            {
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
                                           .Version = 0,
                                           .Reserved = {},
                                           .AttributeCount = 0,
                                           .Attribute = {},
                                       });

            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtAdjustPrivilegesToken(const syscall_context& c, const handle /*token_handle*/,
                                                const BOOLEAN disable_all_privileges, const emulator_object<TOKEN_PRIVILEGES64> new_state,
                                                const ULONG /*buffer_length*/, const emulator_object<TOKEN_PRIVILEGES64> previous_state,
                                                const emulator_object<ULONG> return_length)
        {
            constexpr ULONG se_change_notify_privilege = 23;

            if (previous_state)
            {
                previous_state.write(TOKEN_PRIVILEGES64{});
            }

            return_length.write_if_valid(static_cast<ULONG>(sizeof(ULONG)));

            if (disable_all_privileges || !new_state)
            {
                return STATUS_SUCCESS;
            }

            const auto requested_count = new_state.read().PrivilegeCount;
            bool all_assigned = true;
            for (ULONG i = 0; i < requested_count; ++i)
            {
                const auto entry = c.emu.read_memory<LUID_AND_ATTRIBUTES>(new_state.value() + offsetof(TOKEN_PRIVILEGES64, Privileges) +
                                                                          i * sizeof(LUID_AND_ATTRIBUTES));
                all_assigned &= entry.Luid.HighPart == 0 && entry.Luid.LowPart == se_change_notify_privilege;
            }

            return all_assigned ? STATUS_SUCCESS : STATUS_NOT_ALL_ASSIGNED;
        }

        NTSTATUS handle_NtQuerySecurityPolicy()
        {
            return STATUS_NOT_SUPPORTED;
        }
    }

} // namespace sogen
