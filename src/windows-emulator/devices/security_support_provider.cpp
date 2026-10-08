#include "../std_include.hpp"
#include "security_support_provider.hpp"

#include "../ksec_memory_crypt.hpp"
#include "../windows_emulator.hpp"

#include <utils/string.hpp>

namespace sogen
{

    namespace
    {
        // Partial layout of the KsecDD ioctl 0x390400 request, observed via a live capture against
        // real ntdll/KsecDD traffic. Covers the two fields the handler consumes: the operation
        // selector and the requested algorithm name.
        struct ksec_algorithm_request
        {
            std::array<uint8_t, 6> reserved0;
            uint16_t operation;
            std::array<uint8_t, 0x28> reserved1;
            std::array<char16_t, 8> algorithm_name;
        };

        static_assert(offsetof(ksec_algorithm_request, operation) == 6);
        static_assert(offsetof(ksec_algorithm_request, algorithm_name) == 0x30);
        static_assert(sizeof(ksec_algorithm_request) == 0x40);

        struct security_support_provider : stateless_device
        {
            // RNG Microsoft Primitive Provider
            // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays)
            std::uint8_t rng_output_data[216] = //
                {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x58, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
                 0xFF, 0xFF, 0xFF, 0xFF, 0x98, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                 0x52, 0x00, 0x4E, 0x00, 0x47, 0x00, 0x00, 0x00, 0x4D, 0x00, 0x69, 0x00, 0x63, 0x00, 0x72, 0x00, 0x6F, 0x00, 0x73, 0x00,
                 0x6F, 0x00, 0x66, 0x00, 0x74, 0x00, 0x20, 0x00, 0x50, 0x00, 0x72, 0x00, 0x69, 0x00, 0x6D, 0x00, 0x69, 0x00, 0x74, 0x00,
                 0x69, 0x00, 0x76, 0x00, 0x65, 0x00, 0x20, 0x00, 0x50, 0x00, 0x72, 0x00, 0x6F, 0x00, 0x76, 0x00, 0x69, 0x00, 0x64, 0x00,
                 0x65, 0x00, 0x72, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xA8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x62, 0x00, 0x63, 0x00, 0x72, 0x00, 0x79, 0x00, 0x70, 0x00, 0x74, 0x00,
                 0x70, 0x00, 0x72, 0x00, 0x69, 0x00, 0x6D, 0x00, 0x69, 0x00, 0x74, 0x00, 0x69, 0x00, 0x76, 0x00, 0x65, 0x00, 0x73, 0x00,
                 0x2E, 0x00, 0x64, 0x00, 0x6C, 0x00, 0x6C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

            // SHA256 Microsoft Primitive Provider
            // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays)
            std::uint8_t sha256_output_data[224] = //
                {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x41, 0x00, 0x32, 0x00, 0xFF,
                 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xA0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
                 0xFF, 0xFF, 0xFF, 0xFF, 0x53, 0x00, 0x48, 0x00, 0x41, 0x00, 0x32, 0x00, 0x35, 0x00, 0x36, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x4D, 0x00, 0x69, 0x00, 0x63, 0x00, 0x72, 0x00, 0x6F, 0x00, 0x73, 0x00, 0x6F, 0x00, 0x66, 0x00, 0x74, 0x00,
                 0x20, 0x00, 0x50, 0x00, 0x72, 0x00, 0x69, 0x00, 0x6D, 0x00, 0x69, 0x00, 0x74, 0x00, 0x69, 0x00, 0x76, 0x00, 0x65,
                 0x00, 0x20, 0x00, 0x50, 0x00, 0x72, 0x00, 0x6F, 0x00, 0x76, 0x00, 0x69, 0x00, 0x64, 0x00, 0x65, 0x00, 0x72, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xB0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x62, 0x00, 0x63, 0x00, 0x72, 0x00, 0x79, 0x00, 0x70, 0x00, 0x74, 0x00, 0x70, 0x00,
                 0x72, 0x00, 0x69, 0x00, 0x6D, 0x00, 0x69, 0x00, 0x74, 0x00, 0x69, 0x00, 0x76, 0x00, 0x65, 0x00, 0x73, 0x00, 0x2E,
                 0x00, 0x64, 0x00, 0x6C, 0x00, 0x6C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

            static NTSTATUS complete_status_ioctl(windows_emulator& win_emu, const io_device_context& c, const NTSTATUS payload)
            {
                ULONG info = 0;
                if (c.output_buffer && c.output_buffer_length >= sizeof(payload))
                {
                    win_emu.emu().write_memory(c.output_buffer, &payload, sizeof(payload));
                    info = sizeof(payload);
                }

                if (c.io_status_block)
                {
                    IO_STATUS_BLOCK<EmulatorTraits<Emu64>> block{};
                    block.Information = info;
                    c.io_status_block.write(block);
                }

                return STATUS_SUCCESS;
            }

            static constexpr ULONG k_ioctl_encrypt_same_process = 0x39000E;
            static constexpr ULONG k_ioctl_decrypt_same_process = 0x390012;
            static constexpr ULONG k_ioctl_encrypt_cross_process = 0x390016;
            static constexpr ULONG k_ioctl_decrypt_cross_process = 0x39001A;
            static constexpr ULONG k_ioctl_encrypt_same_logon = 0x39001E;
            static constexpr ULONG k_ioctl_decrypt_same_logon = 0x390022;

            static bool is_memory_crypt_ioctl(const ULONG code)
            {
                switch (code)
                {
                case k_ioctl_encrypt_same_process:
                case k_ioctl_decrypt_same_process:
                case k_ioctl_encrypt_cross_process:
                case k_ioctl_decrypt_cross_process:
                case k_ioctl_encrypt_same_logon:
                case k_ioctl_decrypt_same_logon:
                    return true;
                default:
                    return false;
                }
            }

            static NTSTATUS handle_memory_crypt(windows_emulator& win_emu, const io_device_context& c)
            {
                const auto source = c.input_buffer ? c.input_buffer : c.output_buffer;
                const auto dest = c.output_buffer ? c.output_buffer : c.input_buffer;
                const auto in_len = static_cast<size_t>(c.input_buffer_length);
                const auto out_len = static_cast<size_t>(c.output_buffer_length);
                // Cross-process and same-logon ioctls can pass distinct buffers. Writing the
                // input length into a shorter output buffer overwrites adjacent guest memory.
                if (in_len != 0 && out_len != 0 && out_len < in_len)
                {
                    return STATUS_INVALID_PARAMETER;
                }

                const auto length = in_len != 0 ? in_len : out_len;
                if (!source || !dest || length == 0)
                {
                    return STATUS_INVALID_PARAMETER;
                }

                std::vector<uint8_t> data(length);
                win_emu.emu().read_memory(source, data.data(), data.size());
                xor_ksec_memory(data);
                win_emu.emu().write_memory(dest, data.data(), data.size());
                if (c.io_status_block)
                {
                    IO_STATUS_BLOCK<EmulatorTraits<Emu64>> block{};
                    block.Information = data.size();
                    c.io_status_block.write(block);
                }

                return STATUS_SUCCESS;
            }

            NTSTATUS io_control(windows_emulator& win_emu, const io_device_context& c) override
            {
                if (is_memory_crypt_ioctl(c.io_control_code))
                {
                    return handle_memory_crypt(win_emu, c);
                }

                if (c.io_control_code != 0x390400)
                {
                    return STATUS_NOT_SUPPORTED;
                }

                // BCryptOpenAlgorithmProvider uses a dedicated output buffer sized for the provider blob.
                // Other KsecDD clients issue 0x390400 in-place (same input/output pointer) with a small
                // buffer and expect DeviceIoControl to succeed with a NTSTATUS in the first dword.
                const bool in_place = c.input_buffer != 0 && c.input_buffer == c.output_buffer;
                // The request's algorithm-name tail after the fixed 0x30-byte header varies in length with
                // the requested algorithm name, so only require enough of the buffer to read `operation`;
                // the algorithm_name read below is bounded to whatever the guest actually sent.
                constexpr size_t min_request_size = offsetof(ksec_algorithm_request, operation) + sizeof(uint16_t);
                if (!c.input_buffer || c.input_buffer_length < min_request_size ||
                    (in_place && c.output_buffer_length < sizeof(rng_output_data)))
                {
                    return complete_status_ioctl(win_emu, c, STATUS_SUCCESS);
                }

                const auto operation = win_emu.emu().read_memory<uint16_t>(c.input_buffer + offsetof(ksec_algorithm_request, operation));

                if (operation == 1)
                {
                    // Precedes the op=2 provider-descriptor fetch below. The guest only ever asks for an
                    // 8-byte reply here; a value of 0 reads as "no matching provider" and would abort the
                    // lookup before op=2 is ever issued, so report a single matching provider instead.
                    if (c.output_buffer && c.output_buffer_length >= sizeof(uint64_t))
                    {
                        constexpr uint64_t provider_count = 1;
                        win_emu.emu().write_memory(c.output_buffer, &provider_count, sizeof(provider_count));

                        if (c.io_status_block)
                        {
                            IO_STATUS_BLOCK<EmulatorTraits<Emu64>> block{};
                            block.Information = sizeof(provider_count);
                            c.io_status_block.write(block);
                        }
                    }

                    return STATUS_SUCCESS;
                }

                if (operation == 2)
                {
                    // algorithm_name is guest-controlled, may be shorter than the fixed array (see above -
                    // the guest only packs as many bytes as the name actually needs), and may not be
                    // NUL-terminated; read only what the guest declared, zero-fill the rest, and let the
                    // util bound the view to the fixed array so we never scan past it for a terminator.
                    constexpr size_t name_offset = offsetof(ksec_algorithm_request, algorithm_name);
                    std::array<char16_t, 8> algorithm_name_buffer{};
                    if (c.input_buffer_length > name_offset)
                    {
                        const size_t available = c.input_buffer_length - name_offset;
                        const size_t to_read = std::min(available, sizeof(algorithm_name_buffer));
                        win_emu.emu().try_read_memory(c.input_buffer + name_offset, algorithm_name_buffer.data(), to_read);
                    }
                    const auto algorithm_name = utils::string::to_string_view<char16_t>(algorithm_name_buffer);

                    // The response record is a fixed size, but real callers can (and do, e.g. MW2's WOW64
                    // boot querying SHA1/RSA) pass an output buffer larger than that - the extra bytes are
                    // reserved fields real KsecDD still initializes. Leaving them as whatever the guest
                    // buffer already held (uninitialized stack contents) let bcryptprimitives.dll read
                    // garbage there and abort its own init with STATUS_DLL_INIT_FAILED, so zero the full
                    // caller-declared length before writing the known prefix.
                    const auto write_response = [&](const auto& output_data) -> NTSTATUS {
                        if (!c.output_buffer || c.output_buffer_length < sizeof(output_data))
                        {
                            return STATUS_BUFFER_TOO_SMALL;
                        }

                        win_emu.emu().set_memory(c.output_buffer, 0, c.output_buffer_length);
                        win_emu.emu().write_memory(c.output_buffer, output_data);

                        if (c.io_status_block)
                        {
                            IO_STATUS_BLOCK<EmulatorTraits<Emu64>> block{};
                            block.Information = c.output_buffer_length;
                            c.io_status_block.write(block);
                        }

                        return STATUS_SUCCESS;
                    };

                    if (algorithm_name == u"SHA256")
                    {
                        return write_response(sha256_output_data);
                    }

                    if (algorithm_name == u"SHA1" || algorithm_name == u"MD5" || algorithm_name == u"MD4" || algorithm_name == u"MD2")
                    {
                        // All hash algorithms share the same structure; only the name differs.
                        std::array<std::uint8_t, sizeof(sha256_output_data)> hash_output_data{};
                        std::ranges::copy(sha256_output_data, hash_output_data.begin());
                        std::ranges::fill_n(hash_output_data.begin() + 0x50, 0x10, std::uint8_t{0});
                        std::memcpy(hash_output_data.data() + 0x50, algorithm_name.data(), algorithm_name.size() * sizeof(char16_t));

                        return write_response(hash_output_data);
                    }

                    if (algorithm_name == u"RSA")
                    {
                        // BCRYPT_ASYMMETRIC_ENCRYPTION_INTERFACE = 3, per bcrypt.h.
                        // rng_output_data has an 8-byte name slot at 0x50 ("RNG\0") — safe for 3-char names.
                        std::array<std::uint8_t, sizeof(rng_output_data)> rsa_output_data{};
                        std::ranges::copy(rng_output_data, rsa_output_data.begin());
                        rsa_output_data[0x18] = 0x03;
                        constexpr std::array<char16_t, 4> rsa_name{u'R', u'S', u'A', u'\0'};
                        std::memcpy(rsa_output_data.data() + 0x50, rsa_name.data(), rsa_name.size() * sizeof(char16_t));

                        return write_response(rsa_output_data);
                    }

                    if (algorithm_name == u"DSA")
                    {
                        // BCRYPT_SIGNATURE_INTERFACE = 5, per bcrypt.h.
                        std::array<std::uint8_t, sizeof(rng_output_data)> sig_output_data{};
                        std::ranges::copy(rng_output_data, sig_output_data.begin());
                        sig_output_data[0x18] = 0x05;
                        constexpr std::array<char16_t, 4> dsa_name{u'D', u'S', u'A', u'\0'};
                        std::memcpy(sig_output_data.data() + 0x50, dsa_name.data(), dsa_name.size() * sizeof(char16_t));

                        return write_response(sig_output_data);
                    }

                    if (algorithm_name == u"AES" || algorithm_name == u"DES" || algorithm_name == u"RC2" || algorithm_name == u"RC4")
                    {
                        // BCRYPT_CIPHER_INTERFACE = 1, per bcrypt.h.
                        std::array<std::uint8_t, sizeof(rng_output_data)> cipher_output_data{};
                        std::ranges::copy(rng_output_data, cipher_output_data.begin());
                        cipher_output_data[0x18] = 0x01;
                        std::ranges::fill_n(cipher_output_data.begin() + 0x50, 0x08, std::uint8_t{0});
                        std::memcpy(cipher_output_data.data() + 0x50, algorithm_name.data(), algorithm_name.size() * sizeof(char16_t));

                        return write_response(cipher_output_data);
                    }

                    return write_response(rng_output_data);
                }

                return STATUS_SUCCESS;
            }
        };
    }

    std::unique_ptr<io_device> create_security_support_provider(const device_creation_context&)
    {
        return std::make_unique<security_support_provider>();
    }

} // namespace sogen
