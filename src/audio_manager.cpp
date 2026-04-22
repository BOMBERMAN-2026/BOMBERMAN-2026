/*
 * audio_manager.cpp
 * -----------------
 * Implementación del sistema de audio con miniaudio.
 *
 * IMPORTANTE: Este archivo define MINIAUDIO_IMPLEMENTATION una sola vez.
 * Ningún otro .cpp debe incluir miniaudio.h con ese define.
 */

#define MINIAUDIO_IMPLEMENTATION
#include "external/miniaudio.h"

#include "audio_manager.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>

// resolveAssetPath está definida en sprite_atlas.cpp y usada en todo el proyecto.
// Busca el archivo en varias ubicaciones relativas (build/, ../, etc.).
extern std::string resolveAssetPath(const std::string& path);

// ============================================================
// Configuración de voces simultáneas
// ============================================================
static constexpr int kMaxVoices = 12;  // hasta 12 explosiones simultáneas

// ============================================================
// Tipo de voz (pool de SFX simultáneos)
// ============================================================
struct SfxVoice {
    ma_sound sound;
    bool active = false;
};

// ============================================================
// Estructura interna (Pimpl)
// ============================================================
struct AudioManager::Impl {
    ma_engine engine;
    bool engineReady = false;

    // Sonidos SFX precargados (decodificados en RAM)
    ma_sound sfxExplosion;
    ma_sound sfxPlace;
    ma_sound sfxPickup;
    bool sfxReady = false;

    // Pool de voces clonadas para SFX simultáneos
    std::vector<SfxVoice> explosionPool;
    std::vector<SfxVoice> placePool;
    std::vector<SfxVoice> pickupPool;

    // BGM en streaming
    ma_sound bgmSound;
    bool bgmActive = false;

    // Volumen
    float vfxVolume = 1.0f;
    float bgmVolume = 0.6f;

    std::string basePath;

    // Mutex ligero solo para el pool de voces
    std::mutex poolMutex;
};

// ============================================================
// Singleton
// ============================================================
AudioManager& AudioManager::get() {
    static AudioManager instance;
    return instance;
}

// ============================================================
// Helpers internos
// ============================================================
static std::string normalizePath(const std::string& p) {
    std::string r = p;
    for (char& c : r) {
        if (c == '\\') c = '/';
    }
    return r;
}

// Busca una voz libre en el pool, la marca como activa y reproduce.
// Si no hay voz libre, descarta el sonido (mejor que bloquear).
static void fireFromPool(ma_engine* engine,
                         ma_sound* prototype,
                         std::vector<SfxVoice>& pool,
                         float volume)
{
    for (auto& v : pool) {
        // Si la voz está activa pero terminó, liberarla
        if (v.active && !ma_sound_is_playing(&v.sound)) {
            ma_sound_uninit(&v.sound);
            v.active = false;
        }
        if (!v.active) {
            ma_result r = ma_sound_init_copy(engine, prototype, 0, nullptr, &v.sound);
            if (r != MA_SUCCESS) continue;
            ma_sound_set_volume(&v.sound, volume);
            ma_sound_start(&v.sound);
            v.active = true;
            return;
        }
    }
    // Pool llena: ignorar (evita bloquear)
}

// ============================================================
// init
// ============================================================
bool AudioManager::init(const std::string& basePath) {
    if (impl) return true;  // Ya inicializado

    impl = new Impl();
    impl->basePath = normalizePath(basePath);

    // Configuración del engine miniaudio
    ma_engine_config cfg = ma_engine_config_init();
    cfg.listenerCount = 1;
    // Usamos el backend por defecto del sistema (WASAPI en Windows → latencia baja)

    ma_result r = ma_engine_init(&cfg, &impl->engine);
    if (r != MA_SUCCESS) {
        std::cerr << "[AudioManager] Error al inicializar ma_engine: " << r << std::endl;
        delete impl;
        impl = nullptr;
        return false;
    }
    impl->engineReady = true;

    // Resolver paths de SFX usando el mismo sistema que el resto del proyecto
    auto loadSfx = [&](ma_sound& snd, const char* relPath) -> bool {
        std::string path = resolveAssetPath(relPath);
        ma_result res = ma_sound_init_from_file(
            &impl->engine, path.c_str(),
            MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_NO_SPATIALIZATION,
            nullptr, nullptr, &snd);
        if (res != MA_SUCCESS) {
            std::cerr << "[AudioManager] No se pudo cargar SFX: " << path
                      << " (error " << res << ")" << std::endl;
            return false;
        }
        return true;
    };

    bool ok = true;
    ok &= loadSfx(impl->sfxExplosion, "resources/sounds/VFX/explosion.wav");
    ok &= loadSfx(impl->sfxPlace,     "resources/sounds/VFX/ponerbomba.wav");
    ok &= loadSfx(impl->sfxPickup,    "resources/sounds/VFX/cogerpowerup.wav");

    impl->sfxReady = ok;

    // Inicializar pools de voces (preasignadas, sin sonido todavía)
    impl->explosionPool.resize(kMaxVoices);
    impl->placePool.resize(kMaxVoices);
    impl->pickupPool.resize(kMaxVoices);

    if (!ok) {
        std::cerr << "[AudioManager] Advertencia: algunos SFX no se cargaron." << std::endl;
    }

    return impl->engineReady;
}

// ============================================================
// shutdown
// ============================================================
void AudioManager::shutdown() {
    if (!impl) return;

    // Liberar voces activas del pool
    for (auto& v : impl->explosionPool) if (v.active) { ma_sound_uninit(&v.sound); v.active = false; }
    for (auto& v : impl->placePool)     if (v.active) { ma_sound_uninit(&v.sound); v.active = false; }
    for (auto& v : impl->pickupPool)    if (v.active) { ma_sound_uninit(&v.sound); v.active = false; }

    // Liberar BGM
    if (impl->bgmActive) {
        ma_sound_stop(&impl->bgmSound);
        ma_sound_uninit(&impl->bgmSound);
        impl->bgmActive = false;
    }

    // Liberar prototipos SFX
    if (impl->sfxReady) {
        ma_sound_uninit(&impl->sfxExplosion);
        ma_sound_uninit(&impl->sfxPlace);
        ma_sound_uninit(&impl->sfxPickup);
        impl->sfxReady = false;
    }

    // Apagar engine
    if (impl->engineReady) {
        ma_engine_uninit(&impl->engine);
        impl->engineReady = false;
    }

    delete impl;
    impl = nullptr;
}

// ============================================================
// playVfx
// ============================================================
void AudioManager::playVfx(VfxSound sfx) {
    if (!impl || !impl->engineReady || !impl->sfxReady) return;

    std::lock_guard<std::mutex> lock(impl->poolMutex);

    switch (sfx) {
        case VfxSound::PlaceBomb:
            fireFromPool(&impl->engine, &impl->sfxPlace,
                         impl->placePool, impl->vfxVolume);
            break;
        case VfxSound::Explosion:
            fireFromPool(&impl->engine, &impl->sfxExplosion,
                         impl->explosionPool, impl->vfxVolume);
            break;
        case VfxSound::Pickup:
            fireFromPool(&impl->engine, &impl->sfxPickup,
                         impl->pickupPool, impl->vfxVolume);
            break;
    }
}

// ============================================================
// playBgm
// ============================================================
void AudioManager::playBgm(const std::string& absPath, bool loop, float volume) {
    if (!impl || !impl->engineReady) return;

    // Parar la BGM anterior si existe
    stopBgm();

    std::string path = normalizePath(absPath);

    // MA_SOUND_FLAG_STREAM: streaming desde disco (no cargar MP3 entero en RAM)
    ma_uint32 flags = MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_NO_SPATIALIZATION;

    ma_result r = ma_sound_init_from_file(
        &impl->engine, path.c_str(), flags,
        nullptr, nullptr, &impl->bgmSound);

    if (r != MA_SUCCESS) {
        std::cerr << "[AudioManager] No se pudo abrir BGM: " << path
                  << " (error " << r << ")" << std::endl;
        return;
    }

    ma_sound_set_looping(&impl->bgmSound, loop ? MA_TRUE : MA_FALSE);
    float v = (volume > 0.0f) ? volume : impl->bgmVolume;
    ma_sound_set_volume(&impl->bgmSound, v);
    ma_sound_start(&impl->bgmSound);
    impl->bgmActive = true;
    impl->bgmVolume = v;
}

// ============================================================
// stopBgm
// ============================================================
void AudioManager::stopBgm() {
    if (!impl || !impl->bgmActive) return;
    ma_sound_stop(&impl->bgmSound);
    ma_sound_uninit(&impl->bgmSound);
    impl->bgmActive = false;
}

// ============================================================
// isBgmFinished
// ============================================================
bool AudioManager::isBgmFinished() const {
    if (!impl || !impl->bgmActive) return true;
    return !ma_sound_is_playing(&impl->bgmSound);
}

float AudioManager::getBgmProgress01() const {
    if (!impl || !impl->bgmActive) return -1.0f;

    ma_uint64 lengthFrames = 0;
    ma_uint64 cursorFrames = 0;

    const ma_result lenRes = ma_sound_get_length_in_pcm_frames(&impl->bgmSound, &lengthFrames);
    const ma_result cursorRes = ma_sound_get_cursor_in_pcm_frames(&impl->bgmSound, &cursorFrames);

    if (lenRes != MA_SUCCESS || cursorRes != MA_SUCCESS || lengthFrames == 0) {
        return -1.0f;
    }

    const double ratio = (double)cursorFrames / (double)lengthFrames;
    if (ratio <= 0.0) return 0.0f;
    if (ratio >= 1.0) return 1.0f;
    return (float)ratio;
}

// ============================================================
// Volúmenes
// ============================================================
void AudioManager::setVfxVolume(float v) {
    if (!impl) return;
    impl->vfxVolume = v;
}

void AudioManager::setBgmVolume(float v) {
    if (!impl) return;
    impl->bgmVolume = v;
    if (impl->bgmActive) {
        ma_sound_set_volume(&impl->bgmSound, v);
    }
}
