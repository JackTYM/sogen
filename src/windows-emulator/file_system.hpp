#pragma once
#include "std_include.hpp"
#include "windows_path.hpp"

namespace sogen
{

    class file_system
    {
      public:
        file_system(const std::filesystem::path& root)
            : root_(canonical(root))
        {
        }

        static bool is_escaping_relative_path(const std::filesystem::path& p)
        {
            return p.empty() || *p.begin() == "..";
        }

        static bool is_subpath(const std::filesystem::path& normal_root, const std::filesystem::path& normal_target)
        {
            // Lexical containment: structural check only, so it does not follow symlinks (unlike
            // std::filesystem::relative, which canonicalizes). Callers pass lexically-normalized
            // paths so that ".." escapes are still caught.
            const auto relative_path = normal_target.lexically_relative(normal_root);
            return !is_escaping_relative_path(relative_path);
        }

        std::set<char> list_drives() const
        {
            std::set<char> drives{};

#ifdef OS_WINDOWS
            if (this->root_.empty())
            {
                const auto drive_bits = GetLogicalDrives();

                for (char drive = 'a'; drive <= 'z'; ++drive)
                {
                    const auto drive_index = drive - 'a';
                    if (drive_bits & (1 << drive_index))
                    {
                        drives.insert(drive);
                    }
                }

                return drives;
            }
#endif

            std::error_code ec{};
            for (const auto& file : std::filesystem::directory_iterator(this->root_, ec))
            {
                const auto filename = file.path().filename().string();
                if (filename.size() == 1)
                {
                    drives.insert(utils::string::char_to_lower(filename.front()));
                }
            }

            return drives;
        }

        std::filesystem::path translate(const windows_path& win_path) const
        {
            if (!win_path.is_absolute())
            {
                throw std::runtime_error("Only absolute paths can be translated: " + win_path.string());
            }

            // Exact file mapping (fast path).
            if (const auto mapping = this->mappings_.find(win_path); mapping != this->mappings_.end())
            {
                return mapping->second;
            }

            // Directory mapping: a mapped ancestor directory mounts its whole subtree.
            if (auto mapped = this->translate_directory_mapping(win_path))
            {
                return std::move(*mapped);
            }

#ifdef OS_WINDOWS
            if (this->root_.empty())
            {
                return win_path.u16string();
            }
#endif

            // The emulation-root fallback below resolves host symlinks via weakly_canonical and may scan a
            // directory per path component for a case-insensitive match, which costs real stat()s;
            // guest code that repeatedly probes the same missing path (e.g. retrying a failed
            // NtCreateFile with no backoff) would otherwise pay that cost on every attempt. translate()
            // is a pure function of win_path for a fixed root_/mappings_, so memoize it; the cache is
            // invalidated wholesale on map() since a new mapping can change the result for paths under it.
            const std::lock_guard cache_lock(this->confine_cache_mutex_);
            if (const auto cached = this->confine_cache_.find(win_path); cached != this->confine_cache_.end())
            {
                return cached->second;
            }

            auto result = this->translate_to_root(win_path);
            this->confine_cache_.emplace(win_path, result);
            return result;
        }

        template <typename F>
        void access_mapped_entries(const windows_path& win_path, const F& accessor) const
        {
            for (const auto& mapping : this->mappings_)
            {
                const auto& mapped_path = mapping.first;
                if (!mapped_path.empty() && mapped_path.parent() == win_path)
                {
                    accessor(mapping);
                }
            }
        }

        void map(windows_path src, std::filesystem::path dest)
        {
            this->mappings_[std::move(src)] = std::move(dest);

            const std::lock_guard cache_lock(this->confine_cache_mutex_);
            this->confine_cache_.clear();
        }

      private:
        // Resolve a host path built from a guest-controlled path, but keep it inside `base`: a ".." in the
        // guest path that would escape `base` falls back to `base` itself. The check is purely lexical (it
        // does not follow symlinks), so a crafted path cannot escape. Host-side symlinks placed inside `base`
        // may still point elsewhere (e.g. a mounted game directory); the OS resolves those at open time.
        static std::filesystem::path confine(const std::filesystem::path& base, const std::filesystem::path& candidate)
        {
            if (is_subpath(base.lexically_normal(), candidate.lexically_normal()))
            {
                return weakly_canonical(candidate);
            }

            return base;
        }

        // If a mapped directory is an ancestor of `win_path`, resolve the remaining path against the mapped
        // host directory. The deepest matching mount wins, so nested mounts behave intuitively.
        std::optional<std::filesystem::path> translate_directory_mapping(const windows_path& win_path) const
        {
            const std::filesystem::path* best_dest = nullptr;
            windows_path best_remainder{};
            size_t best_depth = 0;

            for (const auto& [src, dest] : this->mappings_)
            {
                auto remainder = win_path.relative_to(src);
                if (remainder.has_value() && (best_dest == nullptr || src.depth() > best_depth))
                {
                    best_dest = &dest;
                    best_remainder = std::move(*remainder);
                    best_depth = src.depth();
                }
            }

            if (best_dest == nullptr)
            {
                return std::nullopt;
            }

            return confine(*best_dest, *best_dest / best_remainder.to_portable_path());
        }

        std::filesystem::path root_{};
        std::unordered_map<windows_path, std::filesystem::path> mappings_{};

        // Emulation-root translation, confined to the drive root.
        std::filesystem::path translate_to_root(const windows_path& win_path) const
        {
            const std::array<char, 2> root_drive{win_path.get_drive().value_or('c'), 0};
            const auto root = this->root_ / root_drive.data();

            const auto portable = win_path.to_portable_path();
            const auto path = this->root_ / portable;

            // Confine the guest-controlled path to the drive root by resolving "." and ".."
            // lexically, without following symlinks, so a crafted path cannot escape. Host-side
            // symlinks placed inside the root may still point elsewhere (e.g. a mounted game
            // directory); the OS resolves them when the file is opened.
            if (!is_subpath(root.lexically_normal(), path.lexically_normal()))
            {
                return root;
            }

            std::error_code ec{};
            if (std::filesystem::exists(path, ec))
            {
                return weakly_canonical(path);
            }

            // The emulation root preserves the original Windows file casing, but the guest (like
            // Windows itself) treats paths case-insensitively. When the exact-case path does not
            // exist, resolve each component against the matching on-disk entry regardless of case.
            return weakly_canonical(resolve_case_insensitive(portable));
        }

        // Walk a root-relative path component by component, substituting a case-insensitive on-disk
        // match for any component that does not exist with the requested case. Components with no
        // match are kept verbatim so not-found / file-creation paths are unchanged. An exact-case
        // match always wins, keeping resolution deterministic when case-variant entries coexist.
        std::filesystem::path resolve_case_insensitive(const std::filesystem::path& relative) const
        {
            std::filesystem::path result = this->root_;
            std::error_code ec{};

            for (const auto& part : relative)
            {
                auto literal = result / part;
                if (std::filesystem::exists(literal, ec))
                {
                    result = std::move(literal);
                    continue;
                }

                std::filesystem::path match{};
                if (std::filesystem::is_directory(result, ec))
                {
                    const auto part_name = part.u16string();
                    for (const auto& entry : std::filesystem::directory_iterator(result, ec))
                    {
                        if (utils::string::equals_ignore_case(entry.path().filename().u16string(), part_name))
                        {
                            match = entry.path();
                            break;
                        }
                    }
                }

                result = match.empty() ? std::move(literal) : std::move(match);
            }

            return result;
        }

        mutable std::mutex confine_cache_mutex_{};
        mutable std::unordered_map<windows_path, std::filesystem::path> confine_cache_{};
    };

} // namespace sogen
