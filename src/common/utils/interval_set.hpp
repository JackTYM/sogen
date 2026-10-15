#pragma once

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <map>
#include <optional>
#include <utility>

namespace sogen::utils
{
    class interval_set
    {
      public:
        using run_map = std::map<uint64_t, uint64_t>;

        const run_map& runs() const
        {
            return this->runs_;
        }

        bool empty() const
        {
            return this->runs_.empty();
        }

        bool contains(const uint64_t value) const
        {
            return this->run_containing(value).has_value();
        }

        std::optional<std::pair<uint64_t, uint64_t>> run_containing(const uint64_t value) const
        {
            auto it = this->runs_.upper_bound(value);
            if (it == this->runs_.begin())
            {
                return std::nullopt;
            }

            --it;
            if (value >= it->second)
            {
                return std::nullopt;
            }

            return *it;
        }

        std::optional<uint64_t> next_run_start(const uint64_t value) const
        {
            const auto it = this->runs_.upper_bound(value);
            if (it == this->runs_.end())
            {
                return std::nullopt;
            }

            return it->first;
        }

        void insert(const uint64_t start, const uint64_t end)
        {
            if (start >= end)
            {
                return;
            }

            auto merged_start = start;
            auto merged_end = end;

            auto it = this->runs_.upper_bound(start);
            if (it != this->runs_.begin())
            {
                const auto previous = std::prev(it);
                if (previous->second >= start)
                {
                    merged_start = previous->first;
                    merged_end = std::max(merged_end, previous->second);
                    it = this->runs_.erase(previous);
                }
            }

            while (it != this->runs_.end() && it->first <= merged_end)
            {
                merged_end = std::max(merged_end, it->second);
                it = this->runs_.erase(it);
            }

            this->runs_.emplace_hint(it, merged_start, merged_end);
        }

        void erase(const uint64_t start, const uint64_t end)
        {
            if (start >= end)
            {
                return;
            }

            auto it = this->runs_.upper_bound(start);
            if (it != this->runs_.begin())
            {
                const auto previous = std::prev(it);
                if (previous->second > start)
                {
                    const auto previous_end = previous->second;
                    if (previous->first == start)
                    {
                        this->runs_.erase(previous);
                    }
                    else
                    {
                        previous->second = start;
                    }

                    if (previous_end > end)
                    {
                        this->runs_.emplace_hint(it, end, previous_end);
                        return;
                    }
                }
            }

            while (it != this->runs_.end() && it->first < end)
            {
                if (it->second > end)
                {
                    const auto run_end = it->second;
                    it = this->runs_.erase(it);
                    this->runs_.emplace_hint(it, end, run_end);
                    return;
                }

                it = this->runs_.erase(it);
            }
        }

        template <typename Callback>
        void for_each_overlap(const uint64_t start, const uint64_t end, Callback&& callback) const
        {
            auto it = this->runs_.upper_bound(start);
            if (it != this->runs_.begin() && std::prev(it)->second > start)
            {
                --it;
            }

            for (; it != this->runs_.end() && it->first < end; ++it)
            {
                callback(std::max(it->first, start), std::min(it->second, end));
            }
        }

      private:
        run_map runs_{};
    };
}
