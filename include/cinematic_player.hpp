#ifndef CINEMATIC_PLAYER_HPP
#define CINEMATIC_PLAYER_HPP

#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <string>

// Forward declarations de FFmpeg (evitan incluir headers pesados aqui)
struct AVFormatContext;
struct AVCodecContext;
struct AVFrame;
struct AVPacket;
struct SwsContext;

/**
 * CinematicPlayer
 * ---------------
 * Reproductor de video reutilizable que decodifica un .mp4 (u otro formato
 * soportado por FFmpeg) frame a frame y lo sube a una textura OpenGL para
 * renderizarlo a pantalla completa.
 *
 * Uso tipico:
 *   CinematicPlayer player;
 *   player.open("resources/video/HistoryIntro.mp4");
 *   // en el game loop:
 *   player.update(deltaTime);
 *   player.render(VAO, shader, ...);
 *   if (player.isFinished()) { ... }
 *   // pulsar espacio -> player.skip();
 */
class CinematicPlayer {
private:
    // ===== FFmpeg =====
    AVFormatContext* formatCtx  = nullptr;
    AVCodecContext*  codecCtx   = nullptr;
    AVFrame*         frame      = nullptr;
    AVFrame*         frameRGB   = nullptr;
    AVPacket*        packet     = nullptr;
    SwsContext*      swsCtx     = nullptr;
    uint8_t*         rgbBuffer  = nullptr;
    int videoStreamIndex = -1;

    // ===== Info del video =====
    int    videoWidth  = 0;
    int    videoHeight = 0;
    double timeBase    = 0.0;

    // ===== Estado de reproduccion =====
    double elapsedTime      = 0.0;
    double currentFramePts  = 0.0;
    bool   finished         = false;
    bool   opened           = false;
    bool   skipped          = false;
    bool   firstFrameDecoded = false;

    // ===== OpenGL =====
    GLuint videoTexture = 0;

    bool decodeNextFrame();
    void uploadFrameToTexture();

public:
    CinematicPlayer();
    ~CinematicPlayer();

    /// Abre un fichero de video.  Devuelve false si falla.
    bool open(const std::string& videoPath);

    /// Avanza la reproduccion (llamar cada frame con deltaTime en segundos).
    void update(float deltaTime);

    /// Renderiza el frame actual a pantalla completa.
    void render(GLuint VAO, GLuint shader,
                GLuint uniformModel, GLuint uniformProjection,
                GLuint uniformTexture, GLuint uniformUvRect,
                GLuint uniformTintColor, GLuint uniformFlipX,
                int WIDTH, int HEIGHT);

    /// Salta la cinematica (marca como terminada).
    void skip();

    /// True si el video termino o fue saltado.
    bool isFinished() const;

    /// Libera todos los recursos de FFmpeg y OpenGL.
    void close();
};

#endif // CINEMATIC_PLAYER_HPP
