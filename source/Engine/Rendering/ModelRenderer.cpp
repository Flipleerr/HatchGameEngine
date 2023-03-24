#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/Rendering/3D.h>
#include <Engine/Rendering/FaceInfo.h>
#include <Engine/Rendering/VertexBuffer.h>
#include <Engine/Rendering/PolygonRenderer.h>
#include <Engine/ResourceTypes/IModel.h>
#include <Engine/Math/Matrix4x4.h>

class ModelRenderer {
public:
    PolygonRenderer* PolyRenderer;
    VertexBuffer*    Buffer;

    VertexAttribute* AttribBuffer;
    VertexAttribute* Vertex;
    FaceInfo*        FaceItem;

    Matrix4x4*       ModelMatrix;
    Matrix4x4*       ViewMatrix;
    Matrix4x4*       ProjectionMatrix;
    Matrix4x4*       NormalMatrix;

    Matrix4x4        MVPMatrix;

    bool             ClipFaces;
    Armature*        ArmaturePtr;

    Uint32           DrawMode;
    Uint32           CurrentColor;
};
#endif

#include <Engine/Rendering/ModelRenderer.h>
#include <Engine/Rendering/PolygonRenderer.h>
#include <Engine/Utilities/ColorUtils.h>

PRIVATE void ModelRenderer::Init() {
    FaceItem = &Buffer->FaceInfoBuffer[Buffer->FaceCount];
    AttribBuffer = Vertex = &Buffer->Vertices[Buffer->VertexCount];

    ClipFaces = false;
    ArmaturePtr = nullptr;
}

PUBLIC ModelRenderer::ModelRenderer(PolygonRenderer* polyRenderer) {
    PolyRenderer = polyRenderer;
    Buffer = PolyRenderer->VertexBuf;

    Init();
}

PUBLIC ModelRenderer::ModelRenderer(VertexBuffer* buffer) {
    PolyRenderer = nullptr;
    Buffer = buffer;

    Init();
}

PUBLIC void ModelRenderer::SetMatrices(Matrix4x4* model, Matrix4x4* view, Matrix4x4* projection, Matrix4x4* normal) {
    ModelMatrix = model;
    ViewMatrix = view;
    ProjectionMatrix = projection;
    NormalMatrix = normal;
}

PRIVATE void ModelRenderer::AddFace(int faceVertexCount, Material* material) {
    FaceItem->DrawMode = DrawMode;
    FaceItem->NumVertices = faceVertexCount;
    FaceItem->SetBlendState(Graphics::GetBlendState());

    if (material)
        FaceItem->SetMaterial(material);
    else
        FaceItem->UseMaterial = false;

    FaceItem++;

    Buffer->FaceCount++;
    Buffer->VertexCount += faceVertexCount;
}

PRIVATE int ModelRenderer::ClipFace(int faceVertexCount) {
    if (!ClipFaces)
        return faceVertexCount;

    Vertex = AttribBuffer;

    if (!PolygonRenderer::CheckPolygonVisible(Vertex, faceVertexCount))
        return 0;
    else if (PolyRenderer && PolyRenderer->ClipPolygonsByFrustum) {
        PolygonClipBuffer clipper;

        faceVertexCount = PolyRenderer->ClipPolygon(clipper, Vertex, faceVertexCount);
        if (faceVertexCount == 0)
            return 0;

        Uint32 maxVertexCount = Buffer->VertexCount + faceVertexCount;
        if (maxVertexCount > Buffer->Capacity) {
            Buffer->Resize(maxVertexCount);
            FaceItem = &Buffer->FaceInfoBuffer[Buffer->FaceCount];
            AttribBuffer = Vertex = &Buffer->Vertices[Buffer->VertexCount];
        }

        PolygonRenderer::CopyVertices(clipper.Buffer, Vertex, faceVertexCount);
    }

    Vertex += faceVertexCount;
    AttribBuffer = Vertex;

    return faceVertexCount;
}

PRIVATE void ModelRenderer::DrawMesh(IModel* model, Mesh* mesh, Skeleton* skeleton, Matrix4x4& mvpMatrix) {
    Material* material = mesh->MaterialIndex != -1 ? model->Materials[mesh->MaterialIndex] : nullptr;

    Sint16* modelVertexIndexPtr = mesh->VertexIndexBuffer;

    int vertexTypeMask = VertexType_Position | VertexType_Normal | VertexType_Color | VertexType_UV;
    int color = CurrentColor;

    Vector3* positionPtr;
    Vector3* normalPtr;
    Uint32* colorPtr;
    Vector2* uvPtr;

    Vector3* positionBuffer = mesh->PositionBuffer;
    Vector3* normalBuffer = mesh->NormalBuffer;

    if (skeleton) {
        positionBuffer = skeleton->TransformedPositions;
        normalBuffer = skeleton->TransformedNormals;
    }

    switch (mesh->VertexFlag & vertexTypeMask) {
        case VertexType_Position:
            // For every face,
            while (*modelVertexIndexPtr != -1) {
                int faceVertexCount = model->VertexPerFace;

                // For every vertex index,
                int numVertices = faceVertexCount;
                while (numVertices--) {
                    positionPtr = &positionBuffer[*modelVertexIndexPtr];
                    APPLY_MAT4X4(Vertex->Position, positionPtr[0], mvpMatrix.Values);
                    Vertex->Color = color;
                    modelVertexIndexPtr++;
                    Vertex++;
                }

                if ((faceVertexCount = ClipFace(faceVertexCount)))
                    AddFace(faceVertexCount, material);
            }
            break;
        case VertexType_Position | VertexType_Normal:
            if (NormalMatrix) {
                // For every face,
                while (*modelVertexIndexPtr != -1) {
                    int faceVertexCount = model->VertexPerFace;

                    // For every vertex index,
                    int numVertices = faceVertexCount;
                    while (numVertices--) {
                        positionPtr = &positionBuffer[*modelVertexIndexPtr];
                        normalPtr = &normalBuffer[*modelVertexIndexPtr];
                        // Calculate position
                        APPLY_MAT4X4(Vertex->Position, positionPtr[0], mvpMatrix.Values);
                        // Calculate normals
                        APPLY_MAT4X4(Vertex->Normal, normalPtr[0], NormalMatrix->Values);
                        Vertex->Color = color;
                        modelVertexIndexPtr++;
                        Vertex++;
                    }

                    if ((faceVertexCount = ClipFace(faceVertexCount)))
                        AddFace(faceVertexCount, material);
                }
            }
            else {
                // For every face,
                while (*modelVertexIndexPtr != -1) {
                    int faceVertexCount = model->VertexPerFace;

                    // For every vertex index,
                    int numVertices = faceVertexCount;
                    while (numVertices--) {
                        positionPtr = &positionBuffer[*modelVertexIndexPtr];
                        normalPtr = &normalBuffer[*modelVertexIndexPtr];
                        // Calculate position
                        APPLY_MAT4X4(Vertex->Position, positionPtr[0], mvpMatrix.Values);
                        COPY_NORMAL(Vertex->Normal, normalPtr[0]);
                        Vertex->Color = color;
                        modelVertexIndexPtr++;
                        Vertex++;
                    }

                    if ((faceVertexCount = ClipFace(faceVertexCount)))
                        AddFace(faceVertexCount, material);
                }
            }
            break;
        case VertexType_Position | VertexType_Normal | VertexType_Color:
            if (NormalMatrix) {
                // For every face,
                while (*modelVertexIndexPtr != -1) {
                    int faceVertexCount = model->VertexPerFace;

                    // For every vertex index,
                    int numVertices = faceVertexCount;
                    while (numVertices--) {
                        positionPtr = &positionBuffer[*modelVertexIndexPtr];
                        normalPtr = &normalBuffer[*modelVertexIndexPtr];
                        colorPtr = &mesh->ColorBuffer[*modelVertexIndexPtr];
                        APPLY_MAT4X4(Vertex->Position, positionPtr[0], mvpMatrix.Values);
                        APPLY_MAT4X4(Vertex->Normal, normalPtr[0], NormalMatrix->Values);
                        Vertex->Color = ColorUtils::Tint(colorPtr[0], color);
                        modelVertexIndexPtr++;
                        Vertex++;
                    }

                    if ((faceVertexCount = ClipFace(faceVertexCount)))
                        AddFace(faceVertexCount, material);
                }
            }
            else {
                // For every face,
                while (*modelVertexIndexPtr != -1) {
                    int faceVertexCount = model->VertexPerFace;

                    // For every vertex index,
                    int numVertices = faceVertexCount;
                    while (numVertices--) {
                        positionPtr = &positionBuffer[*modelVertexIndexPtr];
                        normalPtr = &normalBuffer[*modelVertexIndexPtr];
                        colorPtr = &mesh->ColorBuffer[*modelVertexIndexPtr];
                        APPLY_MAT4X4(Vertex->Position, positionPtr[0], mvpMatrix.Values);
                        COPY_NORMAL(Vertex->Normal, normalPtr[0]);
                        Vertex->Color = ColorUtils::Tint(colorPtr[0], color);
                        modelVertexIndexPtr++;
                        Vertex++;
                    }

                    if ((faceVertexCount = ClipFace(faceVertexCount)))
                        AddFace(faceVertexCount, material);
                }
            }
            break;
        case VertexType_Position | VertexType_Normal | VertexType_UV:
            if (NormalMatrix) {
                // For every face,
                while (*modelVertexIndexPtr != -1) {
                    int faceVertexCount = model->VertexPerFace;

                    // For every vertex index,
                    int numVertices = faceVertexCount;
                    while (numVertices--) {
                        positionPtr = &positionBuffer[*modelVertexIndexPtr];
                        normalPtr = &normalBuffer[*modelVertexIndexPtr];
                        uvPtr = &mesh->UVBuffer[*modelVertexIndexPtr];
                        APPLY_MAT4X4(Vertex->Position, positionPtr[0], mvpMatrix.Values);
                        APPLY_MAT4X4(Vertex->Normal, normalPtr[0], NormalMatrix->Values);
                        Vertex->Color = color;
                        Vertex->UV = uvPtr[0];
                        modelVertexIndexPtr++;
                        Vertex++;
                    }

                    if ((faceVertexCount = ClipFace(faceVertexCount)))
                        AddFace(faceVertexCount, material);
                }
            }
            else {
                // For every face,
                while (*modelVertexIndexPtr != -1) {
                    int faceVertexCount = model->VertexPerFace;

                    // For every vertex index,
                    int numVertices = faceVertexCount;
                    while (numVertices--) {
                        positionPtr = &positionBuffer[*modelVertexIndexPtr];
                        normalPtr = &normalBuffer[*modelVertexIndexPtr];
                        uvPtr = &mesh->UVBuffer[*modelVertexIndexPtr];
                        APPLY_MAT4X4(Vertex->Position, positionPtr[0], mvpMatrix.Values);
                        COPY_NORMAL(Vertex->Normal, normalPtr[0]);
                        Vertex->Color = color;
                        Vertex->UV = uvPtr[0];
                        modelVertexIndexPtr++;
                        Vertex++;
                    }

                    if ((faceVertexCount = ClipFace(faceVertexCount)))
                        AddFace(faceVertexCount, material);
                }
            }
            break;
        case VertexType_Position | VertexType_Normal | VertexType_UV | VertexType_Color:
            if (NormalMatrix) {
                // For every face,
                while (*modelVertexIndexPtr != -1) {
                    int faceVertexCount = model->VertexPerFace;

                    // For every vertex index,
                    int numVertices = faceVertexCount;
                    while (numVertices--) {
                        positionPtr = &positionBuffer[*modelVertexIndexPtr];
                        normalPtr = &normalBuffer[*modelVertexIndexPtr];
                        uvPtr = &mesh->UVBuffer[*modelVertexIndexPtr];
                        colorPtr = &mesh->ColorBuffer[*modelVertexIndexPtr];
                        APPLY_MAT4X4(Vertex->Position, positionPtr[0], mvpMatrix.Values);
                        APPLY_MAT4X4(Vertex->Normal, normalPtr[0], NormalMatrix->Values);
                        Vertex->Color = ColorUtils::Tint(colorPtr[0], color);
                        Vertex->UV = uvPtr[0];
                        modelVertexIndexPtr++;
                        Vertex++;
                    }

                    if ((faceVertexCount = ClipFace(faceVertexCount)))
                        AddFace(faceVertexCount, material);
                }
            }
            else {
                // For every face,
                while (*modelVertexIndexPtr != -1) {
                    int faceVertexCount = model->VertexPerFace;

                    // For every vertex index,
                    int numVertices = faceVertexCount;
                    while (numVertices--) {
                        positionPtr = &positionBuffer[*modelVertexIndexPtr];
                        normalPtr = &normalBuffer[*modelVertexIndexPtr];
                        uvPtr = &mesh->UVBuffer[*modelVertexIndexPtr];
                        colorPtr = &mesh->ColorBuffer[*modelVertexIndexPtr];
                        APPLY_MAT4X4(Vertex->Position, positionPtr[0], mvpMatrix.Values);
                        COPY_NORMAL(Vertex->Normal, normalPtr[0]);
                        Vertex->Color = ColorUtils::Tint(colorPtr[0], color);
                        Vertex->UV = uvPtr[0];
                        modelVertexIndexPtr++;
                        Vertex++;
                    }

                    if ((faceVertexCount = ClipFace(faceVertexCount)))
                        AddFace(faceVertexCount, material);
                }
            }
            break;
    }
}

PRIVATE void ModelRenderer::DrawNode(IModel* model, ModelNode* node, Matrix4x4* world) {
    size_t numMeshes = node->Meshes.size();
    size_t numChildren = node->Children.size();

    Matrix4x4::Multiply(world, world, node->TransformMatrix);
    Matrix4x4 nodeToWorldMat;
    bool madeMatrix = false;

    for (size_t i = 0; i < numMeshes; i++) {
        Mesh* mesh = node->Meshes[i];

        if (mesh->SkeletonIndex != -1) // in world space
            DrawMesh(model, mesh, ArmaturePtr->Skeletons[mesh->SkeletonIndex], MVPMatrix);
        else {
            if (!madeMatrix) {
                if (ViewMatrix && ProjectionMatrix) {
                    Matrix4x4::Multiply(&nodeToWorldMat, world, ModelMatrix);
                    Matrix4x4::Multiply(&nodeToWorldMat, &nodeToWorldMat, ViewMatrix);
                    Matrix4x4::Multiply(&nodeToWorldMat, &nodeToWorldMat, ProjectionMatrix);
                } else
                    Matrix4x4::Multiply(&nodeToWorldMat, world, ModelMatrix);

                madeMatrix = true;
            }

            DrawMesh(model, mesh, nullptr, nodeToWorldMat);
        }
    }

    for (size_t i = 0; i < numChildren; i++)
        DrawNode(model, node->Children[i], world);
}

PUBLIC void ModelRenderer::DrawModel(IModel* model, Uint32 frame) {
    if (ViewMatrix && ProjectionMatrix)
        Graphics::CalculateMVPMatrix(&MVPMatrix, ModelMatrix, ViewMatrix, ProjectionMatrix);
    else
        Graphics::CalculateMVPMatrix(&MVPMatrix, ModelMatrix, NULL, NULL);

    if (!model->UseVertexAnimation) {
        Matrix4x4 identity;
        Matrix4x4::Identity(&identity);

        DrawNode(model, model->BaseArmature->RootNode, &identity);
    }
    else {
        static Skeleton vertexAnimSkeleton;

        frame = model->GetKeyFrame(frame) % model->FrameCount;

        // Just render every mesh directly
        for (size_t i = 0; i < model->MeshCount; i++) {
            Mesh* mesh = model->Meshes[i];

            Skeleton *skeletonPtr = nullptr;
            if (frame) {
                skeletonPtr = &vertexAnimSkeleton;
                skeletonPtr->TransformedPositions = mesh->PositionBuffer + (frame * model->VertexCount);
                skeletonPtr->TransformedNormals = mesh->NormalBuffer + (frame * model->VertexCount);
            }

            DrawMesh(model, mesh, skeletonPtr, MVPMatrix);
        }
    }
}

PUBLIC void ModelRenderer::DrawModel(IModel* model, Uint16 animation, Uint32 frame) {
    if (!model->UseVertexAnimation) {
        if (ArmaturePtr == nullptr)
            ArmaturePtr = model->BaseArmature;

        Uint16 numAnims = model->AnimationCount;
        if (numAnims > 0) {
            if (animation >= numAnims)
                animation = numAnims - 1;

            model->Animate(ArmaturePtr, model->Animations[animation], frame);
        }
    }

    DrawModel(model, frame);
}
