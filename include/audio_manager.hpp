#pragma once

/*
 * audio_manager.hpp
 * -----------------
 * Sistema de audio centralizado basado en miniaudio.
 *
 * Características:
 *  - SFX (explosión, bomba, pickup) precargados en RAM → latencia < 0.1ms.
 *  - Múltiples instancias simultáneas del mismo SFX (chain de explosiones).
 *  - BGM en streaming (MP3/WAV sin cargar todo en RAM).
 *  - Loop de BGM sin glitch.
 *  - Control de volumen por canal.
 *  - No bloquea nunca el hilo principal.
 */

#include <string>

// Forward-declare la estructura interna para no exponer miniaudio en todos los .cpp
struct ma_engine;

enum class VfxSound {
    PlaceBomb,
    Explosion,
    Pickup
};

class AudioManager {
public:
    // Singleton
    static AudioManager& get();

    // Ciclo de vida
    bool init(const std::string& basePath);  // basePath = directorio raíz de recursos
    void shutdown();

    // SFX: dispara un sonido de efecto de sonido — retorna inmediatamente, sin bloquear
    void playVfx(VfxSound sfx);

    // Música de fondo
    void playBgm(const std::string& absPath, bool loop = true, float volume = 0.6f);
    void stopBgm();
    bool isBgmFinished() const;
    float getBgmProgress01() const; // [0..1], -1 si no se puede calcular.

    // Volúmenes globales (0.0 – 1.0)
    void setVfxVolume(float v);
    void setBgmVolume(float v);

private:
    AudioManager() = default;
    ~AudioManager() { shutdown(); }
    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;

    // Implementación interna oculta (Pimpl — definida en audio_manager.cpp)
    struct Impl;
    Impl* impl = nullptr;
};
