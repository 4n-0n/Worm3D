#include "audio/engine.hpp"

#include <QAudioFormat>
#include <QAudioSink>
#include <QDebug>
#include <QIODevice>
#include <QMediaDevices>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace w3 {
namespace {

struct Voice {
    float phase = 0.0f;
    float freq = 0.0f;
    float freqEnd = 0.0f;
    float amp = 0.0f;
    float decay = 0.0f;
    float delay = 0.0f;
    float pan = 0.0f;
    int wave = 0;  // 0 seno, 1 rumore, 2 dente di sega, 3 quadra, 4 triangolo
    bool active = false;
};

// Musica procedurale: progressione Am-F-C-G a 84 BPM con pad, basso, arpeggio
// e percussioni leggere. Nessun file audio: tutto e' sintetizzato qui.
class MusicSynth {
public:
    void render(float& left, float& right) {
        const float dt = 1.0f / kSampleRate;
        advanceClock();

        // Fade-in lento all'avvio: la musica non deve "entrare a strappo".
        gain_ += (kTargetGain - gain_) * std::min(1.0f, dt * 0.55f);

        float sumLeft = 0.0f;
        float sumRight = 0.0f;
        for (MusicVoice& voice : voices_) {
            if (voice.state == 0) {
                continue;
            }
            if (voice.state == 1) {  // attack
                voice.env += dt / std::max(voice.attack, 0.0005f);
                if (voice.env >= voice.sustain) {
                    voice.env = voice.sustain;
                    voice.state = 2;
                }
            } else if (voice.state == 2) {  // sustain / decadimento
                if (voice.decay > 0.0f) {
                    voice.env *= std::exp(-voice.decay * dt);
                    if (voice.env < 0.0015f) {
                        voice.state = 0;
                        continue;
                    }
                }
            } else {  // release
                voice.env -= dt / std::max(voice.release, 0.01f);
                if (voice.env <= 0.0f) {
                    voice.state = 0;
                    continue;
                }
            }

            voice.phase += voice.freq * dt;
            voice.phase -= std::floor(voice.phase);
            voice.freq += (voice.freqEnd - voice.freq) * std::min(1.0f, dt * 20.0f);

            float sample = 0.0f;
            switch (voice.wave) {
                case 1: sample = noise(); break;
                case 2: sample = 2.0f * voice.phase - 1.0f; break;
                case 3: sample = 1.0f - 4.0f * std::fabs(voice.phase - 0.5f); break;
                default: sample = std::sin(voice.phase * kTwoPi); break;
            }

            sumLeft += sample * voice.env * voice.level * (1.0f - voice.pan) * 0.5f;
            sumRight += sample * voice.env * voice.level * (1.0f + voice.pan) * 0.5f;
        }

        left += sumLeft * gain_;
        right += sumRight * gain_;
    }

private:
    struct MusicVoice {
        float phase = 0.0f;
        float freq = 0.0f;
        float freqEnd = 0.0f;
        float env = 0.0f;
        float level = 0.1f;
        float pan = 0.0f;
        float attack = 0.01f;
        float release = 0.5f;
        float sustain = 1.0f;
        float decay = 0.0f;
        int state = 0;  // 0 spenta, 1 attack, 2 sustain, 3 release
        int wave = 0;
        int group = 0;  // 0 pad, 1 arpeggio, 2 basso, 3 percussioni
    };

    static constexpr float kSampleRate = 48000.0f;
    static constexpr float kBpm = 84.0f;
    static constexpr float kTwoPi = 6.2831853f;
    static constexpr float kTargetGain = 0.85f;
    static constexpr int kVoiceCount = 30;

    void advanceClock() {
        beats_ += kBpm / 60.0f / kSampleRate;
        const int step = static_cast<int>(beats_ * 4.0f);  // sedicesimi
        if (step != lastStep_) {
            lastStep_ = step;
            onStep(step);
        }
    }

    void onStep(int step) {
        static const float kChords[4][3] = {
            {110.00f, 130.81f, 164.81f},  // Am
            {87.31f, 110.00f, 130.81f},   // F
            {130.81f, 164.81f, 196.00f},  // C
            {98.00f, 123.47f, 146.83f},   // G
        };

        const int barStep = step % 16;
        if (barStep == 0) {
            chordIndex_ = (step / 16) % 4;
            startChord(kChords[chordIndex_]);
        }

        const float root = kChords[chordIndex_][0];
        if (barStep % 8 == 0) {
            // Basso e cassa sui tempi forti.
            trigger(root * 0.5f, root * 0.5f, 0.01f, 0.4f, 1.0f, 2.2f, 3, 0.0f, 2, 0.16f);
            trigger(120.0f, 45.0f, 0.003f, 0.25f, 1.0f, 9.0f, 0, 0.0f, 3, 0.22f);
        }
        if (barStep % 4 == 2) {
            // Hi-hat morbido in levare.
            trigger(0.0f, 0.0f, 0.001f, 0.08f, 1.0f, 45.0f, 1, 0.30f, 3, 0.05f);
        }
        if (barStep % 2 == 0) {
            // Arpeggio: note del accordo due ottave sopra, pan alternato.
            const int note = (barStep / 2) % 3;
            const float frequency = kChords[chordIndex_][note] * 4.0f;
            const float pan = ((barStep / 2) % 2 == 0) ? -0.30f : 0.30f;
            trigger(frequency, frequency, 0.004f, 0.35f, 1.0f, 7.0f, 0, pan, 1, 0.10f);
        }
    }

    void startChord(const float (&chord)[3]) {
        // I pad precedenti vanno in release, i nuovi entrano con attacco lungo.
        for (MusicVoice& voice : voices_) {
            if (voice.group == 0 && voice.state != 0 && voice.state != 3) {
                voice.state = 3;
            }
        }
        for (int note = 0; note < 3; ++note) {
            for (int detune = 0; detune < 2; ++detune) {
                const float factor = detune == 0 ? 0.9965f : 1.0035f;
                trigger(chord[note] * factor, chord[note] * factor, 0.9f, 1.6f, 1.0f, 0.0f, 0,
                        detune == 0 ? -0.22f : 0.22f, 0, 0.09f);
            }
        }
    }

    void trigger(float frequency, float frequencyEnd, float attack, float release, float sustain,
                 float decay, int wave, float pan, int group, float level) {
        MusicVoice* voice = allocate();
        if (voice == nullptr) {
            return;
        }
        *voice = MusicVoice{};
        voice->freq = frequency;
        voice->freqEnd = frequencyEnd;
        voice->attack = attack;
        voice->release = release;
        voice->sustain = sustain;
        voice->decay = decay;
        voice->wave = wave;
        voice->pan = pan;
        voice->group = group;
        voice->level = level;
        voice->state = 1;
    }

    MusicVoice* allocate() {
        for (MusicVoice& voice : voices_) {
            if (voice.state == 0) {
                return &voice;
            }
        }
        // Tutte occupate: ruba la piu' debole, mai un crash.
        MusicVoice* weakest = &voices_[0];
        for (MusicVoice& voice : voices_) {
            if (voice.env < weakest->env) {
                weakest = &voice;
            }
        }
        return weakest;
    }

    float noise() {
        rng_ ^= rng_ << 13;
        rng_ ^= rng_ >> 17;
        rng_ ^= rng_ << 5;
        return (static_cast<float>(rng_ >> 8) / 8388608.0f) - 1.0f;
    }

    MusicVoice voices_[kVoiceCount];
    float beats_ = 0.0f;
    int lastStep_ = -1;
    int chordIndex_ = 0;
    float gain_ = 0.0f;
    std::uint32_t rng_ = 0x51ED270Bu;
};

class MixerDevice : public QIODevice {
public:
    static constexpr int kSampleRate = 48000;
    static constexpr int kVoiceCount = 40;

    MixerDevice() { open(QIODevice::ReadOnly); }

    bool isSequential() const override { return true; }
    qint64 bytesAvailable() const override { return 8192 + QIODevice::bytesAvailable(); }

    void setMuted(bool muted) { muted_ = muted; }

    void trigger(Cue cue) {
        switch (cue) {
            case Cue::EatApple:
                add(620.0f, 880.0f, 0.20f, 26.0f, 0.0f, 0);
                break;
            case Cue::EatGolden:
                add(880.0f, 880.0f, 0.17f, 14.0f, 0.0f, 0);
                add(1174.0f, 1174.0f, 0.17f, 13.0f, 0.07f, 0);
                add(1568.0f, 1568.0f, 0.15f, 10.0f, 0.14f, 0);
                break;
            case Cue::EatChili:
                add(170.0f, 540.0f, 0.22f, 6.0f, 0.0f, 2);
                break;
            case Cue::EatBlueberry:
                add(760.0f, 300.0f, 0.20f, 6.0f, 0.0f, 0);
                add(380.0f, 240.0f, 0.10f, 5.0f, 0.0f, 4);
                break;
            case Cue::EatLemon:
                add(990.0f, 1320.0f, 0.15f, 16.0f, 0.0f, 0);
                break;
            case Cue::EatPoison:
                add(210.0f, 60.0f, 0.28f, 5.0f, 0.0f, 2);
                add(0.0f, 0.0f, 0.14f, 6.0f, 0.03f, 1);
                break;
            case Cue::EatStar:
                add(700.0f, 700.0f, 0.15f, 15.0f, 0.0f, 0);
                add(1050.0f, 1050.0f, 0.15f, 13.0f, 0.06f, 0);
                add(1400.0f, 1400.0f, 0.13f, 11.0f, 0.12f, 0);
                break;
            case Cue::EatMagnet:
                add(260.0f, 640.0f, 0.18f, 6.0f, 0.0f, 4);
                break;
            case Cue::EatBomb:
                add(90.0f, 40.0f, 0.40f, 6.0f, 0.0f, 2);
                add(0.0f, 0.0f, 0.30f, 8.0f, 0.0f, 1);
                break;
            case Cue::EatHeart:
                add(523.0f, 523.0f, 0.16f, 10.0f, 0.0f, 0);
                add(659.0f, 659.0f, 0.16f, 10.0f, 0.08f, 0);
                add(784.0f, 784.0f, 0.16f, 9.0f, 0.16f, 0);
                break;
            case Cue::EatCrystal:
                add(1240.0f, 1840.0f, 0.14f, 11.0f, 0.0f, 0);
                break;
            case Cue::Power:
                add(660.0f, 990.0f, 0.15f, 13.0f, 0.0f, 0);
                break;
            case Cue::Bounce:
                add(140.0f, 70.0f, 0.22f, 15.0f, 0.0f, 0);
                break;
            case Cue::Hurt:
                add(320.0f, 70.0f, 0.32f, 4.0f, 0.0f, 2);
                add(0.0f, 0.0f, 0.20f, 7.0f, 0.0f, 1);
                break;
            case Cue::Explosion:
                add(0.0f, 0.0f, 0.42f, 5.0f, 0.0f, 1);
                add(70.0f, 30.0f, 0.32f, 4.0f, 0.0f, 2);
                break;
            case Cue::LevelUp:
                add(523.0f, 523.0f, 0.15f, 11.0f, 0.0f, 0);
                add(659.0f, 659.0f, 0.15f, 11.0f, 0.09f, 0);
                add(784.0f, 784.0f, 0.15f, 10.0f, 0.18f, 0);
                add(1046.0f, 1046.0f, 0.14f, 9.0f, 0.27f, 0);
                break;
            case Cue::GameOver:
                add(392.0f, 392.0f, 0.18f, 3.0f, 0.0f, 2);
                add(330.0f, 330.0f, 0.18f, 3.0f, 0.18f, 2);
                add(262.0f, 262.0f, 0.18f, 3.0f, 0.36f, 2);
                add(196.0f, 196.0f, 0.20f, 2.2f, 0.56f, 2);
                break;
        }
    }

protected:
    qint64 readData(char* data, qint64 maxSize) override {
        const int frames = static_cast<int>(maxSize / 4);  // int16 stereo
        if (frames <= 0) {
            return 0;
        }
        auto* out = reinterpret_cast<qint16*>(data);
        const float dt = 1.0f / static_cast<float>(kSampleRate);
        const float targetGain = muted_ ? 0.0f : 0.85f;

        for (int i = 0; i < frames; ++i) {
            float left = 0.0f;
            float right = 0.0f;
            for (Voice& voice : voices_) {
                if (!voice.active) {
                    continue;
                }
                if (voice.delay > 0.0f) {
                    voice.delay -= dt;
                    continue;
                }
                voice.phase += voice.freq * dt;
                voice.phase -= std::floor(voice.phase);

                float sample = 0.0f;
                switch (voice.wave) {
                    case 0: sample = std::sin(voice.phase * 6.2831853f); break;
                    case 1: sample = noise(); break;
                    case 2: sample = 2.0f * voice.phase - 1.0f; break;
                    case 3: sample = voice.phase < 0.5f ? 1.0f : -1.0f; break;
                    default: sample = 1.0f - 4.0f * std::fabs(voice.phase - 0.5f); break;
                }

                left += sample * voice.amp * (1.0f - voice.pan);
                right += sample * voice.amp * (1.0f + voice.pan);

                voice.freq += (voice.freqEnd - voice.freq) * std::min(1.0f, dt * 24.0f);
                voice.amp *= std::exp(-voice.decay * dt);
                if (voice.amp < 0.0004f) {
                    voice.active = false;
                }
            }

            gain_ += (targetGain - gain_) * std::min(1.0f, dt * 14.0f);
            music_.render(left, right);
            left = std::tanh(left * gain_ * 1.15f);
            right = std::tanh(right * gain_ * 1.15f);

            out[i * 2] = static_cast<qint16>(std::clamp(left, -1.0f, 1.0f) * 32767.0f);
            out[i * 2 + 1] = static_cast<qint16>(std::clamp(right, -1.0f, 1.0f) * 32767.0f);
        }
        return static_cast<qint64>(frames) * 4;
    }

    qint64 writeData(const char*, qint64) override { return -1; }

private:
    void add(float freq, float freqEnd, float amp, float decay, float delay, int wave) {
        Voice* slot = nullptr;
        for (Voice& voice : voices_) {
            if (!voice.active) {
                slot = &voice;
                break;
            }
        }
        if (slot == nullptr) {
            // Tutte occupate: ruba la voce piu' debole (mai un crash).
            slot = &voices_[0];
            for (Voice& voice : voices_) {
                if (voice.amp < slot->amp) {
                    slot = &voice;
                }
            }
        }
        slot->phase = 0.0f;
        slot->freq = freq;
        slot->freqEnd = freqEnd;
        slot->amp = amp;
        slot->decay = decay;
        slot->delay = delay;
        slot->pan = 0.0f;
        slot->wave = wave;
        slot->active = true;
    }

    float noise() {
        rng_ ^= rng_ << 13;
        rng_ ^= rng_ >> 17;
        rng_ ^= rng_ << 5;
        return (static_cast<float>(rng_ >> 8) / 8388608.0f) - 1.0f;
    }

    std::array<Voice, kVoiceCount> voices_{};
    MusicSynth music_;
    std::uint32_t rng_ = 0x1234567u;
    float gain_ = 0.0f;
    bool muted_ = false;
};

}  // namespace

struct AudioEngine::Impl {
    QAudioSink* sink = nullptr;
    MixerDevice* device = nullptr;
    bool muted = false;
};

void AudioEngine::ImplDeleter::operator()(Impl* impl) const { delete impl; }

AudioEngine::AudioEngine() : impl_(new Impl()) {}
AudioEngine::~AudioEngine() { stop(); }

bool AudioEngine::start() {
    if (impl_->sink != nullptr) {
        return true;
    }
    const QAudioDevice output = QMediaDevices::defaultAudioOutput();
    if (output.isNull()) {
        qInfo() << "audio: nessun dispositivo di uscita, gioco muto";
        return false;
    }

    QAudioFormat format;
    format.setSampleRate(MixerDevice::kSampleRate);
    format.setChannelCount(2);
    format.setSampleFormat(QAudioFormat::Int16);
    if (!output.isFormatSupported(format)) {
        qInfo() << "audio: formato 48 kHz int16 non supportato, gioco muto";
        return false;
    }

    impl_->device = new MixerDevice();
    impl_->device->setMuted(impl_->muted);
    impl_->sink = new QAudioSink(output, format);
    impl_->sink->setBufferSize(8192);
    impl_->sink->start(impl_->device);
    if (impl_->sink->error() != QAudio::NoError) {
        qInfo() << "audio: avvio del sink fallito, gioco muto";
        stop();
        return false;
    }
    return true;
}

void AudioEngine::stop() {
    if (impl_->sink != nullptr) {
        impl_->sink->stop();
        delete impl_->sink;
        impl_->sink = nullptr;
    }
    delete impl_->device;
    impl_->device = nullptr;
}

void AudioEngine::play(Cue cue) {
    if (impl_->device != nullptr) {
        impl_->device->trigger(cue);
    }
}

void AudioEngine::setMuted(bool muted) {
    impl_->muted = muted;
    if (impl_->device != nullptr) {
        impl_->device->setMuted(muted);
    }
}

bool AudioEngine::muted() const { return impl_->muted; }

bool AudioEngine::available() const { return impl_->sink != nullptr; }

}  // namespace w3
