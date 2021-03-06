/*
    This file is part of Mechanica.

    Based on Magnum example

    Original authors — credit is appreciated but not required:

        2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019 —
            Vladimír Vondruš <mosra@centrum.cz>
        2019 — Nghia Truong <nghiatruong.vn@gmail.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.
 */


#pragma once

#include <vector>

#include <Magnum/GL/Buffer.h>
#include <Magnum/GL/Mesh.h>

#include <Magnum/SceneGraph/Camera.h>

#include <MxUniverse.h>
#include <rendering/MxRenderer.h>
#include <rendering/MxGlfwWindow.h>
#include <shaders/ParticleSphereShader.h>

#include <Corrade/Containers/Pointer.h>

#include <Corrade/Containers/Pointer.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Primitives/Cube.h>
#include <Magnum/Platform/GlfwApplication.h>
#include <Magnum/SceneGraph/MatrixTransformation3D.h>
#include <Magnum/Timeline.h>
#include <rendering/MxUniverseRenderer.h>
#include <rendering/MxGlfwWindow.h>

#include <rendering/MxWindow.h>

using namespace Magnum;

using Object3D = SceneGraph::Object<SceneGraph::MatrixTransformation3D>;
using Scene3D  = SceneGraph::Scene<SceneGraph::MatrixTransformation3D>;

class WireframeGrid;
class WireframeBox;



struct MxUniverseRenderer : MxRenderer {


    // TODO, implement the event system instead of hard coding window events.
    MxUniverseRenderer(MxGlfwWindow *win, float particleRadius = 1.0);

    MxUniverseRenderer& draw(Containers::Pointer<SceneGraph::Camera3D>& camera, const Vector2i& viewportSize);

    bool& isDirty() { return _dirty; }

    MxUniverseRenderer& setDirty() {
        _dirty = true;
        return *this;
    }

    Float& particleRadius() { return _particleRadius; }

    MxUniverseRenderer& setParticleRadius(Float radius) {
        _particleRadius = radius;
        return *this;
    }

    ParticleSphereShader::ColorMode& colorMode() { return _colorMode; }

    MxUniverseRenderer& setColorMode(ParticleSphereShader::ColorMode colorMode) {
        _colorMode = colorMode;
        return *this;
    }

    Color3& ambientColor() { return _ambientColor; }

    MxUniverseRenderer& setAmbientColor(const Color3& color) {
        _ambientColor = color;
        return *this;
    }

    Color3& diffuseColor() { return _diffuseColor; }

    MxUniverseRenderer& setDiffuseColor(const Color3& color) {
        _diffuseColor = color;
        return *this;
    }

    Color3& specularColor() { return _specularColor; }

    MxUniverseRenderer& setSpecularColor(const Color3& color) {
        _specularColor = color;
        return *this;
    }

    Float& shininess() { return _shininess; }

    MxUniverseRenderer& setShininess(Float shininess) {
        _shininess = shininess;
        return *this;
    }

    Vector3& lightDirection() { return _lightDir; }

    MxUniverseRenderer& setLightDirection(const Vector3& lightDir) {
        _lightDir = lightDir;
        return *this;
    }

    MxUniverseRenderer& setModelViewTransform(const Magnum::Matrix4& mat) {
        modelViewMat = mat;
        _shader->setViewMatrix(modelViewMat);
        return *this;
    }

    MxUniverseRenderer& setProjectionTransform(const Magnum::Matrix4& mat) {
        projMat = mat;
        _shader->setProjectionMatrix(projMat);
        return *this;
    }



    bool renderUniverse = true;


    void onCursorMove(double xpos, double ypos);

    void onCursorEnter(int entered);

    void onMouseButton(int button, int action, int mods);

    void onRedraw();

    void onWindowMove(int x, int y);

    void onWindowSizeChange(int x, int y);

    void onFramebufferSizeChange( int x, int y);

    void viewportEvent(const int w, const int h);

    void draw();





    bool _dirty = false;

    Float _particleRadius = 1.0f;
    ParticleSphereShader::ColorMode _colorMode = ParticleSphereShader::ColorMode::ConsistentRandom;
    Color3 _ambientColor{0.1f};
    Color3 _diffuseColor{0.0f, 0.5f, 0.9f};
    Color3 _specularColor{ 1.0f};
    Float _shininess = 150.0f;
    Vector3 _lightDir{1.0f, 1.0f, 2.0f};

    GL::Buffer _vertexBuffer;
    GL::Mesh _mesh;
    Containers::Pointer<ParticleSphereShader> _shader;

    /**
     * Only set a single combined matrix in the shader, this way,
     * the shader only performs a single matrix multiply of the vertices, update the
     * shader matrix whenever any of these change.
     *
     * multiplication order is the reverse of the pipeline.
     * Therefore you do totalmat = proj * view * model.
     */
    Magnum::Matrix4 modelViewMat = Matrix4{Math::IdentityInit};
    Magnum::Matrix4 projMat =  Matrix4{Math::IdentityInit};

    Vector2i _prevMousePosition;
    Vector3  _rotationPoint, _translationPoint;
    Float _lastDepth;


    /* Scene and drawable group must be constructed before camera and other
    scene objects */
    Containers::Pointer<Scene3D> _scene;
    Containers::Pointer<SceneGraph::DrawableGroup3D> _drawableGroup;
    Containers::Pointer<Object3D> _objCamera;
    Containers::Pointer<SceneGraph::Camera3D> _camera;
    /* Ground grid */
     Containers::Pointer<WireframeGrid> _grid;

     /* Fluid simulation system */
     Containers::Pointer<WireframeBox> _drawableBox;

    /* Camera helpers */
    Vector3 _defaultCamPosition{0.0f, 1.5f, 8.0f};
    Vector3 _defaultCamTarget{0.0f, 0.0f, 0.0f};

    Vector3 center;

    float sideLength = 10.0;



    MxGlfwWindow *window;

    Vector3 unproject(const Vector2i& windowPosition, float depth) const;


    void setupCallbacks();


};


/**
 * The the renderer type
 */
CAPI_DATA(PyTypeObject) MxUniverseRenderer_Type;

HRESULT MyUniverseRenderer_Init(PyObject *module);





