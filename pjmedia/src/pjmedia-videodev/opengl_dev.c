/*
 * Copyright (C) 2013-2014 Teluu Inc. (http://www.teluu.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <pjmedia-videodev/videodev_imp.h>
#include <pj/assert.h>
#include <pj/log.h>

#if defined(PJMEDIA_HAS_VIDEO) && PJMEDIA_HAS_VIDEO != 0 && \
    defined(PJMEDIA_VIDEO_DEV_HAS_OPENGL) && \
    PJMEDIA_VIDEO_DEV_HAS_OPENGL != 0

#include <pjmedia-videodev/opengl_dev.h>
#ifdef PJMEDIA_VIDEO_DEV_HAS_OPENGL_ES
#   if PJ_ANDROID
#       include <GLES2/gl2.h>
#       include <GLES2/gl2ext.h>
#       undef GL_RGBA
#       define GL_RGBA GL_BGRA_EXT
#       define GL_BGRA GL_BGRA_EXT
#   else
#       include <OpenGLES/ES2/gl.h>
#       include <OpenGLES/ES2/glext.h>
#   endif
#else
#   include <GL/gl.h>
#   include <GL/glext.h>
#endif

#define THIS_FILE               "opengl_dev.c"
#define DEFAULT_CLOCK_RATE      90000
#define DEFAULT_WIDTH           480
#define DEFAULT_HEIGHT          360
#define DEFAULT_FPS             15

#if PJ_ANDROID
#    define LOG(a) PJ_LOG(3, (THIS_FILE, a))
#else
#    define LOG(a)
#endif

enum {
    ATTRIB_VERTEX,
    ATTRIB_TEXTUREPOSITON,
    NUM_ATTRIBUTES
};

/* Vertex and fragment shaders */
static const GLchar *vertSrc = " \
attribute vec4 position; \
attribute vec4 inTexCoord; \
varying vec2 texCoord; \
void main() \
{ \
    gl_Position = position; \
    texCoord = inTexCoord.xy; \
} \
";
static const GLchar *fragSrc = " \
varying highp vec2 texCoord; \
uniform sampler2D videoFrame; \
void main() \
{ \
    gl_FragColor = texture2D(videoFrame, texCoord); \
} \
";

/* OpenGL buffers structure. */
struct gl_buffers {
    GLuint      frameBuf;
    GLuint      rendBuf;
    GLuint      rendTex;
    GLuint      directProg;
    
    int         rendBufW;
    int         rendBufH;
    pj_bool_t   direct;
};

/* Supported formats */
static pjmedia_format_id opengl_fmts[] = {PJMEDIA_FORMAT_BGRA};

/* opengl device info */
struct opengl_dev_info
{
    pjmedia_vid_dev_info         info;
};

/* opengl factory */
struct opengl_factory
{
    pjmedia_vid_dev_factory      base;
    pj_pool_t                   *pool;
    pj_pool_factory             *pf;
    
    unsigned                     dev_count;
    struct opengl_dev_info      *dev_info;
};

/* Prototypes */
static pj_status_t opengl_factory_init(pjmedia_vid_dev_factory *f);
static pj_status_t opengl_factory_destroy(pjmedia_vid_dev_factory *f);
static pj_status_t opengl_factory_refresh(pjmedia_vid_dev_factory *f);
static unsigned    opengl_factory_get_dev_count(pjmedia_vid_dev_factory *f);
static pj_status_t opengl_factory_get_dev_info(pjmedia_vid_dev_factory *f,
                                               unsigned index,
                                               pjmedia_vid_dev_info *info);
static pj_status_t opengl_factory_default_param(pj_pool_t *pool,
                                                pjmedia_vid_dev_factory *f,
                                                unsigned index,
                                                pjmedia_vid_dev_param *param);
static pj_status_t
opengl_factory_create_stream(pjmedia_vid_dev_factory *f,
                             pjmedia_vid_dev_param *param,
                             const pjmedia_vid_dev_cb *cb,
                             void *user_data,
                             pjmedia_vid_dev_stream **p_vid_strm);

/* Operations */
static pjmedia_vid_dev_factory_op factory_op =
{
    &opengl_factory_init,
    &opengl_factory_destroy,
    &opengl_factory_get_dev_count,
    &opengl_factory_get_dev_info,
    &opengl_factory_default_param,
    &opengl_factory_create_stream,
    &opengl_factory_refresh
};

/****************************************************************************
 * OpenGL utility functions
 */
/* Compile a shader from the provided source(s) */
GLint compile_shader(GLenum target, GLsizei count, const GLchar **sources,
                     GLuint *shader)
{
    GLint status;
    
    *shader = glCreateShader(target);
    glShaderSource(*shader, count, sources, NULL);
    glCompileShader(*shader);
    
    glGetShaderiv(*shader, GL_COMPILE_STATUS, &status);
    
    return status;
}

/* Create program, compile shader, link program, bind attributes */
GLint create_program(const GLchar *vertSource, const GLchar *fragSource,
                     GLsizei attribNameCnt, const GLchar **attribNames,
                     const GLint *attribLocations, GLuint *program)
{
    GLuint vertShader = 0, fragShader = 0, prog = 0, i;
    GLint status;
    
    /* Create shader program */
    prog = glCreateProgram();
    *program = prog;
    
    /* Create and compile vertex shader */
    status = compile_shader(GL_VERTEX_SHADER, 1, &vertSource, &vertShader);
    if (status == 0) {
        LOG("Unable to compile vertex shader");
        return status;
    }
    
    /* Create and compile fragment shader */
    status = compile_shader(GL_FRAGMENT_SHADER, 1, &fragSource, &fragShader);
    if (status == 0) {
        LOG("Unable to compile fragment shader");
        return status;
    }
    
    /* Attach vertex shader to program */
    glAttachShader(prog, vertShader);
    
    /* Attach fragment shader to program */
    glAttachShader(prog, fragShader);
    
    /* Bind attribute locations prior to linking */
    for (i = 0; i < attribNameCnt; i++) {
        glBindAttribLocation(prog, attribLocations[i], attribNames[i]);
    }
    
    /* Link program */
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &status);
    if (status == 0) {
        LOG("Unable to link program");
        return status;
    }
    
    /* Release vertex and fragment shaders */
    if (vertShader)
        glDeleteShader(vertShader);
    if (fragShader)
        glDeleteShader(fragShader);
    
    return status;
}

void pjmedia_vid_dev_opengl_create_buffers(pj_pool_t *pool, pj_bool_t direct,
                                           gl_buffers **glb)
{
    gl_buffers *glbuf = PJ_POOL_ZALLOC_T(pool, gl_buffers);
    
    *glb = glbuf;
    glDisable(GL_DEPTH_TEST);
    
    if (!(glbuf->direct = direct)) {
        glGenFramebuffers(1, &glbuf->frameBuf);
        glBindFramebuffer(GL_FRAMEBUFFER, glbuf->frameBuf);
    
        glGenRenderbuffers(1, &glbuf->rendBuf);
        glBindRenderbuffer(GL_RENDERBUFFER, glbuf->rendBuf);
    }
    
    glGenTextures(1, &glbuf->rendTex);
}

pj_status_t pjmedia_vid_dev_opengl_init_buffers(gl_buffers *glb)
{
    /* Attributes */
    GLint attribLocation[NUM_ATTRIBUTES] = { ATTRIB_VERTEX,
        ATTRIB_TEXTUREPOSITON };
    GLchar *attribName[NUM_ATTRIBUTES] = { "position", "texCoord" };
    
    if (!glb->direct ) {
        glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH,
                                     &glb->rendBufW);
        glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT,
                                     &glb->rendBufH);
    
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                  GL_RENDERBUFFER, glb->rendBuf);
        if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            LOG("Unable to create frame buffer");
            return -1;
        }
    }
    
    create_program(vertSrc, fragSrc, NUM_ATTRIBUTES,
                   (const GLchar **)&attribName[0], attribLocation,
                   &glb->directProg);
    
    if (!glb->directProg) {
        LOG("Unable to create program");
        return -2;
    }
    
    return PJ_SUCCESS;
}

pj_status_t pjmedia_vid_dev_opengl_draw(gl_buffers *glb, unsigned int width,
                                        unsigned int height, void *pixels)
{
    static const GLfloat squareVertices[] = {
        -1.0f, -1.0f,
        1.0f, -1.0f,
        -1.0f,  1.0f,
        1.0f,  1.0f,
    };
    GLfloat textureVertices[] = {
        0, 1, 1, 1, 0, 0, 1, 0
    };

    glBindTexture(GL_TEXTURE_2D, glb->rendTex);
    
    /* Set texture parameters */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)width, (GLsizei)height,
                 0, GL_BGRA, GL_UNSIGNED_BYTE, (GLvoid *)pixels);
    
    glFlush();
    
    /* Do we render directly to the screen? */
    glBindFramebuffer(GL_FRAMEBUFFER, (glb->direct? 0: glb->frameBuf));
    
    /* Set the view port to the entire view */
    glViewport(0, 0, (glb->direct? width: glb->rendBufW),
               (glb->direct? height: glb->rendBufH));
    
    /* Draw the texture on the screen with OpenGL ES 2 */
    /* Use program */
    glUseProgram(glb->directProg);
    
    /* Update attribute values */
    glVertexAttribPointer(ATTRIB_VERTEX, 2, GL_FLOAT, 0, 0, squareVertices);
    glEnableVertexAttribArray(ATTRIB_VERTEX);
    glVertexAttribPointer(ATTRIB_TEXTUREPOSITON, 2, GL_FLOAT, 0, 0,
                          textureVertices);
    glEnableVertexAttribArray(ATTRIB_TEXTUREPOSITON);
    
    /* Update uniform values if there are any */
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    
    /* Present */
    if (!glb->direct)
        glBindRenderbuffer(GL_RENDERBUFFER, glb->rendBuf);
    
    return PJ_SUCCESS;
}

void pjmedia_vid_dev_opengl_destroy_buffers(gl_buffers *glb)
{
    if (glb->frameBuf) {
        glDeleteFramebuffers(1, &glb->frameBuf);
        glb->frameBuf = 0;
    }
    
    if (glb->rendBuf) {
        glDeleteRenderbuffers(1, &glb->rendBuf);
        glb->rendBuf = 0;
    }
    
    if (glb->rendTex) {
        glDeleteTextures(1, &glb->rendTex);
        glb->rendTex = 0;
    }
    
    if (glb->directProg) {
        glDeleteProgram(glb->directProg);
        glb->directProg = 0;
    }
}

/****************************************************************************
 * Factory operations
 */
/*
 * Init opengl video driver.
 */
pjmedia_vid_dev_factory* pjmedia_opengl_factory(pj_pool_factory *pf)
{
    struct opengl_factory *f;
    pj_pool_t *pool;
    
    pool = pj_pool_create(pf, "opengl rend", 512, 512, NULL);
    f = PJ_POOL_ZALLOC_T(pool, struct opengl_factory);
    f->pf = pf;
    f->pool = pool;
    f->base.op = &factory_op;
    
    return &f->base;
}

/* API: init factory */
static pj_status_t opengl_factory_init(pjmedia_vid_dev_factory *f)
{
    struct opengl_factory *qf = (struct opengl_factory*)f;
    struct opengl_dev_info *qdi;
    unsigned l;
    
    /* Initialize input and output devices here */
    qf->dev_info = (struct opengl_dev_info*)
    pj_pool_calloc(qf->pool, 1, sizeof(struct opengl_dev_info));
    
    qf->dev_count = 0;
    qdi = &qf->dev_info[qf->dev_count++];
    pj_bzero(qdi, sizeof(*qdi));
    pj_ansi_strxcpy(qdi->info.name, "OpenGL renderer", sizeof(qdi->info.name));
    pj_ansi_strxcpy(qdi->info.driver, "OpenGL", sizeof(qdi->info.driver));
    qdi->info.dir = PJMEDIA_DIR_RENDER;
    qdi->info.has_callback = PJ_FALSE;
    qdi->info.caps = PJMEDIA_VID_DEV_CAP_FORMAT;
    qdi->info.fmt_cnt = PJ_ARRAY_SIZE(opengl_fmts);
    qdi->info.caps |= pjmedia_vid_dev_opengl_imp_get_cap();
        
    for (l = 0; l < PJ_ARRAY_SIZE(opengl_fmts); l++) {
        pjmedia_format *fmt = &qdi->info.fmt[l];
        pjmedia_format_init_video(fmt, opengl_fmts[l], DEFAULT_WIDTH,
                                  DEFAULT_HEIGHT, DEFAULT_FPS, 1);
    }
    
    PJ_LOG(4, (THIS_FILE, "OpenGL device initialized"));
    
    return PJ_SUCCESS;
}

/* API: destroy factory */
static pj_status_t opengl_factory_destroy(pjmedia_vid_dev_factory *f)
{
    struct opengl_factory *qf = (struct opengl_factory*)f;
    pj_pool_t *pool = qf->pool;
    
    qf->pool = NULL;
    pj_pool_release(pool);
    
    return PJ_SUCCESS;
}

/* API: refresh the list of devices */
static pj_status_t opengl_factory_refresh(pjmedia_vid_dev_factory *f)
{
    PJ_UNUSED_ARG(f);
    return PJ_SUCCESS;
}

/* API: get number of devices */
static unsigned opengl_factory_get_dev_count(pjmedia_vid_dev_factory *f)
{
    struct opengl_factory *qf = (struct opengl_factory*)f;
    return qf->dev_count;
}

/* API: get device info */
static pj_status_t opengl_factory_get_dev_info(pjmedia_vid_dev_factory *f,
                                              unsigned index,
                                              pjmedia_vid_dev_info *info)
{
    struct opengl_factory *qf = (struct opengl_factory*)f;
    
    PJ_ASSERT_RETURN(index < qf->dev_count, PJMEDIA_EVID_INVDEV);
    
    pj_memcpy(info, &qf->dev_info[index].info, sizeof(*info));
    
    return PJ_SUCCESS;
}

/* API: create default device parameter */
static pj_status_t opengl_factory_default_param(pj_pool_t *pool,
                                               pjmedia_vid_dev_factory *f,
                                               unsigned index,
                                               pjmedia_vid_dev_param *param)
{
    struct opengl_factory *qf = (struct opengl_factory*)f;
    struct opengl_dev_info *di = &qf->dev_info[index];
    
    PJ_ASSERT_RETURN(index < qf->dev_count, PJMEDIA_EVID_INVDEV);
    
    PJ_UNUSED_ARG(pool);
    
    pj_bzero(param, sizeof(*param));
    if (di->info.dir & PJMEDIA_DIR_RENDER) {
        param->dir = PJMEDIA_DIR_RENDER;
        param->rend_id = index;
        param->cap_id = PJMEDIA_VID_INVALID_DEV;
    } else {
        return PJMEDIA_EVID_INVDEV;
    }
    
    param->flags = PJMEDIA_VID_DEV_CAP_FORMAT;
    param->clock_rate = DEFAULT_CLOCK_RATE;
    pj_memcpy(&param->fmt, &di->info.fmt[0], sizeof(param->fmt));
    
    return PJ_SUCCESS;
}

/* API: create stream */
static pj_status_t
opengl_factory_create_stream(pjmedia_vid_dev_factory *f,
                             pjmedia_vid_dev_param *param,
                             const pjmedia_vid_dev_cb *cb,
                             void *user_data,
                             pjmedia_vid_dev_stream **p_vid_strm)
{
    struct opengl_factory *qf = (struct opengl_factory*)f;
    pj_pool_t *pool;
    const pjmedia_video_format_info *vfi;
    
    PJ_ASSERT_RETURN(f && param && p_vid_strm, PJ_EINVAL);
    PJ_ASSERT_RETURN(param->fmt.type == PJMEDIA_TYPE_VIDEO &&
                     param->fmt.detail_type == PJMEDIA_FORMAT_DETAIL_VIDEO &&
                     (param->dir == PJMEDIA_DIR_CAPTURE ||
                      param->dir == PJMEDIA_DIR_RENDER),
                     PJ_EINVAL);
    
    vfi = pjmedia_get_video_format_info(NULL, param->fmt.id);
    if (!vfi)
        return PJMEDIA_EVID_BADFORMAT;
    
    /* Create and Initialize stream descriptor */
    pool = pj_pool_create(qf->pf, "opengl-dev", 4000, 4000, NULL);
    PJ_ASSERT_RETURN(pool != NULL, PJ_ENOMEM);

    return pjmedia_vid_dev_opengl_imp_create_stream(pool, param, cb,
                                                    user_data, p_vid_strm);
}

#endif  /* PJMEDIA_VIDEO_DEV_HAS_OPENGL */
