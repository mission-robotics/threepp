
#include "threepp/renderers/GLRenderer.hpp"

#include <glad/glad.h>

using namespace threepp;

namespace {

    inline unsigned int createShader(int type, const char *str) {

        const auto shader = glCreateShader(type);

        glShaderSource(shader, 1, &str, nullptr);
        glCompileShader(shader);

        return shader;
    }

}// namespace

GLRenderer::GLRenderer(Canvas &canvas, const GLRenderer::Parameters &parameters)
    : canvas_(canvas), _width(canvas.getWidth()), _height(canvas.getHeight()),
      _viewport(0, 0, _width, _height),
      _scissor(0, 0, _width, _height), state(canvas),
      background(state, parameters.premultipliedAlpha),
      bufferRenderer(info),
      indexedBufferRenderer(info),
      clipping(properties),
      bindingStates(attributes),
      geometries(attributes, info, bindingStates),
      textures(state, properties, info) {
}

void GLRenderer::initGLContext() {
}

int GLRenderer::getTargetPixelRatio() const {
    return _pixelRatio;
}

void GLRenderer::getSize(Vector2 &target) const {
    target.set((float) _width, (float) _height);
}

void GLRenderer::setSize(int width, int height) {

    _width = width;
    _height = height;

    canvas_.setSize(width * _pixelRatio, height * _pixelRatio);
}

void GLRenderer::getDrawingBufferSize(Vector2 &target) const {

    target.set((float) (_width * _pixelRatio), (float) (_height * _pixelRatio)).floor();
}

void GLRenderer::setDrawingBufferSize(int width, int height, int pixelRatio) {

    _width = width;
    _height = height;

    _pixelRatio = pixelRatio;

    canvas_.setSize(width * pixelRatio, height * pixelRatio);

    this->setViewport(0, 0, width, height);
}

void GLRenderer::getCurrentViewport(Vector4 &target) const {

    target.copy(_currentViewport);
}

void GLRenderer::getViewport(Vector4 &target) const {

    target.copy(_viewport);
}

void GLRenderer::setViewport(const Vector4 &v) {

    _viewport.copy(v);

    state.viewport(_currentViewport.copy(_viewport).multiplyScalar((float) _pixelRatio).floor());
}

void GLRenderer::setViewport(int x, int y, int width, int height) {

    _viewport.set((float) x, (float) y, (float) width, (float) height);

    state.viewport(_currentViewport.copy(_viewport).multiplyScalar((float) _pixelRatio).floor());
}

void GLRenderer::getScissor(Vector4 &target) {

    target.copy(_scissor);
}

void GLRenderer::setScissor(const Vector4 &v) {

    _scissor.copy(v);

    state.scissor(_currentScissor.copy(_scissor).multiplyScalar((float) _pixelRatio).floor());
}


void GLRenderer::setScissor(int x, int y, int width, int height) {

    _scissor.set((float) x, (float) y, (float) width, (float) height);

    state.scissor(_currentScissor.copy(_scissor).multiplyScalar((float) _pixelRatio).floor());
}

bool GLRenderer::getScissorTest() const {

    return _scissorTest;
}

void GLRenderer::setScissorTest(bool boolean) {

    _scissorTest = boolean;

    state.setScissorTest(_scissorTest);
}

void GLRenderer::getClearColor(Color &target) const {

    target.copy(background.getClearColor());
}

void GLRenderer::setClearColor(const Color &color, float alpha) {

    background.setClearColor(color, alpha);
}

float GLRenderer::getClearAlpha() const {

    return background.getClearAlpha();
}

void GLRenderer::setClearAlpha(float clearAlpha) {

    background.setClearAlpha(clearAlpha);
}

void GLRenderer::clear(bool color, bool depth, bool stencil) {

    GLbitfield bits = 0;

    if (color) bits |= GL_COLOR_BUFFER_BIT;
    if (depth) bits |= GL_DEPTH_BUFFER_BIT;
    if (stencil) bits |= GL_STENCIL_BUFFER_BIT;

    glClear(bits);
}

void GLRenderer::clearColor() { clear(true, false, false); }

void GLRenderer::clearDepth() { clear(false, true, false); }

void GLRenderer::clearStencil() { clear(false, false, true); }

void GLRenderer::dispose() {
    //            const material = event.target;
    //
    //            material.removeEventListener( 'dispose', onMaterialDispose );
    //
    //            deallocateMaterial( material );
}
void GLRenderer::deallocateMaterial(Material *material) {

    releaseMaterialProgramReferences(material);

    properties.materialProperties.remove(material->uuid);
}
void GLRenderer::releaseMaterialProgramReferences(Material *material) {

    //            auto& programs = properties.materialProperties.get( material->uuid ).programs;
    //
    //            if ( programs !== undefined ) {
    //
    //                programs.forEach( function ( program ) {
    //
    //                    programCache.releaseProgram( program );
    //
    //                } );
    //
    //            }
}

void GLRenderer::renderBufferDirect(Camera *camera, Scene *scene, BufferGeometry *geometry, Material *material, Object3D *object, GeometryGroup *group) {

    if (scene == nullptr) scene = &_emptyScene;// renderBufferDirect second parameter used to be fog (could be nullptr)

    bool isMesh = instanceof <Mesh>(object);

    const auto frontFaceCW = (isMesh && object->matrixWorld.determinant() < 0);

    auto program = setProgram(camera, scene, material, object);

    state.setMaterial(material, frontFaceCW);

    //

    auto index = geometry->getIndex();
    const auto &position = geometry->getAttribute<float>("position");

    //

    if (index == nullptr) {

        if (!geometry->hasAttribute("position") || position->count() == 0) return;

    } else if (index->count() == 0) {

        return;
    }

    //

    int rangeFactor = 1;

    MaterialWithWireframe *wireframeMaterial;
    bool isWireframeMaterial = instanceof <MaterialWithWireframe>(material);

    if (isWireframeMaterial) {

        wireframeMaterial = dynamic_cast<MaterialWithWireframe *>(material);

        if (wireframeMaterial->getWireframe()) {

            index = geometries.getWireframeAttribute(geometry);
            rangeFactor = 2;
        }
    }

    bindingStates.setup(object, material, program, geometry, index);

    gl::Buffer attribute{};
    gl::BufferRenderer* renderer = &bufferRenderer;

    if (index != nullptr) {

        attribute = attributes.get(index);

        renderer = &indexedBufferRenderer;
        indexedBufferRenderer.setIndex(attribute);
    }

    //

    const auto dataCount = (index != nullptr) ? index->count() : position->count();

    const auto rangeStart = geometry->drawRange.start * rangeFactor;
    const auto rangeCount = geometry->drawRange.count * rangeFactor;

    const auto groupStart = group != nullptr ? group->start * rangeFactor : 0;
    const auto groupCount = group != nullptr ? group->count * rangeFactor : Infinity<int>;

    const auto drawStart = std::max(rangeStart, groupStart);
    const auto drawEnd = std::min(dataCount, std::min(rangeStart + rangeCount, groupStart + groupCount)) - 1;

    const auto drawCount = std::max(0, drawEnd - drawStart + 1);

    if (drawCount == 0) return;

    //

    if (isMesh) {

        if (isWireframeMaterial) {

            if (isWireframeMaterial && wireframeMaterial->getWireframe()) {

                state.setLineWidth(wireframeMaterial->getWireframeLinewidth() * (float) getTargetPixelRatio());
                renderer->setMode(GL_LINES);
            }

        } else {

            renderer->setMode(GL_TRIANGLES);
        }

    } else if (instanceof <Line3>(object)) {

        float lineWidth = 1;
        if (isWireframeMaterial) {
            lineWidth = wireframeMaterial->getWireframeLinewidth();
        }

        state.setLineWidth(lineWidth * getTargetPixelRatio());

        //                if (object.isLineSegments) {
        //
        //                    renderer.setMode(GL_LINES);
        //
        //                } else if (object.isLineLoop) {
        //
        //                    renderer.setMode(GL_LINE_LOOP);
        //
        //                } else {
        //
        //                    renderer.setMode(GL_LINE_STRIP);
        //                }

    } else if (instanceof <Points *>(object)) {

        renderer->setMode(GL_POINTS);

    } else if (false /*object.isSprite*/) {

        //                renderer.setMode(GL_TRIANGLES);
    }

    if (false /*object.isInstancedMesh*/) {

        //                renderer.renderInstances(drawStart, drawCount, object.count);

    } else if (false /*geometry.isInstancedBufferGeometry*/) {

        //                const instanceCount = Math.min(geometry.instanceCount, geometry._maxInstanceCount);
        //
        //                renderer.renderInstances(drawStart, drawCount, instanceCount);

    } else {

        renderer->render(drawStart, drawCount);
    }
}

gl::GLProgram GLRenderer::setProgram(Camera *camera, Object3D *scene, Material *material, Object3D *object) {

    bool isScene = instanceof <Scene>(scene);

//            if (isScene) scene = _emptyScene;// scene could be a Mesh, Line, Points, ...
//
//            textures.resetTextureUnits();
//
//            const fog = scene->fog;
//            const environment = material.isMeshStandardMaterial ? scene.environment : nullptr;
//            const encoding = (_currentRenderTarget == = nullptr) ? _this.outputEncoding : _currentRenderTarget.texture.encoding;
//            const envMap = cubemaps.get(material.envMap || environment);
//            const vertexAlphas = material.vertexColors == = true &&object.geometry &&object.geometry.attributes.color &&object.geometry.attributes.color.itemSize == = 4;
//
//            const materialProperties = properties.get(material);
//            const lights = currentRenderState.state.lights;
//
//            if (_clippingEnabled) {
//
//                if (_localClippingEnabled || camera != _currentCamera) {
//
//                    const useCache =
//                            camera == _currentCamera && material->id == _currentMaterialId;
//
//                    // we might want to call this function with some ClippingGroup
//                    // object instead of the material, once it becomes feasible
//                    // (#8465, #8379)
//                    clipping.setState(material, camera, useCache);
//                }
//            }
//
//            //
//
//            let needsProgramChange = false;
//
//            if (material.version == = materialProperties.__version) {
//
//                if (materialProperties.needsLights && (materialProperties.lightsStateVersion != = lights.state.version)) {
//
//                    needsProgramChange = true;
//
//                } else if (materialProperties.outputEncoding != = encoding) {
//
//                    needsProgramChange = true;
//
//                } else if (object.isInstancedMesh &&materialProperties.instancing == = false) {
//
//                    needsProgramChange = true;
//
//                } else if (!object.isInstancedMesh &&materialProperties.instancing == = true) {
//
//                    needsProgramChange = true;
//
//                } else if (object.isSkinnedMesh &&materialProperties.skinning == = false) {
//
//                    needsProgramChange = true;
//
//                } else if (!object.isSkinnedMesh &&materialProperties.skinning == = true) {
//
//                    needsProgramChange = true;
//
//                } else if (materialProperties.envMap != = envMap) {
//
//                    needsProgramChange = true;
//
//                } else if (material.fog &&materialProperties.fog != = fog) {
//
//                    needsProgramChange = true;
//
//                } else if (materialProperties.numClippingPlanes != = undefined &&
//                                                                     (materialProperties.numClippingPlanes != = clipping.numPlanes ||
//                                                                                                                materialProperties.numIntersection != = clipping.numIntersection)) {
//
//                    needsProgramChange = true;
//
//                } else if (materialProperties.vertexAlphas != = vertexAlphas) {
//
//                    needsProgramChange = true;
//                }
//
//            } else {
//
//                needsProgramChange = true;
//                materialProperties.__version = material.version;
//            }
//
//            //
//
//            let program = materialProperties.currentProgram;
//
//            if (needsProgramChange == = true) {
//
//                program = getProgram(material, scene, object);
//            }
//
//            let refreshProgram = false;
//            let refreshMaterial = false;
//            let refreshLights = false;
//
//            const p_uniforms = program.getUniforms(),
//                  m_uniforms = materialProperties.uniforms;
//
//            if (state.useProgram(program.program)) {
//
//                refreshProgram = true;
//                refreshMaterial = true;
//                refreshLights = true;
//            }
//
//            if (material.id != = _currentMaterialId) {
//
//                _currentMaterialId = material.id;
//
//                refreshMaterial = true;
//            }
//
//            if (refreshProgram || _currentCamera != = camera) {
//
//                p_uniforms.setValue(_gl, 'projectionMatrix', camera.projectionMatrix);
//
//                if (capabilities.logarithmicDepthBuffer) {
//
//                    p_uniforms.setValue(_gl, 'logDepthBufFC',
//                                        2.0 / (Math.log(camera.far + 1.0) / Math.LN2));
//                }
//
//                if (_currentCamera != = camera) {
//
//                    _currentCamera = camera;
//
//                    // lighting uniforms depend on the camera so enforce an update
//                    // now, in case this material supports lights - or later, when
//                    // the next material that does gets activated:
//
//                    refreshMaterial = true;// set to true on material change
//                    refreshLights = true;  // remains set until update done
//                }
//
//                // load material specific uniforms
//                // (shader material also gets them for the sake of genericity)
//
//                if (material.isShaderMaterial ||
//                    material.isMeshPhongMaterial ||
//                    material.isMeshToonMaterial ||
//                    material.isMeshStandardMaterial ||
//                    material.envMap) {
//
//                    const uCamPos = p_uniforms.map.cameraPosition;
//
//                    if (uCamPos != = undefined) {
//
//                        uCamPos.setValue(_gl,
//                                         _vector3.setFromMatrixPosition(camera.matrixWorld));
//                    }
//                }
//
//                if (material.isMeshPhongMaterial ||
//                    material.isMeshToonMaterial ||
//                    material.isMeshLambertMaterial ||
//                    material.isMeshBasicMaterial ||
//                    material.isMeshStandardMaterial ||
//                    material.isShaderMaterial) {
//
//                    p_uniforms.setValue(_gl, 'isOrthographic', camera.isOrthographicCamera == = true);
//                }
//
//                if (material.isMeshPhongMaterial ||
//                    material.isMeshToonMaterial ||
//                    material.isMeshLambertMaterial ||
//                    material.isMeshBasicMaterial ||
//                    material.isMeshStandardMaterial ||
//                    material.isShaderMaterial ||
//                    material.isShadowMaterial ||
//                    object.isSkinnedMesh) {
//
//                    p_uniforms.setValue(_gl, 'viewMatrix', camera.matrixWorldInverse);
//                }
//            }
//
//            // skinning uniforms must be set even if material didn't change
//            // auto-setting of texture unit for bone texture must go before other textures
//            // otherwise textures used for skinning can take over texture units reserved for other material textures
//
//            if (object.isSkinnedMesh) {
//
//                p_uniforms.setOptional(_gl, object, 'bindMatrix');
//                p_uniforms.setOptional(_gl, object, 'bindMatrixInverse');
//
//                const skeleton = object.skeleton;
//
//                if (skeleton) {
//
//                    if (capabilities.floatVertexTextures) {
//
//                        if (skeleton.boneTexture == = nullptr) skeleton.computeBoneTexture();
//
//                        p_uniforms.setValue(_gl, 'boneTexture', skeleton.boneTexture, textures);
//                        p_uniforms.setValue(_gl, 'boneTextureSize', skeleton.boneTextureSize);
//
//                    } else {
//
//                        p_uniforms.setOptional(_gl, skeleton, 'boneMatrices');
//                    }
//                }
//            }
//
//            if (refreshMaterial || materialProperties.receiveShadow != = object.receiveShadow) {
//
//                materialProperties.receiveShadow = object.receiveShadow;
//                p_uniforms.setValue(_gl, 'receiveShadow', object.receiveShadow);
//            }
//
//            if (refreshMaterial) {
//
//                p_uniforms.setValue(_gl, 'toneMappingExposure', _this.toneMappingExposure);
//
//                if (materialProperties.needsLights) {
//
//                    // the current material requires lighting info
//
//                    // note: all lighting uniforms are always set correctly
//                    // they simply reference the renderer's state for their
//                    // values
//                    //
//                    // use the current material's .needsUpdate flags to set
//                    // the GL state when required
//
//                    markUniformsLightsNeedsUpdate(m_uniforms, refreshLights);
//                }
//
//                // refresh uniforms common to several materials
//
//                if (fog && material.fog) {
//
//                    materials.refreshFogUniforms(m_uniforms, fog);
//                }
//
//                materials.refreshMaterialUniforms(m_uniforms, material, _pixelRatio, _height, _transmissionRenderTarget);
//
//                WebGLUniforms.upload(_gl, materialProperties.uniformsList, m_uniforms, textures);
//            }
//
//            if (material.isShaderMaterial &&material.uniformsNeedUpdate == = true) {
//
//                WebGLUniforms.upload(_gl, materialProperties.uniformsList, m_uniforms, textures);
//                material.uniformsNeedUpdate = false;
//            }
//
//            if (material.isSpriteMaterial) {
//
//                p_uniforms.setValue(_gl, 'center', object.center);
//            }
//
//            // common matrices
//
//            p_uniforms.setValue(_gl, 'modelViewMatrix', object.modelViewMatrix);
//            p_uniforms.setValue(_gl, 'normalMatrix', object.normalMatrix);
//            p_uniforms.setValue(_gl, 'modelMatrix', object.matrixWorld);
//
//            return program;

    return {};
}
