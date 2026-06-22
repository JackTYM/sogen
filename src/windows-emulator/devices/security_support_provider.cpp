#include "../std_include.hpp"
#include "security_support_provider.hpp"

#include "../windows_emulator.hpp"

namespace sogen
{

    namespace
    {
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

            NTSTATUS io_control(windows_emulator& win_emu, const io_device_context& c) override
            {
                if (c.io_control_code != 0x390400)
                {
                    return STATUS_NOT_SUPPORTED;
                }

                const auto operation = win_emu.emu().read_memory<USHORT>(c.input_buffer + 6);

                if (operation == 2)
                {
                    std::array<char16_t, 8> alg_name_buffer{};
                    win_emu.emu().read_memory(c.input_buffer + 0x30, alg_name_buffer.data(), sizeof(alg_name_buffer));

                    const std::u16string algorithm_name(alg_name_buffer.data());

                    if (algorithm_name == u"SHA256")
                    {
                        win_emu.emu().write_memory(c.output_buffer, sha256_output_data);

                        if (c.io_status_block)
                        {
                            IO_STATUS_BLOCK<EmulatorTraits<Emu64>> block{};
                            block.Information = sizeof(sha256_output_data);
                            c.io_status_block.write(block);
                        }
                    }
                    else if (algorithm_name == u"SHA1" || algorithm_name == u"MD5" || algorithm_name == u"MD4" || algorithm_name == u"MD2")
                    {
                        // All hash algorithms share the same structure; only the name differs.
                        std::array<std::uint8_t, sizeof(sha256_output_data)> hash_output_data{};
                        std::ranges::copy(sha256_output_data, hash_output_data.begin());
                        std::ranges::fill_n(hash_output_data.begin() + 0x50, 0x10, 0);
                        std::memcpy(hash_output_data.data() + 0x50, algorithm_name.data(), algorithm_name.size() * sizeof(char16_t));

                        win_emu.emu().write_memory(c.output_buffer, hash_output_data.data(), hash_output_data.size());

                        if (c.io_status_block)
                        {
                            IO_STATUS_BLOCK<EmulatorTraits<Emu64>> block{};
                            block.Information = hash_output_data.size();
                            c.io_status_block.write(block);
                        }
                    }
                    else if (algorithm_name == u"RSA")
                    {
                        // BCRYPT_ASYMMETRIC_ENCRYPTION_INTERFACE = 3
                        // rng_output_data has an 8-byte name slot at 0x50 ("RNG\0") — safe for 3-char names.
                        std::array<std::uint8_t, sizeof(rng_output_data)> rsa_output_data{};
                        std::ranges::copy(rng_output_data, rsa_output_data.begin());
                        rsa_output_data[0x18] = 0x03;
                        constexpr char16_t rsa_name[] = u"RSA";
                        std::memcpy(rsa_output_data.data() + 0x50, rsa_name, sizeof(rsa_name));

                        win_emu.emu().write_memory(c.output_buffer, rsa_output_data.data(), rsa_output_data.size());

                        if (c.io_status_block)
                        {
                            IO_STATUS_BLOCK<EmulatorTraits<Emu64>> block{};
                            block.Information = rsa_output_data.size();
                            c.io_status_block.write(block);
                        }
                    }
                    else if (algorithm_name == u"DSA")
                    {
                        // BCRYPT_SIGNATURE_INTERFACE = 5
                        std::array<std::uint8_t, sizeof(rng_output_data)> sig_output_data{};
                        std::ranges::copy(rng_output_data, sig_output_data.begin());
                        sig_output_data[0x18] = 0x05;
                        constexpr char16_t dsa_name[] = u"DSA";
                        std::memcpy(sig_output_data.data() + 0x50, dsa_name, sizeof(dsa_name));

                        win_emu.emu().write_memory(c.output_buffer, sig_output_data.data(), sig_output_data.size());

                        if (c.io_status_block)
                        {
                            IO_STATUS_BLOCK<EmulatorTraits<Emu64>> block{};
                            block.Information = sig_output_data.size();
                            c.io_status_block.write(block);
                        }
                    }
                    else if (algorithm_name == u"AES" || algorithm_name == u"DES" || algorithm_name == u"RC2" || algorithm_name == u"RC4")
                    {
                        // BCRYPT_CIPHER_INTERFACE = 1
                        std::array<std::uint8_t, sizeof(rng_output_data)> cipher_output_data{};
                        std::ranges::copy(rng_output_data, cipher_output_data.begin());
                        cipher_output_data[0x18] = 0x01;
                        std::ranges::fill_n(cipher_output_data.begin() + 0x50, 0x08, 0);
                        std::memcpy(cipher_output_data.data() + 0x50, algorithm_name.data(), algorithm_name.size() * sizeof(char16_t));

                        win_emu.emu().write_memory(c.output_buffer, cipher_output_data.data(), cipher_output_data.size());

                        if (c.io_status_block)
                        {
                            IO_STATUS_BLOCK<EmulatorTraits<Emu64>> block{};
                            block.Information = cipher_output_data.size();
                            c.io_status_block.write(block);
                        }
                    }
                    else
                    {
                        win_emu.emu().write_memory(c.output_buffer, rng_output_data);

                        if (c.io_status_block)
                        {
                            IO_STATUS_BLOCK<EmulatorTraits<Emu64>> block{};
                            block.Information = sizeof(rng_output_data);
                            c.io_status_block.write(block);
                        }
                    }
                }

                return STATUS_SUCCESS;
            }
        };
    }

    std::unique_ptr<io_device> create_security_support_provider()
    {
        return std::make_unique<security_support_provider>();
    }

} // namespace sogen
