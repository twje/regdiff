# Use the Visual Studio 2022 (v143) toolset when building vcpkg dependencies.
set(VCPKG_PLATFORM_TOOLSET v143)

# Target 64-bit architecture
set(VCPKG_TARGET_ARCHITECTURE x64)

# Link against the dynamic MSVC runtime (/MD, /MDd)
set(VCPKG_CRT_LINKAGE dynamic)

# Build libraries as static (recommended default)
set(VCPKG_LIBRARY_LINKAGE static)