#include "cinematic_player.hpp"
#include "sprite_atlas.hpp"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cctype>
#include <iostream>
#include <string>
#include <vector>

namespace {
constexpr float kOverlayCanvasWidth = 1920.0f;
constexpr float kOverlayCanvasHeight = 1080.0f;

glm::vec2 overlayPixelsToOrtho(float centerXpx, float centerYpx, float aspect) {
    const float xNdc = (centerXpx / kOverlayCanvasWidth) * 2.0f - 1.0f;
    const float yNdc = 1.0f - (centerYpx / kOverlayCanvasHeight) * 2.0f;
    return glm::vec2(xNdc * aspect, yNdc);
}

glm::vec2 overlaySizeToOrthoHalfExtents(float widthPx, float heightPx, float aspect) {
    const float halfWidth = (widthPx / kOverlayCanvasWidth) * aspect;
    const float halfHeight = (heightPx / kOverlayCanvasHeight);
    return glm::vec2(halfWidth, halfHeight);
}

bool resolveOrangeGlyphSpriteName(char glyph, std::string& spriteName) {
    const char upperGlyph = static_cast<char>(std::toupper(static_cast<unsigned char>(glyph)));

    if ((upperGlyph >= '0' && upperGlyph <= '9') ||
        (upperGlyph >= 'A' && upperGlyph <= 'Z') ||
        upperGlyph == '!') {
        spriteName = std::string(1, upperGlyph) + "_Nar";
        return true;
    }

    return false;
}

bool drawOrangeVocabGlyph(char glyph,
                          const SpriteAtlas& vocabAtlas,
                          GLuint vocabTexture,
                          float centerXpx,
                          float centerYpx,
                          float glyphWidthPx,
                          float glyphHeightPx,
                          float aspect,
                          GLuint uniformModel,
                          GLuint uniformUvRect,
                          GLuint uniformTintColor,
                          GLuint uniformFlipX) {
    if (vocabTexture == 0 || vocabAtlas.sprites.empty()) {
        return false;
    }

    std::string spriteName;
    if (!resolveOrangeGlyphSpriteName(glyph, spriteName)) {
        return false;
    }

    glm::vec4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);
    if (!getUvRectForSprite(vocabAtlas, spriteName, uvRect)) {
        return false;
    }

    const glm::vec2 center = overlayPixelsToOrtho(centerXpx, centerYpx, aspect);
    const glm::vec2 halfExtents = overlaySizeToOrthoHalfExtents(glyphWidthPx, glyphHeightPx, aspect);

    glm::mat4 model(1.0f);
    model = glm::translate(model, glm::vec3(center.x, center.y, 0.0f));
    model = glm::scale(model, glm::vec3(halfExtents.x, halfExtents.y, 1.0f));

    glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));
    glUniform4fv(uniformUvRect, 1, glm::value_ptr(uvRect));
    glUniform4f(uniformTintColor, 1.0f, 1.0f, 1.0f, 1.0f);
    glUniform1f(uniformFlipX, 0.0f);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, vocabTexture);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    return true;
}
} // namespace

// ============================== Ctor / Dtor ==============================

CinematicPlayer::CinematicPlayer() {}

CinematicPlayer::~CinematicPlayer() {
    close();
}

// ============================== open ==============================

bool CinematicPlayer::open(const std::string& videoPath) {
    close(); // limpiar estado previo

    // --- Abrir fichero ---
    if (avformat_open_input(&formatCtx, videoPath.c_str(), nullptr, nullptr) != 0) {
        std::cerr << "[CinematicPlayer] No se pudo abrir: " << videoPath << std::endl;
        return false;
    }

    if (avformat_find_stream_info(formatCtx, nullptr) < 0) {
        std::cerr << "[CinematicPlayer] No se encontro info de streams" << std::endl;
        close();
        return false;
    }

    // --- Buscar stream de video ---
    videoStreamIndex = -1;
    for (unsigned i = 0; i < formatCtx->nb_streams; i++) {
        if (formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIndex = (int)i;
            break;
        }
    }

    if (videoStreamIndex < 0) {
        std::cerr << "[CinematicPlayer] No hay stream de video" << std::endl;
        close();
        return false;
    }

    AVStream* videoStream = formatCtx->streams[videoStreamIndex];
    timeBase = av_q2d(videoStream->time_base);

    // --- Abrir codec ---
    const AVCodec* codec = avcodec_find_decoder(videoStream->codecpar->codec_id);
    if (!codec) {
        std::cerr << "[CinematicPlayer] Codec no encontrado" << std::endl;
        close();
        return false;
    }

    codecCtx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codecCtx, videoStream->codecpar);

    if (avcodec_open2(codecCtx, codec, nullptr) < 0) {
        std::cerr << "[CinematicPlayer] No se pudo abrir el codec" << std::endl;
        close();
        return false;
    }

    videoWidth  = codecCtx->width;
    videoHeight = codecCtx->height;

    // --- Frames y buffer RGB ---
    frame    = av_frame_alloc();
    frameRGB = av_frame_alloc();

    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, videoWidth, videoHeight, 1);
    rgbBuffer = (uint8_t*)av_malloc(numBytes);
    av_image_fill_arrays(frameRGB->data, frameRGB->linesize, rgbBuffer,
                         AV_PIX_FMT_RGB24, videoWidth, videoHeight, 1);

    // --- Scaler (pixel format del video -> RGB24) ---
    swsCtx = sws_getContext(videoWidth, videoHeight, codecCtx->pix_fmt,
                            videoWidth, videoHeight, AV_PIX_FMT_RGB24,
                            SWS_BILINEAR, nullptr, nullptr, nullptr);

    // --- Paquete ---
    packet = av_packet_alloc();

    // --- Textura OpenGL ---
    glGenTextures(1, &videoTexture);
    glBindTexture(GL_TEXTURE_2D, videoTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, videoWidth, videoHeight, 0,
                 GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);

    // --- Estado inicial ---
    elapsedTime       = 0.0;
    currentFramePts   = 0.0;
    finished          = false;
    skipped           = false;
    opened            = true;
    firstFrameDecoded = false;

    // Decodificar primer frame inmediatamente
    decodeNextFrame();

    // Sincronizar reloj con el PTS del primer frame
    elapsedTime = currentFramePts;

    std::cout << "[CinematicPlayer] Abierto: " << videoPath
              << " (" << videoWidth << "x" << videoHeight << ")" << std::endl;

    return true;
}

// ============================== Decode ==============================

bool CinematicPlayer::decodeNextFrame() {
    if (!formatCtx || !codecCtx) return false;

    while (av_read_frame(formatCtx, packet) >= 0) {
        if (packet->stream_index == videoStreamIndex) {
            int ret = avcodec_send_packet(codecCtx, packet);
            if (ret < 0) {
                av_packet_unref(packet);
                continue;
            }

            ret = avcodec_receive_frame(codecCtx, frame);
            if (ret == 0) {
                // Convertir a RGB24
                sws_scale(swsCtx, frame->data, frame->linesize, 0, videoHeight,
                          frameRGB->data, frameRGB->linesize);

                // Actualizar PTS
                if (frame->pts != AV_NOPTS_VALUE) {
                    currentFramePts = frame->pts * timeBase;
                }

                uploadFrameToTexture();
                firstFrameDecoded = true;
                av_packet_unref(packet);
                return true;
            }
        }
        av_packet_unref(packet);
    }

    // Fin del fichero
    finished = true;
    return false;
}

void CinematicPlayer::uploadFrameToTexture() {
    if (!videoTexture || !rgbBuffer) return;

    glBindTexture(GL_TEXTURE_2D, videoTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, videoWidth, videoHeight,
                    GL_RGB, GL_UNSIGNED_BYTE, rgbBuffer);
    glBindTexture(GL_TEXTURE_2D, 0);
}

// ============================== Update ==============================

void CinematicPlayer::update(float deltaTime) {
    if (finished || skipped || !opened) return;

    elapsedTime += (double)deltaTime;

    // Avanzar frames hasta sincronizar con el tiempo transcurrido
    while (!finished && currentFramePts < elapsedTime) {
        if (!decodeNextFrame()) {
            break;
        }
    }
}

// ============================== Render ==============================

void CinematicPlayer::render(GLuint VAO, GLuint shader,
                              GLuint uniformModel, GLuint uniformProjection,
                              GLuint uniformTexture, GLuint uniformUvRect,
                              GLuint uniformTintColor, GLuint uniformFlipX,
                              int WIDTH, int HEIGHT) {
    if (!opened || !videoTexture || !firstFrameDecoded) return;

    glUseProgram(shader);

    // --- Proyeccion: letterbox/pillarbox manteniendo aspect ratio del video ---
    float windowAspect = (float)WIDTH / (float)HEIGHT;
    float videoAspect  = (float)videoWidth / (float)videoHeight;

    glm::mat4 projection;
    if (windowAspect > videoAspect) {
        // Ventana mas ancha que el video -> pillarbox (barras laterales)
        float scale = windowAspect / videoAspect;
        projection = glm::ortho(-scale, scale, -1.0f, 1.0f, -1.0f, 1.0f);
    } else {
        // Ventana mas alta que el video -> letterbox (barras arriba/abajo)
        float scale = videoAspect / windowAspect;
        projection = glm::ortho(-1.0f, 1.0f, -scale, scale, -1.0f, 1.0f);
    }

    glUniformMatrix4fv(uniformProjection, 1, GL_FALSE, glm::value_ptr(projection));
    glUniform1i(uniformTexture, 0);

    // Model = identidad (el quad [-1,1] ya llena la proyeccion)
    glm::mat4 model(1.0f);
    glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));

    // UV rect completo
    glUniform4f(uniformUvRect, 0.0f, 0.0f, 1.0f, 1.0f);

    // Sin tint ni flip
    glUniform4f(uniformTintColor, 1.0f, 1.0f, 1.0f, 1.0f);
    glUniform1f(uniformFlipX, 0.0f);

    // Bind textura del video
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, videoTexture);

    // Dibujar quad fullscreen
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    glUseProgram(0);
}

// ============================== Overlay (Level X Match) ==============================

void CinematicPlayer::renderWithLevelOverlay(GLuint VAO, GLuint shader,
                                             GLuint uniformModel, GLuint uniformProjection,
                                             GLuint uniformTexture, GLuint uniformUvRect,
                                             GLuint uniformTintColor, GLuint uniformFlipX,
                                             int WIDTH, int HEIGHT,
                                             int levelNumber,
                                             void* vocabAtlasPtr,
                                             GLuint vocabTexture) {
    if (!opened || !videoTexture || !firstFrameDecoded) {
        return;
    }

    // Renderizar video primero
    render(VAO, shader, uniformModel, uniformProjection, uniformTexture, uniformUvRect,
           uniformTintColor, uniformFlipX, WIDTH, HEIGHT);

    // Si no tenemos el atlas o la textura, no renderizamos overlay
    if (!vocabAtlasPtr || vocabTexture == 0 || VAO == 0) {
        return;
    }

    const SpriteAtlas& vocabAtlas = *reinterpret_cast<const SpriteAtlas*>(vocabAtlasPtr);

    // Parámetros ajustables para el overlay
    const float glyphWidthPx = 60.0f;
    const float glyphHeightPx = 67.0f;
    const float spacingPx = 8.0f;
    const float posYpx = 106.0f;
    const float posXpx = 980.0f;

    // Compilar el texto "LEVEL X MATCH!"
    std::string text = "LEVEL ";
    text += std::to_string(levelNumber);
    text += " MATCH!";

    glUseProgram(shader);

    const float canvasAspect = kOverlayCanvasWidth / kOverlayCanvasHeight;

    // Primero: calcular anchos de cada glifo para centrar el texto
    std::vector<float> glyphWidths;
    glyphWidths.reserve(text.length());
    float totalWidthPx = 0.0f;

    for (size_t i = 0; i < text.length(); ++i) {
        const float widthPx = (text[i] == ' ') ? glyphWidthPx * 0.60f : glyphWidthPx;
        glyphWidths.push_back(widthPx);
        totalWidthPx += widthPx;
        if (i + 1 < text.length()) totalWidthPx += spacingPx;
    }

    if (glyphWidths.empty()) {
        glUseProgram(0);
        return;
    }

    // Calcular posición X inicial (centrada)
    float currentCenterXpx = posXpx - (totalWidthPx * 0.5f) + (glyphWidths.front() * 0.5f);

    // Proyección ortho 2D (usando canvas dimensions como en custom_game_menu)
    glm::mat4 projection = glm::ortho(-canvasAspect, canvasAspect, -1.0f, 1.0f, -1.0f, 1.0f);
    glUniformMatrix4fv(uniformProjection, 1, GL_FALSE, glm::value_ptr(projection));
    glUniform1i(uniformTexture, 0);
    glUniform1f(uniformFlipX, 0.0f);

    glBindVertexArray(VAO);

    // Renderizar cada carácter
    for (size_t i = 0; i < text.length(); ++i) {
        if (text[i] == ' ') {
            currentCenterXpx += glyphWidths[i] * 0.5f + spacingPx + (i + 1 < text.length() ? glyphWidths[i + 1] * 0.5f : 0.0f);
            continue;
        }

        drawOrangeVocabGlyph(text[i],
                             vocabAtlas,
                             vocabTexture,
                             currentCenterXpx,
                             posYpx,
                             glyphWidths[i],
                             glyphHeightPx,
                             canvasAspect,
                             uniformModel,
                             uniformUvRect,
                             uniformTintColor,
                             uniformFlipX);

        // Mover a la siguiente posición
        currentCenterXpx += glyphWidths[i] * 0.5f + spacingPx + (i + 1 < text.length() ? glyphWidths[i + 1] * 0.5f : 0.0f);
    }

    glBindVertexArray(0);
    glUseProgram(0);
}

// ============================== Control ==============================

void CinematicPlayer::skip() {
    skipped  = true;
    finished = true;
}

bool CinematicPlayer::isFinished() const {
    return finished;
}

// ============================== Cleanup ==============================

void CinematicPlayer::close() {
    if (videoTexture) {
        glDeleteTextures(1, &videoTexture);
        videoTexture = 0;
    }

    if (rgbBuffer) {
        av_free(rgbBuffer);
        rgbBuffer = nullptr;
    }

    if (frameRGB) {
        av_frame_free(&frameRGB);
        frameRGB = nullptr;
    }

    if (frame) {
        av_frame_free(&frame);
        frame = nullptr;
    }

    if (packet) {
        av_packet_free(&packet);
        packet = nullptr;
    }

    if (swsCtx) {
        sws_freeContext(swsCtx);
        swsCtx = nullptr;
    }

    if (codecCtx) {
        avcodec_free_context(&codecCtx);
        codecCtx = nullptr;
    }

    if (formatCtx) {
        avformat_close_input(&formatCtx);
        formatCtx = nullptr;
    }

    opened           = false;
    finished         = false;
    skipped          = false;
    firstFrameDecoded = false;
    videoStreamIndex = -1;
}
