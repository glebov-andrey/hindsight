# Copyright 2023 Andrey Glebov
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

from conan import ConanFile
from conan.tools.build.cppstd import check_min_cppstd
from conan.tools.cmake import CMakeToolchain, CMakeDeps, CMake, cmake_layout
from conan.tools.files import update_conandata, copy
from conan.tools.scm import Git


class HindsightConan(ConanFile):
    name = "hindsight"
    version = "0.1.0"
    description = "A C++ stack trace library"
    license = "Apache-2.0"
    author = "Andrey Glebov (andrey458641387@gmail.com)"
    topics = "hindsight", "stacktrace", "stack", "trace", "backtrace", "debug"
    homepage = "https://github.com/glebov-andrey/hindsight"
    url = homepage

    settings = "os", "arch", "compiler", "build_type"
    options = {
        "shared": [False, True],
        "fPIC": [False, True],
        "with_fmt": [False, True],
        "resolver_backend": ["libdw", "libbacktrace"],
        "build_tests": [False, True],
        "build_examples": [False, True],
        "build_docs": [False, True],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "with_fmt": False,
        "resolver_backend": "libdw",
        "build_tests": False,
        "build_examples": False,
        "build_docs": False,

        "elfutils/*:shared": True,
    }

    def config_options(self):
        if self.settings.os != "Linux":
            del self.options.resolver_backend

    @property
    def _uses_libdw(self):
        return self.settings.os == "Linux" and self.options.resolver_backend == "libdw"

    @property
    def _uses_libbacktrace(self):
        return (self.settings.os != "Windows" and self.settings.os != "Linux") or \
               (self.settings.os == "Linux" and self.options.resolver_backend == "libbacktrace")

    _fmt_package_name = "fmt/[^10.1.1]"

    def requirements(self):
        self.requires("tl-function-ref/[^1.0.0]")
        if self.options.with_fmt:
            self.requires(self._fmt_package_name)
        if self.settings.os != "Windows":
            self.requires("libunwind/[^1.7.2]")
        if self._uses_libdw:
            self.requires("elfutils/0.189")
        if self._uses_libbacktrace:
            self.requires("libbacktrace/cci.20210118")

    def build_requirements(self):
        if self.options.build_examples and not self.options.with_fmt:
            self.test_requires(self._fmt_package_name)
        if self.options.build_tests:
            self.test_requires("catch2/[^3.4.0]")
        if self.options.build_docs:
            self.tool_requires("doxygen/[^1.9.4]")

    def configure(self):
        if self.settings.os == "Windows" or self.options.shared:
            del self.options.fPIC

    def validate(self):
        check_min_cppstd(self, 20)
        if self._uses_libdw and self.options["elfutils"].shared:
            self.output.warn("Linking against a static build of libdw (part of elfutils).\n"
                             "Doing so requires distributing the combined work under the (L)GPL 3.0 license!")

    def layout(self):
        cmake_layout(self)
        self.cpp.build.libs = ["hindsight"]

        self.cpp.build.requires = ["tl-function-ref::tl-function-ref"]
        if self.options.with_fmt:
            self.cpp.build.requires.append("fmt::fmt")
        if self.settings.os != "Windows":
            self.cpp.build.requires.append("libunwind::generic")
        if self._uses_libdw:
            self.cpp.build.requires.append("elfutils::libdw")
        if self._uses_libbacktrace:
            self.cpp.build.requires.append("libbacktrace::libbacktrace")

        self.cpp.build.system_libs = []
        if self.settings.os == "Linux":
            self.cpp.build.system_libs.append("pthread")

        self.cpp.build.defines = []
        if self.options.shared:
            self.cpp.build.defines.append("HINDSIGHT_SHARED")
        if self.options.with_fmt:
            self.cpp.build.defines.append("HINDSIGHT_WITH_FMT")
        if self.settings.os == "Linux" and self.options.resolver_backend != "libdw":
            backend_macro = f"HINDSIGHT_RESOLVER_BACKEND_{str(self.options.resolver_backend).upper()}"
            self.cpp.build.defines.append(f"HINDSIGHT_RESOLVER_BACKEND={backend_macro}")

        self.cpp.package.includedirs = ["include"]
        self.cpp.package.libdirs = ["lib"]
        if self.settings.os == "Windows":
            self.cpp.package.bindirs = ["bin"]
        self.cpp.package.libs = ["hindsight"]
        self.cpp.package.requires = self.cpp.build.requires
        self.cpp.package.system_libs = self.cpp.build.system_libs
        self.cpp.package.defines = self.cpp.build.defines

    def export(self):
        scm_url, scm_commit = Git(self, self.recipe_folder).get_url_and_commit()
        update_conandata(self, {"sources": {"url": scm_url, "commit": scm_commit}})

    def source(self):
        Git(self).fetch_commit(**self.conan_data["sources"])

    def generate(self):
        toolchain = CMakeToolchain(self)
        toolchain.variables["HINDSIGHT_WITH_FMT"] = self.options.with_fmt
        if self.settings.os == "Linux":
            toolchain.variables["HINDSIGHT_RESOLVER_BACKEND"] = self.options.resolver_backend
        toolchain.variables["HINDSIGHT_BUILD_TESTS"] = self.options.build_tests
        toolchain.variables["HINDSIGHT_BUILD_EXAMPLES"] = self.options.build_examples
        toolchain.variables["HINDSIGHT_BUILD_DOCS"] = self.options.build_docs
        toolchain.generate()
        CMakeDeps(self).generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
        if self.options.build_tests:
            cmake.test()

    def package(self):
        copy(self, "LICENSE.txt", src=self.source_path, dst=self.package_path / "licenses")
        copy(self, "NOTICE.txt", src=self.source_path, dst=self.package_path / "licenses")
        cmake = CMake(self)
        cmake.install()

    def package_id(self):
        del self.info.options.build_tests
        del self.info.options.build_examples
