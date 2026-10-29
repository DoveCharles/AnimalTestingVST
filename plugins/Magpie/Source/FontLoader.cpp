#include "FontLoader.h"

namespace magpie
{
    FontLoader::FontLoader() : juce::Thread ("Magpie SoundFont loader")
    {
        startThread (juce::Thread::Priority::low);
    }

    FontLoader::~FontLoader()
    {
        stopThread (10000);
    }

    void FontLoader::load (const juce::File& file)
    {
        {
            const juce::ScopedLock sl (infoLock);
            requested = file;
            hasRequest = true;
            info.file = file;
            info.loading = file != juce::File();
            info.error = {};
        }
        sendChangeMessage();
        notify();
    }

    FontInfo FontLoader::getInfo() const
    {
        const juce::ScopedLock sl (infoLock);
        return info;
    }

    juce::File FontLoader::getFile() const
    {
        const juce::ScopedLock sl (infoLock);
        return info.file;
    }

    int FontLoader::getLoadedSerial() const
    {
        const juce::ScopedLock sl (infoLock);
        return info.loading || hasRequest ? -1 : info.serial;
    }

    const SoundFont* FontLoader::acquire (int& serial) noexcept
    {
        const juce::SpinLock::ScopedTryLockType lock (swapLock);
        if (lock.isLocked() && hasPending && retired == nullptr)
        {
            retired = std::move (active);
            active = std::move (pending);
            activeSerial = pendingSerial;
            hasPending = false;
        }

        serial = activeSerial;
        return active.get();
    }

    void FontLoader::run()
    {
        while (! threadShouldExit())
        {
            juce::File file;
            bool loadNow = false;
            int serial = 0;
            {
                const juce::ScopedLock sl (infoLock);
                if (hasRequest)
                {
                    file = requested;
                    hasRequest = false;
                    loadNow = true;
                    serial = info.serial + 1;
                }
            }

            if (loadNow)
            {
                juce::String error;
                std::unique_ptr<SoundFont> font;
                if (file.existsAsFile())
                    font = readSoundFont (file, error);
                else if (file != juce::File())
                    error = "Can't find " + file.getFullPathName();

                FontInfo next;
                next.file = file;
                next.error = error;
                next.serial = serial;
                if (font != nullptr)
                {
                    next.name = font->name;
                    next.sampleBytes = (juce::int64) font->pcm.size() * 2;
                    for (const auto& p : font->presets)
                        next.presets.push_back ({ p.name, p.bank, p.program });
                }

                std::unique_ptr<SoundFont> unused;
                {
                    const juce::SpinLock::ScopedLockType lock (swapLock);
                    unused = std::move (pending);   // loaded but never picked up
                    pending = std::move (font);
                    pendingSerial = serial;
                    hasPending = true;
                }
                unused.reset();

                {
                    const juce::ScopedLock sl (infoLock);
                    // A newer request may have come in meanwhile; it will overwrite this when it finishes.
                    next.loading = hasRequest;
                    if (hasRequest)
                        next.file = info.file;
                    info = std::move (next);
                }
                sendChangeMessage();
            }

            std::unique_ptr<SoundFont> toFree;
            {
                const juce::SpinLock::ScopedLockType lock (swapLock);
                toFree = std::move (retired);
            }
            toFree.reset();

            wait (100);
        }
    }
}
