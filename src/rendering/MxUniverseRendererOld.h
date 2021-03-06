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
#include <Corrade/Containers/Pointer.h>
#include <Magnum/GL/Buffer.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/Math/Color.h>
#include <Magnum/SceneGraph/Camera.h>

#include <shaders/ParticleSphereShader.h>
#include <MxUniverse.h>

using namespace Magnum;


class CAPI_EXPORT MxUniverseRendererOld {
    public:

        explicit MxUniverseRendererOld(float particleRadius);

        MxUniverseRendererOld& draw(Containers::Pointer<SceneGraph::Camera3D>& camera, const Vector2i& viewportSize);

        bool& isDirty() { return _dirty; }

        MxUniverseRendererOld& setDirty() {
            _dirty = true;
            return *this;
        }

        Float& particleRadius() { return _particleRadius; }

        MxUniverseRendererOld& setParticleRadius(Float radius) {
            _particleRadius = radius;
            return *this;
        }

        ParticleSphereShader::ColorMode& colorMode() { return _colorMode; }

        MxUniverseRendererOld& setColorMode(ParticleSphereShader::ColorMode colorMode) {
            _colorMode = colorMode;
            return *this;
        }

        Color3& ambientColor() { return _ambientColor; }

        MxUniverseRendererOld& setAmbientColor(const Color3& color) {
            _ambientColor = color;
            return *this;
        }

        Color3& diffuseColor() { return _diffuseColor; }

        MxUniverseRendererOld& setDiffuseColor(const Color3& color) {
            _diffuseColor = color;
            return *this;
        }

        Color3& specularColor() { return _specularColor; }

        MxUniverseRendererOld& setSpecularColor(const Color3& color) {
            _specularColor = color;
            return *this;
        }

        Float& shininess() { return _shininess; }

        MxUniverseRendererOld& setShininess(Float shininess) {
            _shininess = shininess;
            return *this;
        }

        Vector3& lightDirection() { return _lightDir; }

        MxUniverseRendererOld& setLightDirection(const Vector3& lightDir) {
            _lightDir = lightDir;
            return *this;
        }

        MxUniverseRendererOld& setModelViewTransform(const Magnum::Matrix4& mat) {
            modelViewMat = mat;
            _shader->setViewMatrix(modelViewMat);
            return *this;
        }

        MxUniverseRendererOld& setProjectionTransform(const Magnum::Matrix4& mat) {
            projMat = mat;
            _shader->setProjectionMatrix(projMat);
            return *this;
        }

        bool renderUniverse = true;

    private:

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
};




