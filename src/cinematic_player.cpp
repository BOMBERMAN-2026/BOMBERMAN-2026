#include "cinematic_player.hpp"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>

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
