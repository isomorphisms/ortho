#ifndef ORTHANT_MODEL_H
#define ORTHANT_MODEL_H

/* Generated from src/Orthant.idric by src/Generate.idric. */
static const float ORTHANT_FACE_POSITIONS[] = {
    0.0f, 0.0f, 0.0f,
    1.0f, 0.0f, 0.0f,
    1.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f,
    1.0f, 1.0f, 0.0f,
    0.0f, 1.0f, 0.0f,

    0.0f, 0.0f, 0.0f,
    1.0f, 0.0f, 0.0f,
    1.0f, 0.0f, 1.0f,
    0.0f, 0.0f, 0.0f,
    1.0f, 0.0f, 1.0f,
    0.0f, 0.0f, 1.0f,

    0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f,
    0.0f, 1.0f, 1.0f,
    0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 1.0f,
    0.0f, 0.0f, 1.0f
};
#define ORTHANT_FACE_VERTEX_COUNT 18

static const float ORTHANT_EDGE_POSITIONS[] = {
    0.0f, 0.0f, 0.0f,
    1.25f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f,
    0.0f, 1.25f, 0.0f,
    0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.25f
};
#define ORTHANT_EDGE_VERTEX_COUNT 6

#endif
