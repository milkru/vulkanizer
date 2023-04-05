#ifndef SHADER_CONSTANTS_H
#define SHADER_CONSTANTS_H

// TODO-MILKRU: Move this file to the cpp side, once shader define passing gets implemented.

// TODO-MILKRU: Thread Group Size can be reflected.
const int kShaderGroupSizeNV = 32;
const int kMaxVerticesPerMeshlet = 64;
const int kMaxTrianglesPerMeshlet = 124;
const int kMaxMeshLods = 3; // TODO-MILKUR: This value is temp, revert to 12
const int kFrustumPlaneCount = 5;

#endif // SHADER_CONSTANTS_H
