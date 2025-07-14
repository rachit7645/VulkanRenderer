#!/usr/bin/env python3

# Copyright 2023 - 2025 Rachit
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import os
import sys
import subprocess
import argparse
from pathlib import Path
from typing import List

def find_glsl_compiler() -> str:
    vulkan_sdk = os.getenv('VULKAN_SDK')

    if vulkan_sdk:
        # Linux
        compiler_path = Path(vulkan_sdk) / 'bin' / 'glslangValidator'

        if compiler_path.exists():
            return str(compiler_path)

        # Windows
        compiler_path = Path(vulkan_sdk) / 'Bin' / 'glslangValidator.exe'

        if compiler_path.exists():
            return str(compiler_path)

    raise RuntimeError("glslangValidator not found!")

def compile_shader(shader_file: Path, output_file: Path, compiler: str, includes: List[str], flags: List[str]) -> tuple[bool, str]:
    output_file.parent.mkdir(parents=True, exist_ok=True)

    cmd = [compiler, str(shader_file)] + flags + includes + ['-o', str(output_file)]

    try:
        result = subprocess.run(cmd, capture_output=True, text=True)

        if result.returncode != 0:
            error_msg = f"{result.stdout}"
            return False, error_msg

        return True, ""
    except Exception as e:
        error_msg = f"Failed to compile {shader_file}! [Error={e}]"
        return False, error_msg

def main():
    parser = argparse.ArgumentParser(description='Compile Shader')

    parser.add_argument('shader_file', help='Path to the shader file to compile')
    parser.add_argument('--release', action='store_true')

    args = parser.parse_args()

    try:
        compiler = find_glsl_compiler()
    except RuntimeError as e:
        print(f"Error={e}")
        return 1

    shader_file = Path(args.shader_file)

    if not shader_file.exists():
        print(f"Shader file {shader_file} does not exist!")
        return 1

    script_dir = Path(__file__).parent
    project_root = script_dir.parent.parent
    output_dir = project_root / 'Bin' / 'Assets' / 'Shaders'

    try:
        rel_path = shader_file.relative_to(script_dir)
    except ValueError:
        print(f"Shader file {shader_file} is not in the expected shader directory!")
        return 1

    output_file = output_dir / f"{rel_path}.spv"

    # Setup compilation flags
    flags = ['--target-env', 'vulkan1.3', '--enhanced-msgs']

    if not args.release:
        flags.append('-gVS')

    script_dir = Path(__file__).parent
    project_root = script_dir.parent.parent

    includes = [
        f'-I{str(script_dir / "Include")}',
        f'-I{str(project_root / "Shared")}'
    ]

    success, message = compile_shader(shader_file, output_file, compiler, includes, flags)

    if success:
        return 0
    else:
        print(message)
        return 1

if __name__ == '__main__':
    sys.exit(main())