#include <android/input.h>
#include <android/log.h>
#include <android/native_window.h>
#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "orthant_model.h"

#define LOG_TAG "Ortho"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

struct mat4 {
    float m[16];
};

struct engine {
    struct android_app *app;
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    int32_t width;
    int32_t height;

    GLuint program;
    GLuint face_vao;
    GLuint face_vbo;
    GLuint edge_vao;
    GLuint edge_vbo;
    GLint mvp_location;
    GLint color_location;
    GLint point_size_location;

    float yaw;
    float pitch;
    bool dragging;
    float last_x;
    float last_y;
    bool dirty;
};

static const char *VERTEX_SHADER =
    "#version 300 es\n"
    "precision highp float;\n"
    "layout(location = 0) in vec3 a_position;\n"
    "uniform mat4 u_mvp;\n"
    "uniform float u_point_size;\n"
    "void main() {\n"
    "    gl_Position = u_mvp * vec4(a_position, 1.0);\n"
    "    gl_PointSize = u_point_size;\n"
    "}\n";

static const char *FRAGMENT_SHADER =
    "#version 300 es\n"
    "precision mediump float;\n"
    "uniform vec4 u_color;\n"
    "out vec4 out_color;\n"
    "void main() {\n"
    "    out_color = u_color;\n"
    "}\n";

static struct mat4 mat4_identity(void) {
    struct mat4 result = {{
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    }};
    return result;
}

static struct mat4 mat4_multiply(struct mat4 left, struct mat4 right) {
    struct mat4 result = {{0}};
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            float value = 0.0f;
            for (int k = 0; k < 4; ++k) {
                value += left.m[k * 4 + row] * right.m[column * 4 + k];
            }
            result.m[column * 4 + row] = value;
        }
    }
    return result;
}

static struct mat4 mat4_translation(float x, float y, float z) {
    struct mat4 result = mat4_identity();
    result.m[12] = x;
    result.m[13] = y;
    result.m[14] = z;
    return result;
}

static struct mat4 mat4_rotation_x(float angle) {
    struct mat4 result = mat4_identity();
    float cosine = cosf(angle);
    float sine = sinf(angle);
    result.m[5] = cosine;
    result.m[6] = sine;
    result.m[9] = -sine;
    result.m[10] = cosine;
    return result;
}

static struct mat4 mat4_rotation_y(float angle) {
    struct mat4 result = mat4_identity();
    float cosine = cosf(angle);
    float sine = sinf(angle);
    result.m[0] = cosine;
    result.m[2] = -sine;
    result.m[8] = sine;
    result.m[10] = cosine;
    return result;
}

static struct mat4 mat4_perspective(float fovy, float aspect, float near_plane, float far_plane) {
    struct mat4 result = {{0}};
    float f = 1.0f / tanf(0.5f * fovy);
    result.m[0] = f / aspect;
    result.m[5] = f;
    result.m[10] = (far_plane + near_plane) / (near_plane - far_plane);
    result.m[11] = -1.0f;
    result.m[14] = (2.0f * far_plane * near_plane) / (near_plane - far_plane);
    return result;
}

static GLuint compile_shader(GLenum type, const char *source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_TRUE) {
        return shader;
    }

    GLint log_length = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
    char *log = log_length > 0 ? malloc((size_t)log_length) : NULL;
    if (log != NULL) {
        glGetShaderInfoLog(shader, log_length, NULL, log);
        LOGE("shader compilation failed: %s", log);
        free(log);
    } else {
        LOGE("shader compilation failed");
    }
    glDeleteShader(shader);
    return 0;
}

static GLuint link_program(GLuint vertex_shader, GLuint fragment_shader) {
    GLuint program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked == GL_TRUE) {
        return program;
    }

    GLint log_length = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);
    char *log = log_length > 0 ? malloc((size_t)log_length) : NULL;
    if (log != NULL) {
        glGetProgramInfoLog(program, log_length, NULL, log);
        LOGE("program link failed: %s", log);
        free(log);
    } else {
        LOGE("program link failed");
    }
    glDeleteProgram(program);
    return 0;
}

static bool create_renderer(struct engine *engine) {
    GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, VERTEX_SHADER);
    GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER, FRAGMENT_SHADER);
    if (vertex_shader == 0 || fragment_shader == 0) {
        if (vertex_shader != 0) glDeleteShader(vertex_shader);
        if (fragment_shader != 0) glDeleteShader(fragment_shader);
        return false;
    }

    engine->program = link_program(vertex_shader, fragment_shader);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    if (engine->program == 0) {
        return false;
    }

    engine->mvp_location = glGetUniformLocation(engine->program, "u_mvp");
    engine->color_location = glGetUniformLocation(engine->program, "u_color");
    engine->point_size_location = glGetUniformLocation(engine->program, "u_point_size");

    glGenVertexArrays(1, &engine->face_vao);
    glBindVertexArray(engine->face_vao);
    glGenBuffers(1, &engine->face_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, engine->face_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(ORTHANT_FACE_POSITIONS), ORTHANT_FACE_POSITIONS, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * (GLsizei)sizeof(float), (const void *)0);
    glEnableVertexAttribArray(0);

    glGenVertexArrays(1, &engine->edge_vao);
    glBindVertexArray(engine->edge_vao);
    glGenBuffers(1, &engine->edge_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, engine->edge_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(ORTHANT_EDGE_POSITIONS), ORTHANT_EDGE_POSITIONS, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * (GLsizei)sizeof(float), (const void *)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    LOGI("renderer ready: %s / %s", glGetString(GL_VERSION), glGetString(GL_RENDERER));
    return true;
}

static void destroy_renderer(struct engine *engine) {
    if (engine->face_vbo != 0) glDeleteBuffers(1, &engine->face_vbo);
    if (engine->edge_vbo != 0) glDeleteBuffers(1, &engine->edge_vbo);
    if (engine->face_vao != 0) glDeleteVertexArrays(1, &engine->face_vao);
    if (engine->edge_vao != 0) glDeleteVertexArrays(1, &engine->edge_vao);
    if (engine->program != 0) glDeleteProgram(engine->program);
    engine->face_vbo = 0;
    engine->edge_vbo = 0;
    engine->face_vao = 0;
    engine->edge_vao = 0;
    engine->program = 0;
}

static bool initialize_display(struct engine *engine) {
    if (engine->app->window == NULL) {
        return false;
    }

    const EGLint config_attributes[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 24,
        EGL_NONE
    };
    const EGLint context_attributes[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE
    };

    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY || !eglInitialize(display, NULL, NULL)) {
        LOGE("eglInitialize failed: 0x%x", eglGetError());
        return false;
    }

    EGLConfig config = NULL;
    EGLint config_count = 0;
    if (!eglChooseConfig(display, config_attributes, &config, 1, &config_count) || config_count != 1) {
        LOGE("could not choose GLES3 config: 0x%x", eglGetError());
        eglTerminate(display);
        return false;
    }

    EGLint format = 0;
    eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format);
    ANativeWindow_setBuffersGeometry(engine->app->window, 0, 0, format);

    EGLSurface surface = eglCreateWindowSurface(display, config, engine->app->window, NULL);
    if (surface == EGL_NO_SURFACE) {
        LOGE("eglCreateWindowSurface failed: 0x%x", eglGetError());
        eglTerminate(display);
        return false;
    }

    EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attributes);
    if (context == EGL_NO_CONTEXT) {
        LOGE("eglCreateContext failed: 0x%x", eglGetError());
        eglDestroySurface(display, surface);
        eglTerminate(display);
        return false;
    }

    if (!eglMakeCurrent(display, surface, surface, context)) {
        LOGE("eglMakeCurrent failed: 0x%x", eglGetError());
        eglDestroyContext(display, context);
        eglDestroySurface(display, surface);
        eglTerminate(display);
        return false;
    }

    engine->display = display;
    engine->surface = surface;
    engine->context = context;
    eglQuerySurface(display, surface, EGL_WIDTH, &engine->width);
    eglQuerySurface(display, surface, EGL_HEIGHT, &engine->height);

    if (!create_renderer(engine)) {
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroyContext(display, context);
        eglDestroySurface(display, surface);
        eglTerminate(display);
        engine->display = EGL_NO_DISPLAY;
        engine->surface = EGL_NO_SURFACE;
        engine->context = EGL_NO_CONTEXT;
        return false;
    }

    engine->dirty = true;
    return true;
}

static void terminate_display(struct engine *engine) {
    if (engine->display == EGL_NO_DISPLAY) {
        return;
    }

    eglMakeCurrent(engine->display, engine->surface, engine->surface, engine->context);
    destroy_renderer(engine);
    eglMakeCurrent(engine->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (engine->context != EGL_NO_CONTEXT) {
        eglDestroyContext(engine->display, engine->context);
    }
    if (engine->surface != EGL_NO_SURFACE) {
        eglDestroySurface(engine->display, engine->surface);
    }
    eglTerminate(engine->display);

    engine->display = EGL_NO_DISPLAY;
    engine->surface = EGL_NO_SURFACE;
    engine->context = EGL_NO_CONTEXT;
    engine->width = 0;
    engine->height = 0;
}

static struct mat4 current_mvp(const struct engine *engine) {
    float width = engine->width > 0 ? (float)engine->width : 1.0f;
    float height = engine->height > 0 ? (float)engine->height : 1.0f;
    float aspect = width / height;

    struct mat4 projection = mat4_perspective(0.82f, aspect, 0.1f, 20.0f);
    struct mat4 view = mat4_translation(0.0f, 0.0f, -3.2f);
    struct mat4 rotate_y = mat4_rotation_y(engine->yaw);
    struct mat4 rotate_x = mat4_rotation_x(engine->pitch);
    struct mat4 center = mat4_translation(-0.42f, -0.42f, -0.42f);

    return mat4_multiply(
        projection,
        mat4_multiply(view, mat4_multiply(rotate_y, mat4_multiply(rotate_x, center)))
    );
}

static void draw_frame(struct engine *engine) {
    if (engine->display == EGL_NO_DISPLAY || engine->surface == EGL_NO_SURFACE) {
        return;
    }

    eglQuerySurface(engine->display, engine->surface, EGL_WIDTH, &engine->width);
    eglQuerySurface(engine->display, engine->surface, EGL_HEIGHT, &engine->height);
    if (engine->width <= 0 || engine->height <= 0) {
        return;
    }

    glViewport(0, 0, engine->width, engine->height);
    glClearColor(0.025f, 0.028f, 0.035f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    struct mat4 mvp = current_mvp(engine);
    glUseProgram(engine->program);
    glUniformMatrix4fv(engine->mvp_location, 1, GL_FALSE, mvp.m);
    glUniform1f(engine->point_size_location, 1.0f);

    glBindVertexArray(engine->face_vao);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    glUniform4f(engine->color_location, 0.35f, 0.64f, 0.94f, 0.30f);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glUniform4f(engine->color_location, 0.94f, 0.49f, 0.34f, 0.30f);
    glDrawArrays(GL_TRIANGLES, 6, 6);
    glUniform4f(engine->color_location, 0.42f, 0.82f, 0.55f, 0.30f);
    glDrawArrays(GL_TRIANGLES, 12, 6);

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    glBindVertexArray(engine->edge_vao);
    glUniform4f(engine->color_location, 0.94f, 0.95f, 0.98f, 1.0f);
    glLineWidth(3.0f);
    glDrawArrays(GL_LINES, 0, ORTHANT_EDGE_VERTEX_COUNT);

    glUniform1f(engine->point_size_location, 15.0f);
    glDrawArrays(GL_POINTS, 0, 1);
    glBindVertexArray(0);

    if (!eglSwapBuffers(engine->display, engine->surface)) {
        LOGE("eglSwapBuffers failed: 0x%x", eglGetError());
    }
    engine->dirty = false;
}

static int32_t handle_input(struct android_app *app, AInputEvent *event) {
    struct engine *engine = app->userData;
    if (AInputEvent_getType(event) != AINPUT_EVENT_TYPE_MOTION) {
        return 0;
    }

    int32_t action = AMotionEvent_getAction(event);
    int32_t masked_action = action & AMOTION_EVENT_ACTION_MASK;
    size_t pointer_count = AMotionEvent_getPointerCount(event);

    switch (masked_action) {
        case AMOTION_EVENT_ACTION_DOWN:
            if (pointer_count == 1) {
                engine->dragging = true;
                engine->last_x = AMotionEvent_getX(event, 0);
                engine->last_y = AMotionEvent_getY(event, 0);
                return 1;
            }
            break;

        case AMOTION_EVENT_ACTION_POINTER_DOWN:
            engine->dragging = false;
            return 1;

        case AMOTION_EVENT_ACTION_MOVE:
            if (engine->dragging && pointer_count == 1) {
                float x = AMotionEvent_getX(event, 0);
                float y = AMotionEvent_getY(event, 0);
                float dx = x - engine->last_x;
                float dy = y - engine->last_y;
                engine->last_x = x;
                engine->last_y = y;

                float scale = 0.0105f;
                engine->yaw += dx * scale;
                engine->pitch += dy * scale;
                if (engine->pitch > 1.48f) engine->pitch = 1.48f;
                if (engine->pitch < -1.48f) engine->pitch = -1.48f;
                engine->dirty = true;
                return 1;
            }
            break;

        case AMOTION_EVENT_ACTION_UP:
        case AMOTION_EVENT_ACTION_CANCEL:
        case AMOTION_EVENT_ACTION_POINTER_UP:
            engine->dragging = false;
            return 1;

        default:
            break;
    }

    return 0;
}

static void handle_command(struct android_app *app, int32_t command) {
    struct engine *engine = app->userData;

    switch (command) {
        case APP_CMD_INIT_WINDOW:
            if (app->window != NULL && engine->display == EGL_NO_DISPLAY) {
                initialize_display(engine);
            }
            break;

        case APP_CMD_TERM_WINDOW:
            terminate_display(engine);
            break;

        case APP_CMD_WINDOW_RESIZED:
        case APP_CMD_CONFIG_CHANGED:
        case APP_CMD_GAINED_FOCUS:
            engine->dirty = true;
            break;

        default:
            break;
    }
}

void android_main(struct android_app *app) {
    struct engine engine;
    memset(&engine, 0, sizeof(engine));
    engine.app = app;
    engine.display = EGL_NO_DISPLAY;
    engine.surface = EGL_NO_SURFACE;
    engine.context = EGL_NO_CONTEXT;
    engine.yaw = 0.72f;
    engine.pitch = -0.48f;
    engine.dirty = true;

    app->userData = &engine;
    app->onAppCmd = handle_command;
    app->onInputEvent = handle_input;

    while (true) {
        int events = 0;
        struct android_poll_source *source = NULL;
        int timeout = engine.dirty ? 0 : -1;
        int poll_result = ALooper_pollOnce(timeout, NULL, &events, (void **)&source);

        if (poll_result >= 0 && source != NULL) {
            source->process(app, source);
        }

        if (app->destroyRequested != 0) {
            terminate_display(&engine);
            return;
        }

        if (engine.dirty && engine.display != EGL_NO_DISPLAY) {
            draw_frame(&engine);
        }
    }
}
