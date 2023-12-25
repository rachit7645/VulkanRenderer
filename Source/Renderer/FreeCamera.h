/*
 *    Copyright 2023 Rachit Khandelwal
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#ifndef FREE_CAMERA_H
#define FREE_CAMERA_H

#include "Camera.h"
#include "Externals/GLM.h"

namespace Renderer
{
    class FreeCamera : public Camera
    {
    public:
        // Default constructor
        FreeCamera();
        // Constructor w/ vectors
        FreeCamera(const glm::vec3& position, const glm::vec3& rotation, f32 FOV);
        // Update
        void Update(f32 frameDelta) override;
    private:
        // Check user inputs
        void CheckInputs(f32 frameDelta);
        // Camera movement
        void Move(f32 frameDelta);
        // Camera rotation
        void Rotate(f32 frameDelta);
        // Camera zooming
        void Zoom(f32 frameDelta);
    };
}

#endif
