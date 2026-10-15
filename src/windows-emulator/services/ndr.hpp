#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace sogen::ndr
{
    using guid = std::array<uint8_t, 16>;

    // NDR 2.0 (DCE transfer syntax) stub data as rpcrt4 exchanges it over LRPC: little-endian, every primitive
    // aligned to its own size relative to the start of the stub data, embedded pointers as 4-byte referent ids.
    class reader
    {
      public:
        explicit reader(const std::span<const uint8_t> data)
            : data_(data)
        {
        }

        bool ok() const
        {
            return !this->failed_;
        }

        size_t offset() const
        {
            return this->offset_;
        }

        void align(const size_t alignment)
        {
            this->offset_ = (this->offset_ + alignment - 1) & ~(alignment - 1);
        }

        template <typename T>
        T read()
        {
            this->align(sizeof(T));
            T value{};
            this->read_bytes(&value, sizeof(T));
            return value;
        }

        void read_bytes(void* destination, const size_t length)
        {
            if (this->failed_ || this->offset_ + length > this->data_.size())
            {
                this->failed_ = true;
                std::memset(destination, 0, length);
                return;
            }

            std::memcpy(destination, this->data_.data() + this->offset_, length);
            this->offset_ += length;
        }

        std::vector<uint8_t> read_bytes(const size_t length)
        {
            std::vector<uint8_t> bytes(length);
            this->read_bytes(bytes.data(), length);
            return bytes;
        }

        guid read_guid()
        {
            this->align(4);
            guid value{};
            this->read_bytes(value.data(), value.size());
            return value;
        }

        uint32_t read_pointer()
        {
            return this->read<uint32_t>();
        }

        // A conformant varying wide string (max_count, offset, actual_count, characters).
        std::u16string read_string()
        {
            this->read<uint32_t>();
            this->read<uint32_t>();
            const auto count = this->read<uint32_t>();
            if (count > (this->data_.size() - std::min(this->offset_, this->data_.size())) / 2)
            {
                this->failed_ = true;
                return {};
            }

            std::u16string value(count, u'\0');
            this->read_bytes(value.data(), count * sizeof(char16_t));
            while (!value.empty() && value.back() == u'\0')
            {
                value.pop_back();
            }

            return value;
        }

        std::optional<std::u16string> read_unique_string()
        {
            if (this->read_pointer() == 0)
            {
                return std::nullopt;
            }

            return this->read_string();
        }

      private:
        std::span<const uint8_t> data_{};
        size_t offset_{};
        bool failed_{};
    };

    class writer
    {
      public:
        void align(const size_t alignment)
        {
            while (this->data_.size() % alignment != 0)
            {
                this->data_.push_back(0);
            }
        }

        template <typename T>
        void write(const T& value)
        {
            this->align(sizeof(T));
            this->write_bytes(&value, sizeof(T));
        }

        void write_bytes(const void* bytes, const size_t length)
        {
            const auto* begin = static_cast<const uint8_t*>(bytes);
            this->data_.insert(this->data_.end(), begin, begin + length);
        }

        void write_guid(const guid& value)
        {
            this->align(4);
            this->write_bytes(value.data(), value.size());
        }

        uint32_t write_referent(const bool present)
        {
            const uint32_t referent = present ? 0x20000 + 4 * this->next_referent_++ : 0;
            this->write(referent);
            return referent;
        }

        void write_string(const std::u16string_view value)
        {
            const auto count = static_cast<uint32_t>(value.size() + 1);
            this->write(count);
            this->write(uint32_t{0});
            this->write(count);
            this->write_bytes(value.data(), value.size() * sizeof(char16_t));
            this->write(char16_t{0});
        }

        size_t size() const
        {
            return this->data_.size();
        }

        std::vector<uint8_t> take()
        {
            this->align(8);
            return std::move(this->data_);
        }

      private:
        std::vector<uint8_t> data_{};
        uint32_t next_referent_{};
    };
}
