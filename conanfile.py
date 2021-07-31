# Copyright 2021 Andrey Glebov
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0

from conans import ConanFile, tools
from conan.tools.cmake import CMakeToolchain, CMake


class HindsightConan(ConanFile):
    name = "hindsight"
    version = "0.1.0"
    url = "https://github.com/glebov-andrey/hindsight"
    license = "Apache-2.0"
    author = "Andrey Glebov (andrey458641387@gmail.com)"
    description = "A C++ stack trace library"
    topics = "hindsight", "stacktrace", "stack", "trace", "backtrace", "debug"
    homepage = url
    generators = "CMakeDeps"

    settings = "os", "arch", "compiler", "build_type"
    options = {
        "shared": [False, True],
        "fPIC": [False, True],
        "with_fmt": [False, True],
        "build_tests": [False, True],
        "build_examples": [False, True],
        "build_docs": [False, True],
        "enable_coverage": [False, True],
        "enable_clang_tidy": [False, True],
        "enable_lld_thinlto_cache": [False, True],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "with_fmt": False,
        "build_tests": False,
        "build_examples": False,
        "build_docs": False,
        "enable_coverage": False,
        "enable_clang_tidy": False,
        "enable_lld_thinlto_cache": False,

        "libunwind:coredump": False,
        "libunwind:ptrace": False,
        "libunwind:setjmp": False,
    }

    def requirements(self):
        self.requires("tl-function-ref/[^1.0.0]")
        if self.options.with_fmt:
            self.requires("fmt/[^8.0.1]")
        if self.settings.os != "Windows":
            self.requires("libunwind/[^1.5.0]")
        if self.settings.os != "Windows" and self.settings.os != "Linux":
            self.requires("libbacktrace/cci.20210118")

    def build_requirements(self):
        if self.options.build_examples:
            self.build_requires("fmt/[^8.0.0]", force_host_context=True)
        if self.options.build_tests:
            self.build_requires("catch2/[^2.13.6]", force_host_context=True)
        if self.options.build_docs:
            self.build_requires("doxygen/[^1.9.1]")

    def configure(self):
        if self.settings.os == "Windows" or self.options.shared:
            del self.options.fPIC

    def validate(self):
        tools.check_min_cppstd(self, 20)

    def generate(self):
        toolchain = CMakeToolchain(self)
        toolchain.variables["HINDSIGHT_WITH_FMT"] = self.options.with_fmt
        toolchain.variables["HINDSIGHT_BUILD_TESTS"] = self.options.build_tests
        toolchain.variables["HINDSIGHT_BUILD_EXAMPLES"] = self.options.build_examples
        toolchain.variables["HINDSIGHT_BUILD_DOCS"] = self.options.build_docs
        toolchain.variables["HINDSIGHT_ENABLE_COVERAGE"] = self.options.enable_coverage
        toolchain.variables["HINDSIGHT_ENABLE_CLANG_TIDY"] = self.options.enable_clang_tidy
        toolchain.variables["HINDSIGHT_ENABLE_LLD_THINLTO_CACHE"] = self.options.enable_lld_thinlto_cache
        toolchain.generate()

    scm = {
        "type": "git",
        "url": "git@github.com:glebov-andrey/hindsight.git",
        "revision": "auto",
    }

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
        if self.options.build_tests:
            cmake.test(output_on_failure=True)

    def package(self):
        self.copy("LICENSE.txt", dst="licenses")
        self.copy("NOTICE.txt", dst="licenses")
        cmake = CMake(self)
        cmake.configure()
        cmake.install()

    def package_info(self):
        self.cpp_info.libs.append("hindsight")
        if self.settings.os == "Linux":
            self.cpp_info.system_libs.append("pthread")
        if self.options.with_fmt:
            self.cpp_info.defines.append("HINDSIGHT_WITH_FMT")
        if self.options.shared:
            self.cpp_info.defines.append("HINDSIGHT_SHARED")

    def package_id(self):
        del self.info.options.build_tests
        del self.info.options.build_examples
        del self.info.options.enable_clang_tidy
        del self.info.options.enable_lld_thinlto_cache
