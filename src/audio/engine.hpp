#pragma once
// engine.hpp — audio procedurale. Nessun file WAV: i suoni sono sintetizzati
// a runtime in un QIODevice in modalita' pull. Se non c'e' un dispositivo il
// gioco resta muto, senza eccezioni.

namespace w3 {

enum class Cue {
    EatApple,
    EatGolden,
    EatChili,
    EatBlueberry,
    EatLemon,
    EatPoison,
    EatStar,
    EatMagnet,
    EatBomb,
    EatHeart,
    EatCrystal,
    Power,
    Bounce,
    Hurt,
    Explosion,
    LevelUp,
    GameOver,
};

class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();
    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    bool start();
    void stop();
    void play(Cue cue);
    void setMuted(bool muted);
    bool muted() const;
    bool available() const;

private:
    struct Impl;
    struct ImplDeleter {
        void operator()(Impl* impl) const;
    };
    Impl* impl_ = nullptr;
};

}  // namespace w3
