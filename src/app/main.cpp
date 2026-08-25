// Graphical entry point.
//
// This file owns the window, the graphics context and the frame loop, and
// nothing else. Everything the application actually does lives in
// `app::Application`, which is why the headless tool and the test suite drive
// the same code without a display.

#include "app/Application.h"
#include "utilities/FileSystem.h"
#include "ui/MainWindow.h"
#include "ui/Theme.h"
#include "utilities/Logging.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <GLFW/glfw3.h>

#include <chrono>
#include <filesystem>
#include <string>

namespace {

using namespace tnp;

constexpr std::string_view kLogCategory = "main";

void onGlfwError(int code, const char* description) {
    logging::error(kLogCategory, "GLFW error {}: {}", code, description);
}

/// Fonts TNP will use if the system has one, in preference order.
///
/// No font is embedded: a binary blob in the repository is a maintenance cost,
/// and every supported platform ships something better than the built-in bitmap
/// face. When none is found the default font is used and the application looks
/// plainer, which is a fair trade.
const char* const kFontCandidates[] = {
#if defined(_WIN32)
    "C:/Windows/Fonts/segoeui.ttf",
    "C:/Windows/Fonts/tahoma.ttf",
    "C:/Windows/Fonts/arial.ttf",
#elif defined(__APPLE__)
    "/System/Library/Fonts/SFNS.ttf",
    "/System/Library/Fonts/Helvetica.ttc",
    "/Library/Fonts/Arial.ttf",
#else
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
    "/usr/share/fonts/TTF/DejaVuSans.ttf",
#endif
};

void loadFont(float pixelSize) {
    ImGuiIO& io = ImGui::GetIO();
    for (const char* path : kFontCandidates) {
        std::error_code ec;
        if (!std::filesystem::exists(path, ec)) continue;
        if (io.Fonts->AddFontFromFileTTF(path, pixelSize) != nullptr) {
            logging::debug(kLogCategory, "using font {}", path);
            return;
        }
    }
    logging::debug(kLogCategory, "no system font found; using the built-in face");
}

/// The GLSL version string matching the context created below.
const char* glslVersion() {
#if defined(__APPLE__)
    return "#version 150";
#else
    return "#version 130";
#endif
}

void configureGlContext() {
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
#if defined(__APPLE__)
    // macOS only offers 3.2+ through the core profile, and only forward-compatible.
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif
}

} // namespace

int main(int argc, char** argv) {
    logging::Logger::instance().addSink(std::make_shared<logging::ConsoleSink>());

    app::Application application;
    application.initialize();

    // --- Window -------------------------------------------------------------
    glfwSetErrorCallback(onGlfwError);
    if (glfwInit() == GLFW_FALSE) {
        logging::error(kLogCategory, "GLFW could not be initialised; TNP cannot open a window");
        return 1;
    }

    configureGlContext();

    const app::ApplicationConfig& config = application.config();
    GLFWwindow* window = glfwCreateWindow(config.windowWidth, config.windowHeight,
                                          "TNP - The Network Project", nullptr, nullptr);
    if (window == nullptr) {
        logging::error(kLogCategory, "could not create a window; is a GPU driver available?");
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // vsync: a network editor has no reason to spin the GPU
    if (config.maximized) glfwMaximizeWindow(window);

    // --- Dear ImGui ---------------------------------------------------------
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigWindowsMoveFromTitleBarOnly = true;

    // The layout is remembered next to the preferences rather than in the
    // working directory, so it follows the user rather than the shell.
    static const std::string layoutPath = (files::userConfigDirectory() / "layout.ini").string();
    io.IniFilename = layoutPath.c_str();

    ui::applyTheme();
    loadFont(16.0f * config.uiScale);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glslVersion());

    ui::MainWindow mainWindow{application};

    // Open a project named on the command line.
    if (argc > 1) {
        if (const Status status = application.open(argv[1]); !status) {
            logging::error(kLogCategory, "{}", status.message());
            mainWindow.context().setStatus(status.message(), true);
        }
    }

    // --- Frame loop ---------------------------------------------------------
    auto previousFrame = std::chrono::steady_clock::now();

    while (glfwWindowShouldClose(window) == GLFW_FALSE && !mainWindow.wantsToClose()) {
        glfwPollEvents();

        // The window manager can iconify us; there is nothing to draw then.
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0) {
            glfwWaitEvents();
            previousFrame = std::chrono::steady_clock::now();
            continue;
        }

        const auto now = std::chrono::steady_clock::now();
        const Duration elapsed = std::chrono::duration_cast<Duration>(now - previousFrame);
        previousFrame = now;

        // The simulation advances on real elapsed time, independent of how long
        // rendering took: the engine is never driven by the frame rate.
        application.update(elapsed);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        mainWindow.draw();

        ImGui::Render();

        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        glViewport(0, 0, width, height);

        const ImVec4 background = ImGui::ColorConvertU32ToFloat4(ui::theme().canvasBackground);
        glClearColor(background.x, background.y, background.z, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);

        // The window close button goes through the same "save first?" path as
        // File > Quit.
        if (glfwWindowShouldClose(window) != GLFW_FALSE) {
            glfwSetWindowShouldClose(window, GLFW_FALSE);
            mainWindow.requestClose();
        }
    }

    // --- Shutdown -----------------------------------------------------------
    int windowWidth = 0;
    int windowHeight = 0;
    glfwGetWindowSize(window, &windowWidth, &windowHeight);

    application.config().windowWidth = windowWidth;
    application.config().windowHeight = windowHeight;
    application.config().maximized = glfwGetWindowAttrib(window, GLFW_MAXIMIZED) != 0;
    application.shutdown();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
