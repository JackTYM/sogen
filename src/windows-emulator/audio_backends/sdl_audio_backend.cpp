#include "../std_include.hpp"
#include <platform/audio_backend.hpp>

#include <cstddef>
#include <cstdio>
#include <mutex>
#include <optional>
#include <vector>

#include <SDL3/SDL.h>

// The embedding app owns main(); without this SDL refuses to initialize on iOS.
#define SDL_MAIN_HANDLED
#include <SDL3/SDL_main.h>

namespace sogen
{
    namespace
    {
        std::optional<SDL_AudioFormat> to_sdl_format(const audio_format& format)
        {
            if (format.is_float)
            {
                return format.bits_per_sample == 32 ? std::optional{SDL_AUDIO_F32} : std::nullopt;
            }

            switch (format.bits_per_sample)
            {
            case 8:
                return SDL_AUDIO_U8;
            case 16:
                return SDL_AUDIO_S16;
            case 32:
                return SDL_AUDIO_S32;
            default:
                return std::nullopt;
            }
        }

        class sdl_audio_backend final : public audio_backend
        {
          public:
            ~sdl_audio_backend() override
            {
                this->stop();
                if (this->initialized_)
                {
                    SDL_QuitSubSystem(SDL_INIT_AUDIO);
                }
            }

            bool start(const audio_format& format) override
            {
                const std::lock_guard lock{this->mutex_};

                if (this->stream_ && this->format_ == format)
                {
                    return true;
                }

                this->close_stream();

                const auto sdl_format = to_sdl_format(format);
                if (!sdl_format || format.channels == 0 || format.sample_rate == 0)
                {
                    return false;
                }

                if (!this->initialized_)
                {
                    SDL_SetMainReady();
                    this->initialized_ = SDL_InitSubSystem(SDL_INIT_AUDIO);
                    if (!this->initialized_)
                    {
                        std::fprintf(stderr, "[audio] SDL_InitSubSystem(AUDIO) failed: %s\n", SDL_GetError());
                        return false;
                    }
                }

                const SDL_AudioSpec spec{*sdl_format, static_cast<int>(format.channels), static_cast<int>(format.sample_rate)};
                this->stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
                if (!this->stream_)
                {
                    std::fprintf(stderr, "[audio] SDL_OpenAudioDeviceStream failed: %s\n", SDL_GetError());
                    return false;
                }

                std::fprintf(stderr, "[audio] started %u Hz, %u channels, %u bits%s\n", format.sample_rate, format.channels,
                             format.bits_per_sample, format.is_float ? " (float)" : "");
                this->format_ = format;
                SDL_ResumeAudioStreamDevice(this->stream_);
                return true;
            }

            void submit(const void* data, const size_t size) override
            {
                const std::lock_guard lock{this->mutex_};

                if (this->stream_ && data && size)
                {
                    static const bool silent = getenv("EMULATOR_AUDIO_SILENT") != nullptr;
                    if (silent)
                    {
                        this->silence_buffer_.assign(size, std::byte{0});
                        data = this->silence_buffer_.data();
                    }

                    SDL_PutAudioStreamData(this->stream_, data, static_cast<int>(size));
                }
            }

            std::optional<uint64_t> queued_bytes() const override
            {
                const std::lock_guard lock{this->mutex_};

                if (!this->stream_)
                {
                    return std::nullopt;
                }

                const auto queued = SDL_GetAudioStreamQueued(this->stream_);
                return queued < 0 ? std::nullopt : std::optional{static_cast<uint64_t>(queued)};
            }

            void stop() override
            {
                const std::lock_guard lock{this->mutex_};
                this->close_stream();
            }

          private:
            void close_stream()
            {
                if (this->stream_)
                {
                    SDL_DestroyAudioStream(this->stream_);
                    this->stream_ = nullptr;
                    this->format_ = {};
                }
            }

            // The audio render thread submits and polls the queue while the emulator thread may start or stop the
            // sink (e.g. when the UI backend is swapped), so every entry point serializes on this.
            mutable std::mutex mutex_{};
            bool initialized_{false};
            SDL_AudioStream* stream_{nullptr};
            audio_format format_{};
            std::vector<std::byte> silence_buffer_{};
        };
    }

    std::unique_ptr<audio_backend> create_sdl_audio_backend()
    {
        return std::make_unique<sdl_audio_backend>();
    }

} // namespace sogen
