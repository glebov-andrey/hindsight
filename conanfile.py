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

from conans import ConanFile, CMake


class HindsightConan(ConanFile):
    name = "hindsight"
    version = "0.1.0"
    url = "https://github.com/glebov-andrey/hindsight"
    license = "Apache-2.0"
    author = "Andrey Glebov (andrey458641387@gmail.com)"
    description = "A C++ stack trace library"
    topics = "hindsight", "stacktrace", "stack", "trace", "backtrace", "debug"
    homepage = url
    generators = "cmake", "cmake_find_package"

    settings = "os", "arch", "compiler", "build_type"
    options = {
        "shared": [False, True],
        "fPIC": [False, True],
        "with_fmt": [False, True],
        "build_tests": [False, True],
        "build_examples": [False, True],
        "build_docs": [False, True],
        "enable_clang_tidy": [False, True],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "with_fmt": False,
        "build_tests": False,
        "build_examples": False,
        "build_docs": False,
        "enable_clang_tidy": False,

        "libunwind:coredump": False,
        "libunwind:ptrace": False,
        "libunwind:setjmp": False,
    }

    def requirements(self):
        if self.settings.os != "Windows":
            self.requires("libunwind/[^1.5.0]")
            self.requires("libbacktrace/cci.20210118")

    def build_requirements(self):
        if self.options.with_fmt or self.options.build_examples:
            self.build_requires("fmt/[^7.1.3]")
        if self.options.build_tests:
            self.build_requires("catch2/[^2.13.6]")

    def configure(self):
        if self.settings.os == "Windows" or not self.options.shared:
            del self.options.fPIC

    scm = {
        "type": "git",
        "url": "git@github.com:glebov-andrey/hindsight.git",
        "revision": "auto",
    }

    def _configure_cmake(self):
        cmake = CMake(self)
        cmake.definitions["HINDSIGHT_INCLUDE_CONANBUILDINFO"] = True
        cmake.definitions["HINDSIGHT_WITH_FMT"] = self.options.with_fmt
        cmake.definitions["HINDSIGHT_BUILD_TESTS"] = self.options.build_tests
        cmake.definitions["HINDSIGHT_BUILD_EXAMPLES"] = self.options.build_examples
        cmake.definitions["HINDSIGHT_BUILD_DOCS"] = self.options.build_docs
        cmake.definitions["HINDSIGHT_ENABLE_CLANG_TIDY"] = self.options.enable_clang_tidy
        cmake.configure()
        return cmake

    def build(self):
        cmake = self._configure_cmake()
        cmake.build()
        if self.options.build_tests:
            cmake.test()
        if self.options.build_docs:
            cmake.build(target="docs")

    def package(self):
        cmake = self._configure_cmake()
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
        del self.info.options.build_docs
        del self.info.options.enable_clang_tidy
