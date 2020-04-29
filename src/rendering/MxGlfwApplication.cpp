/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019
              Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2016 Jonathan Hale <squareys@googlemail.com>
    Copyright © 2019, 2020 Marco Melorio <m.melorio@icloud.com>

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included
    in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
    THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
 */

#include "MxGlfwApplication.h"

#include <cstring>
#include <tuple>
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/StridedArrayView.h>
#include <Corrade/Utility/Arguments.h>
#include <Corrade/Utility/String.h>
#include <Corrade/Utility/Unicode.h>

#include "Magnum/ImageView.h"
#include "Magnum/PixelFormat.h"
#include "Magnum/Math/ConfigurationValue.h"
#include "Magnum/Platform/ScreenedApplication.hpp"
#include "Magnum/Platform/Implementation/DpiScaling.h"

#ifdef MAGNUM_TARGET_GL
#include "Magnum/GL/Version.h"
#include "Magnum/Platform/GLContext.h"
#endif

using namespace Magnum::Platform;

#ifdef GLFW_TRUE
/* The docs say that it's the same, verify that just in case */
static_assert(GLFW_TRUE == true && GLFW_FALSE == false, "GLFW does not have sane bool values");
#endif

enum class MxGlfwApplication::Flag: UnsignedByte {
    Redraw = 1 << 0,
            TextInputActive = 1 << 1,
#ifdef CORRADE_TARGET_APPLE
            HiDpiWarningPrinted = 1 << 2
#elif defined(CORRADE_TARGET_WINDOWS)
            /* On Windows, GLFW fires a viewport event already when creating the
       window, which means viewportEvent() gets called even before the
       constructor exits. That's not a problem if the window is created
       implicitly (because derived class vtable is not setup yet and so the
       call goes into the base class no-op viewportEvent()), but when calling
       create() / tryCreate() from user constructor, this might lead to crashes
       as things touched by viewportEvent() might not be initialized yet. To
       fix this, we ignore the first ever viewport event. This behavior was not
       observed on Linux or macOS (and thus ignoring the first viewport event
       there may be harmful), so keeping this Windows-only. */
            FirstViewportEventIgnored = 1 << 2
#endif
};

MxGlfwApplication::MxGlfwApplication(const Arguments& arguments): MxGlfwApplication{arguments, Configuration{}} {}

MxGlfwApplication::MxGlfwApplication(const Arguments& arguments, const Configuration& configuration): MxGlfwApplication{arguments, NoCreate} {
    create(configuration);
}

#ifdef MAGNUM_TARGET_GL
MxGlfwApplication::MxGlfwApplication(const Arguments& arguments, const Configuration& configuration, const GLConfig& GLConfig): MxGlfwApplication{arguments, NoCreate} {
    create(configuration, GLConfig);
}
#endif

MxGlfwApplication::MxGlfwApplication(const Arguments& arguments, NoCreateT):
            _flags{Flag::Redraw}
{
    Utility::Arguments args{Magnum::Platform::Implementation::windowScalingArguments()};
#ifdef MAGNUM_TARGET_GL
    _context.reset(new GLContext{NoCreate, args, arguments.argc, arguments.argv});
#else
    /** @todo this is duplicated here and in Sdl2Application, figure out a nice
        non-duplicated way to handle this */
    args.addOption("log", "default").setHelp("log", "console logging", "default|quiet|verbose")
                .setFromEnvironment("log")
                .parse(arguments.argc, arguments.argv);
#endif

    /* Init GLFW */
    glfwSetErrorCallback([](int, const char* const description) {
        Error{} << description;
    });

    if(!glfwInit()) {
        Error() << "Could not initialize GLFW";
        std::exit(8);
    }

    /* Save command-line arguments */
    if(args.value("log") == "verbose") _verboseLog = true;
    const std::string dpiScaling = args.value("dpi-scaling");
    if(dpiScaling == "default")
        _commandLineDpiScalingPolicy = MxSimulator::DpiScalingPolicy::Default;
#ifdef CORRADE_TARGET_APPLE
    else if(dpiScaling == "framebuffer")
        _commandLineDpiScalingPolicy = MxSimulator::DpiScalingPolicy::Framebuffer;
#else
    else if(dpiScaling == "virtual")
        _commandLineDpiScalingPolicy = MxSimulator::DpiScalingPolicy::Virtual;
    else if(dpiScaling == "physical")
        _commandLineDpiScalingPolicy = MxSimulator::DpiScalingPolicy::Physical;
#endif
    else if(dpiScaling.find_first_of(" \t\n") != std::string::npos)
        _commandLineDpiScaling = args.value<Vector2>("dpi-scaling");
    else
        _commandLineDpiScaling = Vector2{args.value<Float>("dpi-scaling")};
}

void MxGlfwApplication::create() {
    create(Configuration{});
}

void MxGlfwApplication::create(const Configuration& configuration) {
    if(!tryCreate(configuration)) std::exit(1);
}

#ifdef MAGNUM_TARGET_GL
void MxGlfwApplication::create(const Configuration& configuration, const GLConfig& GLConfig) {
    if(!tryCreate(configuration, GLConfig)) std::exit(1);
}
#endif

Vector2 MxGlfwApplication::dpiScaling(const Configuration& configuration) {
    std::ostream* verbose = _verboseLog ? Debug::output() : nullptr;

    /* Print a helpful warning in case some extra steps are needed for HiDPI
       support */
#ifdef CORRADE_TARGET_APPLE
    if(!Magnum::Platform::Implementation::isAppleBundleHiDpiEnabled() && !(_flags & Flag::HiDpiWarningPrinted)) {
        Warning{} << "Platform::MxGlfwApplication: warning: the executable is not a HiDPI-enabled app bundle";
        _flags |= Flag::HiDpiWarningPrinted;
    }
#elif defined(CORRADE_TARGET_WINDOWS)
    /** @todo */
#endif

    /* Use values from the configuration only if not overriden on command line
       to something non-default. In any case explicit scaling has a precedence
       before the policy. */
    MxSimulator::DpiScalingPolicy dpiScalingPolicy{};
    if(!_commandLineDpiScaling.isZero()) {
        Debug{verbose} << "Platform::MxGlfwApplication: user-defined DPI scaling" << _commandLineDpiScaling.x();
        return _commandLineDpiScaling;
    } else if(_commandLineDpiScalingPolicy != MxSimulator::DpiScalingPolicy::Default) {
        dpiScalingPolicy = _commandLineDpiScalingPolicy;
    } else if(!configuration.dpiScaling().isZero()) {
        Debug{verbose} << "Platform::MxGlfwApplication: app-defined DPI scaling" << _commandLineDpiScaling.x();
        return configuration.dpiScaling();
    } else {
        dpiScalingPolicy = configuration.dpiScalingPolicy();
    }

    /* There's no choice on Apple, it's all controlled by the plist file. So
       unless someone specified custom scaling via config or command-line
       above, return the default. */
#ifdef CORRADE_TARGET_APPLE
    return Vector2{1.0f};

    /* Otherwise there's a choice between virtual and physical DPI scaling */
#else
    /* Try to get virtual DPI scaling first, if supported and requested */
    if(dpiScalingPolicy == DpiScalingPolicy::Virtual) {
        /* Use Xft.dpi on X11. This could probably be dropped for GLFW 3.3+
           as glfwGetMonitorContentScale() does the same, but I'd still need to
           keep it for 2.2 and below, plus the same code needs to be used for
           SDL anyway. So keeping it to reduce the chance for unexpected minor
           differences across app implementations. */
#ifdef _MAGNUM_PLATFORM_USE_X11
        const Vector2 dpiScaling{Implementation::x11DpiScaling()};
        if(!dpiScaling.isZero()) {
            Debug{verbose} << "Platform::MxGlfwApplication: virtual DPI scaling" << dpiScaling.x();
            return dpiScaling;
        }

        /* Check for DPI awareness on non-RT Windows and then ask for content
           scale (available since GLFW 3.3). GLFW is advertising the
           application to be DPI-aware on its own even without supplying an
           explicit manifest -- https://github.com/glfw/glfw/blob/089ea9af227fdffdf872348923e1c12682e63029/src/win32_init.c#L564-L569
           If, for some reason, the app is still not DPI-aware, tell that to
           the user explicitly and don't even attempt to query the value if the
           app is not DPI aware. If it's desired to get the DPI value
           unconditionally, the user should use physical DPI scaling instead. */
#elif defined(CORRADE_TARGET_WINDOWS) && !defined(CORRADE_TARGET_WINDOWS_RT)
        if(!Implementation::isWindowsAppDpiAware()) {
            Warning{verbose} << "Platform::MxGlfwApplication: your application is not set as DPI-aware, DPI scaling won't be used";
            return Vector2{1.0f};
        }
#if GLFW_VERSION_MAJOR*100 + GLFW_VERSION_MINOR >= 303
        GLFWmonitor* const monitor = glfwGetPrimaryMonitor();
        Vector2 dpiScaling;
        glfwGetMonitorContentScale(monitor, &dpiScaling.x(), &dpiScaling.y());
        Debug{verbose} << "Platform::MxGlfwApplication: virtual DPI scaling" << dpiScaling;
        return dpiScaling;
#else
        Debug{verbose} << "Platform::MxGlfwApplication: sorry, virtual DPI scaling only available on GLFW 3.3+, falling back to physical DPI scaling";
#endif

        /* Otherwise ¯\_(ツ)_/¯ */
#else
        Debug{verbose} << "Platform::MxGlfwApplication: sorry, virtual DPI scaling not implemented on this platform yet, falling back to physical DPI scaling";
#endif
    }

    /* At this point, either the virtual DPI query failed or a physical DPI
       scaling is requested */
    CORRADE_INTERNAL_ASSERT(dpiScalingPolicy == MxSimulator::DpiScalingPolicy::Virtual || dpiScalingPolicy == Implementation::GlfwDpiScalingPolicy::Physical);

    /* Physical DPI scaling. Enable only on Linux (where it gets the usually
       very-off value from X11) and on non-RT Windows (where it calculates it
       from actual monitor dimensions). */
#if defined(CORRADE_TARGET_UNIX) || (defined(CORRADE_TARGET_WINDOWS) && !defined(CORRADE_TARGET_WINDOWS_RT))
    GLFWmonitor* const monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* const mode = glfwGetVideoMode(monitor);
    Vector2i monitorSize;
    glfwGetMonitorPhysicalSize(monitor, &monitorSize.x(), &monitorSize.y());
    if(monitorSize.isZero()) {
        Warning{verbose} << "Platform::MxGlfwApplication: the physical monitor size is zero? DPI scaling won't be used";
        return Vector2{1.0f};
    }
    auto dpi = Vector2{Vector2i{mode->width, mode->height}*25.4f/Vector2{monitorSize}};
    const Vector2 dpiScaling{dpi/96.0f};
    Debug{verbose} << "Platform::MxGlfwApplication: physical DPI scaling" << dpiScaling;
    return dpiScaling;

    /* Not implemented otherwise */
#else
    Debug{verbose} << "Platform::MxGlfwApplication: sorry, physical DPI scaling not implemented on this platform yet";
    return Vector2{1.0f};
#endif
#endif
}

void MxGlfwApplication::setWindowTitle(const std::string& title) {
    glfwSetWindowTitle(_window, title.data());
}

#if GLFW_VERSION_MAJOR*100 + GLFW_VERSION_MINOR >= 302
void MxGlfwApplication::setWindowIcon(const ImageView2D& image) {
    setWindowIcon({image});
}

namespace {

template<class T> inline void packPixels(const Containers::StridedArrayView2D<const T>& in, const Containers::StridedArrayView2D<Color4ub>& out) {
    for(std::size_t row = 0; row != in.size()[0]; ++row)
        for(std::size_t col = 0; col != in.size()[1]; ++col)
            out[row][col] = in[row][col];
}

}

void MxGlfwApplication::setWindowIcon(std::initializer_list<ImageView2D> images) {
    /* Calculate the total size needed to allocate first so we don't allocate
       a ton of tiny arrays */
    std::size_t size = 0;
    for(const ImageView2D& image: images)
        size += sizeof(GLFWimage) + 4*image.size().product();
    Containers::Array<char> data{size};

    /* Pack array of GLFWimages and pixel data together into the memory
       allocated above */
    std::size_t offset = images.size()*sizeof(GLFWimage);
    Containers::ArrayView<GLFWimage> glfwImages = Containers::arrayCast<GLFWimage>(data.prefix(offset));
    std::size_t i = 0;
    for(const ImageView2D& image: images) {
        /* Copy and tightly pack pixels. GLFW doesn't allow arbitrary formats
           or strides (for subimages and/or Y flip), so we have to copy */
        Containers::ArrayView<char> target = data.slice(offset, offset + 4*image.size().product());
        auto out = Containers::StridedArrayView2D<Color4ub>{
            Containers::arrayCast<Color4ub>(target),
                    {std::size_t(image.size().y()),
                            std::size_t(image.size().x())}}.flipped<0>();
                            /** @todo handle sRGB differently? */
                            if(image.format() == PixelFormat::RGB8Snorm ||
                                    image.format() == PixelFormat::RGB8Unorm)
                                packPixels(image.pixels<Color3ub>(), out);
                            else if(image.format() == PixelFormat::RGBA8Snorm ||
                                    image.format() == PixelFormat::RGBA8Unorm)
                                packPixels(image.pixels<Color4ub>(), out);
                            else CORRADE_ASSERT(false, "Platform::MxGlfwApplication::setWindowIcon(): unexpected format" << image.format(), );

                            /* Specify the image metadata */
                            glfwImages[i].width = image.size().x();
                            glfwImages[i].height = image.size().y();
                            glfwImages[i].pixels = reinterpret_cast<unsigned char*>(target.data());

                            ++i;
                            offset += target.size();
    }

    glfwSetWindowIcon(_window, glfwImages.size(), glfwImages);
}
#endif

bool MxGlfwApplication::tryCreate(const Configuration& configuration) {
#ifdef MAGNUM_TARGET_GL
#ifdef GLFW_NO_API
    if(!(configuration.windowFlags() & MxSimulator::WindowFlags::Contextless))
#endif
    {
        return tryCreate(configuration, GLConfig{});
    }
#endif

    CORRADE_ASSERT(!_window, "Platform::MxGlfwApplication::tryCreate(): window already created", false);

    /* Scale window based on DPI */
    _dpiScaling = dpiScaling(configuration);
    const Vector2i scaledWindowSize = configuration.size()*_dpiScaling;

    /* Window flags */
    GLFWmonitor* monitor = nullptr; /* Needed for setting fullscreen */
    if (configuration.windowFlags() >= MxSimulator::WindowFlags::Fullscreen) {
        monitor = glfwGetPrimaryMonitor();
        glfwWindowHint(GLFW_AUTO_ICONIFY, configuration.windowFlags() >= MxSimulator::WindowFlags::AutoIconify);
    } else {
        const uint flags = configuration.windowFlags();
        glfwWindowHint(GLFW_RESIZABLE, flags >= MxSimulator::WindowFlags::Resizable);
        glfwWindowHint(GLFW_VISIBLE, !(flags >= MxSimulator::WindowFlags::Hidden));
#ifdef GLFW_MAXIMIZED
        glfwWindowHint(GLFW_MAXIMIZED, flags >= MxSimulator::WindowFlags::Maximized);
#endif
        glfwWindowHint(GLFW_FLOATING, flags >= MxSimulator::WindowFlags::Floating);
    }
    glfwWindowHint(GLFW_FOCUSED, configuration.windowFlags() >= MxSimulator::WindowFlags::Focused);

#ifdef GLFW_NO_API
    /* Disable implicit GL context creation */
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
#endif

    /* Create the window */
    _window = glfwCreateWindow(scaledWindowSize.x(), scaledWindowSize.y(), configuration.title().c_str(), monitor, nullptr);
    if(!_window) {
        Error() << "Platform::MxGlfwApplication::tryCreate(): cannot create window";
        glfwTerminate();
        return false;
    }

    /* Proceed with configuring other stuff that couldn't be done with window
       hints */
    if(configuration.windowFlags() >= MxSimulator::WindowFlags::Minimized)
        glfwIconifyWindow(_window);

    /* Set callbacks */
    setupCallbacks();

    return true;
}

namespace {

MxGlfwApplication::InputEvent::Modifiers currentGlfwModifiers(GLFWwindow* window) {
    static_assert(GLFW_PRESS == true && GLFW_RELEASE == false,
            "GLFW press and release constants do not correspond to bool values");

    MxGlfwApplication::InputEvent::Modifiers mods;
    if(glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) ||
            glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT))
        mods |= MxGlfwApplication::InputEvent::Modifier::Shift;
    if(glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) ||
            glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL))
        mods |= MxGlfwApplication::InputEvent::Modifier::Ctrl;
    if(glfwGetKey(window, GLFW_KEY_LEFT_ALT) ||
            glfwGetKey(window, GLFW_KEY_RIGHT_ALT))
        mods |= MxGlfwApplication::InputEvent::Modifier::Alt;
    if(glfwGetKey(window, GLFW_KEY_LEFT_SUPER) ||
            glfwGetKey(window, GLFW_KEY_RIGHT_SUPER))
        mods |= MxGlfwApplication::InputEvent::Modifier::Super;

    return mods;
}

}

#ifdef MAGNUM_TARGET_GL
bool MxGlfwApplication::tryCreate(const Configuration& configuration, const GLConfig& GLConfig) {
    CORRADE_ASSERT(!_window && _context->version() == GL::Version::None, "Platform::MxGlfwApplication::tryCreate(): window with OpenGL context already created", false);

    /* Scale window based on DPI */
    _dpiScaling = dpiScaling(configuration);
    const Vector2i scaledWindowSize = configuration.size()*_dpiScaling;

    /* Window flags */
    GLFWmonitor* monitor = nullptr; /* Needed for setting fullscreen */
    if (configuration.windowFlags() >= MxSimulator::WindowFlags::Fullscreen) {
        monitor = glfwGetPrimaryMonitor();
        glfwWindowHint(GLFW_AUTO_ICONIFY, configuration.windowFlags() >= MxSimulator::WindowFlags::AutoIconify);
    } else {
        const uint flags = configuration.windowFlags();
        glfwWindowHint(GLFW_RESIZABLE, flags >= MxSimulator::WindowFlags::Resizable);
        glfwWindowHint(GLFW_VISIBLE, !(flags >= MxSimulator::WindowFlags::Hidden));
#ifdef GLFW_MAXIMIZED
        glfwWindowHint(GLFW_MAXIMIZED, flags >= MxSimulator::WindowFlags::Maximized);
#endif
        glfwWindowHint(GLFW_FLOATING, flags >= MxSimulator::WindowFlags::Floating);
    }
    glfwWindowHint(GLFW_FOCUSED, configuration.windowFlags() >= MxSimulator::WindowFlags::Focused);

    /* Framebuffer setup */
    glfwWindowHint(GLFW_RED_BITS, GLConfig.colorBufferSize().r());
    glfwWindowHint(GLFW_GREEN_BITS, GLConfig.colorBufferSize().g());
    glfwWindowHint(GLFW_BLUE_BITS, GLConfig.colorBufferSize().b());
    glfwWindowHint(GLFW_ALPHA_BITS, GLConfig.colorBufferSize().a());
    glfwWindowHint(GLFW_DEPTH_BITS, GLConfig.depthBufferSize());
    glfwWindowHint(GLFW_STENCIL_BITS, GLConfig.stencilBufferSize());
    glfwWindowHint(GLFW_SAMPLES, GLConfig.sampleCount());
    glfwWindowHint(GLFW_SRGB_CAPABLE, GLConfig.isSrgbCapable());

    /* Request debug context if --magnum-gpu-validation is enabled */
    GLConfig::Flags glFlags = GLConfig.flags();
    if(_context->internalFlags() & GL::Context::InternalFlag::GpuValidation)
        glFlags |= GLConfig::Flag::Debug;

#ifdef GLFW_CONTEXT_NO_ERROR
    glfwWindowHint(GLFW_CONTEXT_NO_ERROR, glFlags >= GLConfig::Flag::NoError);
#endif
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, glFlags >= GLConfig::Flag::Debug);
    glfwWindowHint(GLFW_STEREO, glFlags >= GLConfig::Flag::Stereo);

    /* Set context version, if requested */
    if(GLConfig.version() != GL::Version::None) {
        Int major, minor;
        std::tie(major, minor) = version(GLConfig.version());
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, major);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, minor);
#ifndef MAGNUM_TARGET_GLES
        if(GLConfig.version() >= GL::Version::GL320) {
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
            glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, glFlags >= GLConfig::Flag::ForwardCompatible);
        }
#else
        glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#endif

        /* Request usable version otherwise */
    } else {
#ifndef MAGNUM_TARGET_GLES
        /* First try to create core context. This is needed mainly on macOS and
           Mesa, as support for recent OpenGL versions isn't implemented in
           compatibility contexts (which are the default). Unlike SDL2, GLFW
           requires at least version 3.2 to be able to request a core profile. */
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, glFlags >= GLConfig::Flag::ForwardCompatible);
#else
        /* For ES the major context version is compile-time constant */
#ifdef MAGNUM_TARGET_GLES3
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
#elif defined(MAGNUM_TARGET_GLES2)
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
#else
#error unsupported OpenGL ES version
#endif
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
        glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#endif
    }

    /* Create window. Hide it by default so we don't have distracting window
       blinking in case we have to destroy it again right away. If the creation
       succeeds, make the context current so we can query GL_VENDOR below.
       If we are on Wayland, this is causing a segfault; a blinking window is
       acceptable in this case. */
    constexpr const char waylandString[] = "wayland";
    const char* const xdgSessionType = std::getenv("XDG_SESSION_TYPE");
    if(!xdgSessionType || std::strncmp(xdgSessionType, waylandString, sizeof(waylandString)) != 0)
        glfwWindowHint(GLFW_VISIBLE, false);
    else if(_verboseLog)
        Warning{} << "Platform::MxGlfwApplication: Wayland detected, GL context has to be created with the window visible and may cause flicker on startup";
        if((_window = glfwCreateWindow(scaledWindowSize.x(), scaledWindowSize.y(), configuration.title().c_str(), monitor, nullptr)))
            glfwMakeContextCurrent(_window);

#ifndef MAGNUM_TARGET_GLES
        /* Fall back to (forward compatible) GL 2.1, if version is not
       user-specified and either core context creation fails or we are on
       binary NVidia/AMD drivers on Linux/Windows or Intel Windows drivers.
       Instead of creating forward-compatible context with highest available
       version, they force the version to the one specified, which is
       completely useless behavior. */
#ifndef CORRADE_TARGET_APPLE
        constexpr static const char nvidiaVendorString[] = "NVIDIA Corporation";
#ifdef CORRADE_TARGET_WINDOWS
        constexpr static const char intelVendorString[] = "Intel";
#endif
        constexpr static const char amdVendorString[] = "ATI Technologies Inc.";
        const char* vendorString;
#endif
        if(GLConfig.version() == GL::Version::None && (!_window
#ifndef CORRADE_TARGET_APPLE
                /* If context creation fails *really bad*, glGetString() may actually
           return nullptr. Check for that to avoid crashes deep inside
           strncmp(). Sorry about the UGLY code, HOPEFULLY THERE WON'T BE MORE
           WORKAROUNDS */
                || (vendorString = reinterpret_cast<const char*>(glGetString(GL_VENDOR)),
                        vendorString && (std::strncmp(vendorString, nvidiaVendorString, sizeof(nvidiaVendorString)) == 0 ||
#ifdef CORRADE_TARGET_WINDOWS
                                std::strncmp(vendorString, intelVendorString, sizeof(intelVendorString)) == 0 ||
#endif
                                std::strncmp(vendorString, amdVendorString, sizeof(amdVendorString)) == 0)
                                && !_context->isDriverWorkaroundDisabled("no-forward-compatible-core-context"))
#endif
        )) {
            /* Don't print any warning when doing the workaround, because the bug
           will be there probably forever */
            if(!_window) Warning{}
            << "Platform::MxGlfwApplication::tryCreate(): cannot create a window with core OpenGL context, falling back to compatibility context";
            else glfwDestroyWindow(_window);

            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);
            /** @todo or keep the fwcompat? */
            glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, false);

            _window = glfwCreateWindow(scaledWindowSize.x(), scaledWindowSize.y(), configuration.title().c_str(), monitor, nullptr);
        }
#endif

        if(!_window) {
            Error() << "Platform::MxGlfwApplication::tryCreate(): cannot create a window with OpenGL context";
            return false;
        }

        /* Proceed with configuring other stuff that couldn't be done with window
       hints */
        if(configuration.windowFlags() >= MxSimulator::WindowFlags::Minimized)
            glfwIconifyWindow(_window);


        /* Set callbacks */
        setupCallbacks();

        /* Make the final context current */
        glfwMakeContextCurrent(_window);

        /* Destroy everything when the Magnum context creation fails */
        if(!_context->tryCreate()) {
            glfwDestroyWindow(_window);
            _window = nullptr;
        }

        /* Show the window once we are sure that everything is okay */
        if(!(configuration.windowFlags() & MxSimulator::WindowFlags::Hidden))
            glfwShowWindow(_window);

        /* Return true if the initialization succeeds */
        return true;
}
#endif

void MxGlfwApplication::setupCallbacks() {
    glfwSetWindowUserPointer(_window, this);
    glfwSetWindowCloseCallback(_window, [](GLFWwindow* const window){
        ExitEvent e;
        static_cast<MxGlfwApplication*>(glfwGetWindowUserPointer(window))->exitEvent(e);
        if(!e.isAccepted()) glfwSetWindowShouldClose(window, false);
    });
    glfwSetWindowRefreshCallback(_window, [](GLFWwindow* const window){
        /* Properly redraw after the window is restored from minimized state */
        static_cast<MxGlfwApplication*>(glfwGetWindowUserPointer(window))->drawEvent();
    });
#ifdef MAGNUM_TARGET_GL
    glfwSetFramebufferSizeCallback
#else
    glfwSetWindowSizeCallback
#endif
    (_window, [](GLFWwindow* const window, const int w, const int h) {
        auto& app = *static_cast<MxGlfwApplication*>(glfwGetWindowUserPointer(window));
#ifdef CORRADE_TARGET_WINDOWS
        /* See the flag for details */
        if(!(app._flags & Flag::FirstViewportEventIgnored)) {
            app._flags |= Flag::FirstViewportEventIgnored;
            return;
        }
#endif
#ifdef MAGNUM_TARGET_GL
        ViewportEvent e{app.windowSize(), {w, h}, app.dpiScaling()};
#else
        ViewportEvent e{{w, h}, app.dpiScaling()};
#endif
        app.viewportEvent(e);
    });
    glfwSetKeyCallback(_window, [](GLFWwindow* const window, const int key, int, const int action, const int mods) {
        auto& app = *static_cast<MxGlfwApplication*>(glfwGetWindowUserPointer(window));

        KeyEvent e(static_cast<KeyEvent::Key>(key), {static_cast<InputEvent::Modifier>(mods)}, action == GLFW_REPEAT);

        if(action == GLFW_PRESS || action == GLFW_REPEAT)
            app.keyPressEvent(e);
        else if(action == GLFW_RELEASE)
            app.keyReleaseEvent(e);
    });
    glfwSetMouseButtonCallback(_window, [](GLFWwindow* const window, const int button, const int action, const int mods) {
        auto& app = *static_cast<MxGlfwApplication*>(glfwGetWindowUserPointer(window));

        double x, y;
        glfwGetCursorPos(window, &x, &y);
        MouseEvent e(static_cast<MouseEvent::Button>(button), {Int(x), Int(y)}, {static_cast<InputEvent::Modifier>(mods)});

        if(action == GLFW_PRESS) /* we don't handle GLFW_REPEAT */
            app.mousePressEvent(e);
        else if(action == GLFW_RELEASE)
            app.mouseReleaseEvent(e);
    });
    glfwSetCursorPosCallback(_window, [](GLFWwindow* const window, const double x, const double y) {
        auto& app = *static_cast<MxGlfwApplication*>(glfwGetWindowUserPointer(window));
        /* Avoid bogus offset at first -- report 0 when the event is called for
           the first time */
        Vector2i position{Int(x), Int(y)};
        MouseMoveEvent e{window, position,
            app._previousMouseMovePosition == Vector2i{-1} ? Vector2i{} :
                    position - app._previousMouseMovePosition};
        app._previousMouseMovePosition = position;
        app.mouseMoveEvent(e);
    });
    glfwSetScrollCallback(_window, [](GLFWwindow* window, double xoffset, double yoffset) {
        MouseScrollEvent e(window, Vector2{Float(xoffset), Float(yoffset)});
        static_cast<MxGlfwApplication*>(glfwGetWindowUserPointer(window))->mouseScrollEvent(e);
    });
    glfwSetCharCallback(_window, [](GLFWwindow* window, unsigned int codepoint) {
        auto& app = *static_cast<MxGlfwApplication*>(glfwGetWindowUserPointer(window));

        if(!(app._flags & Flag::TextInputActive)) return;

        char utf8[4]{};
        const std::size_t size = Utility::Unicode::utf8(codepoint, utf8);
        TextInputEvent e{{utf8, size}};
        app.textInputEvent(e);
    });
}

MxGlfwApplication::~MxGlfwApplication() {
    glfwDestroyWindow(_window);
    for(auto& cursor: _cursors)
        glfwDestroyCursor(cursor);
    glfwTerminate();
}

Vector2i MxGlfwApplication::windowSize() const {
    CORRADE_ASSERT(_window, "Platform::MxGlfwApplication::windowSize(): no window opened", {});

    Vector2i size;
    glfwGetWindowSize(_window, &size.x(), &size.y());
    return size;
}

void MxGlfwApplication::setWindowSize(const Vector2i& size) {
    CORRADE_ASSERT(_window, "Platform::MxGlfwApplication::setWindowSize(): no window opened", );

    const Vector2i newSize = _dpiScaling*size;
    glfwSetWindowSize(_window, newSize.x(), newSize.y());
}

#if GLFW_VERSION_MAJOR*100 + GLFW_VERSION_MINOR >= 302
void MxGlfwApplication::setMinWindowSize(const Vector2i& size) {
    CORRADE_ASSERT(_window, "Platform::MxGlfwApplication::setMinWindowSize(): no window opened", );

    const Vector2i newSize = _dpiScaling*size;
    glfwSetWindowSizeLimits(_window, newSize.x(), newSize.y(), _maxWindowSize.x(), _maxWindowSize.y());
    _minWindowSize = newSize;
}

void MxGlfwApplication::setMaxWindowSize(const Vector2i& size) {
    CORRADE_ASSERT(_window, "Platform::MxGlfwApplication::setMaxWindowSize(): no window opened", );

    const Vector2i newSize = _dpiScaling*size;
    glfwSetWindowSizeLimits(_window, _minWindowSize.x(), _minWindowSize.y(), newSize.x(), newSize.y());
    _maxWindowSize = newSize;
}
#endif

#ifdef MAGNUM_TARGET_GL
Vector2i MxGlfwApplication::framebufferSize() const {
    CORRADE_ASSERT(_window, "Platform::MxGlfwApplication::framebufferSize(): no window opened", {});

    Vector2i size;
    glfwGetFramebufferSize(_window, &size.x(), &size.y());
    return size;
}
#endif

void MxGlfwApplication::setSwapInterval(const Int interval) {
    glfwSwapInterval(interval);
}

void MxGlfwApplication::redraw() { _flags |= Flag::Redraw; }

int MxGlfwApplication::exec() {
    CORRADE_ASSERT(_window, "Platform::MxGlfwApplication::exec(): no window opened", {});

    while(mainLoopIteration()) {}

    return _exitCode;
}

bool MxGlfwApplication::mainLoopIteration() {
    if(_flags & Flag::Redraw) {
        _flags &= ~Flag::Redraw;
        drawEvent();
    }
    glfwPollEvents();

    return !glfwWindowShouldClose(_window);
}

namespace {

constexpr Int CursorMap[] {
        GLFW_ARROW_CURSOR,
        GLFW_IBEAM_CURSOR,
        GLFW_CROSSHAIR_CURSOR,
#ifdef GLFW_RESIZE_NWSE_CURSOR
        GLFW_RESIZE_NWSE_CURSOR,
        GLFW_RESIZE_NESW_CURSOR,
#endif
        GLFW_HRESIZE_CURSOR,
        GLFW_VRESIZE_CURSOR,
#ifdef GLFW_RESIZE_NWSE_CURSOR
        GLFW_RESIZE_ALL_CURSOR,
        GLFW_NOT_ALLOWED_CURSOR,
#endif
        GLFW_HAND_CURSOR
};

}

void MxGlfwApplication::setCursor(Cursor cursor) {
    CORRADE_INTERNAL_ASSERT(UnsignedInt(cursor) < Containers::arraySize(_cursors));

    _cursor = cursor;

    if(cursor == Cursor::Hidden) {
        glfwSetInputMode(_window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
        return;
    } else if(cursor == Cursor::HiddenLocked) {
        glfwSetInputMode(_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        return;
    } else {
        glfwSetInputMode(_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }

    if(!_cursors[UnsignedInt(cursor)])
        _cursors[UnsignedInt(cursor)] = glfwCreateStandardCursor(CursorMap[UnsignedInt(cursor)]);

    glfwSetCursor(_window, _cursors[UnsignedInt(cursor)]);
}

MxGlfwApplication::Cursor MxGlfwApplication::cursor() {
    return _cursor;
}

auto MxGlfwApplication::MouseMoveEvent::buttons() -> Buttons {
    if(!_buttons) {
        _buttons = Buttons{};
        for(const Int button: {GLFW_MOUSE_BUTTON_LEFT,
            GLFW_MOUSE_BUTTON_MIDDLE,
            GLFW_MOUSE_BUTTON_RIGHT}) {
            if(glfwGetMouseButton(_window, button) == GLFW_PRESS)
                *_buttons |= Button(1 << button);
        }
    }

    return *_buttons;
}

auto MxGlfwApplication::MouseMoveEvent::modifiers() -> Modifiers {
    if(!_modifiers) _modifiers = currentGlfwModifiers(_window);
    return *_modifiers;
}

Vector2i MxGlfwApplication::MouseScrollEvent::position() {
    if(!_position) {
        Vector2d position;
        glfwGetCursorPos(_window, &position.x(), &position.y());
        _position = Vector2i{position};
    }

    return *_position;
}

auto MxGlfwApplication::MouseScrollEvent::modifiers() -> Modifiers {
    if(!_modifiers) _modifiers = currentGlfwModifiers(_window);
    return *_modifiers;
}

void MxGlfwApplication::exitEvent(ExitEvent& event) {
    event.setAccepted();
}

void MxGlfwApplication::viewportEvent(ViewportEvent& event) {
#ifdef MAGNUM_BUILD_DEPRECATED
    CORRADE_IGNORE_DEPRECATED_PUSH
    viewportEvent(event.windowSize());
    CORRADE_IGNORE_DEPRECATED_POP
#else
    static_cast<void>(event);
#endif
}

#ifdef MAGNUM_BUILD_DEPRECATED
void MxGlfwApplication::viewportEvent(const Vector2i&) {}
#endif

void MxGlfwApplication::keyPressEvent(KeyEvent&) {}
void MxGlfwApplication::keyReleaseEvent(KeyEvent&) {}
void MxGlfwApplication::mousePressEvent(MouseEvent&) {}
void MxGlfwApplication::mouseReleaseEvent(MouseEvent&) {}
void MxGlfwApplication::mouseMoveEvent(MouseMoveEvent&) {}
void MxGlfwApplication::mouseScrollEvent(MouseScrollEvent&) {}
void MxGlfwApplication::textInputEvent(TextInputEvent&) {}

bool MxGlfwApplication::isTextInputActive() const {
    return !!(_flags & Flag::TextInputActive);
}

void MxGlfwApplication::startTextInput() {
    _flags |= Flag::TextInputActive;
}

void MxGlfwApplication::stopTextInput() {
    _flags &= ~Flag::TextInputActive;
}



HRESULT MxGlfwApplication::MxGlfwApplication::pollEvents()
{
    glfwPollEvents();
    return glfwGetError(NULL);
}

HRESULT MxGlfwApplication::MxGlfwApplication::waitEvents()
{
    glfwWaitEvents();
    return glfwGetError(NULL);
}

HRESULT MxGlfwApplication::MxGlfwApplication::waitEventsTimeout(double timeout)
{
    glfwWaitEventsTimeout(timeout);
    return glfwGetError(NULL);
}

HRESULT MxGlfwApplication::MxGlfwApplication::postEmptyEvent()
{
    glfwPostEmptyEvent();
    return glfwGetError(NULL);
}

#if defined(DOXYGEN_GENERATING_OUTPUT) || GLFW_VERSION_MAJOR*100 + GLFW_VERSION_MINOR >= 302
            std::string MxGlfwApplication::KeyEvent::keyName(const Key key) {
                /* It can return null, so beware */
                return Utility::String::fromArray(glfwGetKeyName(int(key), 0));
            }

            std::string MxGlfwApplication::KeyEvent::keyName() const {
                return keyName(_key);
            }
#endif

            //template class BasicScreen<MxGlfwApplication>;
            //template class BasicScreenedApplication<MxGlfwApplication>;


