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
import threading
from pathlib import Path
from typing import List
from concurrent.futures import ThreadPoolExecutor, as_completed

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

def get_shader_files(shader_dir: Path) -> List[Path]:
    shader_files = []

    shader_files.extend(shader_dir.rglob('*.vert'))
    shader_files.extend(shader_dir.rglob('*.frag'))
    shader_files.extend(shader_dir.rglob('*.comp'))
    shader_files.extend(shader_dir.rglob('*.rgen'))
    shader_files.extend(shader_dir.rglob('*.rmiss'))
    shader_files.extend(shader_dir.rglob('*.rahit'))

    return sorted(shader_files)

def compile_shader(shader_file: Path, output_file: Path, compiler: str, includes: List[str], flags: List[str]) -> tuple[bool, str]:
    output_file.parent.mkdir(parents=True, exist_ok=True)

    cmd = [compiler, str(shader_file)] + flags + includes + ['-o', str(output_file)]

    try:
        result = subprocess.run(cmd, capture_output=True, text=True)

        if result.returncode != 0:
            error_msg = f"Error compiling {shader_file}!\n{result.stdout}\n{result.stderr}"
            return False, error_msg

        return True, ""
    except Exception as e:
        error_msg = f"Failed to compile {shader_file}! [Error={e}]"
        return False, error_msg

def main():
    parser = argparse.ArgumentParser(description='Compile Shaders')

    parser.add_argument('--release', action='store_true')

    args = parser.parse_args()

    try:
        compiler = find_glsl_compiler()
    except RuntimeError as e:
        print(f"Error={e}")
        return 1

    script_dir = Path(__file__).parent
    project_root = script_dir.parent.parent

    shader_dir = script_dir
    output_dir = project_root / 'Bin' / 'Assets' / 'Shaders'

    if not shader_dir.exists():
        print(f"Shader directory {shader_dir} does not exist!")
        return 1

    flags = ['--target-env', 'vulkan1.3', '--enhanced-msgs']

    if not args.release:
        flags.append('-gVS')

    includes = [
        f'-I{str(shader_dir / "Include")}',
        f'-I{str(project_root / "Shared")}'
    ]

    shader_files = get_shader_files(shader_dir)

    if not shader_files:
        print(f"No shader files found in {shader_dir}!")
        return 0

    max_workers = os.cpu_count()

    failed_count = 0
    print_lock = threading.Lock()

    tasks = []

    for shader_file in shader_files:
        rel_path = shader_file.relative_to(shader_dir)
        spirv_file = output_dir / f"{rel_path}.spv"
        tasks.append((shader_file, spirv_file, compiler, includes, flags))

    with ThreadPoolExecutor(max_workers=max_workers) as executor:
        future_to_shader = {
            executor.submit(compile_shader, *task): task[0] for task in tasks
        }

        for future in as_completed(future_to_shader):
            shader_file = future_to_shader[future]
            try:
                success, message = future.result()

                with print_lock:
                    if success:
                        rel_path = shader_file.relative_to(shader_dir)
                        print(f"Compiling SPIR-V Binary {rel_path}.spv")
                    else:
                        failed_count += 1
                        print(message)

            except Exception as e:
                with print_lock:
                    failed_count += 1
                    print(f"Error compiling {shader_file}! [Error={e}]")

    return 1 if failed_count > 0 else 0

if __name__ == '__main__':
    sys.exit(main())